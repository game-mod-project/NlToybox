#include "Cheats.hpp"

#include "Access.hpp"
#include "core/Rate.hpp"
#include "core/SpeedTrial.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

using NlCore::Area;
using NlCore::Cheat;
using NlCore::CheatKind;
using NlCore::Fixed;
using NlCore::Shortest;
using NlCore::SpeedStep;
using NlCore::SpeedTrial;

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
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		double Current = 0;
		std::string Note, Logged;
	};

	// 게임 속도의 후보(스펙 §9). 시험하는 차례대로. 이름은 덤프의 o_time_controller 에서 봤다. 어느 것이 속도를 정하는지는 모른다.
	struct Handle
	{
		const char* Label;
		const char* Path;			// nullptr 이면 time_speed_variants 의 지금 자리
	};
	constexpr Handle k_Handles[] = {
		{ "time_warp_new", "inst:o_time_controller.time_warp_new" },
		{ "time_warp", "inst:o_time_controller.time_warp" },
		{ "__debug_custom_wrap", "inst:o_time_controller.__debug_custom_wrap" },
		{ "time_speed_variants[time_speed_index]", nullptr },
	};
	constexpr int k_HandleCount = 4;
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";
	constexpr const char* k_Warp = "inst:o_time_controller.time_warp";
	constexpr const char* k_SpeedIndex = "inst:o_time_controller.time_speed_index";
	constexpr double k_Factors[] = { 0.25, 0.5, 1, 2, 5, 10, 50 };		// time_warp_max 는 100 이다(덤프)

	std::recursive_mutex g_Mutex;	// 아래 전부를 지킨다
	NlCheats::LogFn g_Log;
	std::vector<Item> g_Items;
	bool g_Changed = false;			// 상태 파일에 적을 것이 바뀌었다
	bool g_Dirty = true;			// 다음 틱에 바로 적용한다
	double g_NextApply = 0;

	// 게임 속도
	NlCore::Rate g_Rate(1.0);
	SpeedTrial g_Trial(k_HandleCount);
	double g_Wanted = 0;			// 고른 배율. 0 이면 손대지 않는다
	bool g_Pressed = false, g_ReleaseAsked = false;		// 창이 올리고 틱이 내린다
	bool g_Holding = false;			// 계속 써 넣는 중인가
	std::string g_HoldPath;
	double g_HoldValue = 0;
	double g_NextSpeed = 0, g_NextHold = 0;
	size_t g_AttemptsLogged = 0;
	bool g_TimeFound = false;		// 스냅샷
	double g_Flow = 0, g_Warp = 0;
	std::string g_SpeedNote;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// ---- 게임 스레드 ----

	void Apply(Item& It, bool Visible)
	{
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

	std::string HandlePath(int Index)
	{
		if (Index < 0 || Index >= k_HandleCount)
			return "";
		if (k_Handles[Index].Path)
			return k_Handles[Index].Path;

		double at = 0;
		if (!NlAccess::ReadNumber(k_SpeedIndex, at) || at < 0)
			return "";
		return "inst:o_time_controller.time_speed_variants[" + Shortest(std::floor(at)) + "]";
	}

	// 고른 손잡이로 바라는 배율을 건다.
	void ApplyWanted()
	{
		if (g_Wanted <= 0 || g_Trial.State() != SpeedTrial::Phase::Done)
			return;

		const int chosen = g_Trial.Candidate();
		const std::string path = HandlePath(chosen);
		std::string why;
		const bool ok = !path.empty() && NlAccess::WriteNumber(path, g_Wanted, why);
		g_Holding = ok && !g_Trial.Sticky();
		g_HoldPath = path;
		g_HoldValue = g_Wanted;
		g_SpeedNote = ok ? "x" + Shortest(g_Wanted) + " 을 걸었습니다." : "써지지 않음: " + why;
		Log("speed x" + Shortest(g_Wanted) + " via " + k_Handles[chosen].Label + (ok ? "" : ": " + why));
	}

	void Release()
	{
		g_ReleaseAsked = false;
		if (g_Trial.Running())
			return;
		g_Wanted = 0;
		g_Holding = false;
		if (g_Trial.State() != SpeedTrial::Phase::Done)
		{
			g_SpeedNote.clear();
			return;
		}

		const int chosen = g_Trial.Candidate();
		const std::string path = HandlePath(chosen);
		std::string why;
		const bool ok = !path.empty() && NlAccess::WriteNumber(path, g_Trial.SavedOld(), why);
		g_SpeedNote = ok ? "게임의 속도로 되돌렸습니다." : "되돌리지 못했습니다: " + why;
		Log(std::string("speed release via ") + k_Handles[chosen].Label + " = " + Shortest(g_Trial.SavedOld()) + (ok ? "" : ": " + why));
	}

	// 배율 단추를 눌렀다.
	void Begin()
	{
		if (g_Trial.Running())
			return;			// 시험이 끝나면 g_Wanted 를 건다
		if (g_Trial.State() == SpeedTrial::Phase::Done)
		{
			ApplyWanted();
			return;
		}
		if (!g_Rate.Ready() || !g_Trial.Start(g_Flow, g_Warp))
		{
			g_SpeedNote = "시간이 흐르지 않아 손잡이를 시험할 수 없습니다. 게임 화면에서 일시정지를 풀고 다시 누르세요.";
			g_Wanted = 0;
			return;
		}
		g_AttemptsLogged = 0;
		g_SpeedNote = "손잡이를 시험하는 중입니다(길어도 10초). 그동안 게임 속도가 잠깐 바뀝니다.";
		Log("speed trial start: flow " + Fixed(g_Flow, 2) + "/s, time_warp " + Shortest(g_Warp) + ", probe " + Shortest(g_Trial.ProbeWarp()));
	}

	// 시험을 한 걸음 나아간다(0.1초마다).
	void StepTrial(double Now)
	{
		const std::string path = HandlePath(g_Trial.Candidate());
		double current = 0;
		const bool readable = !path.empty() && NlAccess::ReadNumber(path, current);
		const SpeedStep step = g_Trial.Tick(Now, readable, current, g_Flow);

		g_Holding = step.What == SpeedStep::Kind::Write;
		if (step.What == SpeedStep::Kind::Write)
		{
			g_HoldPath = HandlePath(step.Candidate);
			g_HoldValue = step.Value;
			g_NextHold = 0;
		}
		else if (step.What == SpeedStep::Kind::Restore)
		{
			std::string why;
			NlAccess::WriteNumber(HandlePath(step.Candidate), step.Value, why);
		}

		for (; g_AttemptsLogged < g_Trial.Attempts().size(); g_AttemptsLogged++)
		{
			const NlCore::SpeedAttempt& attempt = g_Trial.Attempts()[g_AttemptsLogged];
			Log(std::string("speed trial ") + k_Handles[attempt.Candidate].Label + ": "
				+ (attempt.Readable ? "old " + Shortest(attempt.Old) + ", flow " + Fixed(attempt.Rate, 2) + "/s"
					+ (attempt.Matched ? " -> matched" : " -> no effect, restored") : "not readable"));
		}

		switch (g_Trial.State())
		{
		case SpeedTrial::Phase::Done:
			Log(std::string("speed trial done: ") + k_Handles[g_Trial.Candidate()].Label + (g_Trial.Sticky() ? " (write once)" : " (keep writing)"));
			ApplyWanted();
			break;
		case SpeedTrial::Phase::Failed:
			g_SpeedNote = "네 후보 모두 속도를 바꾸지 못했습니다. 시도마다의 값은 NlToyBox.log 에 있습니다.";
			g_Wanted = 0;
			Log("speed trial failed");
			break;
		case SpeedTrial::Phase::Aborted:
			g_SpeedNote = "시험 중에 시간이 멈췄습니다. 일시정지를 풀고 다시 누르세요.";
			g_Wanted = 0;
			Log("speed trial aborted: time stopped");
			break;
		default:
			break;
		}
	}

	void TickSpeed(double Now, bool Visible)
	{
		// 계속 쓰기는 프레임마다 한 번쯤 한다. 게임이 프레임마다 되돌리는 값이면 이래야 먹는다.
		if (g_Holding && Now >= g_NextHold)
		{
			g_NextHold = Now + 0.015;
			std::string why;
			NlAccess::WriteNumber(g_HoldPath, g_HoldValue, why);
		}

		const bool busy = g_Trial.Running() || g_Wanted > 0 || g_Pressed || g_ReleaseAsked;
		if (Now < g_NextSpeed || (!Visible && !busy))
			return;
		g_NextSpeed = Now + 0.1;

		double game_time = 0;
		g_TimeFound = NlAccess::ReadNumber(k_GameTime, game_time);
		if (!g_TimeFound)
		{
			// 시간 컨트롤러가 없다(로딩 중). 하던 것을 놓는다. 써 둔 값은 컨트롤러와 함께 사라졌다.
			g_Rate.Reset();
			g_Flow = 0;
			g_Holding = false;
			g_Pressed = g_ReleaseAsked = false;
			if (g_Trial.Running())
				g_Trial = SpeedTrial(k_HandleCount);
			return;
		}
		g_Rate.Add(Now, game_time);
		g_Flow = g_Rate.PerSecond();
		NlAccess::ReadNumber(k_Warp, g_Warp);

		if (g_ReleaseAsked)
			Release();
		if (g_Pressed)
		{
			g_Pressed = false;
			Begin();
		}
		if (g_Trial.Running())
			StepTrial(Now);
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
		if (cheat.Kind == CheatKind::Toggle)
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
	TickSpeed(Now, Visible);

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
		if (item.Def->Kind == CheatKind::Toggle)
			DrawToggle(item);
		else
			DrawNumber(item);
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
	ImGui::TextWrapped("배율을 누르면 게임 속도를 바꿉니다. 처음 누를 때는 어느 값이 속도를 정하는지 시험으로 찾습니다"
		"(게임 화면에서, 일시정지를 푼 채로 누르세요).");
	ImGui::Spacing();

	if (!g_TimeFound)
		ImGui::TextDisabled("시간 컨트롤러(o_time_controller)가 아직 없습니다.");
	else
	{
		const double unit = g_Trial.UnitRate();
		if (unit > 0)
			ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f  (기준의 %.2f배)", g_Flow, g_Flow / unit);
		else
			ImGui::Text("게임 시간의 흐름: 실제 1초에 %.1f", g_Flow);
		ImGui::SameLine();
		ImGui::TextDisabled("time_warp %s", Shortest(g_Warp).c_str());
	}

	ImGui::BeginDisabled(g_Trial.Running() || !g_TimeFound);
	for (const double factor : k_Factors)
	{
		const std::string label = "x" + Shortest(factor);
		if (ImGui::Button(label.c_str(), ImVec2(64, 0)))
		{
			g_Wanted = factor;
			g_Pressed = true;
		}
		ImGui::SameLine();
	}
	if (ImGui::Button("게임에 맡김"))
		g_ReleaseAsked = true;
	ImGui::EndDisabled();

	if (!g_SpeedNote.empty())
		ImGui::TextWrapped("%s", g_SpeedNote.c_str());
	if (g_Trial.State() == SpeedTrial::Phase::Done)
		ImGui::TextDisabled("손잡이: %s (%s)", k_Handles[g_Trial.Candidate()].Label, g_Trial.Sticky() ? "한 번 쓰기" : "계속 쓰기");
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
		if (item.Def->Kind == CheatKind::Toggle)
			On.insert(item.Def->Id);
		else
			Numbers[item.Def->Id] = item.Number;
	}
	return true;
}

int NlCheats::ActiveCount()
{
	std::lock_guard lock(g_Mutex);
	const auto on = std::count_if(g_Items.begin(), g_Items.end(), [](const Item& item) { return item.On; });
	return static_cast<int>(on) + (g_Wanted > 0 ? 1 : 0);
}

void NlCheats::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Item& item : g_Items)
		if (item.On)
			TurnOff(item);
	if (g_Wanted > 0)
		g_ReleaseAsked = true;
}
