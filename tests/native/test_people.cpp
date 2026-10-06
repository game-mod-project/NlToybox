#include "common.hpp"

void RunPeopleTests()
{
	// 요청 파일의 오타로 게임 실행 한 번을 버리지 않는다.
	Test("인물: 능력치 여덟과 욕구 여섯의 이름", [] {
		// 능력치의 열쇠는 __soul.__skills.__level 의 이름, 욕구의 차례는 게임의 번호다(research/11).
		const std::vector<NamedKey>& skills = SkillNames();
		CHECK(skills.size() == 8);
		const char* keys[] = { "combat", "command", "education", "knowledge", "management", "manners", "negotiation", "oratory" };
		for (size_t i = 0; i < skills.size() && i < 8; i++)
			CHECK_STR(skills[i].Key, keys[i]);
		const std::vector<const char*>& needs = NeedNames();
		CHECK(needs.size() == 6);
		CHECK_STR(needs[0], "수면");
		CHECK_STR(needs[1], "음식");
		CHECK_STR(needs[5], "돌봄");
	});

	Test("인물: 명령의 낱말과 그것이 받는 것", [] {
		for (const char* word : { "skill_set", "skill_add", "skills_max", "need_set", "needs_fill", "age_set", "happy", "cure", "trait_add", "trait_remove" })
		{
			PersonAct act = PersonAct::Cure;
			CHECK(ParsePersonAct(word, act));
			CHECK_STR(PersonActWord(act), word);
		}
		PersonAct act = PersonAct::Cure;
		CHECK(!ParsePersonAct("kill", act) && !ParsePersonAct("", act));
		CHECK(NeedsIndex(PersonAct::SkillSet) && NeedsIndex(PersonAct::SkillAdd) && NeedsIndex(PersonAct::NeedSet));
		CHECK(!NeedsIndex(PersonAct::SkillsMax) && !NeedsIndex(PersonAct::AgeSet) && !NeedsIndex(PersonAct::Happy));
		CHECK(NeedsAmount(PersonAct::SkillSet) && NeedsAmount(PersonAct::NeedSet) && NeedsAmount(PersonAct::AgeSet) && !NeedsAmount(PersonAct::Cure));
		CHECK(NeedsText(PersonAct::TraitAdd) && NeedsText(PersonAct::TraitRemove) && !NeedsText(PersonAct::Happy));
	});

	Test("인물: 온전하지 않은 명령은 하지 않는다", [] {
		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::SkillSet;
		c.Index = 4;
		c.Amount = 12;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 8;		// 능력치는 여덟이다
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Index = -1;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 0;
		c.Amount = std::numeric_limits<double>::quiet_NaN();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::NeedSet;
		c.Amount = 100;
		c.Index = 5;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 6;		// 욕구는 여섯이다
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::TraitAdd;
		c.Text = "brave";
		CHECK(CheckPersonCommand(c, why));
		c.Text = "Brave!";
		CHECK(!CheckPersonCommand(c, why));
		c.Text.clear();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::Happy;
		CHECK(CheckPersonCommand(c, why));
		c.Who.clear();		// 누구인지 없다
		CHECK(!CheckPersonCommand(c, why));
	});

	Test("인물: 일괄 명령은 플레이어의 산 사람에게만 간다", [] {
		// 영주 A, 주민 B, 손님 C(상인의 진영은 unique_guests 였다), 죽은 영주 D, 다른 왕국의 영주 E
		std::vector<PersonRow> people(5);
		people[0] = { "aaaa", "A", "player", true, 0, 3, false };
		people[1] = { "bbbb", "B", "player", false, 0, 1, false };
		people[2] = { "cccc", "C", "unique_guests", true, 1, 3, false };
		people[3] = { "dddd", "D", "player", true, 2, 3, true };
		people[4] = { "eeee", "E", "kingdom_7", true, 3, 3, false };
		CHECK(IsPlayers(people[0]) && IsPlayers(people[1]) && !IsPlayers(people[2]) && !IsPlayers(people[3]) && !IsPlayers(people[4]));

		CHECK(PickTargets(people, "lords") == std::vector<size_t>{ 0 });
		CHECK((PickTargets(people, "people") == std::vector<size_t>{ 0, 1 }));
		// 하나를 짚어 고른 것은 손님이어도 된다(사용자가 골랐다). 죽은 사람과 없는 사람은 안 된다.
		CHECK(PickTargets(people, "cccc") == std::vector<size_t>{ 2 });
		CHECK(PickTargets(people, "dddd").empty());
		CHECK(PickTargets(people, "ffff").empty() && PickTargets(people, "").empty());
	});

	Test("인물: 쓰는 수는 범위 안의 수로 다듬는다", [] {
		double out = -1;
		CHECK(SkillValue(12, out) && out == 12);
		CHECK(SkillValue(25, out) && out == 20);			// 게임의 최고 등급(get_max_level 이 20 을 돌려줬다)
		CHECK(SkillValue(-3, out) && out == 0);
		CHECK(SkillValue(7.6, out) && out == 8);			// 등급은 정수다
		CHECK(!SkillValue(std::numeric_limits<double>::infinity(), out));
		CHECK(SkillAfterAdd(19, 5, out) && out == 20);
		CHECK(SkillAfterAdd(2, -5, out) && out == 0);
		CHECK(!SkillAfterAdd(std::numeric_limits<double>::quiet_NaN(), 1, out));

		CHECK(NeedValue(150, 100, out) && out == 100);
		CHECK(NeedValue(-1, 100, out) && out == 0);
		CHECK(NeedValue(40.5, 100, out) && out == 40.5);
		CHECK(NeedValue(90, 60, out) && out == 60);			// 상한이 낮아진 욕구는 그 상한까지만
		CHECK(!NeedValue(50, std::numeric_limits<double>::quiet_NaN(), out) && !NeedValue(50, 0, out));

		CHECK(AgeValue(30.4, out) && out == 30);
		CHECK(AgeValue(500, out) && out == 120);
		CHECK(AgeValue(-4, out) && out == 1);
		CHECK(!AgeValue(std::numeric_limits<double>::quiet_NaN(), out));
	});

	Test("인물: 특성 이름의 꼴과 치료가 떼는 부상", [] {
		CHECK(GoodTraitName("brave") && GoodTraitName("pneumonia_st_1") && GoodTraitName("__wolves_wont_attack__"));
		CHECK(!GoodTraitName("") && !GoodTraitName("a b") && !GoodTraitName("Brave") && !GoodTraitName("brave;x") && !GoodTraitName(std::string(65, 'a')));

		const std::vector<const char*>& wounds = WoundTraits();
		const auto has = [&](const char* name) {
			for (const char* wound : wounds)
				if (std::string(wound) == name)
					return true;
			return false;
		};
		CHECK(has("bruise_light") && has("cut") && has("wound_deep") && has("burn_heavy"));
		CHECK(!has("human") && !has("brave") && !has("kid") && !has("lost_head") && !has("dead"));
		for (const char* wound : wounds)
			CHECK(GoodTraitName(wound));
	});

	Test("인물: 죽음은 게임의 캐시에서 읽고, 비어 있으면 모른다고 답한다", [] {
		// c_status.__is_dead 는 true/false 이거나, 아직 셈하지 않았으면 -4 다. 특성을 바꾸면 게임이 캐시를 비운다(research/11).
		// -4 를 "죽었다"로 읽으면 특성을 붙인 바로 뒤의 명령이 그 사람을 놓친다(확인 실행에서 그랬다).
		CHECK(AliveFromDeadCache(true, 0) == Alive::Yes);
		CHECK(AliveFromDeadCache(true, 1) == Alive::No);
		CHECK(AliveFromDeadCache(true, -4) == Alive::Unknown);
		CHECK(AliveFromDeadCache(false, 0) == Alive::Unknown);		// 읽지 못했다
		CHECK(AliveFromDeadCache(true, std::numeric_limits<double>::quiet_NaN()) == Alive::Unknown);
	});

	Test("인물: 종과 죽음의 특성은 붙이지도 떼지도 않는다", [] {
		for (const char* name : { "human", "wolf", "pig", "dog", "dead", "delayed_dead", "dead_from_old_age", "dead_from_poison", "dying_from_old", "lost_head" })
			CHECK(IsProtectedTrait(name));
		for (const char* name : { "brave", "kid", "bruise_light", "pneumonia_st_1", "inspired", "" })
			CHECK(!IsProtectedTrait(name));

		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::TraitAdd;
		c.Text = "dead";
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Act = PersonAct::TraitRemove;
		c.Text = "human";
		CHECK(!CheckPersonCommand(c, why));
		c.Text = "brave";
		CHECK(CheckPersonCommand(c, why));
		CHECK(!ParseRemoteLine("person 25556c3312bce178 trait_add name=lost_head").Error.empty());
	});

	Test("인물: 여럿에게 한꺼번에 하는 것은 네 가지뿐이다", [] {
		CHECK(IsBulkWho("lords") && IsBulkWho("people") && !IsBulkWho("25556c3312bce178") && !IsBulkWho(""));
		std::string why;
		PersonCommand c;
		c.Who = "people";
		for (const PersonAct act : { PersonAct::SkillsMax, PersonAct::NeedsFill, PersonAct::Happy, PersonAct::Cure })
		{
			c.Act = act;
			CHECK(CheckPersonCommand(c, why));
		}
		// 나이·특성·능력치 하나·욕구 하나는 한 사람을 짚어서만 한다(한 줄의 실수가 모든 사람을 세이브에 남게 바꾸지 않게).
		c.Act = PersonAct::AgeSet;
		c.Amount = 1;
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Act = PersonAct::TraitAdd;
		c.Text = "brave";
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::SkillSet;
		c.Index = 0;
		c.Amount = 20;
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::NeedSet;
		CHECK(!CheckPersonCommand(c, why));
		c.Who = "lords";
		c.Act = PersonAct::TraitRemove;
		CHECK(!CheckPersonCommand(c, why));
		CHECK(!ParseRemoteLine("person people age_set amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person lords trait_add name=brave").Error.empty());
		CHECK(ParseRemoteLine("person lords cure").Error.empty());
	});

	Test("인물: 채우기는 상한까지, 번호는 유한한 정수만", [] {
		double out = -1;
		CHECK(NeedValue(k_FillAll, 100, out) && out == 100);		// 창의 "채우기"는 읽은 상한이 아니라 이 수를 보낸다(상한을 읽지 못했어도 0 을 쓰지 않는다)
		CHECK(NeedValue(k_FillAll, 60, out) && out == 60);
		CHECK(ShouldFillNeed(99.4, 100) && !ShouldFillNeed(99.6, 100) && !ShouldFillNeed(100, 100));	// 거의 찬 칸은 건드리지 않는다
		CHECK(!ShouldFillNeed(50, 0) && !ShouldFillNeed(std::numeric_limits<double>::quiet_NaN(), 100));

		// 행복 생각: 한 사람을 짚었으면 합을 읽지 못해도 붙인다. 여럿을 돌 때는 읽지 못한 사람을 건너뛴다(되풀이해 쌓이지 않게).
		CHECK(ShouldAttachHappy(true, 40, false) && ShouldAttachHappy(true, 40, true));
		CHECK(!ShouldAttachHappy(true, 100, false) && !ShouldAttachHappy(true, 136.5, true));
		CHECK(ShouldAttachHappy(false, 0, false) && !ShouldAttachHappy(false, 0, true));

		CHECK(!ParseRemoteLine("person lords skill_set index=inf amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=1e300 amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 happy index=inf").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 skill_set index=1.5 amount=1").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 age_set amount=nan").Error.empty());
		CHECK(ParseRemoteLine("person 25556c3312bce178 skill_add index=7 amount=-1").Error.empty());
	});

	Test("인구: 노화 깃발을 끈 뒤에는 처음부터 온전한 한 바퀴로 되돌린다", [] {
		const auto run = [](HoldRound& round, size_t count, bool ageless, bool skip = false) {
			// 한 틱: 계획을 받고, 묶음의 사람마다 깃발을 쓰고, 묶음을 닫는다. 돌려주는 것: 이번 틱에 깃발을 쓴 사람의 자리.
			std::vector<size_t> touched;
			const HoldPlan plan = HoldBegin(round, false, false, ageless);
			if (!plan.Work)
				return touched;
			const PeopleSlice slice = NextPeopleSlice(count, round.Cursor, 40);
			for (size_t i = slice.Begin; i < slice.End; i++)
			{
				const bool skipped = skip && i == slice.Begin;
				if (plan.WriteAge && !skipped)
					touched.push_back(i);
				HoldTouched(round, plan.WriteAge && !skipped && plan.AgeValue == 0, skipped);
			}
			HoldEnd(round, slice, ageless);
			return touched;
		};

		// (가) 바퀴 도중에 끈다: 100명, 40명씩. 80명까지 끈 뒤 항목을 끄면 처음부터 다시 돌아 100명을 모두 되돌린다.
		HoldRound round;
		std::vector<bool> off(100, false);
		for (int tick = 0; tick < 2; tick++)
			for (const size_t i : run(round, 100, true))
				off[i] = true;
		CHECK(round.AgeWritten && round.Cursor == 80);
		for (int tick = 0; tick < 3; tick++)
			for (const size_t i : run(round, 100, false))
				off[i] = false;
		CHECK(std::find(off.begin(), off.end(), true) == off.end());		// 남은 깃발이 없다
		CHECK(!round.AgeWritten);
		CHECK(run(round, 100, false).empty());								// 되돌릴 것이 없으면 돌지 않는다

		// (나) 켠 뒤 첫 바퀴가 감기기 전에 끈다: 첫 묶음만 쓴 채 꺼도 되돌린다.
		round = HoldRound();
		CHECK(run(round, 100, true).size() == 40 && round.AgeWritten);
		size_t restored = 0;
		for (int tick = 0; tick < 3; tick++)
			restored += run(round, 100, false).size();
		CHECK(restored == 100 && !round.AgeWritten);

		// (다) 되돌리는 바퀴에서 건너뛴 사람이 있으면(자리가 바뀌었다) 한 바퀴를 더 돈다.
		round = HoldRound();
		run(round, 30, true);
		CHECK(round.AgeWritten);
		run(round, 30, false, true);										// 한 사람을 건너뛰었다
		CHECK(round.AgeWritten);											// 아직 되돌릴 것이 있다고 본다
		const HoldPlan again = HoldBegin(round, false, false, false);
		CHECK(again.Work && again.Rescan && again.WriteAge && again.AgeValue == 1);
		round.Cursor = 0;
		run(round, 30, false);												// 이번에는 깨끗이 돈다
		CHECK(!round.AgeWritten);

		// 다시 켜면 되돌리던 것을 그만두고 끈다.
		round = HoldRound();
		run(round, 100, true);
		run(round, 100, false);
		const HoldPlan on = HoldBegin(round, false, false, true);
		CHECK(on.WriteAge && on.AgeValue == 0 && !round.Restoring);

		// 할 일이 없으면 일하지 않는다. 욕구나 행복만 켜져 있으면 깃발은 건드리지 않는다.
		round = HoldRound();
		CHECK(!HoldBegin(round, false, false, false).Work);
		const HoldPlan needs = HoldBegin(round, true, false, false);
		CHECK(needs.Work && !needs.WriteAge);
	});

	Test("인물: 지식·소지금·소지품을 주는 명령", [] {
		for (const char* word : { "knowledge_all", "knowledge_add", "money_add", "item_add" })
		{
			PersonAct act = PersonAct::Cure;
			CHECK(ParsePersonAct(word, act));
			CHECK_STR(PersonActWord(act), word);
		}
		CHECK(NeedsText(PersonAct::KnowledgeAdd) && !NeedsText(PersonAct::KnowledgeAll));
		CHECK(NeedsAmount(PersonAct::MoneyAdd) && NeedsAmount(PersonAct::ItemAdd) && !NeedsAmount(PersonAct::KnowledgeAll) && !NeedsAmount(PersonAct::KnowledgeAdd));
		CHECK(NeedsIndex(PersonAct::ItemAdd) && !NeedsIndex(PersonAct::MoneyAdd) && !NeedsIndex(PersonAct::KnowledgeAdd));
		CHECK(IndexLimit(PersonAct::SkillSet) == 8 && IndexLimit(PersonAct::NeedSet) == 6 && IndexLimit(PersonAct::ItemAdd) == 200 && IndexLimit(PersonAct::Happy) == 0);

		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::KnowledgeAdd;
		c.Text = "building_mine";
		CHECK(CheckPersonCommand(c, why));
		c.Text = "dead";			// 지식의 이름은 특성의 막힌 이름과 무관하다(게임의 지식 목록으로 본다)
		CHECK(CheckPersonCommand(c, why));
		c.Text = "Bad Name";
		CHECK(!CheckPersonCommand(c, why));
		c.Text.clear();
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::ItemAdd;
		c.Index = 1;
		c.Amount = 50;
		CHECK(CheckPersonCommand(c, why));
		c.Index = -1;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 200;
		CHECK(!CheckPersonCommand(c, why));
		c.Index = 1;
		c.Amount = 0;				// 줄 것이 없다
		CHECK(!CheckPersonCommand(c, why));

		c.Act = PersonAct::MoneyAdd;
		c.Amount = 1000;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = -200;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = std::numeric_limits<double>::quiet_NaN();
		CHECK(!CheckPersonCommand(c, why));
		c.Amount = 2e9;				// 한 번에 백만까지
		CHECK(!CheckPersonCommand(c, why));

		// 모든 지식은 영주 전원에게도 된다(주민은 지식을 갖지 않는다). 소지금·소지품·지식 하나는 한 사람을 짚어서만.
		c.Act = PersonAct::KnowledgeAll;
		c.Who = "lords";
		CHECK(CheckPersonCommand(c, why));
		c.Who = "people";
		CHECK(!CheckPersonCommand(c, why));
		c.Who = "lords";
		c.Act = PersonAct::MoneyAdd;
		c.Amount = 100;
		CHECK(!CheckPersonCommand(c, why));
		c.Act = PersonAct::Cure;
		c.Who = "people";
		CHECK(CheckPersonCommand(c, why));
		CHECK(BulkAllowed("lords", PersonAct::KnowledgeAll) && !BulkAllowed("people", PersonAct::KnowledgeAll) && BulkAllowed("people", PersonAct::Happy)
			&& !BulkAllowed("lords", PersonAct::ItemAdd) && BulkAllowed("25556c3312bce178", PersonAct::ItemAdd));
	});

	Test("인물: 소지품의 번호는 0(신성 반지)부터, 착용 중인 장비는 빼지 않는다", [] {
		std::string why;
		PersonCommand c;
		c.Who = "25556c3312bce178";
		c.Act = PersonAct::ItemAdd;
		c.Amount = 5;
		// 0 번(신성 반지)도 준다: 영주의 소지품 0 번 칸에 넣은 수를 게임의 character_runes_get_count 가 그대로 돌려줬다(research/18).
		c.Index = 0;
		CHECK(CheckPersonCommand(c, why) && why.empty());
		c.Index = -1;
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Index = 1;
		CHECK(CheckPersonCommand(c, why));
		c.Index = 199;
		CHECK(CheckPersonCommand(c, why));
		CHECK(ParseRemoteLine("person 25556c3312bce178 item_add index=0 amount=5").Error.empty());
		CHECK(!ParseRemoteLine("person 25556c3312bce178 item_add index=-1 amount=5").Error.empty());

		// "0 으로" 단추는 -k_GiftMax 를 보낸다(가진 것까지만 빠진다). 그 수는 받아야 하고, 그것을 넘는 수는 받지 않는다.
		c.Index = 1;
		c.Amount = -k_GiftMax;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = k_GiftMax;
		CHECK(CheckPersonCommand(c, why));
		c.Amount = k_GiftMax + 1;
		CHECK(!CheckPersonCommand(c, why));
		c.Amount = -(k_GiftMax + 1);
		CHECK(!CheckPersonCommand(c, why));

		// 착용 중인 장비의 자원 번호(__equipment.__cached_armor·first_arm·second_arm 의 __resource. 없으면 -1 이나 읽지 못한 수).
		// 그 칸은 소지품에서 빼지 않는다: 수만 줄고 착용은 그대로라 어긋난다(게임이 어떻게 받는지 재지 않았다).
		const std::vector<double> equipped = { 6, 10, -1 };
		CHECK(IsEquipped(6, equipped) && IsEquipped(10, equipped));
		CHECK(!IsEquipped(1, equipped) && !IsEquipped(-1, equipped) && !IsEquipped(0, {}));
		CHECK(!IsEquipped(6, { -1e9, std::numeric_limits<double>::quiet_NaN() }));
	});

	Test("인물: 주는 수는 정수로, 가진 것보다 많이 빼지 않는다", [] {
		double delta = 0;
		CHECK(GiftDelta(493, 1000, delta) && delta == 1000);
		CHECK(GiftDelta(493, -1000, delta) && delta == -493);		// 0 아래로 내려가지 않는다
		CHECK(GiftDelta(10, 2.6, delta) && delta == 3);
		CHECK(!GiftDelta(0, -5, delta));							// 뺄 것이 없다
		CHECK(!GiftDelta(5, 0.2, delta));							// 반올림하면 0 이다
		CHECK(!GiftDelta(std::numeric_limits<double>::quiet_NaN(), 5, delta) && !GiftDelta(5, std::numeric_limits<double>::infinity(), delta));
		CHECK(GiftDelta(-3, 10, delta) && delta == 10);			// 읽은 수가 음수여도 더하는 것은 된다
	});

	Test("장비: 선호 장비의 묶음과 넣어 줄 것", [] {
		PersonAct act = PersonAct::SkillSet;
		CHECK(ParsePersonAct("equip", act) && act == PersonAct::Equip && std::string(PersonActWord(PersonAct::Equip)) == "equip");
		CHECK(NeedsText(PersonAct::Equip) && !NeedsAmount(PersonAct::Equip) && !NeedsIndex(PersonAct::Equip));

		// 묶음의 이름: 게임의 선호 장비 자료(o_data.__preferred_equipment_data)의 멤버를 가리킨다(research/17)
		CHECK(FindLoadout("h_swordman") && std::string(FindLoadout("h_swordman")->Member) == "__h_swordman");
		CHECK(FindLoadout("any") && std::string(FindLoadout("any")->Member) == "__any");
		CHECK(!FindLoadout("wolf") && !FindLoadout("") && !FindLoadout("__h_swordman"));
		CHECK(Loadouts().size() >= 4);

		// 한 사람이나 플레이어의 사람 전원(병사만 고른다)에게. 영주 전원에게는 하지 않는다
		std::string why;
		CHECK(CheckPersonCommand(PersonCommand{ PersonAct::Equip, "25556c3312bce178", -1, 0, "h_swordman" }, why));
		CHECK(CheckPersonCommand(PersonCommand{ PersonAct::Equip, "people", -1, 0, "any" }, why));
		CHECK(!CheckPersonCommand(PersonCommand{ PersonAct::Equip, "lords", -1, 0, "h_swordman" }, why));
		CHECK(!CheckPersonCommand(PersonCommand{ PersonAct::Equip, "people", -1, 0, "wolf" }, why));
		CHECK(!CheckPersonCommand(PersonCommand{ PersonAct::Equip, "people", -1, 0, "" }, why));

		// 넣어 줄 것: 선호 장비의 갑옷·무기·방패 가운데 소지품에 없는 것(자원 번호). -1(없음)과 -2(아무거나)는 주지 않는다
		std::vector<double> bag(39, 0);
		CHECK(EquipGifts(7, 12, true, bag) == (std::vector<int>{ 7, 12, k_ShieldResource }));
		bag[12] = 1;
		CHECK(EquipGifts(7, 12, true, bag) == (std::vector<int>{ 7, k_ShieldResource }));
		bag[7] = 1;
		bag[k_ShieldResource] = 2;
		CHECK(EquipGifts(7, 12, true, bag).empty());
		CHECK(EquipGifts(-1, 10, false, std::vector<double>(39, 0)) == (std::vector<int>{ 10 }));
		CHECK(EquipGifts(-2, -2, true, std::vector<double>(39, 0)).empty());		// "아무 장비나": 정해진 것이 없으니 방패도 넣지 않는다
		CHECK(EquipGifts(7, 12, true, std::vector<double>(10, 0)) == (std::vector<int>{ 7 }));		// 칸 밖의 번호는 주지 않는다
		CHECK(EquipGifts(std::numeric_limits<double>::quiet_NaN(), 7.5, false, std::vector<double>(39, 0)).empty());
		CHECK(EquipGifts(0, 12, false, std::vector<double>(39, 0)) == (std::vector<int>{ 12 }));		// 0 번 자원은 건드리지 않는다

		// 장비 지급의 결과(검토의 지적): 선호 장비가 남지 않았거나 넣지 못한 것이 있으면 실패로 적는다
		const EquipResult done = EquipReport("중갑·검·방패", true, 3, 3);
		CHECK(done.Ok && done.Note == "중갑·검·방패: 선호 장비로 정하고 3개를 넣었습니다");
		const EquipResult had = EquipReport("중갑·검·방패", true, 0, 0);
		CHECK(had.Ok && had.Note == "중갑·검·방패: 선호 장비로 정했습니다 (넣을 장비는 이미 갖고 있거나 없습니다)");
		const EquipResult part = EquipReport("중갑·검·방패", true, 3, 1);
		CHECK(!part.Ok && part.Note == "중갑·검·방패: 선호 장비는 정했지만 넣을 3개 가운데 2개를 넣지 못했습니다");
		const EquipResult unstuck = EquipReport("중갑·검·방패", false, 3, 0);
		CHECK(!unstuck.Ok && unstuck.Note == "중갑·검·방패: 선호 장비가 바뀌지 않았습니다 (장비는 넣지 않았습니다)");
		// 묶음의 이름은 모두 원격 명령의 이름 검사를 지난다
		for (const Loadout& loadout : Loadouts())
			CHECK(GoodTraitName(loadout.Key) && ParseRemoteLine(std::string("person people equip name=") + loadout.Key).Error.empty());

		PersonRow row;
		row.Character = false;
		row.Strata = 2;
		CHECK(IsSoldier(row));
		row.Strata = 1;
		CHECK(!IsSoldier(row));
		row.Strata = 2;
		row.Character = true;
		CHECK(!IsSoldier(row));

		RemoteCommand c = ParseRemoteLine("person people equip name=h_swordman");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "people" && c.Options.at("act") == "equip" && c.Options.at("name") == "h_swordman");
		CHECK(ParseRemoteLine("person 25556c3312bce178 equip name=any").Error.empty());
		CHECK(!ParseRemoteLine("person lords equip name=h_swordman").Error.empty());
		CHECK(!ParseRemoteLine("person people equip name=wolf").Error.empty());
		CHECK(!ParseRemoteLine("person people equip").Error.empty());
	});

	Test("군대: 디버그 소환기의 종류와 원격 명령", [] {
		SpawnKind kind = SpawnKind::Soldier;
		CHECK(ParseSpawnKind("knight", kind) && kind == SpawnKind::Knight);
		CHECK(ParseSpawnKind("peasant", kind) && kind == SpawnKind::Peasant);
		CHECK(ParseSpawnKind("slave", kind) && ParseSpawnKind("lord", kind) && ParseSpawnKind("soldier", kind));
		CHECK(!ParseSpawnKind("bandit", kind) && !ParseSpawnKind("wolf", kind) && !ParseSpawnKind("", kind));		// 플레이어의 사람만 만든다
		CHECK(std::string(SpawnMethod(SpawnKind::Soldier)) == "__spawn_soldier" && std::string(SpawnMethod(SpawnKind::Lord)) == "__spawn_lord");
		CHECK(std::string(SpawnMethod(SpawnKind::Slave)) == "__spawn_slave");		// 소환기의 목록에는 "slaves" 지만 메서드는 __spawn_slave 다(research/13)
		CHECK(std::string(SpawnWord(SpawnKind::Peasant)) == "peasant");

		RemoteCommand c = ParseRemoteLine("person spawn knight");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "spawn" && c.Options.at("kind") == "knight");
		CHECK(!ParseRemoteLine("person spawn").Error.empty());
		CHECK(!ParseRemoteLine("person spawn bandit").Error.empty());
		CHECK(!ParseRemoteLine("person spawn knight 2").Error.empty());
	});

	Test("군대: 한 번에 만드는 병사의 수와 원격 명령", [] {
		int count = 0;
		CHECK(SoldierBatch(3, count) && count == 3);
		CHECK(SoldierBatch(1, count) && count == 1);
		CHECK(SoldierBatch(25, count) && count == 20);			// 한 번에 스무 명까지
		CHECK(SoldierBatch(2.6, count) && count == 3);
		CHECK(!SoldierBatch(0, count) && !SoldierBatch(-4, count) && !SoldierBatch(0.3, count));
		CHECK(!SoldierBatch(std::numeric_limits<double>::quiet_NaN(), count) && !SoldierBatch(std::numeric_limits<double>::infinity(), count));

		RemoteCommand c = ParseRemoteLine("person spawn_soldier amount=3");
		CHECK(c.Error.empty() && c.Verb == "person" && c.Target == "spawn_soldier" && c.Number == 3);
		CHECK(!ParseRemoteLine("person spawn_soldier").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=0").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=nan").Error.empty());
		CHECK(!ParseRemoteLine("person spawn_soldier amount=3 extra").Error.empty());
	});

	Test("인구: 켠 항목이 채워 둘 욕구의 번호", [] {
		CHECK(NeedsToHold(false, false, false, false).empty());
		CHECK(NeedsToHold(true, false, false, false) == std::vector<int>{ 1 });				// 음식
		CHECK((NeedsToHold(false, true, false, false) == std::vector<int>{ 0, 2 }));			// 수면, 휴식
		CHECK((NeedsToHold(true, true, false, false) == std::vector<int>{ 0, 1, 2 }));
		CHECK((NeedsToHold(false, false, true, false) == std::vector<int>{ 0, 1, 2, 3, 4, 5 }));
		CHECK((NeedsToHold(true, true, true, false) == std::vector<int>{ 0, 1, 2, 3, 4, 5 }));
		// 신앙심 채워 두기(종교 영역의 piety_full): 욕구 3번
		CHECK(NeedsToHold(false, false, false, true) == std::vector<int>{ 3 });
		CHECK((NeedsToHold(true, false, false, true) == std::vector<int>{ 1, 3 }));
		CHECK((NeedsToHold(false, true, false, true) == std::vector<int>{ 0, 2, 3 }));
		CHECK((NeedsToHold(false, false, true, true) == std::vector<int>{ 0, 1, 2, 3, 4, 5 }));
	});

	Test("인구: 한 틱에 다루는 사람의 수를 묶는다", [] {
		// 300명을 40명씩: 여덟 틱에 한 바퀴. 끝에 닿으면 다음은 처음부터다.
		PeopleSlice slice = NextPeopleSlice(300, 0, 40);
		CHECK(slice.Begin == 0 && slice.End == 40 && slice.Next == 40 && !slice.Wrapped);
		slice = NextPeopleSlice(300, 280, 40);
		CHECK(slice.Begin == 280 && slice.End == 300 && slice.Next == 0 && slice.Wrapped);
		// 사람이 줄어 자리가 끝을 넘었으면 처음부터 다시 한다.
		slice = NextPeopleSlice(20, 280, 40);
		CHECK(slice.Begin == 0 && slice.End == 20 && slice.Next == 0 && slice.Wrapped);
		slice = NextPeopleSlice(0, 5, 40);
		CHECK(slice.Begin == 0 && slice.End == 0 && slice.Next == 0 && slice.Wrapped);
		slice = NextPeopleSlice(10, 0, 0);		// 묶음이 0 이어도 멈추지 않는다(한 명씩)
		CHECK(slice.Begin == 0 && slice.End == 1);
	});
}
