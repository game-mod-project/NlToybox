#include "common.hpp"

void RunCourtTests()
{
	Test("영주의 호감·충성: 명령, 일, 걸음", [] {
		// 충성 상태의 이름(get_loyalty_state 의 수. 평판이 오르면 0 -> 1 -> 2 로 올랐다. research/20)
		CHECK_STR(LoyaltyLabel(0), "낮음");
		CHECK_STR(LoyaltyLabel(1), "보통");
		CHECK_STR(LoyaltyLabel(2), "높음");
		CHECK_STR(LoyaltyLabel(3), "?");
		CHECK_STR(LoyaltyLabel(-1), "?");
		CHECK_STR(LoyaltyLabel(1.5), "?");
		CHECK_STR(LoyaltyLabel(std::numeric_limits<double>::quiet_NaN()), "?");

		// 낱말
		CourtGoal goal = CourtGoal::Clear;
		bool king = false;
		CHECK(ParseCourtGoal("loyal", goal, king) && goal == CourtGoal::Raise && king);
		CHECK(ParseCourtGoal("like", goal, king) && goal == CourtGoal::Raise && !king);
		CHECK(ParseCourtGoal("opinion", goal, king) && goal == CourtGoal::Opinion && !king);
		CHECK(ParseCourtGoal("clear", goal, king) && goal == CourtGoal::Clear);
		CHECK(ParseCourtGoal("release", goal, king) && goal == CourtGoal::Release);
		CHECK(!ParseCourtGoal("friends", goal, king) && !ParseCourtGoal("", goal, king));
		CHECK(GoodCourtWho("lords") && GoodCourtWho("25556c3312bce178") && !GoodCourtWho("king") && !GoodCourtWho("all") && !GoodCourtWho("25556c33"));
		CHECK(GoodCourtAbout("king") && GoodCourtAbout("lords") && GoodCourtAbout("a34ba8b605c96ab7") && !GoodCourtAbout("") && !GoodCourtAbout("people"));

		// 명령이 말이 되는가
		std::string why;
		CHECK(CheckCourt({ "lords", CourtGoal::Raise, "king", 0 }, why) && why.empty());
		CHECK(CheckCourt({ "lords", CourtGoal::Raise, "lords", 150 }, why));
		CHECK(!CheckCourt({ "lords", CourtGoal::Raise, "king", 201 }, why) && !why.empty());		// 목표는 1 ~ 200
		CHECK(!CheckCourt({ "lords", CourtGoal::Raise, "king", -5 }, why));
		CHECK(!CheckCourt({ "lords", CourtGoal::Raise, "king", 50.5 }, why));
		CHECK(!CheckCourt({ "lords", CourtGoal::Raise, "", 0 }, why));
		CHECK(CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "a34ba8b605c96ab7", -3 }, why));
		CHECK(!CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "a34ba8b605c96ab7", 0 }, why));
		CHECK(!CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "a34ba8b605c96ab7", 41 }, why));
		CHECK(!CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "25556c3312bce178", 1 }, why));		// 자기 자신
		CHECK(CheckCourt({ "lords", CourtGoal::Clear, "lords", 0 }, why) && !CheckCourt({ "lords", CourtGoal::Clear, "lords", 3 }, why));
		CHECK(CheckCourt({ "lords", CourtGoal::Release, "", 0 }, why) && !CheckCourt({ "lords", CourtGoal::Release, "king", 0 }, why));
		CHECK(!CheckCourt({ "people", CourtGoal::Clear, "lords", 0 }, why));

		// 일로 풀기: 평판을 갖는 쪽과 대상의 짝마다 하나. 자기 자신은 뺀다.
		const std::vector<CourtLord> lords = { { "aaaaaaaaaaaaaaa1", false }, { "aaaaaaaaaaaaaaa2", false }, { "aaaaaaaaaaaaaaa3", true } };
		auto jobs = PlanCourtJobs({ "lords", CourtGoal::Raise, "king", 0 }, lords);
		CHECK(jobs.size() == 2 && jobs[0].Holder == "aaaaaaaaaaaaaaa1" && jobs[0].About == "aaaaaaaaaaaaaaa3" && jobs[1].Holder == "aaaaaaaaaaaaaaa2");
		CHECK(jobs.size() == 2 && jobs[0].Target == k_CourtRaiseTo && jobs[0].Left == k_OpinionStepsMax && jobs[0].Sign == 1 && jobs[0].Goal == CourtGoal::Raise);
		jobs = PlanCourtJobs({ "lords", CourtGoal::Raise, "lords", 60 }, lords);
		CHECK(jobs.size() == 6 && jobs[0].Target == 60);
		jobs = PlanCourtJobs({ "aaaaaaaaaaaaaaa3", CourtGoal::Raise, "king", 0 }, lords);
		CHECK(jobs.empty());		// 왕이 저를 보는 평판은 없다
		jobs = PlanCourtJobs({ "aaaaaaaaaaaaaaa1", CourtGoal::Opinion, "aaaaaaaaaaaaaaa2", -7 }, lords);
		CHECK(jobs.size() == 1 && jobs[0].Left == 7 && jobs[0].Sign == -1 && jobs[0].Goal == CourtGoal::Opinion);
		jobs = PlanCourtJobs({ "aaaaaaaaaaaaaaa1", CourtGoal::Clear, "lords", 0 }, lords);
		CHECK(jobs.size() == 2 && jobs[0].Left == k_CourtClearMax);
		jobs = PlanCourtJobs({ "lords", CourtGoal::Release, "", 0 }, lords);
		CHECK(jobs.size() == 3 && jobs[0].About.empty() && jobs[0].Goal == CourtGoal::Release);
		CHECK(PlanCourtJobs({ "bbbbbbbbbbbbbbb1", CourtGoal::Clear, "lords", 0 }, lords).empty());		// 없는 영주
		CHECK(PlanCourtJobs({ "aaaaaaaaaaaaaaa1", CourtGoal::Clear, "bbbbbbbbbbbbbbb1", 0 }, lords).empty());
		CHECK(PlanCourtJobs({ "lords", CourtGoal::Opinion, "lords", 0 }, lords).empty());			// 말이 안 되는 명령
		CHECK(PlanCourtJobs({ "lords", CourtGoal::Raise, "king", 0 }, { { "aaaaaaaaaaaaaaa1", false } }).empty());		// 왕이 없다

		// 충성 올리기(loyal)는 게임이 충성을 따지는 영주에게만 간다(아이와 왕은 뺀다. research/20). 왕을 보는 평판을 올리는 like about=king 은 가리지 않는다.
		const std::vector<CourtLord> court = { { "aaaaaaaaaaaaaaa1", false, true }, { "aaaaaaaaaaaaaaa2", false, false }, { "aaaaaaaaaaaaaaa3", true, false } };
		CourtCommand loyal_all{ "lords", CourtGoal::Raise, "king", 0 };
		loyal_all.OnlyLoyal = true;
		CHECK(CheckCourt(loyal_all, why));
		jobs = PlanCourtJobs(loyal_all, court);
		CHECK(jobs.size() == 1 && jobs[0].Holder == "aaaaaaaaaaaaaaa1" && jobs[0].About == "aaaaaaaaaaaaaaa3");
		loyal_all.Who = "aaaaaaaaaaaaaaa2";
		CHECK(PlanCourtJobs(loyal_all, court).empty());		// 충성을 따지지 않는 영주 하나
		CHECK(PlanCourtJobs({ "lords", CourtGoal::Raise, "king", 0 }, court).size() == 2);
		// OnlyLoyal 은 왕을 보는 올리기에만 쓴다
		CourtCommand odd{ "lords", CourtGoal::Clear, "king", 0 };
		odd.OnlyLoyal = true;
		CHECK(!CheckCourt(odd, why) && !why.empty());
		CourtCommand odd_about{ "lords", CourtGoal::Raise, "lords", 0 };
		odd_about.OnlyLoyal = true;
		CHECK(!CheckCourt(odd_about, why));
		// 평판의 수는 한 짝씩만 한다: 여럿을 한꺼번에 내리는 명령을 받지 않는다(여럿에게는 올리기와 떼기만)
		CHECK(!CheckCourt({ "lords", CourtGoal::Opinion, "king", 3 }, why) && !CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "lords", -3 }, why));
		CHECK(!CheckCourt({ "lords", CourtGoal::Opinion, "lords", -40 }, why) && CheckCourt({ "25556c3312bce178", CourtGoal::Opinion, "king", -3 }, why));

		// 주교(research/21): 플레이어의 영주가 아니다. "bishop"으로 가리키고, "lords"에는 들지 않는다(평판을 갖는 쪽으로도 대상으로도).
		{
			const std::vector<CourtLord> with_bishop = { { "aaaaaaaaaaaaaaa1", false, true }, { "aaaaaaaaaaaaaaa3", true, false }, { "bbbbbbbbbbbbbbb9", false, false, true } };
			CHECK(GoodCourtWho("bishop") && !GoodCourtAbout("bishop"));
			auto bishop_jobs = PlanCourtJobs({ "bishop", CourtGoal::Raise, "king", 0 }, with_bishop);
			CHECK(bishop_jobs.size() == 1 && bishop_jobs[0].Holder == "bbbbbbbbbbbbbbb9" && bishop_jobs[0].About == "aaaaaaaaaaaaaaa3");
			bishop_jobs = PlanCourtJobs({ "bishop", CourtGoal::Opinion, "king", -2 }, with_bishop);
			CHECK(bishop_jobs.size() == 1 && bishop_jobs[0].Sign == -1 && bishop_jobs[0].Left == 2);
			CHECK(PlanCourtJobs({ "bishop", CourtGoal::Clear, "king", 0 }, with_bishop).size() == 1);
			// 영주 모두: 주교는 빠진다
			CHECK(PlanCourtJobs({ "lords", CourtGoal::Raise, "lords", 0 }, with_bishop).size() == 2);
			CHECK(PlanCourtJobs({ "lords", CourtGoal::Raise, "king", 0 }, with_bishop).size() == 1);
			CHECK(PlanCourtJobs({ "lords", CourtGoal::Release, "", 0 }, with_bishop).size() == 2);
			// 주교가 없으면 일이 없다. 충성 올리기는 주교에게 가지 않는다(게임이 충성을 따지지 않는다).
			CHECK(PlanCourtJobs({ "bishop", CourtGoal::Raise, "king", 0 }, lords).empty());
			CourtCommand bishop_loyal{ "bishop", CourtGoal::Raise, "king", 0 };
			bishop_loyal.OnlyLoyal = true;
			CHECK(PlanCourtJobs(bishop_loyal, with_bishop).empty());
			// 원격 줄
			CHECK(ParseRemoteLine("court bishop like about=king").Error.empty() && ParseRemoteLine("court bishop opinion about=king amount=5").Error.empty()
				&& ParseRemoteLine("court bishop clear about=king").Error.empty());
			CHECK(!ParseRemoteLine("court bishop like about=bishop").Error.empty());
			// 주교를 uuid 로 가리켜도 평판을 갖는 쪽이 된다. 주교에서 영주 모두로도 된다.
			CHECK(PlanCourtJobs({ "bbbbbbbbbbbbbbb9", CourtGoal::Raise, "king", 0 }, with_bishop).size() == 1);
			CHECK(PlanCourtJobs({ "bishop", CourtGoal::Raise, "lords", 0 }, with_bishop).size() == 2);
			// 주교를 대상으로 삼지 않는다(uuid 로도): 창에 그것을 떼는 단추가 없다
			CHECK(PlanCourtJobs({ "lords", CourtGoal::Raise, "bbbbbbbbbbbbbbb9", 0 }, with_bishop).empty());
			CHECK(PlanCourtJobs({ "aaaaaaaaaaaaaaa1", CourtGoal::Clear, "bbbbbbbbbbbbbbb9", 0 }, with_bishop).empty());
			CHECK(PlanCourtJobs({ "bishop", CourtGoal::Raise, "bbbbbbbbbbbbbbb9", 0 }, with_bishop).empty());
			// 주교에게는 충성 올리기와 충성 대상 지우기가 없다(말이 안 되는 명령으로 거부한다)
			std::string bishop_why;
			CHECK(!CheckCourt({ "bishop", CourtGoal::Release, "", 0 }, bishop_why) && !bishop_why.empty());
			CHECK(!CheckCourt(bishop_loyal, bishop_why) && !bishop_why.empty());
			CHECK(!ParseRemoteLine("court bishop release").Error.empty() && !ParseRemoteLine("court bishop loyal").Error.empty());
		}

		// 한 걸음. 올리기: 나쁜 것이 붙어 있으면 그것부터 뗀다. 좋은 것이 겹침 한도면 멈춘다.
		CourtJob raise{ "h", "a", CourtGoal::Raise, 40, 1, 100 };
		CHECK(PlanCourtStep(raise, 100, 0, 0, 0, 50).Outcome == 'a');
		CHECK(PlanCourtStep(raise, 120, 3, 0, 3, 50).Outcome == 'd');
		CHECK(PlanCourtStep(raise, 13, 0, 0, 0, 50).Outcome == 0 && PlanCourtStep(raise, 13, 0, 0, 0, 50).Action == 'A');
		CHECK(PlanCourtStep(raise, 13, 4, 2, 0, 50).Action == 'd');
		CHECK(PlanCourtStep(raise, 13, 50, 0, 5, 50).Outcome == 's');
		raise.Left = 0;
		CHECK(PlanCourtStep(raise, 13, 0, 0, 40, 50).Outcome == 'l');
		// 읽지 못한 수로는 걷지 않는다
		raise.Left = 40;
		CHECK(PlanCourtStep(raise, std::numeric_limits<double>::quiet_NaN(), 0, 0, 0, 50).Outcome == 'f');
		CHECK(PlanCourtStep(raise, 13, -1, 0, 0, 50).Outcome == 'f' && PlanCourtStep(raise, 13, 0, -1, 0, 50).Outcome == 'f');
		CHECK(PlanCourtStep(raise, 13, 0, 0, 0, 0).Outcome == 'f' && PlanCourtStep(raise, 13, 0, 0, 0, std::numeric_limits<double>::quiet_NaN()).Outcome == 'f');
		// 평판의 수: 방향의 반대 것이 붙어 있으면 그것을 뗀다
		CourtJob down{ "h", "a", CourtGoal::Opinion, 3, -1, 0 };
		CHECK(PlanCourtStep(down, 30, 2, 0, 0, 50).Action == 'D' && PlanCourtStep(down, 30, 0, 0, 0, 50).Action == 'a');
		CHECK(PlanCourtStep(down, 30, 0, 50, 0, 50).Outcome == 's');
		CourtJob up{ "h", "a", CourtGoal::Opinion, 3, 1, 0 };
		CHECK(PlanCourtStep(up, 30, 0, 2, 0, 50).Action == 'd' && PlanCourtStep(up, 30, 0, 0, 0, 50).Action == 'A' && PlanCourtStep(up, 30, 50, 0, 0, 50).Outcome == 's');
		up.Left = 0;
		CHECK(PlanCourtStep(up, 30, 3, 0, 3, 50).Outcome == 'd');
		up.Left = 3;
		up.Sign = 0;
		CHECK(PlanCourtStep(up, 30, 0, 0, 0, 50).Outcome == 'f');
		// 떼기
		CourtJob clear{ "h", "a", CourtGoal::Clear, k_CourtClearMax, 0, 0 };
		CHECK(PlanCourtStep(clear, 30, 2, 1, 0, 50).Action == 'D' && PlanCourtStep(clear, 30, 0, 1, 2, 50).Action == 'd');
		CHECK(PlanCourtStep(clear, 30, 0, 0, 0, 50).Outcome == 'a' && PlanCourtStep(clear, 30, 0, 0, 3, 50).Outcome == 'd');
		clear.Left = 0;
		CHECK(PlanCourtStep(clear, 30, 2, 0, 120, 50).Outcome == 'l');
		// 충성 대상 지우기는 평판의 걸음이 아니다
		CHECK(PlanCourtStep({ "h", "", CourtGoal::Release, 0, 0, 0 }, 30, 0, 0, 0, 50).Outcome == 'f');

		// 한 걸음이 됐는가: 센 수가 바라는 대로 하나만 바뀌었다
		CHECK(CourtStepDone('A', 3, 0, 4, 0) && !CourtStepDone('A', 3, 0, 3, 0) && !CourtStepDone('A', 3, 0, 5, 0) && !CourtStepDone('A', 3, 0, 4, 1));
		CHECK(CourtStepDone('D', 3, 0, 2, 0) && !CourtStepDone('D', 3, 0, 3, 0));
		CHECK(CourtStepDone('a', 0, 3, 0, 4) && !CourtStepDone('a', 0, 3, 1, 4));
		CHECK(CourtStepDone('d', 0, 3, 0, 2) && !CourtStepDone('d', 0, 3, 0, 3));
		CHECK(!CourtStepDone('x', 0, 0, 0, 0) && !CourtStepDone('A', -1, 0, 0, 0) && !CourtStepDone('A', 3, 0, std::numeric_limits<double>::quiet_NaN(), 0));

		// 가짜 영주로 끝까지 돌려 본다: 평판 13, 좋은 것 하나에 +6, 나쁜 것 둘이 붙어 있다(하나에 -5).
		{
			double good = 0, bad = 2, base = 23;
			auto opinion = [&] { return base + good * 6 - bad * 5; };
			CourtJob job{ "h", "a", CourtGoal::Raise, k_OpinionStepsMax, 1, 100 };
			CourtDone done;
			char outcome = 0;
			for (int guard = 0; guard < 200 && outcome == 0; guard++)
			{
				const CourtStep step = PlanCourtStep(job, opinion(), good, bad, done.Total(), 50);
				outcome = step.Outcome;
				if (outcome != 0)
					break;
				const double g = good, b = bad;
				if (step.Action == 'A') good++;
				else if (step.Action == 'D') good--;
				else if (step.Action == 'a') bad++;
				else if (step.Action == 'd') bad--;
				CHECK(CourtStepDone(step.Action, g, b, good, bad));
				CountCourtStep(done, step.Action);
				job.Left--;
			}
			CHECK(outcome == 'd' && bad == 0 && good == 13 && opinion() == 101);		// 23 + 13 * 6 = 101
			CHECK(done.BadOff == 2 && done.GoodOn == 13 && done.GoodOff == 0 && done.BadOn == 0 && done.Total() == 15);
			CHECK_STR(CourtReport("Amold", "Daven", job, 13, 101, done, 'd', ""), "Amold -> Daven: 평판 13 -> 101 (나쁜 평판 2개 뗌, 좋은 평판 13개 붙임)");
		}
		// 한 번의 한도: 좋은 것 하나에 +1 이면 40개로는 목표에 닿지 않는다
		{
			double good = 0;
			CourtJob job{ "h", "a", CourtGoal::Raise, k_OpinionStepsMax, 1, 100 };
			CourtDone done;
			char outcome = 0;
			for (int guard = 0; guard < 200 && outcome == 0; guard++)
			{
				const CourtStep step = PlanCourtStep(job, good, good, 0, done.Total(), 50);
				outcome = step.Outcome;
				if (outcome == 0)
				{
					good++;
					CountCourtStep(done, step.Action);
					job.Left--;
				}
			}
			CHECK(outcome == 'l' && good == 40);
			CHECK_STR(CourtReport("A", "B", job, 0, 40, done, 'l', ""), "A -> B: 평판 0 -> 40 (좋은 평판 40개 붙임). 한 번의 한도까지 했지만 목표(100)에 닿지 않았습니다");
		}

		// 올리기에서 나쁜 평판을 떼는 걸음이 한도를 다 쓴다: 붙인 것 없이 'l'
		{
			double bad = 45;
			CourtJob job{ "h", "a", CourtGoal::Raise, k_OpinionStepsMax, 1, 100 };
			CourtDone done;
			char outcome = 0;
			for (int guard = 0; guard < 200 && outcome == 0; guard++)
			{
				const CourtStep step = PlanCourtStep(job, -bad * 5, 0, bad, done.Total(), 50);
				outcome = step.Outcome;
				if (outcome == 0)
				{
					CHECK(step.Action == 'd');
					bad--;
					CountCourtStep(done, step.Action);
					job.Left--;
				}
			}
			CHECK(outcome == 'l' && bad == 5 && done.BadOff == 40 && done.GoodOn == 0);
			CHECK_STR(CourtReport("A", "B", job, -225, -25, done, 'l', ""), "A -> B: 평판 -225 -> -25 (나쁜 평판 40개 뗌). 한 번의 한도까지 했지만 목표(100)에 닿지 않았습니다");
		}
		// 내리기에 좋은 평판이 붙어 있다: 좋은 것 둘을 떼고 나쁜 것 셋을 붙인다
		{
			double good = 2, bad = 0;
			CourtJob job{ "h", "a", CourtGoal::Opinion, 5, -1, 0 };
			CourtDone done;
			char outcome = 0;
			for (int guard = 0; guard < 200 && outcome == 0; guard++)
			{
				const CourtStep step = PlanCourtStep(job, 10 + good * 5 - bad * 5, good, bad, done.Total(), 50);
				outcome = step.Outcome;
				if (outcome != 0)
					break;
				const double g = good, b = bad;
				if (step.Action == 'D') good--;
				else if (step.Action == 'a') bad++;
				else g_Failed++;
				CHECK(CourtStepDone(step.Action, g, b, good, bad));
				CountCourtStep(done, step.Action);
				job.Left--;
			}
			CHECK(outcome == 'd' && good == 0 && bad == 3 && done.GoodOff == 2 && done.BadOn == 3 && done.Total() == 5);
			CHECK_STR(CourtReport("A", "B", job, 20, -5, done, 'd', ""), "A -> B: 평판 20 -> -5 (좋은 평판 2개 뗌, 나쁜 평판 3개 붙임)");
		}

		// 결과의 글
		CourtDone none;
		CHECK_STR(CourtReport("A", "B", raise, 120, 120, none, 'a', ""), "A -> B: 평판 120. 이미 목표(100) 이상입니다");
		CHECK_STR(CourtReport("A", "B", clear, 30, 30, none, 'a', ""), "A -> B: 붙여 둔 디버그 평판이 없습니다 (평판 30)");
		CourtDone ten;
		ten.GoodOn = 10;
		CHECK_STR(CourtReport("A", "B", raise, 40, 90, ten, 's', ""), "A -> B: 평판 40 -> 90 (좋은 평판 10개 붙임). 같은 평판의 겹침 한도에 닿아 더 붙일 수 없습니다");
		CHECK_STR(CourtReport("A", "B", raise, 40, 40, none, 's', ""), "A -> B: 평판 40. 같은 평판의 겹침 한도에 닿아 더 붙일 수 없습니다");
		CHECK_STR(CourtReport("A", "B", raise, 40, 50, ten, 'f', "did not stick"), "A -> B: 하지 못했습니다 (did not stick). 그 전까지: 평판 40 -> 50 (좋은 평판 10개 붙임)");
		CHECK_STR(CourtReport("A", "B", raise, 40, 40, none, 'f', "그 영주를 찾지 못했습니다"), "A -> B: 하지 못했습니다 (그 영주를 찾지 못했습니다)");
		CourtDone off;
		off.GoodOff = 3;
		off.BadOff = 1;
		CHECK_STR(CourtReport("A", "B", clear, 40, 30, off, 'd', ""), "A -> B: 평판 40 -> 30 (좋은 평판 3개 뗌, 나쁜 평판 1개 뗌)");
		CourtDone bad_on;
		bad_on.BadOn = 2;
		CHECK_STR(CourtReport("A", "B", down, 40, 30, bad_on, 'd', ""), "A -> B: 평판 40 -> 30 (나쁜 평판 2개 붙임)");
		// 실패로 세는 것
		CHECK(JobFailed('l') && JobFailed('s') && JobFailed('f') && !JobFailed('d') && !JobFailed('a'));

		// 충성 대상 지우기
		CHECK(ReleaseOutcome(0, 0) == 'a' && ReleaseOutcome(2, 2) == 'd' && ReleaseOutcome(2, 1) == 'f' && ReleaseOutcome(2, 0) == 'f');
		CHECK_STR(ReleaseReport("Barra", 0, 0, ""), "Barra: 따르는 사람이 없습니다");
		CHECK_STR(ReleaseReport("Barra", 2, 2, ""), "Barra: 따르던 2명의 충성 대상을 지웠습니다");
		CHECK_STR(ReleaseReport("Barra", 2, 1, "did not stick"), "Barra: 따르던 2명 가운데 1명의 충성 대상만 지웠습니다 (did not stick)");

		// 원격 명령
		const RemoteCommand list = ParseRemoteLine("court list");
		CHECK(list.Error.empty() && list.Verb == "court" && list.Target == "list");
		const RemoteCommand loyal = ParseRemoteLine("court lords loyal");
		CHECK(loyal.Error.empty() && loyal.Target == "lords" && loyal.Options.at("act") == "loyal" && loyal.Number == 0);
		const RemoteCommand loyal_to = ParseRemoteLine("court 25556c3312bce178 loyal goal=60");
		CHECK(loyal_to.Error.empty() && loyal_to.Number == 60);
		const RemoteCommand like = ParseRemoteLine("court lords like about=lords queue=1");
		CHECK(like.Error.empty() && like.Options.at("about") == "lords" && like.Options.at("queue") == "1");
		const RemoteCommand op = ParseRemoteLine("court 25556c3312bce178 opinion about=a34ba8b605c96ab7 amount=-3");
		CHECK(op.Error.empty() && op.Number == -3 && op.Options.at("about") == "a34ba8b605c96ab7");
		CHECK(ParseRemoteLine("court lords clear about=king").Error.empty() && ParseRemoteLine("court lords release").Error.empty());
		for (const char* bad : { "court", "court lords", "court people loyal", "court lords loyal about=king", "court lords like", "court lords like about=people",
			"court lords like about=lords goal=0", "court lords like about=lords goal=201", "court lords opinion about=king", "court lords opinion about=king amount=0",
			"court lords opinion about=king amount=41", "court lords clear", "court lords clear about=king amount=2", "court lords release about=king",
			"court lords loyal sdie=1", "court lords loyal queue=2", "court list now", "court 25556c3312bce178 opinion about=25556c3312bce178 amount=1",
			"court lords opinion about=king amount=3", "court 25556c3312bce178 opinion about=lords amount=-3", "court lords opinion about=lords amount=-40" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
	});
}
