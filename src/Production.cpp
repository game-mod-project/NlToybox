#include "Production.hpp"

#include "Access.hpp"
#include "Jobs.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CrimePlan.hpp"
#include "core/FamilyPlan.hpp"
#include "core/WorldPlan.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::Shortest;
using NlJobs::Visit;

namespace
{
	// research/10 에서 잰 것이다(0.5588.9777.0).
	// 건물 종류의 이름들과 구조체(research/09. src/Build.cpp 와 같다).
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
	// 창고 종류(hall, storage, granary, armory, default) → __capacity_in_categories.<갈래>.capacity
	constexpr const char* k_Warehouses = "inst:o_data.__building_warehouse_data.__generic_warehouses";
	// 설교의 종류 열 가지(__name, __cost …). research/21.
	constexpr const char* k_Preaches = "inst:o_data.__preach_data.__preach_list";
	// 영지 창고가 갈래별 용량의 합을 캐시해 둔다. 창고가 바뀔 때마다 게임이 인자 없이 부르는 것을 기록했다. 부르면 다음에 물을 때 다시 셈한다.
	constexpr const char* k_CleanCache = "inst:o_game_map_controller.__province.__warehouse.clean_cached_total_capacity_for_storage_type";

	NlProduction::LogFn g_Log;
	bool g_PreachSkippedLogged = false;		// 같은 줄을 주기마다 적지 않는다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 창고 종류마다의 갈래별 용량을 넘긴다. Key 는 "종류.갈래".
	bool WalkCapacity(const Visit& V, std::string& Why)
	{
		RValue types;		// 이 함수 안에서만 든다
		if (!NlAccess::Read(NlCore::ParseAskPath(k_Warehouses), types, Why) || !types.IsStruct())
		{
			Why = "the warehouse types are not available";
			return false;
		}
		NlAccess::ForEachChild(types, Holder::Struct, [&](const PathStep& type, const RValue& data) {
			RValue categories;
			std::string why;
			if (!data.IsStruct() || !NlAccess::Follow(data, { { '.', "__capacity_in_categories", 0 } }, categories, why) || !categories.IsStruct())
				return true;
			NlAccess::ForEachChild(categories, Holder::Struct, [&](const PathStep& category, const RValue& params) {
				const PathStep step{ '.', "capacity", 0 };
				RValue capacity;
				std::string ignored;
				if (params.IsStruct() && NlAccess::Follow(params, { step }, capacity, ignored))
					V(params, step, type.Name + "." + category.Name, 0, -1, capacity);
				return true;
			});
			return true;
		});
		return true;
	}

	// 건물 종류마다의 조리법을 넘긴다. Inputs 면 재료의 칸들을(Slot 은 자원 번호), 아니면 만들어지는 수를(Slot 은 -1).
	// 조리법은 ds_map(만드는 자원의 번호 → 구조체)에 있다. 건물 종류 여럿이 한 ds_map 을 함께 쓸 수 있으므로(등급별 건물 종류)
	// 한 번 본 ds_map 은 다시 보지 않는다. Key 는 그 ds_map 을 처음 내놓은 건물 종류의 이름이다(이름의 차례는 게임이 주는 배열의 차례라 늘 같다.
	// ds_map 의 번호는 게임이 자료를 다시 만들면 다른 것에 다시 쓰일 수 있어 열쇠로 삼지 않는다).
	bool WalkRecipes(const Visit& V, bool Inputs, std::string& Why)
	{
		RValue names;
		if (!NlGame::CallScript(k_AllNames, {}, names) || !names.IsArray())
		{
			Why = "the list of buildings is not available";
			return false;
		}

		std::vector<double> seen;
		NlAccess::ForEachChild(names, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (!name.IsString())
				return true;
			RValue generic, map;
			std::string why;
			if (!NlGame::CallScript(k_Generic, { name }, generic) || !generic.IsStruct()
				|| !NlAccess::Follow(generic, { { '.', "__production", 0 }, { '.', "__map_of_production", 0 } }, map, why) || !NlGame::IsNumber(map))
				return true;		// 만드는 것이 없는 건물 종류
			const double id = map.ToDouble();
			if (std::find(seen.begin(), seen.end(), id) != seen.end())
				return true;
			seen.push_back(id);

			const std::string key = name.ToString();
			NlAccess::ForEachChild(map, Holder::Map, [&](const PathStep& produced, const RValue& recipe) {
				double resource = 0;
				if (!recipe.IsStruct() || !NlCore::ParseNumber(produced.Name, resource))
					return true;
				const int level = static_cast<int>(resource);
				std::string ignored;
				if (Inputs)
				{
					RValue pile;
					if (!NlAccess::Follow(recipe, { { '.', "pile_of_raw_resources", 0 }, { '.', "__array_of_resource_quantity", 0 } }, pile, ignored) || !pile.IsArray())
						return true;
					NlAccess::ForEachChild(pile, Holder::Array, [&](const PathStep& step, const RValue& quantity) {
						V(pile, step, key, level, static_cast<int>(step.Index), quantity);
						return true;
					});
				}
				else
				{
					const PathStep step{ '.', "__quantity", 0 };
					RValue made, quantity;
					if (NlAccess::Follow(recipe, { { '.', "produced_resource", 0 } }, made, ignored) && made.IsStruct() && NlAccess::Follow(made, { step }, quantity, ignored))
						V(made, step, key, level, -1, quantity);
				}
				return true;
			});
			return true;
		});
		return true;
	}

	// 종교 행동의 비용: 게임 변수 여섯과 설교 종류마다의 __cost(inst:o_data.__preach_data.__preach_list. 열쇠는 "preach.<설교의 이름>"). research/21.
	bool WalkReligionCosts(const Visit& V, std::string& Why)
	{
		if (!NlJobs::WalkVars(V, NlCore::ReligionCostVars(), Why))
			return false;
		RValue list;
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(k_Preaches), list, why) || !list.IsArray())
		{
			Why = "the preach list is not available";		// 게임 변수만 쓰고 "됐다"고 하지 않는다
			return false;
		}
		// 장부의 열쇠는 설교의 이름이다. 이름이 비었거나 앞의 것과 같은 설교는 건너뛴다(한 칸을 둘이 함께 쓰면 끌 때 남의 값이 써진다).
		std::vector<std::string> seen;
		size_t skipped = 0;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& item) {
			const PathStep step{ '.', "__cost", 0 };
			RValue name, cost;
			std::string ignored;
			if (!item.IsStruct() || !NlAccess::Follow(item, { { '.', "__name", 0 } }, name, ignored) || !name.IsString() || name.ToString().empty()
				|| std::find(seen.begin(), seen.end(), name.ToString()) != seen.end() || !NlAccess::Follow(item, { step }, cost, ignored))
			{
				skipped++;
				return true;
			}
			seen.push_back(name.ToString());
			V(item, step, "preach." + name.ToString(), 0, -1, cost);
			return true;
		});
		if (skipped && !g_PreachSkippedLogged)
		{
			g_PreachSkippedLogged = true;
			Log("religion costs: skipped " + std::to_string(skipped) + " preach(es) without a usable name or cost (" + std::to_string(seen.size()) + " used)");
		}
		return true;
	}

	// 설교 전환 계수(게임 변수 하나).
	bool WalkPreachFactor(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PreachFactorVars(), Why); }

	// 기도와 예배가 신앙심을 되돌리는 양(게임 변수 넷).
	bool WalkPietyRestore(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PietyRestoreVars(), Why); }

	// 임신·출산의 게임 변수(research/24): 임신 확률 둘, 유산 확률 하나, 출산 중 사망 확률 둘.
	bool WalkPregnancyChance(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PregnancyChanceVars(), Why); }
	bool WalkMiscarriage(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::MiscarriageVars(), Why); }
	bool WalkChildbirthDeath(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::ChildbirthDeathVars(), Why); }

	// 범죄의 게임 변수들(research/26. 열쇠는 core/CrimePlan). 써지고 되돌려지는 것까지만 본다.
	bool WalkBanditTurn(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::BanditTurnVars(), Why); }
	bool WalkCrimeMinds(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::CrimeMindVars(), Why); }
	bool WalkThugDays(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::ThugDaysVars(), Why); }
	bool WalkTheftAmount(const Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::TheftAmountVars(), Why); }

	bool WalkAmounts(const Visit& V, std::string& Why) { return WalkRecipes(V, false, Why); }
	bool WalkInputs(const Visit& V, std::string& Why) { return WalkRecipes(V, true, Why); }

	// 용량을 쓴 뒤: 영지 창고의 캐시를 비워 새 용량으로 다시 셈하게 한다.
	void CleanCapacityCache()
	{
		Log("capacity: calling clean_cached_total_capacity_for_storage_type");		// 부르기 전에 남긴다
		RValue result;
		std::string why;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(k_CleanCache), {}, result, why))
			Log("capacity: the cache was not cleaned: " + why);
	}

}

void NlProduction::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
	// 틱의 차례는 등록한 차례다(앞의 g_Jobs 배열과 같다). 종교·임신·범죄의 일은 제 영역의 파일로 간다(리팩토링 C, Task 4).
	NlJobs::Add({ "storage_capacity", "capacity", false, &WalkCapacity, &CleanCapacityCache, 3 });
	NlJobs::Add({ "production_amount", "production amount", false, &WalkAmounts, nullptr, 15 });
	NlJobs::Add({ "production_free", "production inputs", true, &WalkInputs, nullptr, 15 });
	NlJobs::Add({ "religion_free", "religion costs", true, &WalkReligionCosts, nullptr, 15 });
	NlJobs::Add({ "piety_restore", "piety restore", false, &WalkPietyRestore, nullptr, 15 });
	NlJobs::Add({ "preach_conversion", "preach conversion", false, &WalkPreachFactor, nullptr, 15 });
	NlJobs::Add({ "pregnancy_chance", "pregnancy chance", false, &WalkPregnancyChance, nullptr, 15 });
	NlJobs::Add({ "no_miscarriage", "miscarriage chance", true, &WalkMiscarriage, nullptr, 15 });
	NlJobs::Add({ "safe_childbirth", "childbirth death chance", true, &WalkChildbirthDeath, nullptr, 15 });
	NlJobs::Add({ "no_bandit_turn", "bandit turn chance", true, &WalkBanditTurn, nullptr, 15 });
	NlJobs::Add({ "crime_minds_off", "crime minds", true, &WalkCrimeMinds, nullptr, 15 });
	NlJobs::Add({ "thug_days", "days to thug", false, &WalkThugDays, nullptr, 15 });
	NlJobs::Add({ "theft_none", "storage theft amount", true, &WalkTheftAmount, nullptr, 15 });
}
