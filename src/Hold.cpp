#include "Hold.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "PeopleAccess.hpp"
#include "core/AskPath.hpp"
#include "core/Text.hpp"

#include <string>
#include <vector>

using namespace YYTK;
using namespace NlPeopleAccess;
using NlCore::PathStep;
using NlCore::PersonRow;

namespace
{
	NlHold::LogFn g_Log;
	NlHold::RowsFn g_Rows;
	NlHold::HappyFn g_Happy;

	// 표의 항목(플레이어의 사람을 조금씩 돌며 쓴다). 바퀴의 판단은 core/PeoplePlan 의 HoldRound 가 한다.
	std::vector<PersonRow> g_HoldPeople;
	NlCore::HoldRound g_Hold;
	double g_NextHold = 0, g_NextHoldScan = 0, g_NextHappy = 0, g_NextHoldLog = 0;
	bool g_HappyRound = false;			// 이번 바퀴에서 행복 생각을 본다
	bool g_HoldNoted = false;			// 항목 옆에 글을 적어 두었다(할 일이 없어지면 한 번 비운다)
	size_t g_RoundPeople = 0;
	size_t g_LogNeeds = 0, g_LogHappy = 0, g_LogAge = 0;		// 마지막 로그 줄 뒤로 쓴 수

	constexpr const char* k_HoldIds[] = { "no_hunger", "no_tiredness", "needs_full", "piety_full", "always_happy", "no_old_age_death" };

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	void HoldNotes(const std::string& Note)
	{
		g_HoldNoted = true;
		for (const char* id : k_HoldIds)
			NlCheats::SetNote(id, NlCheats::IsOn(id) ? Note : std::string());
	}

	void ClearHoldNotes()
	{
		if (!g_HoldNoted)
			return;
		g_HoldNoted = false;
		for (const char* id : k_HoldIds)
			NlCheats::SetNote(id, std::string());
	}

	void HoldTick(double Now)
	{
		// 이 함수는 오브젝트 이벤트마다 불린다. 시각부터 본다: 아래의 것들(항목 읽기, 게임 화면인지)은 0.25초에 한 번만 한다.
		if (Now < g_NextHold)
			return;
		g_NextHold = Now + 0.25;

		const bool hunger = NlCheats::IsOn("no_hunger"), tired = NlCheats::IsOn("no_tiredness"), all = NlCheats::IsOn("needs_full");
		const bool happy = NlCheats::IsOn("always_happy"), ageless = NlCheats::IsOn("no_old_age_death");
		const std::vector<int> needs = NlCore::NeedsToHold(hunger, tired, all, NlCheats::IsOn("piety_full"));
		if (needs.empty() && !happy && !ageless && !g_Hold.AgeWritten)
		{
			g_Hold.Cursor = 0;
			ClearHoldNotes();		// 끈 항목 옆에 "적용 중"이 남지 않게
			return;
		}
		if (!NlAccess::InGame())
		{
			HoldNotes("게임을 시작하면 적용");
			g_Hold = NlCore::HoldRound();		// 새로 불러온 게임의 깃발은 처음 값이다(세이브에 남지 않는다)
			g_HoldPeople.clear();
			return;
		}

		const NlCore::HoldPlan plan = NlCore::HoldBegin(g_Hold, !needs.empty(), happy, ageless);
		if (!plan.Work)
			return;

		if (g_Hold.Cursor == 0)
		{
			// 바퀴의 처음: 사람들을 다시 읽는다(5초에 한 번까지. 되돌리는 바퀴를 시작할 때는 바로).
			if (plan.Rescan || Now >= g_NextHoldScan || g_HoldPeople.empty())
			{
				g_NextHoldScan = Now + 5;
				std::vector<PersonRow> people;
				if (!g_Rows || !g_Rows(people))
					return;
				g_HoldPeople.clear();
				for (const size_t at : NlCore::PickTargets(people, "people"))
					g_HoldPeople.push_back(people[at]);
			}
			// 행복 생각은 60초에 한 바퀴만 본다(사람마다 게임의 함수를 부르고, 붙일 때마다 로그를 남긴다. 생각은 하루 동안 간다).
			g_HappyRound = happy && Now >= g_NextHappy;
			if (g_HappyRound)
				g_NextHappy = Now + 60;
			g_RoundPeople = 0;
		}

		const NlCore::PeopleSlice slice = NlCore::NextPeopleSlice(g_HoldPeople.size(), g_Hold.Cursor, 40);
		for (size_t i = slice.Begin; i < slice.End && i < g_HoldPeople.size(); i++)
		{
			const PersonRow row = g_HoldPeople[i];		// 사본(아래의 Scan 이나 게임의 함수가 목록을 바꿔도 흔들리지 않게)
			RValue soul;
			if (!StillThere(row, soul))
			{
				g_NextHoldScan = 0;		// 사람이 드나들었다. 다음 바퀴에서 다시 읽는다
				NlCore::HoldTouched(g_Hold, false, true);
				continue;
			}
			g_RoundPeople++;
			std::string why;

			if (!needs.empty())
			{
				RValue values, limits;
				if (NlAccess::Follow(soul, { { '.', "__motive", 0 }, { '.', "__motive", 0 } }, values, why) && values.IsArray())
				{
					const bool have_limits = NlAccess::Follow(soul, { { '.', "__motive", 0 }, { '.', "__motive_limit", 0 } }, limits, why) && limits.IsArray();
					for (const int need : needs)
					{
						const PathStep step{ '[', "", static_cast<double>(need) };
						double current = 0, limit = NlCore::k_NeedMax, wanted = 0;
						if (!FollowNumber(values, { step }, current))
							continue;
						if (have_limits)
							FollowNumber(limits, { step }, limit);
						if (NlCore::ShouldFillNeed(current, limit) && NlCore::NeedValue(NlCore::k_FillAll, limit, wanted) && NlAccess::SetNumber(values, step, wanted, why))
							g_LogNeeds++;
					}
				}
			}

			bool age_off = false;
			if (plan.WriteAge)
			{
				// __soul.__aging.__old.__debug_is_can_die_of_old_age: 인물마다 true 다. 켜면 false 로, 끄면 다시 true 로 쓴다.
				RValue old;
				const PathStep flag{ '.', "__debug_is_can_die_of_old_age", 0 };
				double current = 0;
				if (NlAccess::Follow(soul, { { '.', "__aging", 0 }, { '.', "__old", 0 } }, old, why) && old.IsStruct() && FollowNumber(old, { flag }, current))
				{
					if (current != plan.AgeValue && NlAccess::SetNumber(old, flag, plan.AgeValue, why))
					{
						g_LogAge++;
						current = plan.AgeValue;
					}
					age_off = current == 0;		// 꺼진 깃발이 있다(되돌릴 것이 있다)
				}
			}
			NlCore::HoldTouched(g_Hold, age_off, false);

			if (g_HappyRound && happy)		// 도중에 끄면 남은 사람에게는 붙이지 않는다
			{
				std::string note;
				if (g_Happy && g_Happy(row, note) && note.empty())
					g_LogHappy++;
			}
		}
		NlCore::HoldEnd(g_Hold, slice, ageless);

		if (slice.Wrapped)
		{
			HoldNotes(std::to_string(g_RoundPeople) + "명에게 적용 중");
			// 로그는 쓴 것이 있을 때, 60초에 한 줄까지만 남긴다(욕구는 틱마다 조금씩 줄어 바퀴마다 쓸 것이 생긴다).
			if ((g_LogNeeds || g_LogHappy || g_LogAge) && Now >= g_NextHoldLog)
			{
				g_NextHoldLog = Now + 60;
				Log("people hold: " + std::to_string(g_RoundPeople) + " people; since the last line " + std::to_string(g_LogNeeds) + " need(s) filled, "
					+ std::to_string(g_LogHappy) + " happy mind(s), " + std::to_string(g_LogAge) + " old-age flag(s) written");
				g_LogNeeds = g_LogHappy = g_LogAge = 0;
			}
		}
	}

}

void NlHold::Init(LogFn Log_, RowsFn Rows, HappyFn Happy)
{
	g_Log = std::move(Log_);
	g_Rows = std::move(Rows);
	g_Happy = std::move(Happy);
}

void NlHold::Tick(double Now)
{
	HoldTick(Now);
}
