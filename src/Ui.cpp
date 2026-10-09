#include "Ui.hpp"

#include "Game.hpp"
#include "core/NumberEdit.hpp"

#include <d3d11.h>
#include <dxgi.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

// imgui_impl_win32.h 는 이 선언을 주석으로만 둔다(쓰는 쪽이 직접 선언하라고 한다).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace Aurie;
using namespace YYTK;
using Clock = std::chrono::steady_clock;

namespace
{
	using PresentFn = HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT);

	constexpr int k_ToggleKey = VK_F8;
	constexpr int k_MaxHookTries = 40;			// 0.5초 간격. 20초 안에 안 되면 그만둔다
	constexpr double k_HintSeconds = 15;		// 창을 여는 키를 화면 구석에 보여 주는 시간
	constexpr const char* k_HookName = "NlToyBox.Present";

	AurieModule* g_Module = nullptr;
	std::filesystem::path g_Dir;
	NlUi::LogFn g_Log;
	std::function<void()> g_Content;

	// 훅 걸기 (게임 스레드)
	PresentFn g_OrigPresent = nullptr;
	std::atomic<bool> g_Hooked = false;
	int g_HookTries = 0;
	Clock::time_point g_NextTry;
	DWORD g_GameThread = 0;

	// 그리기 (Present 를 부르는 스레드)
	IDXGISwapChain* g_Chain = nullptr;
	ID3D11Device* g_Device = nullptr;
	ID3D11DeviceContext* g_Context = nullptr;
	HWND g_Window = nullptr;
	std::atomic<bool> g_Ready = false;
	bool g_SetupFailed = false;
	std::atomic<bool> g_Visible = false;
	Clock::time_point g_ReadyAt;
	long long g_Frames = 0;

	// 시험용 설정 (NlToyBox.ui.txt): 창을 연 채 시작하고, 몇 초 뒤의 화면을 파일로 뜬다
	bool g_OpenAtStart = false;
	double g_ShotSeconds = -1;
	bool g_ShotDone = false;
	std::vector<std::string> g_TestSets;
	std::vector<std::pair<std::string, std::string>> g_TestExtra;	// 이 파일이 모르는 줄(키, 값). Menu 가 읽는다
	std::mutex g_ShotMutex;
	std::filesystem::path g_ShotRequest;		// 밖에서 청한 화면. 다음 프레임에 뜬다
	long long g_MouseMessages = 0, g_KeyMessages = 0;		// 창이 열려 있는 동안 받은 입력 메시지의 수
	long long g_YytkWndProcCalls = 0;						// YYToolkit 의 창 메시지 콜백이 온 횟수(오는지 보려고 센다)
	WNDPROC g_OrigWndProc = nullptr;
	bool g_WindowUnicode = true;
	// 시험용 끌기(drag=x1,y1,x2,y2): 화면을 뜨기 5초 전부터 2초 동안 ImGui 에 마우스 입력을 넣는다.
	// 실제 마우스를 건드리지 않고 슬라이더가 움직이는지 본다.
	bool g_TestDrag = false, g_DragDown = false, g_DragUp = false;
	float g_Drag[4] = {};

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	std::string Hex(HRESULT Value)
	{
		std::ostringstream out;
		out << "0x" << std::hex << static_cast<unsigned long>(Value);
		return out.str();
	}

	// 가상 함수표에서 Present(8번)의 주소를 읽는다. 받은 포인터가 엉뚱하면 죽지 않고 nullptr 을 돌려준다.
	void* ReadPresentSlot(void* Chain)
	{
		__try
		{
			void** table = *static_cast<void***>(Chain);
			return table ? table[8] : nullptr;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return nullptr;
		}
	}

	// 한글이 든 글꼴을 올린다(맑은 고딕). 없으면 ImGui 의 기본 글꼴을 쓴다(한글은 보이지 않는다).
	void LoadFont(ImGuiIO& Io)
	{
		wchar_t windows[MAX_PATH] = {};
		GetWindowsDirectoryW(windows, MAX_PATH);
		const std::filesystem::path font = std::filesystem::path(windows) / "Fonts" / "malgun.ttf";

		std::error_code ec;
		if (!std::filesystem::exists(font, ec))
		{
			Io.Fonts->AddFontDefault();
			Log("ui font: default (malgun.ttf not found)");
			return;
		}

		static ImVector<ImWchar> ranges;		// 글꼴 아틀라스가 만들어질 때까지 살아 있어야 한다
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(Io.Fonts->GetGlyphRangesDefault());
		builder.AddRanges(Io.Fonts->GetGlyphRangesKorean());
		builder.BuildRanges(&ranges);
		Io.Fonts->AddFontFromFileTTF(font.string().c_str(), 21.0f, nullptr, ranges.Data);
		Log("ui font: malgun.ttf");
	}

	// 메시지를 모드창이 가져갔으면 참. 그 메시지는 게임에 넘기지 않는다.
	bool HandleMessage(HWND Window, UINT Message, WPARAM W, LPARAM L)
	{
		if (Message == WM_KEYDOWN && W == k_ToggleKey)
		{
			if (!(L & (1LL << 30)))		// 누르고 있는 동안의 되풀이는 무시한다
			{
				g_Visible = !g_Visible;
				Log(g_Visible ? "ui window opened (F8)" : "ui window closed (F8)");
			}
			return true;
		}
		if (!g_Visible)
			return false;

		ImGui_ImplWin32_WndProcHandler(Window, Message, W, L);
		const ImGuiIO& io = ImGui::GetIO();
		const bool mouse = Message >= WM_MOUSEFIRST && Message <= WM_MOUSELAST;
		const bool keys = Message >= WM_KEYFIRST && Message <= WM_KEYLAST;
		g_MouseMessages += mouse;
		g_KeyMessages += keys;
		return (mouse && io.WantCaptureMouse) || (keys && io.WantCaptureKeyboard);
	}

	// YYToolkit v5.0.0c 의 EVENT_WNDPROC 콜백은 이 게임에서 오지 않았다(F8 을 보내도 불리지 않았다).
	// 그래서 창 프로시저를 직접 바꿔 건다. 가져가지 않은 메시지는 원래 프로시저로 넘긴다.
	LRESULT CALLBACK HkWndProc(HWND Window, UINT Message, WPARAM W, LPARAM L)
	{
		if (g_Ready && HandleMessage(Window, Message, W, L))
			return 0;
		return g_WindowUnicode ? CallWindowProcW(g_OrigWndProc, Window, Message, W, L)
			: CallWindowProcA(g_OrigWndProc, Window, Message, W, L);
	}

	bool Setup(IDXGISwapChain* Chain)
	{
		HRESULT hr = Chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g_Device));
		if (FAILED(hr) || !g_Device)
		{
			Log("ui setup failed: GetDevice " + Hex(hr));
			return false;
		}
		g_Device->GetImmediateContext(&g_Context);

		DXGI_SWAP_CHAIN_DESC desc = {};
		hr = Chain->GetDesc(&desc);
		if (FAILED(hr) || !desc.OutputWindow)
		{
			Log("ui setup failed: GetDesc " + Hex(hr));
			return false;
		}
		g_Window = desc.OutputWindow;
		g_Chain = Chain;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;		// 게임 폴더에는 게임의 imgui.ini 가 있다. 건드리지 않는다
		io.LogFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;		// 게임의 커서를 바꾸지 않는다
		ImGui::StyleColorsDark();
		LoadFont(io);

		if (!ImGui_ImplWin32_Init(g_Window) || !ImGui_ImplDX11_Init(g_Device, g_Context))
		{
			Log("ui setup failed: imgui backend");
			return false;
		}

		g_WindowUnicode = IsWindowUnicode(g_Window) != FALSE;
		const LONG_PTR previous = g_WindowUnicode
			? SetWindowLongPtrW(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HkWndProc))
			: SetWindowLongPtrA(g_Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HkWndProc));
		g_OrigWndProc = reinterpret_cast<WNDPROC>(previous);
		if (!g_OrigWndProc)
		{
			Log("ui setup failed: window procedure");
			return false;
		}

		g_Visible = g_OpenAtStart;
		g_ReadyAt = Clock::now();
		g_Ready = true;
		Log("ui ready " + std::to_string(desc.BufferDesc.Width) + "x" + std::to_string(desc.BufferDesc.Height)
			+ " format " + std::to_string(static_cast<int>(desc.BufferDesc.Format))
			+ " samples " + std::to_string(desc.SampleDesc.Count)
			+ " draw thread " + std::to_string(GetCurrentThreadId()) + " game thread " + std::to_string(g_GameThread));
		return true;
	}

	// 지금 그려진 화면을 BMP 로 쓴다. 모드창이 제대로 그려졌는지 사람(또는 도구)이 보기 위한 것이다.
	// File: 쓸 파일. Announce: 시험 설정의 화면이다(ui-check.ps1 이 "ui shot done" 줄을 기다린다).
	void Capture(ID3D11Texture2D* Back, const std::filesystem::path& File, bool Announce)
	{
		D3D11_TEXTURE2D_DESC desc = {};
		Back->GetDesc(&desc);

		D3D11_TEXTURE2D_DESC plain = desc;
		plain.SampleDesc.Count = 1;
		plain.SampleDesc.Quality = 0;
		plain.MipLevels = 1;
		plain.ArraySize = 1;
		plain.MiscFlags = 0;

		D3D11_TEXTURE2D_DESC readable = plain;
		readable.Usage = D3D11_USAGE_STAGING;
		readable.BindFlags = 0;
		readable.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

		ID3D11Texture2D* staging = nullptr;
		HRESULT hr = g_Device->CreateTexture2D(&readable, nullptr, &staging);
		if (FAILED(hr) || !staging)
		{
			Log("ui shot failed: staging " + Hex(hr));
			return;
		}

		if (desc.SampleDesc.Count > 1)
		{
			plain.Usage = D3D11_USAGE_DEFAULT;
			plain.BindFlags = D3D11_BIND_RENDER_TARGET;
			plain.CPUAccessFlags = 0;
			ID3D11Texture2D* resolved = nullptr;
			hr = g_Device->CreateTexture2D(&plain, nullptr, &resolved);
			if (FAILED(hr) || !resolved)
			{
				staging->Release();
				Log("ui shot failed: resolve target " + Hex(hr));
				return;
			}
			g_Context->ResolveSubresource(resolved, 0, Back, 0, desc.Format);
			g_Context->CopyResource(staging, resolved);
			resolved->Release();
		}
		else
			g_Context->CopyResource(staging, Back);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		hr = g_Context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr))
		{
			staging->Release();
			Log("ui shot failed: map " + Hex(hr));
			return;
		}

		// BGRA 로 쓴다. 뒤 버퍼가 RGBA 꼴이면 R 과 B 를 바꾼다.
		const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM || desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
			|| desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS;
		const uint32_t width = desc.Width, height = desc.Height;
		std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
		for (uint32_t y = 0; y < height; y++)
		{
			const uint8_t* src = static_cast<const uint8_t*>(mapped.pData) + static_cast<size_t>(y) * mapped.RowPitch;
			uint8_t* dst = pixels.data() + static_cast<size_t>(y) * width * 4;
			for (uint32_t x = 0; x < width; x++)
			{
				dst[x * 4 + 0] = src[x * 4 + (rgba ? 2 : 0)];
				dst[x * 4 + 1] = src[x * 4 + 1];
				dst[x * 4 + 2] = src[x * 4 + (rgba ? 0 : 2)];
				dst[x * 4 + 3] = 255;
			}
		}
		g_Context->Unmap(staging, 0);
		staging->Release();

		BITMAPFILEHEADER file = {};
		BITMAPINFOHEADER info = {};
		file.bfType = 0x4D42;
		file.bfOffBits = sizeof(file) + sizeof(info);
		file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
		info.biSize = sizeof(info);
		info.biWidth = static_cast<LONG>(width);
		info.biHeight = -static_cast<LONG>(height);		// 위에서 아래로
		info.biPlanes = 1;
		info.biBitCount = 32;
		info.biCompression = BI_RGB;

		std::ofstream out(File, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(&file), sizeof(file));
		out.write(reinterpret_cast<const char*>(&info), sizeof(info));
		out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
		out.close();
		if (!Announce)
		{
			Log("ui shot saved " + File.filename().string());
			return;
		}
		Log("ui shot done " + std::to_string(width) + "x" + std::to_string(height) + " format "
			+ std::to_string(static_cast<int>(desc.Format)) + " frames " + std::to_string(g_Frames)
			+ (g_Visible ? " window open" : " window closed")
			+ " mouse " + std::to_string(g_MouseMessages) + " keys " + std::to_string(g_KeyMessages)
			+ " yytk wndproc " + std::to_string(g_YytkWndProcCalls));
	}

	void Frame(IDXGISwapChain* Chain)
	{
		if (g_SetupFailed)
			return;
		if (!g_Ready && !Setup(Chain))
		{
			g_SetupFailed = true;
			return;
		}
		if (Chain != g_Chain)
			return;
		g_Frames++;

		const double elapsed = std::chrono::duration<double>(Clock::now() - g_ReadyAt).count();
		const bool hint = elapsed < k_HintSeconds;
		const bool shot = g_ShotSeconds >= 0 && !g_ShotDone && elapsed >= g_ShotSeconds;
		const bool visible = g_Visible;
		std::filesystem::path requested;
		{
			std::lock_guard lock(g_ShotMutex);
			requested.swap(g_ShotRequest);
		}
		if (!visible && !hint && !shot && requested.empty())
			return;

		ID3D11Texture2D* back = nullptr;
		if (FAILED(Chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))) || !back)
			return;
		ID3D11RenderTargetView* view = nullptr;
		if (FAILED(g_Device->CreateRenderTargetView(back, nullptr, &view)) || !view)
		{
			back->Release();
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		io.MouseDrawCursor = visible;		// 게임이 그린 커서는 모드창 밑에 깔린다. 창이 열려 있을 때는 ImGui 가 그린다

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		if (g_TestDrag && g_ShotSeconds >= 0)
		{
			// 백엔드가 방금 실제 커서의 자리를 넣었을 수 있다. 그 뒤에 넣어 이 프레임의 자리를 정한다.
			const double t = elapsed - (g_ShotSeconds - 5);
			if (t >= 0 && t < 2)
			{
				io.AddMousePosEvent(g_Drag[t < 1 ? 0 : 2], g_Drag[t < 1 ? 1 : 3]);
				if (t >= 0.5 && !g_DragDown)
				{
					g_DragDown = true;
					io.AddMouseButtonEvent(0, true);
				}
			}
			else if (t >= 2 && g_DragDown && !g_DragUp)
			{
				g_DragUp = true;
				io.AddMousePosEvent(g_Drag[2], g_Drag[3]);
				io.AddMouseButtonEvent(0, false);
				Log("ui test drag done");
			}
		}
		ImGui::NewFrame();
		if (visible)
		{
			ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(980, 620), ImGuiCond_FirstUseEver);
			if (ImGui::Begin("NlToyBox  (F8)"))
			{
				if (g_Content)
					g_Content();
				else
					ImGui::TextUnformatted("...");
			}
			ImGui::End();
		}
		else if (hint)
			ImGui::GetForegroundDrawList()->AddText(ImVec2(16, 12), IM_COL32(255, 255, 255, 200), "NlToyBox: F8");
		ImGui::Render();

		// ImGui 의 DX11 백엔드는 렌더 타깃을 되돌려 놓지 않는다. 여기서 한다.
		ID3D11RenderTargetView* old_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		ID3D11DepthStencilView* old_depth = nullptr;
		g_Context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, old_views, &old_depth);
		g_Context->OMSetRenderTargets(1, &view, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_Context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, old_views, old_depth);
		for (ID3D11RenderTargetView* old_view : old_views)
			if (old_view)
				old_view->Release();
		if (old_depth)
			old_depth->Release();

		if (shot)
		{
			g_ShotDone = true;
			Capture(back, g_Dir / "NlToyBox.ui.bmp", true);
		}
		if (!requested.empty())
			Capture(back, requested, false);
		view->Release();
		back->Release();
	}

	HRESULT WINAPI HkPresent(IDXGISwapChain* Chain, UINT Sync, UINT Flags)
	{
		if (!(Flags & DXGI_PRESENT_TEST))
			Frame(Chain);
		return g_OrigPresent(Chain, Sync, Flags);
	}
}

void NlUi::Init(AurieModule* Module, const std::filesystem::path& ModuleDir, LogFn Log_)
{
	g_Module = Module;
	g_Dir = ModuleDir;
	g_Log = std::move(Log_);
	g_NextTry = Clock::now();

	const std::filesystem::path config = ModuleDir / "NlToyBox.ui.txt";
	std::ifstream in(config);
	if (!in)
		return;
	std::string line;
	while (std::getline(in, line))
	{
		const size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
		if (key == "open")
			g_OpenAtStart = value.rfind("1", 0) == 0;
		else if (key == "shot_seconds")
			g_ShotSeconds = std::atof(value.c_str());
		else if (key == "drag")
			g_TestDrag = sscanf_s(value.c_str(), "%f,%f,%f,%f", &g_Drag[0], &g_Drag[1], &g_Drag[2], &g_Drag[3]) == 4;		// MSVC 전용 빌드다. %f 에는 크기 인자가 없다
		else if (key == "set")
			g_TestSets.push_back(value.substr(0, value.find_last_not_of(" \r\n") + 1));
		else
			g_TestExtra.emplace_back(key, value.substr(0, value.find_last_not_of(" \r\n") + 1));
	}
	in.close();
	std::error_code ec;
	std::filesystem::remove(config, ec);		// 한 번만 쓴다. 평소 플레이에 남지 않게 한다
	Log("ui test config: open " + std::to_string(g_OpenAtStart) + ", shot after " + std::to_string(g_ShotSeconds) + "s");
}

void NlUi::SetContent(std::function<void()> Draw)
{
	g_Content = std::move(Draw);
}

bool NlUi::Visible()
{
	return g_Ready && g_Visible;
}

const std::vector<std::string>& NlUi::TestSets()
{
	return g_TestSets;
}

std::vector<std::string> NlUi::TestValues(const std::string& Key)
{
	std::vector<std::string> values;
	for (const auto& [key, value] : g_TestExtra)
		if (key == Key)
			values.push_back(value);
	return values;
}

double NlUi::ShotSeconds()
{
	return g_ShotSeconds;
}

double NlUi::SecondsSinceReady()
{
	return g_Ready ? std::chrono::duration<double>(Clock::now() - g_ReadyAt).count() : -1;
}

void NlUi::GameTick()
{
	if (g_Hooked || g_HookTries >= k_MaxHookTries)
		return;
	const Clock::time_point now = Clock::now();
	if (now < g_NextTry)
		return;
	g_NextTry = now + std::chrono::milliseconds(500);
	g_HookTries++;
	g_GameThread = GetCurrentThreadId();
	const bool last = g_HookTries >= k_MaxHookTries;

	// os_get_info 는 부를 때마다 ds_map 을 새로 만든다. 다 쓰면 지운다(GameMaker 매뉴얼 os_get_info).
	RValue info;
	if (!NlGame::Call("os_get_info", {}, info) || !NlGame::IsNumber(info) || info.ToDouble() < 0)
	{
		if (last)
			Log("ui hook failed: os_get_info");
		return;
	}
	RValue chain_value, ignored;
	const bool found = NlGame::Call("ds_map_find_value", { info, RValue("video_d3d11_swapchain") }, chain_value);
	NlGame::Call("ds_map_destroy", { info }, ignored);
	void* chain = found && !chain_value.IsUndefined() ? chain_value.ToPointer() : nullptr;
	void* present = chain ? ReadPresentSlot(chain) : nullptr;
	if (!present)
	{
		if (last)
			Log("ui hook failed: no swapchain (kind " + chain_value.GetKindName() + ")");
		return;
	}

	const AurieStatus status = MmCreateHook(g_Module, k_HookName, present, HkPresent, reinterpret_cast<PVOID*>(&g_OrigPresent));
	if (!AurieSuccess(status) || !g_OrigPresent)
	{
		Log(std::string("ui hook failed: MmCreateHook ") + AurieStatusToString(status));
		g_HookTries = k_MaxHookTries;
		return;
	}
	g_Hooked = true;
	Log("ui hook ok (try " + std::to_string(g_HookTries) + ")");
}

void NlUi::RequestShot(const std::filesystem::path& File)
{
	std::lock_guard lock(g_ShotMutex);
	g_ShotRequest = File;
}

void NlUi::SetVisible(bool Visible)
{
	g_Visible = Visible;
}

bool NlUi::InjectClick(float X, float Y)
{
	if (!g_Ready)
		return false;
	ImGuiIO& io = ImGui::GetIO();
	io.AddMousePosEvent(X, Y);
	io.AddMouseButtonEvent(0, true);
	io.AddMouseButtonEvent(0, false);		// 같은 단추의 두 번째 바뀜은 ImGui 가 다음 프레임으로 넘긴다
	Log("ui test click " + std::to_string(static_cast<int>(X)) + "," + std::to_string(static_cast<int>(Y)));
	return true;
}

bool NlUi::InjectText(const std::string& Text)
{
	if (!g_Ready)
		return false;
	ImGui::GetIO().AddInputCharactersUTF8(Text.c_str());
	Log("ui test type " + Text);
	return true;
}

bool NlUi::InjectKey(const std::string& Name)
{
	if (!g_Ready)
		return false;
	const ImGuiKey key = Name == "enter" ? ImGuiKey_Enter : Name == "tab" ? ImGuiKey_Tab : Name == "escape" ? ImGuiKey_Escape
		: Name == "backspace" ? ImGuiKey_Backspace : ImGuiKey_None;
	if (key == ImGuiKey_None)
		return false;
	ImGuiIO& io = ImGui::GetIO();
	io.AddKeyEvent(key, true);
	io.AddKeyEvent(key, false);
	Log("ui test key " + Name);
	return true;
}

void NlUi::WndProc(FWWndProc& Context)
{
	// 입력은 직접 건 창 프로시저(HkWndProc)가 받는다. 여기서는 이 콜백이 오는지만 센다.
	UNREFERENCED_PARAMETER(Context);
	g_YytkWndProcCalls++;
}

void NlUi::Hint(const char* Text)
{
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextDisabled("%s", Text);
	ImGui::PopTextWrapPos();
}

void NlUi::Hint(const std::string& Text)
{
	Hint(Text.c_str());
}

namespace
{
	NlCore::NumberEdit g_NumberEdit;		// 잡힌 수 입력 칸의 편집 상태(한 번에 하나)
	std::string g_NumberEditKey;
}

bool NlUi::InputNumber(const char* Label, const std::string& Key, double Value, const char* Format, double& Out)
{
	const bool typed = ImGui::InputDouble(Label, &Value, 0, 0, Format);
	const bool active = ImGui::IsItemActive(), left = ImGui::IsItemDeactivatedAfterEdit();
	if (g_NumberEditKey != Key)
	{
		if (!typed && !active && !left)		// 다른 칸의 상태를 건드리지 않는다
			return false;
		g_NumberEdit = NlCore::NumberEdit();
		g_NumberEditKey = Key;
	}
	return NlCore::StepNumberEdit(g_NumberEdit, typed, Value, left, active, Out);		// 열쇠가 같으면 매 프레임 부른다(잡혀 있지 않으면 남은 수를 버린다)
}
