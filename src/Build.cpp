#include "Build.hpp"

#include "Access.hpp"
#include "Buildings.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CostBook.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <algorithm>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;

namespace
{
	// research/09 에서 잰 것이다(0.5588.9777.0).
	constexpr const char* k_Cheat = "build_free";
	// (이름) → 건물 종류의 구조체. StillZero 가 장부의 첫 자리 하나를 다시 볼 때만 쓴다(걷기는 src/Buildings). 리팩토링 C 의 Task 3 에서 StillZero 와 함께 없어진다.
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
	// 건물 종류.__construction_cost.levels[등급] = undefined 또는 { money, resources.__array_of_resource_quantity[39] }

	NlBuild::LogFn g_Log;
	NlCore::CostBook g_Book;		// 0 으로 쓰기 전에 본 값. 게임 스레드만 만진다. 비어 있지 않으면 되돌릴 것이 남아 있다
	bool g_Applied = false;			// 마지막으로 0 으로 쓴 것이 모두 남았다
	bool g_Wanted = false;			// 앞 틱에 켜져 있었는가
	bool g_CheckLogged = false;
	double g_Next = 0;
	NlCore::Retry g_Retry(2, 60);	// 실패했거나 게임이 값을 되돌렸을 때 다시 해 볼 때. 잇달아 그러면 간격이 늘어난다
	std::string g_Note;				// 마지막으로 한 일(창에 보인다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	struct Walk
	{
		int Buildings = 0;		// 비용을 본 건물 종류의 수
		int Skipped = 0;		// 구조체를 얻지 못했거나 비용의 자리가 없는 건물 종류의 수
	};

	// 건물 종류의 등급별 비용 구조체들을 차례로 넘긴다. 건물 종류의 목록을 얻지 못하면 거짓이고 Why 에 까닭.
	bool ForEachCost(const std::function<void(const std::string& Building, int Level, const RValue& Cost)>& Visit, Walk& Seen, std::string& Why)
	{
		return NlBuildings::ForEachType([&](const std::string& building, const RValue& generic) {
			RValue levels;
			std::string why;
			if (!NlAccess::Follow(generic, { { '.', "__construction_cost", 0 }, { '.', "levels", 0 } }, levels, why) || !levels.IsArray())
			{
				Seen.Skipped++;
				return true;
			}
			Seen.Buildings++;
			NlAccess::ForEachChild(levels, Holder::Array, [&](const PathStep& step, const RValue& cost) {
				if (cost.IsStruct())
					Visit(building, static_cast<int>(step.Index), cost);
				return true;
			});
			return true;
		}, Why);
	}

	// 등급 하나의 비용에서 칸들의 배열(자원)을 얻는다.
	bool Quantities(const RValue& Cost, RValue& Out)
	{
		std::string why;
		return NlAccess::Follow(Cost, { { '.', "resources", 0 }, { '.', "__array_of_resource_quantity", 0 } }, Out, why) && Out.IsArray();
	}

	std::string Place(const std::string& Building, int Level, int Slot)
	{
		return Building + " level " + std::to_string(Level) + (Slot < 0 ? " money" : " resource " + std::to_string(Slot));
	}

	struct Outcome
	{
		bool Ok = false;
		std::string Note;		// 창에 보일 글
	};

	// 모든 건물 종류의 비용을 0 으로 쓴다. 되돌릴 값을 장부에 적은 자리에만 쓰고, 쓴 뒤 다시 읽어 남았는지 본다.
	Outcome ZeroAll()
	{
		size_t written = 0, failed = 0;
		std::string first_failure, why;
		Walk seen;
		const auto zero = [&](const RValue& in, const PathStep& step, const std::string& building, int level, int slot, const RValue& value) {
			// 0 인 자리는 쓸 것이 없다. 유한하지 않은 값은 장부가 받지 않는다: 되돌릴 수 없는 자리에는 쓰지 않는다.
			if (!NlGame::IsNumber(value) || value.ToDouble() == 0 || !g_Book.Remember(building, level, slot, value.ToDouble()))
				return;
			std::string write_why;
			if (NlAccess::SetNumber(in, step, 0, write_why))
				written++;
			else if (!failed++)
				first_failure = Place(building, level, slot) + ": " + write_why;
		};
		const bool walked = ForEachCost([&](const std::string& building, int level, const RValue& cost) {
			const PathStep money_step{ '.', "money", 0 };
			RValue money, quantities;
			std::string ignored;
			if (NlAccess::Follow(cost, { money_step }, money, ignored))
				zero(cost, money_step, building, level, -1, money);
			if (!Quantities(cost, quantities))
				return;
			NlAccess::ForEachChild(quantities, Holder::Array, [&](const PathStep& step, const RValue& value) {
				zero(quantities, step, building, level, static_cast<int>(step.Index), value);
				return true;
			});
		}, seen, why);
		if (!walked)
		{
			Log("build: not zeroed: " + why);
			return { false, "하지 못했습니다: " + why };
		}

		Log("build: zeroed " + std::to_string(written) + " cost value(s) in " + std::to_string(seen.Buildings) + " building type(s), book "
			+ std::to_string(g_Book.Size()) + (seen.Skipped ? ", skipped " + std::to_string(seen.Skipped) + " type(s)" : "")
			+ (failed ? ", " + std::to_string(failed) + " did not stick (first: " + first_failure + ")" : ""));
		if (failed)
			return { false, std::to_string(failed) + "칸이 써지지 않았습니다 (" + first_failure + ")" };
		if (g_Book.Empty())
			return { true, "비용이 이미 모두 0 입니다 (되돌릴 값이 없습니다)" };
		return { true, "건물 종류 " + std::to_string(seen.Buildings) + "개의 비용을 0 으로 썼습니다 (" + std::to_string(g_Book.Size()) + "칸)" };
	}

	// 장부의 값으로 되돌린다. 쓴 뒤 다시 읽어 남은 자리만 장부에서 지운다. 못 되돌린 자리는 장부에 남아 다음에 다시 되돌린다.
	Outcome RestoreAll()
	{
		const std::vector<NlCore::CostEntry>& entries = g_Book.Entries();
		std::vector<char> done(entries.size(), 0);
		std::string why;
		Walk seen;
		const bool walked = ForEachCost([&](const std::string& building, int level, const RValue& cost) {
			RValue quantities;
			const bool have = Quantities(cost, quantities);
			for (size_t i = 0; i < entries.size(); i++)
			{
				const NlCore::CostEntry& entry = entries[i];
				if (done[i] || entry.Level != level || entry.Building != building)
					continue;
				std::string write_why;
				if (entry.Slot < 0)
					done[i] = NlAccess::SetNumber(cost, { '.', "money", 0 }, entry.Value, write_why);
				else if (have)
					done[i] = NlAccess::SetNumber(quantities, { '[', "", static_cast<double>(entry.Slot) }, entry.Value, write_why);
			}
		}, seen, why);
		if (!walked)
		{
			Log("build: not restored: " + why);
			return { false, "되돌리지 못했습니다: " + why + " (다시 해 봅니다)" };
		}

		const size_t all = entries.size();
		const size_t restored = static_cast<size_t>(std::count(done.begin(), done.end(), static_cast<char>(1)));
		std::string left;
		for (size_t i = 0; i < all && left.empty(); i++)
			if (!done[i])
				left = Place(entries[i].Building, entries[i].Level, entries[i].Slot) + " = " + NlCore::Shortest(entries[i].Value);
		g_Book.Forget(done);
		Log("build: restored " + std::to_string(restored) + "/" + std::to_string(all) + " cost value(s)"
			+ (left.empty() ? "" : ", first left: " + left));
		if (g_Book.Empty())
			return { true, "" };
		return { false, std::to_string(g_Book.Size()) + "칸을 되돌리지 못했습니다 (다시 해 봅니다)" };
	}

	// 0 으로 쓴 값이 그대로인가. 게임이 건물 종류를 다시 만들면(다른 게임을 불러올 때 그런지는 모른다) 장부의 첫 자리에 값이 돌아와 있다.
	bool StillZero()
	{
		if (g_Book.Empty())
			return true;
		const NlCore::CostEntry& entry = g_Book.Entries().front();
		if (!g_CheckLogged)
		{
			g_CheckLogged = true;
			Log("build: checking every second that " + Place(entry.Building, entry.Level, entry.Slot) + " is still zero (calls get_generic_building)");		// 부르기 전에 남긴다
		}
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

	// ---- 건설비 (위의 ZeroAll, RestoreAll) ----

	void TickCosts(double Now)
	{
		const bool want = NlCheats::IsOn(k_Cheat);
		if (want != g_Wanted)
		{
			g_Wanted = want;
			g_Retry.Succeeded();		// 사용자가 바꿨다. 기다리지 않고 바로 한다
		}
		if (!want && g_Book.Empty())
		{
			g_Applied = false;			// 되돌릴 것이 없다
			return;
		}
		if (!NlAccess::InGame())
		{
			if (want)
				NlCheats::SetNote(k_Cheat, "게임을 시작하면 적용");
			return;
		}
		if (!g_Retry.Due(Now))
		{
			NlCheats::SetNote(k_Cheat, g_Note);
			return;
		}

		if (want)
		{
			const bool again = g_Applied;
			if (again && StillZero())
			{
				g_Retry.Succeeded();
				NlCheats::SetNote(k_Cheat, g_Note);		// 메뉴에 다녀오면 글이 "게임을 시작하면 적용"에 머물러 있다
				return;
			}
			Log(std::string("build: zeroing construction costs") + (again ? " again (they are no longer zero)" : ""));		// 부르기 전에 남긴다
			const Outcome done = ZeroAll();
			g_Applied = done.Ok;
			g_Note = done.Note;
			if (!done.Ok || again)
				g_Retry.Failed(Now);		// 실패했거나 게임이 값을 되돌렸다. 잇달아 그러면 1초마다 171종을 다시 쓰지 않게 간격을 늘린다
			else
				g_Retry.Succeeded();
		}
		else
		{
			g_Applied = false;
			Log("build: restoring construction costs");		// 부르기 전에 남긴다
			const Outcome done = RestoreAll();
			g_Note = done.Note;
			if (done.Ok)
				g_Retry.Succeeded();
			else
				g_Retry.Failed(Now);
		}
		NlCheats::SetNote(k_Cheat, g_Note);
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

	TickCosts(Now);
	TickUpgrades(Now);
}
