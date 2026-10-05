#include "Production.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CostBook.hpp"
#include "core/Knobs.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::Shortest;

namespace
{
	// research/10 에서 잰 것이다(0.5588.9777.0).
	// 건물 종류의 이름들과 구조체(research/09. src/Build.cpp 와 같다).
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
	// 창고 종류(hall, storage, granary, armory, default) → __capacity_in_categories.<갈래>.capacity
	constexpr const char* k_Warehouses = "inst:o_data.__building_warehouse_data.__generic_warehouses";
	// 영지 창고가 갈래별 용량의 합을 캐시해 둔다. 창고가 바뀔 때마다 게임이 인자 없이 부르는 것을 기록했다. 부르면 다음에 물을 때 다시 셈한다.
	constexpr const char* k_CleanCache = "inst:o_game_map_controller.__province.__warehouse.clean_cached_total_capacity_for_storage_type";

	NlProduction::LogFn g_Log;
	double g_Next = 0;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 대상 하나: In 안의 Step 자리에 수가 있다. Key·Level·Slot 은 장부에서 그 자리를 가리는 이름이다.
	using Visit = std::function<void(const RValue& In, const PathStep& Step, const std::string& Key, int Level, int Slot, const RValue& Value)>;

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

	struct Job
	{
		const char* Cheat;			// 치트 표의 Id
		const char* What;			// 로그에 쓸 이름
		bool Zero;					// 켜면 0 을 쓴다(배율이 없다). 아니면 창에서 정한 배율을 곱한다
		bool (*Walk)(const Visit&, std::string&);
		void (*After)();			// 값을 쓴 뒤에 부른다(없으면 nullptr)
		double Period;				// 다 쓴 뒤 다시 훑는 간격(초). 게임이 자료를 다시 만들면 그때 다시 쓴다

		NlCore::CostBook Book;		// 처음 본 값. 비어 있지 않으면 되돌릴 것이 남아 있다
		NlCore::Retry Retry{ 2, 60 };
		double Target = 1;			// 바라는 배율. 1 은 원래 값, 0 은 "0 으로 쓴다"
		int Misses = 0;				// 되돌릴 자리를 잇달아 찾지 못한 횟수
		double NextPass = 0;
		bool Settled = false;		// 바라는 배율이 다 써졌다
		bool Announced = false;
		std::string Note, Logged;
	};

	Job g_Jobs[] = {
		{ "storage_capacity", "capacity", false, &WalkCapacity, &CleanCapacityCache, 3 },
		{ "production_amount", "production amount", false, &WalkAmounts, nullptr, 15 },
		{ "production_free", "production inputs", true, &WalkInputs, nullptr, 15 },
	};

	std::string Place(const std::string& Key, int Level, int Slot)
	{
		return Key + (Level ? " product " + std::to_string(Level) : "") + (Slot < 0 ? "" : " resource " + std::to_string(Slot));
	}

	// 같은 줄을 잇달아 적지 않는다(주기마다 같은 실패가 되풀이될 수 있다).
	void LogOnce(Job& J, const std::string& Line)
	{
		if (Line == J.Logged)
			return;
		J.Logged = Line;
		Log(Line);
	}

	// 모든 대상에 바라는 값을 쓴다. Target 이 1 이면 장부의 값으로 되돌리고, 되돌린 자리는 장부에서 지운다.
	void Pass(Job& J, double Now)
	{
		const bool restoring = J.Target == 1;
		const bool again = J.Settled;		// 다 써 둔 뒤의 훑기다. 여기서 또 쓰게 되면 게임이 값을 되돌린 것이다
		const size_t before = J.Book.Size();
		std::vector<char> restored(before, 0);
		size_t written = 0, failed = 0, held = 0;
		std::string first_failure, why;
		// 이번 훑기에서 다룬 자리(담는 것의 포인터, 단계). 게임이 한 구조체를 두 이름 아래에서 함께 쓰면 같은 자리를 두 번 만난다.
		// 두 번째는 건너뛴다: 써 둔 값을 다른 열쇠의 바탕으로 적어 배율을 두 번 곱하지 않게.
		std::vector<std::pair<const void*, std::string>> touched;

		if (!J.Announced)
		{
			J.Announced = true;
			Log(std::string(J.What) + ": walking the game data for the first time");		// 부르기 전에 남긴다(건물 종류를 얻는 스크립트를 부른다)
		}
		const bool walked = J.Walk([&](const RValue& in, const PathStep& step, const std::string& key, int level, int slot, const RValue& value) {
			if (!NlGame::IsNumber(value))
				return;
			const std::pair<const void*, std::string> place(in.m_Pointer, step.Kind + step.Name + Shortest(step.Index));
			if (std::find(touched.begin(), touched.end(), place) != touched.end())
				return;
			touched.push_back(place);

			const double current = value.ToDouble();
			double wanted = current;
			size_t index = 0;
			if (!NlCore::PlanValue(J.Book, key, level, slot, current, J.Target, J.Zero, wanted, index))
				return;
			std::string write_why;
			if (current == wanted || NlAccess::SetNumber(in, step, wanted, write_why))
			{
				written += current != wanted;
				held++;
				if (restoring && index < restored.size())
					restored[index] = 1;
			}
			else if (!failed++)
				first_failure = Place(key, level, slot) + ": " + write_why;
		}, why);

		if (!walked)
		{
			LogOnce(J, std::string(J.What) + ": not written: " + why);
			J.Retry.Failed(Now);
			J.Note = "하지 못했습니다: " + why;
			return;
		}

		if (restoring)
			J.Book.Forget(restored);
		const std::string target = restoring ? "the first values" : J.Zero ? "0" : "x" + Shortest(J.Target);
		if (written || failed || (restoring && before))
			LogOnce(J, std::string(J.What) + ": wrote " + std::to_string(written) + " value(s) to " + target + ", " + std::to_string(held) + " hold it, book "
				+ std::to_string(J.Book.Size()) + (failed ? ", " + std::to_string(failed) + " did not stick (first: " + first_failure + ")" : ""));
		if (written && J.After)
			J.After();

		if (failed || (restoring && !J.Book.Empty()))
		{
			// 써지지 않았거나, 되돌릴 자리를 다시 찾지 못했다(게임이 그 자료를 다시 만들었으면 그 자리는 이미 원래 값이다).
			J.Retry.Failed(Now);
			J.Misses = restoring && !failed ? J.Misses + 1 : 0;		// 쓰기 실패와 따로 센다
			if (J.Misses >= 5)
			{
				Log(std::string(J.What) + ": giving up on " + std::to_string(J.Book.Size()) + " value(s): their place is gone");
				J.Book.Clear();
				J.Retry.Succeeded();
				J.Misses = 0;
				J.Settled = true;
				J.Note.clear();
				return;
			}
			J.Note = failed ? std::to_string(failed) + "칸이 써지지 않았습니다 (" + first_failure + ")"
				: std::to_string(J.Book.Size()) + "칸을 되돌리지 못했습니다 (다시 해 봅니다)";
			return;
		}

		J.Misses = 0;
		if (again && written)
			J.Retry.Failed(Now);		// 게임이 값을 되돌려 다시 썼다. 잇달아 그러면 주기마다 쓰지 않게 간격을 늘린다(src/Build.cpp 와 같다)
		else
			J.Retry.Succeeded();
		J.Settled = true;
		J.Note = restoring ? std::string() : std::to_string(held) + (J.Zero ? "칸을 0 으로 썼습니다" : "칸에 배율 " + Shortest(J.Target) + " 을 썼습니다");
	}

	void Tick(Job& J, double Now)
	{
		double factor = 1;
		const bool want = J.Zero ? NlCheats::IsOn(J.Cheat) : (NlCheats::Factor(J.Cheat, factor) && factor > 0 && factor != 1);
		const double target = !want ? 1 : J.Zero ? 0 : factor;
		if (target != J.Target)
		{
			J.Target = target;
			J.Settled = false;
			J.Retry.Succeeded();		// 사용자가 바꿨다. 기다리지 않고 바로 한다
		}
		if (!want && J.Book.Empty())
			return;						// 되돌릴 것이 없다
		if (!NlAccess::InGame())
		{
			if (want)
				NlCheats::SetNote(J.Cheat, "게임을 시작하면 적용");
			return;
		}
		if (!J.Retry.Due(Now) || (J.Settled && Now < J.NextPass))
		{
			NlCheats::SetNote(J.Cheat, J.Note);
			return;
		}

		J.NextPass = Now + J.Period;
		Pass(J, Now);
		NlCheats::SetNote(J.Cheat, J.Note);
	}
}

void NlProduction::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

void NlProduction::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;

	for (Job& job : g_Jobs)
		Tick(job, Now);
}
