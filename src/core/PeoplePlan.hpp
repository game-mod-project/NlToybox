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

	enum class PersonAct { SkillSet, SkillAdd, SkillsMax, NeedSet, NeedsFill, AgeSet, Happy, Cure, TraitAdd, TraitRemove };

	struct PersonCommand
	{
		PersonAct Act = PersonAct::NeedsFill;
		std::string Who;		// 인물의 uuid. 또는 "lords"(플레이어의 영주 전원), "people"(플레이어의 사람 전원)
		int Index = -1;			// 능력치나 욕구의 번호
		double Amount = 0;		// 맞출 수, 더할 수, 나이
		std::string Text;		// 특성의 이름
	};

	// 원격 명령의 낱말: skill_set, skill_add, skills_max, need_set, needs_fill, age_set, happy, cure, trait_add, trait_remove.
	bool ParsePersonAct(const std::string& Word, PersonAct& Out);
	const char* PersonActWord(PersonAct Act);
	// 그 명령이 번호(능력치·욕구)를, 수를, 글(특성 이름)을 받는가.
	bool NeedsIndex(PersonAct Act);
	bool NeedsAmount(PersonAct Act);
	bool NeedsText(PersonAct Act);
	// 명령이 온전한가: 누구인지 있고, 번호가 범위 안이고, 수가 유한하고, 글이 특성 이름의 꼴이다. 아니면 거짓이고 Why 에 까닭.
	bool CheckPersonCommand(const PersonCommand& Command, std::string& Why);

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
	// "치료"가 떼는 부상의 특성 이름들(inst:o_data.game_trait_list 에 있는 이름). 병과 출혈은 게임의 함수(cure_all_disease, cure_bleeding)가 한다.
	// 사라지면 안 되는 상태(human, kid, dead, lost_head …)와 영구한 흉터는 넣지 않는다.
	const std::vector<const char*>& WoundTraits();

	// 켠 항목에 따라 채워 둘 욕구의 번호들(작은 번호부터). 배고픔 없음 → 음식(1), 피로 없음 → 수면(0)·휴식(2), 모두 → 0~5.
	std::vector<int> NeedsToHold(bool NoHunger, bool NoTiredness, bool All);

	struct PeopleSlice
	{
		size_t Begin = 0, End = 0;	// 이번 틱에 다룰 사람들 [Begin, End)
		size_t Next = 0;			// 다음 틱이 시작할 자리
		bool Wrapped = false;		// 이번으로 한 바퀴가 끝났다
	};
	// 사람이 많으면 한 틱에 다 쓰지 않는다: Count 명 가운데 Cursor 부터 Batch 명. 끝에 닿으면 다음은 처음부터다.
	PeopleSlice NextPeopleSlice(size_t Count, size_t Cursor, size_t Batch);
}
