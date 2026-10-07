#include "People.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Hold.hpp"
#include "PeopleAccess.hpp"
#include "Shield.hpp"
#include "TraitText.hpp"
#include "Game.hpp"
#include "Jobs.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Localization.hpp"
#include "core/EconomyPlan.hpp"
#include "core/FamilyPlan.hpp"
#include "core/RolePlan.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::PersonAct;
using NlCore::PersonCommand;
using NlCore::PersonRow;
using NlCore::Shortest;
using namespace NlPeopleAccess;

namespace
{
	// 자리와 함수는 research/11 에서 잰 것이다(0.5588.9777.0. 불러온 세이브).
	// 게임의 디버그용 생각: 기분 +100, 하루. Minds.attach_generic_mind(생각 구조체)로 붙인다(게임이 그 꼴로 부르는 것을 기록했다).
	constexpr const char* k_HappyMind = "inst:o_data.mind_debug_totally_happy";
	// 지식의 종류 121개(research/12). 한 칸은 구조체: __name, __caption_replaced(화면의 이름), __category.
	constexpr const char* k_KnowledgeList = "inst:o_data.__knowledge_data.__knowledge_list";
	constexpr const char* k_ResourceCaptions = "global.__resource_caption";	// 자원 번호 → "resource.wood"(research/07)
	// 플레이어의 병사 하나를 만드는 게임의 디버그 함수(research/13). 인자를 하나까지 받고 생략할 수 있다(기계어). 인자 없이 불러 병사가 생기고
	// 병영의 목록과 게임의 군대 창에 올라오는 것을 봤다. 지도 가장자리의 자리에 나타나 마을로 걸어온다.
	constexpr const char* k_SpawnSoldier = "gml_Script_rebellion_debug_spawn_player_soldier";
	// 선호 장비의 묶음들(research/17): __h_swordman, __any … 한 묶음은 __armor_resource[0], __weapon_resource[0], __is_need_shield 를 가진 구조체다.
	constexpr const char* k_PreferredData = "inst:o_data.__preferred_equipment_data";
	// 게임의 디버그 소환기(CreatureSpawner. research/13). __spawn_soldier 같은 메서드는 인자가 없고(기계어) 마우스가 가리키는 지도의 자리에 만든다.
	constexpr const char* k_Spawner = "inst:o_debug.debug_spawner";
	struct Detail			// 고른 사람의 값. RValue 를 담지 않는다
	{
		bool Ready = false;
		std::string Uuid;
		double Age = k_Unknown, Moral = k_Unknown, Pain = k_Unknown, MindSum = k_Unknown;
		double Gender = k_Unknown;				// SoulBasic.get_gender() -> 1(여성) | 0(남성). research/24
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
	bool g_TraitListOpen = false;		// 창의 선택: 전체 특성 목록을 펼쳐 둔다
	bool g_TraitScroll = false;			// 다음에 그릴 때 특성의 자리로 내려간다(원격 traits 가 켠다)
	int g_RolePick = 0;					// 창의 선택: 역할 프리셋의 자리(core/RolePlan 의 차례)
	std::unordered_map<std::string, NlCore::RoleMemory> g_RoleMemory;	// 이 실행에서 역할 프리셋을 입힌 사람(uuid)마다의 "전"(core/RolePlan). 되돌리기가 쓴다. 파일에 남기지 않는다
	std::string g_RoleLast, g_RoleLastFor;		// 역할 프리셋의 마지막 결과와 그 사람의 uuid(단추 아래에 보인다. 맨 아래의 글까지 내려가지 않아도 되게)
	std::string g_FatherPick;					// 창의 선택: 임신을 시작할 때의 아버지(uuid)
	bool g_BulkBirthArmed = false;				// "임신한 영주 모두 출산"은 이것을 켠 뒤에만 눌린다(되돌릴 수 없다)
	char g_KnowledgeFilter[48] = "";
	int g_SpawnQueued = 0;				// 창이 청한 병사의 수(틱이 만든다)
	std::deque<NlCore::SpawnKind> g_SpawnKinds;		// 창이 청한 "마우스 자리에 소환"(틱마다 하나씩 한다)
	bool g_BulkKnowledgeArmed = false;	// "영주 전원에게 모든 지식"은 이것을 켠 뒤에만 눌린다(되돌릴 수 없다)

	bool g_AliveLogged = false;			// is_alive() 를 부른다고 로그에 남겼는가(게임마다 한 번)
	bool g_Busy = false;				// 틱이나 원격 명령을 하는 중이다. 여기서 부른 게임의 함수가 오브젝트 이벤트를 일으켜 다시 들어오면 안쪽은 아무것도 하지 않는다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 임신·출산의 게임 변수 셋(research/24. 열쇠는 core/FamilyPlan). 엔진은 src/Jobs.
	bool WalkPregnancyChance(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::PregnancyChanceVars(), Why); }
	bool WalkMiscarriage(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::MiscarriageVars(), Why); }
	bool WalkChildbirthDeath(const NlJobs::Visit& V, std::string& Why) { return NlJobs::WalkVars(V, NlCore::ChildbirthDeathVars(), Why); }

	// ---- 게임 스레드 ----

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
			NlTraitText::LoadForGame();
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
			Log("people call get_age(), get_pain(), get_total_modify(), get_knowledge_count(), get_gender() on " + Base(Row) + " " + Row.Uuid);		// 사람마다 한 번만 남긴다
		}
		CallNumber(base + ".get_age", Out.Age);
		CallNumber(base + ".get_gender", Out.Gender);		// 게임이 인자 없이 부른다: () -> 1 | 0 (334번. research/24)
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

	// 역할 프리셋의 걸음들의 셈. Why 는 첫 까닭.
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
	bool One(const PersonCommand& C, const PersonRow& Row, std::string& Note, bool Bulk = false)
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

	void RefreshDetail()
	{
		const PersonRow* row = g_Selected.empty() ? nullptr : FindRow(g_Selected);
		if (!row || !ReadDetail(*row, g_Now.One))
			g_Now.One = Detail();
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
	void DrawLast()
	{
		if (!g_Now.Last.empty())
			NlUi::Hint(g_Now.Last);
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

	// 특성의 설명을 풍선 글로(바로 앞에 그린 것 위에 마우스가 있을 때).
	void TraitTooltip(const std::string& Name)
	{
		const std::string& about = NlTraitText::Hint(Name);
		if (!about.empty() && ImGui::BeginItemTooltip())
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
			ImGui::TextUnformatted(about.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	// 역할 프리셋: 고르면 그 사람에게서 무엇이 바뀌는지 보이고, '이 역할로'를 누르면 틱이 입힌다.
	// 미리 보기는 읽어 둔 값(Detail)으로 셈한다(core 의 PlanRole. 러너를 부르지 않는다). 틱은 적용할 때 값을 다시 읽어 다시 셈한다.
	void DrawRole(const PersonRow& Row, const Detail& One)
	{
		const std::vector<NlCore::RolePreset>& roles = NlCore::RolePresets();
		if (g_RolePick < 0 || static_cast<size_t>(g_RolePick) >= roles.size())
			g_RolePick = 0;
		ImGui::SetNextItemWidth(200);
		if (ImGui::BeginCombo("##role", roles[g_RolePick].Label))
		{
			for (size_t i = 0; i < roles.size(); i++)
				if (ImGui::Selectable(roles[i].Label, static_cast<int>(i) == g_RolePick))
					g_RolePick = static_cast<int>(i);
			ImGui::EndCombo();
		}
		const NlCore::RolePreset& role = roles[g_RolePick];
		const NlCore::RoleTodo todo = NlCore::PlanRole(role, One.Skills, One.Traits);
		ImGui::SameLine();
		ImGui::BeginDisabled(todo.Empty());
		if (ImGui::Button("이 역할로"))
			Push(PersonAct::Role, Row.Uuid, -1, 0, role.Id);
		ImGui::EndDisabled();
		// 되돌리기: 이 실행에서 이 사람에게 입힌 것이 있을 때만(기억은 이 실행 안에서만 남는다).
		const bool remembered = g_RoleMemory.count(Row.Uuid) > 0 && !g_RoleMemory.at(Row.Uuid).Empty();
		ImGui::SameLine();
		ImGui::BeginDisabled(!remembered);
		if (ImGui::Button("되돌리기"))
			Push(PersonAct::RoleUndo, Row.Uuid, -1, 0, std::string());
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", remembered ? "이 실행에서 입힌 역할 프리셋을 되돌립니다: 능력치는 전 값으로, 붙인 특성은 떼고 뗀 특성은 다시 붙입니다"
				: "이 실행에서 이 사람에게 입힌 역할 프리셋이 없습니다 (기억은 이 실행 안에서만 남습니다)");
		// 요약은 제 줄에 둔다(단추 옆에 두자 기본 너비의 창에서 오른쪽이 잘렸다. 0.24.0 의 화면에서 봤다).
		NlUi::Hint(NlCore::RolePreview(todo));
		if (g_RoleLastFor == Row.Uuid && !g_RoleLast.empty())
			NlUi::Hint("마지막 결과 - " + g_RoleLast);

		// 능력치: 프리셋의 것을 차례대로. 지금 값이 더 높으면 그대로 둔다.
		// 한 능력치의 글("… 4 -> 20")이 줄 사이에서 갈리지 않게 조각마다 따로 그리고, 다음 조각이 들어갈 때만 옆에 둔다.
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		std::vector<std::string> items = { "능력치:" };
		for (const NlCore::RoleSkill& wanted : role.Skills)
			for (size_t i = 0; i < skills.size(); i++)
			{
				if (std::string(skills[i].Key) != wanted.Skill)
					continue;
				const double now = i < One.Skills.size() ? One.Skills[i] : k_Unknown;
				std::string item = std::string(NlTraitText::SkillLabel(i)) + " ";
				if (now == k_Unknown)
					item += "없음";		// 그 사람에게 없는 능력치(주민은 전투만 있다)
				else if (now >= wanted.Level)
					item += NumberText(now, 0) + " (그대로)";
				else
					item += NumberText(now, 0) + " -> " + std::to_string(wanted.Level);
				items.push_back(std::move(item));
			}
		const float gap = ImGui::GetStyle().ItemSpacing.x * 2;
		const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		for (size_t i = 0; i < items.size(); i++)
		{
			ImGui::TextUnformatted(items[i].c_str());
			if (i + 1 < items.size() && ImGui::GetItemRectMax().x + gap + ImGui::CalcTextSize(items[i + 1].c_str()).x < right)
				ImGui::SameLine(0.0f, gap);
		}

		// 특성: 한 줄에 하나(설명은 풍선 글로). 좁은 창에서도 읽히게 표의 칸에 두지 않는다.
		// 뗄 것을 먼저 보인다(먼저 떼기도 한다). 가진 것만 한 줄씩 그리고, 이 사람에게 없는 것은 흐린 한 줄로 모은다.
		ImGui::TextUnformatted("뗄 특성 (이 역할에 해로운 것. 가진 것만 뗍니다):");
		ImGui::Indent();
		std::string absent;
		bool owned = false;
		for (const char* name : role.Remove)
		{
			if (!NlCore::Has(One.Traits, name))
			{
				absent += (absent.empty() ? "" : ", ") + NlTraitText::Label(name);
				continue;
			}
			owned = true;
			ImGui::Text("뗌    %s", NlTraitText::Label(name).c_str());
			TraitTooltip(name);
			const char* note = NlCore::RemoveNote(name);
			if (note[0])
			{
				ImGui::Indent();
				NlUi::Hint(note);		// 제 줄에, 창의 너비에서 줄을 바꾼다
				ImGui::Unindent();
			}
		}
		if (!owned)
			ImGui::TextDisabled("뗄 것이 없습니다");
		if (!absent.empty())
			NlUi::Hint("이 사람에게 없는 것: " + absent);
		ImGui::Unindent();
		ImGui::TextUnformatted("붙일 특성:");
		ImGui::Indent();
		for (const char* name : role.Add)
		{
			if (NlCore::Has(One.Traits, name))
				ImGui::TextDisabled("있음    %s", NlTraitText::Label(name).c_str());
			else if (!NlTraitText::Known(name))
				ImGui::TextDisabled("게임에 없음    %s", name);
			else
				ImGui::Text("붙임    %s", NlTraitText::Label(name).c_str());
			TraitTooltip(name);
		}
		ImGui::Unindent();
		NlUi::Hint("능력치는 올리기만 합니다(이미 더 높은 것과 프리셋에 없는 것은 그대로 둡니다). 특성은 게임의 설명 글을 읽고 골랐고, 재능마다의 효과를 플레이에서 재지는 않았습니다. "
			"한 번에 되돌리는 단추는 없습니다: 붙인 특성은 아래 '특성'에서 하나씩 떼고, 능력치는 아래 '능력치'에서 내립니다.");
	}

	// 임신·성장(core/FamilyPlan, research/24). 그리는 쪽은 읽어 둔 값으로 판단하고 청만 쌓는다.
	void DrawFamily(const PersonRow& Row, const Detail& One)
	{
		if (!NlCore::IsPlayersLord(Row))
		{
			NlUi::Hint("임신·성장의 단추는 플레이어의 영주에게만 둡니다 (주민·손님·다른 진영에게는 게임의 함수를 불러 본 적이 없습니다).");
			return;
		}
		const int stage = NlCore::PregnancyStage(One.Traits);
		const bool kid = NlCore::IsKid(One.Traits);
		const bool forbid = NlCore::Has(One.Traits, NlCore::k_PregnantForbid);
		const char* gender = One.Gender == NlCore::k_Female ? "여성" : One.Gender == NlCore::k_Male ? "남성" : "성별을 읽지 못했습니다";
		if (stage > 0)
			ImGui::Text("%s, 임신 %d/3기", gender, stage);
		else
			ImGui::Text("%s%s%s", gender, kid ? ", 아이" : "", forbid ? ", 출산 뒤의 임신 금지가 붙어 있습니다" : "");
		ImGui::BeginDisabled(stage == 0);
		if (ImGui::Button("임신 다음 단계"))
			Push(PersonAct::PregnancyNext, Row.Uuid);
		ImGui::SameLine();
		if (ImGui::Button("바로 출산"))
			Push(PersonAct::Birth, Row.Uuid);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!kid);
		if (ImGui::Button("어른으로 (18세)"))
			Push(PersonAct::GrowUp, Row.Uuid);
		ImGui::EndDisabled();
		if (forbid)
		{
			ImGui::SameLine();
			if (ImGui::Button("임신 금지 떼기"))
				Push(PersonAct::TraitRemove, Row.Uuid, -1, 0, NlCore::k_PregnantForbid);
		}

		// 임신 시작: 아버지를 고른다(플레이어의 다른 영주. 성별과 나이는 틱이 누를 때 다시 본다).
		std::string why;
		if (NlCore::CanConceive(One.Gender, One.Traits, why))
		{
			const PersonRow* father = g_FatherPick.empty() ? nullptr : FindRow(g_FatherPick);
			if (father && (father->Uuid == Row.Uuid || father->Dead || !NlCore::IsPlayers(*father) || !father->Character))
				father = nullptr;
			ImGui::SetNextItemWidth(160);
			if (ImGui::BeginCombo("##father", father ? father->Name.c_str() : "아버지를 고르세요"))
			{
				for (const PersonRow& row : g_Now.People)
					if (NlCore::IsPlayers(row) && row.Character && !row.Dead && row.Uuid != Row.Uuid && ImGui::Selectable((row.Name + "##" + row.Uuid).c_str(), father == &row))
						g_FatherPick = row.Uuid;
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!father);
			if (ImGui::Button("임신 시키기") && father)
				Push(PersonAct::Conceive, Row.Uuid, -1, 0, father->Uuid);
			ImGui::EndDisabled();
		}
		NlUi::Hint("'임신 다음 단계'와 '바로 출산'은 게임의 디버그 함수를 부릅니다(1/3기, 2/3기, 3/3기, 출산). 게임의 확률을 그대로 타서 유산으로 끝날 수 있습니다"
			"('인구·욕구'의 '유산 없음'을 켜 두면 나지 않았습니다). '임신 시키기'는 아버지를 적고 임신 1/3기를 붙입니다. "
			"'어른으로'는 나이를 18 로 맞춥니다: 게임이 아이를 소영주로 만들어 영주 목록에서 빠집니다. 되돌리는 단추는 없습니다.");
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
		NlUi::Hint("기분은 게임이 생각의 합으로 다시 셈합니다. '행복하게'는 게임의 디버그용 생각(+100, 하루)을 붙입니다. 능력치는 0~20, 나이는 1~120 입니다.");

		// 짧은 것을 위에 둔다(역할 프리셋의 미리 보기가 길어 그 아래의 것은 스크롤해야 보였다).
		ImGui::SeparatorText("임신·성장");
		DrawFamily(Row, one);

		ImGui::SeparatorText("역할 프리셋");
		DrawRole(Row, one);

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
				ImGui::Text("%s (%s)", NlTraitText::SkillLabel(i), skills[i].Key);
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
		if (g_TraitScroll)
		{
			ImGui::SetScrollHereY(0.0f);
			g_TraitScroll = false;
		}
		for (size_t i = 0; i < one.Traits.size(); i++)
		{
			ImGui::PushID(200 + static_cast<int>(i));
			ImGui::BeginDisabled(NlCore::IsProtectedTrait(one.Traits[i]));		// 종과 죽음의 특성은 떼지 않는다
			if (ImGui::SmallButton("떼기"))
				Push(PersonAct::TraitRemove, who, -1, 0, one.Traits[i]);
			ImGui::EndDisabled();
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::TextUnformatted(NlTraitText::Label(one.Traits[i]).c_str());
			TraitTooltip(one.Traits[i]);
		}
		ImGui::SetNextItemWidth(180);
		ImGui::InputText("찾기 (명칭, 게임의 이름, 설명의 글)", g_TraitFilter, sizeof(g_TraitFilter));
		ImGui::SameLine();
		ImGui::Checkbox("전체 특성 목록", &g_TraitListOpen);
		if (g_TraitFilter[0] || g_TraitListOpen)
		{
			// 게임의 특성 전부(찾는 글이 있으면 맞는 것만). 표가 길어 스크롤되는 칸 안에 둔다.
			int shown = 0;
			if (ImGui::BeginChild("trait_list", ImVec2(0, 280), ImGuiChildFlags_Borders))
			{
				// 한 특성에 두 줄: 단추와 명칭(게임의 이름), 그 아래에 설명. 설명을 표의 칸에 두면 좁은 창에서 한 글자 너비가 된다(0.22.1 의 화면에서 봤다).
				for (const std::string& name : NlTraitText::Shown())
				{
					const std::string& caption = NlTraitText::Caption(name);
					const std::string& about = NlTraitText::Hint(name);
					// 이름, 명칭, 설명 어디에든 들어 있으면 맞는다.
					if (!NlCore::TraitMatches(g_TraitFilter, name, caption) && !NlCore::TraitMatches(g_TraitFilter, "", about))
						continue;
					shown++;
					if (NlCore::Has(one.Traits, name))
						ImGui::TextDisabled("있음");
					else
					{
						ImGui::PushID(name.c_str());
						ImGui::BeginDisabled(NlCore::IsProtectedTrait(name));		// 종과 죽음의 특성은 붙이지 않는다
						if (ImGui::SmallButton("붙이기"))
							Push(PersonAct::TraitAdd, who, -1, 0, name);
						ImGui::EndDisabled();
						ImGui::PopID();
					}
					ImGui::SameLine();
					if (NlTraitText::Titled(name))
					{
						// 이름의 줄이 없는 특성: 설명의 제목을 흐리게 보인다(게임이 화면에 쓰는 이름과 다를 수 있다).
						ImGui::TextDisabled("%s", caption.c_str());
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("설명의 제목입니다 (게임 파일에 이 특성의 이름 줄이 없습니다)");
					}
					else
						ImGui::TextUnformatted(caption.empty() ? "-" : caption.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled("(%s)", name.c_str());
					if (!about.empty())
					{
						ImGui::Indent();
						NlUi::Hint(about);		// 칸의 너비에서 줄을 바꾼다
						ImGui::Unindent();
					}
				}
				if (!shown)
					ImGui::TextDisabled("명칭, 게임의 이름, 설명 어디에도 그 글이 든 특성이 없습니다.");
			}
			ImGui::EndChild();
			ImGui::TextDisabled("%d개 (게임의 특성 %d개)", shown, static_cast<int>(NlTraitText::Names().size()));
		}
		NlUi::Hint(("명칭과 설명은 게임의 한국어 글입니다. 한국어가 비어 있는 것은 영어로 보입니다(" + NlTraitText::GetNotes().Names + ". " + NlTraitText::GetNotes().Hints + "). "
			"설명의 '(값)'은 게임의 글에 {…} 로 적혀 있는 자리입니다(게임이 화면에서 채워 넣는 자리로 보입니다). "
			"이름의 줄이 없는 특성은 설명의 제목을 흐린 글씨의 명칭으로 보이고(여러 특성이 한 설명을 함께 쓰면 같은 제목이 됩니다), 그것도 없으면 게임의 이름만 보입니다. "
			"종과 죽음의 특성(human, dead 같은 것)은 붙이거나 뗄 수 없습니다.").c_str());
	}

	size_t CountPlayers(bool Characters)
	{
		size_t count = 0;
		for (const PersonRow& row : g_Now.People)
			count += NlCore::IsPlayers(row) && row.Character == Characters;
		return count;
	}
}

void NlPeople::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	(void)GameDir;		// 특성의 글은 NlTraitText::Init 이 읽는다(리팩토링 C)
	std::string why;
	if (!NlCore::CheckRoles(why))
		Log("people: the role preset table is wrong: " + why);		// 시험이 막는다. 여기까지 오면 로그에 남긴다
	// 인구 바퀴(src/Hold)는 People 의 틱 안에서 돈다: 사람들을 읽는 길과 행복 생각을 붙이는 길을 넘긴다(People 의 잠금 아래에서 불린다).
	NlHold::Init(g_Log,
		[](std::vector<PersonRow>& Out) {
			if (!Scan())
				return false;
			Out = g_Now.People;
			return true;
		},
		[](const PersonRow& Row, std::string& Note) {
			PersonCommand command;
			command.Act = PersonAct::Happy;
			command.Who = Row.Uuid;
			return One(command, Row, Note, true);
		});
	NlJobs::Add({ "pregnancy_chance", "pregnancy chance", false, &WalkPregnancyChance, nullptr, 15 });
	NlJobs::Add({ "no_miscarriage", "miscarriage chance", true, &WalkMiscarriage, nullptr, 15 });
	NlJobs::Add({ "safe_childbirth", "childbirth death chance", true, &WalkChildbirthDeath, nullptr, 15 });
}

void NlPeople::GameTick(double Now, bool Active)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)		// 여기서 부른 게임의 함수가 오브젝트 이벤트를 일으켜 다시 들어왔다
		return;
	const NlCore::ScopedFlag busy(g_Busy);

	NlHold::Tick(Now);
	NlShield::Tick(Now);

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
			Run(command);
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
	NlUi::Hint("지식은 영주가 가집니다. 준 지식은 되돌릴 수 없고 세이브에 남습니다. 교과서 지식은 능력치도 올리고 재능과 별명이 붙을 수 있습니다. "
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
	ImGui::Separator();
	ImGui::TextUnformatted("병사 전원에게 장비 지급 (선호 장비를 정하고 없는 장비를 넣는다)");
	bool first = true;
	for (const NlCore::Loadout& loadout : NlCore::Loadouts())
	{
		if (!first)
			ImGui::SameLine();
		first = false;
		if (ImGui::Button(loadout.Label))
			Push(PersonAct::Equip, "people", -1, 0, loadout.Key);
	}
	NlUi::Hint("플레이어의 병사(고용한 병사와 기사도)마다 게임의 '선호 장비'를 그 묶음으로 바꾸고, 그 갑옷·무기·방패 가운데 없는 것을 소지품에 하나씩 넣습니다(넣으면 바로 착용됩니다). "
		"'아무 장비나'는 선호만 바꾸고 장비는 넣지 않습니다. 앞의 선호 장비는 남기지 않으므로 되돌릴 수 없습니다. "
		"게임은 선호 장비에 없는 장비를 무기고로 돌려보내는 것으로 보입니다. 바꾼 선호 장비가 여덟 시간 넘게 남는 것까지 봤습니다.");
	NlUi::Hint("+1, +5, +10 은 게임의 디버그 함수로 병사를 만듭니다: 지도 가장자리에 나타나 마을로 걸어오고 게임의 군대 창에 전사로 올라옵니다(단검, 갑옷 없음). "
		"'마우스 자리에 소환'은 게임의 디버그 소환기를 부릅니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다(병사는 경갑과 창). "
		"병영의 정원과 임금은 따지지 않습니다(재지 않았습니다). 되돌릴 수 없고, 저장하면 세이브에 남을 것으로 보입니다.");
	DrawLast();
}

std::vector<std::string> NlPeople::SpawnHere(NlCore::SpawnKind Kind)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	g_Now.Last = SpawnHereNow(Kind);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::SpawnSoldiers(double Count)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
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
		NlUi::Hint("소지품과 소지금은 게임의 함수로 바꿉니다(게임의 인물 창에 보입니다). 세이브에 남습니다. 영지 창고의 자원은 '경제'에 있습니다. "
			"갑옷과 무기는 수만 바뀌고 착용은 바뀌지 않습니다. 착용 중인 것은 뺄 수 없습니다.");

		ImGui::SeparatorText("소지품");
		if (ImGui::BeginTable("items", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			// 0 번은 신성 반지다: 영주의 소지품 0 번 칸의 수를 게임의 character_runes_get_count 가 그대로 돌려준다(research/18).
			for (size_t i = 0; i < g_Now.Resources.size() && i < one.Items.size(); i++)
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
	NlUi::Hint("손님과 다른 진영의 영주에게는 가지 않습니다. 한 사람씩 고치려면 '인물'에서 고릅니다.");
	ImGui::Checkbox("되돌릴 수 없다는 것을 압니다##birth", &g_BulkBirthArmed);
	ImGui::SameLine();
	ImGui::BeginDisabled(!g_BulkBirthArmed);
	if (ImGui::Button("임신한 영주 모두 출산"))
	{
		Push(PersonAct::Birth, "lords");
		g_BulkBirthArmed = false;
	}
	ImGui::EndDisabled();
	NlUi::Hint("임신한 영주마다 게임의 다음 단계 함수를 출산까지 부릅니다. 게임의 확률을 그대로 타서 유산으로 끝날 수 있습니다('인구·욕구'의 '유산 없음'을 켜 두면 나지 않았습니다).");
	DrawSpawnHere({ NlCore::SpawnKind::Lord });
	NlUi::Hint("게임의 디버그 소환기로 플레이어의 영주 하나를 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 되돌릴 수 없습니다.");
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
	NlUi::Hint("계속 유지하려면 위의 항목을 켭니다. 인구는 '날마다 추가 이주민'에 수를 넣으면 다음 이주 때(저녁) 그만큼 더 옵니다.");
	DrawSpawnHere({ NlCore::SpawnKind::Peasant, NlCore::SpawnKind::Slave });
	NlUi::Hint("게임의 디버그 소환기로 플레이어의 주민이나 노예 하나를 바로 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 집과 일자리는 따지지 않습니다.");
	DrawLast();
}

std::vector<std::string> NlPeople::Do(const NlCore::PersonCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	Run(Command);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::List(bool All)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
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
	const NlCore::ScopedFlag busy(g_Busy);
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
	lines.push_back("age " + NumberText(one.Age, 0) + "  moral " + NumberText(one.Moral, 2) + "  minds " + NumberText(one.MindSum, 2) + "  pain " + NumberText(one.Pain, 2)
		+ "  gender " + NumberText(one.Gender, 0) + "  pregnancy " + std::to_string(NlCore::PregnancyStage(one.Traits)));
	std::string skills = "skills", needs = "needs", traits = "traits";
	for (size_t i = 0; i < one.Skills.size(); i++)
		skills += std::string(" ") + NlCore::SkillNames()[i].Key + "=" + NumberText(one.Skills[i], 0);
	for (size_t i = 0; i < one.Needs.size(); i++)
		needs += " " + std::to_string(i) + "=" + NumberText(one.Needs[i], 1);
	for (const std::string& trait : one.Traits)
		traits += " " + trait + (NlTraitText::Caption(trait).empty() ? "" : "(" + NlTraitText::Caption(trait) + ")");
	lines.push_back(skills);
	lines.push_back(needs);
	lines.push_back(traits);
	return lines;
}

std::vector<std::string> NlPeople::Traits(const std::string& Find, size_t Max)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	if (!Scan())
		return { g_Now.Why };
	// 인물 패널의 찾기도 그 글로 맞춘다(칸보다 긴 글은 글자의 중간에서 자르지 않게 통째로 버린다).
	if (Find.size() < sizeof(g_TraitFilter))
	{
		std::fill(std::begin(g_TraitFilter), std::end(g_TraitFilter), '\0');
		std::copy(Find.begin(), Find.end(), g_TraitFilter);
		g_TraitListOpen = true;
		g_TraitScroll = true;
	}
	std::vector<std::string> lines;
	size_t matched = 0, captioned = 0;
	for (const std::string& name : NlTraitText::Shown())
	{
		const std::string& caption = NlTraitText::Caption(name);
		if (!caption.empty())
			captioned++;
		if (!NlCore::TraitMatches(Find, name, caption) && !NlCore::TraitMatches(Find, "", NlTraitText::Hint(name)))
			continue;
		if (matched++ < Max)
		{
			// 설명은 한 줄로(줄바꿈은 " / "), 앞의 120바이트쯤만(글자의 중간에서 자르지 않는다. 찾는 글이 그 뒤에 있어 맞은 줄은 그 글이 보이지 않을 수 있다).
			std::string about;
			for (const char c : NlTraitText::Hint(name))
				about += c == '\n' ? std::string(" / ") : std::string(1, c);
			size_t cut = about.size() > 120 ? 120 : about.size();
			while (cut < about.size() && (static_cast<unsigned char>(about[cut]) & 0xC0) == 0x80)
				cut++;
			lines.push_back(name + "  " + (caption.empty() ? "-" : caption) + (NlTraitText::Titled(name) ? " (title)" : "")
				+ (about.empty() ? "" : "  | " + about.substr(0, cut) + (cut < about.size() ? " ..." : "")));
		}
	}
	size_t described = 0, titled = 0;
	for (const std::string& name : NlTraitText::Names())
	{
		described += NlTraitText::Hint(name).empty() ? 0 : 1;
		titled += NlTraitText::Titled(name) ? 1 : 0;
	}
	lines.push_back("(" + std::to_string(lines.size()) + " of " + std::to_string(matched) + " matching; the game has " + std::to_string(NlTraitText::Names().size())
		+ " traits, " + std::to_string(captioned - titled) + " with a caption row, " + std::to_string(titled) + " named by a hint title, " + std::to_string(described)
		+ " with a description; " + NlTraitText::GetNotes().Names + "; " + NlTraitText::GetNotes().Hints + ")");
	return lines;
}

std::string NlPeople::TraitName(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return NlTraitText::Label(Name);
}

NlPeople::RowsResult NlPeople::Rows(std::vector<NlCore::PersonRow>& Out, std::string& Why)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
	{
		Why = "busy";
		return RowsResult::Busy;
	}
	const NlCore::ScopedFlag busy(g_Busy);
	if (!Scan())
	{
		Why = g_Now.Why;
		return RowsResult::Failed;
	}
	Out = g_Now.People;
	return RowsResult::Ok;
}
