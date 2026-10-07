#include "World.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Jobs.hpp"
#include "Events.hpp"
#include "Season.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::WorldAct;

namespace
{
	// 주교를 다루는 관리자. is_has_bishop()·debug_force_send_bishop() 은 인자가 없다(기계어). 불러서 주교가 오는 것을 봤다.
	constexpr const char* k_Religion = "inst:o_game_map_controller.__province.__religiosity_manager";

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlWorld::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	std::string g_UtilLast;				// 유틸(지금 저장)에 마지막으로 한 일
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	bool g_BishopCalled = false;		// 이 게임에서 주교를 이미 불렀다(디버그 함수를 되풀이해 부르지 않는다). 게임 화면이 아니게 되면 푼다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
	}

	bool HasBishop(bool& Has, std::string& Why)
	{
		RValue answer;
		if (!CallNoArgs(std::string(k_Religion) + ".is_has_bishop", answer, Why) || !NlGame::IsNumber(answer))
			return false;
		Has = answer.ToDouble() != 0;
		return true;
	}

	// 세이브 폴더(%LOCALAPPDATA%\Strategy\saves). 읽기만 한다(새 파일의 이름을 보려고).
	std::filesystem::path SavesDir()
	{
		char* local = nullptr;
		size_t length = 0;
		if (_dupenv_s(&local, &length, "LOCALAPPDATA") != 0 || !local)
			return std::filesystem::path();
		const std::filesystem::path dir = std::filesystem::path(local) / "Strategy" / "saves";
		std::free(local);
		return dir;
	}

	std::vector<std::string> ListSaves()
	{
		std::vector<std::string> names;
		std::error_code ec;
		const std::filesystem::path dir = SavesDir();
		if (dir.empty() || !std::filesystem::is_directory(dir, ec))
			return names;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
			if (entry.is_regular_file(ec) && entry.path().extension() == ".norland")
				names.push_back(entry.path().filename().string());
		return names;
	}

	// 지금 저장: 게임의 자동 저장 함수 save_game(0, 1)(아침의 자동 저장이 그 꼴로 불렀다. 저녁은 (0, 2). research/28).
	// 게임의 저장이 꺼져 있으면(유틸의 no_autosave) 부르지 않는다. 새 파일의 이름은 세이브 폴더를 앞뒤로 보고 적는다.
	std::string SaveNow()
	{
		double disabled = 0;
		if (!NlAccess::ReadNumber("inst:o_debug.is_save_disabled", disabled))
			return NlCore::SaveNowReport('u', std::string());
		if (disabled != 0)
			return NlCore::SaveNowReport('d', std::string());
		const std::vector<std::string> before = ListSaves();
		RValue result;		// 이 함수 안에서만 든다
		Log("world call gml_Script_save_game(0, 1)");		// 부르기 전에 남긴다
		if (!NlGame::CallScript("gml_Script_save_game", { RValue(0.0), RValue(1.0) }, result))
			return NlCore::SaveNowReport('f', "스크립트를 부르지 못했습니다");
		std::string fresh;
		for (const std::string& name : ListSaves())
			if (std::find(before.begin(), before.end(), name) == before.end())
				fresh = name;
		Log("world: save_game called" + (fresh.empty() ? std::string(" (no new file seen yet)") : ", new file " + fresh));
		return NlCore::SaveNowReport(fresh.empty() ? 'n' : 's', fresh);
	}

	// 설교의 종류 열 가지(__name, __cost …). research/21. 종교의 게임 변수 일 셋은 src/Jobs 의 엔진에 등록한다(리팩토링 C 에서 Production 에서 옮겼다).
	// 설교의 종류 열 가지(__name, __cost …). research/21.
	constexpr const char* k_Preaches = "inst:o_data.__preach_data.__preach_list";
	bool g_PreachSkippedLogged = false;		// 같은 줄을 주기마다 적지 않는다

	// 종교 행동의 비용: 게임 변수 여섯과 설교 종류마다의 __cost(inst:o_data.__preach_data.__preach_list. 열쇠는 "preach.<설교의 이름>"). research/21.
	bool WalkReligionCosts(const NlJobs::Visit& V, std::string& Why)
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
	bool WalkPreachFactor(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PreachFactorVars(), Why); }

	// 기도와 예배가 신앙심을 되돌리는 양(게임 변수 넷).
	bool WalkPietyRestore(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PietyRestoreVars(), Why); }

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
		{
			g_BishopCalled = false;
			return "게임 화면이 아닙니다";
		}
		switch (Act)
		{
		case WorldAct::SaveNow:
			return SaveNow();
		// 이벤트의 일(EventForce·EventList·EventCancel·EventEnd·CooldownsClear)은 여기 오지 않는다: NlWorld::Do 가 NlEvents::Do 로 바로 돌려준다.
		case WorldAct::BishopSend:
		{
			bool has = false;
			std::string why;
			const bool read = HasBishop(has, why);
			switch (NlCore::ChooseBishopStep(read, has))
			{
			case NlCore::BishopStep::Unknown:
				return "주교가 있는지 읽지 못했습니다: " + why;
			case NlCore::BishopStep::AlreadyHere:
				return "주교가 이미 있습니다";
			case NlCore::BishopStep::Call:
				break;
			}
			if (g_BishopCalled)		// 불렀는데 아직 없다고 읽힌다. 디버그 함수를 되풀이해 부르지 않는다
				return "주교를 이미 불렀습니다. 아직 보이지 않으면 세이브를 다시 불러온 뒤에 눌러 주세요";
			RValue result;
			Log("world call debug_force_send_bishop()");		// 부르기 전에 남긴다
			if (!CallNoArgs(std::string(k_Religion) + ".debug_force_send_bishop", result, why))
				return "주교를 부르지 못했습니다: " + why;
			g_BishopCalled = true;
			bool came = false;
			const bool reread = HasBishop(came, why);
			Log(std::string("world: bishop ") + (!reread ? "called (cannot read back)" : came ? "is here" : "called (not here yet)"));
			return reread && came ? "주교가 왔습니다" : "주교를 불렀습니다 (아직 왔다고 읽히지 않습니다)";
		}
		// 계절의 일(SeasonShow·SeasonDelay·SeasonEnd)은 여기 오지 않는다: NlWorld::Do 가 NlSeason::Do 로 바로 돌려준다(src/Season 이 제 자리에 결과를 둔다).
		}
		return std::string();
	}

	// 한 일의 글을 그 패널의 자리에 둔다(지금 저장은 유틸 패널에, 주교는 종교 패널에. 계절과 이벤트는 제 모듈이 제 자리에 둔다).
	std::string& LastOf(WorldAct Act)
	{
		return Act == WorldAct::SaveNow ? g_UtilLast : g_Last;
	}

	std::string Remember(WorldAct Act, std::string Text)
	{
		if (NlCore::WorldActChanges(Act))
			LastOf(Act) = Text;
		return Text;
	}

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	void DrawLast()
	{
		if (!g_Last.empty())
			NlUi::Hint(g_Last.c_str());
	}

	constexpr size_t k_MaxQueue = 4;		// 창이 쌓아 둘 청의 수. 넘치면 받지 않고 결과 줄에 적는다(조용히 버리지 않는다. 2026-10-07 리뷰 R3)

	void Push(WorldAct Act)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			LastOf(Act) = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(Act);
	}
}

void NlWorld::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	NlJobs::Add({ "religion_free", "religion costs", true, &WalkReligionCosts, nullptr, 15 });
	NlJobs::Add({ "piety_restore", "piety restore", false, &WalkPietyRestore, nullptr, 15 });
	NlJobs::Add({ "preach_conversion", "preach conversion", false, &WalkPreachFactor, nullptr, 15 });
}

    void NlWorld::GameTick(double Now)
    {
    	std::lock_guard lock(g_Mutex);
    	if (g_Busy)
    		return;
    	if (g_Queue.empty())		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다). 계절·광산·이벤트는 제 모듈의 틱이 한다
    		return;
    	(void)Now;
    	const NlCore::ScopedFlag busy(g_Busy);
    	const WorldAct act = g_Queue.front();
    	g_Queue.pop_front();
    	Remember(act, DoNow(act));
    }
    

void NlWorld::DrawWorld()
{
	NlSeason::Draw();		// 계절(src/Season). 월드 영역의 표의 항목 아래에 그린다
}

void NlWorld::DrawReligion()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("주교 부르기"))
		Push(WorldAct::BishopSend);
	NlUi::Hint("게임의 디버그 함수로 주교를 바로 오게 합니다(교단의 영주 하나가 영지에 나타납니다). 주교가 이미 있으면 부르지 않습니다. 되돌릴 수 없습니다.");
	DrawLast();
}

void NlWorld::DrawUtil()
{
	std::lock_guard lock(g_Mutex);
	if (ImGui::Button("지금 저장 (게임의 자동 저장 함수)"))
		Push(WorldAct::SaveNow);
	NlUi::Hint("게임의 자동 저장 함수를 아침의 꼴(save_game(0, 1))로 부릅니다: 세이브 폴더에 '…_Autosave_Morning_…' 파일이 하나 생깁니다. "
		"위의 '게임의 저장 끄기'가 켜져 있으면 부르지 않습니다.");
	if (!g_UtilLast.empty())
		NlUi::Hint(g_UtilLast.c_str());
}

std::string NlWorld::Do(NlCore::WorldAct Act)
{
	if (NlCore::IsSeasonAct(Act))
		return NlSeason::Do(Act);		// 계절은 src/Season 이 하고 제 자리(계절 패널)에 적는다. 이벤트·종교 패널의 "마지막 한 일"에 섞지 않는다(최종 리뷰 1번)
	if (NlCore::IsEventAct(Act))
		return NlEvents::Do(Act);		// 이벤트는 src/Events 가 하고 제 자리(이벤트 패널)에 적는다
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	return Remember(Act, DoNow(Act));
}
