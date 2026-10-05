#include "People.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "Recorder.hpp"
#include "core/AskPath.hpp"
#include "core/BattlePlan.hpp"
#include "core/EconomyPlan.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::PersonAct;
using NlCore::PersonCommand;
using NlCore::PersonRow;
using NlCore::Shortest;

namespace
{
	// 자리와 함수는 research/11 에서 잰 것이다(0.5588.9777.0. 불러온 세이브).
	// 게임의 디버그용 생각: 기분 +100, 하루. Minds.attach_generic_mind(생각 구조체)로 붙인다(게임이 그 꼴로 부르는 것을 기록했다).
	constexpr const char* k_HappyMind = "inst:o_data.mind_debug_totally_happy";
	constexpr const char* k_TraitList = "inst:o_data.game_trait_list";		// 특성 이름 282개
	// 지식의 종류 121개(research/12). 한 칸은 구조체: __name, __caption_replaced(화면의 이름), __category.
	constexpr const char* k_KnowledgeList = "inst:o_data.__knowledge_data.__knowledge_list";
	constexpr const char* k_ResourceCaptions = "global.__resource_caption";	// 자원 번호 → "resource.wood"(research/07)
	// 플레이어의 병사 하나를 만드는 게임의 디버그 함수(research/13). 인자를 하나까지 받고 생략할 수 있다(기계어). 인자 없이 불러 병사가 생기고
	// 병영의 목록과 게임의 군대 창에 올라오는 것을 봤다. 지도 가장자리의 자리에 나타나 마을로 걸어온다.
	constexpr const char* k_SpawnSoldier = "gml_Script_rebellion_debug_spawn_player_soldier";
	// 게임의 디버그 소환기(CreatureSpawner. research/13). __spawn_soldier 같은 메서드는 인자가 없고(기계어) 마우스가 가리키는 지도의 자리에 만든다.
	constexpr const char* k_Spawner = "inst:o_debug.debug_spawner";
	// 상처를 입히는 함수(research/13): SoulBasic.take_damage(상처의 이름, 구조체, 불리언) -> true. 생성자의 정적 메서드라 영주 하나의 영혼에서 스크립트를 찾는다.
	constexpr const char* k_TakeDamage = "inst:o_character.__soul.take_damage";
	constexpr double k_Unknown = -1e9;										// 읽지 못한 수

	struct Detail			// 고른 사람의 값. RValue 를 담지 않는다
	{
		bool Ready = false;
		std::string Uuid;
		double Age = k_Unknown, Moral = k_Unknown, Pain = k_Unknown, MindSum = k_Unknown;
		std::vector<double> Skills, Points;		// 능력치 여덟의 등급과 점수. 그 사람에게 없는 칸은 k_Unknown(주민은 전투만 있다)
		std::vector<double> Needs, Limits;		// 욕구 여섯과 상한
		std::vector<std::string> Traits;
		double Money = k_Unknown, KnowledgeCount = k_Unknown;		// 소지금, 가진 지식의 수(지식을 갖지 않는 사람은 k_Unknown)
		std::vector<double> Items;									// 소지품: 자원 번호 → 수
		std::vector<double> Equipped;								// 착용 중인 장비의 자원 번호(갑옷, 첫째 손, 둘째 손. 없으면 음수)
	};

	struct KnowledgeName
	{
		std::string Name, Caption, Category;		// 게임의 이름("building_mine"), 화면의 이름("광산"), 갈래(economic, cultural_knowledge, textbooks)
	};

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다
	{
		bool Ready = false;
		std::string Why;
		std::vector<PersonRow> People;
		std::vector<std::string> TraitNames;	// 게임에 있는 특성의 이름(이름순)
		std::vector<KnowledgeName> Knowledge;	// 게임에 있는 지식(게임의 목록의 차례. 자리가 __knowledge_list 의 번호다)
		std::vector<std::string> Resources;		// 자원 번호 → 창에 보일 이름
		Detail One;
		std::string Last;						// 마지막으로 한 일
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlPeople::LogFn g_Log;
	Snapshot g_Now;
	std::deque<PersonCommand> g_Queue;				// 창이 쌓고 틱이 한다
	std::string g_Selected;							// 고른 사람의 uuid
	std::map<std::string, std::string> g_Names;		// uuid → 화면의 이름(get_name 은 사람마다 한 번만 부른다)
	double g_NextScan = 0, g_NextDetail = 0;
	std::string g_DetailLogged;						// 읽기 함수를 부른다고 로그에 남긴 사람(사람마다 한 번만 남긴다)

	// 창의 입력
	bool g_ShowAll = false;				// 주민과 손님도 목록에 보인다
	int g_AgeInput = 0;
	std::string g_AgeInputFor;			// 입력 칸을 누구의 나이로 채웠는가
	char g_TraitFilter[48] = "";
	char g_KnowledgeFilter[48] = "";
	int g_SpawnQueued = 0;				// 창이 청한 병사의 수(틱이 만든다)
	std::deque<NlCore::SpawnKind> g_SpawnKinds;		// 창이 청한 "마우스 자리에 소환"(틱마다 하나씩 한다)
	double g_NextShield = 0;			// 전투의 항목들: 영혼의 주소를 다시 모을 시각
	bool g_BattleOn = false;			// 전투의 항목 가운데 하나라도 걸어 두었다(끌 때 주소 묶음을 비운다)

	struct SideHook			// 영혼의 한 함수에 거는 아군·적 배율
	{
		const char* Path;				// 영혼의 메서드(생성자의 정적 메서드라 영주 하나의 영혼에서 스크립트를 찾는다)
		const char* Ally;				// 아군 배율 항목의 Id
		const char* Enemy;				// 적 배율 항목의 Id
		double Cap;						// 올린 값의 위쪽 한도(0 이면 없음)
		NlCore::SideScale Applied;		// 걸어 둔 것
		std::string Name;				// 건 스크립트의 이름
	};
	SideHook g_SideHooks[] = {
		// 싸울 때의 전투 기술: () -> 10, 7(research/13. 싸우는 동안 150번). 기술은 0~20 이라 올린 값을 20 에서 멈춘다(표를 번호로 읽는 곳이 있을 수 있다. 추정).
		{ "inst:o_character.__soul.get_combat_level_in_battle", "ally_power", "enemy_power", 20, {}, {} },
		// 치명적인 통증의 한도: () -> 40(research/13. 게임이 계속 부른다).
		{ "inst:o_character.__soul.get_mortal_pain_threshold", "ally_toughness", "enemy_toughness", 0, {}, {} },
	};
	bool g_ShieldOn = false;			// 아군 무적의 바꾸기를 이 모듈이 걸었다
	std::string g_ShieldName;			// 건 스크립트의 이름
	bool g_BulkKnowledgeArmed = false;	// "영주 전원에게 모든 지식"은 이것을 켠 뒤에만 눌린다(되돌릴 수 없다)

	bool g_AliveLogged = false;			// is_alive() 를 부른다고 로그에 남겼는가(게임마다 한 번)
	bool g_Busy = false;				// 틱이나 원격 명령을 하는 중이다. 여기서 부른 게임의 함수가 오브젝트 이벤트를 일으켜 다시 들어오면 안쪽은 아무것도 하지 않는다

	struct Busy
	{
		Busy() { g_Busy = true; }
		~Busy() { g_Busy = false; }
	};

	// 표의 항목(플레이어의 사람을 조금씩 돌며 쓴다). 바퀴의 판단은 core/PeoplePlan 의 HoldRound 가 한다.
	std::vector<PersonRow> g_HoldPeople;
	NlCore::HoldRound g_Hold;
	double g_NextHold = 0, g_NextHoldScan = 0, g_NextHappy = 0, g_NextHoldLog = 0;
	bool g_HappyRound = false;			// 이번 바퀴에서 행복 생각을 본다
	bool g_HoldNoted = false;			// 항목 옆에 글을 적어 두었다(할 일이 없어지면 한 번 비운다)
	size_t g_RoundPeople = 0;
	size_t g_LogNeeds = 0, g_LogHappy = 0, g_LogAge = 0;		// 마지막 로그 줄 뒤로 쓴 수

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// ---- 게임 스레드 ----

	std::string Base(const PersonRow& Row)
	{
		return std::string("inst:") + (Row.Character ? "o_character" : "o_dummy") + ":" + std::to_string(Row.Index);
	}

	bool FollowString(const RValue& From, const std::vector<PathStep>& Steps, std::string& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Follow(From, Steps, value, why) || !value.IsString())
			return false;
		Out = value.ToString();
		return true;
	}

	bool FollowNumber(const RValue& From, const std::vector<PathStep>& Steps, double& Out)
	{
		RValue value;
		std::string why;
		if (!NlAccess::Follow(From, Steps, value, why) || !NlGame::IsNumber(value))
			return false;
		Out = value.ToDouble();
		return true;
	}

	// 그 사람의 __soul. 없으면 거짓.
	bool ReadSoul(const PersonRow& Row, RValue& Soul)
	{
		std::string why;
		return NlAccess::Read(NlCore::ParseAskPath(Base(Row) + ".__soul"), Soul, why) && Soul.IsStruct();
	}

	// 인자 없는 메서드를 부른다. 게임이 인자 없이 부르는 것을 기록한 것에만 쓴다(research/11).
	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
	}

	bool CallNumber(const std::string& Path, double& Out)
	{
		RValue result;
		std::string why;
		if (!CallNoArgs(Path, result, why) || !NlGame::IsNumber(result))
			return false;
		Out = result.ToDouble();
		return true;
	}

	// 지식의 목록과 자원의 이름. 게임마다 한 번 읽는다.
	void LoadTables()
	{
		RValue list;
		std::string why;
		g_Now.Knowledge.clear();
		if (NlAccess::Read(NlCore::ParseAskPath(k_KnowledgeList), list, why) && list.IsArray())
			NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& item) {
				KnowledgeName one;
				if (item.IsStruct())
				{
					FollowString(item, { { '.', "__name", 0 } }, one.Name);
					FollowString(item, { { '.', "__caption_replaced", 0 } }, one.Caption);
					FollowString(item, { { '.', "__category", 0 } }, one.Category);
				}
				g_Now.Knowledge.push_back(std::move(one));		// 읽지 못한 칸도 자리를 지킨다(자리가 번호다)
				return true;
			});

		RValue captions;
		g_Now.Resources.clear();
		if (NlAccess::Read(NlCore::ParseAskPath(k_ResourceCaptions), captions, why) && captions.IsArray())
			NlAccess::ForEachChild(captions, Holder::Array, [&](const PathStep& step, const RValue& caption) {
				// 번호의 자리에 놓는다. 읽지 못해 건너뛴 칸이 있어도 이름과 번호가 어긋나지 않는다.
				const size_t at = step.Index >= 0 ? static_cast<size_t>(step.Index) : g_Now.Resources.size();
				if (at >= 1000)
					return false;
				while (g_Now.Resources.size() <= at)
					g_Now.Resources.push_back("#" + std::to_string(g_Now.Resources.size()));
				if (caption.IsString())
					g_Now.Resources[at] = NlCore::ResourceLabel(NlCore::ResourceKey(caption.ToString()));
				return true;
			});
		Log("people: " + std::to_string(g_Now.Knowledge.size()) + " knowledge names, " + std::to_string(g_Now.Resources.size()) + " resources");
	}

	void LoadTraitNames()
	{
		RValue list;
		std::string why;
		g_Now.TraitNames.clear();
		if (!NlAccess::Read(NlCore::ParseAskPath(k_TraitList), list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (name.IsString() && NlCore::GoodTraitName(name.ToString()))
				g_Now.TraitNames.push_back(name.ToString());
			return true;
		});
		std::sort(g_Now.TraitNames.begin(), g_Now.TraitNames.end());
		g_Now.TraitNames.erase(std::unique(g_Now.TraitNames.begin(), g_Now.TraitNames.end()), g_Now.TraitNames.end());
		Log("people: " + std::to_string(g_Now.TraitNames.size()) + " trait names");
	}

	// 사람들을 다시 읽는다. 읽지 못하면 거짓이고 g_Now.Why 에 까닭.
	bool Scan()
	{
		if (!NlAccess::InGame())
		{
			g_Now.Ready = false;
			g_Now.Why = "게임 화면이 아닙니다";
			g_Now.People.clear();
			g_Now.One = Detail();
			g_Names.clear();
			g_AliveLogged = false;
			return false;
		}

		std::vector<PersonRow> people;
		for (const bool character : { true, false })
		{
			const char* object = character ? "o_character" : "o_dummy";
			const int count = NlAccess::InstanceCount(object);
			bool announced = false;		// 이 훑기에서 이 종류에 get_name 을 부른다고 남겼는가
			for (int n = 0; n < count; n++)
			{
				PersonRow row;
				row.Character = character;
				row.Index = n;
				RValue soul;
				if (!ReadSoul(row, soul) || !FollowString(soul, { { '.', "__uuid", 0 } }, row.Uuid) || row.Uuid.empty())
					continue;
				FollowString(soul, { { '.', "__faction", 0 }, { '.', "__system_name", 0 } }, row.Faction);
				FollowNumber(soul, { { '.', "__social_strata", 0 } }, row.Strata);
				// c_status.__is_dead 는 게임의 캐시다. 특성이 바뀌면 -4 로 비워진다(research/11). 비어 있으면 게임에 묻는다:
				// SoulBasic.is_alive() -> 불리언(게임이 인자 없이 부르는 것을 기록했다).
				double dead = 0;
				const bool read = NlAccess::ReadNumber(Base(row) + ".c_status.__is_dead", dead);
				NlCore::Alive alive = NlCore::AliveFromDeadCache(read, dead);
				if (alive == NlCore::Alive::Unknown)
				{
					if (!g_AliveLogged)
					{
						g_AliveLogged = true;
						Log("people call is_alive() where the death cache is empty (first: " + Base(row) + " " + row.Uuid + ")");		// 부르기 전에 남긴다
					}
					RValue answer;
					std::string why;
					if (CallNoArgs(Base(row) + ".__soul.is_alive", answer, why) && NlGame::IsNumber(answer))
						alive = answer.ToDouble() != 0 ? NlCore::Alive::Yes : NlCore::Alive::No;
				}
				row.Dead = alive != NlCore::Alive::Yes;		// 알 수 없는 사람은 건드리지 않는다

				auto known = g_Names.find(row.Uuid);
				if (known == g_Names.end())
				{
					// SoulBasic.get_name(): 게임이 인자 없이 불러 화면의 이름("Barra")을 받는 것을 기록했다. 처음 본 사람에게 한 번만 부른다.
					RValue name;
					std::string why;
					if (!announced)
					{
						announced = true;
						Log(std::string("people call get_name() on new ") + object + " (first: " + std::to_string(n) + " " + row.Uuid + ")");		// 부르기 전에 남긴다
					}
					if (CallNoArgs(Base(row) + ".__soul.get_name", name, why) && name.IsString())
						known = g_Names.emplace(row.Uuid, name.ToString()).first;
				}
				row.Name = known != g_Names.end() ? known->second : row.Uuid;
				people.push_back(std::move(row));
			}
		}

		if (!g_Now.Ready)
		{
			LoadTraitNames();
			LoadTables();
		}
		g_Now.People = std::move(people);
		g_Now.Ready = true;
		g_Now.Why.clear();
		return true;
	}

	const PersonRow* FindRow(const std::string& Uuid)
	{
		for (const PersonRow& row : g_Now.People)
			if (row.Uuid == Uuid)
				return &row;
		return nullptr;
	}

	// 그 자리의 인스턴스가 아직 그 사람인가(인물이 드나들면 n 번째가 다른 사람이 된다).
	bool StillThere(const PersonRow& Row, RValue& Soul)
	{
		std::string uuid;
		return ReadSoul(Row, Soul) && FollowString(Soul, { { '.', "__uuid", 0 } }, uuid) && uuid == Row.Uuid;
	}

	void ReadNumbers(const RValue& Soul, const std::vector<PathStep>& Steps, std::vector<double>& Out)
	{
		RValue list;
		std::string why;
		Out.clear();
		if (!NlAccess::Follow(Soul, Steps, list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& value) {
			Out.push_back(NlGame::IsNumber(value) ? value.ToDouble() : k_Unknown);
			return true;
		});
	}

	void ReadTraits(const RValue& Soul, std::vector<std::string>& Out)
	{
		RValue list;
		std::string why;
		Out.clear();
		if (!NlAccess::Follow(Soul, { { '.', "__traits", 0 }, { '.', "__list_of_traits", 0 } }, list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (name.IsString())
				Out.push_back(name.ToString());
			return true;
		});
	}

	// 착용 중인 장비의 자원 번호들(값 읽기). __soul.__equipment.__cached_armor·__cached_first_arm·__cached_second_arm 의 __resource(research/12: 6, 10, -1).
	void ReadEquipped(const RValue& Soul, std::vector<double>& Out)
	{
		Out.clear();
		for (const char* slot : { "__cached_armor", "__cached_first_arm", "__cached_second_arm" })
		{
			double resource = -1;
			FollowNumber(Soul, { { '.', "__equipment", 0 }, { '.', slot, 0 }, { '.', "__resource", 0 } }, resource);
			Out.push_back(resource);
		}
	}

	// 한 사람의 값을 읽는다.
	bool ReadDetail(const PersonRow& Row, Detail& Out)
	{
		RValue soul;
		Out = Detail();
		if (!StillThere(Row, soul))
			return false;
		Out.Uuid = Row.Uuid;
		for (const NlCore::NamedKey& skill : NlCore::SkillNames())
		{
			double level = k_Unknown, points = 0;
			FollowNumber(soul, { { '.', "__skills", 0 }, { '.', "__level", 0 }, { '.', skill.Key, 0 } }, level);
			FollowNumber(soul, { { '.', "__skills", 0 }, { '.', "__points", 0 }, { '.', skill.Key, 0 } }, points);
			Out.Skills.push_back(level);
			Out.Points.push_back(points);
		}
		ReadNumbers(soul, { { '.', "__motive", 0 }, { '.', "__motive", 0 } }, Out.Needs);
		ReadNumbers(soul, { { '.', "__motive", 0 }, { '.', "__motive_limit", 0 } }, Out.Limits);
		FollowNumber(soul, { { '.', "__moral", 0 } }, Out.Moral);
		ReadTraits(soul, Out.Traits);

		// 게임이 인자 없이 부르는 것을 기록한 읽기 함수들: get_age() -> 나이, get_pain() -> 통증, Minds.get_total_modify() -> 생각의 합.
		const std::string base = Base(Row) + ".__soul";
		if (g_DetailLogged != Row.Uuid)
		{
			g_DetailLogged = Row.Uuid;
			Log("people call get_age(), get_pain(), get_total_modify(), get_knowledge_count() on " + Base(Row) + " " + Row.Uuid);		// 사람마다 한 번만 남긴다
		}
		CallNumber(base + ".get_age", Out.Age);
		CallNumber(base + ".get_pain", Out.Pain);
		CallNumber(base + ".__minds.get_total_modify", Out.MindSum);
		// 소지금과 소지품은 값으로 읽는다. 가진 지식의 수는 ComponentKnowledge.get_knowledge_count()(인자 없음. 3, 4, 121 을 돌려줬다).
		FollowNumber(soul, { { '.', "__inventory", 0 }, { '.', "__money", 0 } }, Out.Money);
		ReadNumbers(soul, { { '.', "__inventory", 0 }, { '.', "__resources", 0 } }, Out.Items);
		ReadEquipped(soul, Out.Equipped);
		CallNumber(base + ".__character_soul.__knowledge.get_knowledge_count", Out.KnowledgeCount);
		Out.Ready = true;
		return true;
	}

	// 게임의 지식 목록에서의 자리. 없으면 -1.
	int KnowledgeIndex(const std::string& Name)
	{
		for (size_t i = 0; i < g_Now.Knowledge.size(); i++)
			if (g_Now.Knowledge[i].Name == Name)
				return static_cast<int>(i);
		return -1;
	}

	bool KnownTrait(const std::string& Name)
	{
		return std::binary_search(g_Now.TraitNames.begin(), g_Now.TraitNames.end(), Name);
	}

	bool Has(const std::vector<std::string>& List, const std::string& Name)
	{
		return std::find(List.begin(), List.end(), Name) != List.end();
	}

	// 수 하나를 쓴다(있는 자리에만. 쓴 뒤 다시 읽어 확인한다).
	bool WriteAt(const std::string& Path, double Value, std::string& Note)
	{
		std::string why;
		if (NlAccess::WriteNumber(Path, Value, why))
			return true;
		Note = why;
		return false;
	}

	double NeedLimit(const std::string& Soul, int Index)
	{
		double limit = NlCore::k_NeedMax;
		NlAccess::ReadNumber(Soul + ".__motive.__motive_limit[" + std::to_string(Index) + "]", limit);
		return limit;
	}

	bool SetNeed(const std::string& Soul, int Index, double Asked, std::string& Note)
	{
		double value = 0;
		if (!NlCore::NeedValue(Asked, NeedLimit(Soul, Index), value))
		{
			Note = "욕구의 상한을 읽지 못했습니다";
			return false;
		}
		return WriteAt(Soul + ".__motive.__motive[" + std::to_string(Index) + "]", value, Note);
	}

	// 특성을 뗀다: Traits.trait_detach("이름"). 게임이 글 하나로 부르는 것을 기록했다.
	bool Detach(const PersonRow& Row, const std::string& Name, std::string& Note)
	{
		Log("people call trait_detach(" + Name + ") on " + Row.Uuid);		// 부르기 전에 남긴다
		RValue result;
		return NlAccess::CallMethod(NlCore::ParseAskPath(Base(Row) + ".__soul.__traits.trait_detach"), { RValue(std::string_view(Name)) }, result, Note);
	}

	// 한 사람에게 명령 하나를 한다. 안 됐으면 거짓이고 Note 에 까닭. 됐을 때의 Note 는 덧붙일 말(없어도 된다).
	// Bulk: 여럿을 도는 길이다(일괄 명령, 표의 항목). 읽지 못한 사람은 건너뛴다.
	bool One(const PersonCommand& C, const PersonRow& Row, std::string& Note, bool Bulk = false)
	{
		RValue soul_value;
		if (!StillThere(Row, soul_value))
		{
			Note = "그 자리의 사람이 바뀌었습니다";
			return false;
		}
		const std::string soul = Base(Row) + ".__soul";
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		RValue result;

		switch (C.Act)
		{
		case PersonAct::SkillSet:
		case PersonAct::SkillAdd:
		{
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
		case PersonAct::SkillsMax:
		{
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
		case PersonAct::NeedSet:
			return SetNeed(soul, C.Index, C.Amount, Note);
		case PersonAct::NeedsFill:
		{
			size_t written = 0;
			for (int i = 0; i < static_cast<int>(NlCore::NeedNames().size()); i++)
				written += SetNeed(soul, i, NlCore::k_NeedMax * 100, Note);		// 상한까지
			if (written)
				Note.clear();
			return written > 0;
		}
		case PersonAct::AgeSet:
		{
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
		case PersonAct::Happy:
		{
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
		case PersonAct::Cure:
		{
			// Traits.cure_all_disease(), cure_bleeding(): 게임이 인자 없이 부르는 것을 기록했다. 부상은 특성이라 이름으로 뗀다.
			Log("people call cure_all_disease(), cure_bleeding() on " + Row.Uuid);
			if (!CallNoArgs(soul + ".__traits.cure_all_disease", result, Note) || !CallNoArgs(soul + ".__traits.cure_bleeding", result, Note))
				return false;
			std::vector<std::string> traits;
			ReadTraits(soul_value, traits);
			size_t removed = 0;
			for (const char* wound : NlCore::WoundTraits())
				if (Has(traits, wound) && Detach(Row, wound, Note))
					removed++;
			Note = removed ? "부상 " + std::to_string(removed) + "개를 뗐습니다" : "";
			return true;
		}
		case PersonAct::TraitAdd:
		{
			std::vector<std::string> traits;
			ReadTraits(soul_value, traits);
			if (Has(traits, C.Text))
			{
				Note = "이미 있는 특성입니다";
				return true;
			}
			// Traits.trait_attach("이름") -> uuid: 게임이 글 하나로 부르는 것을 기록했다.
			Log("people call trait_attach(" + C.Text + ") on " + Row.Uuid);
			if (!NlAccess::CallMethod(NlCore::ParseAskPath(soul + ".__traits.trait_attach"), { RValue(std::string_view(C.Text)) }, result, Note))
				return false;
			if (!StillThere(Row, soul_value))
				return false;
			ReadTraits(soul_value, traits);
			if (!Has(traits, C.Text))
			{
				Note = "게임이 붙이지 않았습니다";		// 함께 가질 수 없는 특성이 있다
				return false;
			}
			return true;
		}
		case PersonAct::TraitRemove:
		{
			std::vector<std::string> traits;
			ReadTraits(soul_value, traits);
			if (!Has(traits, C.Text))
			{
				Note = "없는 특성입니다";
				return false;
			}
			return Detach(Row, C.Text, Note);
		}
		case PersonAct::KnowledgeAll:
		{
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
		case PersonAct::KnowledgeAdd:
		{
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
		case PersonAct::MoneyAdd:
		{
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
		case PersonAct::ItemAdd:
		{
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
		if (trait && !KnownTrait(C.Text))
			return "게임에 없는 특성입니다: " + C.Text;
		if (C.Act == PersonAct::KnowledgeAdd && KnowledgeIndex(C.Text) < 0)
			return "게임에 없는 지식입니다: " + C.Text;

		// 대상의 사본을 먼저 뜬다. 아래에서 부르는 게임의 함수가 사람들의 목록을 바꿔도(드나듦, 다시 읽기) 낡은 자리로 목록을 다시 찾지 않는다.
		std::vector<PersonRow> targets;
		for (const size_t at : NlCore::PickTargets(g_Now.People, C.Who))
			targets.push_back(g_Now.People[at]);
		if (targets.empty())
			return "대상이 없습니다";

		const bool bulk = NlCore::IsBulkWho(C.Who);
		size_t done = 0;
		std::string first_failure, note;
		for (const PersonRow& row : targets)
		{
			std::string one;
			if (One(C, row, one, bulk))
			{
				done++;
				if (targets.size() == 1)
					note = one;
			}
			else if (first_failure.empty())
				first_failure = row.Name + ": " + (one.empty() ? "하지 못했습니다" : one);
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
		g_NextShield = 0;		// 새 병사의 영혼을 바로 다음 틱에 묶음에 넣는다(전투의 항목들)
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
		g_NextShield = 0;		// 새 사람의 영혼을 바로 다음 틱에 묶음에 넣는다(전투의 항목들)
		Log("people: spawner " + std::string(NlCore::SpawnWord(Kind)) + ", people " + std::to_string(before) + " -> " + std::to_string(after));
		return label + (after > before ? " 하나를 만들었습니다" : ": 불렀지만 사람의 수가 그대로입니다")
			+ " (영주와 주민 " + std::to_string(before) + "명에서 " + std::to_string(after) + "명으로)";
	}

	// 아군 무적을 끈다(걸어 둔 것이 있으면).
	void ShieldOff()
	{
		if (!g_ShieldOn)
			return;
		g_ShieldOn = false;
		NlRecorder::Unoverride(g_ShieldName);
		NlCheats::SetNote("ally_invincible", std::string());
		Log("people: ally_invincible off");
	}

	// 한 함수의 아군·적 배율을 끈다(걸어 둔 것이 있으면).
	void SideOff(SideHook& Hook)
	{
		if (!Hook.Applied.On)
			return;
		Hook.Applied = NlCore::SideScale();
		NlRecorder::Unoverride(Hook.Name);
		NlCheats::SetNote(Hook.Ally, std::string());
		NlCheats::SetNote(Hook.Enemy, std::string());
		Log(std::string("people: ") + Hook.Ally + "/" + Hook.Enemy + " off");
	}

	// 전투의 항목들(아군 무적, 아군·적의 전투력과 맷집): 훅이 self 가 플레이어의 영혼인지로 가린다. 영혼의 주소는 0.5초마다 다시 모은다.
	// 새로 온 플레이어의 사람(이주민, 태어난 아이)은 다음에 모을 때까지 "그 밖"으로 읽힌다: 0.5초까지 무적이 아니고 적의 배율을 받는다.
	// 모듈이 만든 사람(병사 추가, 소환)은 만든 바로 다음 틱에 다시 모은다. 사라진 영혼의 주소는 다음에 모을 때 빠진다.
	void BattleTick(double Now)
	{
		if (Now < g_NextShield)
			return;
		g_NextShield = Now + 0.5;
		constexpr const char* id = "ally_invincible";
		const bool shield = NlCheats::IsOn(id);
		NlCore::SideScale want[std::size(g_SideHooks)];
		bool any = shield;
		for (size_t i = 0; i < std::size(g_SideHooks); i++)
		{
			double ally = 0, enemy = 0;
			const bool ally_on = NlCheats::Factor(g_SideHooks[i].Ally, ally);
			const bool enemy_on = NlCheats::Factor(g_SideHooks[i].Enemy, enemy);
			want[i] = NlCore::PlanSides(ally_on, ally, enemy_on, enemy);
			any = any || want[i].On;
		}
		if (!any)
		{
			if (g_BattleOn)
			{
				g_BattleOn = false;
				ShieldOff();
				for (SideHook& hook : g_SideHooks)
					SideOff(hook);
				NlRecorder::SetPlayerSelves({});
			}
			return;
		}
		if (!NlAccess::InGame())
		{
			// 지난 게임의 주소가 남지 않게 묶음을 비운다. 배율의 바꾸기도 끈다: 다음 게임에서 "주소부터 넣고 건다"의 차례를 되살린다
			// (묶음이 빈 채 배율이 걸려 있으면 모두가 적의 배율을 받는다. 훅 쪽도 빈 묶음에는 곱하지 않는다: core 의 HookFactor).
			for (SideHook& hook : g_SideHooks)
			{
				SideOff(hook);
				if (NlCheats::IsOn(hook.Ally))
					NlCheats::SetNote(hook.Ally, "게임을 시작하면 적용");
				if (NlCheats::IsOn(hook.Enemy))
					NlCheats::SetNote(hook.Enemy, "게임을 시작하면 적용");
			}
			NlRecorder::SetPlayerSelves({});
			if (shield)
				NlCheats::SetNote(id, "게임을 시작하면 적용");
			return;
		}
		g_BattleOn = true;
		std::vector<std::uintptr_t> selves;
		for (const bool character : { true, false })
		{
			const int count = NlAccess::InstanceCount(character ? "o_character" : "o_dummy");
			for (int n = 0; n < count; n++)
			{
				PersonRow row;
				row.Character = character;
				row.Index = n;
				RValue soul;
				std::string faction;
				if (ReadSoul(row, soul) && soul.IsStruct()
					&& FollowString(soul, { { '.', "__faction", 0 }, { '.', "__system_name", 0 } }, faction) && faction == "player")
					selves.push_back(reinterpret_cast<std::uintptr_t>(soul.m_Object));		// 구조체의 주소. 훅의 self 와 견준다
			}
		}
		const size_t count = selves.size();
		NlRecorder::SetPlayerSelves(std::move(selves));		// 주소부터 넣고 건다

		// 아군·적의 배율: 한 함수에 둘을 함께 건다(바뀌었으면 같은 훅에 다시 건다).
		for (size_t i = 0; i < std::size(g_SideHooks); i++)
		{
			SideHook& hook = g_SideHooks[i];
			if (!want[i].On)
			{
				SideOff(hook);
				continue;
			}
			if (!NlCore::SameSides(hook.Applied, want[i]) || !NlRecorder::Overriding(hook.Name))
			{
				NlRecorder::Forced value;
				value.Kind = 'x';
				value.Number = want[i].Mine;
				value.Other = want[i].Other;
				value.Who = 'p';
				value.Whole = true;		// 정수로 돌아온 값은 정수로 남기고 양수는 1 아래로 내리지 않는다(본 값은 10, 7, 5, 3, 40 모두 정수다)
				value.Cap = hook.Cap;
				std::string name, why;
				if (!NlRecorder::Override(hook.Path, value, name, why))
				{
					NlCheats::SetNote(hook.Ally, NlCheats::IsOn(hook.Ally) ? "걸지 못했습니다: " + why : std::string());
					NlCheats::SetNote(hook.Enemy, NlCheats::IsOn(hook.Enemy) ? "걸지 못했습니다: " + why : std::string());
					continue;
				}
				hook.Applied = want[i];
				hook.Name = name;
				Log(std::string("people: ") + hook.Ally + " x" + Shortest(want[i].Mine) + ", " + hook.Enemy + " x" + Shortest(want[i].Other) + ", " + std::to_string(count) + " souls");
			}
			// 곱한 호출의 수를 보인다: 아군의 것은 self 가 플레이어의 영혼이었던 호출, 적의 것은 그 밖의 호출.
			uint64_t mine = 0, others = 0;
			NlRecorder::Counts(hook.Name, mine, others);
			NlCheats::SetNote(hook.Ally, want[i].Mine != 1 ? "아군의 호출 " + std::to_string(mine) + "번에 곱함" : std::string());
			NlCheats::SetNote(hook.Enemy, want[i].Other != 1 ? "그 밖의 호출 " + std::to_string(others) + "번에 곱함" : std::string());
		}

		if (!shield)
		{
			ShieldOff();
			return;
		}
		if (!g_ShieldOn || !NlRecorder::Overriding(g_ShieldName))
		{
			NlRecorder::Forced value;
			value.Kind = 'b';
			value.Number = 0;
			value.Skip = true;		// 들어올 때 Result 가 undefined 인 것을 표본에서 봤다(research/13)
			value.Who = 'p';
			std::string name, why;
			if (!NlRecorder::Override(k_TakeDamage, value, name, why))
			{
				NlCheats::SetNote(id, "걸지 못했습니다: " + why);
				return;
			}
			g_ShieldOn = true;
			g_ShieldName = name;
			Log("people: ally_invincible on, " + std::to_string(count) + " souls");
		}
		// 넣은 주소의 수와, 훅이 실제로 막은 호출·지나가게 둔 호출의 수를 함께 보인다(주소를 넣었다는 것이 막았다는 뜻은 아니다).
		uint64_t applied = 0, passed = 0;
		NlRecorder::Counts(g_ShieldName, applied, passed);
		NlCheats::SetNote(id, "플레이어의 사람 " + std::to_string(count) + "명의 주소를 넣음. 막은 상처 " + std::to_string(applied)
			+ ", 그대로 둔 상처 " + std::to_string(passed));
	}

	void RefreshDetail()
	{
		const PersonRow* row = g_Selected.empty() ? nullptr : FindRow(g_Selected);
		if (!row || !ReadDetail(*row, g_Now.One))
			g_Now.One = Detail();
	}

	// ---- 표의 항목: 플레이어의 사람을 조금씩 돌며 쓴다 ----

	constexpr const char* k_HoldIds[] = { "no_hunger", "no_tiredness", "needs_full", "always_happy", "no_old_age_death" };

	void HoldNotes(const std::string& Note)
	{
		g_HoldNoted = true;
		for (const char* id : k_HoldIds)
			NlCheats::SetNote(id, NlCheats::IsOn(id) ? Note : std::string());
	}

	void ClearHoldNotes()
	{
		if (!g_HoldNoted)
			return;
		g_HoldNoted = false;
		for (const char* id : k_HoldIds)
			NlCheats::SetNote(id, std::string());
	}

	void HoldTick(double Now)
	{
		// 이 함수는 오브젝트 이벤트마다 불린다. 시각부터 본다: 아래의 것들(항목 읽기, 게임 화면인지)은 0.25초에 한 번만 한다.
		if (Now < g_NextHold)
			return;
		g_NextHold = Now + 0.25;

		const bool hunger = NlCheats::IsOn("no_hunger"), tired = NlCheats::IsOn("no_tiredness"), all = NlCheats::IsOn("needs_full");
		const bool happy = NlCheats::IsOn("always_happy"), ageless = NlCheats::IsOn("no_old_age_death");
		const std::vector<int> needs = NlCore::NeedsToHold(hunger, tired, all);
		if (needs.empty() && !happy && !ageless && !g_Hold.AgeWritten)
		{
			g_Hold.Cursor = 0;
			ClearHoldNotes();		// 끈 항목 옆에 "적용 중"이 남지 않게
			return;
		}
		if (!NlAccess::InGame())
		{
			HoldNotes("게임을 시작하면 적용");
			g_Hold = NlCore::HoldRound();		// 새로 불러온 게임의 깃발은 처음 값이다(세이브에 남지 않는다)
			g_HoldPeople.clear();
			return;
		}

		const NlCore::HoldPlan plan = NlCore::HoldBegin(g_Hold, !needs.empty(), happy, ageless);
		if (!plan.Work)
			return;

		if (g_Hold.Cursor == 0)
		{
			// 바퀴의 처음: 사람들을 다시 읽는다(5초에 한 번까지. 되돌리는 바퀴를 시작할 때는 바로).
			if (plan.Rescan || Now >= g_NextHoldScan || g_HoldPeople.empty())
			{
				g_NextHoldScan = Now + 5;
				if (!Scan())
					return;
				g_HoldPeople.clear();
				for (const size_t at : NlCore::PickTargets(g_Now.People, "people"))
					g_HoldPeople.push_back(g_Now.People[at]);
			}
			// 행복 생각은 60초에 한 바퀴만 본다(사람마다 게임의 함수를 부르고, 붙일 때마다 로그를 남긴다. 생각은 하루 동안 간다).
			g_HappyRound = happy && Now >= g_NextHappy;
			if (g_HappyRound)
				g_NextHappy = Now + 60;
			g_RoundPeople = 0;
		}

		const NlCore::PeopleSlice slice = NlCore::NextPeopleSlice(g_HoldPeople.size(), g_Hold.Cursor, 40);
		for (size_t i = slice.Begin; i < slice.End && i < g_HoldPeople.size(); i++)
		{
			const PersonRow row = g_HoldPeople[i];		// 사본(아래의 Scan 이나 게임의 함수가 목록을 바꿔도 흔들리지 않게)
			RValue soul;
			if (!StillThere(row, soul))
			{
				g_NextHoldScan = 0;		// 사람이 드나들었다. 다음 바퀴에서 다시 읽는다
				NlCore::HoldTouched(g_Hold, false, true);
				continue;
			}
			g_RoundPeople++;
			std::string why;

			if (!needs.empty())
			{
				RValue values, limits;
				if (NlAccess::Follow(soul, { { '.', "__motive", 0 }, { '.', "__motive", 0 } }, values, why) && values.IsArray())
				{
					const bool have_limits = NlAccess::Follow(soul, { { '.', "__motive", 0 }, { '.', "__motive_limit", 0 } }, limits, why) && limits.IsArray();
					for (const int need : needs)
					{
						const PathStep step{ '[', "", static_cast<double>(need) };
						double current = 0, limit = NlCore::k_NeedMax, wanted = 0;
						if (!FollowNumber(values, { step }, current))
							continue;
						if (have_limits)
							FollowNumber(limits, { step }, limit);
						if (NlCore::ShouldFillNeed(current, limit) && NlCore::NeedValue(NlCore::k_FillAll, limit, wanted) && NlAccess::SetNumber(values, step, wanted, why))
							g_LogNeeds++;
					}
				}
			}

			bool age_off = false;
			if (plan.WriteAge)
			{
				// __soul.__aging.__old.__debug_is_can_die_of_old_age: 인물마다 true 다. 켜면 false 로, 끄면 다시 true 로 쓴다.
				RValue old;
				const PathStep flag{ '.', "__debug_is_can_die_of_old_age", 0 };
				double current = 0;
				if (NlAccess::Follow(soul, { { '.', "__aging", 0 }, { '.', "__old", 0 } }, old, why) && old.IsStruct() && FollowNumber(old, { flag }, current))
				{
					if (current != plan.AgeValue && NlAccess::SetNumber(old, flag, plan.AgeValue, why))
					{
						g_LogAge++;
						current = plan.AgeValue;
					}
					age_off = current == 0;		// 꺼진 깃발이 있다(되돌릴 것이 있다)
				}
			}
			NlCore::HoldTouched(g_Hold, age_off, false);

			if (g_HappyRound && happy)		// 도중에 끄면 남은 사람에게는 붙이지 않는다
			{
				PersonCommand command;
				command.Act = PersonAct::Happy;
				command.Who = row.Uuid;
				std::string note;
				if (One(command, row, note, true) && note.empty())
					g_LogHappy++;
			}
		}
		NlCore::HoldEnd(g_Hold, slice, ageless);

		if (slice.Wrapped)
		{
			HoldNotes(std::to_string(g_RoundPeople) + "명에게 적용 중");
			// 로그는 쓴 것이 있을 때, 60초에 한 줄까지만 남긴다(욕구는 틱마다 조금씩 줄어 바퀴마다 쓸 것이 생긴다).
			if ((g_LogNeeds || g_LogHappy || g_LogAge) && Now >= g_NextHoldLog)
			{
				g_NextHoldLog = Now + 60;
				Log("people hold: " + std::to_string(g_RoundPeople) + " people; since the last line " + std::to_string(g_LogNeeds) + " need(s) filled, "
					+ std::to_string(g_LogHappy) + " happy mind(s), " + std::to_string(g_LogAge) + " old-age flag(s) written");
				g_LogNeeds = g_LogHappy = g_LogAge = 0;
			}
		}
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	void Push(PersonAct Act, const std::string& Who, int Index = -1, double Amount = 0, const std::string& Text = std::string())
	{
		PersonCommand command;
		command.Act = Act;
		command.Who = Who;
		command.Index = Index;
		command.Amount = Amount;
		command.Text = Text;
		g_Queue.push_back(std::move(command));
	}

	std::string KindText(const PersonRow& Row)
	{
		if (!NlCore::IsPlayers(Row))
			return Row.Dead ? "죽음" : "손님·다른 진영";
		return Row.Character ? "영주" : "주민";
	}

	std::string NumberText(double Value, int Digits)
	{
		return Value == k_Unknown ? "-" : NlCore::Fixed(Value, Digits);
	}

	bool NotReady()
	{
		if (g_Now.Ready)
			return false;
		ImGui::TextDisabled("%s", g_Now.Why.empty() ? "게임을 시작하면 사람들이 보입니다." : g_Now.Why.c_str());
		return true;
	}

	// 흐린 글. 창의 너비에서 줄을 바꾼다(긴 안내 글이 창 밖으로 잘리지 않게).
	void Hint(const std::string& Text)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", Text.c_str());
		ImGui::PopTextWrapPos();
	}

	void DrawLast()
	{
		if (!g_Now.Last.empty())
			Hint(g_Now.Last);
	}

	// "마우스 자리에 소환" 단추들. 누르면 틱이 디버그 소환기를 부른다(그리는 쪽은 청만 쌓는다).
	void DrawSpawnHere(std::initializer_list<NlCore::SpawnKind> Kinds)
	{
		ImGui::TextUnformatted("마우스 자리에 소환");
		for (const NlCore::SpawnKind kind : Kinds)
		{
			ImGui::SameLine();
			if (ImGui::Button((std::string(NlCore::SpawnLabel(kind)) + " +1").c_str()) && g_SpawnKinds.size() < 20)
				g_SpawnKinds.push_back(kind);
		}
	}

	void DrawDetail(const PersonRow& Row)
	{
		const Detail& one = g_Now.One;
		ImGui::Text("%s  (%s)", Row.Name.c_str(), KindText(Row).c_str());
		if (!one.Ready || one.Uuid != Row.Uuid)
		{
			ImGui::TextDisabled("값을 읽는 중입니다.");
			return;
		}
		const std::string who = Row.Uuid;

		// 나이, 기분, 통증
		if (g_AgeInputFor != who && one.Age != k_Unknown)
		{
			g_AgeInput = static_cast<int>(one.Age);
			g_AgeInputFor = who;
		}
		ImGui::Text("나이 %s", NumberText(one.Age, 0).c_str());
		ImGui::SameLine();
		ImGui::BeginDisabled(one.Age == k_Unknown || g_AgeInputFor != who);		// 나이를 읽지 못한 사람에게 앞 사람의 수로 부르지 않는다
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("##age", &g_AgeInput);
		ImGui::SameLine();
		if (ImGui::Button("이 나이로"))
			Push(PersonAct::AgeSet, who, -1, g_AgeInput);
		ImGui::EndDisabled();
		ImGui::Text("기분 %s   생각의 합 %s   통증 %s", NumberText(one.Moral, 0).c_str(), NumberText(one.MindSum, 0).c_str(), NumberText(one.Pain, 1).c_str());
		if (ImGui::Button("행복하게"))
			Push(PersonAct::Happy, who);
		ImGui::SameLine();
		if (ImGui::Button("치료"))
			Push(PersonAct::Cure, who);
		ImGui::SameLine();
		if (ImGui::Button("욕구 모두 채우기"))
			Push(PersonAct::NeedsFill, who);
		ImGui::SameLine();
		if (ImGui::Button("능력치 모두 20"))
			Push(PersonAct::SkillsMax, who);
		Hint("기분은 게임이 생각의 합으로 다시 셈합니다. '행복하게'는 게임의 디버그용 생각(+100, 하루)을 붙입니다. 능력치는 0~20, 나이는 1~120 입니다.");

		ImGui::SeparatorText("능력치");
		if (ImGui::BeginTable("skills", 3, ImGuiTableFlags_SizingFixedFit))
		{
			const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
			for (size_t i = 0; i < skills.size() && i < one.Skills.size(); i++)
			{
				if (one.Skills[i] == k_Unknown)
					continue;		// 그 사람에게 없는 능력치(주민은 전투만 있다)
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::Text("%s (%s)", skills[i].Label, skills[i].Key);
				ImGui::TableNextColumn();
				ImGui::Text("%s  +%s", NumberText(one.Skills[i], 0).c_str(), NlCore::Fixed(one.Points[i], 2).c_str());
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::SmallButton("-1"))
					Push(PersonAct::SkillAdd, who, static_cast<int>(i), -1);
				ImGui::SameLine();
				if (ImGui::SmallButton("+1"))
					Push(PersonAct::SkillAdd, who, static_cast<int>(i), 1);
				ImGui::SameLine();
				if (ImGui::SmallButton("+5"))
					Push(PersonAct::SkillAdd, who, static_cast<int>(i), 5);
				ImGui::SameLine();
				if (ImGui::SmallButton("20"))
					Push(PersonAct::SkillSet, who, static_cast<int>(i), NlCore::k_SkillMax);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("욕구");
		if (ImGui::BeginTable("needs", 3, ImGuiTableFlags_SizingFixedFit))
		{
			const std::vector<const char*>& needs = NlCore::NeedNames();
			for (size_t i = 0; i < needs.size() && i < one.Needs.size(); i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(needs[i]);
				ImGui::TableNextColumn();
				const double limit = i < one.Limits.size() ? one.Limits[i] : NlCore::k_NeedMax;
				ImGui::Text("%s / %s", NumberText(one.Needs[i], 0).c_str(), NumberText(limit, 0).c_str());
				ImGui::TableNextColumn();
				ImGui::PushID(100 + static_cast<int>(i));
				if (ImGui::SmallButton("채우기"))
					Push(PersonAct::NeedSet, who, static_cast<int>(i), NlCore::k_FillAll);		// 상한까지(읽은 상한을 보내지 않는다)
				ImGui::SameLine();
				if (ImGui::SmallButton("0 으로"))
					Push(PersonAct::NeedSet, who, static_cast<int>(i), 0);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("특성");
		for (size_t i = 0; i < one.Traits.size(); i++)
		{
			ImGui::PushID(200 + static_cast<int>(i));
			ImGui::BeginDisabled(NlCore::IsProtectedTrait(one.Traits[i]));		// 종과 죽음의 특성은 떼지 않는다
			if (ImGui::SmallButton("떼기"))
				Push(PersonAct::TraitRemove, who, -1, 0, one.Traits[i]);
			ImGui::EndDisabled();
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::TextUnformatted(one.Traits[i].c_str());
		}
		ImGui::SetNextItemWidth(160);
		ImGui::InputText("이름의 일부로 찾아 붙이기", g_TraitFilter, sizeof(g_TraitFilter));
		if (g_TraitFilter[0])
		{
			int shown = 0;
			for (const std::string& name : g_Now.TraitNames)
			{
				if (name.find(g_TraitFilter) == std::string::npos || Has(one.Traits, name) || NlCore::IsProtectedTrait(name))
					continue;
				if (shown++ >= 12)
				{
					ImGui::TextDisabled("더 있습니다. 이름을 더 적어 주세요.");
					break;
				}
				ImGui::PushID(name.c_str());
				if (ImGui::SmallButton("붙이기"))
					Push(PersonAct::TraitAdd, who, -1, 0, name);
				ImGui::PopID();
				ImGui::SameLine();
				ImGui::TextUnformatted(name.c_str());
			}
			if (!shown)
				ImGui::TextDisabled("그런 이름의 특성이 없습니다(게임의 영문 이름입니다. 예: brave, gifted).");
		}
		Hint("특성은 게임의 이름 그대로입니다. 종과 죽음의 특성(human, dead 같은 것)은 붙이거나 뗄 수 없습니다.");
	}

	size_t CountPlayers(bool Characters)
	{
		size_t count = 0;
		for (const PersonRow& row : g_Now.People)
			count += NlCore::IsPlayers(row) && row.Character == Characters;
		return count;
	}
}

void NlPeople::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlPeople::GameTick(double Now, bool Active)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)		// 여기서 부른 게임의 함수가 오브젝트 이벤트를 일으켜 다시 들어왔다
		return;
	const Busy busy;

	HoldTick(Now);
	BattleTick(Now);

	if (!g_SpawnKinds.empty())
	{
		const NlCore::SpawnKind kind = g_SpawnKinds.front();
		g_SpawnKinds.pop_front();		// 틱마다 하나씩(잰 꼴과 같다)
		g_Now.Last = SpawnHereNow(kind);
		g_NextScan = 0;
	}

	if (g_SpawnQueued > 0)
	{
		const int count = g_SpawnQueued;
		g_SpawnQueued = 0;
		g_Now.Last = SpawnSoldiersNow(count);
		g_NextScan = 0;
	}

	if (!g_Queue.empty())
	{
		while (!g_Queue.empty())
		{
			const PersonCommand command = g_Queue.front();
			g_Queue.pop_front();
			g_Now.Last = Execute(command);
		}
		RefreshDetail();
		g_NextScan = Now + 2;
		g_NextDetail = Now + 0.5;
		return;
	}
	if (!Active)
		return;
	if (Now >= g_NextScan)
	{
		g_NextScan = Now + 2;
		Scan();
		g_NextDetail = 0;
	}
	if (g_Now.Ready && Now >= g_NextDetail)
	{
		g_NextDetail = Now + 0.5;
		RefreshDetail();
	}
}

namespace
{
	// 왼쪽의 사람 목록. 인물·지식·아이템 패널이 함께 쓴다(고른 사람도 함께 쓴다). 돌려주는 것: 고른 사람(없으면 nullptr).
	const PersonRow* DrawWho(bool LordsOnly)
	{
		ImGui::BeginChild("who", ImVec2(210, 0), ImGuiChildFlags_Borders);
		if (!LordsOnly)
			ImGui::Checkbox("주민·손님도 보기", &g_ShowAll);
		for (const PersonRow& row : g_Now.People)
		{
			if ((LordsOnly || !g_ShowAll) && !(NlCore::IsPlayers(row) && row.Character))
				continue;
			const std::string label = row.Name + "  " + KindText(row) + "##" + row.Uuid;
			ImGui::BeginDisabled(row.Dead);
			if (ImGui::Selectable(label.c_str(), g_Selected == row.Uuid))
				g_Selected = row.Uuid;
			ImGui::EndDisabled();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		return g_Selected.empty() ? nullptr : FindRow(g_Selected);
	}

	// 고른 사람의 값을 아직 읽지 못했으면 참(그 글을 그린다).
	bool DetailPending(const PersonRow& Row)
	{
		ImGui::Text("%s  (%s)", Row.Name.c_str(), KindText(Row).c_str());
		if (g_Now.One.Ready && g_Now.One.Uuid == Row.Uuid)
			return false;
		ImGui::TextDisabled("값을 읽는 중입니다.");
		return true;
	}
}

void NlPeople::DrawPerson()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(false);
	ImGui::BeginChild("one", ImVec2(0, 0));
	if (!row)
		ImGui::TextDisabled("왼쪽에서 사람을 고르세요.");
	else
		DrawDetail(*row);
	DrawLast();
	ImGui::EndChild();
}

void NlPeople::DrawKnowledge()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(true);		// 지식은 영주가 가진다
	if (row && !(NlCore::IsPlayers(*row) && row->Character))
		row = nullptr;							// 다른 패널에서 주민이나 손님을 골라 둔 채 왔다
	ImGui::BeginChild("one", ImVec2(0, 0));
	Hint("지식은 영주가 가집니다. 준 지식은 되돌릴 수 없고 세이브에 남습니다. 교과서 지식은 능력치도 올리고 재능과 별명이 붙을 수 있습니다. "
		"모든 지식을 가진 영주가 있으면 지식으로 잠겨 있던 건물(창고, 사원, 무기고에서 봤습니다)을 지을 수 있습니다.");
	if (!row)
		ImGui::TextDisabled("왼쪽에서 영주를 고르세요.");
	else if (!DetailPending(*row))
	{
		const Detail& one = g_Now.One;
		const std::string who = row->Uuid;
		ImGui::Text("가진 지식 %s / %d", NumberText(one.KnowledgeCount, 0).c_str(), static_cast<int>(g_Now.Knowledge.size()));
		ImGui::SameLine();
		if (ImGui::Button("이 영주에게 모든 지식 주기"))
			Push(PersonAct::KnowledgeAll, who);

		ImGui::SetNextItemWidth(160);
		ImGui::InputText("이름의 일부로 찾아 하나 주기", g_KnowledgeFilter, sizeof(g_KnowledgeFilter));
		if (g_KnowledgeFilter[0])
		{
			int shown = 0;
			for (const KnowledgeName& knowledge : g_Now.Knowledge)
			{
				if (knowledge.Name.empty() || (knowledge.Caption.find(g_KnowledgeFilter) == std::string::npos && knowledge.Name.find(g_KnowledgeFilter) == std::string::npos))
					continue;
				if (shown++ >= 14)
				{
					ImGui::TextDisabled("더 있습니다. 이름을 더 적어 주세요.");
					break;
				}
				ImGui::PushID(knowledge.Name.c_str());
				if (ImGui::SmallButton("주기"))
					Push(PersonAct::KnowledgeAdd, who, -1, 0, knowledge.Name);
				ImGui::PopID();
				ImGui::SameLine();
				ImGui::Text("%s  (%s, %s)", knowledge.Caption.c_str(), knowledge.Name.c_str(), knowledge.Category.c_str());
			}
			if (!shown)
				ImGui::TextDisabled("그런 이름의 지식이 없습니다(화면의 이름이나 게임의 영문 이름. 예: 광산, mine).");
		}
	}

	// 영주 전원에게 주는 것은 맨 아래에 따로, 두 단계로 둔다(한 번의 잘못된 클릭으로 모든 영주가 바뀌지 않게).
	ImGui::SeparatorText("영주 전원");
	ImGui::Checkbox("되돌릴 수 없다는 것을 압니다", &g_BulkKnowledgeArmed);
	ImGui::SameLine();
	ImGui::BeginDisabled(!g_BulkKnowledgeArmed);
	if (ImGui::Button("영주 전원에게 모든 지식 주기"))
	{
		Push(PersonAct::KnowledgeAll, "lords");
		g_BulkKnowledgeArmed = false;
	}
	ImGui::EndDisabled();
	DrawLast();
	ImGui::EndChild();
}

void NlPeople::DrawArmy()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;
	int soldiers = 0;
	for (const PersonRow& row : g_Now.People)
		soldiers += NlCore::IsPlayers(row) && !row.Character && !row.Dead && row.Strata == 2;		// 병사의 갈래는 2 다(research/13)
	ImGui::Text("플레이어의 병사 %d명", soldiers);
	for (const int amount : { 1, 5, 10 })
	{
		ImGui::SameLine();
		ImGui::PushID(amount);
		if (ImGui::Button(("+" + std::to_string(amount)).c_str()))
			g_SpawnQueued = (std::min)(g_SpawnQueued + amount, NlCore::k_SoldierBatchMax);		// windows.h 의 min 매크로를 피한다
		ImGui::PopID();
	}
	DrawSpawnHere({ NlCore::SpawnKind::Soldier, NlCore::SpawnKind::Knight });
	Hint("+1, +5, +10 은 게임의 디버그 함수로 병사를 만듭니다: 지도 가장자리에 나타나 마을로 걸어오고 게임의 군대 창에 전사로 올라옵니다(단검, 갑옷 없음). "
		"'마우스 자리에 소환'은 게임의 디버그 소환기를 부릅니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다(병사는 경갑과 창). "
		"병영의 정원과 임금은 따지지 않습니다(재지 않았습니다). 되돌릴 수 없고, 저장하면 세이브에 남을 것으로 보입니다.");
	DrawLast();
}

std::vector<std::string> NlPeople::SpawnHere(NlCore::SpawnKind Kind)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	g_Now.Last = SpawnHereNow(Kind);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::SpawnSoldiers(double Count)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	g_Now.Last = SpawnSoldiersNow(Count);
	return { g_Now.Last };
}

void NlPeople::DrawItems()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(false);
	ImGui::BeginChild("one", ImVec2(0, 0));
	if (!row)
		ImGui::TextDisabled("왼쪽에서 사람을 고르세요.");
	else if (!DetailPending(*row))
	{
		const Detail& one = g_Now.One;
		const std::string who = row->Uuid;
		ImGui::Text("소지금 %s", one.Money == k_Unknown ? "-" : NlCore::Thousands(one.Money).c_str());
		for (const double amount : { 100.0, 1000.0, 10000.0, -100.0 })
		{
			ImGui::SameLine();
			ImGui::PushID(static_cast<int>(amount));
			if (ImGui::SmallButton((std::string(amount > 0 ? "+" : "") + NlCore::Thousands(amount)).c_str()))
				Push(PersonAct::MoneyAdd, who, -1, amount);
			ImGui::PopID();
		}
		Hint("소지품과 소지금은 게임의 함수로 바꿉니다(게임의 인물 창에 보입니다). 세이브에 남습니다. 영지 창고의 자원은 '경제'에 있습니다. "
			"갑옷과 무기는 수만 바뀌고 착용은 바뀌지 않습니다. 착용 중인 것은 뺄 수 없습니다.");

		ImGui::SeparatorText("소지품");
		if (ImGui::BeginTable("items", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			for (size_t i = 1; i < g_Now.Resources.size() && i < one.Items.size(); i++)		// 0 번(룬)은 건드리지 않는다(경제 패널과 같다)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(g_Now.Resources[i].c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(NumberText(one.Items[i], 0).c_str());
				ImGui::TableNextColumn();
				ImGui::PushID(300 + static_cast<int>(i));
				for (const double amount : { 1.0, 10.0, 100.0 })
				{
					if (ImGui::SmallButton(("+" + NlCore::Fixed(amount, 0)).c_str()))
						Push(PersonAct::ItemAdd, who, static_cast<int>(i), amount);
					ImGui::SameLine();
				}
				ImGui::BeginDisabled(!(one.Items[i] > 0) || NlCore::IsEquipped(static_cast<int>(i), one.Equipped));		// 착용 중인 장비는 빼지 않는다
				if (ImGui::SmallButton("0 으로"))
					Push(PersonAct::ItemAdd, who, static_cast<int>(i), -NlCore::k_GiftMax);
				ImGui::EndDisabled();
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
	DrawLast();
	ImGui::EndChild();
}

void NlPeople::DrawLords()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;
	ImGui::Text("플레이어의 영주 %d명에게 한꺼번에", static_cast<int>(CountPlayers(true)));
	if (ImGui::Button("능력치 모두 20"))
		Push(PersonAct::SkillsMax, "lords");
	ImGui::SameLine();
	if (ImGui::Button("욕구 모두 채우기"))
		Push(PersonAct::NeedsFill, "lords");
	ImGui::SameLine();
	if (ImGui::Button("행복하게"))
		Push(PersonAct::Happy, "lords");
	ImGui::SameLine();
	if (ImGui::Button("치료"))
		Push(PersonAct::Cure, "lords");
	Hint("손님과 다른 진영의 영주에게는 가지 않습니다. 한 사람씩 고치려면 '인물'에서 고릅니다.");
	DrawSpawnHere({ NlCore::SpawnKind::Lord });
	Hint("게임의 디버그 소환기로 플레이어의 영주 하나를 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 되돌릴 수 없습니다.");
	DrawLast();
}

void NlPeople::DrawPeople()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;
	ImGui::Text("플레이어의 사람 모두(영주 %d, 주민 %d)에게 지금 한 번", static_cast<int>(CountPlayers(true)), static_cast<int>(CountPlayers(false)));
	if (ImGui::Button("욕구 모두 채우기"))
		Push(PersonAct::NeedsFill, "people");
	ImGui::SameLine();
	if (ImGui::Button("행복하게"))
		Push(PersonAct::Happy, "people");
	ImGui::SameLine();
	if (ImGui::Button("치료"))
		Push(PersonAct::Cure, "people");
	Hint("계속 유지하려면 위의 항목을 켭니다. 인구는 '날마다 추가 이주민'에 수를 넣으면 다음 이주 때(저녁) 그만큼 더 옵니다.");
	DrawSpawnHere({ NlCore::SpawnKind::Peasant, NlCore::SpawnKind::Slave });
	Hint("게임의 디버그 소환기로 플레이어의 주민이나 노예 하나를 바로 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 집과 일자리는 따지지 않습니다.");
	DrawLast();
}

std::vector<std::string> NlPeople::Do(const NlCore::PersonCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	g_Now.Last = Execute(Command);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::List(bool All)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_Now.Why };
	std::vector<std::string> lines;
	for (const PersonRow& row : g_Now.People)
		if (All || (NlCore::IsPlayers(row) && row.Character))
			lines.push_back(row.Uuid + "  " + row.Name + "  " + (row.Character ? "character" : "dummy") + ":" + std::to_string(row.Index) + "  faction " + row.Faction
				+ "  strata " + Shortest(row.Strata) + (row.Dead ? "  dead" : ""));
	lines.push_back("(" + std::to_string(lines.size()) + " of " + std::to_string(g_Now.People.size()) + " people)");
	return lines;
}

std::vector<std::string> NlPeople::Show(const std::string& Uuid)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const Busy busy;
	if (!Scan())
		return { g_Now.Why };
	const PersonRow* row = FindRow(Uuid);
	Detail one;
	if (!row || !ReadDetail(*row, one))
		return { "no such person" };
	g_Selected = row->Uuid;		// 인물 패널도 그 사람을 고른다(page person 뒤 shot 으로 창을 볼 수 있게)
	g_Now.One = one;

	std::vector<std::string> lines;
	lines.push_back(row->Name + "  " + (row->Character ? "character" : "dummy") + ":" + std::to_string(row->Index) + "  faction " + row->Faction);
	lines.push_back("knowledge " + NumberText(one.KnowledgeCount, 0) + "  money " + NumberText(one.Money, 0));
	lines.push_back("age " + NumberText(one.Age, 0) + "  moral " + NumberText(one.Moral, 2) + "  minds " + NumberText(one.MindSum, 2) + "  pain " + NumberText(one.Pain, 2));
	std::string skills = "skills", needs = "needs", traits = "traits";
	for (size_t i = 0; i < one.Skills.size(); i++)
		skills += std::string(" ") + NlCore::SkillNames()[i].Key + "=" + NumberText(one.Skills[i], 0);
	for (size_t i = 0; i < one.Needs.size(); i++)
		needs += " " + std::to_string(i) + "=" + NumberText(one.Needs[i], 1);
	for (const std::string& trait : one.Traits)
		traits += " " + trait;
	lines.push_back(skills);
	lines.push_back(needs);
	lines.push_back(traits);
	return lines;
}
