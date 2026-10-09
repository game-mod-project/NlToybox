#include "Mines.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/Text.hpp"
#include "core/WorldPlan.hpp"

#include <mutex>

using namespace YYTK;
using NlAccess::Holder;

namespace
{
	// 광산의 매장량(research/25): 광산의 자리("69_156") -> 남은 수. 게임이 캘 때마다 1 씩 줄인다.
	constexpr const char* k_MineStock = "inst:o_game_map_controller.__current_local_map.__mines_manager.__mines_stock";
	constexpr const char* k_MineCheat = "mine_stock_hold";			// 치트 표의 항목
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";

	std::recursive_mutex g_Mutex;
	NlMines::LogFn g_Log;
	double g_Next = 0;
	NlCore::StockBook g_Mines;					// 매장량 붙들기가 광산마다 기억한 수와 그것의 자리(core/WorldPlan)
	int g_MineWrites = 0;						// 되돌려 쓴 횟수
	bool g_MineWasOn = false;					// 지난 틱에 켜져 있었는가(끈 틱에 한 번 정리한다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 붙들기가 기억한 값이 어느 게임·어느 지도의 것인지(core/PlaceKey): 그 관리자 구조체의 주소와 지도 관리 인스턴스.
	bool PlaceOf(const std::string& Path, NlCore::PlaceKey& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		int64_t instance = 0;
		if (!NlAccess::Read(NlCore::ParseAskPath(Path), value, why) || !value.IsStruct()
			|| !NlAccess::InstanceIdentity(NlCore::ParseAskPath("inst:o_game_map_controller"), instance))
			return false;
		Out.Struct = reinterpret_cast<std::uintptr_t>(value.m_Object);
		Out.Instance = instance;
		return true;
	}

	// 광산의 매장량 붙들기(치트 표의 mine_stock_hold): 1초마다 줄어든 매장량을 줄기 전의 수로 되돌려 쓴다. 판단은 core/WorldPlan 의 KeepStock.
	void MineTick(bool On)
	{
		if (!On)		// 끈 틱에 한 번 온다(g_MineWasOn)
		{
			if (g_MineWrites > 0)
				Log("world: mine stock hold off after " + std::to_string(g_MineWrites) + " write(s)");
			g_Mines = NlCore::StockBook{};
			g_MineWrites = 0;
			NlCheats::SetNote(k_MineCheat, std::string());
			return;
		}
		double now = 0;
		NlCore::PlaceKey place;
		const bool in_game = NlAccess::InGame();
		if (!in_game || !NlAccess::ReadNumber(k_GameTime, now) || !PlaceOf(k_MineStock, place))
		{
			g_Mines = NlCore::StockBook{};		// 게임 화면이 아니거나 읽지 못했다: 다음에 앞의 수를 쓰지 않는다
			NlCheats::SetNote(k_MineCheat, in_game ? "광산의 매장량을 읽지 못했습니다" : std::string());
			return;
		}
		NlCore::EnterStockPlace(g_Mines, place, now);		// 다른 세이브나 다른 지도의 광산이면 앞에서 본 수를 버린다

		RValue box;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!NlAccess::Open(NlCore::ParseAskPath(k_MineStock), box, kind, why) || kind != Holder::Struct)
		{
			NlCheats::SetNote(k_MineCheat, "광산의 매장량을 읽지 못했습니다");
			return;
		}
		struct Fix
		{
			NlCore::PathStep Step;
			double Value;
		};
		std::vector<Fix> fixes;		// 도는 동안에는 쓰지 않는다
		int mines = 0;
		NlAccess::ForEachChild(box, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			if (!NlGame::IsRealNumber(child))
				return true;
			mines++;
			double write = 0;
			if (NlCore::KeepStock(g_Mines.Kept, step.Name, child.ToDouble(), write))
				fixes.push_back({ step, write });
			return true;
		});
		for (const Fix& fix : fixes)
		{
			if (g_MineWrites % 20 == 0)		// 첫 번째와 그 뒤로 20번마다 남긴다
				Log("world: mine stock hold writes " + std::string(k_MineStock) + "." + fix.Step.Name + " back to " + NlCore::Shortest(fix.Value) + " (write " + std::to_string(g_MineWrites + 1) + ")");
			if (NlAccess::SetNumber(box, fix.Step, fix.Value, why))		// 쓴 뒤 다시 읽어 확인한다
				g_MineWrites++;
			else
				NlCheats::SetNote(k_MineCheat, "쓰지 못했습니다: " + why);
		}
		if (fixes.empty() || why.empty())
			NlCheats::SetNote(k_MineCheat, "광산 " + std::to_string(mines) + "곳" + (g_MineWrites > 0 ? ", 되돌려 쓴 횟수 " + std::to_string(g_MineWrites) : std::string()));
	}

}

void NlMines::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlMines::Tick(double Now)
{
	std::lock_guard lock(g_Mutex);
	if (Now < g_Next)
		return;
	g_Next = Now + 1;
	const bool mines = NlCheats::IsOn(k_MineCheat);
	if (mines || g_MineWasOn)
		MineTick(mines);
	g_MineWasOn = mines;
}
