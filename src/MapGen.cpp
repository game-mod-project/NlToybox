#include "MapGen.hpp"

#include "Access.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
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
	struct Snapshot			// 틱이 채우고 Draw 가 읽는다. 글과 수만
	{
		bool Ready = false, Screen = false;
		std::vector<double> Values, Stashed;		// MapKnobs() 의 차례. 길이는 ReadSnapshot 이 표의 크기로 잡는다(표에 줄을 더해도 어긋나지 않게)
		std::vector<bool> Read, StashedRead;
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
	char g_PresetName[33] = "";			// 창의 입력. 32바이트 + 끝(GoodPresetName 의 한도. Dear ImGui 는 넘치는 글자를 통째로 거절한다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	size_t IndexOf(const MapKnob& Knob)
	{
		return static_cast<size_t>(&Knob - &NlCore::MapKnobs()[0]);
	}

	// 생성기 화면인가(core/MapPlan 의 MapScreen): 초기화기의 __is_active, 설정의 구조체, 미리 보기 제어기(regenerate_map 을 가진 구조체), 게임 안이 아님.
	bool Screen()
	{
		double active = 0;
		const bool initializer = NlAccess::ReadNumber(std::string(NlCore::k_MapInitializerPath) + ".__is_active", active) && active != 0;
		RValue settings, preview;		// 이 함수 안에서만 든다
		std::string why;
		const bool read = NlAccess::Read(NlCore::ParseAskPath(NlCore::k_MapSettingsPath), settings, why) && settings.IsStruct();
		const bool previewRead = NlAccess::Read(NlCore::ParseAskPath(NlCore::k_MapPreviewPath), preview, why) && preview.IsStruct();
		return NlCore::MapScreen(initializer, read, previewRead, NlAccess::InGame());
	}

	void ReadSnapshot()
	{
		Snapshot next;
		const size_t count = NlCore::MapKnobs().size();
		next.Values.assign(count, 0);
		next.Stashed.assign(count, 0);
		next.Read.assign(count, false);
		next.StashedRead.assign(count, false);
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
		preset.Seed = -1;		// 씨앗 고정은 뺐다(파일의 seed= 칸은 꼴만 남는다. 언제나 무작위)
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
		Log("map preset loaded: " + Name + " (" + std::to_string(written) + " written)");
		return NlCore::MapReport(MapAct::PresetLoad, 'l', Name + " (" + std::to_string(written) + ")");
	}

	std::string PresetDeleteNow(const std::string& Name)
	{
		if (!NlCore::GoodPresetName(Name))
			return NlCore::MapReport(MapAct::PresetDelete, 'b', std::string());
		if (!NlCore::ErasePreset(g_Presets, Name))
			return NlCore::MapReport(MapAct::PresetDelete, 'm', Name);
		const bool saved = SavePresets();
		LoadPresets();		// 파일이 가진 것을 보인다(쓰지 못했으면 지운 것이 되살아난다)
		if (!saved)
		{
			Log("map preset file: could not write " + g_PresetsPath.string());
			return NlCore::MapReport(MapAct::PresetDelete, 'f', std::string());
		}
		Log("map preset deleted: " + Name);
		return NlCore::MapReport(MapAct::PresetDelete, 'x', Name);
	}


	// 씨앗 고정은 뺐다(2026-10-10 의 확인. research/31): regenerate_map() 이 안에서 set_generator_seed(-1) 을 부르고, 생성기는 get_generator_seed() 를 읽지 않는다
	// (모듈의 호출과 게임의 "생성" 둘 다 기록 0번). 원격 map seed 는 그 까닭만 답한다.
	std::string SeedNow(const MapCommand&)
	{
		return NlCore::MapReport(MapAct::Seed, 'u', std::string());
	}

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
	ImGui::SeparatorText("영지의 생성 설정");
	if (!g_Now.Ready)
		NlUi::Hint("상태를 읽습니다.");
	else if (!g_Now.Screen)
		NlUi::Hint("새 게임의 지도 화면(영주관 배치 전)에서만 됩니다. 영지를 고른 뒤 생성기 창이 보이는 화면에서 이 탭을 여세요.");
	else
	{
		// 수 하나의 줄: 이름 [값] [-][+]. 누르면 큐에 set 하나(틱이 쓴다. 범위는 코어가 당긴다)
		const auto knobLine = [&](const MapKnob& knob) {
			const size_t i = IndexOf(knob);
			const double value = g_Now.Values[i];
			ImGui::Text("%s %s", knob.Label, g_Now.Read[i] ? NlCore::Shortest(value).c_str() : "?");
			ImGui::SameLine();
			ImGui::BeginDisabled(!g_Now.Read[i] || value <= knob.Min);
			if (ImGui::SmallButton((std::string("-##") + knob.Key).c_str()))
			{
				MapCommand command;
				command.Act = MapAct::Set;
				command.Sets.emplace_back(knob.Key, value - 1);
				Push(command);
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::BeginDisabled(!g_Now.Read[i] || value >= knob.Max);
			if (ImGui::SmallButton((std::string("+##") + knob.Key).c_str()))
			{
				MapCommand command;
				command.Act = MapAct::Set;
				command.Sets.emplace_back(knob.Key, value + 1);
				Push(command);
			}
			ImGui::EndDisabled();
		};
		for (const NlCore::MapGroup group : { NlCore::MapGroup::Terrain, NlCore::MapGroup::Blocked, NlCore::MapGroup::Resource })
		{
			ImGui::TextDisabled("%s", NlCore::MapGroupWord(group));
			int shown = 0;
			for (const MapKnob& knob : NlCore::MapKnobs())
			{
				if (knob.Group != group)
					continue;
				if (shown++ % 4 != 0)
					ImGui::SameLine(0, 24);
				if (std::string(knob.Key) == "hills_distribution")
				{
					// 언덕 배치는 콤보(중심, 가장자리, 무작위)
					const size_t i = IndexOf(knob);
					ImGui::Text("%s", knob.Label);
					ImGui::SameLine();
					ImGui::SetNextItemWidth(110);
					if (ImGui::BeginCombo("##hills_distribution", NlCore::HillsDistributionWord(g_Now.Values[i]).c_str()))
					{
						for (int option = 0; option <= 2; option++)
							if (ImGui::Selectable(NlCore::HillsDistributionWord(option).c_str(), g_Now.Values[i] == option))
							{
								MapCommand command;
								command.Act = MapAct::Set;
								command.Sets.emplace_back(knob.Key, option);
								Push(command);
							}
						ImGui::EndCombo();
					}
				}
				else if (std::string(knob.Key) == "river")
				{
					const size_t i = IndexOf(knob);
					bool on = g_Now.Values[i] != 0;
					if (ImGui::Checkbox("강", &on))
					{
						MapCommand command;
						command.Act = MapAct::Set;
						command.Sets.emplace_back(knob.Key, on ? 1.0 : 0.0);
						Push(command);
					}
				}
				else
					knobLine(knob);
			}
		}
		if (ImGui::Button("다시 생성"))
		{
			MapCommand command;
			command.Act = MapAct::Regenerate;
			Push(command);
		}
		ImGui::SameLine();
		if (ImGui::Button("원래대로"))
		{
			MapCommand command;
			command.Act = MapAct::Restore;
			Push(command);
		}
		ImGui::SameLine();
		if (ImGui::Button("새로 읽기"))
			g_NextRead = 0;
	}
	NlUi::Hint("값은 게임의 생성기 창이 쓰는 자리(영지의 생성 설정)에 바로 씁니다. '다시 생성'은 게임의 함수(regenerate_map)를 부릅니다(창의 '생성'과 같습니다). "
		"'원래대로'는 영지의 원래 값으로 되돌립니다. 자원 8개는 창에 없는 값입니다: 단계는 자리의 수로 보였습니다(철 4, 점토 4 로 쓰자 광산과 점토 자리가 4개씩. 한 영지에서 한 번 잰 것). 씨앗 고정은 없습니다(생성기가 씨앗 함수를 읽지 않습니다). 막힘 4개는 영지의 모양이라 프리셋에 들지 않습니다.");

	ImGui::SeparatorText("프리셋");
	ImGui::SetNextItemWidth(160);
	ImGui::InputText("이름", g_PresetName, sizeof(g_PresetName));
	ImGui::SameLine();
	ImGui::BeginDisabled(!g_Now.Screen || !NlCore::GoodPresetName(g_PresetName));
	if (ImGui::Button("저장"))
	{
		MapCommand command;
		command.Act = MapAct::PresetSave;
		command.Name = g_PresetName;
		Push(command);
	}
	ImGui::EndDisabled();
	if (g_PresetName[0] != '\0' && !NlCore::GoodPresetName(g_PresetName))
	{
		ImGui::SameLine();
		ImGui::TextDisabled("이름: %s", NlCore::PresetNameRule());
	}
	for (const std::string& name : g_Now.Presets)
	{
		ImGui::TextUnformatted(name.c_str());
		ImGui::SameLine();
		ImGui::BeginDisabled(!g_Now.Screen);
		if (ImGui::SmallButton(("불러오기##" + name).c_str()))
		{
			MapCommand command;
			command.Act = MapAct::PresetLoad;
			command.Name = name;
			Push(command);
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::SmallButton(("지우기##" + name).c_str()))
		{
			MapCommand command;
			command.Act = MapAct::PresetDelete;
			command.Name = name;
			Push(command);
		}
	}
	NlUi::Hint("프리셋은 지형 5개와 자원 8개를 mods\\Aurie\\NlToyBox.maps.txt 에 이름으로 저장합니다. 불러오면 값만 채웁니다. 생성은 '다시 생성'으로.");
	if (!g_Last.empty())
		NlUi::Hint(g_Last.c_str());
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
			lines.push_back("seed " + (g_Now.SeedRead ? NlCore::Shortest(g_Now.Seed) : std::string("?")) + " (fixing not available)");
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
