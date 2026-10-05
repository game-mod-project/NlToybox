#pragma once
// 모드창. 게임의 화면 출력(IDXGISwapChain::Present)에 끼어들어 Dear ImGui 로 창을 그린다.
// YYToolkit v5.0.0c 의 배포본은 Present 를 훅하지 않으므로(research/00) 이 모듈이 직접 건다.
// 방법은 YYToolkit 소스가 쓰는 것과 같다: os_get_info 의 video_d3d11_swapchain, 가상 함수표의 8번(Hooks.cpp).

#include <YYTK_Shared.hpp>

#include <filesystem>
#include <functional>
#include <string>

namespace NlUi
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번 부른다. 시험용 설정(NlToyBox.ui.txt)이 있으면 읽고 지운다.
	void Init(Aurie::AurieModule* Module, const std::filesystem::path& ModuleDir, LogFn Log);

	// 창 안을 그리는 함수. 창이 열려 있는 동안 프레임마다 불린다(ImGui::Begin 과 End 사이).
	void SetContent(std::function<void()> Draw);

	// 게임 스레드의 콜백에서 부른다. Present 에 훅을 건다. 될 때까지 몇 번 다시 해 본다.
	void GameTick();

	// 창 메시지 콜백에서 부른다. 모드창이 가져간 입력은 게임에 넘기지 않는다.
	void WndProc(YYTK::FWWndProc& Context);

	// 창이 열려 있는가.
	bool Visible();
}
