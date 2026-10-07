#include "common.hpp"

void RunDiplomacyTests()
{
	Test("외교: 관계의 종류와 왕국의 이름", [] {
		// FactionsAlliesMatrix.relationship_to_string 이 돌려준 이름(research/19): 0 allies … 7 opponent
		CHECK_STR(RelationLabel(0), "동맹");
		CHECK_STR(RelationLabel(1), "적");
		CHECK_STR(RelationLabel(2), "철천지원수");
		CHECK_STR(RelationLabel(3), "중립");
		CHECK_STR(RelationLabel(4), "우호");
		CHECK_STR(RelationLabel(5), "봉신");
		CHECK_STR(RelationLabel(6), "주군");
		CHECK_STR(RelationLabel(7), "대립");
		CHECK_STR(RelationLabel(8), "?");
		CHECK_STR(RelationLabel(-1), "?");
		CHECK_STR(RelationLabel(3.5), "?");
		CHECK_STR(RelationLabel(std::numeric_limits<double>::quiet_NaN()), "?");
		// 왕국은 이름이 faction.new.name.<수> 인 세력이다. 도적, 상인, 교단, 플레이어의 꾸러미는 아니다.
		CHECK(IsKingdom("faction.new.name.22") && IsKingdom("faction.new.name.1"));
		CHECK(!IsKingdom("player") && !IsKingdom("forest_bandits") && !IsKingdom("faction.new.name.") && !IsKingdom("faction.new.name.x")
			&& !IsKingdom("") && !IsKingdom("xfaction.new.name.3") && !IsKingdom("faction.new.name.3 "));
	});

	Test("외교: 바라는 관계로 가는 한 걸음", [] {
		// 우호: 우호가 될 때까지 올린다
		CHECK(StepToward(7, DiplomacyGoal::Friends) == 1 && StepToward(3, DiplomacyGoal::Friends) == 1 && StepToward(2, DiplomacyGoal::Friends) == 1);
		CHECK(StepToward(4, DiplomacyGoal::Friends) == 0);
		// 중립: 나쁜 관계면 올리고 우호면 내린다
		CHECK(StepToward(1, DiplomacyGoal::Neutral) == 1 && StepToward(2, DiplomacyGoal::Neutral) == 1 && StepToward(7, DiplomacyGoal::Neutral) == 1);
		CHECK(StepToward(4, DiplomacyGoal::Neutral) == -1 && StepToward(3, DiplomacyGoal::Neutral) == 0);
		// 적대: 철천지원수가 될 때까지 내린다(is_enemy_with 가 참이 되는 관계)
		CHECK(StepToward(3, DiplomacyGoal::Hostile) == -1 && StepToward(4, DiplomacyGoal::Hostile) == -1 && StepToward(7, DiplomacyGoal::Hostile) == -1
			&& StepToward(1, DiplomacyGoal::Hostile) == -1);
		CHECK(StepToward(2, DiplomacyGoal::Hostile) == 0);
		// 동맹·봉신·주군과 모르는 수는 건드리지 않는다
		for (const DiplomacyGoal goal : { DiplomacyGoal::Friends, DiplomacyGoal::Neutral, DiplomacyGoal::Hostile })
			CHECK(StepToward(0, goal) == 2 && StepToward(5, goal) == 2 && StepToward(6, goal) == 2 && StepToward(8, goal) == 2 && StepToward(-1, goal) == 2
				&& StepToward(3.5, goal) == 2 && StepToward(std::numeric_limits<double>::quiet_NaN(), goal) == 2);
		CHECK(StepToward(3, DiplomacyGoal::Opinion) == 2);		// 평판의 수는 관계를 보지 않는다

		// 평판의 수: 붙일 디버그 평판의 개수다(부호가 방향). 한 왕에게 하나가 실제로 얼마를 움직이는지는 왕마다 달랐다(검토의 지적: 수치를 약속하지 않는다).
		// 0 이 아닌 정수, 한 번에 40개까지. 그 밖은 0(받지 않는다. 큰 수를 한도로 당겨 "했다"고 적지 않는다).
		CHECK(OpinionSteps(1) == 1 && OpinionSteps(-1) == -1 && OpinionSteps(40) == 40 && OpinionSteps(-40) == -40);
		CHECK(OpinionSteps(41) == 0 && OpinionSteps(-41) == 0 && OpinionSteps(1000) == 0 && OpinionSteps(0) == 0 && OpinionSteps(2.5) == 0);
		CHECK(OpinionSteps(std::numeric_limits<double>::quiet_NaN()) == 0 && OpinionSteps(std::numeric_limits<double>::infinity()) == 0);
		CHECK(k_OpinionUnit == 5 && k_OpinionStepsMax < k_OpinionStackLimit && k_OpinionStackLimit == 50);		// 그 평판은 50겹까지다
	});

	Test("외교: 한 걸음의 판단과 한도", [] {
		// (지금의 관계, 목표, 방향, 더 붙여도 되는 수, 붙인 수)
		DiplomacyStep step = PlanStep(7, DiplomacyGoal::Friends, 0, 40, 0);
		CHECK(step.Outcome == 0 && step.Direction == 1);
		CHECK(PlanStep(4, DiplomacyGoal::Friends, 0, 40, 0).Outcome == 'a');		// 처음부터 그 관계였다
		CHECK(PlanStep(4, DiplomacyGoal::Friends, 0, 33, 7).Outcome == 'd');		// 붙여서 됐다
		CHECK(PlanStep(4, DiplomacyGoal::Friends, 0, 0, 40).Outcome == 'd');		// 마지막으로 허락된 걸음에 닿았다: 한도가 아니라 됐다
		CHECK(PlanStep(3, DiplomacyGoal::Friends, 0, 0, 40).Outcome == 'l');		// 한도까지 붙였지만 안 됐다
		CHECK(PlanStep(3, DiplomacyGoal::Friends, 0, -1, 40).Outcome == 'l');		// 음수인 한도도 한도다
		CHECK(PlanStep(5, DiplomacyGoal::Neutral, 0, 40, 0).Outcome == 'k');
		step = PlanStep(4, DiplomacyGoal::Hostile, 0, 40, 0);
		CHECK(step.Outcome == 0 && step.Direction == -1);
		// 평판의 수: 정한 만큼만. 동맹·봉신·주군·모르는 관계에는 붙이지 않는다(목표가 있는 일과 같다).
		step = PlanStep(3, DiplomacyGoal::Opinion, -1, 2, 0);
		CHECK(step.Outcome == 0 && step.Direction == -1);
		CHECK(PlanStep(3, DiplomacyGoal::Opinion, -1, 0, -2).Outcome == 'd');
		CHECK(PlanStep(0, DiplomacyGoal::Opinion, 1, 2, 0).Outcome == 'k' && PlanStep(6, DiplomacyGoal::Opinion, 1, 2, 0).Outcome == 'k'
			&& PlanStep(8, DiplomacyGoal::Opinion, 1, 2, 0).Outcome == 'k');
		CHECK(PlanStep(3, DiplomacyGoal::Opinion, 0, 2, 0).Outcome == 'k');			// 방향이 없다
		CHECK(PlanStep(3, DiplomacyGoal::Pact, 0, 40, 0).Outcome == 'k');			// 협정은 걸음이 아니다
		// 끝나는가: 가짜 관계표로 돌려 본다. 한 걸음에 관계가 어떻게 뛰든 0 을 돌려준 걸음마다 Left 가 줄어 한도 안에 끝난다.
		for (const DiplomacyGoal goal : { DiplomacyGoal::Friends, DiplomacyGoal::Neutral, DiplomacyGoal::Hostile })
			for (int start = -60; start <= 60; start += 7)
			{
				int opinion = start, left = k_OpinionStepsMax, done = 0, steps = 0;
				const auto kind = [](int value) { return value >= 25 ? 4 : value <= -45 ? 2 : value <= -20 ? 1 : value < 0 ? 7 : 3; };
				DiplomacyStep now = PlanStep(kind(opinion), goal, 0, left, done);
				while (now.Outcome == 0 && steps < 1000)
				{
					opinion += now.Direction * 31;		// 중립을 건너뛸 만큼 크게 움직이는 왕
					left--;
					done += now.Direction;
					steps++;
					now = PlanStep(kind(opinion), goal, 0, left, done);
				}
				CHECK(steps <= k_OpinionStepsMax && now.Outcome != 0);
			}
	});

	Test("외교: 건드려도 되는 왕국인가를 물은 결과", [] {
		// (is_destroyed 를 불러 불리언을 받았는가, 그 답, 왕국의 왕을 물어 답을 받았는가, 왕이 구조체인가, 우리 왕을 물어 답을 받았는가, 우리 왕이 구조체인가)
		CHECK(AliveOutcome(true, false, true, true, true, true) == 0);			// 살아 있고 양쪽에 왕이 있다
		CHECK(AliveOutcome(true, true, false, false, false, false) == 'x');		// 망했다: 더 묻지 않는다
		CHECK(AliveOutcome(true, false, true, false, true, true) == 'x');		// 그 왕국에 왕이 없다
		// 묻지 못한 것은 "망했다"가 아니라 실패다(둘째 검토의 지적: 게임이 갱신돼 함수가 없어지면 스물네 왕국이 모두 망한 것으로 읽혔다).
		CHECK(AliveOutcome(false, false, false, false, false, false) == 'f');
		CHECK(AliveOutcome(true, false, false, false, true, true) == 'f');
		CHECK(AliveOutcome(true, false, true, true, false, false) == 'f');
		CHECK(AliveOutcome(true, false, true, true, true, false) == 'f');		// 우리 쪽에 왕이 없다: 그 왕국의 탓이 아니다
	});

	Test("외교: 명령을 일들로 푼다", [] {
		const std::vector<std::string> kingdoms = { "1fa50db321ce450b", "0d8e3a894f258750", "b113a12eef97ea23" };
		std::vector<DiplomacyJob> jobs = PlanJobs({ "all", DiplomacyGoal::Neutral, 'b', 0 }, kingdoms);
		CHECK(jobs.size() == 6 && jobs[0].Uuid == kingdoms[0] && jobs[0].Side == 't' && jobs[1].Uuid == kingdoms[0] && jobs[1].Side == 'u'
			&& jobs[0].Left == k_OpinionStepsMax && jobs[0].Sign == 0 && jobs[5].Uuid == kingdoms[2]);
		jobs = PlanJobs({ "0d8e3a894f258750", DiplomacyGoal::Friends, 't', 0 }, kingdoms);
		CHECK(jobs.size() == 1 && jobs[0].Uuid == "0d8e3a894f258750" && jobs[0].Side == 't');
		jobs = PlanJobs({ "0d8e3a894f258750", DiplomacyGoal::Opinion, 'u', -3 }, kingdoms);
		CHECK(jobs.size() == 1 && jobs[0].Side == 'u' && jobs[0].Left == 3 && jobs[0].Sign == -1);
		jobs = PlanJobs({ "b113a12eef97ea23", DiplomacyGoal::Pact, 'b', 0 }, kingdoms);
		CHECK(jobs.size() == 1 && jobs[0].Side == 't');		// 협정은 양쪽에 한 번에 쓰인다: 일 하나
		CHECK(PlanJobs({ "0000000000000000", DiplomacyGoal::Friends, 'b', 0 }, kingdoms).empty());		// 없는 왕국
		CHECK(PlanJobs({ "all", DiplomacyGoal::Hostile, 'b', 0 }, kingdoms).empty());					// 말이 안 되는 명령은 일을 내지 않는다
		CHECK(PlanJobs({ "0d8e3a894f258750", DiplomacyGoal::Opinion, 't', 41 }, kingdoms).empty());
	});

	Test("외교: 결과를 세고 실패한 줄을 앞에 둔다", [] {
		JobTally tally;
		CHECK(tally.Empty() && tally.Asked() == 0 && tally.Lines().empty());
		tally.Expect(5);
		tally.Add('d', "A 바꿈");
		tally.Add('a', "B 그대로");
		tally.Add('f', "C 실패");
		tally.Add('k', "D 안 건드림");
		tally.Add('l', "E 한도");
		CHECK(!tally.Empty() && tally.Asked() == 5 && tally.Changed() == 1 && tally.Same() == 2 && tally.Failed() == 2 && tally.Pending() == 0);
		// 검토의 지적: 창은 앞의 몇 줄만 보인다. 실패한 줄이 앞에 와야 "안 된 것"을 읽을 수 있다. 실패끼리, 나머지끼리는 한 차례대로.
		CHECK(tally.Lines() == (std::vector<std::string>{ "C 실패", "E 한도", "A 바꿈", "B 그대로", "D 안 건드림" }));
		// "이미 그 관계"와 "건드리지 않음"을 "됐다"에 넣지 않는다.
		CHECK_STR(tally.Summary(), "5개 가운데 한 것 1개, 그대로 둔 것 2개, 안 된 것 2개");
		// 일을 두 번에 걸쳐 쌓아도 더해진다. 버린 일 뒤의 실패도 실패끼리의 차례대로 앞에 온다.
		tally.Expect(2);
		tally.Drop(1, "게임 화면이 아닙니다");
		tally.Add('f', "F 실패");
		CHECK(tally.Asked() == 7 && tally.Failed() == 4 && tally.Pending() == 0);
		CHECK(tally.Lines()[2] == "하지 못하고 버린 일 1개: 게임 화면이 아닙니다" && tally.Lines()[3] == "F 실패" && tally.Lines()[4] == "A 바꿈");
		// 하지 못하고 버린 일은 실패로 센다(게임 화면을 떠났다. 검토의 지적: 버린 일이 "됐다"에 남았다).
		JobTally dropped;
		dropped.Expect(48);
		dropped.Add('d', "A 바꿈");
		dropped.Drop(47, "게임 화면이 아닙니다");
		CHECK(dropped.Failed() == 47 && dropped.Pending() == 0 && dropped.Lines()[0] == "하지 못하고 버린 일 47개: 게임 화면이 아닙니다");
		CHECK_STR(dropped.Summary(), "48개 가운데 한 것 1개, 그대로 둔 것 0개, 안 된 것 47개");
		dropped.Drop(0, "아무것도");
		CHECK(dropped.Failed() == 47 && dropped.Lines().size() == 2);
		// 아직 하는 중
		JobTally busy;
		busy.Expect(3);
		busy.Add('x', "망한 왕국");
		CHECK(busy.Pending() == 2 && busy.Same() == 1);
		CHECK_STR(busy.Summary(), "3개 가운데 한 것 0개, 그대로 둔 것 1개, 안 된 것 0개 (남은 일 2개)");
		busy.Reset();
		CHECK(busy.Empty() && busy.Lines().empty() && busy.Pending() == 0);
	});

	Test("외교: 명령의 낱말과 검사", [] {
		DiplomacyGoal goal = DiplomacyGoal::Opinion;
		CHECK(ParseDiplomacyGoal("friends", goal) && goal == DiplomacyGoal::Friends);
		CHECK(ParseDiplomacyGoal("neutral", goal) && goal == DiplomacyGoal::Neutral);
		CHECK(ParseDiplomacyGoal("hostile", goal) && goal == DiplomacyGoal::Hostile);
		CHECK(ParseDiplomacyGoal("opinion", goal) && goal == DiplomacyGoal::Opinion);
		CHECK(!ParseDiplomacyGoal("allies", goal) && !ParseDiplomacyGoal("", goal));
		CHECK_STR(DiplomacyGoalWord(DiplomacyGoal::Hostile), "hostile");
		CHECK_STR(DiplomacyGoalLabel(DiplomacyGoal::Friends), "우호");
		char side = 0;
		CHECK(ParseDiplomacySide("them", side) && side == 't' && ParseDiplomacySide("us", side) && side == 'u' && ParseDiplomacySide("both", side) && side == 'b');
		CHECK(!ParseDiplomacySide("all", side) && !ParseDiplomacySide("", side));
		CHECK(GoodFactionWho("all") && GoodFactionWho("1fa50db321ce450b"));
		CHECK(!GoodFactionWho("") && !GoodFactionWho("1fa50db321ce450") && !GoodFactionWho("1FA50DB321CE450B") && !GoodFactionWho("lords") && !GoodFactionWho("1fa50db321ce450bz"));

		std::string why;
		CHECK(CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Friends, 'b', 0 }, why) && why.empty());
		CHECK(CheckDiplomacy({ "all", DiplomacyGoal::Neutral, 't', 0 }, why));
		CHECK(CheckDiplomacy({ "all", DiplomacyGoal::Friends, 'b', 0 }, why));
		// 모든 왕국을 한꺼번에 적으로 돌리지 않는다. 평판의 수도 한 왕국씩.
		CHECK(!CheckDiplomacy({ "all", DiplomacyGoal::Hostile, 'b', 0 }, why) && !why.empty());
		CHECK(!CheckDiplomacy({ "all", DiplomacyGoal::Opinion, 'b', 25 }, why));
		CHECK(CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Hostile, 'u', 0 }, why));
		CHECK(CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Opinion, 't', -25 }, why) && CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Opinion, 't', 2 }, why));
		CHECK(!CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Opinion, 't', 0 }, why) && !why.empty());
		CHECK(!CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Opinion, 't', 41 }, why) && !CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Opinion, 't', 2.5 }, why));
		CHECK(!CheckDiplomacy({ "nobody", DiplomacyGoal::Friends, 'b', 0 }, why));
		CHECK(!CheckDiplomacy({ "1fa50db321ce450b", DiplomacyGoal::Friends, 'x', 0 }, why));

		// 결과의 글: 무엇이 어떻게 바뀌었는지, 안 된 것은 안 됐다고
		// 붙인 것은 개수로 적는다("+5 를 N번"이라 적지 않는다: 하나가 움직이는 크기가 왕마다 달랐다).
		CHECK_STR(DiplomacyReport("크래스터", 't', 7, 3, 1, 'd', ""), "크래스터: 그쪽이 우리를 대립 -> 중립 (좋은 평판 1개)");
		CHECK_STR(DiplomacyReport("크래스터", 'u', 3, 2, -11, 'd', ""), "크래스터: 우리가 그쪽을 중립 -> 철천지원수 (나쁜 평판 11개)");
		CHECK_STR(DiplomacyReport("크래스터", 't', 4, 4, 0, 'a', ""), "크래스터: 그쪽이 우리를 이미 우호입니다");
		CHECK_STR(DiplomacyReport("크래스터", 't', 5, 5, 0, 'k', ""), "크래스터: 그쪽이 우리를 봉신 관계는 건드리지 않습니다");
		CHECK_STR(DiplomacyReport("크래스터", 't', 3, 3, 40, 'l', ""), "크래스터: 그쪽이 우리를 중립 -> 중립 (좋은 평판 40개). 한도까지 붙였지만 바라는 관계가 되지 않았습니다");
		CHECK_STR(DiplomacyReport("크래스터", 'u', 3, 3, 2, 'f', "no such method"), "크래스터: 우리가 그쪽을 중립 -> 중립 (좋은 평판 2개). 실패: no such method");
		CHECK_STR(DiplomacyReport("크래스터", 't', -1, -1, 0, 'f', "관계를 읽지 못했습니다"), "크래스터: 그쪽이 우리를 ? -> ?. 실패: 관계를 읽지 못했습니다");
		CHECK_STR(DiplomacyReport("크래스터", 't', 3, 3, 0, 'x', ""), "크래스터: 망했거나 왕이 없는 왕국입니다. 건드리지 않습니다");
		CHECK(JobFailed('l') && JobFailed('f') && !JobFailed('d') && !JobFailed('a') && !JobFailed('k') && !JobFailed('x'));
		// 평판의 수(목표가 없는 일): 붙인 개수와 관계를 따로 적는다. 관계가 그대로면 그대로라고 적는다(둘째 검토의 지적: 붙인 것을 "바꿨다"고 적지 않는다).
		CHECK_STR(OpinionReport("라크리아", 'u', 3, 3, -2), "라크리아: 우리가 그쪽을 보는 평판에 나쁜 평판 2개를 붙였습니다 (관계는 중립 그대로)");
		CHECK_STR(OpinionReport("라크리아", 't', 4, 3, -1), "라크리아: 그쪽이 우리를 보는 평판에 나쁜 평판 1개를 붙였습니다 (관계는 우호 -> 중립)");
		CHECK_STR(OpinionReport("라크리아", 't', 3, 3, 3), "라크리아: 그쪽이 우리를 보는 평판에 좋은 평판 3개를 붙였습니다 (관계는 중립 그대로)");

		// 붙었는지는 반환값이 아니라 그 왕의 평판 목록(__opinion_minds)의 원소 수로 본다(research/19: 51번째부터는 구조체가 돌아와도 원소가 늘지 않는다).
		// 붙이기 바로 앞뒤의 수를 견준다(같은 걸음 안. 사이에는 붙이는 호출 하나뿐이다).
		CHECK(AttachCheck(33, 34) == 'y' && AttachCheck(0, 1) == 'y');		// 하나 늘었다: 붙었다
		CHECK(AttachCheck(83, 83) == 'n' && AttachCheck(0, 0) == 'n');		// 그대로다: 붙지 않았다
		// 줄었거나 둘 이상 늘었으면 다른 것이 끼었다: 모른다(검토의 지적: 한도로는 수가 줄지 않는다).
		CHECK(AttachCheck(83, 82) == 'u' && AttachCheck(33, 35) == 'u');
		CHECK(AttachCheck(-1, 34) == 'u' && AttachCheck(33, -1) == 'u' && AttachCheck(-1, -1) == 'u');		// 세지 못했다: 모른다
		CHECK(AttachCheck(std::numeric_limits<double>::quiet_NaN(), 5) == 'u' && AttachCheck(5, std::numeric_limits<double>::infinity()) == 'u');

		// 붙인 뒤의 판단: 'c' 붙은 것을 확인했다, 't' 확인하지 못했다(함수의 반환값을 믿고 센다), 's' 붙지 않았다(세지 않고 멈춘다).
		// "늘지 않았다"를 한도로 읽는 것은 이 일에서 느는 것을 한 번이라도 본 뒤에만이다(검토의 지적: 상대 왕의 평판의 수가 붙을 때마다
		// 느는지는 재지 않았다. 세는 길이 듣지 않는 왕에게서 되던 일을 끊지 않는다).
		CHECK(AfterAttach('y', false) == 'c' && AfterAttach('y', true) == 'c');
		CHECK(AfterAttach('n', true) == 's');
		CHECK(AfterAttach('n', false) == 't');
		CHECK(AfterAttach('u', false) == 't' && AfterAttach('u', true) == 't');
		// 가짜 목록으로 돌려 본다: 같은 평판은 한도(50)까지만 는다. Works 가 거짓이면 세는 길이 듣지 않는 왕이다(수가 늘지 않는다).
		const auto run = [](int had, int ask, bool works, int& counted, bool& unsure) {
			int list = works ? had : 7, same = had;		// same: 실제로 붙어 있는 그 평판의 수
			bool seen = false;
			counted = 0;
			unsure = false;
			for (int i = 0; i < ask; i++)
			{
				const int before = list;
				if (same < k_OpinionStackLimit)
				{
					same++;
					if (works)
						list++;
				}
				const char after = AfterAttach(AttachCheck(before, list), seen);
				if (after == 's')
					return 's';
				seen = seen || after == 'c';
				unsure = unsure || after == 't';
				counted++;
			}
			return 'd';
		};
		int counted = 0;
		bool unsure = false;
		CHECK(run(40, 15, true, counted, unsure) == 's' && counted == 10 && !unsure);		// 40개에서 15개를 청하면 10개를 세고 멈춘다
		CHECK(run(0, 40, true, counted, unsure) == 'd' && counted == 40 && !unsure);
		CHECK(run(50, 3, true, counted, unsure) == 'd' && counted == 3 && unsure);			// 처음부터 한도다: 느는 것을 본 적이 없어 가리지 못한다(반환값을 믿고 그렇게 적는다)
		CHECK(run(0, 7, false, counted, unsure) == 'd' && counted == 7 && unsure);			// 세는 길이 듣지 않는 왕: 되던 일이 끊기지 않는다
		CHECK_STR(UnsureNote(), " (붙었는지는 게임의 함수가 돌려준 값으로만 봤습니다)");

		// 한도에 닿아 멈춘 일('s')은 실패다: 청한 만큼 하지 못했다. 붙인 수는 실제로 붙은 것만 적는다. 본 것(더 붙지 않았다)을 적고 한도는 그 까닭으로 적는다.
		CHECK(JobFailed('s'));
		CHECK_STR(OpinionReport("하라우", 'u', 3, 4, 10, true),
			"하라우: 우리가 그쪽을 보는 평판에 좋은 평판 10개를 붙였고 그 뒤로는 더 붙지 않았습니다 (같은 평판의 겹침 한도 50개로 보입니다. 관계는 중립 -> 우호)");
		CHECK_STR(OpinionReport("하라우", 't', 4, 3, -2, true),
			"하라우: 그쪽이 우리를 보는 평판에 나쁜 평판 2개를 붙였고 그 뒤로는 더 붙지 않았습니다 (같은 평판의 겹침 한도 50개로 보입니다. 관계는 우호 -> 중립)");
		CHECK_STR(DiplomacyReport("하라우", 'u', 4, 3, -10, 's', ""),
			"하라우: 우리가 그쪽을 우호 -> 중립 (나쁜 평판 10개). 그 뒤로는 더 붙지 않았습니다 (같은 평판의 겹침 한도 50개로 보입니다). 바라는 관계가 되지 않았습니다");
		// 협정의 칸에 아는 비트로 설명되지 않는 것이 있는가(창이 "?"를 덧붙인다)
		CHECK(!PactUnknown(0) && !PactUnknown(-1) && !PactUnknown(4) && !PactUnknown(204));
		CHECK(PactUnknown(64) && PactUnknown(68) && PactUnknown(1) && PactUnknown(4.5) && PactUnknown(1e300));
		JobTally stuck;
		stuck.Expect(2);
		stuck.Add('d', "A 됨");
		stuck.Add('s', "B 한도");
		CHECK(stuck.Failed() == 1 && stuck.Changed() == 1 && stuck.Lines()[0] == "B 한도");

		// 원격 명령
		const RemoteCommand list = ParseRemoteLine("diplomacy list");
		CHECK(list.Error.empty() && list.Verb == "diplomacy" && list.Target == "list");
		const RemoteCommand one = ParseRemoteLine("diplomacy 1fa50db321ce450b friends side=them");
		CHECK(one.Error.empty() && one.Target == "1fa50db321ce450b" && one.Options.at("goal") == "friends" && one.Options.at("side") == "them");
		const RemoteCommand all = ParseRemoteLine("diplomacy all neutral");
		CHECK(all.Error.empty() && all.Target == "all" && all.Options.at("goal") == "neutral" && all.Options.count("side") == 0);
		const RemoteCommand opinion = ParseRemoteLine("diplomacy 1fa50db321ce450b opinion amount=-25 side=us");
		CHECK(opinion.Error.empty() && opinion.Number == -25);
		// 단추와 같은 길(쌓기)
		const RemoteCommand queued = ParseRemoteLine("diplomacy 1fa50db321ce450b friends queue=1");
		CHECK(queued.Error.empty() && queued.Options.count("queue") == 1 && queued.Options.at("goal") == "friends");
		// 모르는 열쇠는 받지 않는다(검토의 지적: sdie=them 이 조용히 양쪽을 움직였다). queue 는 1 만.
		CHECK(ParseRemoteLine("diplomacy 1fa50db321ce450b pact name=peace queue=1").Error.empty());
		for (const char* bad : { "diplomacy 1fa50db321ce450b hostile sdie=them", "diplomacy 1fa50db321ce450b friends queue=0", "diplomacy 1fa50db321ce450b friends queue=yes",
			"diplomacy 1fa50db321ce450b pact name=peace foo=1", "diplomacy all neutral side=both extra=1" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
		for (const char* bad : { "diplomacy", "diplomacy all", "diplomacy all hostile", "diplomacy all opinion amount=25", "diplomacy nobody friends",
			"diplomacy 1fa50db321ce450b allies", "diplomacy 1fa50db321ce450b friends side=sideways", "diplomacy 1fa50db321ce450b opinion",
			"diplomacy 1fa50db321ce450b opinion amount=0", "diplomacy 1fa50db321ce450b opinion amount=41", "diplomacy 1fa50db321ce450b opinion amount=2.5",
			"diplomacy 1fa50db321ce450b friends amount=5", "diplomacy 1fa50db321ce450b friends now", "diplomacy list now" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
	});

	Test("외교: 협정의 종류와 칸의 비트", [] {
		// 게임의 판정 함수가 is_has_agreement 에 넘기는 수(research/19): 평화 4, 교역 협정 8, 방어 동맹 192
		DiplomacyPact pact = DiplomacyPact::Peace;
		CHECK(ParseDiplomacyPact("peace", pact) && pact == DiplomacyPact::Peace && PactBits(pact) == 4);
		CHECK(ParseDiplomacyPact("trade", pact) && pact == DiplomacyPact::Trade && PactBits(pact) == 8);
		CHECK(ParseDiplomacyPact("defence", pact) && pact == DiplomacyPact::Defence && PactBits(pact) == 192);
		CHECK(!ParseDiplomacyPact("war", pact) && !ParseDiplomacyPact("", pact));
		CHECK_STR(DiplomacyPactWord(DiplomacyPact::Defence), "defence");
		CHECK_STR(DiplomacyPactLabel(DiplomacyPact::Peace), "평화 협정");
		CHECK_STR(DiplomacyPactLabel(DiplomacyPact::Trade), "교역 협정");
		CHECK_STR(DiplomacyPactLabel(DiplomacyPact::Defence), "방어 동맹");
		// 칸의 수에 그 협정의 비트가 모두 켜져 있어야 든 것이다. 칸이 없으면(읽지 못하면 음수) 없다.
		CHECK(HasPact(4, DiplomacyPact::Peace) && HasPact(12, DiplomacyPact::Peace) && HasPact(12, DiplomacyPact::Trade) && HasPact(196, DiplomacyPact::Defence));
		CHECK(!HasPact(0, DiplomacyPact::Peace) && !HasPact(8, DiplomacyPact::Peace) && !HasPact(64, DiplomacyPact::Defence) && !HasPact(128, DiplomacyPact::Defence));
		CHECK(!HasPact(-1, DiplomacyPact::Peace) && !HasPact(4.5, DiplomacyPact::Peace) && !HasPact(std::numeric_limits<double>::quiet_NaN(), DiplomacyPact::Peace)
			&& !HasPact(1e300, DiplomacyPact::Peace));
		// 쓸 수: 이미 든 협정을 지우지 않게 지금의 비트에 더한다. 칸이 없으면 그 협정의 비트만.
		CHECK(PactCell(0, DiplomacyPact::Peace) == 4 && PactCell(4, DiplomacyPact::Trade) == 12 && PactCell(12, DiplomacyPact::Defence) == 204);
		CHECK(PactCell(-1, DiplomacyPact::Trade) == 8 && PactCell(4, DiplomacyPact::Peace) == 4);
		CHECK(PactCell(64, DiplomacyPact::Defence) == 192 && PactCell(68, DiplomacyPact::Trade) == 76);		// 모르는 비트(반쪽)도 지우지 않는다
		// 창에 보일 글
		CHECK_STR(PactText(0), "-");
		CHECK_STR(PactText(-1), "-");
		CHECK_STR(PactText(4), "평화");
		CHECK_STR(PactText(204), "평화, 교역, 방어 동맹");
		CHECK_STR(PactText(64), "?");			// 모르는 비트뿐이다(방어 동맹의 반쪽)

		// 명령: 협정은 한 왕국씩. 양쪽에 쓰이므로 side 는 받지 않는다.
		DiplomacyGoal goal = DiplomacyGoal::Friends;
		CHECK(ParseDiplomacyGoal("pact", goal) && goal == DiplomacyGoal::Pact);
		std::string why;
		DiplomacyCommand c{ "1fa50db321ce450b", DiplomacyGoal::Pact, 'b', 0 };
		c.Pact = DiplomacyPact::Defence;
		CHECK(CheckDiplomacy(c, why) && why.empty());
		c.Who = "all";
		CHECK(!CheckDiplomacy(c, why) && !why.empty());
		CHECK(StepToward(3, DiplomacyGoal::Pact) == 2);		// 협정은 평판의 걸음이 아니다
		// 결과의 글
		CHECK_STR(PactReport("크래스터", DiplomacyPact::Peace, 'd', ""), "크래스터: 평화 협정을 맺었습니다");
		CHECK_STR(PactReport("크래스터", DiplomacyPact::Peace, 'a', ""), "크래스터: 이미 평화 협정이 있습니다");
		CHECK_STR(PactReport("크래스터", DiplomacyPact::Defence, 'f', "did not stick"), "크래스터: 방어 동맹을 맺지 못했습니다 (did not stick)");
		// 원격 명령
		const RemoteCommand pact_line = ParseRemoteLine("diplomacy 1fa50db321ce450b pact name=defence");
		CHECK(pact_line.Error.empty() && pact_line.Options.at("goal") == "pact" && pact_line.Options.at("name") == "defence");
		for (const char* bad : { "diplomacy 1fa50db321ce450b pact", "diplomacy 1fa50db321ce450b pact name=war", "diplomacy all pact name=peace",
			"diplomacy 1fa50db321ce450b pact name=peace side=them", "diplomacy 1fa50db321ce450b pact name=peace amount=5" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
	});

	Test("외교: 붙인 디버그 평판 떼기(clear)", [] {
		// 사용자 요청 2026-10-07: 붙인 디버그 평판을 하나씩 뗀다(지금까지는 반대쪽을 붙여 상쇄했다. 50개가 찬 쌍은 풀 길이 없었다).
		// 게임의 detach_opinion_about_faction(대상 세력, 평판 자료)로 좋은 자료부터, 더 떨어지지 않으면 나쁜 자료로. 뗐는지는 붙이기와 같은 눈(왕의 __opinion_minds 의 수)으로 본다.
		DiplomacyGoal goal = DiplomacyGoal::Friends;
		CHECK(ParseDiplomacyGoal("clear", goal) && goal == DiplomacyGoal::Clear);
		CHECK_STR(DiplomacyGoalWord(DiplomacyGoal::Clear), "clear");
		CHECK_STR(DiplomacyGoalLabel(DiplomacyGoal::Clear), "평판 떼기");
		std::string why;
		const std::string uuid = "0d8e3a894f258750";
		// 모든 왕국에게도 된다(붙이는 것이 아니라 떼는 것이다). 쪽은 셋 다.
		CHECK(CheckDiplomacy({ "all", DiplomacyGoal::Clear, 'b', 0 }, why) && why.empty());
		CHECK(CheckDiplomacy({ uuid, DiplomacyGoal::Clear, 't', 0 }, why) && CheckDiplomacy({ uuid, DiplomacyGoal::Clear, 'u', 0 }, why));
		CHECK(!CheckDiplomacy({ uuid, DiplomacyGoal::Clear, 'x', 0 }, why));
		// 일: 양쪽이면 왕국마다 둘('t' 먼저). 한도는 떼기의 것(겹침 한도 50 이 둘, 그리고 여유). 방향은 없다.
		const std::vector<std::string> kingdoms = { uuid, "b113a12eef97ea23" };
		std::vector<DiplomacyJob> jobs = PlanJobs({ "all", DiplomacyGoal::Clear, 'b', 0 }, kingdoms);
		CHECK(jobs.size() == 4 && jobs[0].Side == 't' && jobs[1].Side == 'u' && jobs[0].Left == k_ClearStepsMax && jobs[0].Sign == 0);
		CHECK(k_ClearStepsMax > 2 * k_OpinionStackLimit);
		jobs = PlanJobs({ uuid, DiplomacyGoal::Clear, 'u', 0 }, kingdoms);
		CHECK(jobs.size() == 1 && jobs[0].Uuid == uuid && jobs[0].Side == 'u');
		// 떼기는 붙이는 걸음이 아니다: 붙이기의 계획은 건드리지 않는다고 답한다.
		CHECK(StepToward(3, DiplomacyGoal::Clear) == 2 && PlanStep(3, DiplomacyGoal::Clear, 0, 10, 0).Outcome == 'k');
		// 뗐는가: 떼기 바로 앞뒤의 평판의 수. 하나 줄었으면 'y', 그대로면 'n', 그 밖(세지 못함, 둘 이상 줆, 늚)은 'u'.
		CHECK(DetachCheck(10, 9) == 'y' && DetachCheck(10, 10) == 'n' && DetachCheck(10, 8) == 'u' && DetachCheck(10, 11) == 'u');
		CHECK(DetachCheck(-1, 9) == 'u' && DetachCheck(10, -1) == 'u' && DetachCheck(std::numeric_limits<double>::quiet_NaN(), 9) == 'u');
		// 걸음 뒤의 판단: 'c' 뗐다(센다. 같은 자료로 계속), 'n' 그 자료는 더 없다(다음 자료로. 나쁜 것까지 끝났으면 끝), 'u' 모른다(멈춘다: 세지 못하면 떼지 않는다).
		CHECK(AfterDetach('y') == 'c' && AfterDetach('n') == 'n' && AfterDetach('u') == 'u' && AfterDetach('?') == 'u');
		// 결과의 글: 뗀 개수(좋은 것, 나쁜 것)와 관계의 앞뒤. Outcome: 'd' 다 뗐다, 'a' 뗄 것이 없었다, 'l' 한도에 닿았다, 'f' 실패(Why), 'x' 망한 왕국.
		CHECK_STR(ClearOpinionReport("크래스터", 't', 4, 3, 3, 0, 'd', ""), "크래스터: 그쪽이 우리를 보는 평판에서 좋은 평판 3개를 뗐습니다 (관계는 우호 -> 중립)");
		CHECK_STR(ClearOpinionReport("크래스터", 'u', 3, 3, 0, 2, 'd', ""), "크래스터: 우리가 그쪽을 보는 평판에서 나쁜 평판 2개를 뗐습니다 (관계는 중립 그대로)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 3, 3, 1, 1, 'd', ""), "크래스터: 그쪽이 우리를 보는 평판에서 좋은 평판 1개, 나쁜 평판 1개를 뗐습니다 (관계는 중립 그대로)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 3, 3, 0, 0, 'a', ""), "크래스터: 그쪽이 우리를 보는 평판에 뗄 디버그 평판이 없습니다 (관계는 중립 그대로)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 3, 3, 0, 0, 'f', "cannot count"), "크래스터: 그쪽이 우리를 보는 평판을 떼지 못했습니다 (cannot count)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 3, 3, 2, 0, 'f', ""), "크래스터: 그쪽이 우리를 보는 평판에서 좋은 평판 2개를 뗐고 그 뒤로는 떼지 못했습니다 (모릅니다. 관계는 중립 그대로)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 4, 3, 120, 0, 'l', ""), "크래스터: 그쪽이 우리를 보는 평판에서 좋은 평판 120개를 뗐습니다. 한도에 닿아 멈췄습니다 (관계는 우호 -> 중립)");
		CHECK_STR(ClearOpinionReport("크래스터", 't', 3, 3, 0, 0, 'x', ""), "크래스터: 망했거나 왕이 없는 왕국입니다. 건드리지 않습니다");
		// 원격 명령: diplomacy <uuid|all> clear [side=them|us|both] [queue=1]. 수와 이름은 받지 않는다.
		RemoteCommand line = ParseRemoteLine("diplomacy all clear");
		CHECK(line.Error.empty() && line.Target == "all" && line.Options.at("goal") == "clear");
		line = ParseRemoteLine("diplomacy " + uuid + " clear side=us queue=1");
		CHECK(line.Error.empty() && line.Options.at("side") == "us");
		for (const char* bad : { "diplomacy 0d8e3a894f258750 clear amount=3", "diplomacy 0d8e3a894f258750 clear name=x", "diplomacy 0d8e3a894f258750 clear side=me" })
			CHECK(!ParseRemoteLine(bad).Error.empty());
	});
}
