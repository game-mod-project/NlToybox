#include "PeoplePlan.hpp"

#include "Text.hpp"

#include "FamilyPlan.hpp"
#include "LibraryPlan.hpp"
#include "RolePlan.hpp"

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
			{ PersonAct::MoneyAdd, "money_add" }, { PersonAct::ItemAdd, "item_add" }, { PersonAct::Equip, "equip" }, { PersonAct::Role, "role" },
			{ PersonAct::RoleUndo, "role_undo" },
			{ PersonAct::PregnancyNext, "pregnancy_next" }, { PersonAct::Birth, "birth" }, { PersonAct::GrowUp, "grow_up" }, { PersonAct::Conceive, "conceive" },
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
		return Act == PersonAct::TraitAdd || Act == PersonAct::TraitRemove || Act == PersonAct::KnowledgeAdd || Act == PersonAct::Equip || Act == PersonAct::Role
			|| Act == PersonAct::Conceive;
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
		if (Act == PersonAct::Equip)
			return Who == "people";		// 플레이어의 사람 가운데 병사에게만 간다(부르는 쪽이 고른다). 영주는 병사가 아니다
		if (Act == PersonAct::Birth)
			return Who == "lords";		// 임신한 영주 모두(부르는 쪽이 고른다). 주민에게는 다음 단계 함수를 불러 보지 않았다
		return Act == PersonAct::KnowledgeAll && Who == "lords";		// 주민은 지식을 갖지 않는다
	}

	bool IsEquipped(int Index, const std::vector<double>& Equipped)
	{
		if (Index < 0)
			return false;
		return std::find(Equipped.begin(), Equipped.end(), static_cast<double>(Index)) != Equipped.end();
	}

	namespace
	{
		struct SpawnInfo
		{
			SpawnKind Kind;
			const char* Word;
			const char* Method;
			const char* Label;
		};
		// 메서드의 이름은 소환기의 statics 에서, 만들어지는 사람의 진영과 갈래는 불러서 봤다(research/13).
		constexpr SpawnInfo k_Spawns[] = {
			{ SpawnKind::Soldier, "soldier", "__spawn_soldier", "병사" },
			{ SpawnKind::Knight, "knight", "__spawn_knight", "기사" },
			{ SpawnKind::Peasant, "peasant", "__spawn_peasant", "주민" },
			{ SpawnKind::Slave, "slave", "__spawn_slave", "노예" },
			{ SpawnKind::Lord, "lord", "__spawn_lord", "영주" },
		};
		const SpawnInfo& SpawnOf(SpawnKind Kind)
		{
			for (const SpawnInfo& spawn : k_Spawns)
				if (spawn.Kind == Kind)
					return spawn;
			return k_Spawns[0];
		}
	}

	bool ParseSpawnKind(const std::string& Word, SpawnKind& Out)
	{
		for (const SpawnInfo& spawn : k_Spawns)
			if (Word == spawn.Word)
			{
				Out = spawn.Kind;
				return true;
			}
		return false;
	}

	const char* SpawnWord(SpawnKind Kind) { return SpawnOf(Kind).Word; }
	const char* SpawnMethod(SpawnKind Kind) { return SpawnOf(Kind).Method; }
	const char* SpawnLabel(SpawnKind Kind) { return SpawnOf(Kind).Label; }

	const std::vector<Loadout>& Loadouts()
	{
		// 게임의 자료에서 읽은 묶음들(research/17): h_swordman 은 갑옷 7·무기 12·방패, any 는 갑옷 -2·무기 -2(아무거나)였다. 나머지 셋은 이름과 번호만 읽었다.
		static const std::vector<Loadout> loadouts = {
			{ "h_swordman", "__h_swordman", "중갑·검·방패" },
			{ "h_axeman", "__h_axeman", "중갑·도끼·방패" },
			{ "h_spearman", "__h_spearman", "중갑·창·방패" },
			{ "h_hammerhead", "__h_hammerhead", "중갑·망치·방패" },
			{ "any", "__any", "아무 장비나" },
		};
		return loadouts;
	}

	const Loadout* FindLoadout(const std::string& Key)
	{
		for (const Loadout& loadout : Loadouts())
			if (Key == loadout.Key)
				return &loadout;
		return nullptr;
	}

	std::vector<int> EquipGifts(double Armor, double Weapon, bool Shield, const std::vector<double>& Inventory)
	{
		std::vector<int> gifts;
		const auto want = [&](double Resource) {
			if (!std::isfinite(Resource) || Resource < 1 || Resource != std::floor(Resource) || Resource >= static_cast<double>(Inventory.size()))
				return false;
			const int index = static_cast<int>(Resource);
			if (!(Inventory[index] >= 1))
				gifts.push_back(index);
			return true;
		};
		const bool armor = want(Armor);
		const bool weapon = want(Weapon);
		if (Shield && (armor || weapon))
			want(k_ShieldResource);
		return gifts;
	}

	EquipResult EquipReport(const char* Label, bool Stuck, int Wanted, int Given)
	{
		EquipResult out;
		const std::string label = Label ? Label : "";
		if (!Stuck)
			out.Note = label + ": 선호 장비가 바뀌지 않았습니다 (장비는 넣지 않았습니다)";
		else if (Given < Wanted)
			out.Note = label + ": 선호 장비는 정했지만 넣을 " + std::to_string(Wanted) + "개 가운데 " + std::to_string(Wanted - Given) + "개를 넣지 못했습니다";
		else
		{
			out.Ok = true;
			out.Note = Wanted > 0 ? label + ": 선호 장비로 정하고 " + std::to_string(Given) + "개를 넣었습니다"
				: label + ": 선호 장비로 정했습니다 (넣을 장비는 이미 갖고 있거나 없습니다)";
		}
		return out;
	}

	bool IsSoldier(const PersonRow& Row)
	{
		return !Row.Character && Row.Strata == 2;
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
		if (Why.empty() && NeedsAmount(Command.Act) && !std::isfinite(Command.Amount))
			Why = "수가 아닙니다";
		if (Why.empty() && gift && (std::round(Command.Amount) == 0 || std::fabs(Command.Amount) > k_GiftMax))
			Why = "줄 수가 0 이거나 너무 큽니다";
		// 지식의 이름은 특성의 이름의 꼴(소문자·숫자·밑줄)에 느낌표를 더 받는다: 게임의 이름 둘에 있다(core/LibraryPlan 의 GoodKnowledgeName. research/33).
		// 게임에 있는 이름인지는 부르는 쪽이 게임의 목록으로 본다.
		if (Why.empty() && Command.Act == PersonAct::Equip && !FindLoadout(Command.Text))
			Why = "모르는 장비 묶음입니다";
		if (Why.empty() && Command.Act == PersonAct::Role && !FindRole(Command.Text))
			Why = "모르는 역할 프리셋입니다";
		if (Why.empty() && Command.Act == PersonAct::Conceive && (!IsUuid(Command.Text) || Command.Text == Command.Who))
			Why = "아버지를 uuid 로 짚습니다 (자기 자신은 안 됩니다)";
		if (Why.empty() && NeedsText(Command.Act) && !GoodPersonText(Command.Act, Command.Text))
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

	bool IsPlayersLord(const PersonRow& Row)
	{
		return IsPlayers(Row) && Row.Character;
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

	bool GoodPersonText(PersonAct Act, const std::string& Text)
	{
		return Act == PersonAct::KnowledgeAdd ? GoodKnowledgeName(Text) : GoodTraitName(Text);
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

	std::vector<int> NeedsToHold(bool NoHunger, bool NoTiredness, bool All, bool Piety)
	{
		std::vector<int> needs;
		for (int i = 0; i < static_cast<int>(NeedNames().size()); i++)
			if (All || (NoHunger && i == 1) || (NoTiredness && (i == 0 || i == 2)) || (Piety && i == 3))
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
