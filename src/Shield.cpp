#include "Shield.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "PeopleAccess.hpp"
#include "Recorder.hpp"
#include "core/AskPath.hpp"
#include "core/BattlePlan.hpp"
#include "core/PeoplePlan.hpp"
#include "core/Text.hpp"

#include <string>
#include <vector>

using namespace YYTK;
using namespace NlPeopleAccess;
using NlCore::PersonRow;
using NlCore::Shortest;

namespace
{
	// 상처를 입히는 함수(research/13): SoulBasic.take_damage(상처의 이름, 구조체, 불리언) -> true. 생성자의 정적 메서드라 영주 하나의 영혼에서 스크립트를 찾는다.
	constexpr const char* k_TakeDamage = "inst:o_character.__soul.take_damage";

	NlShield::LogFn g_Log;
	double g_NextShield = 0;			// 전투의 항목들: 영혼의 주소를 다시 모을 시각
	bool g_BattleOn = false;			// 전투의 항목 가운데 하나라도 걸어 두었다(끌 때 주소 묶음을 비운다)

	struct SideHook			// 영혼의 한 함수에 거는 아군·적 배율
	{
		const char* Path;				// 영혼의 메서드(생성자의 정적 메서드라 영주 하나의 영혼에서 스크립트를 찾는다)
		const char* Ally;				// 아군 배율 항목의 Id
		const char* Enemy;				// 적 배율 항목의 Id
		double Cap;						// 올린 값의 위쪽 한도(0 이면 없음)
		NlCore::SideScale Applied;		// 걸어 둔 것
		std::string Name;				// 건 스크립트의 이름
	};
	SideHook g_SideHooks[] = {
		// 싸울 때의 전투 기술: () -> 10, 7(research/13. 싸우는 동안 150번). 기술은 0~20 이라 올린 값을 20 에서 멈춘다(표를 번호로 읽는 곳이 있을 수 있다. 추정).
		{ "inst:o_character.__soul.get_combat_level_in_battle", "ally_power", "enemy_power", 20, {}, {} },
		// 치명적인 통증의 한도: () -> 40(research/13. 게임이 계속 부른다).
		{ "inst:o_character.__soul.get_mortal_pain_threshold", "ally_toughness", "enemy_toughness", 0, {}, {} },
	};
	bool g_ShieldOn = false;			// 아군 무적의 바꾸기를 이 모듈이 걸었다
	std::string g_ShieldName;			// 건 스크립트의 이름

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 아군 무적을 끈다(걸어 둔 것이 있으면).
	void ShieldOff()
	{
		if (!g_ShieldOn)
			return;
		g_ShieldOn = false;
		NlRecorder::Unoverride(g_ShieldName);
		NlCheats::SetNote("ally_invincible", std::string());
		Log("people: ally_invincible off");
	}

	// 한 함수의 아군·적 배율을 끈다(걸어 둔 것이 있으면).
	void SideOff(SideHook& Hook)
	{
		if (!Hook.Applied.On)
			return;
		Hook.Applied = NlCore::SideScale();
		NlRecorder::Unoverride(Hook.Name);
		NlCheats::SetNote(Hook.Ally, std::string());
		NlCheats::SetNote(Hook.Enemy, std::string());
		Log(std::string("people: ") + Hook.Ally + "/" + Hook.Enemy + " off");
	}

	// 전투의 항목들(아군 무적, 아군·적의 전투력과 맷집): 훅이 self 가 플레이어의 영혼인지로 가린다. 영혼의 주소는 0.5초마다 다시 모은다.
	// 새로 온 플레이어의 사람(이주민, 태어난 아이)은 다음에 모을 때까지 "그 밖"으로 읽힌다: 0.5초까지 무적이 아니고 적의 배율을 받는다.
	// 모듈이 만든 사람(병사 추가, 소환)은 만든 바로 다음 틱에 다시 모은다. 사라진 영혼의 주소는 다음에 모을 때 빠진다.
	void BattleTick(double Now)
	{
		if (Now < g_NextShield)
			return;
		g_NextShield = Now + 0.5;
		constexpr const char* id = "ally_invincible";
		const bool shield = NlCheats::IsOn(id);
		NlCore::SideScale want[std::size(g_SideHooks)];
		bool any = shield;
		for (size_t i = 0; i < std::size(g_SideHooks); i++)
		{
			double ally = 0, enemy = 0;
			const bool ally_on = NlCheats::Factor(g_SideHooks[i].Ally, ally);
			const bool enemy_on = NlCheats::Factor(g_SideHooks[i].Enemy, enemy);
			want[i] = NlCore::PlanSides(ally_on, ally, enemy_on, enemy);
			any = any || want[i].On;
		}
		if (!any)
		{
			if (g_BattleOn)
			{
				g_BattleOn = false;
				ShieldOff();
				for (SideHook& hook : g_SideHooks)
					SideOff(hook);
				NlRecorder::SetPlayerSelves({});
			}
			return;
		}
		if (!NlAccess::InGame())
		{
			// 지난 게임의 주소가 남지 않게 묶음을 비운다. 배율의 바꾸기도 끈다: 다음 게임에서 "주소부터 넣고 건다"의 차례를 되살린다
			// (묶음이 빈 채 배율이 걸려 있으면 모두가 적의 배율을 받는다. 훅 쪽도 빈 묶음에는 곱하지 않는다: core 의 HookFactor).
			for (SideHook& hook : g_SideHooks)
			{
				SideOff(hook);
				if (NlCheats::IsOn(hook.Ally))
					NlCheats::SetNote(hook.Ally, "게임을 시작하면 적용");
				if (NlCheats::IsOn(hook.Enemy))
					NlCheats::SetNote(hook.Enemy, "게임을 시작하면 적용");
			}
			NlRecorder::SetPlayerSelves({});
			if (shield)
				NlCheats::SetNote(id, "게임을 시작하면 적용");
			return;
		}
		g_BattleOn = true;
		std::vector<std::uintptr_t> selves;
		for (const bool character : { true, false })
		{
			const int count = NlAccess::InstanceCount(character ? "o_character" : "o_dummy");
			for (int n = 0; n < count; n++)
			{
				PersonRow row;
				row.Character = character;
				row.Index = n;
				RValue soul;
				std::string faction;
				if (ReadSoul(row, soul) && soul.IsStruct()
					&& FollowString(soul, { { '.', "__faction", 0 }, { '.', "__system_name", 0 } }, faction) && faction == "player")
					selves.push_back(reinterpret_cast<std::uintptr_t>(soul.m_Object));		// 구조체의 주소. 훅의 self 와 견준다
			}
		}
		const size_t count = selves.size();
		NlRecorder::SetPlayerSelves(std::move(selves));		// 주소부터 넣고 건다

		// 아군·적의 배율: 한 함수에 둘을 함께 건다(바뀌었으면 같은 훅에 다시 건다).
		for (size_t i = 0; i < std::size(g_SideHooks); i++)
		{
			SideHook& hook = g_SideHooks[i];
			if (!want[i].On)
			{
				SideOff(hook);
				continue;
			}
			if (!NlCore::SameSides(hook.Applied, want[i]) || !NlRecorder::Overriding(hook.Name))
			{
				NlRecorder::Forced value;
				value.Kind = 'x';
				value.Number = want[i].Mine;
				value.Other = want[i].Other;
				value.Who = 'p';
				value.Whole = true;		// 정수로 돌아온 값은 정수로 남기고 양수는 1 아래로 내리지 않는다(본 값은 10, 7, 5, 3, 40 모두 정수다)
				value.Cap = hook.Cap;
				std::string name, why;
				if (!NlRecorder::Override(hook.Path, value, name, why))
				{
					NlCheats::SetNote(hook.Ally, NlCheats::IsOn(hook.Ally) ? "걸지 못했습니다: " + why : std::string());
					NlCheats::SetNote(hook.Enemy, NlCheats::IsOn(hook.Enemy) ? "걸지 못했습니다: " + why : std::string());
					continue;
				}
				hook.Applied = want[i];
				hook.Name = name;
				Log(std::string("people: ") + hook.Ally + " x" + Shortest(want[i].Mine) + ", " + hook.Enemy + " x" + Shortest(want[i].Other) + ", " + std::to_string(count) + " souls");
			}
			// 곱한 호출의 수를 보인다: 아군의 것은 self 가 플레이어의 영혼이었던 호출, 적의 것은 그 밖의 호출.
			uint64_t mine = 0, others = 0;
			NlRecorder::Counts(hook.Name, mine, others);
			NlCheats::SetNote(hook.Ally, want[i].Mine != 1 ? "아군의 호출 " + std::to_string(mine) + "번에 곱함" : std::string());
			NlCheats::SetNote(hook.Enemy, want[i].Other != 1 ? "그 밖의 호출 " + std::to_string(others) + "번에 곱함" : std::string());
		}

		if (!shield)
		{
			ShieldOff();
			return;
		}
		if (!g_ShieldOn || !NlRecorder::Overriding(g_ShieldName))
		{
			NlRecorder::Forced value;
			value.Kind = 'b';
			value.Number = 0;
			value.Skip = true;		// 들어올 때 Result 가 undefined 인 것을 표본에서 봤다(research/13)
			value.Who = 'p';
			std::string name, why;
			if (!NlRecorder::Override(k_TakeDamage, value, name, why))
			{
				NlCheats::SetNote(id, "걸지 못했습니다: " + why);
				return;
			}
			g_ShieldOn = true;
			g_ShieldName = name;
			Log("people: ally_invincible on, " + std::to_string(count) + " souls");
		}
		// 넣은 주소의 수와, 훅이 실제로 막은 호출·지나가게 둔 호출의 수를 함께 보인다(주소를 넣었다는 것이 막았다는 뜻은 아니다).
		uint64_t applied = 0, passed = 0;
		NlRecorder::Counts(g_ShieldName, applied, passed);
		NlCheats::SetNote(id, "플레이어의 사람 " + std::to_string(count) + "명의 주소를 넣음. 막은 상처 " + std::to_string(applied)
			+ ", 그대로 둔 상처 " + std::to_string(passed));
	}

}

void NlShield::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

void NlShield::Tick(double Now)
{
	BattleTick(Now);
}

void NlShield::RefreshSoon()
{
	g_NextShield = 0;
}
