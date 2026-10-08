#include "Jobs.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Game.hpp"
#include "core/CostBook.hpp"
#include "core/Knobs.hpp"
#include "core/Retry.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <vector>

using namespace YYTK;
using NlCore::PathStep;
using NlCore::Shortest;
using NlJobs::Visit;

namespace
{
	constexpr const char* k_GameplayVars = "global.__gameplay_vars";

	NlJobs::LogFn g_Log;
	double g_Next = 0;
	bool g_VarsMissingLogged = false;		// 같은 줄을 주기마다 적지 않는다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	struct Job
	{
		NlJobs::JobDef Def;
		NlCore::CostBook Book;		// 처음 본 값. 비어 있지 않으면 되돌릴 것이 남아 있다
		NlCore::Retry Retry{ 2, 60 };
		double Target = 1;			// 바라는 배율. 1 은 원래 값, 0 은 "0 으로 쓴다"
		int Misses = 0;				// 되돌릴 자리를 잇달아 찾지 못한 횟수
		double NextPass = 0;
		bool Settled = false;		// 바라는 배율이 다 써졌다
		bool Announced = false;
		std::string Note, Logged;
	};
	std::vector<Job> g_Jobs;		// 등록한 차례대로(Init 에서만 더한다. 틱은 보기만 한다)

	std::string Place(const std::string& Key, int Level, int Slot)
	{
		return Key + (Level ? " product " + std::to_string(Level) : "") + (Slot < 0 ? "" : " resource " + std::to_string(Slot));
	}

	// 같은 줄을 잇달아 적지 않는다(주기마다 같은 실패가 되풀이될 수 있다).
	void LogOnce(Job& J, const std::string& Line)
	{
		if (Line == J.Logged)
			return;
		J.Logged = Line;
		Log(Line);
	}

	// 모든 대상에 바라는 값을 쓴다. Target 이 1 이면 장부의 값으로 되돌리고, 되돌린 자리는 장부에서 지운다.
	void Pass(Job& J, double Now)
	{
		const bool restoring = J.Target == 1;
		const bool again = J.Settled;		// 다 써 둔 뒤의 훑기다. 여기서 또 쓰게 되면 게임이 값을 되돌린 것이다
		const size_t before = J.Book.Size();
		std::vector<char> restored(before, 0);
		size_t written = 0, failed = 0, held = 0;
		std::string first_failure, why;
		// 이번 훑기에서 다룬 자리(담는 것의 포인터, 단계). 게임이 한 구조체를 두 이름 아래에서 함께 쓰면 같은 자리를 두 번 만난다.
		// 두 번째는 건너뛴다: 써 둔 값을 다른 열쇠의 바탕으로 적어 배율을 두 번 곱하지 않게.
		std::vector<std::pair<const void*, std::string>> touched;

		if (!J.Announced)
		{
			J.Announced = true;
			Log(std::string(J.Def.What) + ": walking the game data for the first time");		// 부르기 전에 남긴다(건물 종류를 얻는 스크립트를 부른다)
		}
		const bool walked = J.Def.Walk([&](const RValue& in, const PathStep& step, const std::string& key, int level, int slot, const RValue& value) {
			if (!NlGame::IsNumber(value))
				return;
			const std::pair<const void*, std::string> place(in.m_Pointer, step.Kind + step.Name + Shortest(step.Index));
			if (std::find(touched.begin(), touched.end(), place) != touched.end())
				return;
			touched.push_back(place);

			const double current = value.ToDouble();
			double wanted = current;
			size_t index = 0;
			if (!NlCore::PlanValue(J.Book, key, level, slot, current, J.Target, J.Def.Zero, wanted, index))
				return;
			std::string write_why;
			if (current == wanted || NlAccess::SetNumber(in, step, wanted, write_why))
			{
				written += current != wanted;
				held++;
				if (restoring && index < restored.size())
					restored[index] = 1;
			}
			else if (!failed++)
				first_failure = Place(key, level, slot) + ": " + write_why;
		}, why);

		if (!walked)
		{
			LogOnce(J, std::string(J.Def.What) + ": not written: " + why);
			J.Retry.Failed(Now);
			J.Note = "하지 못했습니다: " + why;
			return;
		}

		if (restoring)
			J.Book.Forget(restored);
		const std::string target = restoring ? "the first values" : J.Def.Zero ? "0" : "x" + Shortest(J.Target);
		if (written || failed || (restoring && before))
			LogOnce(J, std::string(J.Def.What) + ": wrote " + std::to_string(written) + " value(s) to " + target + ", " + std::to_string(held) + " hold it, book "
				+ std::to_string(J.Book.Size()) + (failed ? ", " + std::to_string(failed) + " did not stick (first: " + first_failure + ")" : ""));
		if (written && J.Def.After)
			J.Def.After();

		if (failed || (restoring && !J.Book.Empty()))
		{
			// 써지지 않았거나, 되돌릴 자리를 다시 찾지 못했다(게임이 그 자료를 다시 만들었으면 그 자리는 이미 원래 값이다).
			J.Retry.Failed(Now);
			J.Misses = restoring && !failed ? J.Misses + 1 : 0;		// 쓰기 실패와 따로 센다
			if (J.Misses >= 5)
			{
				Log(std::string(J.Def.What) + ": giving up on " + std::to_string(J.Book.Size()) + " value(s): their place is gone");
				J.Book.Clear();
				J.Retry.Succeeded();
				J.Misses = 0;
				J.Settled = true;
				J.Note.clear();
				return;
			}
			J.Note = failed ? std::to_string(failed) + "칸이 써지지 않았습니다 (" + first_failure + ")"
				: std::to_string(J.Book.Size()) + "칸을 되돌리지 못했습니다 (다시 해 봅니다)";
			return;
		}

		J.Misses = 0;
		if (again && written)
			J.Retry.Failed(Now);		// 게임이 값을 되돌려 다시 썼다. 잇달아 그러면 주기마다 쓰지 않게 간격을 늘린다(src/Build.cpp 와 같다)
		else
			J.Retry.Succeeded();
		J.Settled = true;
		J.Note = restoring ? std::string() : std::to_string(held) + (J.Def.Zero ? "칸을 0 으로 썼습니다" : "칸에 배율 " + Shortest(J.Target) + " 을 썼습니다");
	}

	void Tick(Job& J, double Now)
	{
		double factor = 1;
		const bool want = J.Def.Zero ? NlCheats::IsOn(J.Def.Cheat) : (NlCheats::Factor(J.Def.Cheat, factor) && factor > 0 && factor != 1);
		const double target = !want ? 1 : J.Def.Zero ? 0 : factor;
		if (target != J.Target)
		{
			J.Target = target;
			J.Settled = false;
			J.Retry.Succeeded();		// 사용자가 바꿨다. 기다리지 않고 바로 한다
		}
		if (!want && J.Book.Empty())
			return;						// 되돌릴 것이 없다
		if (!J.Def.AnyScreen && !NlAccess::InGame())
		{
			if (want)
				NlCheats::SetNote(J.Def.Cheat, "게임을 시작하면 적용");
			return;
		}
		if (!J.Retry.Due(Now) || (J.Settled && Now < J.NextPass))
		{
			NlCheats::SetNote(J.Def.Cheat, J.Note);
			return;
		}

		J.NextPass = Now + J.Def.Period;
		Pass(J, Now);
		NlCheats::SetNote(J.Def.Cheat, J.Note);
	}
}

void NlJobs::Log(const std::string& Line)
{
	::Log(Line);
}

	// 게임 변수(global.__gameplay_vars)의 열쇠들을 넘긴다. Key 는 그 열쇠다. 없는 열쇠는 건너뛴다.
bool NlJobs::WalkVars(const Visit& V, const std::vector<const char*>& Keys, std::string& Why)
{
	RValue vars;		// 이 함수 안에서만 든다
	if (!NlAccess::Read(NlCore::ParseAskPath(k_GameplayVars), vars, Why) || !vars.IsStruct())
	{
		Why = "the gameplay variables are not available";
		return false;
	}
	size_t found = 0;
	for (const char* key : Keys)
	{
		const PathStep step{ '.', key, 0 };
		RValue value;
		std::string ignored;
		if (NlAccess::Follow(vars, { step }, value, ignored))
		{
			V(vars, step, key, 0, -1, value);
			found++;
		}
	}
	// 바란 열쇠가 다 있지 않으면 한 번 남긴다(게임이 갱신돼 이름이 바뀌면 조용히 일부만 쓰게 된다).
	if (found != Keys.size() && !g_VarsMissingLogged)
	{
		g_VarsMissingLogged = true;
		Log("gameplay variables: only " + std::to_string(found) + " of " + std::to_string(Keys.size()) + " keys exist (first asked: " + (Keys.empty() ? "" : Keys.front()) + ")");
	}
	if (found == 0)
	{
		Why = "none of the gameplay variables exist";
		return false;
	}
	return true;
}

void NlJobs::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

void NlJobs::Add(const JobDef& Def)
{
	for (const Job& job : g_Jobs)
		if (std::string(job.Def.Cheat) == Def.Cheat)
		{
			Log(std::string("jobs: ") + Def.Cheat + " is already registered; ignoring the second");
			return;
		}
	Job job;
	job.Def = Def;
	g_Jobs.push_back(std::move(job));
}

void NlJobs::GameTick(double Now)
{
	if (Now < g_Next)
		return;
	g_Next = Now + 1.0;
	for (Job& job : g_Jobs)
		Tick(job, Now);
}
