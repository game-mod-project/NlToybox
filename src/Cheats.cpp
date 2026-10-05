#include "Cheats.hpp"

#include "Access.hpp"
#include "Recorder.hpp"
#include "core/AskPath.hpp"
#include "core/Hooks.hpp"
#include "core/Presets.hpp"
#include "core/Rate.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

using NlCore::Area;
using NlCore::Cheat;
using NlCore::CheatKind;
using NlCore::HasNumber;
using NlCore::IsHook;
using NlCore::Shortest;

namespace
{
	struct Item
	{
		const Cheat* Def = nullptr;
		bool On = false;			// Toggle: 켰다. Number: 값을 정했다
		double Number = 0;			// Number 가 써 넣는 값
		bool Restore = false;		// 방금 껐다. 다음 틱에 원래 값을 한 번 써 넣는다
		bool HasBase = false;
		double Base = 0;			// Number 의 처음 본 값
		bool Applied = false;		// Hook: 바꾸기를 걸었다
		double AppliedNumber = 0;	// HookScale: 걸어 둔 배율
		std::string HookName;		// Hook: 훅을 건 스크립트의 이름(끌 때 쓴다)
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		double Current = 0;
		std::string Note, Logged;
	};

	// 게임 속도는 게임의 함수로 건다(research/08): o_time_controller.__set_warp(배속). 게임이 스스로 (3) 으로 부르는 것을 기록했고,
	// 24 로 불러 흐름이 배속 1 의 약 24배가 되는 것을 쟀다. 1·3·6·12 는 게임의 단추가 쓰는 값(time_speed_variants)이다. time_warp_max 는 100 이다.
	constexpr const char* k_SetWarp = "inst:o_time_controller.__set_warp";
	constexpr const char* k_SetSpeed = "inst:o_time_controller.set_time_speed";		// 게임의 속도 단추가 쓰는 길. 0 번은 첫 속도이고 일시정지를 푼다
	constexpr double k_Factors[] = { 1, 3, 6, 12, 24, 50 };

	std::recursive_mutex g_Mutex;	// 아래 전부를 지킨다
	NlCheats::LogFn g_Log;
	std::vector<Item> g_Items;
	bool g_Changed = false;			// 상태 파일에 적을 것이 바뀌었다
	bool g_Dirty = true;			// 다음 틱에 바로 적용한다
	double g_NextApply = 0;
	double g_WarpAsked = 0;			// 창이 누른 배속. 0 이면 없다. 다음 틱이 게임의 함수로 건다
	bool g_PauseAsked = false, g_ResumeAsked = false;		// 창이나 원격이 청한 멈춤·다시 흐르게. 다음 틱이 게임의 함수를 부른다
	std::string g_WarpNote;			// 마지막으로 건 결과

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 게임 시간의 흐름을 재서 보여 준다(건 배속이 먹었는지 눈으로 본다. 배속 1 에서 실제 1초에 약 60 이었다. research/08).
	// 창이 열려 있는 동안만 잰다. 배속은 게임의 함수로 건다(위의 k_SetWarp).
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";
	constexpr const char* k_TimeWarp = "inst:o_time_controller.time_warp";
	NlCore::Rate g_Flow(1.0);
	bool g_TimeFound = false;		// 아래는 GameTick 이 채우는 스냅샷
	double g_FlowNow = 0, g_WarpNow = 0;

	void MeasureFlow(double Now, bool Visible)
	{
		double time = 0;
		g_TimeFound = Visible && NlAccess::ReadNumber(k_GameTime, time);
		if (!g_TimeFound)
		{
			g_Flow.Reset();
			g_FlowNow = 0;
			return;
		}
		g_Flow.Add(Now, time);
		g_FlowNow = g_Flow.Ready() ? g_Flow.PerSecond() : 0;
		if (!NlAccess::ReadNumber(k_TimeWarp, g_WarpNow))
			g_WarpNow = 0;
	}

	// ---- 게임 스레드 ----

	// 함수가 돌려주는 값을 바꾸는 항목(스펙 §3 의 수단 D). 켜면 훅을 걸고(없으면) 바꾸기를 켠다. 끄면 바꾸기만 끈다(훅은 떼지 않는다).
	// 대상이 메서드의 주소이면 그 인스턴스가 있어야 스크립트를 알 수 있다. 없으면 다음 틱에 다시 해 본다(그때는 훅의 자리를 쓰지 않는다).
	// 켠 것과 실제로 걸린 것을 틱마다 견준다: 원격의 unoverride 가 바꾸기를 꺼도 체크가 켜져 있으면 다시 건다(core/Hooks).
	void ApplyHook(Item& It)
	{
		// HookScale: 돌려주는 수에 창에서 정한 배율을 곱한다. 배율이 바뀌면 같은 훅에 새 배율을 다시 건다.
		const bool scale = It.Def->Kind == CheatKind::HookScale;
		const bool live = It.Applied && NlRecorder::Overriding(It.HookName);
		const bool current = NlCore::HookCurrent(It.On, It.Applied, scale, It.AppliedNumber, It.Number);
		switch (NlCore::ChooseHookStep(It.On, current, live))
		{
		case NlCore::HookStep::Apply:
		{
			const bool again = current;		// 걸어 둔 그대로인데 꺼져 있었다
			NlRecorder::Forced value;
			value.Kind = scale ? 'x' : 'b';
			value.Number = scale ? It.Number : It.Def->On;
			value.Whole = scale && It.Def->Off != 0;
			std::string name, why;
			if (NlRecorder::Override(It.Def->Path, value, name, why))
			{
				It.Applied = true;
				It.AppliedNumber = It.Number;
				It.HookName = name;
				It.Note = scale ? "배율 " + Shortest(It.Number) + " 을 걸었습니다" : "걸었습니다";
				It.Logged.clear();
				Log(std::string("cheat ") + It.Def->Id + ": overriding " + name + (scale ? " x" + Shortest(It.Number) : "")
					+ (again ? " again (it was turned off elsewhere)" : ""));
			}
			else
			{
				// 배율을 바꿔 다시 걸다 실패했으면 앞 배율의 바꾸기가 살아 있다. 걸었다는 표시를 남겨 다음 틱에 다시 걸고, 끄면 끌 수 있게 한다.
				It.Applied = NlCore::AppliedAfterFailure(live);
				const bool in_game = NlAccess::InGame();
				It.Note = in_game ? "걸지 못했습니다: " + why : "게임을 시작하면 적용";
				const std::string line = std::string("cheat ") + It.Def->Id + ": cannot override: " + why;
				if (in_game && line != It.Logged)		// 같은 까닭을 되풀이해 적지 않는다
				{
					It.Logged = line;
					Log(line);
				}
			}
			break;
		}
		case NlCore::HookStep::Remove:
			NlRecorder::Unoverride(It.HookName);
			It.Applied = false;
			It.Note.clear();
			Log(std::string("cheat ") + It.Def->Id + ": off");
			break;
		case NlCore::HookStep::None:
			break;
		}
		It.Found = true;
		It.Restore = false;
	}

	void Apply(Item& It, bool Visible)
	{
		if (IsHook(It.Def->Kind))
		{
			ApplyHook(It);
			return;
		}
		if (It.Def->Kind == CheatKind::Custom || It.Def->Kind == CheatKind::CustomScale)
		{
			It.Restore = false;		// 모듈의 코드가 IsOn·Factor 를 보고 한다(src/Build.cpp, src/Production.cpp)
			return;
		}
		if (!It.On && !It.Restore && !Visible)
			return;			// 켜지 않았고 보는 사람도 없으면 읽지도 않는다

		double current = 0;
		It.Found = NlAccess::ReadNumber(It.Def->Path, current);
		if (!It.Found)
		{
			// 대상이 없다(메뉴에서는 o_debug 가 없다). 되돌릴 것도 대상과 함께 사라졌다.
			It.Restore = false;
			It.Note = It.On ? "게임을 시작하면 적용" : "";
			return;
		}
		It.Current = current;

		const bool number = It.Def->Kind == CheatKind::Number;
		if (number && !It.HasBase)
		{
			It.Base = current;
			It.HasBase = true;
		}

		double wanted = current;
		if (It.On)
			wanted = number ? It.Number : It.Def->On;
		else if (It.Restore)
			wanted = number ? It.Base : It.Def->Off;
		It.Restore = false;
		if (wanted == current)
		{
			It.Note = It.On ? "써 넣음" : "";
			return;
		}

		std::string why;
		const bool ok = NlAccess::WriteNumber(It.Def->Path, wanted, why);
		if (ok)
			It.Current = wanted;
		It.Note = ok ? (It.On ? "써 넣음" : "") : "써지지 않음: " + why;

		// 같은 결과를 되풀이해 적지 않는다(게임이 값을 되돌리면 0.5초마다 다시 쓴다).
		const std::string line = std::string("cheat ") + It.Def->Id + " = " + Shortest(wanted) + (ok ? ": ok" : ": " + why);
		if (line != It.Logged)
		{
			It.Logged = line;
			Log(line);
		}
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 이름 옆의 "(?)": 효과를 아직 확인하지 않은 항목이다. 올리면 주소와 설명이 보인다.
	void DrawHelp(const Cheat& Def)
	{
		if (!Def.Verified)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(?)");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s\n%s%s", Def.Path, Def.Help, Def.Verified ? "" : "\n효과 확인 전");
	}

	void TurnOff(Item& It)
	{
		It.On = false;
		It.Restore = true;
		g_Changed = g_Dirty = true;
	}

	void DrawToggle(Item& It)
	{
		bool on = It.On;
		if (ImGui::Checkbox(It.Def->Label, &on))
		{
			if (on)
			{
				It.On = true;
				It.Restore = false;
				g_Changed = g_Dirty = true;
			}
			else
				TurnOff(It);
		}
		DrawHelp(*It.Def);
		if (!It.Note.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", It.Note.c_str());
		}
	}

	void DrawNumber(Item& It)
	{
		double value = It.On ? It.Number : It.Current;
		ImGui::SetNextItemWidth(130);
		if (ImGui::InputDouble("##v", &value, 0, 0, "%.6g", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			It.Number = std::clamp(value, It.Def->Min, It.Def->Max);
			It.On = true;
			It.Restore = false;
			g_Changed = g_Dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!It.On);
		if (ImGui::Button("원래대로"))
			TurnOff(It);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextUnformatted(It.Def->Label);
		DrawHelp(*It.Def);

		ImGui::SameLine();
		if (It.On)
			ImGui::TextDisabled("%s", It.Note.c_str());
		else if (It.Found)
			ImGui::TextDisabled("지금 %s", Shortest(It.Current).c_str());
		else
			ImGui::TextDisabled("게임을 시작하면 보입니다");
	}

	// 배율 항목(HookScale, CustomScale): 체크로 켜고 끄고, 수는 Enter 로 넣는다. 1 이 원래 값이다.
	void DrawScale(Item& It)
	{
		bool on = It.On;
		if (ImGui::Checkbox("##on", &on))
		{
			if (on)
			{
				if (!(It.Number > 0))
					It.Number = It.Def->On;
				It.On = true;
				It.Restore = false;
				g_Changed = g_Dirty = true;
			}
			else
				TurnOff(It);
		}
		ImGui::SameLine();
		double value = It.Number > 0 ? It.Number : It.Def->On;
		ImGui::SetNextItemWidth(90);
		if (ImGui::InputDouble("##v", &value, 0, 0, "%.4g", ImGuiInputTextFlags_EnterReturnsTrue))
		{
			It.Number = std::clamp(value, It.Def->Min, It.Def->Max);
			It.On = true;
			It.Restore = false;
			g_Changed = g_Dirty = true;
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(It.Def->Label);
		DrawHelp(*It.Def);
		ImGui::SameLine();
		if (It.On)
			ImGui::TextDisabled("%s", It.Note.c_str());
		else
			ImGui::TextDisabled("%s ~ %s", Shortest(It.Def->Min).c_str(), Shortest(It.Def->Max).c_str());
	}
}

void NlCheats::Init(LogFn Log_, const NlCore::CheatState& State)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	g_Items.clear();

	std::string loaded;
	for (const Cheat& cheat : NlCore::Cheats())
	{
		Item item;
		item.Def = &cheat;
		if (!HasNumber(cheat.Kind))
			item.On = State.On.count(cheat.Id) > 0;
		else if (const auto it = State.Numbers.find(cheat.Id); it != State.Numbers.end())
		{
			item.On = true;
			item.Number = it->second;
		}
		if (item.On)
			loaded += std::string(" ") + cheat.Id + (HasNumber(cheat.Kind) ? "=" + Shortest(item.Number) : "");
		g_Items.push_back(std::move(item));
	}
	g_Dirty = true;
	Log("cheat state:" + (loaded.empty() ? std::string(" none") : loaded));
}

void NlCheats::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	MeasureFlow(Now, Visible);		// 흐름을 재서 보여 준다. 배속은 아래에서 게임의 함수로 건다

	if (g_WarpAsked > 0)
	{
		const double factor = g_WarpAsked;
		g_WarpAsked = 0;
		YYTK::RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log("speed call __set_warp(" + Shortest(factor) + ")");		// 부르기 전에 남긴다
		const bool ok = NlAccess::CallMethod(NlCore::ParseAskPath(k_SetWarp), { YYTK::RValue(factor) }, result, why);
		g_WarpNote = ok ? "배속 " + Shortest(factor) + " 을 걸었습니다" : "걸지 못했습니다: " + why;
		Log(std::string("speed: ") + (ok ? "ok" : why));
	}

	if (g_PauseAsked || g_ResumeAsked)
	{
		const bool pause = g_PauseAsked;
		g_PauseAsked = g_ResumeAsked = false;
		YYTK::RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log(pause ? "speed call __set_warp(0)" : "speed call set_time_speed(0)");		// 부르기 전에 남긴다
		const bool ok = NlAccess::CallMethod(NlCore::ParseAskPath(pause ? k_SetWarp : k_SetSpeed), { YYTK::RValue(0.0) }, result, why);
		g_WarpNote = !ok ? "하지 못했습니다: " + why : pause ? "시간을 멈췄습니다" : "시간이 다시 흐릅니다";
		Log(std::string("speed: ") + (ok ? "ok" : why));
	}

	if (!g_Dirty && Now < g_NextApply)
		return;
	g_Dirty = false;
	g_NextApply = Now + 0.5;		// 게임이 값을 다시 만들면(새 게임, 불러오기) 다시 써 넣는다
	for (Item& item : g_Items)
		Apply(item, Visible);
}

void NlCheats::DrawArea(Area Where)
{
	std::lock_guard lock(g_Mutex);
	bool any = false, any_on = false;
	for (Item& item : g_Items)
	{
		if (item.Def->Where != Where)
			continue;
		any = true;
		any_on = any_on || item.On;
		ImGui::PushID(item.Def->Id);
		if (item.Def->Kind == CheatKind::Number)
			DrawNumber(item);
		else if (HasNumber(item.Def->Kind))
			DrawScale(item);		// HookScale, CustomScale: 체크와 배율
		else
			DrawToggle(item);		// Toggle, Hook, Custom 은 모두 체크 하나다
		ImGui::PopID();
	}
	if (!any)
	{
		ImGui::TextDisabled("이 영역은 %d단계에서 채웁니다.", NlCore::GetArea(Where).Stage);
		return;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::BeginDisabled(!any_on);
	if (ImGui::Button("이 영역 모두 끄기"))
		for (Item& item : g_Items)
			if (item.Def->Where == Where && item.On)
				TurnOff(item);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("(?) 는 효과를 아직 확인하지 않은 항목입니다. 수는 Enter 로 써 넣습니다.");
}

namespace
{
	std::string g_PresetNote;		// 마지막으로 건 묶음

	// 묶음을 건다(g_Mutex 를 든 채 부른다). 돌려주는 것: 켜 둔 항목의 수.
	int ApplyPresetLocked(const NlCore::Preset& Preset)
	{
		int on = 0;
		for (Item& item : g_Items)
		{
			const NlCore::PresetItem* want = nullptr;
			for (const NlCore::PresetItem& candidate : Preset.Items)
				if (std::string(candidate.Id) == item.Def->Id)
					want = &candidate;
			if (!want)
			{
				if (item.On)
					TurnOff(item);
				continue;
			}
			const bool number = HasNumber(item.Def->Kind);
			const double value = number ? std::clamp(want->Number, item.Def->Min, item.Def->Max) : 0;
			if (!item.On || (number && item.Number != value))
			{
				if (number)
					item.Number = value;
				item.On = true;
				item.Restore = false;
				g_Changed = g_Dirty = true;
			}
			on++;
		}
		g_PresetNote = std::string(Preset.Label) + ": " + std::to_string(on) + "개를 켜 두고 나머지는 껐습니다";
		Log(std::string("preset ") + Preset.Key + ": " + std::to_string(on) + " on");
		return on;
	}

	// 묶음이 켜는 것을 글로: "이름, 이름 x0.1, …"
	std::string PresetList(const NlCore::Preset& Preset)
	{
		std::string text;
		for (const NlCore::PresetItem& want : Preset.Items)
		{
			const Cheat* cheat = NlCore::FindCheat(want.Id);
			if (!cheat)
				continue;
			text += (text.empty() ? "" : ", ") + std::string(cheat->Label) + (HasNumber(cheat->Kind) ? " x" + Shortest(want.Number) : "");
		}
		return text.empty() ? "켜는 것이 없습니다" : text;
	}
}

void NlCheats::DrawPresets()
{
	std::lock_guard lock(g_Mutex);
	ImGui::PushTextWrapPos(0.0f);
	ImGui::TextUnformatted("프리셋은 플레이에서 확인된 항목의 묶음입니다. 누르면 묶음에 없는 표의 항목은 끄고 묶음의 항목은 켭니다. "
		"탐색기의 잠금, 배율 7개, 한 번 하는 단추(금화, 병사 등)는 건드리지 않습니다.");
	ImGui::PopTextWrapPos();
	ImGui::Spacing();
	for (const NlCore::Preset& preset : NlCore::Presets())
	{
		ImGui::PushID(preset.Key);
		if (ImGui::Button(preset.Label, ImVec2(150, 0)))
			ApplyPresetLocked(preset);
		ImGui::SameLine();
		ImGui::TextUnformatted(preset.Help);
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", PresetList(preset).c_str());
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		ImGui::PopID();
	}
	if (!g_PresetNote.empty())
		ImGui::TextDisabled("%s", g_PresetNote.c_str());
}

bool NlCheats::ApplyPreset(const std::string& Key, std::string& Text)
{
	std::lock_guard lock(g_Mutex);
	const NlCore::Preset* preset = NlCore::FindPreset(Key);
	if (!preset)
		return false;
	ApplyPresetLocked(*preset);
	Text = g_PresetNote + " (" + PresetList(*preset) + ")";
	return true;
}

void NlCheats::AskTime(bool Pause)
{
	std::lock_guard lock(g_Mutex);
	(Pause ? g_PauseAsked : g_ResumeAsked) = true;
}

void NlCheats::DrawTime()
{
	std::lock_guard lock(g_Mutex);
	ImGui::TextWrapped("배속을 누르면 게임의 함수로 게임 속도를 겁니다. 게임 화면에서, 일시정지를 푼 채로 누르세요. "
		"게임의 속도 단추를 누르면 게임의 배속으로 돌아갑니다.");
	ImGui::Spacing();

	if (!g_TimeFound)
		ImGui::TextDisabled("시간 컨트롤러(o_time_controller)가 아직 없습니다.");
	else
	{
		ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f", g_FlowNow);		// 배속 1 에서 약 60 이었다(research/08)
		ImGui::SameLine();
		ImGui::TextDisabled("time_warp %s", Shortest(g_WarpNow).c_str());
	}

	ImGui::BeginDisabled(!g_TimeFound);
	for (const double factor : k_Factors)
	{
		const std::string label = "x" + Shortest(factor);
		if (ImGui::Button(label.c_str(), ImVec2(64, 0)))
			g_WarpAsked = factor;
		ImGui::SameLine();
	}
	ImGui::NewLine();
	// 멈춤: __set_warp(0). 다시: set_time_speed(0)(게임의 첫 속도 단추와 같다. 일시정지를 푼다). 둘 다 실행 묶음에서 수십 번 불러 봤다(research/13).
	if (ImGui::Button("멈춤", ImVec2(64, 0)))
		g_PauseAsked = true;
	ImGui::SameLine();
	if (ImGui::Button("다시 흐르게", ImVec2(110, 0)))
		g_ResumeAsked = true;
	ImGui::EndDisabled();
	ImGui::NewLine();

	if (!g_WarpNote.empty())
		ImGui::TextDisabled("%s", g_WarpNote.c_str());
}

bool NlCheats::HasItems(Area Where)
{
	for (const Cheat& cheat : NlCore::Cheats())
		if (cheat.Where == Where)
			return true;
	return false;
}

bool NlCheats::TakeChanges(std::set<std::string>& On, std::map<std::string, double>& Numbers)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Changed)
		return false;
	g_Changed = false;

	On.clear();
	Numbers.clear();
	for (const Item& item : g_Items)
	{
		if (!item.On)
			continue;
		if (!HasNumber(item.Def->Kind))
			On.insert(item.Def->Id);
		else
			Numbers[item.Def->Id] = item.Number;
	}
	return true;
}

bool NlCheats::IsOn(const std::string& Id)
{
	std::lock_guard lock(g_Mutex);
	for (const Item& item : g_Items)
		if (Id == item.Def->Id)
			return item.On;
	return false;
}

void NlCheats::SetNote(const std::string& Id, const std::string& Note)
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
		if (Id == item.Def->Id)
			item.Note = Note;
}

bool NlCheats::Set(const std::string& Id, bool On)
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
	{
		if (Id != item.Def->Id)
			continue;
		if (On && item.Def->Kind == CheatKind::Number)
			return false;			// 써 넣을 값이 있어야 한다(SetNumber)
		if (On && !item.On)
		{
			if (HasNumber(item.Def->Kind) && !(item.Number > 0))
				item.Number = item.Def->On;		// 배율 항목: 표가 내놓는 배율
			item.On = true;
			item.Restore = false;
			g_Changed = g_Dirty = true;
		}
		else if (!On && item.On)
			TurnOff(item);
		return true;
	}
	return false;
}

bool NlCheats::SetNumber(const std::string& Id, double Value)
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
	{
		if (Id != item.Def->Id || !HasNumber(item.Def->Kind) || !std::isfinite(Value))
			continue;
		item.Number = std::clamp(Value, item.Def->Min, item.Def->Max);
		item.On = true;
		item.Restore = false;
		g_Changed = g_Dirty = true;
		return true;
	}
	return false;
}

bool NlCheats::Factor(const std::string& Id, double& Out)
{
	std::lock_guard lock(g_Mutex);
	for (const Item& item : g_Items)
		if (Id == item.Def->Id && HasNumber(item.Def->Kind) && item.On)
		{
			Out = item.Number;
			return true;
		}
	return false;
}

int NlCheats::ActiveCount()
{
	std::lock_guard lock(g_Mutex);
	const auto on = std::count_if(g_Items.begin(), g_Items.end(), [](const Item& item) { return item.On; });
	return static_cast<int>(on);
}

void NlCheats::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
		if (item.On)
			TurnOff(item);
}
