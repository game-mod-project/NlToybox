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

	bool g_EffectsLogged = false;		// 효과의 목록은 한 실행에 한 번만 적는다

	// 구조체의 멤버 하나를 로그에 적을 글로(글은 그대로, 수는 가장 짧게, 그 밖과 없는 것은 "?").
	std::string Brief(const RValue& In, const char* Member)
	{
		RValue value;
		std::string ignored;
		if (!NlAccess::Follow(In, { { '.', Member, 0 } }, value, ignored))
			return "?";
		return value.IsString() ? value.ToString() : NlGame::IsNumber(value) ? NlCore::Shortest(value.ToDouble()) : "?";
	}

	// 건물 종류마다의 효과를 로그에 적는다(쓰기 전의 값으로): 효과(이름, 범위, 크기, 종류)마다 그것을 가진 건물 종류들과 구조체의 수.
	// 어느 건물이 어떤 효과를 갖는지, 건물 종류들이 한 구조체를 함께 쓰는지를 이 빌드에서 보려고 둔다.
	void LogEffects()
	{
		std::map<std::string, std::pair<std::string, std::set<const void*>>> seen;
		std::string why;
		NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue effect;
			std::string ignored;
			if (!NlAccess::Follow(generic, { { '.', "__effect", 0 } }, effect, ignored) || !effect.IsStruct())
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

	// 건물 효과의 범위(치트 표의 effect_range): 건물 종류의 __effect.__range 를 엔진에 넘긴다. Key 는 건물 종류의 이름.
	// 건물 종류들이 한 구조체를 함께 쓰면 엔진이 그 자리를 한 번만 다룬다(같은 훑기에서 같은 자리는 건너뛴다).
	bool WalkEffectRanges(const NlJobs::Visit& V, std::string& Why)
	{
		if (!g_EffectsLogged)
		{
			g_EffectsLogged = true;
			LogEffects();
		}
		const PathStep step{ '.', "__range", 0 };
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue effect, range;
			std::string ignored;
			if (NlAccess::Follow(generic, { { '.', "__effect", 0 } }, effect, ignored) && effect.IsStruct() && NlAccess::Follow(effect, { step }, range, ignored))
				V(effect, step, building, 0, -1, range);
			return true;
		}, Why);
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
	NlJobs::Add({ "effect_range", "effect ranges", false, &WalkEffectRanges, nullptr, 15 });
}

void NlBuild::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;
	TickUpgrades(Now);
}
