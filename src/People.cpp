#include "People.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
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
	constexpr double k_Unknown = -1e9;										// 읽지 못한 수

	struct Detail			// 고른 사람의 값. RValue 를 담지 않는다
	{
		bool Ready = false;
		std::string Uuid;
		double Age = k_Unknown, Moral = k_Unknown, Pain = k_Unknown, MindSum = k_Unknown;
		std::vector<double> Skills, Points;		// 능력치 여덟의 등급과 점수. 그 사람에게 없는 칸은 k_Unknown(주민은 전투만 있다)
		std::vector<double> Needs, Limits;		// 욕구 여섯과 상한
		std::vector<std::string> Traits;
	};

	struct Snapshot			// 틱이 채우고 Draw 가 읽는다
	{
		bool Ready = false;
		std::string Why;
		std::vector<PersonRow> People;
		std::vector<std::string> TraitNames;	// 게임에 있는 특성의 이름(이름순)
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

	// 표의 항목(플레이어의 사람을 조금씩 돌며 쓴다)
	std::vector<PersonRow> g_HoldPeople;
	size_t g_HoldCursor = 0;
	double g_NextHold = 0, g_NextHoldScan = 0, g_NextHappy = 0;
	bool g_HappyRound = false;			// 이번 바퀴에서 행복 생각을 본다
	bool g_AgeTouched = false;			// "노화로 죽을 수 있다"를 끈 사람이 있다(되돌릴 것이 있다)
	size_t g_RoundNeeds = 0, g_RoundHappy = 0, g_RoundAge = 0, g_RoundPeople = 0;

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
				double dead = 0;
				if (NlAccess::ReadNumber(Base(row) + ".c_status.__is_dead", dead))
					row.Dead = dead != 0;

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
			LoadTraitNames();
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
			Log("people call get_age(), get_pain(), get_total_modify() on " + Base(Row) + " " + Row.Uuid);		// 사람마다 한 번만 남긴다
		}
		CallNumber(base + ".get_age", Out.Age);
		CallNumber(base + ".get_pain", Out.Pain);
		CallNumber(base + ".__minds.get_total_modify", Out.MindSum);
		Out.Ready = true;
		return true;
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
	bool One(const PersonCommand& C, const PersonRow& Row, std::string& Note)
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
			if (CallNumber(soul + ".__minds.get_total_modify", sum) && sum >= 100)
			{
				Note = "이미 생각의 합이 100 을 넘습니다";
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
		if (NlCore::NeedsText(C.Act) && !KnownTrait(C.Text))
			return "게임에 없는 특성입니다: " + C.Text;

		const std::vector<size_t> targets = NlCore::PickTargets(g_Now.People, C.Who);
		if (targets.empty())
			return "대상이 없습니다";

		size_t done = 0;
		std::string first_failure, note;
		for (const size_t at : targets)
		{
			const PersonRow row = g_Now.People[at];		// 사본(게임의 함수가 사람을 늘리거나 줄여도 흔들리지 않게)
			std::string one;
			if (One(C, row, one))
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
		for (const char* id : k_HoldIds)
			NlCheats::SetNote(id, NlCheats::IsOn(id) ? Note : std::string());
	}

	void HoldTick(double Now)
	{
		const bool hunger = NlCheats::IsOn("no_hunger"), tired = NlCheats::IsOn("no_tiredness"), all = NlCheats::IsOn("needs_full");
		const bool happy = NlCheats::IsOn("always_happy"), ageless = NlCheats::IsOn("no_old_age_death");
		const std::vector<int> needs = NlCore::NeedsToHold(hunger, tired, all);
		if (needs.empty() && !happy && !ageless && !g_AgeTouched)
		{
			g_HoldCursor = 0;
			return;
		}
		if (!NlAccess::InGame())
		{
			HoldNotes("게임을 시작하면 적용");
			g_AgeTouched = false;		// 새로 불러온 게임의 깃발은 처음 값이다(세이브에 남지 않는다)
			g_HoldCursor = 0;
			return;
		}
		if (Now < g_NextHold)
			return;
		g_NextHold = Now + 0.25;

		if (g_HoldCursor == 0)
		{
			// 바퀴의 처음: 사람들을 다시 읽는다(5초에 한 번까지).
			if (Now >= g_NextHoldScan || g_HoldPeople.empty())
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
			g_RoundNeeds = g_RoundHappy = g_RoundAge = g_RoundPeople = 0;
		}

		const NlCore::PeopleSlice slice = NlCore::NextPeopleSlice(g_HoldPeople.size(), g_HoldCursor, 40);
		for (size_t i = slice.Begin; i < slice.End; i++)
		{
			const PersonRow& row = g_HoldPeople[i];
			RValue soul;
			if (!StillThere(row, soul))
			{
				g_NextHoldScan = 0;		// 사람이 드나들었다. 다음 바퀴에서 다시 읽는다
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
						// 거의 찬 칸은 건드리지 않는다(게임은 한 번에 0.3 쯤씩 줄인다).
						if (NlCore::NeedValue(limit, limit, wanted) && current < wanted - 0.5 && NlAccess::SetNumber(values, step, wanted, why))
							g_RoundNeeds++;
					}
				}
			}

			if (ageless || g_AgeTouched)
			{
				// __soul.__aging.__old.__debug_is_can_die_of_old_age: 인물마다 true 다. 켜면 false 로, 끄면 다시 true 로 쓴다.
				RValue old;
				const PathStep flag{ '.', "__debug_is_can_die_of_old_age", 0 };
				double current = 0;
				const double wanted = ageless ? 0 : 1;
				if (NlAccess::Follow(soul, { { '.', "__aging", 0 }, { '.', "__old", 0 } }, old, why) && old.IsStruct()
					&& FollowNumber(old, { flag }, current) && current != wanted && NlAccess::SetNumber(old, flag, wanted, why))
					g_RoundAge++;
			}

			if (g_HappyRound)
			{
				PersonCommand command;
				command.Act = PersonAct::Happy;
				command.Who = row.Uuid;
				std::string note;
				if (One(command, row, note) && note.empty())
					g_RoundHappy++;
			}
		}
		g_HoldCursor = slice.Next;

		if (slice.Wrapped)
		{
			if (g_RoundNeeds || g_RoundHappy || g_RoundAge)
				Log("people hold: " + std::to_string(g_RoundPeople) + " people, " + std::to_string(g_RoundNeeds) + " need(s) filled, "
					+ std::to_string(g_RoundHappy) + " happy mind(s), " + std::to_string(g_RoundAge) + " old-age flag(s) " + (ageless ? "off" : "back on"));
			g_AgeTouched = ageless;		// 끈 뒤의 한 바퀴가 되돌렸다
			HoldNotes(std::to_string(g_RoundPeople) + "명에게 적용 중");
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

	void DrawLast()
	{
		if (!g_Now.Last.empty())
			ImGui::TextDisabled("%s", g_Now.Last.c_str());
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
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("##age", &g_AgeInput);
		ImGui::SameLine();
		if (ImGui::Button("이 나이로"))
			Push(PersonAct::AgeSet, who, -1, g_AgeInput);
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
		ImGui::TextDisabled("기분은 게임이 생각의 합으로 다시 셈합니다. '행복하게'는 게임의 디버그용 생각(+100, 하루)을 붙입니다.");

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
					Push(PersonAct::NeedSet, who, static_cast<int>(i), limit);
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
			if (ImGui::SmallButton("떼기"))
				Push(PersonAct::TraitRemove, who, -1, 0, one.Traits[i]);
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
				if (name.find(g_TraitFilter) == std::string::npos || Has(one.Traits, name))
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
		ImGui::TextDisabled("특성은 게임의 이름 그대로입니다. 상태(human, kid, dead 같은 것)를 떼면 게임이 어떻게 받는지는 재지 않았습니다.");
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

	HoldTick(Now);

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

void NlPeople::DrawPerson()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	ImGui::BeginChild("who", ImVec2(210, 0), ImGuiChildFlags_Borders);
	ImGui::Checkbox("주민·손님도 보기", &g_ShowAll);
	for (const PersonRow& row : g_Now.People)
	{
		if (!g_ShowAll && !(NlCore::IsPlayers(row) && row.Character))
			continue;
		const std::string label = row.Name + "  " + KindText(row) + "##" + row.Uuid;
		ImGui::BeginDisabled(row.Dead);
		if (ImGui::Selectable(label.c_str(), g_Selected == row.Uuid))
			g_Selected = row.Uuid;
		ImGui::EndDisabled();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("one", ImVec2(0, 0));
	const PersonRow* row = g_Selected.empty() ? nullptr : FindRow(g_Selected);
	if (!row)
		ImGui::TextDisabled("왼쪽에서 사람을 고르세요.");
	else
		DrawDetail(*row);
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
	ImGui::TextDisabled("손님과 다른 진영의 영주에게는 가지 않습니다. 한 사람씩 고치려면 '인물'에서 고릅니다.");
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
	ImGui::TextDisabled("계속 유지하려면 위의 항목을 켭니다. 인구는 '날마다 추가 이주민'에 수를 넣으면 다음 이주 때(저녁) 그만큼 더 옵니다.");
	DrawLast();
}

std::vector<std::string> NlPeople::Do(const NlCore::PersonCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	g_Now.Last = Execute(Command);
	return { g_Now.Last };
}

std::vector<std::string> NlPeople::List(bool All)
{
	std::lock_guard lock(g_Mutex);
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
