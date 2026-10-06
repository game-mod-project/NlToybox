#pragma once
// 인물·영주·인구 패널의 러너에 기대지 않는 부분: 능력치와 욕구의 이름, 명령의 꼴, 대상 고르기, 쓰는 수 다듬기.
// tests/native 가 직접 부른다. 자리와 게임의 함수는 research/11. 스펙: 치트 메뉴 §8(인물, 영주, 인구·욕구).

#include <cstddef>
#include <string>
#include <vector>

namespace NlCore
{
	struct NamedKey
	{
		const char* Key;		// 게임의 열쇠
		const char* Label;		// 창에 보일 이름(이 레포가 붙인 것이다)
	};

	// 능력치 여덟. Key 는 __soul.__skills.__level 의 이름이다.
	const std::vector<NamedKey>& SkillNames();
	// 욕구 여섯의 이름. 자리가 게임의 번호다(gml_Script_motive_get_caption 이 돌려준 이름).
	const std::vector<const char*>& NeedNames();

	constexpr double k_SkillMax = 20;		// Skills.get_max_level 이 돌려준 수
	constexpr double k_NeedMax = 100;		// __motive_limit 의 수(읽지 못했을 때 쓴다)
	constexpr double k_AgeMin = 1, k_AgeMax = 120;
	constexpr double k_FillAll = 1e9;		// "상한까지 채운다"로 보내는 수(NeedValue 가 상한으로 당긴다). 읽은 상한을 보내지 않는다: 읽지 못했으면 0 이 쓰인다

	constexpr double k_GiftMax = 1e6;		// 소지금·소지품을 한 번에 주거나 빼는 수의 한도
	constexpr int k_ItemIndexMax = 200;		// 자원 번호의 한도(실제 칸의 수는 부르는 쪽이 그 사람의 소지품에서 읽어 본다. 39개였다). 0 번은 신성 반지다

	// KnowledgeAll: 모든 지식을 준다. KnowledgeAdd: 이름으로 지식 하나를 준다. MoneyAdd: 소지금을 더하거나 뺀다. ItemAdd: 소지품의 자원을 더하거나 뺀다(research/12).
	enum class PersonAct
	{
		SkillSet, SkillAdd, SkillsMax, NeedSet, NeedsFill, AgeSet, Happy, Cure, TraitAdd, TraitRemove,
		KnowledgeAll, KnowledgeAdd, MoneyAdd, ItemAdd, Equip,
		Role,		// 역할 프리셋을 입힌다(Text 는 프리셋의 Id. core/RolePlan). 한 사람을 짚어서만
	};

	struct PersonCommand
	{
		PersonAct Act = PersonAct::NeedsFill;
		std::string Who;		// 인물의 uuid. 또는 "lords"(플레이어의 영주 전원), "people"(플레이어의 사람 전원)
		int Index = -1;			// 능력치나 욕구의 번호
		double Amount = 0;		// 맞출 수, 더할 수, 나이
		std::string Text;		// 특성이나 지식의 이름, 장비 묶음의 이름, 역할 프리셋의 Id
	};

	// 원격 명령의 낱말: skill_set, skill_add, skills_max, need_set, needs_fill, age_set, happy, cure, trait_add, trait_remove,
	// knowledge_all, knowledge_add, money_add, item_add, equip, role.
	bool ParsePersonAct(const std::string& Word, PersonAct& Out);
	const char* PersonActWord(PersonAct Act);
	// 그 명령이 번호(능력치·욕구·자원)를, 수를, 글(특성이나 지식의 이름)을 받는가.
	bool NeedsIndex(PersonAct Act);
	// 번호를 받는 명령의 번호의 한도(0 이상 이 수 미만). 번호를 받지 않으면 0.
	int IndexLimit(PersonAct Act);
	bool NeedsAmount(PersonAct Act);
	bool NeedsText(PersonAct Act);
	// 명령이 온전한가: 누구인지 있고, 번호가 범위 안이고, 수가 유한하고, 글이 특성 이름의 꼴이고 붙이거나 떼도 되는 특성이다.
	// 여럿("lords", "people")에게는 SkillsMax·NeedsFill·Happy·Cure 만 된다. 아니면 거짓이고 Why 에 까닭.
	bool CheckPersonCommand(const PersonCommand& Command, std::string& Why);

	// 여럿을 가리키는 글인가("lords", "people").
	bool IsBulkWho(const std::string& Who);
	// 그 대상에게 그 일을 해도 되는가. 한 사람에게는 무엇이든. 여럿에게는 능력치 최대·욕구·행복·치료, 영주 전원에게는 모든 지식도.
	bool BulkAllowed(const std::string& Who, PersonAct Act);

	// 그 자원 번호가 착용 중인 장비인가. Equipped: 착용 중인 것들의 자원 번호(없는 자리는 음수나 수가 아닌 값).
	bool IsEquipped(int Index, const std::vector<double>& Equipped);

	// 선호 장비의 묶음(research/17). 게임은 영혼마다 선호 장비(__preferred_equipment)를 두고 거기에 없는 장비를 무기고로 돌려보낸다.
	// Member: 게임의 자료 o_data.__preferred_equipment_data 의 멤버 이름. 그 구조체를 SoulBasic.set_preferred_equipment 에 넘긴다.
	struct Loadout
	{
		const char* Key;		// 원격 명령의 이름
		const char* Member;
		const char* Label;		// 창에 보일 이름
	};
	const std::vector<Loadout>& Loadouts();
	const Loadout* FindLoadout(const std::string& Key);

	constexpr int k_ShieldResource = 15;		// 자원 번호 15 = 방패(global.__resource_caption 의 15 번이 방패다. research/07, 13)
	// 선호 장비의 갑옷·무기·방패 가운데 소지품에 없어서 넣어 줄 것(자원 번호). 1 보다 작은 번호(-1 없음, -2 아무거나, 0 은 건드리지 않는 자원),
	// 정수가 아닌 수, 소지품의 칸 밖의 번호는 주지 않는다. 방패는 갑옷이나 무기가 정해진 묶음에만 넣는다.
	std::vector<int> EquipGifts(double Armor, double Weapon, bool Shield, const std::vector<double>& Inventory);

	// 장비 지급 한 번의 결과. Stuck: 세터를 부른 뒤 다시 읽은 선호 장비가 그 묶음의 것이다. Wanted: 넣으려던 장비의 수. Given: 넣은 뒤 소지품에 있는 것을 본 수.
	// 선호 장비가 남지 않았거나 넣지 못한 것이 있으면 실패다(여럿에게 할 때 "N/N명"에 섞여 묻히지 않게).
	struct EquipResult
	{
		bool Ok = false;
		std::string Note;
	};
	EquipResult EquipReport(const char* Label, bool Stuck, int Wanted, int Given);

	// 병사인가: 주민 쪽 오브젝트(o_dummy)이고 갈래가 2 다(research/13).
	struct PersonRow;
	bool IsSoldier(const PersonRow& Row);

	// 게임의 디버그 소환기(CreatureSpawner)가 만드는 것 가운데 플레이어의 사람(research/13). 마우스가 가리키는 지도의 자리에 나타난다.
	enum class SpawnKind { Soldier, Knight, Peasant, Slave, Lord };
	bool ParseSpawnKind(const std::string& Word, SpawnKind& Out);
	const char* SpawnWord(SpawnKind Kind);		// 원격 명령의 낱말
	const char* SpawnMethod(SpawnKind Kind);	// 소환기의 메서드 이름(인자 없음)
	const char* SpawnLabel(SpawnKind Kind);		// 창에 보일 이름

	// 한 번에 만드는 병사의 수: 1~20 의 정수. 0 이하이거나 수가 아니면 거짓(research/13 의 디버그 소환을 그만큼 부른다).
	constexpr int k_SoldierBatchMax = 20;
	bool SoldierBatch(double Asked, int& Count);

	// 소지금·소지품에 더할 정수. 가진 것보다 많이 빼지 않는다(0 아래로 내려가지 않는다). 할 것이 없거나(0) 수가 아니면 거짓.
	bool GiftDelta(double Current, double Asked, double& Delta);

	// 누구를 가리키는 글의 꼴: 글자·숫자·밑줄만(uuid, lords, people). 주소에 그대로 들어가지 않지만 로그와 답에 적힌다.
	bool GoodWho(const std::string& Who);

	struct PersonRow		// 한 사람. 틱이 읽어 글로 둔다
	{
		std::string Uuid;		// __soul.__uuid. 사람을 가리는 이름이다
		std::string Name;		// 화면의 이름(__soul.get_name())
		std::string Faction;	// __soul.__faction.__system_name. 플레이어의 사람은 "player"
		bool Character = false;	// o_character 인가(영주·손님). 아니면 o_dummy(주민)
		int Index = 0;			// 그 오브젝트의 몇 번째 인스턴스였는가. 인물이 드나들면 바뀐다: 쓰기 전에 uuid 로 다시 확인한다
		double Strata = 0;		// __soul.__social_strata
		bool Dead = false;		// c_status.__is_dead
	};

	// 죽음은 c_status.__is_dead 에서 읽는다. 그것은 게임의 캐시다: true/false 이거나, 아직 셈하지 않았으면 -4(특성이 바뀌면 게임이 비운다).
	// Unknown 이면 부르는 쪽이 게임의 is_alive() 로 묻는다. -4 를 죽음으로 읽으면 특성을 붙인 바로 뒤의 명령이 그 사람을 놓친다.
	enum class Alive { Yes, No, Unknown };
	Alive AliveFromDeadCache(bool Read, double Raw);

	// 플레이어의 산 사람인가.
	bool IsPlayers(const PersonRow& Row);
	// 명령의 대상들(People 안의 자리). "lords": 플레이어의 o_character, "people": 플레이어의 사람 모두(일괄 명령은 손님과 다른 진영에 가지 않는다).
	// 그 밖: 그 uuid 하나(짚어 고른 것은 손님이어도 된다). 죽은 사람은 어느 쪽에서도 뺀다.
	std::vector<size_t> PickTargets(const std::vector<PersonRow>& People, const std::string& Who);

	// 써 넣을 수. 유한하지 않으면 거짓.
	bool SkillValue(double Asked, double& Out);						// 0~20 의 정수
	bool SkillAfterAdd(double Current, double Delta, double& Out);
	bool NeedValue(double Asked, double Limit, double& Out);		// 0~Limit. Limit 이 0 이하이거나 수가 아니면 거짓
	bool AgeValue(double Asked, double& Out);						// 1~120 의 정수

	// 특성 이름의 꼴: 소문자·숫자·밑줄만, 1~64자. 게임에 있는 이름인지는 부르는 쪽이 게임의 목록으로 본다.
	bool GoodTraitName(const std::string& Name);
	// 붙이지도 떼지도 않는 특성: 종(human, wolf, pig, dog)과 죽음의 상태(dead, lost_head …). 게임이 어떻게 받는지 재지 않았다.
	bool IsProtectedTrait(const std::string& Name);
	// "치료"가 떼는 부상의 특성 이름들(inst:o_data.game_trait_list 에 있는 이름). 병과 출혈은 게임의 함수(cure_all_disease, cure_bleeding)가 한다.
	// 사라지면 안 되는 상태(human, kid, dead, lost_head …)와 영구한 흉터는 넣지 않는다.
	const std::vector<const char*>& WoundTraits();

	// 이 칸을 채울까: 상한에서 0.5 넘게 모자랄 때만(게임은 한 번에 0.3 쯤씩 줄인다. 거의 찬 칸을 틱마다 쓰지 않는다).
	bool ShouldFillNeed(double Current, double Limit);
	// 행복 생각을 붙일까: 생각의 합이 100 아래일 때. 합을 읽지 못했으면, 한 사람을 짚은 명령은 붙이고 여럿을 도는 길(Bulk)은 건너뛴다
	// (읽지 못하는 사람에게 60초마다 되풀이해 쌓이지 않게).
	bool ShouldAttachHappy(bool Read, double MindSum, bool Bulk);

	// 켠 항목에 따라 채워 둘 욕구의 번호들(작은 번호부터). 배고픔 없음 → 음식(1), 피로 없음 → 수면(0)·휴식(2), 모두 → 0~5.
	std::vector<int> NeedsToHold(bool NoHunger, bool NoTiredness, bool All, bool Piety);

	struct PeopleSlice
	{
		size_t Begin = 0, End = 0;	// 이번 틱에 다룰 사람들 [Begin, End)
		size_t Next = 0;			// 다음 틱이 시작할 자리
		bool Wrapped = false;		// 이번으로 한 바퀴가 끝났다
	};
	// 사람이 많으면 한 틱에 다 쓰지 않는다: Count 명 가운데 Cursor 부터 Batch 명. 끝에 닿으면 다음은 처음부터다.
	PeopleSlice NextPeopleSlice(size_t Count, size_t Cursor, size_t Batch);

	// 표의 인구 항목이 사람들을 도는 바퀴의 상태(src/People.cpp 의 HoldTick). 러너와 무관한 판단만 든다.
	struct HoldRound
	{
		size_t Cursor = 0;			// 다음 틱이 시작할 자리
		bool AgeWritten = false;	// "노화로 죽을 수 있다"를 끈 사람이 있다(되돌릴 것이 있다). 첫 깃발을 쓴 때부터 참이다
		bool Restoring = false;		// 지금 도는 바퀴가 되돌리는 바퀴다(항목이 꺼진 채 처음부터 끝까지 돌아야 한다)
		bool RestoreClean = true;	// 되돌리는 바퀴에서 건너뛴 사람이 없다
	};
	struct HoldPlan
	{
		bool Work = false;			// 이번 틱에 사람들을 돈다
		bool Rescan = false;		// 사람들을 다시 읽고 돈다(되돌리는 바퀴를 새로 시작한다)
		bool WriteAge = false;		// 깃발을 쓴다
		double AgeValue = 1;		// 쓸 값: 0 은 "죽지 않는다", 1 은 원래대로
	};
	// 틱의 처음: 켜진 것을 보고 할 일을 정한다. 노화 항목이 꺼졌는데 되돌릴 것이 남아 있으면 처음부터 한 바퀴를 새로 돈다.
	HoldPlan HoldBegin(HoldRound& Round, bool AnyNeeds, bool Happy, bool Ageless);
	// 한 사람을 다룬 뒤: 깃발을 껐는가(WroteAgeOff), 그 사람을 건너뛰었는가(자리의 사람이 바뀌었다).
	void HoldTouched(HoldRound& Round, bool WroteAgeOff, bool Skipped);
	// 묶음을 다룬 뒤. 되돌리는 바퀴가 건너뛴 사람 없이 끝났을 때만 되돌릴 것이 없어진다. 아니면 다음 틱이 다시 한 바퀴를 시작한다.
	void HoldEnd(HoldRound& Round, const PeopleSlice& Slice, bool Ageless);
}
