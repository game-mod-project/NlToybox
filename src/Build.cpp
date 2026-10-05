#include "Build.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/CostBook.hpp"
#include "core/Text.hpp"

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;

namespace
{
	// research/09 에서 잰 것이다(0.5588.9777.0).
	constexpr const char* k_Cheat = "build_free";
	// 인자 없이 건물 종류의 이름 171개를 배열로 돌려준다(기계어로 인자 0 을 봤고, treecall 로 불러 봤다).
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	// (이름) → 건물 종류의 구조체. 게임이 (string)으로 수천 번 부르는 것을 기록했다.
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
	// 건물 종류.__construction_cost.levels[등급] = undefined 또는 { money, resources.__array_of_resource_quantity[39] }

	NlBuild::LogFn g_Log;
	NlCore::CostBook g_Book;		// 0 으로 쓰기 전에 본 값. 게임 스레드만 만진다
	bool g_Applied = false;
	double g_Next = 0;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 건물 종류의 등급별 비용 구조체들을 차례로 넘긴다. 돌려주는 값: 건물 종류의 수. 못 얻으면 -1 이고 Why 에 까닭.
	int ForEachCost(const std::function<void(const std::string& Building, int Level, const RValue& Cost)>& Visit, std::string& Why)
	{
		RValue names;		// 이 함수 안에서만 든다
		if (!NlGame::CallScript(k_AllNames, {}, names) || !names.IsArray())
		{
			Why = "the list of buildings is not available";
			return -1;
		}

		int buildings = 0;
		NlAccess::ForEachChild(names, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (!name.IsString())
				return true;
			RValue generic, levels;
			std::string why;
			if (!NlGame::CallScript(k_Generic, { name }, generic) || !generic.IsStruct()
				|| !NlAccess::Follow(generic, { { '.', "__construction_cost", 0 }, { '.', "levels", 0 } }, levels, why) || !levels.IsArray())
				return true;
			buildings++;
			const std::string building = name.ToString();
			NlAccess::ForEachChild(levels, Holder::Array, [&](const PathStep& step, const RValue& cost) {
				if (cost.IsStruct())
					Visit(building, static_cast<int>(step.Index), cost);
				return true;
			});
			return true;
		});
		return buildings;
	}

	// 등급 하나의 비용에서 칸들의 배열(자원)을 얻는다.
	bool Quantities(const RValue& Cost, RValue& Out)
	{
		std::string why;
		return NlAccess::Follow(Cost, { { '.', "resources", 0 }, { '.', "__array_of_resource_quantity", 0 } }, Out, why) && Out.IsArray();
	}

	// 모든 건물 종류의 비용을 0 으로 쓴다. 처음 본 값은 장부에 적는다. 돌려주는 글: 한 일.
	std::string ZeroAll()
	{
		size_t written = 0;
		std::string why;
		const int buildings = ForEachCost([&](const std::string& building, int level, const RValue& cost) {
			RValue money, quantities, ignored;
			std::string ignored_why;
			if (NlAccess::Follow(cost, { { '.', "money", 0 } }, money, ignored_why) && NlGame::IsNumber(money) && money.ToDouble() != 0)
			{
				g_Book.Remember(building, level, -1, money.ToDouble());
				NlGame::Call("variable_struct_set", { cost, RValue(std::string_view("money")), RValue(0.0) }, ignored);
				written++;
			}
			if (!Quantities(cost, quantities))
				return;
			NlAccess::ForEachChild(quantities, Holder::Array, [&](const PathStep& step, const RValue& value) {
				if (NlGame::IsNumber(value) && value.ToDouble() != 0)
				{
					g_Book.Remember(building, level, static_cast<int>(step.Index), value.ToDouble());
					NlGame::Call("array_set", { quantities, RValue(step.Index), RValue(0.0) }, ignored);
					written++;
				}
				return true;
			});
		}, why);
		if (buildings < 0)
			return "하지 못했습니다: " + why;
		Log("build: zeroed " + std::to_string(written) + " cost value(s) in " + std::to_string(buildings) + " building type(s), book "
			+ std::to_string(g_Book.Size()));
		return "건물 종류 " + std::to_string(buildings) + "개의 비용을 0 으로 썼습니다 (" + std::to_string(g_Book.Size()) + "칸)";
	}

	// 장부의 값으로 되돌린다.
	std::string RestoreAll()
	{
		size_t restored = 0;
		std::string why;
		const int buildings = ForEachCost([&](const std::string& building, int level, const RValue& cost) {
			RValue quantities, ignored;
			const bool have = Quantities(cost, quantities);
			for (const NlCore::CostEntry& entry : g_Book.Entries())
			{
				if (entry.Level != level || entry.Building != building)
					continue;
				if (entry.Slot < 0)
					NlGame::Call("variable_struct_set", { cost, RValue(std::string_view("money")), RValue(entry.Value) }, ignored);
				else if (have && entry.Slot < NlGame::ArrayLength(quantities))
					NlGame::Call("array_set", { quantities, RValue(static_cast<double>(entry.Slot)), RValue(entry.Value) }, ignored);
				else
					continue;
				restored++;
			}
		}, why);
		if (buildings < 0)
			return "되돌리지 못했습니다: " + why;
		Log("build: restored " + std::to_string(restored) + "/" + std::to_string(g_Book.Size()) + " cost value(s)");
		return "";
	}

	// 0 으로 쓴 값이 그대로인가. 게임이 건물 종류를 다시 만들면(다른 게임을 불러올 때 그런지는 모른다) 장부의 첫 자리에 값이 돌아와 있다.
	bool StillZero()
	{
		if (g_Book.Empty())
			return true;
		const NlCore::CostEntry& entry = g_Book.Entries().front();
		RValue generic, value;
		std::string why;
		if (!NlGame::CallScript(k_Generic, { RValue(std::string_view(entry.Building)) }, generic) || !generic.IsStruct())
			return true;		// 알 수 없으면 다시 쓰지 않는다
		std::vector<PathStep> steps = { { '.', "__construction_cost", 0 }, { '.', "levels", 0 }, { '[', "", static_cast<double>(entry.Level) } };
		if (entry.Slot < 0)
			steps.push_back({ '.', "money", 0 });
		else
		{
			steps.push_back({ '.', "resources", 0 });
			steps.push_back({ '.', "__array_of_resource_quantity", 0 });
			steps.push_back({ '[', "", static_cast<double>(entry.Slot) });
		}
		return !NlAccess::Follow(generic, steps, value, why) || !NlGame::IsNumber(value) || value.ToDouble() == 0;
	}
}

void NlBuild::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

void NlBuild::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;

	const bool want = NlCheats::IsOn(k_Cheat);
	if (!want && !g_Applied)
		return;
	if (!NlAccess::InGame())
	{
		if (want)
			NlCheats::SetNote(k_Cheat, "게임을 시작하면 적용");
		return;
	}

	if (want)
	{
		if (g_Applied && StillZero())
			return;
		Log(std::string("build: zeroing construction costs") + (g_Applied ? " again (the game rebuilt the building types)" : ""));		// 부르기 전에 남긴다
		const std::string note = ZeroAll();
		g_Applied = true;
		NlCheats::SetNote(k_Cheat, note);
	}
	else
	{
		Log("build: restoring construction costs");
		const std::string note = RestoreAll();
		if (note.empty())
		{
			g_Applied = false;
			g_Book.Clear();
		}
		NlCheats::SetNote(k_Cheat, note);
	}
}
