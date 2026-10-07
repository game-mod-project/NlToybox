#include "PeopleInternal.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "PeopleAccess.hpp"
#include "Shield.hpp"
#include "TraitText.hpp"
#include "core/AskPath.hpp"
#include "core/EconomyPlan.hpp"
#include "core/FamilyPlan.hpp"
#include "core/RolePlan.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>

using namespace YYTK;
using namespace NlPeopleAccess;
using NlCore::PathStep;
using NlCore::PersonAct;
using NlCore::PersonCommand;
using NlCore::PersonRow;
using NlCore::Shortest;

// 한 사람에게 하는 일(One*), 실행과 집계(Execute·Run), 역할 프리셋의 입히기·되돌리기, 소환. 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다.
namespace NlPeople::Internal
{
	struct RoleCount
	{
		int Skills = 0, Added = 0, Removed = 0, Failed = 0;
		std::string Why;
	};

	// 역할 프리셋의 걸음들을 한다(입히기와 되돌리기가 같은 길. 차례는 core 의 RoleSteps). 쓰는 길은 한 사람 명령의 것과 같다
	// (능력치: 있는 칸에 쓰고 다시 읽기, 특성: trait_detach·trait_attach 뒤 목록을 다시 읽기).
	// 걸음마다 그 자리의 사람이 그대로인지 보고, 특성의 걸음은 바로 앞에 목록을 다시 읽어 아직 할 일인지 본다(없는 것을 떼거나 있는 것을 붙이려 부르지 않는다).
	// Memory 가 있으면 된 것을 거기에 적는다(입힐 때: 능력치의 전 값은 LevelsBefore 에서, 뗀 것, 붙인 것). 되돌리기는 nullptr.
	void RunRoleSteps(const std::vector<NlCore::RoleStep>& Steps, const std::vector<double>& LevelsBefore, const PersonRow& Row, RValue& Soul, const std::string& What,
		RoleCount& Count, NlCore::RoleMemory* Memory)
	{
		const std::string soul = Base(Row) + ".__soul";
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		const auto fail = [&](const std::string& what, const std::string& note) {
			Count.Failed++;
			const std::string text = what + ": " + (note.empty() ? "하지 못했습니다" : note);
			if (Count.Why.empty())
				Count.Why = text;
			Log("people: " + What + " on " + Row.Uuid + " could not do " + text);
		};
		std::string plan;
		for (const NlCore::RoleStep& step : Steps)
			plan += std::string(" ") + step.Kind + ":" + (step.Kind == 's' ? std::string(skills[step.Index].Key) + "=" + std::to_string(step.Level) : step.Name);
		Log("people: " + What + " on " + Row.Uuid + " steps:" + (plan.empty() ? " none" : plan));		// 부르기 전에 남긴다

		std::vector<std::string> traits;
		bool there = true;		// 그 자리의 사람이 그대로다. 아니게 되면 더 쓰지도 부르지도 않는다
		for (const NlCore::RoleStep& step : Steps)
		{
			const std::string what = step.Kind == 's' ? std::string(skills[step.Index].Key) : step.Name;
			std::string note;
			if (!there)
			{
				fail(what, "그 자리의 사람이 바뀌었습니다");
				continue;
			}
			if (step.Kind == 's')
			{
				if (!WriteAt(soul + ".__skills.__level." + skills[step.Index].Key, step.Level, note))
				{
					fail(what, note);
					continue;
				}
				Count.Skills++;
				const double before = static_cast<size_t>(step.Index) < LevelsBefore.size() ? LevelsBefore[step.Index] : k_Unknown;
				if (Memory && std::isfinite(before) && before >= 0)
					Memory->Skills.emplace_back(step.Index, static_cast<int>(std::llround(before)));
				continue;
			}
			if (!ReadTraits(Soul, traits))
			{
				fail(what, "그 사람의 특성을 읽지 못했습니다");
				continue;
			}
			if (!NlCore::RoleStepNeeded(step, traits))
				continue;		// 앞의 걸음이나 게임이 이미 뗐거나 붙였다. 부르지 않는다
			if (step.Kind == 'a')
			{
				if (!NlTraitText::Known(step.Name))
					fail(what, "게임에 없는 특성입니다");
				else if (Attach(Row, step.Name, Soul, note))
				{
					Count.Added++;
					if (Memory)
						Memory->Added.push_back(step.Name);
				}
				else
				{
					fail(what, note);
					there = StillThere(Row, Soul);
				}
				continue;
			}
			const bool called = Detach(Row, step.Name, note);
			there = StillThere(Row, Soul);
			std::vector<std::string> after;
			const bool read = there && ReadTraits(Soul, after);
			if (called && read && !NlCore::Has(after, step.Name))
			{
				Count.Removed++;
				if (Memory)
					Memory->Removed.push_back(step.Name);
			}
			else
				fail(what, !there ? "그 자리의 사람이 바뀌었습니다" : !called ? note : !read ? "그 사람의 특성을 읽지 못했습니다" : "게임이 떼지 않았습니다");
		}
	}

	// 역할 프리셋(core/RolePlan)을 한 사람에게 입힌다: 능력치는 올리기만 하고, 그 역할에 해로운 특성을 떼고, 재능을 붙인다(차례는 core 의 RoleSteps).
	// 된 것은 그 사람의 "전"으로 기억해 둔다(g_RoleMemory. 되돌리기가 쓴다. 두 번 입히면 더 앞의 전 값을 지킨다: core 의 MergeRoleMemory).
	// Note 는 결과의 글이다(RoleReport: 이름, 한 것, 하지 못한 것).
	void ApplyRole(const NlCore::RolePreset& Role, const PersonRow& Row, RValue& Soul, std::string& Note)
	{
		const std::string soul = Base(Row) + ".__soul";
		std::vector<double> levels;
		for (const NlCore::NamedKey& skill : NlCore::SkillNames())
		{
			double level = k_Unknown;
			if (!NlAccess::ReadNumber(soul + ".__skills.__level." + skill.Key, level))
				level = k_Unknown;		// 그 사람에게 없는 능력치(주민은 전투만 있다). 계획이 건너뛴다
			levels.push_back(level);
		}
		std::vector<std::string> traits;
		if (!ReadTraits(Soul, traits))
		{
			// 읽지 못한 목록을 빈 목록으로 보면 이미 가진 특성에 붙이기를 부르게 된다. 아무것도 하지 않는다.
			const std::string why = "특성: 그 사람의 특성을 읽지 못했습니다";
			Log(std::string("people: role ") + Role.Id + " on " + Row.Uuid + " could not do " + why);
			Note = NlCore::RoleReport(Row.Name, Role, 0, 0, 0, 1, why);
			return;
		}
		RoleCount count;
		NlCore::RoleMemory memory;
		RunRoleSteps(NlCore::RoleSteps(NlCore::PlanRole(Role, levels, traits)), levels, Row, Soul, std::string("role ") + Role.Id, count, &memory);
		if (!memory.Empty())
			NlCore::MergeRoleMemory(g_RoleMemory[Row.Uuid], memory);
		Note = NlCore::RoleReport(Row.Name, Role, count.Skills, count.Added, count.Removed, count.Failed, count.Why);
	}

	// 이 실행에서 입힌 역할 프리셋을 되돌린다(core 의 PlanRoleUndo): 능력치는 기억한 전 값으로(이때만 내린다), 붙인 특성을 떼고 뗀 특성을 다시 붙인다.
	// 다 되면 기억을 지운다(일부만 되면 남겨 다시 할 수 있게). Note 는 결과의 글(RoleUndoReport). 기억이 없으면 거짓.
	bool OneRoleUndo(const PersonCommand&, const PersonRow& Row, const std::string&, RValue& soul_value, std::string& Note)
	{
		const auto memory = g_RoleMemory.find(Row.Uuid);
		if (memory == g_RoleMemory.end() || memory->second.Empty())
		{
			Note = NlCore::RoleUndoNoMemory(Row.Name);
			return false;
		}
		std::vector<std::string> traits;
		if (!ReadTraits(soul_value, traits))
		{
			Note = NlCore::RoleUndoReport(Row.Name, 0, 0, 0, 1, "특성: 그 사람의 특성을 읽지 못했습니다");
			return false;
		}
		RoleCount count;
		RunRoleSteps(NlCore::RoleSteps(NlCore::PlanRoleUndo(memory->second, traits)), {}, Row, soul_value, "role undo", count, nullptr);
		if (count.Failed == 0)
			g_RoleMemory.erase(memory);
		Note = NlCore::RoleUndoReport(Row.Name, count.Skills, count.Removed, count.Added, count.Failed, count.Why);
		return true;
	}

	// ---- 한 사람에게 하는 일 하나씩. 공통의 인자: 명령, 사람(사본), 영혼의 주소(soul), 영혼의 RValue(soul_value. StillThere 가 채웠다), 결과의 글(Note) ----

	bool OneSkill(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		const std::string path = soul + ".__skills.__level." + skills[C.Index].Key;
		double current = 0, value = 0;
		if (!NlAccess::ReadNumber(path, current))
		{
			Note = "그 능력치가 없습니다";		// 주민은 전투만 있다. 없는 것을 만들지 않는다
			return false;
		}
		if (!(C.Act == PersonAct::SkillSet ? NlCore::SkillValue(C.Amount, value) : NlCore::SkillAfterAdd(current, C.Amount, value)))
			return false;
		return WriteAt(path, value, Note);
	}

	bool OneSkillsMax(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		size_t written = 0;
		for (const NlCore::NamedKey& skill : skills)
		{
			const std::string path = soul + ".__skills.__level." + skill.Key;
			double current = 0;
			if (NlAccess::ReadNumber(path, current) && WriteAt(path, NlCore::k_SkillMax, Note))
				written++;
		}
		Note = written ? "" : "쓸 능력치가 없습니다";
		return written > 0;
	}

	bool OneNeedsFill(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		size_t written = 0;
		for (int i = 0; i < static_cast<int>(NlCore::NeedNames().size()); i++)
			written += SetNeed(soul, i, NlCore::k_NeedMax * 100, Note);		// 상한까지
		if (written)
			Note.clear();
		return written > 0;
	}

	bool OneAgeSet(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		double age = 0, after = k_Unknown;
		if (!NlCore::AgeValue(C.Amount, age))
			return false;
		// SoulBasic.set_age(나이): 게임이 수 하나로 부르는 것을 기록했다. 30 으로 부르자 get_age 가 30, __born 이 -76 → -56 이 됐다.
		Log("people call set_age(" + Shortest(age) + ") on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".set_age"), { RValue(age) }, result, Note))
			return false;
		if (!CallNumber(soul + ".get_age", after) || after != age)
		{
			Note = "나이가 " + (after == k_Unknown ? std::string("읽히지 않습니다") : Shortest(after) + " 입니다");
			return false;
		}
		return true;
	}

	bool OneHappy(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note, bool Bulk)
	{
		RValue result;		// 이 함수 안에서만 든다
		double sum = 0;
		const bool read = CallNumber(soul + ".__minds.get_total_modify", sum);
		if (!NlCore::ShouldAttachHappy(read, sum, Bulk))
		{
			Note = read ? "이미 생각의 합이 100 을 넘습니다" : "생각의 합을 읽지 못했습니다";
			return true;
		}
		RValue mind;
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(k_HappyMind), mind, why) || !mind.IsStruct())
		{
			Note = "게임의 행복 생각을 찾지 못했습니다";
			return false;
		}
		Log("people call attach_generic_mind(mind_debug_totally_happy) on " + Row.Uuid);
		return NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__minds.attach_generic_mind"), { mind }, result, Note);
	}

	bool OneCure(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		// Traits.cure_all_disease(), cure_bleeding(): 게임이 인자 없이 부르는 것을 기록했다. 부상은 특성이라 이름으로 뗀다.
		Log("people call cure_all_disease(), cure_bleeding() on " + Row.Uuid);
		if (!CallNoArgs(soul + ".__traits.cure_all_disease", result, Note) || !CallNoArgs(soul + ".__traits.cure_bleeding", result, Note))
			return false;
		std::vector<std::string> traits;
		ReadTraits(soul_value, traits);
		size_t removed = 0;
		for (const char* wound : NlCore::WoundTraits())
			if (NlCore::Has(traits, wound) && Detach(Row, wound, Note))
				removed++;
		Note = removed ? "부상 " + std::to_string(removed) + "개를 뗐습니다" : "";
		return true;
	}

	bool OneTraitAdd(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		std::vector<std::string> traits;
		ReadTraits(soul_value, traits);
		if (NlCore::Has(traits, C.Text))
		{
			Note = "이미 있는 특성입니다";
			return true;
		}
		return Attach(Row, C.Text, soul_value, Note);
	}

	bool OnePregnancy(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		// 임신의 다음 단계: ComponentPregnancy.debug_pregnancy_next_stage()(인자 없음: 본문이 argc 를 옮기지 않는다. research/24).
		// 임신 1/3기의 영주에게 세 번 불러 2/3기, 3/3기, 출산(아이가 생기고 어머니에게 pregnant_forbid 가 붙는다)까지 가는 것을 봤다.
		// 한 틱에 두 번 잇달아 불러도 됐다. 임신 중이 아닌 사람에게는 부르지 않는다(그런 상태에서 불러 본 적이 없다).
		// 부를 때마다 특성을 다시 읽어 단계가 본 대로 바뀌었는지 본다. 이 함수는 게임의 확률을 그대로 탄다: 유산으로 끝날 수 있다(그때는 아이가 생기지 않는다).
		if (!NlCore::IsPlayersLord(Row))
		{
			Note = Row.Name + ": 플레이어의 영주에게서만 쟀습니다 (주민·손님·다른 진영에게는 부르지 않습니다)";
			return false;
		}
		std::vector<std::string> traits;
		if (!ReadTraits(soul_value, traits))
		{
			Note = Row.Name + ": 특성을 읽지 못했습니다";
			return false;
		}
		const bool birth = C.Act == PersonAct::Birth;
		const int first = NlCore::PregnancyStage(traits);
		const int limit = birth ? NlCore::k_StageCallsMax : 1;
		int stage = first, calls = 0, children = 0;
		std::string why;
		// 아이가 생겼는지는 사람(o_character, o_dummy)의 수로 본다: 임신이 끝났어도 유산이면 늘지 않는다.
		const auto people = [] { return NlAccess::InstanceCount("o_character") + NlAccess::InstanceCount("o_dummy"); };
		while (calls < limit && NlCore::BirthNeedsCall(stage, calls))
		{
			const int before = people();
			Log("people call debug_pregnancy_next_stage() on " + Row.Uuid + " (stage " + std::to_string(stage) + ")");		// 부르기 전에 남긴다
			if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__pregnancy.debug_pregnancy_next_stage"), {}, result, why))
				break;
			calls++;
			const int born = people() - before;
			children += born > 0 ? born : 0;
			if (!StillThere(Row, soul_value) || !ReadTraits(soul_value, traits))
			{
				stage = -1;		// 불렀지만 뒤의 단계를 모른다
				why = "그 사람을 다시 읽지 못했습니다";
				break;
			}
			const int after = NlCore::PregnancyStage(traits);
			const bool stuck = NlCore::AfterStageCall(stage, after) == NlCore::StageOutcome::Stuck;
			if (stuck && after != stage)
				why = "본 적 없는 바뀜: " + std::to_string(stage) + " -> " + std::to_string(after);
			stage = after;
			if (stuck)
				break;
		}
		Note = NlCore::StageReport(Row.Name, birth, first, stage, calls, why, children);
		return birth ? first > 0 && NlCore::BirthDone(stage, children, why) : NlCore::NextDone(first, stage, calls, why);
	}

	bool OneGrowUp(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		// 아이를 어른으로: 나이를 18 로 맞춘다(SoulBasic.set_age. 15, 16 에서는 아이 그대로였고 18 에서 게임이 kid 를 떼고 untitled_lord 를 붙였다.
		// 진영은 player_untitled 가 됐다. research/24).
		if (!NlCore::IsPlayersLord(Row))
		{
			Note = "플레이어의 영주에게서만 합니다";
			return false;
		}
		std::vector<std::string> traits;
		if (!ReadTraits(soul_value, traits))
		{
			Note = "특성을 읽지 못했습니다";
			return false;
		}
		if (!NlCore::IsKid(traits))
		{
			Note = "아이가 아닙니다";
			return false;
		}
		Log("people call set_age(" + Shortest(NlCore::k_GrownAge) + ") on " + Row.Uuid + " (grow up)");
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".set_age"), { RValue(NlCore::k_GrownAge) }, result, Note))
			return false;
		if (!StillThere(Row, soul_value) || !ReadTraits(soul_value, traits))
		{
			Note = "나이를 맞춘 뒤 그 사람을 다시 읽지 못했습니다";
			return false;
		}
		if (NlCore::IsKid(traits))
		{
			Note = "나이를 18 로 맞췄지만 아이 특성이 남아 있습니다";
			return false;
		}
		Note = NlCore::Has(traits, "untitled_lord") ? "어른이 됐습니다. 게임이 소영주(untitled_lord)로 만들었습니다: 진영이 바뀌어 영주 목록에서 빠집니다('주민·손님도 보기'로 보입니다)"
			: "아이 특성이 없어졌습니다 (소영주의 특성은 보이지 않습니다)";
		return true;
	}

	bool OneConceive(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		// 임신 시작: 아버지의 uuid 를 임신 구성요소의 __father_soul_uuid 에 적고 1/3기의 특성(pregnant_st1)을 붙인다(trait_attach: 본 꼴).
		// 임신한 영주의 그 칸에 아버지의 uuid 가 있었고 단계는 특성이었다. 이렇게 시작한 임신 49번이 모두 다음 단계 함수로 출산이나 유산까지 갔다(research/24).
		// 출산 뒤의 pregnant_forbid 를 뗀 바로 뒤에도 됐다(is_can_pregant 가 참이었다).
		// **ComponentPregnancy.begin_pregnant() 는 부르지 않는다**: 아버지를 적고 불렀는데 게임이 끝났다("I32 argument is undefined").
		// 붙이기 전에 게임의 판정 is_can_pregant()(인자 없음: 기계어. 한 번 불러 참으로 읽혔다)를 묻는다. 되지 않으면 적어 둔 아버지를 지운다.
		if (!NlCore::IsPlayersLord(Row))
		{
			Note = "플레이어의 영주에게서만 합니다 (주민·손님·다른 진영에게는 불러 본 적이 없습니다)";
			return false;
		}
		std::vector<std::string> traits;
		double gender = k_Unknown;
		std::string why;
		if (!ReadTraits(soul_value, traits))
		{
			Note = "특성을 읽지 못했습니다";
			return false;
		}
		CallNumber(soul + ".get_gender", gender);
		if (!NlCore::CanConceive(gender, traits, why))
		{
			Note = why;
			return false;
		}
		const PersonRow* found = FindRow(C.Text);
		if (!found || !NlCore::IsPlayersLord(*found))
		{
			Note = "아버지는 플레이어의 살아 있는 영주여야 합니다";
			return false;
		}
		const PersonRow father = *found;		// 사본(아래의 호출이 목록을 바꿀 수 있다)
		RValue father_soul;
		std::vector<std::string> father_traits;
		double father_gender = k_Unknown;
		Log("people call get_gender() on " + father.Uuid + " (the father)");
		if (!StillThere(father, father_soul) || !ReadTraits(father_soul, father_traits) || !CallNumber(Base(father) + ".__soul.get_gender", father_gender))
		{
			Note = "아버지의 성별이나 특성을 읽지 못했습니다";
			return false;
		}
		if (!NlCore::CanFather(father_gender, father_traits))
		{
			Note = "아버지로 삼을 수 없는 사람입니다 (어른 남성이어야 합니다)";
			return false;
		}
		RValue can;
		Log("people call is_can_pregant() on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__pregnancy.is_can_pregant"), {}, can, Note))
			return false;
		// 돌려주는 형을 기록으로 보지는 못했다. 수나 불리언일 때만 읽는다(러너의 불리언 변환에 다른 형을 넘기지 않는다).
		if (!NlGame::IsNumber(can))
		{
			Note = "게임의 판정(is_can_pregant)이 수나 불리언을 돌려주지 않았습니다";
			return false;
		}
		if (can.ToDouble() == 0)
		{
			Note = "게임이 임신할 수 없다고 답했습니다 (is_can_pregant 가 거짓)";
			return false;
		}
		const std::string where = soul + ".__pregnancy.__father_soul_uuid";
		if (!NlAccess::WriteString(where, C.Text, Note))
			return false;
		Log("people: conceive on " + Row.Uuid + ": father " + C.Text + " written, attaching " + NlCore::PregnancyTrait(1));
		const bool attached = Attach(Row, NlCore::PregnancyTrait(1), soul_value, Note);		// 붙이고 다시 읽어 확인한다
		const bool same = StillThere(Row, soul_value);
		const bool read = same && ReadTraits(soul_value, traits);
		const int stage = read ? NlCore::PregnancyStage(traits) : 0;
		// 아버지의 칸을 다시 읽는다(특성이 붙으며 게임이 비우거나 바꿨을 수 있다)
		RValue cell;
		std::string ignored;
		const bool kept = same && NlAccess::Read(NlCore::ParseAskPath(where), cell, ignored) && cell.IsString() && cell.ToString() == C.Text;
		if (attached && stage > 0 && kept)
		{
			Note = "임신 " + std::to_string(stage) + "/3기가 됐습니다 (아버지 " + father.Name + ")";
			return true;
		}
		if (attached && stage > 0)
		{
			// 특성은 붙은 채다. 지우지 않고 그대로 알린다
			Note = "임신 " + std::to_string(stage) + "/3기의 특성은 붙었지만 아버지의 칸이 적은 대로가 아닙니다";
			Log("people: conceive on " + Row.Uuid + ": the father cell is not what was written");
			return false;
		}
		// 되지 않았다: 적어 둔 아버지를 지운다(그 칸의 평소 값은 빈 글이다)
		std::string clean;
		const bool cleared = same && NlAccess::WriteString(where, "", clean);
		Note = (!attached ? (Note.empty() ? std::string("임신의 특성을 붙이지 못했습니다") : Note) : !read ? std::string("붙인 뒤 그 사람을 다시 읽지 못했습니다")
			: std::string("임신의 특성이 붙지 않았습니다")) + (cleared ? "" : ". 적어 둔 아버지의 uuid 를 지우지 못했습니다");
		Log("people: conceive on " + Row.Uuid + " failed: " + Note);
		return false;
	}

	bool OneRole(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		const NlCore::RolePreset* role = NlCore::FindRole(C.Text);
		if (!role)
		{
			Note = "모르는 역할 프리셋입니다";
			return false;
		}
		ApplyRole(*role, Row, soul_value, Note);
		return true;		// 한 것과 하지 못한 것은 글이 말한다(Execute 가 그 글을 그대로 돌려준다)
	}

	bool OneTraitRemove(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		std::vector<std::string> traits;
		ReadTraits(soul_value, traits);
		if (!NlCore::Has(traits, C.Text))
		{
			Note = "없는 특성입니다";
			return false;
		}
		return Detach(Row, C.Text, Note);
	}

	bool OneKnowledgeAll(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		const std::string knowledge = soul + ".__character_soul.__knowledge";
		double before = 0, after = 0;
		if (!CallNumber(knowledge + ".get_knowledge_count", before))
		{
			Note = "지식을 갖는 사람이 아닙니다";		// 주민
			return false;
		}
		// ComponentKnowledge.add_all_knowledge(): 인자 없음(기계어). 부르자 한 영주의 지식이 121개가 되고 게임의 지식 창에 보였다(research/12).
		Log("people call add_all_knowledge() on " + Row.Uuid);
		if (!CallNoArgs(knowledge + ".add_all_knowledge", result, Note))
			return false;
		if (!CallNumber(knowledge + ".get_knowledge_count", after) || after < before)
		{
			Note = "지식의 수를 다시 읽지 못했습니다";
			return false;
		}
		Note = "지식 " + Shortest(before) + "개에서 " + Shortest(after) + "개로";
		return true;
	}

	bool OneKnowledgeAdd(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		const int index = KnowledgeIndex(C.Text);
		RValue knowledge, answer;
		std::string name, why;
		// 지식 구조체는 게임의 목록에서 얻는다. 그 자리의 이름이 청한 이름인지 다시 본다.
		if (index < 0 || !NlAccess::Read(NlCore::ParseAskPath(std::string(k_KnowledgeList) + "[" + std::to_string(index) + "]"), knowledge, why) || !knowledge.IsStruct()
			|| !FollowString(knowledge, { { '.', "__name", 0 } }, name) || name != C.Text)
		{
			Note = "게임의 지식 목록에서 찾지 못했습니다";
			return false;
		}
		const std::string component = soul + ".__character_soul.__knowledge";
		// is_have_knowledge(지식 구조체) -> 불리언: 게임이 그 꼴로 부르는 것을 기록했다.
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(component + ".is_have_knowledge"), { knowledge }, answer, Note) || !NlGame::IsNumber(answer))
		{
			Note = "지식을 갖는 사람이 아닙니다";
			return false;
		}
		if (answer.ToDouble() != 0)
		{
			Note = "이미 가진 지식입니다";
			return true;
		}
		// add_knowledge(지식 구조체, true, true) -> true: 연구가 끝날 때 게임이 그 꼴로 불렀다.
		Log("people call add_knowledge(" + C.Text + ", true, true) on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(component + ".add_knowledge"), { knowledge, RValue(true), RValue(true) }, result, Note))
			return false;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(component + ".is_have_knowledge"), { knowledge }, answer, Note) || !NlGame::IsNumber(answer) || answer.ToDouble() == 0)
		{
			Note = "게임이 주지 않았습니다";
			return false;
		}
		return true;
	}

	bool OneMoneyAdd(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		const std::string money = soul + ".__inventory.__money";
		double current = 0, delta = 0, after = 0;
		if (!NlAccess::ReadNumber(money, current) || !NlCore::GiftDelta(current, C.Amount, delta))
		{
			Note = "더하거나 뺄 것이 없습니다";
			return false;
		}
		// ComponentInventory.change_money(변화량): 게임이 수 하나로 100번 불렀다((29), (-40). research/12).
		Log("people call change_money(" + Shortest(delta) + ") on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__inventory.change_money"), { RValue(delta) }, result, Note))
			return false;
		if (!NlAccess::ReadNumber(money, after) || after != current + delta)
		{
			Note = "소지금이 청한 만큼 바뀌지 않았습니다(" + Shortest(current) + "에서 " + Shortest(after) + ")";
			return false;
		}
		return true;
	}

	bool OneEquip(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		// 선호 장비(research/17): 영혼마다 __preferred_equipment 가 있고, 게임은 거기에 없는 장비를 무기고(영지 창고)로 돌려보낸다.
		// 소환한 병사의 것은 비어 있다("__empty__"). 그래서 소지품에 넣은 장비가 몇 시간 뒤에 벗겨진다.
		if (!NlCore::IsSoldier(Row))
		{
			Note = "병사가 아닙니다";
			return false;
		}
		const NlCore::Loadout* loadout = NlCore::FindLoadout(C.Text);
		if (!loadout)
		{
			Note = "모르는 장비 묶음입니다";
			return false;
		}
		const std::string preset = std::string(k_PreferredData) + "." + loadout->Member;
		RValue wanted;		// 이 함수 안에서만 든다
		if (!NlAccess::Read(NlCore::ParseAskPath(preset), wanted, Note) || !wanted.IsStruct())
		{
			if (Note.empty())
				Note = "게임에 그 장비 묶음이 없습니다";
			return false;
		}
		// 부르기 전에 묶음의 내용과 소지품을 읽는다. 읽지 못하면 아무것도 하지 않는다("넣을 것이 없다"와 섞이지 않게).
		double armor = 0, weapon = 0, shield = 0;
		if (!NlAccess::ReadNumber(preset + ".__armor_resource[0]", armor) || !NlAccess::ReadNumber(preset + ".__weapon_resource[0]", weapon)
			|| !NlAccess::ReadNumber(preset + ".__is_need_shield", shield))
		{
			Note = "그 장비 묶음의 내용을 읽지 못했습니다";
			return false;
		}
		std::vector<double> items;
		ReadNumbers(soul_value, { { '.', "__inventory", 0 }, { '.', "__resources", 0 } }, items);
		if (items.empty())
		{
			Note = "소지품을 읽지 못했습니다";
			return false;
		}
		// SoulBasic.set_preferred_equipment(선호 장비 구조체) -> undefined: 게임이 구조체 하나로 부르는 것을 기록했고(열한 시간에 33번),
		// 그 꼴로 불러 __preferred_equipment.__name 이 "h_swordman"으로 여덟 시간 넘게 남는 것을 봤다(research/17).
		Log("people call set_preferred_equipment(" + C.Text + ") on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".set_preferred_equipment"), { wanted }, result, Note))
			return false;
		// 쓴 뒤 다시 읽는다: 그 영혼의 선호 장비가 묶음의 갑옷·무기와 같은가.
		double now_armor = 0, now_weapon = 0;
		const bool stuck = NlAccess::ReadNumber(soul + ".__preferred_equipment.__armor_resource[0]", now_armor)
			&& NlAccess::ReadNumber(soul + ".__preferred_equipment.__weapon_resource[0]", now_weapon) && now_armor == armor && now_weapon == weapon;
		const std::vector<int> gifts = stuck ? NlCore::EquipGifts(armor, weapon, shield != 0, items) : std::vector<int>();
		// 그 장비를 소지품에 넣는다(없는 것만 하나씩). 넣으면 바로 착용된다. ComponentInventory.change(자원 번호, 변화량), get(자원 번호)는 ItemAdd 와 같은 길이다.
		int given = 0;
		for (const int resource : gifts)
		{
			const RValue index(static_cast<double>(resource));
			RValue changed, count;
			std::string why;
			Log("people call inventory.change(" + std::to_string(resource) + ", 1) on " + Row.Uuid);
			if (NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__inventory.change"), { index, RValue(1.0) }, changed, why)
				&& NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__inventory.get"), { index }, count, why) && NlGame::IsNumber(count) && count.ToDouble() >= 1)
				given++;
			else
				Log("people: equip could not give resource " + std::to_string(resource) + " to " + Row.Uuid + ": " + (why.empty() ? "the count did not change" : why));
		}
		const NlCore::EquipResult report = NlCore::EquipReport(loadout->Label, stuck, static_cast<int>(gifts.size()), given);
		Note = report.Note;
		return report.Ok;
	}

	bool OneItemAdd(const PersonCommand& C, const PersonRow& Row, const std::string& soul, RValue& soul_value, std::string& Note)
	{
		RValue result;		// 이 함수 안에서만 든다
		// 한도는 그 사람의 소지품 칸의 수다(영주에서 39칸을 봤다. 주민의 것은 재지 않았다).
		std::vector<double> items, equipped;
		ReadNumbers(soul_value, { { '.', "__inventory", 0 }, { '.', "__resources", 0 } }, items);
		if (static_cast<size_t>(C.Index) >= items.size() || static_cast<size_t>(C.Index) >= g_Now.Resources.size())
		{
			Note = "그 사람에게 없는 자원 칸입니다";
			return false;
		}
		ReadEquipped(soul_value, equipped);
		if (C.Amount < 0 && NlCore::IsEquipped(C.Index, equipped))
		{
			Note = "착용 중인 장비는 소지품에서 빼지 않습니다";		// 수만 줄고 착용은 그대로라 어긋난다
			return false;
		}
		// ComponentInventory.get(자원 번호) -> 수, change(자원 번호, 변화량): 게임이 (수), (수, 수)로 불렀다. change(1, 50) 뒤 get(1) 이 50 이었다.
		const NlCore::AskPath get = NlCore::ParseAskPath(soul + ".__inventory.get");
		const RValue resource(static_cast<double>(C.Index));
		RValue count;
		double delta = 0;
		if (!NlAccess::CallMethod(get, { resource }, count, Note) || !NlGame::IsNumber(count))
			return false;
		const double current = count.ToDouble();
		if (!NlCore::GiftDelta(current, C.Amount, delta))
		{
			Note = "더하거나 뺄 것이 없습니다";
			return false;
		}
		Log("people call inventory.change(" + std::to_string(C.Index) + ", " + Shortest(delta) + ") on " + Row.Uuid);
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__inventory.change"), { resource, RValue(delta) }, result, Note))
			return false;
		if (!NlAccess::CallMethod(get, { resource }, count, Note) || !NlGame::IsNumber(count) || count.ToDouble() != current + delta)
		{
			Note = "소지품이 청한 만큼 바뀌지 않았습니다";
			return false;
		}
		return true;
	}

	// 한 사람에게 명령 하나를 한다. 그 자리의 uuid 를 다시 보고(StillThere) 행동마다의 함수로 나눈다(위의 One*. 2026-10-07 리뷰 R4 로 나눴다).
	bool One(const PersonCommand& C, const PersonRow& Row, std::string& Note, bool Bulk)
	{
		RValue soul_value;
		if (!StillThere(Row, soul_value))
		{
			Note = "그 자리의 사람이 바뀌었습니다";
			return false;
		}
		const std::string soul = Base(Row) + ".__soul";

		switch (C.Act)
		{
		case PersonAct::SkillSet:
		case PersonAct::SkillAdd: return OneSkill(C, Row, soul, soul_value, Note);
		case PersonAct::SkillsMax: return OneSkillsMax(C, Row, soul, soul_value, Note);
		case PersonAct::NeedSet: return SetNeed(soul, C.Index, C.Amount, Note);
		case PersonAct::NeedsFill: return OneNeedsFill(C, Row, soul, soul_value, Note);
		case PersonAct::AgeSet: return OneAgeSet(C, Row, soul, soul_value, Note);
		case PersonAct::Happy: return OneHappy(C, Row, soul, soul_value, Note, Bulk);
		case PersonAct::Cure: return OneCure(C, Row, soul, soul_value, Note);
		case PersonAct::TraitAdd: return OneTraitAdd(C, Row, soul, soul_value, Note);
		case PersonAct::PregnancyNext:
		case PersonAct::Birth: return OnePregnancy(C, Row, soul, soul_value, Note);
		case PersonAct::GrowUp: return OneGrowUp(C, Row, soul, soul_value, Note);
		case PersonAct::Conceive: return OneConceive(C, Row, soul, soul_value, Note);
		case PersonAct::Role: return OneRole(C, Row, soul, soul_value, Note);
		case PersonAct::RoleUndo: return OneRoleUndo(C, Row, soul, soul_value, Note);
		case PersonAct::TraitRemove: return OneTraitRemove(C, Row, soul, soul_value, Note);
		case PersonAct::KnowledgeAll: return OneKnowledgeAll(C, Row, soul, soul_value, Note);
		case PersonAct::KnowledgeAdd: return OneKnowledgeAdd(C, Row, soul, soul_value, Note);
		case PersonAct::MoneyAdd: return OneMoneyAdd(C, Row, soul, soul_value, Note);
		case PersonAct::Equip: return OneEquip(C, Row, soul, soul_value, Note);
		case PersonAct::ItemAdd: return OneItemAdd(C, Row, soul, soul_value, Note);
		}
		return false;
	}

	std::string WhoText(const std::string& Who)
	{
		if (Who == "lords")
			return "영주 전원";
		if (Who == "people")
			return "플레이어의 사람 전원";
		const PersonRow* row = FindRow(Who);
		return row ? row->Name : Who;
	}

	// 명령 하나를 한다. 사람들을 바로 앞에서 다시 읽으므로 자리(n 번째)는 지금의 것이다. 돌려주는 글: 한 일.
	std::string Execute(const PersonCommand& C)
	{
		std::string why;
		if (!NlCore::CheckPersonCommand(C, why))
			return why;
		if (!Scan())
			return g_Now.Why;
		const bool trait = C.Act == PersonAct::TraitAdd || C.Act == PersonAct::TraitRemove;
		if (trait && !NlTraitText::Known(C.Text))
			return "게임에 없는 특성입니다: " + C.Text;
		if (C.Act == PersonAct::KnowledgeAdd && KnowledgeIndex(C.Text) < 0)
			return "게임에 없는 지식입니다: " + C.Text;

		// 대상의 사본을 먼저 뜬다. 아래에서 부르는 게임의 함수가 사람들의 목록을 바꿔도(드나듦, 다시 읽기) 낡은 자리로 목록을 다시 찾지 않는다.
		std::vector<PersonRow> targets;
		for (const size_t at : NlCore::PickTargets(g_Now.People, C.Who))
			targets.push_back(g_Now.People[at]);
		const bool bulk = NlCore::IsBulkWho(C.Who);
		if (bulk && C.Act == PersonAct::Equip)		// 여럿에게 하는 장비 지급은 병사에게만 간다
			targets.erase(std::remove_if(targets.begin(), targets.end(), [](const PersonRow& row) { return !NlCore::IsSoldier(row); }), targets.end());
		if (bulk && C.Act == PersonAct::Birth)		// 여럿에게 하는 출산은 임신한 사람에게만 간다
			targets.erase(std::remove_if(targets.begin(), targets.end(), [](const PersonRow& row) {
				RValue soul;
				std::vector<std::string> traits;
				return !StillThere(row, soul) || !ReadTraits(soul, traits) || NlCore::PregnancyStage(traits) == 0;
			}), targets.end());
		if (targets.empty())
			return bulk && C.Act == PersonAct::Equip ? "병사가 없습니다" : bulk && C.Act == PersonAct::Birth ? "임신한 영주가 없습니다" : "대상이 없습니다";

		size_t done = 0;
		std::string first_failure, note, only;
		for (const PersonRow& row : targets)
		{
			std::string one;
			const bool ok = One(C, row, one, bulk);
			if (targets.size() == 1)
				only = one;
			if (ok)
			{
				done++;
				if (targets.size() == 1)
					note = one;
			}
			else if (first_failure.empty())		// 이름으로 시작하는 글(임신의 단계)에는 이름을 다시 붙이지 않는다
				first_failure = one.rfind(row.Name + ":", 0) == 0 ? one : row.Name + ": " + (one.empty() ? "하지 못했습니다" : one);
		}
		if ((C.Act == PersonAct::Role || C.Act == PersonAct::RoleUndo) && done == 1 && !note.empty())
		{
			// 역할 프리셋(과 그 되돌리기)은 그 글이 곧 결과다(이름, 한 것, 하지 못한 것. core 의 RoleReport·RoleUndoReport). 아래의 집계 줄("1/1")은 적지 않는다: 일부만 됐어도 1/1 이 된다.
			Log(std::string("people: ") + NlCore::PersonActWord(C.Act) + " on " + C.Who + ": " + note);
			return note;
		}
		if ((C.Act == PersonAct::PregnancyNext || C.Act == PersonAct::Birth) && !bulk && only.rfind(targets.front().Name + ":", 0) == 0)
		{
			// 임신의 단계도 그 글이 곧 결과다(이름과 단계. core 의 StageReport). 됐든 안 됐든 그대로 돌려준다.
			Log(std::string("people: ") + NlCore::PersonActWord(C.Act) + " on " + C.Who + ": " + only);
			return only;
		}
		Log(std::string("people: ") + NlCore::PersonActWord(C.Act) + " on " + C.Who + ": " + std::to_string(done) + "/" + std::to_string(targets.size())
			+ (first_failure.empty() ? "" : " (first failure: " + first_failure + ")"));

		std::string text = WhoText(C.Who) + ": " + std::to_string(done) + "/" + std::to_string(targets.size()) + "명에게 했습니다";
		if (!note.empty())
			text += " (" + note + ")";
		if (!first_failure.empty())
			text += "; " + first_failure;
		return text;
	}

	// 명령을 하고 마지막으로 한 일로 적어 둔다. 역할 프리셋의 결과는 그 단추 아래에도 보인다.
	void Run(const PersonCommand& C)
	{
		g_Now.Last = Execute(C);
		if (C.Act == PersonAct::Role || C.Act == PersonAct::RoleUndo)
		{
			g_RoleLast = g_Now.Last;
			g_RoleLastFor = C.Who;
		}
	}

	// 병사를 만든다. 돌려주는 글: 한 일.
	std::string SpawnSoldiersNow(double Asked)
	{
		int count = 0;
		if (!NlCore::SoldierBatch(Asked, count))
			return "만들 병사의 수가 없습니다";
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		const int before = NlAccess::InstanceCount("o_dummy");
		int made = 0;
		for (int i = 0; i < count; i++)
		{
			RValue result;		// 이 함수 안에서만 든다
			Log("people call rebellion_debug_spawn_player_soldier() " + std::to_string(i + 1) + "/" + std::to_string(count));		// 부르기 전에 남긴다
			if (!NlGame::CallScript(k_SpawnSoldier, {}, result))
				break;
			made++;
		}
		const int after = NlAccess::InstanceCount("o_dummy");
		NlShield::RefreshSoon();		// 새 병사의 영혼을 바로 다음 틱에 묶음에 넣는다(전투의 항목들)
		Log("people: spawned " + std::to_string(made) + "/" + std::to_string(count) + " soldier(s), o_dummy " + std::to_string(before) + " -> " + std::to_string(after));
		return "병사 " + std::to_string(made) + "명을 만들었습니다 (주민과 병사 " + std::to_string(before) + "명에서 " + std::to_string(after) + "명으로)";
	}

	// 디버그 소환기로 플레이어의 사람 하나를 만든다(마우스가 가리키는 지도의 자리). 돌려주는 글: 한 일.
	std::string SpawnHereNow(NlCore::SpawnKind Kind)
	{
		const std::string label = NlCore::SpawnLabel(Kind);
		if (!NlAccess::InGame())
			return "게임 화면이 아닙니다";
		const int before = NlAccess::InstanceCount("o_dummy") + NlAccess::InstanceCount("o_character");
		const std::string path = std::string(k_Spawner) + "." + NlCore::SpawnMethod(Kind);
		RValue result;		// 이 함수 안에서만 든다
		std::string why;
		Log("people call " + path + "()");		// 부르기 전에 남긴다
		if (!CallNoArgs(path, result, why))
			return label + ": 부르지 못했습니다: " + why;
		const int after = NlAccess::InstanceCount("o_dummy") + NlAccess::InstanceCount("o_character");
		NlShield::RefreshSoon();		// 새 사람의 영혼을 바로 다음 틱에 묶음에 넣는다(전투의 항목들)
		Log("people: spawner " + std::string(NlCore::SpawnWord(Kind)) + ", people " + std::to_string(before) + " -> " + std::to_string(after));
		return label + (after > before ? " 하나를 만들었습니다" : ": 불렀지만 사람의 수가 그대로입니다")
			+ " (영주와 주민 " + std::to_string(before) + "명에서 " + std::to_string(after) + "명으로)";
	}

}
