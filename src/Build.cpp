#include "Build.hpp"

#include "Access.hpp"
#include "Buildings.hpp"
#include "Cheats.hpp"
#include "Jobs.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/BuildingPlan.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <map>
#include <set>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;

namespace
{
	// research/09 에서 잰 것이다(0.5588.9777.0).
	// 건물 종류.__construction_cost.levels[등급] = undefined 또는 { money, resources.__array_of_resource_quantity[39] }. 건물 종류 걷기는 src/Buildings.

	NlBuild::LogFn g_Log;
	double g_Next = 0;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 등급 하나의 비용에서 칸들의 배열(자원)을 얻는다.
	bool Quantities(const RValue& Cost, RValue& Out)
	{
		std::string why;
		return NlAccess::Follow(Cost, { { '.', "resources", 0 }, { '.', "__array_of_resource_quantity", 0 } }, Out, why) && Out.IsArray();
	}

	// 건설비(치트 표의 build_free): 건물 종류마다 등급별 비용의 money 와 자원 39칸을 엔진에 넘긴다. 0 으로 쓰기·장부·되돌리기·"게임이 되돌리면 다시 쓰기"는
	// src/Jobs 의 엔진이 한다(2026-10-07 리팩토링 C 에서 ZeroAll·RestoreAll·StillZero 를 그것으로 바꿨다). Key 는 건물 종류의 이름, Level 은 등급, Slot 은 금화 -1·자원 번호.
	bool WalkCosts(const NlJobs::Visit& V, std::string& Why)
	{
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue levels;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__construction_cost", 0 }, { '.', "levels", 0 } }, levels, why) || !levels.IsArray())
				return true;
			NlAccess::ForEachChild(levels, Holder::Array, [&](const PathStep& level, const RValue& cost) {
				if (!cost.IsStruct())
					return true;
				const PathStep money_step{ '.', "money", 0 };
				RValue money, quantities;
				std::string ignored;
				if (NlAccess::Follow(cost, { money_step }, money, ignored))
					V(cost, money_step, building, static_cast<int>(level.Index), -1, money);
				if (!Quantities(cost, quantities))
					return true;
				NlAccess::ForEachChild(quantities, Holder::Array, [&](const PathStep& step, const RValue& value) {
					V(quantities, step, building, static_cast<int>(level.Index), static_cast<int>(step.Index), value);
					return true;
				});
				return true;
			});
			return true;
		}, Why);
	}

	// ---- 거주 칸과 효과의 범위 (research/32) ----

	// 거주 칸(치트 표의 housing_capacity·barrack_capacity): 건물 종류의 __number_of_living_places 를 엔진에 넘긴다. Key 는 건물 종류의 이름.
	// 병영(core/BuildingPlan 의 IsBarracksName)과 그 밖을 따로 걷는다. 0 인 칸은 엔진의 장부가 받지 않는다(거주 건물이 아니다).
	bool WalkLiving(const NlJobs::Visit& V, bool Barracks, std::string& Why)
	{
		const PathStep step{ '.', "__number_of_living_places", 0 };
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue places;
			std::string ignored;
			if (NlCore::IsBarracksName(building) == Barracks && NlAccess::Follow(generic, { step }, places, ignored))
				V(generic, step, building, 0, -1, places);
			return true;
		}, Why);
	}

	bool WalkHousing(const NlJobs::Visit& V, std::string& Why) { return WalkLiving(V, false, Why); }
	bool WalkBarracks(const NlJobs::Visit& V, std::string& Why) { return WalkLiving(V, true, Why); }

	// 건물의 효과(research/32). 건물 종류의 __effect = { __name, __effect(주변 건물의 안락도에 더하는 수), __range(칸), __type }.
	// 좋은 효과(값이 양수: 교회·제단, 공공장소)는 범위에 배율을 쓰고(effect_range), 나쁜 효과(음수: 대장간·용광로·벌목장, 훈련장)는 값을 0 으로 쓴다(bad_effects_off).
	bool g_EffectsLogged = false;		// 효과의 목록은 한 실행에 한 번만 적는다(처음 걷기 전에: 쓰기 전의 값으로)
	NlCore::EffectSides g_Sides;		// 건물 종류마다 처음 본 부호. 값을 0 으로 써 둔 뒤에도 나쁜 효과의 자리를 다시 찾는다

	// 구조체의 멤버 하나를 로그에 적을 글로(글은 그대로, 수는 가장 짧게, 그 밖과 없는 것은 "?").
	std::string Brief(const RValue& In, const char* Member)
	{
		RValue value;
		std::string ignored;
		if (!NlAccess::Follow(In, { { '.', Member, 0 } }, value, ignored))
			return "?";
		return value.IsString() ? value.ToString() : NlGame::IsNumber(value) ? NlCore::Shortest(value.ToDouble()) : "?";
	}

	// 건물 종류의 효과 구조체와, 그 값의 부호(처음 본 것). 효과가 없거나 값이 수가 아니면 거짓.
	bool EffectOf(const std::string& Building, const RValue& Generic, RValue& Effect, NlCore::EffectSide& Side)
	{
		RValue value;
		std::string ignored;
		if (!NlAccess::Follow(Generic, { { '.', "__effect", 0 } }, Effect, ignored) || !Effect.IsStruct()
			|| !NlAccess::Follow(Effect, { { '.', "__effect", 0 } }, value, ignored) || !NlGame::IsNumber(value))
			return false;
		Side = g_Sides.Of(Building, value.ToDouble());
		return true;
	}

	// 건물 종류마다의 효과를 로그에 적는다(쓰기 전의 값으로): 효과(이름, 범위, 크기, 종류)마다 그것을 가진 건물 종류들과 구조체의 수.
	// 어느 건물이 어떤 효과를 갖는지를 이 빌드에서 보려고 둔다(게임이 갱신되면 research/32 의 표와 견준다). 부호도 여기서 처음 기억한다.
	void LogEffectsOnce()
	{
		if (g_EffectsLogged)
			return;
		g_EffectsLogged = true;
		std::map<std::string, std::pair<std::string, std::set<const void*>>> seen;
		std::string why;
		NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue effect;
			NlCore::EffectSide side = NlCore::EffectSide::None;
			if (!EffectOf(building, generic, effect, side))
				return true;
			auto& entry = seen[Brief(effect, "__name") + " range " + Brief(effect, "__range") + " effect " + Brief(effect, "__effect") + " type " + Brief(effect, "__type")];
			entry.first += " " + building;
			entry.second.insert(effect.m_Pointer);
			return true;
		}, why);
		Log("build: " + std::to_string(seen.size()) + " building effect(s) before any write" + (why.empty() ? "" : " (" + why + ")"));
		for (const auto& [what, entry] : seen)
			Log("build: effect " + what + " in " + std::to_string(entry.second.size()) + " struct(s):" + entry.first);
	}

	// 한쪽(좋은 것이나 나쁜 것)의 효과를 가진 건물 종류마다 그 구조체의 Member 칸을 엔진에 넘긴다. Key 는 건물 종류의 이름.
	bool WalkEffects(const NlJobs::Visit& V, NlCore::EffectSide Wanted, const char* Member, std::string& Why)
	{
		LogEffectsOnce();
		const PathStep step{ '.', Member, 0 };
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue effect, value;
			NlCore::EffectSide side = NlCore::EffectSide::None;
			std::string ignored;
			if (EffectOf(building, generic, effect, side) && side == Wanted && NlAccess::Follow(effect, { step }, value, ignored))
				V(effect, step, building, 0, -1, value);
			return true;
		}, Why);
	}

	// 좋은 효과의 범위(치트 표의 effect_range). 효과를 내는 건물은 지어질 때 범위를 사각형으로 굳혀 둔다: 쓴 뒤에 지어지는 건물부터 먹는다.
	bool WalkGoodRanges(const NlJobs::Visit& V, std::string& Why) { return WalkEffects(V, NlCore::EffectSide::Good, "__range", Why); }
	// 나쁜 효과의 값(치트 표의 bad_effects_off). 엔진이 0 으로 쓰고 끄면 되돌린다. 쓴 뒤에 RefreshEffects 가 건물마다 합을 다시 내게 한다.
	bool WalkBadValues(const NlJobs::Visit& V, std::string& Why) { return WalkEffects(V, NlCore::EffectSide::Bad, "__effect", Why); }

	// 건물마다 받는 효과의 합을 다시 내게 한다: 게임의 c_effect.__update_applied_effects()(BuildingComponentEffects).
	// 인자가 없다는 것은 기계어로 봤다(0x141C112C0: 본문이 argc 를 어디에도 옮기지 않는다). 31번 불러 탈이 없었고, 종류의 값을 바꾼 뒤 부르자 합이 따라왔다(research/32).
	// 이 함수는 받는 효과의 목록은 그대로 두고 합만 다시 낸다(범위에 드는 건물을 다시 찾지는 않는다).
	void RefreshEffects()
	{
		const int count = NlAccess::InstanceCount("o_building");
		Log("build: calling __update_applied_effects on the buildings (" + std::to_string(count) + " instance(s))");		// 부르기 전에 남긴다
		size_t called = 0, failed = 0;
		std::string first_failure;
		for (int i = 0; i < count; i++)
		{
			const std::string base = "inst:o_building:" + std::to_string(i) + ".c_effect";
			RValue component, result;		// 이 함수 안에서만 든다
			std::string why;
			if (!NlAccess::Read(NlCore::ParseAskPath(base), component, why) || !component.IsStruct())
				continue;		// 효과 구성요소가 없는 건물
			if (NlAccess::CallMethod(NlCore::ParseAskPath(base + ".__update_applied_effects"), {}, result, why))
				called++;
			else if (!failed++)
				first_failure = "o_building:" + std::to_string(i) + ": " + why;
		}
		Log("build: effects updated on " + std::to_string(called) + " building(s)" + (failed ? ", " + std::to_string(failed) + " failed (first: " + first_failure + ")" : ""));
	}

	// ---- 즉시 업그레이드 (research/09) ----

	constexpr const char* k_Instant = "instant_upgrade";
	// 건물(o_building)의 건설 구성요소 c_construction(BuildingComponentConstruction).__construction_status.
	// 3 인 건물에서 is_under_upgrade() 가 참, 0 인 건물에서 거짓이었다. 다른 값(짓는 중, 수리, 옮기기)은 건드리지 않는다.
	constexpr double k_UnderUpgrade = 3;
	// build_instantly(): 인자를 읽지 않는 것을 기계어로 봤다(0x142A224A0). 진행도를 1 로 쓰고 __check_progress_is_complete() 를 부른다.
	// 업그레이드 중인 건물에 부르면 등급이 하나 오르고 상태가 0 이 된다(주택, 돼지 농장, 병영에서 봤다).
	constexpr const char* k_Component = ".c_construction";

	NlCore::Retry g_UpgradeRetry(2, 60);	// 불렀는데도 업그레이드 중으로 남으면 간격을 늘린다
	size_t g_Upgraded = 0;					// 이 실행에서 바로 끝낸 수

	void FinishUpgrades(double Now)
	{
		const int count = NlAccess::InstanceCount("o_building");
		size_t failed = 0;
		std::string first_failure;
		for (int i = 0; i < count; i++)
		{
			const std::string base = "inst:o_building:" + std::to_string(i) + k_Component;
			double status = 0;
			if (!NlAccess::ReadNumber(base + ".__construction_status", status) || status != k_UnderUpgrade)
				continue;

			Log("build: calling build_instantly on o_building:" + std::to_string(i) + " (under upgrade)");		// 부르기 전에 남긴다
			RValue result;		// 이 함수 안에서만 든다
			std::string why;
			const bool called = NlAccess::CallMethod(NlCore::ParseAskPath(base + ".build_instantly"), {}, result, why);
			double after = k_UnderUpgrade;
			if (called && NlAccess::ReadNumber(base + ".__construction_status", after) && after != k_UnderUpgrade)
				g_Upgraded++;
			else if (!failed++)
				first_failure = "o_building:" + std::to_string(i) + ": " + (called ? "still under upgrade" : why);
		}

		if (failed)
		{
			Log("build: " + std::to_string(failed) + " upgrade(s) not finished (first: " + first_failure + ")");
			g_UpgradeRetry.Failed(Now);
			NlCheats::SetNote(k_Instant, std::to_string(failed) + "채를 끝내지 못했습니다 (" + first_failure + ")");
			return;
		}
		g_UpgradeRetry.Succeeded();
		NlCheats::SetNote(k_Instant, g_Upgraded ? "업그레이드 " + std::to_string(g_Upgraded) + "번을 바로 끝냈습니다" : "업그레이드를 누르면 바로 끝납니다");
	}

	void TickUpgrades(double Now)
	{
		if (!NlCheats::IsOn(k_Instant))
		{
			g_UpgradeRetry.Succeeded();
			return;
		}
		if (!NlAccess::InGame())
		{
			NlCheats::SetNote(k_Instant, "게임을 시작하면 적용");
			return;
		}
		if (g_UpgradeRetry.Due(Now))
			FinishUpgrades(Now);
	}
}

void NlBuild::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
	// 주기 2초: 앞의 Retry(2초부터 두 배씩)가 "아직 0 인가"를 다시 보던 것과 같은 뜻이다. 엔진은 다 쓴 뒤 주기마다 훑어 바뀐 것만 쓴다.
	NlJobs::Add({ "build_free", "construction costs", true, &WalkCosts, nullptr, 2 });
	// 건물 종류의 자료는 게임이 켜질 때 만들어진다. 15초마다 다시 훑어 게임이 다시 만들었으면 다시 쓴다(조리법과 같다).
	NlJobs::Add({ "housing_capacity", "living places (housing)", false, &WalkHousing, nullptr, 15 });
	NlJobs::Add({ "barrack_capacity", "living places (barracks)", false, &WalkBarracks, nullptr, 15 });
	// 좋은 효과의 범위는 메인 메뉴에서도 쓴다(AnyScreen): 건물은 지어지거나 불러와질 때 범위를 굳히므로, 세이브를 불러오기 전에 써 두어야 그 세이브의 건물에 먹는다.
	NlJobs::Add({ "effect_range", "good effect ranges", false, &WalkGoodRanges, nullptr, 15, true });
	NlJobs::Add({ "bad_effects_off", "bad effect values", true, &WalkBadValues, &RefreshEffects, 15 });
}

void NlBuild::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;
	TickUpgrades(Now);
}
