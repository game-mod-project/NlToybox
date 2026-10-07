#include "Season.hpp"

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

#include <deque>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>

using namespace YYTK;
using NlCore::WorldAct;

namespace
{
	// 계절(research/25). 지금 지도의 관리자(ExtremeSeasonManager). 남은 시간은 게임의 함수로 읽는다(인자 없음. 게임이 그 꼴로 부르는 것을 봤다).
	// 바꿀 때는 시작 시각에 쓴다: 게임은 남은 시간을 그 시각에서 셈한다(시작을 하루 뒤로 쓰자 함수의 값이 하루 늘었다).
	constexpr const char* k_Season = "inst:o_game_map_controller.__current_local_map.__season_manager";
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";
	constexpr const char* k_HoldCheat = "season_hold";				// 치트 표의 항목(core/CheatTable)
	constexpr const char* k_SeasonCaptionPrefix = "extreme_season.";	// 게임의 localization\main.csv 의 열쇠. 게임이 __extreme_season.__caption 에 그 열쇠를 든다
	constexpr double k_DelayStep = 86400;		// 미루기 한 번: 하루
	constexpr double k_EndLead = 60;			// 끝내기: 남은 시간을 1분으로

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlSeason::LogFn g_Log;
	std::deque<WorldAct> g_Queue;		// 창의 단추가 쌓고 틱이 한다(미루기, 끝내기)

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

	bool g_HoldWasOn = false;			// 지난 틱에 켜져 있었는가(끈 틱에 한 번 정리한다)
	bool g_Busy = false;				// 하는 중이다. 계절의 읽기는 게임의 함수(is_extreme 들)를 부르므로, 그것이 오브젝트 이벤트를 일으켜 틱이 다시 들어오면 안쪽은 아무것도 하지 않는다(World 와 같다)

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
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

	std::string DoNow(WorldAct Act)
	{
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		switch (Act)
		{
		case WorldAct::SeasonShow:
			g_Season = ReadSeason();
			return SeasonText(g_Season);
		case WorldAct::SeasonDelay:
		case WorldAct::SeasonEnd:
			return ChangeSeason(Act);
		default:
			return std::string();
		}
	}

	// 읽기만 한 것(계절 보기)은 "한 일"로 남기지 않는다: 패널이 같은 줄을 이미 보이고 있다.
	std::string Remember(WorldAct Act, std::string Text)
	{
		if (NlCore::WorldActChanges(Act))
			g_SeasonLast = Text;
		return Text;
	}

	constexpr size_t k_MaxQueue = 4;		// 창이 쌓아 둘 청의 수. 넘치면 받지 않고 결과 줄에 적는다(조용히 버리지 않는다)

	void Push(WorldAct Act)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			g_SeasonLast = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(Act);
	}
}

void NlSeason::Init(LogFn Log_, const std::filesystem::path& GameDir)
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

void NlSeason::Tick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	const NlCore::ScopedFlag busy(g_Busy);
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
	if (Visible)
		g_Season = ReadSeason();
	else
		g_Season = Season{};		// 패널을 다시 열면 새로 읽은 것을 보인다
}

void NlSeason::Draw()
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

std::string NlSeason::Do(NlCore::WorldAct Act)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return "busy";
	const NlCore::ScopedFlag busy(g_Busy);
	return Remember(Act, DoNow(Act));
}
