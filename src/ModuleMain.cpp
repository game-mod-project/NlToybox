// NlToyBox Phase 0 모듈.
// 붙었다는 증거만 남긴다. 모듈 옆 NlToyBox.log 에 적재, YYToolkit 버전, 프로브 두 개의 결과를 쓴다.
// 줄 형식은 tools/check-load.ps1 이 그대로 찾는다 (스펙 §4.3). 바꾸면 그쪽도 바꾼다.

#include <YYTK_Shared.hpp>

#include <fstream>
#include <string>

using namespace Aurie;
using namespace YYTK;

namespace
{
	constexpr const char* k_Version = "0.0.1";
	constexpr const char* k_ProbeBuiltin = "code_is_compiled";
	constexpr const char* k_ProbeScript = "gml_Script_command_line_parameters_init";

	YYTKInterface* g_Yytk = nullptr;
	fs::path g_LogPath;
	bool g_Probed = false;

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

	void FrameCallback(FWFrame& FrameContext)
	{
		UNREFERENCED_PARAMETER(FrameContext);

		// 첫 프레임에 한 번만 시험한다. 러너가 준비된 뒤여야 빌트인을 부를 수 있다.
		if (g_Probed)
			return;
		g_Probed = true;

		LogLine(std::string("builtin ") + k_ProbeBuiltin + " = " + ProbeBuiltin());
		LogLine(std::string("script ") + k_ProbeScript + " = " + ProbeScript());
		LogLine("probe done");
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

	const AurieStatus status = g_Yytk->CreateCallback(Module, EVENT_FRAME, FrameCallback, 0);
	if (!AurieSuccess(status))
	{
		LogLine("error:CreateCallback " + std::string(AurieStatusToString(status)));
		return status;
	}

	return AURIE_SUCCESS;
}
