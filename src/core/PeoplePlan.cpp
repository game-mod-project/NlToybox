#include "PeoplePlan.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace NlCore
{
	namespace
	{
		struct ActWord
		{
			PersonAct Act;
			const char* Word;
		};

		constexpr ActWord k_Acts[] = {
			{ PersonAct::SkillSet, "skill_set" }, { PersonAct::SkillAdd, "skill_add" }, { PersonAct::SkillsMax, "skills_max" },
			{ PersonAct::NeedSet, "need_set" }, { PersonAct::NeedsFill, "needs_fill" }, { PersonAct::AgeSet, "age_set" },
			{ PersonAct::Happy, "happy" }, { PersonAct::Cure, "cure" }, { PersonAct::TraitAdd, "trait_add" }, { PersonAct::TraitRemove, "trait_remove" },
			{ PersonAct::KnowledgeAll, "knowledge_all" }, { PersonAct::KnowledgeAdd, "knowledge_add" },
			{ PersonAct::MoneyAdd, "money_add" }, { PersonAct::ItemAdd, "item_add" },
		};

		bool Clamp(double Value, double Low, double High, bool Whole, double& Out)
		{
			if (!std::isfinite(Value))
				return false;
			Out = std::clamp(Whole ? std::round(Value) : Value, Low, High);
			return true;
		}
	}

	const std::vector<NamedKey>& SkillNames()
	{
		// 열쇠는 실행 중인 게임의 __soul.__skills.__level 에서 읽었다(research/11). 차례는 그 구조체의 이름순이다.
		static const std::vector<NamedKey> names = {
			{ "combat", "전투" }, { "command", "지휘" }, { "education", "교육" }, { "knowledge", "지식" },
			{ "management", "관리" }, { "manners", "예절" }, { "negotiation", "협상" }, { "oratory", "화술" },
		};
		return names;
	}

	const std::vector<const char*>& NeedNames()
	{
		// gml_Script_motive_get_caption(0..5) 가 돌려준 이름 그대로다(research/11).
		static const std::vector<const char*> names = { "수면", "음식", "휴식", "신앙심", "성관계", "돌봄" };
		return names;
	}

	bool ParsePersonAct(const std::string& Word, PersonAct& Out)
	{
		for (const ActWord& one : k_Acts)
			if (Word == one.Word)
			{
				Out = one.Act;
				return true;
			}
		return false;
	}

	const char* PersonActWord(PersonAct Act)
	{
		for (const ActWord& one : k_Acts)
			if (one.Act == Act)
				return one.Word;
		return "";
	}

	bool NeedsIndex(PersonAct Act)
	{
		return IndexLimit(Act) > 0;
	}

	int IndexLimit(PersonAct Act)
	{
		switch (Act)
		{
		case PersonAct::SkillSet:
		case PersonAct::SkillAdd:
			return static_cast<int>(SkillNames().size());
		case PersonAct::NeedSet:
			return static_cast<int>(NeedNames().size());
		case PersonAct::ItemAdd:
			return k_ItemIndexMax;
		default:
			return 0;
		}
	}

	bool NeedsAmount(PersonAct Act)
	{
		return NeedsIndex(Act) || Act == PersonAct::AgeSet || Act == PersonAct::MoneyAdd;
	}

	bool NeedsText(PersonAct Act)
	{
		return Act == PersonAct::TraitAdd || Act == PersonAct::TraitRemove || Act == PersonAct::KnowledgeAdd;
	}

	bool GoodWho(const std::string& Who)
	{
		if (Who.empty() || Who.size() > 64)
			return false;
		return std::all_of(Who.begin(), Who.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
	}

	bool IsBulkWho(const std::string& Who)
	{
		return Who == "lords" || Who == "people";
	}

	bool BulkAllowed(const std::string& Who, PersonAct Act)
	{
		if (!IsBulkWho(Who))
			return true;
		if (Act == PersonAct::SkillsMax || Act == PersonAct::NeedsFill || Act == PersonAct::Happy || Act == PersonAct::Cure)
			return true;
		return Act == PersonAct::KnowledgeAll && Who == "lords";		// 주민은 지식을 갖지 않는다
	}

	bool IsEquipped(int Index, const std::vector<double>& Equipped)
	{
		if (Index < 0)
			return false;
		return std::find(Equipped.begin(), Equipped.end(), static_cast<double>(Index)) != Equipped.end();
	}

	bool SoldierBatch(double Asked, int& Count)
	{
		if (!std::isfinite(Asked))
			return false;
		const double whole = std::round(Asked);
		if (whole < 1)
			return false;
		Count = static_cast<int>(std::min(whole, static_cast<double>(k_SoldierBatchMax)));
		return true;
	}

	bool GiftDelta(double Current, double Asked, double& Delta)
	{
		if (!std::isfinite(Current) || !std::isfinite(Asked))
			return false;
		double delta = std::round(Asked);
		if (delta < 0)
			delta = std::max(delta, -std::max(0.0, std::floor(Current)));		// 가진 것까지만 뺀다
		if (delta == 0)
			return false;
		Delta = delta;
		return true;
	}

	bool CheckPersonCommand(const PersonCommand& Command, std::string& Why)
	{
		Why.clear();
		const bool trait = Command.Act == PersonAct::TraitAdd || Command.Act == PersonAct::TraitRemove;
		const bool gift = Command.Act == PersonAct::MoneyAdd || Command.Act == PersonAct::ItemAdd;
		if (!GoodWho(Command.Who))
			Why = "누구인지 없습니다";
		else if (!BulkAllowed(Command.Who, Command.Act))
			Why = "한 사람을 짚어서만 할 수 있습니다";
		else if (NeedsIndex(Command.Act) && (Command.Index < 0 || Command.Index >= IndexLimit(Command.Act)))
			Why = "번호가 범위 밖입니다";
		else if (Command.Act == PersonAct::ItemAdd && Command.Index < 1)
			Why = "0 번 자원은 건드리지 않습니다";		// 경제 패널과 같다(갈래에 없는 자원)
		if (Why.empty() && NeedsAmount(Command.Act) && !std::isfinite(Command.Amount))
			Why = "수가 아닙니다";
		if (Why.empty() && gift && (std::round(Command.Amount) == 0 || std::fabs(Command.Amount) > k_GiftMax))
			Why = "줄 수가 0 이거나 너무 큽니다";
		// 지식의 이름도 특성의 이름과 같은 꼴이다(소문자·숫자·밑줄). 게임에 있는 이름인지는 부르는 쪽이 게임의 목록으로 본다.
		if (Why.empty() && NeedsText(Command.Act) && !GoodTraitName(Command.Text))
			Why = trait ? "특성 이름이 아닙니다" : "지식 이름이 아닙니다";
		if (Why.empty() && trait && IsProtectedTrait(Command.Text))
			Why = "붙이거나 뗄 수 없는 특성입니다";
		return Why.empty();
	}

	Alive AliveFromDeadCache(bool Read, double Raw)
	{
		if (!Read)
			return Alive::Unknown;
		return Raw == 0 ? Alive::Yes : Raw == 1 ? Alive::No : Alive::Unknown;		// NaN 과 -4 는 어느 쪽도 아니다
	}

	bool IsPlayers(const PersonRow& Row)
	{
		return !Row.Dead && Row.Faction == "player";
	}

	std::vector<size_t> PickTargets(const std::vector<PersonRow>& People, const std::string& Who)
	{
		std::vector<size_t> picked;
		const bool lords = Who == "lords", everyone = Who == "people";
		for (size_t i = 0; i < People.size(); i++)
		{
			const PersonRow& row = People[i];
			if (row.Dead)
				continue;
			if (lords || everyone ? IsPlayers(row) && (everyone || row.Character) : !Who.empty() && row.Uuid == Who)
				picked.push_back(i);
		}
		return picked;
	}

	bool SkillValue(double Asked, double& Out)
	{
		return Clamp(Asked, 0, k_SkillMax, true, Out);
	}

	bool SkillAfterAdd(double Current, double Delta, double& Out)
	{
		return Clamp(Current + Delta, 0, k_SkillMax, true, Out);
	}

	bool NeedValue(double Asked, double Limit, double& Out)
	{
		if (!std::isfinite(Limit) || !(Limit > 0))
			return false;
		return Clamp(Asked, 0, Limit, false, Out);
	}

	bool AgeValue(double Asked, double& Out)
	{
		return Clamp(Asked, k_AgeMin, k_AgeMax, true, Out);
	}

	bool GoodTraitName(const std::string& Name)
	{
		if (Name.empty() || Name.size() > 64)
			return false;
		return std::all_of(Name.begin(), Name.end(), [](unsigned char c) { return std::islower(c) || std::isdigit(c) || c == '_'; });
	}

	bool IsProtectedTrait(const std::string& Name)
	{
		// 이름은 game_trait_list 에 있는 것이다(research/11).
		for (const char* one : { "human", "wolf", "pig", "dog", "dead", "delayed_dead", "dead_from_old_age", "dead_from_poison", "dying_from_old", "lost_head" })
			if (Name == one)
				return true;
		return false;
	}

	const std::vector<const char*>& WoundTraits()
	{
		// 이름은 실행 중인 게임의 inst:o_data.game_trait_list 에서 읽었다(research/11). 타박상(bruise_light)을 붙였다 떼는 것을 쟀다.
		// 나머지는 같은 함수(trait_detach)에 다른 이름을 넘기는 것이다. 효과는 확인 전이다.
		static const std::vector<const char*> names = {
			"bruise_light", "bruise", "light_cut", "cut", "pierced_arm", "pierced_shoulder", "pierced_leg", "pierced_lung",
			"wound_deep", "wound_deadly", "injury_arm", "injury_face", "injury_rib", "injury_leg",
			"burn_light", "burn_middle", "burn_heavy", "inflamed_wound", "blood_poisoning",
		};
		return names;
	}

	bool ShouldFillNeed(double Current, double Limit)
	{
		double wanted = 0;
		return std::isfinite(Current) && NeedValue(Limit, Limit, wanted) && Current < wanted - 0.5;
	}

	bool ShouldAttachHappy(bool Read, double MindSum, bool Bulk)
	{
		return Read ? MindSum < 100 : !Bulk;
	}

	std::vector<int> NeedsToHold(bool NoHunger, bool NoTiredness, bool All)
	{
		std::vector<int> needs;
		for (int i = 0; i < static_cast<int>(NeedNames().size()); i++)
			if (All || (NoHunger && i == 1) || (NoTiredness && (i == 0 || i == 2)))
				needs.push_back(i);
		return needs;
	}

	PeopleSlice NextPeopleSlice(size_t Count, size_t Cursor, size_t Batch)
	{
		PeopleSlice slice;
		if (Batch == 0)
			Batch = 1;
		slice.Begin = Cursor < Count ? Cursor : 0;
		slice.End = std::min(Count, slice.Begin + Batch);
		slice.Wrapped = slice.End >= Count;
		slice.Next = slice.Wrapped ? 0 : slice.End;
		return slice;
	}

	HoldPlan HoldBegin(HoldRound& Round, bool AnyNeeds, bool Happy, bool Ageless)
	{
		HoldPlan plan;
		if (Ageless)
			Round.Restoring = false;		// 다시 켰다. 되돌리던 것을 그만둔다
		else if (Round.AgeWritten && !Round.Restoring)
		{
			// 꺼졌는데 끈 깃발이 남아 있다. 바퀴의 어디에 있었든 처음부터 온전한 한 바퀴로 되돌린다.
			Round.Restoring = true;
			Round.RestoreClean = true;
			Round.Cursor = 0;
			plan.Rescan = true;
		}
		plan.Work = AnyNeeds || Happy || Ageless || Round.AgeWritten;
		plan.WriteAge = Ageless || Round.AgeWritten;
		plan.AgeValue = Ageless ? 0 : 1;
		if (!plan.Work)
			Round.Cursor = 0;
		return plan;
	}

	void HoldTouched(HoldRound& Round, bool WroteAgeOff, bool Skipped)
	{
		if (WroteAgeOff)
			Round.AgeWritten = true;		// 바퀴가 감기기를 기다리지 않는다(첫 묶음만 쓰고 꺼도 되돌린다)
		if (Skipped && Round.Restoring)
			Round.RestoreClean = false;
	}

	void HoldEnd(HoldRound& Round, const PeopleSlice& Slice, bool Ageless)
	{
		Round.Cursor = Slice.Next;
		if (!Slice.Wrapped || Ageless || !Round.Restoring)
			return;
		if (Round.RestoreClean)
			Round.AgeWritten = false;		// 모두 되돌렸다
		Round.Restoring = false;			// 깨끗하지 않았으면 다음 틱의 HoldBegin 이 다시 한 바퀴를 시작한다
	}
}
