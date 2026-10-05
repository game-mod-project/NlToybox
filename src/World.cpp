#include "World.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"

#include <imgui.h>

#include <deque>
#include <mutex>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::WorldAct;

namespace
{
	// 이벤트를 고르는 감독(research/14). 쿨다운은 이벤트의 이름 → 남은 날, 묶음의 이름 → 남은 날.
	constexpr const char* k_Director = "inst:o_game_map_controller.__game_director";
	// 주교를 다루는 관리자. is_has_bishop()·debug_force_send_bishop() 은 인자가 없다(기계어). 불러서 주교가 오는 것을 봤다.
	constexpr const char* k_Religion = "inst:o_game_map_controller.__province.__religiosity_manager";

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlWorld::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
	}

	// 구조체의 칸 가운데 0 보다 큰 수에 0 을 쓴다. 돌려주는 것: 쓴 칸의 수(구조체를 열지 못하면 -1). Seen 에는 수인 칸의 수를 더한다.
	int ClearNumbers(const std::string& Path, int& Seen, std::string& Why)
	{
		RValue box;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		if (!NlAccess::Open(NlCore::ParseAskPath(Path), box, kind, Why) || kind != Holder::Struct)
		{
			if (Why.empty())
				Why = "not a struct";
			return -1;
		}
		std::vector<NlCore::PathStep> steps;		// 도는 동안에는 쓰지 않는다
		NlAccess::ForEachChild(box, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			const bool number = NlGame::IsNumber(child);
			Seen += number;
			if (NlCore::ShouldClearCooldown(number, number ? child.ToDouble() : 0))
				steps.push_back(step);
			return true;
		});
		int cleared = 0;
		for (const NlCore::PathStep& step : steps)
		{
			std::string why;
			if (NlAccess::SetNumber(box, step, 0, why))		// 쓴 뒤 다시 읽어 확인한다
				cleared++;
			else
				Why = why;
		}
		return cleared;
	}

	bool HasBishop(bool& Has, std::string& Why)
	{
		RValue answer;
		if (!CallNoArgs(std::string(k_Religion) + ".is_has_bishop", answer, Why) || !NlGame::IsNumber(answer))
			return false;
		Has = answer.ToDouble() != 0;
		return true;
	}

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		switch (Act)
		{
		case WorldAct::CooldownsClear:
		{
			int seen = 0;
			std::string why;
			const int events = ClearNumbers(std::string(k_Director) + ".__events_cooldowns", seen, why);
			const int groups = ClearNumbers(std::string(k_Director) + ".__events_groups_cooldowns", seen, why);
			if (events < 0 || groups < 0)
				return "이벤트 쿨다운을 읽지 못했습니다: " + why;
			Log("world: event cooldowns cleared: " + std::to_string(events) + " event(s), " + std::to_string(groups) + " group(s), " + std::to_string(seen) + " numeric");
			return "이벤트 쿨다운 " + std::to_string(events) + "개와 묶음 쿨다운 " + std::to_string(groups) + "개를 0 으로 썼습니다 (수인 칸 " + std::to_string(seen) + "개)";
		}
		case WorldAct::BishopSend:
		{
			bool has = false;
			std::string why;
			const bool read = HasBishop(has, why);
			switch (NlCore::ChooseBishopStep(read, has))
			{
			case NlCore::BishopStep::Unknown:
				return "주교가 있는지 읽지 못했습니다: " + why;
			case NlCore::BishopStep::AlreadyHere:
				return "주교가 이미 있습니다";
			case NlCore::BishopStep::Call:
				break;
			}
			RValue result;
			Log("world call debug_force_send_bishop()");		// 부르기 전에 남긴다
			if (!CallNoArgs(std::string(k_Religion) + ".debug_force_send_bishop", result, why))
				return "주교를 부르지 못했습니다: " + why;
			bool came = false;
			HasBishop(came, why);
			Log(std::string("world: bishop ") + (came ? "is here" : "did not come"));
			return came ? "주교가 왔습니다" : "불렀지만 주교가 오지 않았습니다";
		}
		}
		return std::string();
	}

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	void Hint(const char* Text)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", Text);
		ImGui::PopTextWrapPos();
	}

	void DrawLast()
	{
		if (!g_Last.empty())
			Hint(g_Last.c_str());
	}

	void Push(WorldAct Act)
	{
		if (g_Queue.size() < 4)
			g_Queue.push_back(Act);
	}
}

void NlWorld::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlWorld::GameTick()
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy || g_Queue.empty())
		return;
	g_Busy = true;
	const WorldAct act = g_Queue.front();
	g_Queue.pop_front();
	g_Last = DoNow(act);
	g_Busy = false;
}

void NlWorld::DrawEvents()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("이벤트 쿨다운 지우기"))
		Push(WorldAct::CooldownsClear);
	Hint("게임이 이벤트를 고를 때 보는 '남은 날'(이벤트마다, 묶음마다)을 0 으로 씁니다. 써지는 것까지 봤고, 이벤트가 더 일찍 오는지는 확인 전입니다. "
		"쓴 값은 저장하면 세이브에 남을 것으로 보입니다.");
	DrawLast();
}

void NlWorld::DrawReligion()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("주교 부르기"))
		Push(WorldAct::BishopSend);
	Hint("게임의 디버그 함수로 주교를 바로 오게 합니다(교단의 영주 하나가 영지에 나타납니다). 주교가 이미 있으면 부르지 않습니다. 되돌릴 수 없습니다.");
	DrawLast();
}

std::string NlWorld::Do(NlCore::WorldAct Act)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	g_Busy = true;
	g_Last = DoNow(Act);
	g_Busy = false;
	return g_Last;
}
