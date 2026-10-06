#include "common.hpp"

void RunRoleTests()
{
	Test("인물의 역할 프리셋: 표, 할 일, 결과의 글", [] {
		// 표가 말이 된다(Id, 능력치의 열쇠와 수, 특성의 이름, 붙일 것과 뗄 것이 겹치지 않는다)
		std::string why;
		CHECK(CheckRoles(why) && why.empty());
		const auto& roles = RolePresets();
		CHECK(roles.size() == 13);
		for (const char* id : { "king", "steward", "scholar", "instructor", "general", "duelist", "politician", "schemer", "socialite", "priest", "trader", "producer", "teacher" })
			CHECK(FindRole(id) != nullptr);
		CHECK(!FindRole("") && !FindRole("nope") && !FindRole("General"));
		const auto has = [](const std::vector<const char*>& list, const char* name) {
			return std::find_if(list.begin(), list.end(), [&](const char* item) { return std::string(item) == name; }) != list.end();
		};
		for (const RolePreset& role : roles)
		{
			CHECK(!role.Skills.empty() && !role.Add.empty() && std::string(role.Label).size() > 0);
			// 사용자가 정한 것: 능력치는 20 이나 15 로만 올린다
			for (const RoleSkill& skill : role.Skills)
				CHECK(skill.Level == 20 || skill.Level == 15);
			// 넣지 않기로 한 특성(까닭은 RolePlan.cpp 의 주석에 게임의 이름으로 적었다)
			for (const char* banned : { "politic", "intriguan", "duelist", "religious", "saint", "savant", "genius_king", "religiosity_fanatic" })
				CHECK(!has(role.Add, banned));
			// nervous 는 어느 역할에서든 뗀다
			CHECK(has(role.Remove, "nervous"));
		}
		// 몇 개를 짚어 본다(사용자에게 보인 표 그대로)
		const RolePreset* general = FindRole("general");
		CHECK(general && general->Skills.size() == 2 && has(general->Add, "leader") && has(general->Add, "terrifying") && has(general->Add, "fearless")
			&& has(general->Remove, "coward") && has(general->Remove, "pacifist"));
		const RolePreset* king = FindRole("king");
		CHECK(king && has(king->Add, "iron_fist") && has(king->Add, "moral_ideal") && has(king->Add, "respected") && has(king->Add, "unifier") && has(king->Add, "calm"));
		const RolePreset* scholar = FindRole("scholar");
		CHECK(scholar && has(scholar->Add, "bookworm") && has(scholar->Add, "gifted") && has(scholar->Remove, "stupidity"));
		CHECK(FindRole("priest") && has(FindRole("priest")->Add, "preacher") && FindRole("trader") && has(FindRole("trader")->Add, "honest_merchant"));
		// 사용자에게 보인 표의 크기 그대로(붙일 것, 뗄 것). 표를 고치면 이 수도 함께 고친다.
		struct Size
		{
			const char* Id;
			size_t Add, Remove;
		};
		for (const Size& size : { Size{ "king", 8, 5 }, Size{ "steward", 6, 2 }, Size{ "scholar", 7, 2 }, Size{ "instructor", 4, 4 }, Size{ "general", 7, 3 },
				Size{ "duelist", 5, 3 }, Size{ "politician", 6, 5 }, Size{ "schemer", 7, 1 }, Size{ "socialite", 8, 6 }, Size{ "priest", 7, 1 },
				Size{ "trader", 7, 1 }, Size{ "producer", 8, 1 }, Size{ "teacher", 7, 2 } })
		{
			const RolePreset* role = FindRole(size.Id);
			CHECK(role && role->Add.size() == size.Add && role->Remove.size() == size.Remove);
		}
		// 배우고 다스리는 역할은 stupidity 를 뗀다(왕, 내정, 학자, 교관, 교육). 싸우는 역할(장군, 결투)은 떼지 않는다.
		for (const char* id : { "king", "steward", "scholar", "instructor", "teacher" })
			CHECK(has(FindRole(id)->Remove, "stupidity"));
		for (const char* id : { "general", "duelist", "schemer", "priest", "trader", "producer", "politician", "socialite" })
			CHECK(!has(FindRole(id)->Remove, "stupidity"));
		CHECK(has(king->Remove, "coward") && has(king->Remove, "greedy") && has(king->Remove, "contemptuous"));

		// 능력치의 화면 이름은 게임의 현지화 파일에서 읽는다(열쇠 "actor.skill.<능력치의 열쇠>"). 글은 지어낸 것이다.
		// 전투만 열쇠의 이름이 다르다(게임 파일의 줄은 actor.skill.fight 다. 나머지 일곱은 능력치의 열쇠 그대로다)
		CHECK_STR(SkillCaptionKey("combat"), "actor.skill.fight");
		CHECK_STR(SkillCaptionKey("oratory"), "actor.skill.oratory");
		CHECK_STR(SkillCaptionKey(""), "actor.skill.");
		{
			std::unordered_map<std::string, std::string> rows;
			std::string csv_why;
			const std::string csv = "Key,English,Korean\r\nactor.skill.fight,Brawling,싸움\r\nactor.skill.oratory,Talking,\r\ntrait.leader,Boss,우두머리\r\n";
			CHECK(ReadLocalization(csv, SkillCaptionKey(""), { "Korean", "English" }, rows, csv_why));
			CHECK(rows.size() == 2 && rows[SkillCaptionKey("combat")] == "싸움" && rows[SkillCaptionKey("oratory")] == "Talking");
		}

		// 할 일: 능력치는 올리기만(이미 더 높은 것은 그대로), 가진 특성은 붙이지 않고, 없는 특성은 떼지 않는다
		const std::vector<NamedKey>& skills = SkillNames();
		const auto at = [&](const char* key) {
			for (size_t i = 0; i < skills.size(); i++)
				if (std::string(skills[i].Key) == key)
					return static_cast<int>(i);
			return -1;
		};
		{
			// combat 5, command 20(이미 20), 나머지 3
			std::vector<double> now(skills.size(), 3);
			now[at("combat")] = 5;
			now[at("command")] = 20;
			const RoleTodo todo = PlanRole(*general, now, { "human", "leader", "coward", "stupidity" });
			CHECK(todo.Skills.size() == 1 && todo.Skills[0].first == at("combat") && todo.Skills[0].second == 20);
			CHECK(std::find(todo.Add.begin(), todo.Add.end(), "leader") == todo.Add.end());		// 이미 있다
			CHECK(std::find(todo.Add.begin(), todo.Add.end(), "terrifying") != todo.Add.end());
			CHECK(todo.Add.size() == general->Add.size() - 1);
			CHECK(todo.Remove.size() == 1 && todo.Remove[0] == "coward");		// 가진 것만 뗀다(stupidity 는 장군의 표에 없다)
			CHECK(!todo.Empty());
		}
		{
			// 더 높은 능력치를 깎지 않는다: 15 로 올리는 능력치가 18 이면 그대로
			std::vector<double> now(skills.size(), 18);
			const RoleTodo todo = PlanRole(*king, now, {});
			for (const auto& [index, level] : todo.Skills)
				CHECK(level == 20);		// 15 짜리는 빠지고 20 짜리만 남는다
			CHECK(todo.Skills.size() == 3);
		}
		{
			// 읽지 못한 능력치(주민에게는 전투만 있다)는 건드리지 않는다. 모자란 칸도.
			std::vector<double> now(skills.size(), std::numeric_limits<double>::quiet_NaN());
			now[at("combat")] = 2;
			RoleTodo todo = PlanRole(*general, now, {});
			CHECK(todo.Skills.size() == 1 && todo.Skills[0].first == at("combat"));
			todo = PlanRole(*general, {}, {});
			CHECK(todo.Skills.empty() && todo.Add.size() == general->Add.size());
			std::vector<double> negative(skills.size(), -4);
			CHECK(PlanRole(*general, negative, {}).Skills.empty());
		}
		{
			// 이미 그 프리셋대로면 할 일이 없다
			std::vector<double> now(skills.size(), 20);
			std::vector<std::string> traits;
			for (const char* name : general->Add)
				traits.push_back(name);
			CHECK(PlanRole(*general, now, traits).Empty());
		}

		// 결과의 글
		CHECK_STR(RoleReport("Barra", *general, 2, 7, 1, 0, ""), "Barra: 최고급 장군 프리셋 - 능력치 2개를 올리고, 특성 7개를 붙이고, 1개를 뗐습니다");
		CHECK_STR(RoleReport("Barra", *general, 0, 0, 0, 0, ""), "Barra: 최고급 장군 프리셋 - 이미 그대로입니다 (바꾼 것이 없습니다)");
		CHECK_STR(RoleReport("Barra", *general, 2, 6, 0, 1, "게임이 붙이지 않았습니다"),
			"Barra: 최고급 장군 프리셋 - 능력치 2개를 올리고, 특성 6개를 붙였습니다. 하지 못한 것 1개 (게임이 붙이지 않았습니다)");
		CHECK_STR(RoleReport("Barra", *general, 0, 0, 0, 3, "그 능력치가 없습니다"), "Barra: 최고급 장군 프리셋 - 하지 못했습니다: 3개 (그 능력치가 없습니다)");
		CHECK_STR(RoleReport("Barra", *general, 0, 0, 1, 0, ""), "Barra: 최고급 장군 프리셋 - 특성 1개를 뗐습니다");
		CHECK_STR(RoleReport("Barra", *general, 2, 0, 1, 0, ""), "Barra: 최고급 장군 프리셋 - 능력치 2개를 올리고, 특성 1개를 뗐습니다");
		CHECK_STR(RoleReport("Barra", *general, 0, 3, 0, 0, ""), "Barra: 최고급 장군 프리셋 - 특성 3개를 붙였습니다");
		CHECK_STR(RoleReport("Barra", *general, 0, 0, 0, 2, ""), "Barra: 최고급 장군 프리셋 - 하지 못했습니다: 2개");

		// 걸음의 차례: 능력치, 떼기, 붙이기. 해로운 특성을 먼저 뗀다
		// (그것을 가진 사람에게 맞서는 재능을 게임이 붙여 주는지는 재지 않았다. 어느 쪽이든 먼저 떼는 것이 낫다).
		{
			RoleTodo todo;
			todo.Skills = { { 1, 20 }, { 0, 15 } };
			todo.Add = { "leader", "fearless" };
			todo.Remove = { "coward" };
			const std::vector<RoleStep> steps = RoleSteps(todo);
			std::string kinds;
			for (const RoleStep& step : steps)
				kinds += step.Kind;
			CHECK_STR(kinds, "ssraa");
			CHECK(steps.size() == 5 && steps[0].Index == 1 && steps[0].Level == 20 && steps[1].Index == 0 && steps[1].Level == 15);
			CHECK(steps[2].Name == "coward" && steps[3].Name == "leader" && steps[4].Name == "fearless");
			CHECK(RoleSteps(RoleTodo()).empty());
			// 걸음 바로 앞에 다시 읽은 특성으로 정한다: 이미 없는 것은 떼지 않고, 이미 있는 것은 붙이지 않는다(그런 상태에서 게임의 함수를 불러 본 적이 없다).
			CHECK(RoleStepNeeded(steps[2], { "human", "coward" }) && !RoleStepNeeded(steps[2], { "human" }) && !RoleStepNeeded(steps[2], {}));
			CHECK(RoleStepNeeded(steps[3], { "human" }) && !RoleStepNeeded(steps[3], { "human", "leader" }));
			CHECK(RoleStepNeeded(steps[0], {}) && RoleStepNeeded(steps[0], { "leader" }));
		}

		// 미리 보기의 요약(누르기 전의 글): 걸음의 차례대로 말한다(올리고, 떼고, 붙인다)
		{
			RoleTodo todo;
			CHECK_STR(RolePreview(todo), "바꿀 것이 없습니다 (이미 이 프리셋대로입니다)");
			todo.Skills = { { 1, 20 }, { 0, 15 } };
			CHECK_STR(RolePreview(todo), "능력치 2개를 올립니다");
			todo.Remove = { "coward" };
			CHECK_STR(RolePreview(todo), "능력치 2개를 올리고, 특성 1개를 뗍니다");
			todo.Add = { "leader", "fearless", "brave" };
			CHECK_STR(RolePreview(todo), "능력치 2개를 올리고, 특성 1개를 떼고, 3개를 붙입니다");
			todo.Remove.clear();
			CHECK_STR(RolePreview(todo), "능력치 2개를 올리고, 특성 3개를 붙입니다");
			todo.Skills.clear();
			CHECK_STR(RolePreview(todo), "특성 3개를 붙입니다");
			todo.Add.clear();
			todo.Remove = { "coward", "nervous" };
			CHECK_STR(RolePreview(todo), "특성 2개를 뗍니다");
		}
		// 떼기 전에 알릴 것은 잰 것만: stupidity 를 떼면 생각의 합이 25 내려간다(research/22). 재지 않은 특성에는 아무 말도 하지 않는다.
		CHECK(std::string(RemoveNote("stupidity")).find("25") != std::string::npos);
		CHECK(std::string(RemoveNote("greedy")).empty() && std::string(RemoveNote("nervous")).empty() && std::string(RemoveNote("")).empty());

		// 명령: 한 사람을 짚어서만. 이름은 프리셋의 Id.
		PersonAct act = PersonAct::SkillSet;
		CHECK(ParsePersonAct("role", act) && act == PersonAct::Role && std::string(PersonActWord(PersonAct::Role)) == "role");
		CHECK(NeedsText(PersonAct::Role) && !NeedsAmount(PersonAct::Role) && !NeedsIndex(PersonAct::Role));
		PersonCommand c;
		c.Act = PersonAct::Role;
		c.Who = "25556c3312bce178";
		c.Text = "general";
		CHECK(CheckPersonCommand(c, why) && why.empty());
		c.Text = "nope";
		CHECK(!CheckPersonCommand(c, why) && !why.empty());
		c.Text = "general";
		c.Who = "lords";
		CHECK(!CheckPersonCommand(c, why));
		CHECK(!BulkAllowed("lords", PersonAct::Role) && !BulkAllowed("people", PersonAct::Role) && BulkAllowed("25556c3312bce178", PersonAct::Role));
		const RemoteCommand line = ParseRemoteLine("person 25556c3312bce178 role name=general");
		CHECK(line.Error.empty() && line.Options.at("act") == "role" && line.Options.at("name") == "general");
		CHECK(ParseRemoteLine("person 25556c3312bce178 role name=nope").Error.find("general") != std::string::npos);
		CHECK(!ParseRemoteLine("person lords role name=general").Error.empty() && !ParseRemoteLine("person 25556c3312bce178 role name=nope").Error.empty()
			&& !ParseRemoteLine("person 25556c3312bce178 role").Error.empty());
	});
}
