// NlToyBox 모듈의 들머리. 모듈 옆 NlToyBox.log 에 적재, YYToolkit 버전, 프로브 두 개의 결과를 쓰고
// 게임 스레드의 콜백에서 덤프, 모드창, 배율, 치트 메뉴의 틱을 돌린다.
// 처음 여섯 줄의 형식은 tools/common.ps1 의 Get-NlLoadFailures 가 그대로 찾는다 (Phase 0 스펙 §4.3). 바꾸면 그쪽도 바꾼다.

#include <YYTK_Shared.hpp>
#include "Build.hpp"
#include "Production.hpp"
#include "Dump.hpp"
#include "Court.hpp"
#include "Diplomacy.hpp"
#include "Economy.hpp"
#include "People.hpp"
#include "World.hpp"
#include "Game.hpp"
#include "Menu.hpp"
#include "Recorder.hpp"
#include "Remote.hpp"
#include "Tweaks.hpp"
#include "Ui.hpp"

#include <atomic>
#include <fstream>
#include <string>

using namespace Aurie;
using namespace YYTK;

namespace
{
	constexpr const char* k_Version = "0.21.1";
	constexpr const char* k_ProbeBuiltin = "code_is_compiled";
	constexpr const char* k_ProbeScript = "gml_Script_command_line_parameters_init";

	YYTKInterface* g_Yytk = nullptr;
	fs::path g_LogPath;
	std::atomic<bool> g_Probed = false;

	void LogLine(const std::string& Line, bool Truncate = false)
	{
		std::ofstream out(g_LogPath, Truncate ? std::ios::trunc : std::ios::app);
		out << Line << '\n';
		DbgPrintEx(LOG_SEVERITY_INFO, "[NlToyBox] %s", Line.c_str());
	}

	std::string StatusText(AurieStatus Status)
	{
		return std::string("error:") + AurieStatusToString(Status);
	}

	// 빌트인 호출이 되는지 본다. YYC 빌드이므로 참이 나와야 한다.
	std::string ProbeBuiltin()
	{
		CInstance* global_instance = nullptr;
		AurieStatus status = g_Yytk->GetGlobalInstance(&global_instance);
		if (!AurieSuccess(status))
			return StatusText(status);

		RValue result;
		status = g_Yytk->CallBuiltinEx(result, k_ProbeBuiltin, global_instance, global_instance, {});
		if (!AurieSuccess(status))
			return StatusText(status);

		return result.ToBoolean() ? "true" : "false";
	}

	// 게임 스크립트를 이름으로 찾을 수 있는지 본다.
	std::string ProbeScript()
	{
		PVOID routine = nullptr;
		const AurieStatus status = g_Yytk->GetNamedRoutinePointer(k_ProbeScript, &routine);
		if (!AurieSuccess(status))
			return StatusText(status);

		return routine ? "found" : "error:null pointer";
	}

	// 게임 스레드의 콜백에서 한 번만 시험한다. 어느 콜백이 먼저 왔는지 남긴다.
	// EVENT_FRAME 은 쓰지 않는다. YYToolkit v5.0.0c 는 Present 훅을 걸지 않는다 (스펙 §4.3).
	void ProbeOnce(const char* Trigger)
	{
		if (g_Probed.exchange(true))
			return;

		LogLine(std::string("trigger ") + Trigger);
		LogLine(std::string("builtin ") + k_ProbeBuiltin + " = " + ProbeBuiltin());
		LogLine(std::string("script ") + k_ProbeScript + " = " + ProbeScript());
		LogLine("probe done");
	}

	// 오브젝트 이벤트 코드가 실행될 때마다 온다. 래퍼의 인자 가운데 코드 객체(세 번째)만 쓴다.
	void CodeCallback(FWCodeEvent& CodeContext)
	{
		ProbeOnce("object_call");
		NlDump::Tick(std::get<2>(CodeContext.Arguments()));
		NlUi::GameTick();
		NlMenu::GameTick();
		NlRemote::GameTick();
		NlTweaks::GameTick();
	}

	// 게임 창이 메시지를 받을 때마다 온다.
	void WndProcCallback(FWWndProc& WndProcContext)
	{
		ProbeOnce("wndproc");
		NlUi::WndProc(WndProcContext);
	}

	AurieStatus Register(AurieModule* Module, EventTriggers Trigger, PVOID Routine, const char* Name)
	{
		const AurieStatus status = g_Yytk->CreateCallback(Module, Trigger, Routine, 0);
		if (!AurieSuccess(status))
			LogLine(std::string("error:CreateCallback ") + Name + " " + AurieStatusToString(status));

		return status;
	}
}

EXPORTED AurieStatus ModuleInitialize(
	IN AurieModule* Module,
	IN const fs::path& ModulePath
)
{
	// ModulePath 가 DLL 경로인지 폴더인지에 기대지 않는다.
	std::error_code ec;
	const fs::path module_dir = fs::is_directory(ModulePath, ec) ? ModulePath : ModulePath.parent_path();
	g_LogPath = module_dir / "NlToyBox.log";

	LogLine(std::string("NlToyBox ") + k_Version + " loaded", true);

	g_Yytk = YYTK::GetInterface();
	if (!g_Yytk)
	{
		LogLine("error:YYTK interface not found");
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
	}

	// 헤더(YYTK_MAJOR 5)와 실제 바이너리가 어긋났는지 보는 단서.
	short major = 0, minor = 0, patch = 0;
	g_Yytk->QueryVersion(major, minor, patch);
	LogLine("yytk " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch));

	// 콜백보다 먼저 준비한다. 콜백은 등록하자마자 게임 스레드에서 오기 시작한다.
	// 요청 파일이 있을 때만 덤프를 준비한다 (스펙: 데이터 오버레이 §3.2, §3.7).
	NlGame::Init(g_Yytk);
	NlDump::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
	NlUi::Init(Module, module_dir, [](const std::string& Line) { LogLine(Line); });
	NlTweaks::Init(module_dir, [](const std::string& Line) { LogLine(Line); }, NlUi::TestSets());
	NlRecorder::Init(Module, [](const std::string& Line) { LogLine(Line); });
	NlRemote::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
	// 게임 폴더: 모듈은 <게임>\mods\Aurie 에 있다. 인물 패널이 게임의 현지화 파일(localization\main.csv)에서 특성의 이름을 읽는다.
	NlPeople::Init([](const std::string& Line) { LogLine(Line); }, module_dir.parent_path().parent_path());
	NlCourt::Init([](const std::string& Line) { LogLine(Line); });
	NlWorld::Init([](const std::string& Line) { LogLine(Line); });
	NlDiplomacy::Init([](const std::string& Line) { LogLine(Line); });
	NlBuild::Init([](const std::string& Line) { LogLine(Line); });
	NlProduction::Init([](const std::string& Line) { LogLine(Line); });
	NlMenu::Init(module_dir, k_Version, [](const std::string& Line) { LogLine(Line); });
	NlUi::SetContent(NlMenu::Draw);

	AurieStatus status = Register(Module, EVENT_OBJECT_CALL, CodeCallback, "object_call");
	if (!AurieSuccess(status))
		return status;

	status = Register(Module, EVENT_WNDPROC, WndProcCallback, "wndproc");
	if (!AurieSuccess(status))
		return status;

	return AURIE_SUCCESS;
}
