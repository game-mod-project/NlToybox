#pragma once
// 모드창. 게임의 화면 출력(IDXGISwapChain::Present)에 끼어들어 Dear ImGui 로 창을 그린다.
// YYToolkit v5.0.0c 의 배포본은 Present 를 훅하지 않으므로(research/00) 이 모듈이 직접 건다.
// 방법은 YYToolkit 소스가 쓰는 것과 같다: os_get_info 의 video_d3d11_swapchain, 가상 함수표의 8번(Hooks.cpp).

#include <YYTK_Shared.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlUi
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번 부른다. 시험용 설정(NlToyBox.ui.txt)이 있으면 읽고 지운다.
	void Init(Aurie::AurieModule* Module, const std::filesystem::path& ModuleDir, LogFn Log);

	// 창 안을 그리는 함수. 창이 열려 있는 동안 프레임마다 불린다(ImGui::Begin 과 End 사이).
	void SetContent(std::function<void()> Draw);

	// 게임 스레드의 콜백에서 부른다. Present 에 훅을 건다. 될 때까지 몇 번 다시 해 본다.
	void GameTick();

	// YYToolkit 의 창 메시지 콜백에서 부른다. 이 콜백이 오는지 세기만 한다(입력은 모듈이 직접 건 창 프로시저로 받는다).
	void WndProc(YYTK::FWWndProc& Context);

	// 창이 열려 있는가.
	bool Visible();

	// 시험용 설정의 "set=이름:배율" 줄들(이름:배율). 평소에는 비어 있다.
	const std::vector<std::string>& TestSets();

	// 시험용 설정의 그 밖의 줄: "Key=값" 들의 값. 평소에는 비어 있다(page, path, ask, poke 를 Menu 가 읽는다).
	std::vector<std::string> TestValues(const std::string& Key);

	// 시험용 설정이 정한, 화면을 뜨는 때(모드창이 준비된 뒤의 초). 뜨지 않으면 음수.
	double ShotSeconds();

	// 모드창이 준비된 뒤의 초. 아직 준비되지 않았으면 음수.
	double SecondsSinceReady();

	// 다음에 그려지는 프레임을 그 파일(BMP)로 뜬다. 창이 닫혀 있어도 뜬다. 끝나면 로그에 "ui shot saved <파일 이름>"을 적는다.
	void RequestShot(const std::filesystem::path& File);

	// 모드창을 열거나 닫는다(F8 과 같다).
	void SetVisible(bool Visible);

	// 시험용: Dear ImGui 의 입력 큐에 넣는다(게임 창과 진짜 마우스·키보드는 건드리지 않는다). 게임 스레드에서 부른다. 모드창이 준비되지 않았으면 거짓.
	// 누름은 그 자리로 옮기고 왼쪽 단추를 눌렀다 뗀다. 글자는 잡혀 있는 입력 칸이 받는다. 키는 enter, tab, escape, backspace.
	bool InjectClick(float X, float Y);
	bool InjectText(const std::string& Text);
	bool InjectKey(const std::string& Name);
}
