#include "Cheats.hpp"

#include "Access.hpp"
#include "Recorder.hpp"
#include "core/AskPath.hpp"
#include "core/SpeedControl.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

using NlCore::Area;
using NlCore::Cheat;
using NlCore::CheatKind;
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
		std::string HookName;		// Hook: 훅을 건 스크립트의 이름(끌 때 쓴다)
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		double Current = 0;
		std::string Note, Logged;
	};

	// 게임 속도는 게임의 함수로 건다(research/08): o_time_controller.__set_warp(배속). 게임이 스스로 (3) 으로 부르는 것을 기록했고,
	// 24 로 불러 흐름이 배속 1 의 약 24배가 되는 것을 쟀다. 1·3·6·12 는 게임의 단추가 쓰는 값(time_speed_variants)이다. time_warp_max 는 100 이다.
	constexpr const char* k_SetWarp = "inst:o_time_controller.__set_warp";
	constexpr double k_Factors[] = { 1, 3, 6, 12, 24, 50 };

	std::recursive_mutex g_Mutex;	// 아래 전부를 지킨다
	NlCheats::LogFn g_Log;
	std::vector<Item> g_Items;
	bool g_Changed = false;			// 상태 파일에 적을 것이 바뀌었다
	bool g_Dirty = true;			// 다음 틱에 바로 적용한다
	double g_NextApply = 0;
	double g_WarpAsked = 0;			// 창이 누른 배속. 0 이면 없다. 다음 틱이 게임의 함수로 건다
	std::string g_WarpNote;			// 마지막으로 건 결과

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 게임 속도(core/SpeedControl). 후보의 이름은 덤프의 o_time_controller 에서 봤다. 어느 것이 속도를 정하는지는 모른다:
	// 처음 누를 때 차례로 써 보고 __game_time 의 흐름으로 고른다(스펙 §9). 읽고 쓰는 일은 틱에서만 일어난다.
	NlCore::SpeedControl& Speed()
	{
		static NlCore::SpeedControl control(
			NlCore::SpeedPaths{
				"inst:o_time_controller.__game_time",
				"inst:o_time_controller.time_warp",
				{
					{ "time_warp_new", "inst:o_time_controller.time_warp_new", "", "" },
					{ "time_warp", "inst:o_time_controller.time_warp", "", "" },
					{ "__debug_custom_wrap", "inst:o_time_controller.__debug_custom_wrap", "", "" },
					{ "time_speed_variants[time_speed_index]", "", "inst:o_time_controller.time_speed_variants",
						"inst:o_time_controller.time_speed_index" },
				},
			},
			NlCore::SpeedIo{
				[](const std::string& path, double& out) { return NlAccess::ReadNumber(path, out); },
				[](const std::string& path, double value, std::string& why) { return NlAccess::WriteNumber(path, value, why); },
				[](const std::string& line) { Log(line); },
			});
		return control;
	}

	// ---- 게임 스레드 ----

	// 함수가 돌려주는 값을 바꾸는 항목(스펙 §3 의 수단 D). 켜면 훅을 걸고(없으면) 바꾸기를 켠다. 끄면 바꾸기만 끈다(훅은 떼지 않는다).
	// 대상이 메서드의 주소이면 그 인스턴스가 있어야 스크립트를 알 수 있다. 없으면 다음 틱에 다시 해 본다.
	void ApplyHook(Item& It)
	{
		if (It.On && !It.Applied)
		{
			NlRecorder::Forced value;
			value.Kind = 'b';
			value.Number = It.Def->On;
			std::string name, why;
			if (NlRecorder::Override(It.Def->Path, value, name, why))
			{
				It.Applied = true;
				It.HookName = name;
				It.Note = "걸었습니다";
				Log(std::string("cheat ") + It.Def->Id + ": overriding " + name);
			}
			else
				It.Note = NlAccess::InGame() ? "걸지 못했습니다: " + why : "게임을 시작하면 적용";
		}
		else if (!It.On && It.Applied)
		{
			NlRecorder::Unoverride(It.HookName);
			It.Applied = false;
			It.Note.clear();
			Log(std::string("cheat ") + It.Def->Id + ": off");
		}
		It.Found = true;
		It.Restore = false;
	}

	void Apply(Item& It, bool Visible)
	{
		if (It.Def->Kind == CheatKind::Hook)
		{
			ApplyHook(It);
			return;
		}
		if (It.Def->Kind == CheatKind::Custom)
		{
			It.Restore = false;		// 모듈의 코드가 IsOn 을 보고 한다(src/Build.cpp)
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
		if (cheat.Kind != CheatKind::Number)
			item.On = State.On.count(cheat.Id) > 0;
		else if (const auto it = State.Numbers.find(cheat.Id); it != State.Numbers.end())
		{
			item.On = true;
			item.Number = it->second;
		}
		if (item.On)
			loaded += std::string(" ") + cheat.Id + (cheat.Kind == CheatKind::Number ? "=" + Shortest(item.Number) : "");
		g_Items.push_back(std::move(item));
	}
	g_Dirty = true;
	Log("cheat state:" + (loaded.empty() ? std::string(" none") : loaded));
}

void NlCheats::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	Speed().Tick(Now, Visible);		// 흐름을 재서 보여 준다. 배속은 아래에서 게임의 함수로 건다

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

void NlCheats::DrawTime()
{
	std::lock_guard lock(g_Mutex);
	NlCore::SpeedControl& speed = Speed();
	ImGui::TextWrapped("배속을 누르면 게임의 함수로 게임 속도를 겁니다. 게임 화면에서, 일시정지를 푼 채로 누르세요. "
		"게임의 속도 단추를 누르면 게임의 배속으로 돌아갑니다.");
	ImGui::Spacing();

	if (!speed.TimeFound())
		ImGui::TextDisabled("시간 컨트롤러(o_time_controller)가 아직 없습니다.");
	else
	{
		ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f", speed.Flow());		// 배속 1 에서 약 60 이었다(research/08)
		ImGui::SameLine();
		ImGui::TextDisabled("time_warp %s", Shortest(speed.Warp()).c_str());
	}

	ImGui::BeginDisabled(!speed.TimeFound());
	for (const double factor : k_Factors)
	{
		const std::string label = "x" + Shortest(factor);
		if (ImGui::Button(label.c_str(), ImVec2(64, 0)))
			g_WarpAsked = factor;
		ImGui::SameLine();
	}
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
		if (item.Def->Kind != CheatKind::Number)
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
		if (Id != item.Def->Id || item.Def->Kind == CheatKind::Number)
			continue;
		if (On && !item.On)
		{
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

int NlCheats::ActiveCount()
{
	std::lock_guard lock(g_Mutex);
	const auto on = std::count_if(g_Items.begin(), g_Items.end(), [](const Item& item) { return item.On; });
	return static_cast<int>(on) + (Speed().Wanted() > 0 || Speed().Busy() ? 1 : 0);
}

void NlCheats::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
		if (item.On)
			TurnOff(item);
	if (Speed().Wanted() > 0 || Speed().Busy())
		Speed().Release();
}
