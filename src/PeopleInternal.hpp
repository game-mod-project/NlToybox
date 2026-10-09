#pragma once
// src/People.cpp·PeopleActs.cpp·PeopleDraw.cpp 가 나눠 갖는 상태와 함수(2026-10-07 리팩토링 C). 이 헤더는 그 셋만 포함한다.
// 상태의 정의는 People.cpp 에 하나씩 있다. 모두 Internal::g_Mutex 아래에서 쓴다(그리기는 러너를 부르지 않는다).

#include "People.hpp"
#include "PeopleAccess.hpp"
#include "core/PeoplePlan.hpp"
#include "core/RolePlan.hpp"

#include <YYTK_Shared.hpp>

#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NlPeople::Internal
{
	using NlPeopleAccess::k_Unknown;
	using NlCore::PersonAct;
	using NlCore::PersonCommand;
	using NlCore::PersonRow;

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
	// 영주를 성별을 정해 만드는 길(research/35). 소환기의 __spawn_lord 는 카메라의 get_mouse_x()·get_mouse_y() 로 자리를, 지도의 그래프의 get_node_by_pos(x, y) 로 칸을 얻어
	// 영지의 debug_spawn_new_player_character(성별, 칸)을 부르는 것이 전부다(기계어와 기록). 같은 꼴로 부른다. 그래프는 지도의 get_graph() 가 돌려주는 구조체와 같다(주소).
	constexpr const char* k_Camera = "global.__current_camera";
	constexpr const char* k_MapGraph = "inst:o_game_map_controller.__current_local_map.__graph";
	constexpr const char* k_Province = "inst:o_game_map_controller.__province";
	// 문화의 자료(CulturesData): __cultures_list(게임이 무작위로 고르는 넷), __cultures_map(이름 → 문화 구조체). 문화 구조체: __name, __caption(화면 이름의 열쇠), __dialect …
	constexpr const char* k_Cultures = "inst:o_data.__cultures_data";
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

	struct CultureName
	{
		std::string Name, Label;		// 게임의 이름("gwelts"), 창에 보일 이름(게임의 글이 있으면 "화면 이름 (gwelts)")
	};

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다
	{
		bool Ready = false;
		std::string Why;
		std::vector<PersonRow> People;
		std::vector<KnowledgeName> Knowledge;	// 게임에 있는 지식(게임의 목록의 차례. 자리가 __knowledge_list 의 번호다)
		std::vector<CultureName> Cultures;		// 게임의 문화(__cultures_list 의 차례. 넷을 봤다)
		std::vector<std::string> Resources;		// 자원 번호 → 창에 보일 이름
		Detail One;
		std::string Last;						// 마지막으로 한 일
	};

	extern std::recursive_mutex g_Mutex;
	extern NlPeople::LogFn g_Log;
	extern Snapshot g_Now;
	extern std::deque<NlCore::PersonCommand> g_Queue;
	extern std::string g_Selected;
	extern std::map<std::string, std::string> g_Names;
	extern double g_NextScan, g_NextDetail;
	extern std::string g_DetailLogged;
	extern bool g_ShowAll;
	extern int g_AgeInput;
	extern std::string g_AgeInputFor;
	extern char g_TraitFilter[48];
	extern bool g_TraitListOpen, g_TraitScroll;
	extern int g_RolePick;
	extern std::unordered_map<std::string, NlCore::RoleMemory> g_RoleMemory;
	extern std::string g_RoleLast, g_RoleLastFor;
	extern std::string g_FatherPick;
	extern bool g_BulkBirthArmed;
	extern char g_KnowledgeFilter[48];
	extern int g_SpawnQueued;
	extern std::deque<NlCore::SpawnKind> g_SpawnKinds;
	extern NlCore::LordSpawn g_LordSpawn;
	extern std::deque<NlCore::LordSpawn> g_LordSpawns;
	extern bool g_BulkKnowledgeArmed;
	extern bool g_AliveLogged, g_Busy;

	// People.cpp
	void Log(const std::string& Line);
	bool Scan();
	const NlCore::PersonRow* FindRow(const std::string& Uuid);
	bool ReadDetail(const NlCore::PersonRow& Row, Detail& Out);
	void RefreshDetail();
	int KnowledgeIndex(const std::string& Name);
	// PeopleActs.cpp
	bool One(const NlCore::PersonCommand& C, const NlCore::PersonRow& Row, std::string& Note, bool Bulk = false);
	std::string Execute(const NlCore::PersonCommand& C);
	void Run(const NlCore::PersonCommand& C);
	std::string WhoText(const std::string& Who);
	std::string SpawnSoldiersNow(double Asked);
	std::string SpawnHereNow(NlCore::SpawnKind Kind);
	std::vector<std::string> SpawnLordNow(const NlCore::LordSpawn& Options);
	// PeopleDraw.cpp
	std::string NumberText(double Value, int Digits);
}
