#include "Production.hpp"

#include "Access.hpp"
#include "Buildings.hpp"
#include "Jobs.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
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
	// 창고 종류(hall, storage, granary, armory, default) → __capacity_in_categories.<갈래>.capacity
	constexpr const char* k_Warehouses = "inst:o_data.__building_warehouse_data.__generic_warehouses";
	// 영지 창고가 갈래별 용량의 합을 캐시해 둔다. 창고가 바뀔 때마다 게임이 인자 없이 부르는 것을 기록했다. 부르면 다음에 물을 때 다시 셈한다.
	constexpr const char* k_CleanCache = "inst:o_game_map_controller.__province.__warehouse.clean_cached_total_capacity_for_storage_type";

	NlProduction::LogFn g_Log;

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
		std::vector<double> seen;
		return NlBuildings::ForEachType([&](const std::string& key, const RValue& generic) {
			RValue map;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__production", 0 }, { '.', "__map_of_production", 0 } }, map, why) || !NlGame::IsNumber(map))
				return true;		// 만드는 것이 없는 건물 종류
			const double id = map.ToDouble();
			if (std::find(seen.begin(), seen.end(), id) != seen.end())
				return true;
			seen.push_back(id);

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
		}, Why);
	}

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
	// 틱의 차례는 등록한 차례다(앞의 g_Jobs 배열과 같다). 종교·임신·범죄·건설비의 일은 제 영역의 파일이 등록한다(World, People, Crime, Build).
	NlJobs::Add({ "storage_capacity", "capacity", false, &WalkCapacity, &CleanCapacityCache, 3 });
	NlJobs::Add({ "production_amount", "production amount", false, &WalkAmounts, nullptr, 15 });
	NlJobs::Add({ "production_free", "production inputs", true, &WalkInputs, nullptr, 15 });
}
