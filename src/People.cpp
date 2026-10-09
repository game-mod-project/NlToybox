#include "PeopleInternal.hpp"

#include "Access.hpp"
#include "Hold.hpp"
#include "PeopleAccess.hpp"
#include "Shield.hpp"
#include "TraitText.hpp"
#include "Game.hpp"
#include "Jobs.hpp"
#include "core/AskPath.hpp"
#include "core/EconomyPlan.hpp"
#include "core/Guard.hpp"
#include "core/Localization.hpp"
#include "core/FamilyPlan.hpp"
#include "core/RolePlan.hpp"
#include "core/Text.hpp"

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
using namespace NlPeople::Internal;

// 상태의 정의, 사람 읽기(Scan·ReadDetail), 틱과 진입점. 명령은 src/PeopleActs, 그리기는 src/PeopleDraw(2026-10-07 리팩토링 C).
namespace NlPeople::Internal
{
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
	NlCore::LordSpawn g_LordSpawn;					// 창의 선택: 소환할 영주의 성별·나이·문화·역할(이 실행 안에서만 든다. 파일에 남기지 않는다)
	std::deque<NlCore::LordSpawn> g_LordSpawns;		// 창이 청한, 무언가를 정한 영주 소환(틱마다 하나씩 한다)
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
		// 문화: 게임이 무작위로 고르는 목록(__cultures_list)의 이름들. 영주를 소환할 때 고른다(research/35).
		RValue cultures;
		g_Now.Cultures.clear();
		if (NlAccess::Read(NlCore::ParseAskPath(std::string(k_Cultures) + ".__cultures_list"), cultures, why) && cultures.IsArray())
			NlAccess::ForEachChild(cultures, Holder::Array, [&](const PathStep&, const RValue& item) {
				CultureName one;
				if (item.IsStruct() && FollowString(item, { { '.', "__name", 0 } }, one.Name) && NlCore::GoodTraitName(one.Name))
				{
					one.Label = NlTraitText::CultureLabel(one.Name);
					g_Now.Cultures.push_back(std::move(one));
				}
				return true;
			});
		Log("people: " + std::to_string(g_Now.Knowledge.size()) + " knowledge names, " + std::to_string(g_Now.Resources.size()) + " resources, "
			+ std::to_string(g_Now.Cultures.size()) + " cultures");
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
	void RefreshDetail()
	{
		const PersonRow* row = g_Selected.empty() ? nullptr : FindRow(g_Selected);
		if (!row || !ReadDetail(*row, g_Now.One))
			g_Now.One = Detail();
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

	if (!g_LordSpawns.empty())
	{
		const NlCore::LordSpawn spawn = g_LordSpawns.front();
		g_LordSpawns.pop_front();		// 틱마다 하나씩
		std::string said;
		for (const std::string& line : SpawnLordNow(spawn))
			said += std::string(said.empty() ? "" : " / ") + line;
		g_Now.Last = said;
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

std::vector<std::string> NlPeople::SpawnHere(NlCore::SpawnKind Kind)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	g_Now.Last = SpawnHereNow(Kind);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::SpawnLord(const NlCore::LordSpawn& Options)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	std::vector<std::string> lines = SpawnLordNow(Options);
	if (!lines.empty())
		g_Now.Last = lines.front();
	return lines;
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
