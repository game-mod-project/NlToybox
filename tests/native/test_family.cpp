#include "common.hpp"

void RunFamilyTests()
{
	Test("임신·출생·성장: 단계는 특성으로, 다음 단계 함수를 부른 뒤의 판정, 명령", [] {
		// 임신의 단계는 특성으로 보인다(research/24): pregnant_st1, st2, st3. 임신이 아니면 0.
		CHECK(PregnancyStage({ "human", "gifted" }) == 0 && PregnancyStage({}) == 0);
		CHECK(PregnancyStage({ "human", "pregnant_st1" }) == 1 && PregnancyStage({ "pregnant_st2", "human" }) == 2 && PregnancyStage({ "state_immobilization", "pregnant_st3" }) == 3);
		CHECK(PregnancyStage({ "pregnant_forbid" }) == 0);				// 출산 뒤에 붙는 특성은 임신이 아니다
		CHECK(IsKid({ "human", "kid" }) && !IsKid({ "human", "untitled_lord" }) && !IsKid({}));
		CHECK(k_GrownAge == 18);		// 15, 16 에서는 아이 그대로였고 18 에서 게임이 kid 를 떼고 untitled_lord 를 붙였다

		// 다음 단계 함수를 한 번 부른 뒤: 앞뒤의 단계로 가른다
		CHECK(AfterStageCall(1, 2) == StageOutcome::Advanced && AfterStageCall(2, 3) == StageOutcome::Advanced);
		CHECK(AfterStageCall(3, 0) == StageOutcome::Born);
		CHECK(AfterStageCall(1, 1) == StageOutcome::Stuck && AfterStageCall(3, 3) == StageOutcome::Stuck && AfterStageCall(2, 1) == StageOutcome::Stuck);
		CHECK(AfterStageCall(1, 0) == StageOutcome::Stuck && AfterStageCall(1, 3) == StageOutcome::Stuck);		// 본 적 없는 건너뜀은 된 것으로 치지 않는다
		// "바로 출산": 임신 중이고 세 번을 넘기지 않았을 때만 더 부른다
		CHECK(BirthNeedsCall(1, 0) && BirthNeedsCall(3, 2) && !BirthNeedsCall(0, 0) && !BirthNeedsCall(0, 3) && !BirthNeedsCall(1, 3) && !BirthNeedsCall(2, 5));

		// 결과의 글. 둘째 인자: "바로 출산"을 청했는가. 마지막 인자: 부르는 동안 새로 생긴 사람의 수.
		CHECK_STR(StageReport("Kira", false, 1, 2, 1, "", 0), "Kira: 임신 2/3기가 됐습니다");
		CHECK_STR(StageReport("Kira", false, 2, 3, 1, "", 0), "Kira: 임신 3/3기가 됐습니다");
		CHECK_STR(StageReport("Kira", false, 3, 0, 1, "", 1), "Kira: 출산했습니다 (다음 단계 함수를 1번 불렀습니다)");
		CHECK_STR(StageReport("Kira", true, 1, 0, 3, "", 1), "Kira: 출산했습니다 (다음 단계 함수를 3번 불렀습니다)");
		// 임신이 끝났는데 아이가 생기지 않았다: 출산이라고 말하지 않는다. 다음 단계 함수는 게임의 유산 확률을 그대로 탄다
		// (실행 2 에서 3/3기의 다음 호출 뒤 영주의 수가 그대로였고 어머니에게 생각 pregnancy_miscarriage 가 붙었다. research/24).
		CHECK_STR(StageReport("Kira", true, 2, 0, 2, "", 0), "Kira: 임신이 끝났지만 아이가 생기지 않았습니다 (유산으로 보입니다. 다음 단계 함수를 2번 불렀습니다)");
		CHECK_STR(StageReport("Kira", false, 0, 0, 0, "", 0), "Kira: 임신 중이 아닙니다");
		CHECK_STR(StageReport("Kira", true, 0, 0, 0, "", 0), "Kira: 임신 중이 아닙니다");
		CHECK_STR(StageReport("Kira", false, 2, 2, 1, "", 0), "Kira: 단계가 바뀌지 않았습니다 (임신 2/3기 그대로)");
		CHECK_STR(StageReport("Kira", false, 1, 2, 3, "no member", 0), "Kira: 임신 2/3기에서 멈췄습니다 (no member)");
		CHECK_STR(StageReport("Kira", false, 1, 0, 1, "본 적 없는 바뀜", 0), "Kira: 임신이 끝났습니다 (본 적 없는 바뀜)");
		// 검토의 지적: 출산을 청했는데 중간에 멈췄으면 "N/3기가 됐습니다"(다음 단계의 성공과 같은 글)라고 하지 않는다
		CHECK_STR(StageReport("Kira", true, 1, 2, 2, "", 0), "Kira: 출산까지 가지 못했습니다 (임신 2/3기에서 단계가 더 바뀌지 않았습니다)");
		// 검토의 지적: 부른 뒤 그 사람을 다시 읽지 못했으면 뒤의 단계를 모른다(-1). "3/3기에서 멈췄다"고 단정하지 않는다
		CHECK_STR(StageReport("Kira", true, 3, -1, 1, "그 자리의 사람이 바뀌었습니다", 0),
			"Kira: 다음 단계 함수를 1번 불렀지만 그 뒤를 읽지 못했습니다 (그 자리의 사람이 바뀌었습니다)");
		CHECK_STR(StageReport("Kira", false, 2, -1, 1, "", 0), "Kira: 다음 단계 함수를 1번 불렀지만 그 뒤를 읽지 못했습니다");
		// 된 것인가(반환값). 다음 단계: 한 번 불렀고 본 대로 바뀌었다. 바로 출산: 임신이 끝났고 아이가 생겼다.
		CHECK(NextDone(1, 2, 1, "") && NextDone(2, 3, 1, "") && NextDone(3, 0, 1, ""));
		CHECK(!NextDone(0, 0, 0, "") && !NextDone(2, 2, 1, "") && !NextDone(1, 3, 1, "") && !NextDone(1, 2, 1, "x") && !NextDone(3, -1, 1, "") && !NextDone(1, 2, 0, ""));
		CHECK(BirthDone(0, 1, "") && !BirthDone(0, 0, "") && !BirthDone(2, 1, "") && !BirthDone(0, 1, "x") && !BirthDone(-1, 1, ""));
		// 단계의 특성 이름(임신을 시작할 때 1/3기의 것을 붙인다)
		CHECK_STR(PregnancyTrait(1), "pregnant_st1");
		CHECK_STR(PregnancyTrait(2), "pregnant_st2");
		CHECK_STR(PregnancyTrait(3), "pregnant_st3");
		CHECK_STR(PregnancyTrait(0), "");
		CHECK_STR(PregnancyTrait(4), "");
		CHECK(PregnancyStage({ PregnancyTrait(2) }) == 2);

		// 검토의 지적: 임신·성장의 일은 플레이어의 영주에게만 한다(주민·손님·다른 진영에게는 불러 본 적이 없다). 아버지도 플레이어의 영주여야 한다.
		PersonRow lord;
		lord.Character = true;
		lord.Faction = "player";
		CHECK(IsPlayersLord(lord));
		PersonRow guest = lord;
		guest.Faction = "unique_guests";
		PersonRow grown = lord;
		grown.Faction = "player_untitled";
		PersonRow peasant = lord;
		peasant.Character = false;
		CHECK(!IsPlayersLord(guest) && !IsPlayersLord(grown) && !IsPlayersLord(peasant));
		// 임신을 시작할 수 없는 까닭의 글이 갈린다(성별을 읽지 못한 것과 남성)
		std::string male, unknown;
		CHECK(!CanConceive(0, { "human" }, male) && !CanConceive(-1e9, { "human" }, unknown) && male != unknown);
		// 원격의 답: 아버지가 uuid 꼴이 아니면 그렇게 말한다
		CHECK(ParseRemoteLine("person a34ba8b605c96ab7 conceive name=daven").Error.find("uuid") != std::string::npos);

		// 임신을 시작할 수 있는가(누르기 전의 판정. 게임의 is_can_pregant 는 틱이 따로 묻는다): 여성(성별 1)이고 아이가 아니고 임신 중이 아니고 출산 뒤의 금지가 없다
		std::string why;
		CHECK(CanConceive(1, { "human" }, why) && why.empty());
		CHECK(!CanConceive(0, { "human" }, why) && !why.empty());					// 남성
		CHECK(!CanConceive(1, { "human", "kid" }, why));							// 아이
		CHECK(!CanConceive(1, { "human", "pregnant_st1" }, why));					// 이미 임신 중
		CHECK(!CanConceive(1, { "human", "pregnant_forbid" }, why));				// 출산 뒤
		CHECK(!CanConceive(-1e9, { "human" }, why));								// 성별을 읽지 못했다
		// 아버지: 남성(성별 0)이고 아이가 아니다
		CHECK(CanFather(0, { "human" }) && !CanFather(1, { "human" }) && !CanFather(0, { "human", "kid" }) && !CanFather(-1e9, {}));

		// 게임 변수의 열쇠(값은 게임에서 읽는다): 임신 확률 둘, 유산 하나, 출산 중 사망 둘
		const auto has = [](const std::vector<const char*>& list, const char* key) {
			return std::find_if(list.begin(), list.end(), [&](const char* item) { return std::string(item) == key; }) != list.end();
		};
		CHECK(PregnancyChanceVars().size() == 2 && has(PregnancyChanceVars(), "pregnancy_chance") && has(PregnancyChanceVars(), "pregnancy_from_dummy_chance"));
		CHECK(MiscarriageVars().size() == 1 && has(MiscarriageVars(), "pregnancy_miscarriage_chance"));
		CHECK(ChildbirthDeathVars().size() == 2 && has(ChildbirthDeathVars(), "pregnancy_mother_die") && has(ChildbirthDeathVars(), "trait_death_in_childbirth"));

		// 치트 표: 게임 변수를 쓰는 셋(모듈의 일). 게임이 그 값을 따르는지는 보지 못했다(확인 전)
		CHECK(FindCheat("pregnancy_chance") && FindCheat("pregnancy_chance")->Kind == CheatKind::CustomScale && FindCheat("pregnancy_chance")->Where == Area::People
			&& !FindCheat("pregnancy_chance")->Verified && FindCheat("pregnancy_chance")->Min >= 1 && FindCheat("pregnancy_chance")->Max <= 2);
		for (const char* id : { "no_miscarriage", "safe_childbirth" })
			CHECK(FindCheat(id) && FindCheat(id)->Kind == CheatKind::Custom && FindCheat(id)->Where == Area::People);
		// 유산 없음은 플레이에서 봤다(실행 3: 켠 채 임신 25번에 유산 0, 끈 채 27번에 유산 5. research/24). 다음 실행에서도 켜진 채로 시작하고 신 묶음에 든다.
		// 출산 중 사망 없음은 가리지 못했다(끈 채 22번의 출산에서도 어머니가 죽지 않았다).
		CHECK(FindCheat("no_miscarriage")->Verified && !FindCheat("safe_childbirth")->Verified);
		CheatState family;
		family.On = { "no_miscarriage", "safe_childbirth" };
		CHECK(KeepKnown(family).On == (std::set<std::string>{ "no_miscarriage" }));
		bool in_god = false;
		for (const PresetItem& item : FindPreset("god")->Items)
			in_god = in_god || std::string(item.Id) == "no_miscarriage";
		CHECK(in_god);

		// 명령의 낱말과 검사
		PersonAct act = PersonAct::SkillSet;
		CHECK(ParsePersonAct("pregnancy_next", act) && act == PersonAct::PregnancyNext && std::string(PersonActWord(PersonAct::PregnancyNext)) == "pregnancy_next");
		CHECK(ParsePersonAct("birth", act) && act == PersonAct::Birth && ParsePersonAct("grow_up", act) && act == PersonAct::GrowUp);
		CHECK(ParsePersonAct("conceive", act) && act == PersonAct::Conceive);
		for (const PersonAct one : { PersonAct::PregnancyNext, PersonAct::Birth, PersonAct::GrowUp })
			CHECK(!NeedsText(one) && !NeedsAmount(one) && !NeedsIndex(one));
		CHECK(NeedsText(PersonAct::Conceive) && !NeedsAmount(PersonAct::Conceive));
		// 여럿에게는 "임신한 영주 모두 출산"만. 주민에게는 불러 보지 않았다
		CHECK(BulkAllowed("lords", PersonAct::Birth) && !BulkAllowed("people", PersonAct::Birth));
		for (const PersonAct one : { PersonAct::PregnancyNext, PersonAct::GrowUp, PersonAct::Conceive })
			CHECK(!BulkAllowed("lords", one) && !BulkAllowed("people", one) && BulkAllowed("25556c3312bce178", one));
		PersonCommand c;
		c.Act = PersonAct::Conceive;
		c.Who = "a34ba8b605c96ab7";
		c.Text = "8f2bfdb1951cc39c";
		CHECK(CheckPersonCommand(c, why) && why.empty());
		c.Text = "a34ba8b605c96ab7";
		CHECK(!CheckPersonCommand(c, why));			// 자기 자신은 아버지가 아니다
		c.Text = "king";
		CHECK(!CheckPersonCommand(c, why));			// 아버지는 uuid 로 짚는다
		c.Text = "";
		CHECK(!CheckPersonCommand(c, why));
		CHECK(IsUuid("8f2bfdb1951cc39c") && !IsUuid("8f2bfdb1951cc39") && !IsUuid("8F2BFDB1951CC39C") && !IsUuid("zzzzzzzzzzzzzzzz") && !IsUuid(""));

		// 원격의 줄
		CHECK(ParseRemoteLine("person a34ba8b605c96ab7 pregnancy_next").Error.empty() && ParseRemoteLine("person a34ba8b605c96ab7 birth").Error.empty());
		CHECK(ParseRemoteLine("person lords birth").Error.empty() && ParseRemoteLine("person 514213d4ae16f161 grow_up").Error.empty());
		CHECK(ParseRemoteLine("person a34ba8b605c96ab7 conceive name=8f2bfdb1951cc39c").Error.empty());
		CHECK(!ParseRemoteLine("person people birth").Error.empty() && !ParseRemoteLine("person lords grow_up").Error.empty()
			&& !ParseRemoteLine("person a34ba8b605c96ab7 conceive").Error.empty() && !ParseRemoteLine("person a34ba8b605c96ab7 conceive name=daven").Error.empty());
	});
}
