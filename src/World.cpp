#include "World.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Localization.hpp"
#include "core/SeasonPlan.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::WorldAct;

namespace
{
	// 이벤트를 고르는 감독(research/14). 쿨다운은 이벤트의 이름 → 남은 날, 묶음의 이름 → 남은 날.
	constexpr const char* k_Director = "inst:o_game_map_controller.__game_director";
	// 주교를 다루는 관리자. is_has_bishop()·debug_force_send_bishop() 은 인자가 없다(기계어). 불러서 주교가 오는 것을 봤다.
	constexpr const char* k_Religion = "inst:o_game_map_controller.__province.__religiosity_manager";

	// 계절(research/25). 지금 지도의 관리자(ExtremeSeasonManager). 남은 시간은 게임의 함수로 읽는다(인자 없음. 게임이 그 꼴로 부르는 것을 봤다).
	// 바꿀 때는 시작 시각에 쓴다: 게임은 남은 시간을 그 시각에서 셈한다(시작을 하루 뒤로 쓰자 함수의 값이 하루 늘었다).
	constexpr const char* k_Season = "inst:o_game_map_controller.__current_local_map.__season_manager";
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";
	constexpr const char* k_HoldCheat = "season_hold";				// 치트 표의 항목(core/CheatTable)
	constexpr const char* k_SeasonCaptionPrefix = "extreme_season.";	// 게임의 localization\main.csv 의 열쇠. 게임이 __extreme_season.__caption 에 그 열쇠를 든다
	// 광산의 매장량(research/25): 광산의 자리("69_156") -> 남은 수. 게임이 캘 때마다 1 씩 줄인다.
	constexpr const char* k_MineStock = "inst:o_game_map_controller.__current_local_map.__mines_manager.__mines_stock";
	constexpr const char* k_MineCheat = "mine_stock_hold";			// 치트 표의 항목
	constexpr double k_DelayStep = 86400;		// 미루기 한 번: 하루
	constexpr double k_EndLead = 60;			// 끝내기: 남은 시간을 1분으로

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlWorld::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창이 쌓고 틱이 한다
	std::string g_Last;					// 마지막으로 한 일
	std::string g_UtilLast;				// 유틸(지금 저장)에 마지막으로 한 일
	std::string g_PendingEvent;			// 일으킬 이벤트의 이름(창이 적고 틱이 쓴다)
	std::vector<std::string> g_EventNames;	// 게임의 이벤트 이름들(감독의 자료의 ds_map 열쇠. 게임 화면에서 한 번 읽는다)
	double g_NextNames = 0;				// 이름을 다시 읽어 볼 시각
	int g_EventPick = 0;				// 창의 선택
	constexpr const char* k_EventsData = "inst:o_data.__game_director_events_data";
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)
	bool g_BishopCalled = false;		// 이 게임에서 주교를 이미 불렀다(디버그 함수를 되풀이해 부르지 않는다). 게임 화면이 아니게 되면 푼다

	// 틱이 읽은 계절. 창은 이것만 그린다.
	struct Season
	{
		bool Read = false;
		std::string Why;			// 읽지 못한 까닭
		double Now = 0, Start = 0, Phase = 0;
		double Remain = 0;			// 지금 단계의 남은 초
		double ToExtreme = 0;		// 가혹한 계절이 올 때까지(가혹한 계절이 아닐 때만 읽는다)
		double ToEnd = 0;			// 가혹한 계절이 끝날 때까지(가혹한 계절일 때만 읽는다)
		bool Extreme = false;
		std::string Name;			// 가혹한 계절의 화면 이름(게임의 글). 없으면 게임의 이름
	};
	Season g_Season;
	std::string g_SeasonLast;			// 계절에 마지막으로 한 일
	double g_NextSeason = 0;			// 다음에 계절을 볼 시각
	NlCore::SeasonHold g_Hold;			// 붙들기가 기억한 것
	int g_HoldWrites = 0;				// 붙들기가 쓴 횟수(로그를 드문드문 남긴다)
	bool g_SeasonCallsLogged = false;	// 읽기 함수를 부른다는 것을 로그에 한 번 남겼다
	std::unordered_map<std::string, std::string> g_SeasonCaptions;		// extreme_season.<이름> -> 화면 이름

	NlCore::StockBook g_Mines;					// 매장량 붙들기가 광산마다 기억한 수와 그것의 자리(core/WorldPlan)
	int g_MineWrites = 0;						// 되돌려 쓴 횟수
	bool g_HoldWasOn = false, g_MineWasOn = false;		// 지난 틱에 켜져 있었는가(끈 틱에 한 번 정리한다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
	}

	// 구조체의 칸 가운데 0 보다 큰 수에 0 을 쓴다. 불리언인 칸은 수로 치지 않는다(true 를 false 로 덮어쓰지 않게).
	NlCore::ClearResult ClearNumbers(const std::string& Path)
	{
		NlCore::ClearResult out;
		RValue box;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		if (!NlAccess::Open(NlCore::ParseAskPath(Path), box, kind, why) || kind != Holder::Struct)
		{
			Log("world: cannot open " + Path + ": " + (why.empty() ? "not a struct" : why));
			return out;
		}
		std::vector<NlCore::PathStep> steps;		// 도는 동안에는 쓰지 않는다
		const double children = NlAccess::ForEachChild(box, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			const bool number = NlGame::IsRealNumber(child);
			if (NlCore::ShouldClearCooldown(number, number ? child.ToDouble() : 0))
				steps.push_back(step);
			return true;
		});
		if (children < 0)
		{
			Log("world: cannot list " + Path);
			return out;
		}
		out.Opened = true;
		for (const NlCore::PathStep& step : steps)
		{
			if (NlAccess::SetNumber(box, step, 0, why))		// 쓴 뒤 다시 읽어 확인한다
				out.Cleared++;
			else
			{
				out.Failed++;
				Log("world: cannot clear " + Path + "." + step.Name + ": " + why);
			}
		}
		return out;
	}

	// 인자 없는 함수를 불러 수(불리언 포함)를 받는다.
	bool CallNumber(const std::string& Path, double& Out, std::string& Why)
	{
		RValue result;		// 이 함수 안에서만 든다
		if (!CallNoArgs(Path, result, Why))
			return false;
		if (!NlGame::IsNumber(result))
		{
			Why = "not a number";
			return false;
		}
		Out = result.ToDouble();
		return true;
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

	// 계절을 읽는다. 가혹한 계절이 아닐 때는 "올 때까지"(게임이 그때 부르는 것을 봤다), 가혹한 계절일 때는 "끝날 때까지"를 묻는다
	// (뒤의 것은 게임이 부르는 것을 보지 못했다. 가혹한 계절이 아닐 때와 가혹한 계절일 때 직접 불러 수를 받았다. research/25).
	Season ReadSeason()
	{
		Season s;
		if (!NlAccess::InGame())
		{
			s.Why = "게임 화면이 아닙니다";
			return s;
		}
		const std::string base = k_Season;
		if (!NlAccess::ReadNumber(k_GameTime, s.Now) || !NlAccess::ReadNumber(base + ".__start_phase_time", s.Start) || !NlAccess::ReadNumber(base + ".__current_phase", s.Phase))
		{
			s.Why = "계절의 자료를 읽지 못했습니다";
			return s;
		}
		if (!g_SeasonCallsLogged)		// 1초마다 부르는 읽기 함수다. 로그에는 한 번만 남긴다
		{
			g_SeasonCallsLogged = true;
			Log("world reads call is_extreme(), __get_remain_time_of_current_phase() and get_remain_time_to_extreme_season() | get_remain_time_to_end_of_extreme_season() on the season manager");
		}
		double extreme = 0;
		std::string why;
		if (!CallNumber(base + ".is_extreme", extreme, why) || !CallNumber(base + ".__get_remain_time_of_current_phase", s.Remain, why))
		{
			s.Why = "계절의 함수를 부르지 못했습니다: " + why;
			return s;
		}
		s.Extreme = extreme != 0;
		if (!CallNumber(base + (s.Extreme ? ".get_remain_time_to_end_of_extreme_season" : ".get_remain_time_to_extreme_season"), s.Extreme ? s.ToEnd : s.ToExtreme, why))
		{
			s.Why = "계절의 함수를 부르지 못했습니다: " + why;
			return s;
		}
		std::string key;
		if (NlAccess::ReadText(base + ".__extreme_season.__caption", key))
		{
			const auto found = g_SeasonCaptions.find(key);
			if (found != g_SeasonCaptions.end())
				s.Name = found->second;
		}
		if (s.Name.empty())
			NlAccess::ReadText(base + ".__extreme_season.__name", s.Name);		// 화면 이름을 못 읽었으면 게임의 이름
		s.Read = true;
		return s;
	}

	std::string SeasonText(const Season& S)
	{
		if (!S.Read)
			return S.Why;
		return NlCore::SeasonLine(S.Extreme, S.Name, S.ToExtreme, S.ToEnd) + " (" + NlCore::PhaseNote(S.Phase, S.Remain) + ")";
	}

	// 미루기와 끝내기: 시작 시각에 쓰고, 게임의 함수가 돌려주는 남은 시간이 따라 움직였는지 본다.
	std::string ChangeSeason(WorldAct Act)
	{
		const Season before = ReadSeason();
		if (!before.Read)
			return before.Why;
		const bool delay = Act == WorldAct::SeasonDelay;
		double write = 0;
		if (delay)
		{
			if (before.Extreme)
				return "가혹한 계절 중에는 미루지 않습니다(미루면 이 계절이 길어집니다). '지금 단계 끝내기'를 쓰세요";
			if (!NlCore::DelaySeasonStart(before.Now, before.Start, k_DelayStep, write))
				return "더 미룰 수 없습니다: 이 단계가 방금 시작한 것으로 돼 있습니다";
		}
		else
		{
			if (NlCheats::IsOn(k_HoldCheat))
				return "'계절 붙들기'를 켠 동안에는 단계를 끝내지 않습니다. 끄고 누르세요";
			// 단계의 길이: 남은 시간 + 지나간 시간(게임이 남은 시간을 그렇게 셈한다)
			const double duration = before.Remain + (before.Now - before.Start);
			if (NlCore::PhaseEndTooEarly(before.Now, duration, k_EndLead))
				return "게임을 시작한 지 이 단계의 길이만큼 지나지 않아 끝낼 수 없습니다(0 보다 앞의 시작 시각은 쓰지 않습니다)";
			if (!NlCore::EndPhaseStart(before.Now, before.Start, duration, k_EndLead, write))
				return "이 단계는 곧 끝납니다 (" + NlCore::SpanText(before.Remain) + " 남음)";
		}
		const std::string path = std::string(k_Season) + ".__start_phase_time";
		std::string why;
		Log("world write " + path + " " + NlCore::Shortest(before.Start) + " -> " + NlCore::Shortest(write) + " (" + NlCore::WorldActWord(Act) + ")");		// 쓰기 전에 남긴다
		if (!NlAccess::WriteNumber(path, write, why))		// 쓴 뒤 다시 읽어 확인한다
			return "계절의 시작 시각을 쓰지 못했습니다: " + why;
		NlCore::ForgetSeasonHold(g_Hold);		// 붙들기는 새 값을 다시 기억한다
		const Season after = ReadSeason();
		g_Season = after;
		if (!after.Read)
			return "썼지만 다시 읽지 못했습니다: " + after.Why;
		if (!NlCore::RemainMoved(delay, before.Remain, after.Remain))
			return "시작 시각은 썼지만 게임이 돌려주는 남은 시간이 바뀌지 않았습니다";
		return (delay ? NlCore::DelayReport(write - before.Start, k_DelayStep) + " " : std::string("지금 단계의 남은 시간을 1분으로 줄였습니다. ")) + SeasonText(after);
	}

	// 붙들기(치트 표의 season_hold): 1초마다 시작 시각을 흐른 만큼 따라 민다. 판단은 core/SeasonPlan.
	void HoldTick(bool On)
	{
		const std::string path = std::string(k_Season) + ".__start_phase_time";
		double now = 0, start = 0, phase = 0, write = 0;
		NlCore::PlaceKey place;		// 다른 세이브나 다른 지도의 관리자면 앞에서 기억한 것으로 쓰지 않는다(StepSeasonHold 가 다시 기억한다)
		const bool in_game = On && NlAccess::InGame();
		const bool read = in_game && NlAccess::ReadNumber(k_GameTime, now) && NlAccess::ReadNumber(path, start) && NlAccess::ReadNumber(std::string(k_Season) + ".__current_phase", phase)
			&& PlaceOf(k_Season, place);
		const NlCore::SeasonHold was = g_Hold;
		if (On && !read)
		{
			NlCore::ForgetSeasonHold(g_Hold);
			NlCheats::SetNote(k_HoldCheat, in_game ? "계절의 자료를 읽지 못했습니다" : std::string());
			return;
		}
		if (!NlCore::StepSeasonHold(g_Hold, On, now, start, phase, place, write))
		{
			if (!On)		// 끈 틱에 한 번 온다(g_HoldWasOn)
			{
				Log("world: season hold off after " + std::to_string(g_HoldWrites) + " write(s)");
				NlCheats::SetNote(k_HoldCheat, std::string());
				g_HoldWrites = 0;
			}
			else if (!was.Has || was.Phase != g_Hold.Phase || was.Elapsed != g_Hold.Elapsed)
			{
				Log("world: season hold keeps phase " + NlCore::Shortest(g_Hold.Phase) + " at " + NlCore::Shortest(g_Hold.Elapsed) + " s elapsed");
				NlCheats::SetNote(k_HoldCheat, "단계 " + std::to_string(static_cast<long long>(g_Hold.Phase) + 1) + " 에 머무는 중");
			}
			return;
		}
		std::string why;
		if (g_HoldWrites % 60 == 0)		// 첫 번째와 그 뒤로 60번마다 남긴다
			Log("world: season hold writes " + path + " " + NlCore::Shortest(start) + " -> " + NlCore::Shortest(write) + " (write " + std::to_string(g_HoldWrites + 1) + ")");
		if (!NlAccess::WriteNumber(path, write, why))
		{
			NlCheats::SetNote(k_HoldCheat, "쓰지 못했습니다: " + why);
			return;
		}
		g_HoldWrites++;
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

	// 이벤트 골라 일으키기: 감독의 자료의 __events_by_name(ds_map: 이름 -> 이벤트 구조체)에서 그 구조체를 얻어 __debug_forced_event 에 쓴다.
	// 게임의 감독은 하루 한 번 이벤트를 뽑을 때 get_debug_forced_event() 를 읽고 reset_debug_forced_event() 로 지운다(research/28 의 기록).
	// 구조체를 쓰는 것이 맞다(research/28: 감독의 결정 함수가 그 구조체를 돌려주고 그 이벤트가 쿨다운에 올랐다).
	std::string ForceEventNow(const std::string& Name)
	{
		double id = 0;
		if (!NlAccess::ReadNumber(std::string(k_EventsData) + ".__events_by_name", id))
			return NlCore::ForceEventReport(Name, 'w', "이벤트 목록(ds_map)의 번호를 읽지 못했습니다");
		RValue event;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath("map:" + NlCore::Shortest(id) + "@" + Name), event, why) || !event.IsStruct())
			return NlCore::ForceEventReport(Name, 'n', std::string());
		Log("world: forced event: writing the struct of " + Name + " to " + std::string(k_EventsData) + ".__debug_forced_event");		// 쓰기 전에 남긴다
		if (!NlAccess::Write(NlCore::ParseAskPath(std::string(k_EventsData) + ".__debug_forced_event"), event, why))
			return NlCore::ForceEventReport(Name, 'w', why);
		return NlCore::ForceEventReport(Name, 'd', std::string());
	}

	// 이벤트의 이름들(ds_map 의 열쇠)을 한 번 읽는다. 못 읽으면 다음 틱에 다시.
	void ReadEventNames()
	{
		double id = 0;
		RValue keys;		// 이 함수 안에서만 든다
		if (!NlAccess::ReadNumber(std::string(k_EventsData) + ".__events_by_name", id) || !NlGame::Call("ds_map_keys_to_array", { RValue(id) }, keys) || !keys.IsArray())
			return;
		std::vector<std::string> names;
		NlAccess::ForEachChild(keys, Holder::Array, [&](const NlCore::PathStep&, const RValue& key) {
			if (key.IsString())
				names.push_back(key.ToString());
			return true;
		});
		std::sort(names.begin(), names.end());
		g_EventNames = std::move(names);
		Log("world: event names: " + std::to_string(g_EventNames.size()));
	}

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
		case WorldAct::EventForce:
			return ForceEventNow(g_PendingEvent);
		case WorldAct::CooldownsClear:
		{
			const NlCore::ClearResult events = ClearNumbers(std::string(k_Director) + ".__events_cooldowns");
			const NlCore::ClearResult groups = ClearNumbers(std::string(k_Director) + ".__events_groups_cooldowns");
			if (NlCore::CooldownTouched(events, groups))		// 한쪽만 됐어도 건드린 것은 남긴다
				Log("world: event cooldowns: events cleared " + std::to_string(events.Cleared) + " failed " + std::to_string(events.Failed)
					+ ", groups cleared " + std::to_string(groups.Cleared) + " failed " + std::to_string(groups.Failed));
			return NlCore::CooldownReport(events, groups);
		}
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
		case WorldAct::SeasonShow:
			g_Season = ReadSeason();
			return SeasonText(g_Season);
		case WorldAct::SeasonDelay:
		case WorldAct::SeasonEnd:
			return ChangeSeason(Act);
		}
		return std::string();
	}

	bool IsSeasonAct(WorldAct Act)
	{
		return Act == WorldAct::SeasonShow || Act == WorldAct::SeasonDelay || Act == WorldAct::SeasonEnd;
	}

	// 한 일의 글을 그 패널의 자리에 둔다(계절의 것은 월드 패널에, 지금 저장은 유틸 패널에, 나머지는 이벤트·종교 패널에).
	// 읽기만 한 것(계절 보기)은 "한 일"로 남기지 않는다: 패널이 같은 줄을 이미 보이고 있다.
	std::string& LastOf(WorldAct Act)
	{
		return IsSeasonAct(Act) ? g_SeasonLast : Act == WorldAct::SaveNow ? g_UtilLast : g_Last;
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

void NlWorld::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	// 가혹한 계절의 화면 이름. 게임의 글은 레포에 싣지 않는다: 게임 폴더의 파일에서 읽는다(한 줄짜리 짧은 글만 받는다).
	std::ifstream in(GameDir / "localization" / "main.csv", std::ios::binary);
	if (in)
	{
		const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		std::unordered_map<std::string, std::string> rows;
		std::string why;
		if (NlCore::ReadLocalization(text, k_SeasonCaptionPrefix, { "Korean", "English" }, rows, why))
			for (auto& [key, value] : rows)
				if (!value.empty() && value.size() <= 48 && value.find_first_of("\r\n<{") == std::string::npos)
					g_SeasonCaptions.emplace(key, std::move(value));
	}
	Log("world: " + std::to_string(g_SeasonCaptions.size()) + " season caption(s) from the game's localization file");
}

void NlWorld::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const bool names = g_EventNames.empty() && Now >= g_NextNames;
	if (g_Queue.empty() && Now < g_NextSeason && !names)		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다). 치트 표를 읽는 것도 그 뒤에
		return;
	const NlCore::ScopedFlag busy(g_Busy);
	if (names)
	{
		g_NextNames = Now + 5;
		if (NlAccess::InGame())
			ReadEventNames();
	}
	if (!g_Queue.empty())
	{
		const WorldAct act = g_Queue.front();
		g_Queue.pop_front();
		Remember(act, DoNow(act));
	}
	if (Now < g_NextSeason)
		return;
	g_NextSeason = Now + 1;
	const bool hold = NlCheats::IsOn(k_HoldCheat);
	if (hold || g_HoldWasOn)
		HoldTick(hold);
	g_HoldWasOn = hold;
	const bool mines = NlCheats::IsOn(k_MineCheat);
	if (mines || g_MineWasOn)
		MineTick(mines);
	g_MineWasOn = mines;
	if (Visible)
		g_Season = ReadSeason();
	else
		g_Season = Season{};		// 패널을 다시 열면 새로 읽은 것을 보인다
}

void NlWorld::DrawEvents()
{
	std::lock_guard lock(g_Mutex);
	// 이벤트 골라 일으키기(2026-10-07. research/28): 이름을 고르고 누르면 틱이 그 이벤트의 구조체를 감독의 강제 이벤트에 쓴다.
	ImGui::SeparatorText("이벤트 골라 일으키기");
	if (g_EventNames.empty())
		NlUi::Hint("게임 화면에서 이벤트의 이름을 읽습니다.");
	else
	{
		if (g_EventPick < 0 || static_cast<size_t>(g_EventPick) >= g_EventNames.size())
			g_EventPick = 0;
		ImGui::SetNextItemWidth(320);
		if (ImGui::BeginCombo("##event", g_EventNames[g_EventPick].c_str()))
		{
			for (size_t i = 0; i < g_EventNames.size(); i++)
				if (ImGui::Selectable(g_EventNames[i].c_str(), static_cast<int>(i) == g_EventPick))
					g_EventPick = static_cast<int>(i);
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("일으키기"))
		{
			g_PendingEvent = g_EventNames[g_EventPick];
			Push(WorldAct::EventForce);
		}
	}
	NlUi::Hint("게임의 감독이 보는 '강제 이벤트' 자리에 고른 이벤트의 구조체를 써 둡니다. 감독은 하루 한 번(오후) 이벤트를 뽑을 때 그 자리를 읽어 그것을 고르고 지웁니다"
		"(research/28: 써 둔 u_guest_bard 가 그날 뽑혀 쿨다운에 올랐다). 쿨다운 중인 이벤트도 오는지는 재지 않았습니다. 습격·반란·예언 이벤트도 그대로 옵니다.");
	ImGui::SeparatorText("쿨다운");
	if (ImGui::Button("이벤트 쿨다운 지우기"))
		Push(WorldAct::CooldownsClear);
	NlUi::Hint("게임이 이벤트를 고를 때 보는 '남은 날'(이벤트마다, 묶음마다)을 0 으로 씁니다. 써지는 것까지 봤고, 이벤트가 더 일찍 오는지는 확인 전입니다. "
		"쿨다운은 세이브에 들어가는 자료입니다(세이브 파일에 그 열쇠가 있습니다). 쓴 채 저장하면 남습니다.");
	DrawLast();
}

void NlWorld::DrawWorld()
{
	std::lock_guard lock(g_Mutex);
	ImGui::SeparatorText("계절");
	if (!g_Season.Read)
		NlUi::Hint(g_Season.Why.empty() ? "계절을 읽는 중입니다" : g_Season.Why.c_str());
	else
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(SeasonText(g_Season).c_str());
		ImGui::PopTextWrapPos();
	}
	ImGui::BeginDisabled(!g_Season.Read);
	if (ImGui::Button("가혹한 계절 하루 미루기"))
		Push(WorldAct::SeasonDelay);
	ImGui::SameLine();
	if (ImGui::Button("지금 단계 끝내기"))
		Push(WorldAct::SeasonEnd);
	ImGui::EndDisabled();
	NlUi::Hint("게임은 계절을 단계로 나누고 단계마다의 시작 시각에서 남은 시간을 셈합니다. '미루기'는 지금 단계의 시작 시각을 하루 뒤로 써서 가혹한 계절까지 남은 시간을 늘립니다"
		"(지금보다 뒤로는 밀지 않습니다). '지금 단계 끝내기'는 남은 시간을 1분으로 줄입니다: 게임이 다음 정각에 다음 단계로 넘깁니다"
		"(가혹한 계절 바로 앞의 단계였다면 가혹한 계절이 시작되고, 가혹한 계절 중이었다면 끝납니다). 위의 '계절 붙들기'는 켜 둔 동안 지금 단계에 머물게 합니다. "
		"계절의 상태는 세이브에 들어가는 자료입니다. 바꾼 채 저장하면 남습니다.");
	if (!g_SeasonLast.empty())
		NlUi::Hint(g_SeasonLast.c_str());
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
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	return Remember(Act, DoNow(Act));
}

std::string NlWorld::ForceEvent(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	g_PendingEvent = Name;
	return Remember(WorldAct::EventForce, DoNow(WorldAct::EventForce));
}
