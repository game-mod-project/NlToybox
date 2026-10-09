#include "MapGen.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Recorder.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <fstream>
#include <mutex>

using namespace YYTK;
using NlCore::MapAct;
using NlCore::MapCommand;
using NlCore::MapKnob;

namespace
{
	constexpr size_t k_Knobs = 17;

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다. 글과 수만
	{
		bool Ready = false, Screen = false;
		std::array<double, k_Knobs> Values{}, Stashed{};
		std::array<bool, k_Knobs> Read{}, StashedRead{};
		double Seed = -1;
		bool SeedRead = false;
		std::vector<std::string> Presets;		// 이름순
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlMapGen::LogFn g_Log;
	std::deque<MapCommand> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	Snapshot g_Now;
	std::vector<NlCore::MapPreset> g_Presets;
	std::filesystem::path g_PresetsPath;
	double g_NextRead = 0;
	bool g_Busy = false;				// 하는 중이다(다시 생성은 오브젝트 이벤트를 많이 낸다. 다시 들어온 틱은 아무것도 하지 않는다)
	char g_PresetName[40] = "";			// 창의 입력
	// 씨앗 고정(Task 5): 훅의 이름과 바라는 상태
	bool g_SeedOn = false;
	double g_SeedValue = -1;
	std::string g_SeedHook;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	size_t IndexOf(const MapKnob& Knob)
	{
		return static_cast<size_t>(&Knob - &NlCore::MapKnobs()[0]);
	}

	// 생성기 화면인가(core/MapPlan 의 MapScreen): 초기화기의 __is_active, 설정의 구조체, 게임 안이 아님.
	bool Screen()
	{
		double active = 0;
		const bool initializer = NlAccess::ReadNumber(std::string(NlCore::k_MapInitializerPath) + ".__is_active", active) && active != 0;
		RValue settings;		// 이 함수 안에서만 든다
		std::string why;
		const bool read = NlAccess::Read(NlCore::ParseAskPath(NlCore::k_MapSettingsPath), settings, why) && settings.IsStruct();
		return NlCore::MapScreen(initializer, read, NlAccess::InGame());
	}

	void ReadSnapshot()
	{
		Snapshot next;
		next.Screen = Screen();
		if (next.Screen)
		{
			for (const MapKnob& knob : NlCore::MapKnobs())
			{
				const size_t i = IndexOf(knob);
				next.Read[i] = NlAccess::ReadNumber(NlCore::MapFieldPath(knob), next.Values[i]);
				next.StashedRead[i] = NlAccess::ReadNumber(NlCore::MapStashedPath(knob), next.Stashed[i]);
			}
			next.SeedRead = NlAccess::ReadNumber(std::string(NlCore::k_MapInitializerPath) + ".__generator_seed", next.Seed);
		}
		for (const NlCore::MapPreset& preset : g_Presets)
			next.Presets.push_back(preset.Name);
		std::sort(next.Presets.begin(), next.Presets.end());
		next.Ready = true;
		g_Now = std::move(next);
	}

	void LoadPresets()
	{
		g_Presets.clear();
		if (std::ifstream in(g_PresetsPath); in)
			g_Presets = NlCore::ParseMapPresets(in);
	}

	bool SavePresets()
	{
		std::ofstream out(g_PresetsPath, std::ios::trunc);
		if (!out)
			return false;
		out << NlCore::FormatMapPresets(g_Presets);
		return static_cast<bool>(out);
	}

	// 칸 하나에 쓴다(범위는 코어가 당긴다). 쓴 뒤 다시 읽어 판정한다(NlAccess::WriteNumber 가 한다).
	bool WriteKnob(const MapKnob& Knob, double Value, std::string& Why)
	{
		const double wanted = NlCore::ClampMapValue(Knob, Value);
		return NlAccess::WriteNumber(NlCore::MapFieldPath(Knob), wanted, Why);
	}

	std::string SetNow(const MapCommand& C)
	{
		std::string wrote, failed;
		for (const auto& [key, value] : C.Sets)
		{
			const MapKnob* knob = NlCore::FindMapKnob(key);
			if (!knob)
				return NlCore::MapReport(MapAct::Set, 'k', key);
			std::string why;
			if (WriteKnob(*knob, value, why))
				wrote += (wrote.empty() ? "" : " ") + key + "=" + NlCore::Shortest(NlCore::ClampMapValue(*knob, value));
			else
				failed += (failed.empty() ? "" : ", ") + key + ": " + why;
		}
		Log("map set " + wrote + (failed.empty() ? std::string() : " (failed " + failed + ")"));
		return failed.empty() ? NlCore::MapReport(MapAct::Set, 'w', wrote) : NlCore::MapReport(MapAct::Set, 'f', failed);
	}

	std::string RegenerateNow()
	{
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log("map call MapPreviewController.regenerate_map()");		// 부르기 전에 남긴다(게임이 인자 없이 부르는 꼴. research/31)
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(std::string(NlCore::k_MapPreviewPath) + ".regenerate_map"), {}, result, why))
			return NlCore::MapReport(MapAct::Regenerate, 'f', why);
		return NlCore::MapReport(MapAct::Regenerate, 'd', std::string());
	}

	std::string RestoreNow()
	{
		int done = 0, failed = 0;
		for (const MapKnob& knob : NlCore::MapKnobs())
		{
			double original = 0;
			std::string why;
			if (NlAccess::ReadNumber(NlCore::MapStashedPath(knob), original) && WriteKnob(knob, original, why))
				done++;
			else
				failed++;
		}
		Log("map restore: " + std::to_string(done) + " written, " + std::to_string(failed) + " failed");
		return failed == 0 ? NlCore::MapReport(MapAct::Restore, 'd', std::to_string(done)) : NlCore::MapReport(MapAct::Restore, 'f', std::to_string(failed));
	}

	std::string PresetSaveNow(const std::string& Name)
	{
		if (!NlCore::GoodPresetName(Name))
			return NlCore::MapReport(MapAct::PresetSave, 'b', std::string());
		NlCore::MapPreset preset;
		preset.Name = Name;
		for (const MapKnob& knob : NlCore::MapKnobs())
		{
			double value = 0;
			if (knob.InPreset && NlAccess::ReadNumber(NlCore::MapFieldPath(knob), value))
				preset.Values[knob.Key] = value;
		}
		preset.Seed = g_SeedOn ? g_SeedValue : -1;
		NlCore::UpsertPreset(g_Presets, std::move(preset));
		if (!SavePresets())
			return NlCore::MapReport(MapAct::PresetSave, 'f', std::string());
		LoadPresets();		// 파일이 가진 것을 보인다(스펙 §4: Init 과 저장·지움 뒤에 읽는다)
		Log("map preset saved: " + Name);
		return NlCore::MapReport(MapAct::PresetSave, 's', Name);
	}

	std::string PresetLoadNow(const std::string& Name)
	{
		if (!NlCore::GoodPresetName(Name))
			return NlCore::MapReport(MapAct::PresetLoad, 'b', std::string());
		const NlCore::MapPreset* preset = NlCore::FindPreset(g_Presets, Name);
		if (!preset)
			return NlCore::MapReport(MapAct::PresetLoad, 'm', Name);
		int written = 0;
		for (const auto& [key, value] : preset->Values)
		{
			const MapKnob* knob = NlCore::FindMapKnob(key);
			std::string why;
			if (knob && WriteKnob(*knob, value, why))
				written++;
		}
		if (preset->Seed >= 0 && NlCore::SeedHookVerified())
		{
			g_SeedOn = true;
			g_SeedValue = preset->Seed;
		}
		Log("map preset loaded: " + Name + " (" + std::to_string(written) + " written)");
		return NlCore::MapReport(MapAct::PresetLoad, 'l', Name + " (" + std::to_string(written) + ")");
	}

	std::string PresetDeleteNow(const std::string& Name)
	{
		if (!NlCore::GoodPresetName(Name))
			return NlCore::MapReport(MapAct::PresetDelete, 'b', std::string());
		if (!NlCore::ErasePreset(g_Presets, Name))
			return NlCore::MapReport(MapAct::PresetDelete, 'm', Name);
		if (!SavePresets())
			Log("map preset file: could not write " + g_PresetsPath.string());
		LoadPresets();
		Log("map preset deleted: " + Name);
		return NlCore::MapReport(MapAct::PresetDelete, 'x', Name);
	}

	std::string SeedNow(const MapCommand& C);		// Task 5

	// 한 일. 화면이 아니면 쓰지도 부르지도 않는다(프리셋 지우기와 씨앗은 화면과 무관하다).
	std::string DoNow(const MapCommand& C)
	{
		const bool screen = Screen();
		switch (C.Act)
		{
		case MapAct::Set: return screen ? SetNow(C) : NlCore::MapReport(MapAct::Set, 'n', std::string());
		case MapAct::Regenerate: return screen ? RegenerateNow() : NlCore::MapReport(MapAct::Regenerate, 'n', std::string());
		case MapAct::Restore: return screen ? RestoreNow() : NlCore::MapReport(MapAct::Restore, 'n', std::string());
		case MapAct::PresetSave: return screen ? PresetSaveNow(C.Name) : NlCore::MapReport(MapAct::PresetSave, 'n', std::string());
		case MapAct::PresetLoad: return screen ? PresetLoadNow(C.Name) : NlCore::MapReport(MapAct::PresetLoad, 'n', std::string());
		case MapAct::PresetDelete: return PresetDeleteNow(C.Name);
		case MapAct::Seed: return SeedNow(C);
		default: return std::string();
		}
	}

	std::string Remember(std::string Text)
	{
		g_Last = Text;
		return Text;
	}

	constexpr size_t k_MaxQueue = 4;		// 창이 쌓아 둘 청의 수. 넘치면 받지 않고 결과 줄에 적는다

	void Push(MapCommand Command)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_Last = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(std::move(Command));
	}

	std::string SeedNow(const MapCommand&)
	{
		return NlCore::MapReport(MapAct::Seed, 'u', std::string());		// Task 5 가 채운다
	}
}

void NlMapGen::Init(LogFn Log_, const std::filesystem::path& ModuleDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	g_PresetsPath = ModuleDir / "NlToyBox.maps.txt";
	LoadPresets();
	Log("map: " + std::to_string(g_Presets.size()) + " preset(s) from " + g_PresetsPath.filename().string());
}

void NlMapGen::Tick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const bool read = Visible && Now >= g_NextRead;
	if (g_Queue.empty() && !read)		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다)
	{
		if (!Visible && g_Now.Ready)
			g_Now = Snapshot{};		// 패널을 다시 열면 새로 읽은 것을 보인다
		return;
	}
	const NlCore::ScopedFlag busy(g_Busy);
	if (!g_Queue.empty())
	{
		const MapCommand command = g_Queue.front();
		g_Queue.pop_front();
		Remember(DoNow(command));
		g_NextRead = 0;			// 한 뒤에는 바로 다시 읽는다
	}
	if (Visible && Now >= g_NextRead)
	{
		g_NextRead = Now + 1;
		ReadSnapshot();
	}
}

void NlMapGen::Draw()
{
	std::lock_guard lock(g_Mutex);
	NlUi::Hint("지도 탭은 Task 4 에서 그린다.");
}

std::vector<std::string> NlMapGen::Do(const NlCore::MapCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	if (Command.Act == MapAct::Show)
	{
		ReadSnapshot();
		std::vector<std::string> lines;
		lines.push_back(std::string("screen ") + (g_Now.Screen ? "yes" : "no"));
		if (g_Now.Screen)
		{
			for (const MapKnob& knob : NlCore::MapKnobs())
			{
				const size_t i = IndexOf(knob);
				lines.push_back(std::string(knob.Key) + " " + (g_Now.Read[i] ? NlCore::Shortest(g_Now.Values[i]) : "?") + " (original " + (g_Now.StashedRead[i] ? NlCore::Shortest(g_Now.Stashed[i]) : "?") + ")");
			}
			lines.push_back("seed " + (g_Now.SeedRead ? NlCore::Shortest(g_Now.Seed) : std::string("?")) + (g_SeedOn ? " (fixed " + NlCore::Shortest(g_SeedValue) + ")" : std::string()));
		}
		std::string names;
		for (const std::string& name : g_Now.Presets)
			names += (names.empty() ? "" : ", ") + name;
		lines.push_back("presets: " + (names.empty() ? std::string("(none)") : names));
		return lines;
	}
	const std::string text = Remember(DoNow(Command));
	g_NextRead = 0;
	return { text };
}
