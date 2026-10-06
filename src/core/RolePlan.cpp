#include "RolePlan.hpp"

#include "PeoplePlan.hpp"

#include <algorithm>
#include <cmath>

namespace NlCore
{
	namespace
	{
		int SkillIndex(const char* Key)
		{
			const std::vector<NamedKey>& skills = SkillNames();
			for (size_t i = 0; i < skills.size(); i++)
				if (std::string(skills[i].Key) == Key)
					return static_cast<int>(i);
			return -1;
		}

		bool Has(const std::vector<std::string>& List, const char* Name)
		{
			return std::find(List.begin(), List.end(), Name) != List.end();
		}
	}

	const std::vector<RolePreset>& RolePresets()
	{
		// 고른 근거는 게임의 설명 글이다(글은 여기 옮기지 않는다. 뜻만 적는다. 재능마다의 효과를 플레이에서 재지는 않았다. research/22).
		// 능력치가 쓰이는 곳: combat = 회피와 명중, command = 분대의 사기와 훈련, knowledge = 지식을 익히고 책을 쓰는 일, management = 생산 지시의 덤,
		// manners = 다른 영주와의 대화, negotiation = 대상단·이웃과의 거래, oratory = 뇌물·음모·설교, education = 가르칠 때 주는 경험.
		// 재능은 설명이 그 역할에 맞는 것을 골랐다. 통치자일 때만 듣는다고 적힌 넷(iron_fist, moral_ideal, respected, unifier)은 왕에게만 넣었다.
		// 넣지 않은 것: politic·intriguan·duelist(왕의 경쟁자나 위험한 손님에게 붙는 표식), religious(끝나지 않는 광신), saint(기한이 있는 축복),
		// savant(더 훈련할 수 없게 된다), genius_king(이웃 통치자를 미워하게 된다).
		// 떼는 것은 설명에서 해로움을 읽은 것만: nervous(신경 쇠약이 깊어진다. 모든 역할), coward·pacifist(싸우는 역할), stupidity(배우고 다스리는 역할),
		// contemptuous·sarcastic·cynic·envious·greedy(사람을 대하는 역할).
		static const std::vector<RolePreset> roles = {
			{ "king", "왕 / 통치자", { { "command", 20 }, { "oratory", 20 }, { "manners", 20 }, { "knowledge", 15 }, { "management", 15 } },
				{ "iron_fist", "moral_ideal", "respected", "unifier", "leader", "authority", "charisma", "calm" },
				{ "nervous", "coward", "greedy", "contemptuous", "stupidity" } },
			{ "steward", "최고급 내정 담당", { { "management", 20 }, { "knowledge", 20 }, { "negotiation", 15 }, { "oratory", 15 } },
				{ "strict_owner", "authority", "precise_language", "overseer", "harsh_judge", "admonisher" },
				{ "nervous", "stupidity" } },
			{ "scholar", "최고급 학자", { { "knowledge", 20 }, { "education", 20 }, { "manners", 15 } },
				{ "bookworm", "gifted", "enlightener", "intuitive_genius", "purposeful", "polyglot", "thirst_for_knowledge" },
				{ "nervous", "stupidity" } },
			{ "instructor", "최고급 교관", { { "combat", 20 }, { "command", 20 }, { "education", 20 } },
				{ "combat_teacher", "redeemer", "leader", "purposeful" },
				{ "nervous", "coward", "pacifist", "stupidity" } },
			{ "general", "최고급 장군", { { "command", 20 }, { "combat", 20 } },
				{ "leader", "terrifying", "shield_master", "spear_master", "accurate_archer", "protector", "fearless" },
				{ "nervous", "coward", "pacifist" } },
			{ "duelist", "결투 전문 영주", { { "combat", 20 }, { "command", 15 } },
				{ "duelist_talent", "cutter", "berserker", "fearless", "brave" },
				{ "nervous", "coward", "pacifist" } },
			{ "politician", "정치가", { { "oratory", 20 }, { "manners", 20 }, { "knowledge", 15 }, { "command", 15 } },
				{ "diplomat", "authority", "giver", "kindhearted", "flatterer", "charisma" },
				{ "nervous", "contemptuous", "sarcastic", "cynic", "greedy" } },
			{ "schemer", "음모 전문가", { { "oratory", 20 }, { "knowledge", 20 }, { "command", 15 } },
				{ "shadow_master", "guard_friend", "puppeteer", "forest_lord", "prince_of_thieves", "master_of_adultery", "charisma" },
				{ "nervous" } },
			{ "socialite", "관계 전문가", { { "manners", 20 }, { "oratory", 15 } },
				{ "kindhearted", "poet", "flatterer", "life_of_the_party", "love_singer", "musician", "groomed", "empath" },
				{ "nervous", "contemptuous", "sarcastic", "cynic", "envious", "greedy" } },
			{ "priest", "종교 담당", { { "oratory", 20 }, { "education", 15 }, { "manners", 15 } },
				{ "preacher", "diplomat", "inspirer", "sacrificer", "pious", "tolerant", "comforter" },
				{ "nervous" } },
			{ "trader", "무역 전문가", { { "negotiation", 20 }, { "knowledge", 15 }, { "oratory", 15 } },
				{ "honest_merchant", "salesman", "smuggler", "master_of_profit", "friend_of_scriptonics", "forecaster", "giver" },
				{ "nervous" } },
			{ "producer", "생산 관리자", { { "management", 20 }, { "knowledge", 15 } },
				{ "overseer", "strict_owner", "farmer", "armorer", "chemist", "mining_expert", "builder", "authority" },
				{ "nervous" } },
			{ "teacher", "교육 담당", { { "education", 20 }, { "knowledge", 15 }, { "manners", 15 } },
				{ "thirst_for_knowledge", "enlightener", "intuitive_genius", "polyglot", "purposeful", "bookworm", "admonisher" },
				{ "nervous", "stupidity" } },
		};
		return roles;
	}

	const RolePreset* FindRole(const std::string& Id)
	{
		for (const RolePreset& role : RolePresets())
			if (Id == role.Id)
				return &role;
		return nullptr;
	}

	bool CheckRoles(std::string& Why)
	{
		Why.clear();
		std::vector<std::string> ids;
		for (const RolePreset& role : RolePresets())
		{
			const std::string id = role.Id;
			if (id.empty() || !GoodTraitName(id) || std::find(ids.begin(), ids.end(), id) != ids.end())
				Why = "프리셋의 이름이 비었거나 겹칩니다: " + id;
			ids.push_back(id);
			std::vector<int> seen;
			for (const RoleSkill& skill : role.Skills)
			{
				const int index = SkillIndex(skill.Skill);
				if (index < 0 || skill.Level < 1 || skill.Level > k_SkillMax || std::find(seen.begin(), seen.end(), index) != seen.end())
					Why = id + ": 능력치가 틀렸습니다 (" + skill.Skill + ")";
				seen.push_back(index);
			}
			std::vector<std::string> names;
			for (const std::vector<const char*>* list : { &role.Add, &role.Remove })
				for (const char* name : *list)
				{
					// 붙일 것과 뗄 것을 한 목록으로 본다: 한 프리셋 안에서 같은 이름이 두 번 나오면 틀린 표다.
					if (!GoodTraitName(name) || IsProtectedTrait(name) || std::find(names.begin(), names.end(), name) != names.end())
						Why = id + ": 특성이 틀렸습니다 (" + name + ")";
					names.push_back(name);
				}
			if (!Why.empty())
				return false;
		}
		return true;
	}

	RoleTodo PlanRole(const RolePreset& Role, const std::vector<double>& Skills, const std::vector<std::string>& Traits)
	{
		RoleTodo todo;
		for (const RoleSkill& skill : Role.Skills)
		{
			const int index = SkillIndex(skill.Skill);
			if (index < 0 || static_cast<size_t>(index) >= Skills.size())
				continue;
			const double now = Skills[index];
			// 읽은 능력치(0 이상의 수)만, 지금 값이 더 낮을 때만 올린다.
			if (std::isfinite(now) && now >= 0 && now < skill.Level)
				todo.Skills.emplace_back(index, skill.Level);
		}
		for (const char* name : Role.Add)
			if (!Has(Traits, name))
				todo.Add.push_back(name);
		for (const char* name : Role.Remove)
			if (Has(Traits, name))
				todo.Remove.push_back(name);
		return todo;
	}

	std::vector<RoleStep> RoleSteps(const RoleTodo& Todo)
	{
		std::vector<RoleStep> steps;
		for (const auto& [index, level] : Todo.Skills)
			steps.push_back({ 's', index, level, {} });
		for (const std::string& name : Todo.Remove)
			steps.push_back({ 'r', -1, 0, name });
		for (const std::string& name : Todo.Add)
			steps.push_back({ 'a', -1, 0, name });
		return steps;
	}

	bool RoleStepNeeded(const RoleStep& Step, const std::vector<std::string>& TraitsNow)
	{
		if (Step.Kind == 's')
			return true;
		const bool has = std::find(TraitsNow.begin(), TraitsNow.end(), Step.Name) != TraitsNow.end();
		return Step.Kind == 'r' ? has : !has;
	}

	std::string RolePreview(const RoleTodo& Todo)
	{
		if (Todo.Empty())
			return "바꿀 것이 없습니다 (이미 이 프리셋대로입니다)";
		const bool removes = !Todo.Remove.empty(), adds = !Todo.Add.empty();
		std::string text;
		if (!Todo.Skills.empty())
			text = "능력치 " + std::to_string(Todo.Skills.size()) + "개를 " + (removes || adds ? "올리고, " : "올립니다");
		if (removes)
			text += "특성 " + std::to_string(Todo.Remove.size()) + "개를 " + (adds ? "떼고, " : "뗍니다");
		if (adds)
			text += std::string(removes ? "" : "특성 ") + std::to_string(Todo.Add.size()) + "개를 붙입니다";
		return text;
	}

	const char* RemoveNote(const std::string& Trait)
	{
		// 한 영주에게서 그것만 떼자 생각의 합이 74.94 에서 49.94 가 됐고 다시 붙이자 돌아왔다(research/22).
		// 다른 특성은 따로 재지 않았다(greedy 를 붙였다 뗀 사람은 그대로였다).
		return Trait == "stupidity" ? "떼면 그 사람의 생각의 합이 25 내려갑니다 (잰 것. 다시 붙이면 돌아옵니다)" : "";
	}

	std::string RoleReport(const std::string& Name, const RolePreset& Role, int Skills, int Added, int Removed, int Failed, const std::string& Why)
	{
		const std::string head = Name + ": " + Role.Label + " 프리셋 - ";
		const std::string failed = std::to_string(Failed) + "개" + (Why.empty() ? "" : " (" + Why + ")");
		// 한 것을 잇는다: 마지막 것만 "…했습니다"로 끝난다.
		struct Part
		{
			std::string And, End;
		};
		std::vector<Part> parts;
		if (Skills > 0)
			parts.push_back({ "능력치 " + std::to_string(Skills) + "개를 올리고", "능력치 " + std::to_string(Skills) + "개를 올렸습니다" });
		if (Added > 0)
			parts.push_back({ "특성 " + std::to_string(Added) + "개를 붙이고", "특성 " + std::to_string(Added) + "개를 붙였습니다" });
		if (Removed > 0)
		{
			const std::string what = std::string(Added > 0 ? "" : "특성 ") + std::to_string(Removed) + "개를 ";
			parts.push_back({ what + "떼고", what + "뗐습니다" });
		}
		if (parts.empty())
			return head + (Failed > 0 ? "하지 못했습니다: " + failed : std::string("이미 그대로입니다 (바꾼 것이 없습니다)"));
		std::string text;
		for (size_t i = 0; i < parts.size(); i++)
			text += (i ? ", " : "") + (i + 1 < parts.size() ? parts[i].And : parts[i].End);
		return head + text + (Failed > 0 ? ". 하지 못한 것 " + failed : "");
	}
}
