#include "Crime.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "Ui.hpp"
#include "People.hpp"
#include "core/AskPath.hpp"
#include "core/Guard.hpp"
#include "core/Text.hpp"
#include "core/PeoplePlan.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <mutex>
#include <vector>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::CrimeAct;
using NlCore::CrimeCommand;
using NlCore::PersonRow;
using NlCore::Vagabond;

namespace
{
	constexpr const char* k_GameTime = "inst:o_time_controller.__game_time";

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다
	NlCrime::LogFn g_Log;
	bool g_Busy = false;				// 하는 중이다(여기서 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다)

	// 죄나 혐의가 있는 플레이어의 영주 한 사람.
	struct Lord
	{
		std::string Uuid, Name;
		std::vector<std::string> Sins, Accusations;		// 게임의 이름들
		std::string SinText, AccusationText;			// 창에 보일 글(화면 이름)
	};

	// 틱이 읽은 것. 창은 이것만 그린다.
	struct View
	{
		bool Read = false;
		std::string Why;				// 읽지 못한 까닭
		double Now = 0;					// 게임 시각
		std::vector<Vagabond> Vagabonds;
		int Thugs = 0;
		int UnreadPeople = 0;			// 범죄의 깃발을 읽지 못한 주민("없음"과 가른다)
		std::vector<Lord> Lords;		// 죄나 혐의가 있는 영주
		int LordCount = 0;				// 살펴본 플레이어의 영주
		int UnreadLords = 0;			// 특성을 읽지 못한 영주
	};
	View g_View;
	std::deque<CrimeCommand> g_Queue;	// 창이 쌓고 틱이 한다
	std::string g_LastCrime;			// 부랑자에게 마지막으로 한 일
	std::string g_LastLords;			// 영주에게 마지막으로 한 일
	double g_Next = 0;					// 다음에 다시 읽을 시각

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	std::string Base(bool Character, int Index)
	{
		return std::string("inst:") + (Character ? "o_character" : "o_dummy") + ":" + std::to_string(Index);
	}

	// 그 자리에 아직 그 사람이 있는가(사람이 드나들면 번호가 밀린다).
	bool StillThere(const std::string& BasePath, const std::string& Uuid)
	{
		std::string now;
		return NlAccess::ReadText(BasePath + ".__soul.__uuid", now) && now == Uuid;
	}

	// 가진 특성의 이름들. 목록을 읽지 못하면 거짓(빈 목록과 가른다).
	bool ReadTraits(const std::string& BasePath, std::vector<std::string>& Out)
	{
		RValue list;		// 이 함수 안에서만 든다
		Holder kind = Holder::None;
		std::string why;
		Out.clear();
		if (!NlAccess::Open(NlCore::ParseAskPath(BasePath + ".__soul.__traits.__list_of_traits"), list, kind, why) || kind != Holder::Array)
			return false;
		NlAccess::ForEachChild(list, kind, [&](const NlCore::PathStep&, const RValue& name) {
			if (name.IsString())
				Out.push_back(name.ToString());
			return true;
		});
		return true;
	}

	// 범죄자의 깃발. 읽었는가와 그 값을 따로 준다(읽지 못한 것을 "범죄자가 아니다"로 치지 않게).
	bool ReadFlag(const std::string& BasePath, double& Raw)
	{
		Raw = 0;
		return NlAccess::ReadNumber(BasePath + ".c_criminal.__is_dummy_criminal", Raw);
	}

	bool IsVagabond(const std::string& BasePath)
	{
		double raw = 0;
		const bool read = ReadFlag(BasePath, raw);
		return NlCore::IsVagabondFlag(read, raw);
	}

	double StolenGold(const std::string& BasePath)
	{
		double gold = 0;
		return NlAccess::ReadNumber(BasePath + ".c_criminal.__stolen_gold", gold) && std::isfinite(gold) ? gold : 0;
	}

	std::string Labels(const std::vector<std::string>& Names)
	{
		std::string out;
		for (const std::string& name : Names)
			out += std::string(out.empty() ? "" : ", ") + NlPeople::TraitName(name);
		return out;
	}

	enum class ScanResult { Ok, Busy, Failed };

	// 플레이어의 주민에게서 범죄자를, 플레이어의 영주에게서 죄와 혐의를 읽는다. Rows: 인물 모듈이 지금 읽은 사람들(부르는 쪽이 대상을 찾을 때 쓴다).
	ScanResult Scan(View& Out, std::vector<PersonRow>& Rows)
	{
		Out = View();
		if (!NlAccess::InGame())
		{
			Out.Why = "게임 화면이 아닙니다";
			return ScanResult::Failed;
		}
		std::string why;
		const NlPeople::RowsResult read = NlPeople::Rows(Rows, why);
		if (read == NlPeople::RowsResult::Busy)
			return ScanResult::Busy;
		if (read != NlPeople::RowsResult::Ok)
		{
			Out.Why = "사람들을 읽지 못했습니다: " + why;
			return ScanResult::Failed;
		}
		NlAccess::ReadNumber(k_GameTime, Out.Now);
		for (const PersonRow& row : Rows)
		{
			if (!NlCore::IsPlayers(row))		// 플레이어의 산 사람만: 손님, 다른 진영, 적 부대는 보지 않는다
				continue;
			const std::string base = Base(row.Character, row.Index);
			if (!row.Character)
			{
				double raw = 0;
				if (!ReadFlag(base, raw))
				{
					Out.UnreadPeople++;
					continue;
				}
				if (!NlCore::IsVagabondFlag(true, raw))
					continue;
				Vagabond who;
				who.Uuid = row.Uuid;
				who.Name = row.Name;
				who.Index = row.Index;
				double thug = 0;
				who.Thug = NlAccess::ReadNumber(base + ".c_criminal.__is_dummy_thug", thug) && thug > 0;
				NlAccess::ReadNumber(base + ".c_criminal.__criminal_begin_time", who.Begin);
				who.StolenGold = StolenGold(base);
				Out.Thugs += who.Thug ? 1 : 0;
				Out.Vagabonds.push_back(std::move(who));
				continue;
			}
			Out.LordCount++;
			std::vector<std::string> traits;
			if (!ReadTraits(base, traits))
			{
				Out.UnreadLords++;
				continue;
			}
			Lord lord;
			lord.Uuid = row.Uuid;
			lord.Name = row.Name;
			lord.Sins = NlCore::CrimeTraits(traits, true);
			lord.Accusations = NlCore::CrimeTraits(traits, false);
			if (lord.Sins.empty() && lord.Accusations.empty())
				continue;
			lord.SinText = Labels(lord.Sins);
			lord.AccusationText = Labels(lord.Accusations);
			Out.Lords.push_back(std::move(lord));
		}
		Out.Read = true;
		return ScanResult::Ok;
	}

	const PersonRow* FindRow(const std::vector<PersonRow>& Rows, const std::string& Uuid)
	{
		for (const PersonRow& row : Rows)
			if (row.Uuid == Uuid)
				return &row;
		return nullptr;
	}

	// 범죄자 지정을 푼다: 게임의 set_criminal_scum(false, true). 게임이 (true, true)와 (true)로 부르는 것을 봤고, (false, true)로 불러 깃발이 거짓이 되는 것을 봤다
	// (한 사람씩, 그리고 한 틱에 셋을 잇달아. research/26). 깡패는 풀지 않는다(core 의 CanClear).
	std::vector<std::string> Clear(const View& Seen, const std::string& Who)
	{
		std::vector<Vagabond> targets;
		int thugs = 0;
		for (const Vagabond& who : Seen.Vagabonds)
		{
			if (Who != "all" && who.Uuid != Who)
				continue;
			if (NlCore::CanClear(who))
				targets.push_back(who);
			else
				thugs++;
		}
		const std::string thug_note = thugs > 0 ? " (깡패 " + std::to_string(thugs) + "명은 되돌리지 않습니다: 깡패의 지정을 풀면 어떻게 되는지 재지 못했습니다)" : std::string();
		if (Who != "all" && targets.empty())
			return { thugs > 0 ? "깡패는 되돌리지 않습니다: 깡패의 지정을 풀면 어떻게 되는지 재지 못했습니다" : "그 사람은 플레이어의 부랑자가 아닙니다" };
		int done = 0, skipped = 0, unsure = 0;
		std::string why;
		for (const Vagabond& who : targets)
		{
			const std::string base = Base(false, who.Index);
			if (!StillThere(base, who.Uuid) || !IsVagabond(base))		// 부르기 바로 전에 다시 본다
			{
				skipped++;
				continue;
			}
			RValue result;
			std::string note;
			Log("crime call set_criminal_scum(false, true) on " + who.Uuid);		// 부르기 전에 남긴다
			if (!NlAccess::CallMethod(NlCore::ParseAskPath(base + ".c_criminal.set_criminal_scum"), { RValue(false), RValue(true) }, result, note))
			{
				why = "게임의 함수를 부르지 못했습니다 (" + note + ")";
				continue;
			}
			double raw = 0;
			const bool same = StillThere(base, who.Uuid);
			const bool read = same && ReadFlag(base, raw);
			switch (NlCore::AfterClear(same, read, raw))
			{
			case NlCore::ClearOutcome::Cleared: done++; break;
			case NlCore::ClearOutcome::Unknown: unsure++; break;		// 깃발을 읽지 못했거나 그 자리의 사람이 바뀌었다: 됐다고 적지 않는다
			case NlCore::ClearOutcome::Still: why = "게임의 깃발이 그대로입니다"; break;
			}
		}
		if (!targets.empty())
			Log("crime: clear: " + std::to_string(done) + " of " + std::to_string(targets.size()) + " cleared, " + std::to_string(skipped) + " skipped, "
				+ std::to_string(unsure) + " unsure" + (why.empty() ? "" : ", " + why));
		return { NlCore::ClearReport(static_cast<int>(targets.size()), done, skipped, unsure, why) + thug_note };
	}

	// 게임의 return_back_stolen_to_player_warehouse()(인자 없음)를 부른다. 게임이 부르는 것은 봤지만(사람이 지도에서 없어질 때로 보인다) 훔친 것이 있는 사람에게서는
	// 보지 못했다: 확인 전이다. 그래서 부르기 앞뒤의 훔친 금화를 읽어 결과에 적는다(효과가 있었는지 그 줄로 알 수 있게).
	std::vector<std::string> ReturnStolen(const View& Seen, const std::string& Who)
	{
		std::vector<Vagabond> targets;
		for (const Vagabond& who : Seen.Vagabonds)
			if (Who == "all" || who.Uuid == Who)
				targets.push_back(who);
		if (Who != "all" && targets.empty())
			return { "그 사람은 플레이어의 부랑자가 아닙니다" };
		int called = 0;
		double before = 0, after = 0;
		std::string why;
		for (const Vagabond& who : targets)
		{
			const std::string base = Base(false, who.Index);
			if (!StillThere(base, who.Uuid) || !IsVagabond(base))
				continue;
			const double had = StolenGold(base);
			RValue result;
			std::string note;
			Log("crime call return_back_stolen_to_player_warehouse() on " + who.Uuid + " (stolen gold " + std::to_string(std::llround(had)) + ")");
			if (!NlAccess::CallMethod(NlCore::ParseAskPath(base + ".c_criminal.return_back_stolen_to_player_warehouse"), {}, result, note))
			{
				why = note;
				continue;
			}
			called++;
			before += had;
			const double left = StillThere(base, who.Uuid) ? StolenGold(base) : had;
			after += left;
			Log("crime: return stolen: " + who.Uuid + " stolen gold " + std::to_string(std::llround(had)) + " -> " + std::to_string(std::llround(left)));
		}
		return { NlCore::StolenReport(called, before, after, why) };
	}

	// 영주의 죄(Sins)나 범죄 혐의를 지운다: 인물 모듈의 특성 떼기(게임의 trait_detach)를 그 특성마다 부르고, 특성을 다시 읽어 없어졌는지 본다.
	// 죄는 떼어 봤다(영주 둘의 셋). 혐의의 특성을 가진 영주는 보지 못해 혐의 쪽은 해 보지 못했다(확인 전).
	std::vector<std::string> ClearTraits(const std::vector<PersonRow>& Rows, const std::string& Who, bool Sins)
	{
		std::vector<PersonRow> lords;
		for (const PersonRow& row : Rows)
			if (NlCore::IsPlayersLord(row) && (Who == "lords" || row.Uuid == Who))
				lords.push_back(row);
		if (Who != "lords" && lords.empty())
			return { FindRow(Rows, Who) ? "플레이어의 영주에게만 합니다" : "그런 사람이 없습니다" };
		int removed = 0, failed = 0, unread = 0;
		std::string why;
		for (const PersonRow& lord : lords)
		{
			const std::string base = Base(true, lord.Index);
			std::vector<std::string> traits;
			if (!StillThere(base, lord.Uuid) || !ReadTraits(base, traits))		// 읽지 못한 영주를 "지울 것이 없다"로 치지 않는다
			{
				unread++;
				continue;
			}
			for (const std::string& name : NlCore::CrimeTraits(traits, Sins))
			{
				NlCore::PersonCommand detach;
				detach.Act = NlCore::PersonAct::TraitRemove;
				detach.Who = lord.Uuid;
				detach.Text = name;
				const std::vector<std::string> answer = NlPeople::Do(detach);		// 인물 모듈이 부르기 전에 로그를 남기고 그 자리의 uuid 를 다시 본다
				std::vector<std::string> after;
				const bool gone = StillThere(base, lord.Uuid) && ReadTraits(base, after) && std::find(after.begin(), after.end(), name) == after.end();
				if (gone)
					removed++;
				else
				{
					failed++;
					why = answer.empty() ? std::string("no answer") : answer.back();
				}
			}
		}
		Log(std::string("crime: ") + (Sins ? "absolve" : "acquit") + ": " + std::to_string(lords.size()) + " lord(s), removed " + std::to_string(removed) + ", failed " + std::to_string(failed)
			+ ", unread " + std::to_string(unread));
		return { NlCore::TraitClearReport(Sins, static_cast<int>(lords.size()), removed, failed, unread, why) };
	}

	std::vector<std::string> ListLines(const View& Seen)
	{
		std::vector<std::string> out;
		out.push_back(NlCore::CrimeSummary(static_cast<int>(Seen.Vagabonds.size()), Seen.Thugs, Seen.UnreadPeople));
		for (const Vagabond& who : Seen.Vagabonds)
			out.push_back(who.Uuid + "  " + NlCore::VagabondLine(who, Seen.Now));
		out.push_back(NlCore::LordsLine(Seen.LordCount, static_cast<int>(Seen.Lords.size()), Seen.UnreadLords));
		for (const Lord& lord : Seen.Lords)
		{
			std::string line = lord.Uuid + "  " + lord.Name;
			for (const std::string& name : lord.Sins)
				line += " " + name;
			for (const std::string& name : lord.Accusations)
				line += " " + name;
			out.push_back(line);
		}
		return out;
	}

	bool IsLordAct(CrimeAct Act)
	{
		return Act == CrimeAct::Absolve || Act == CrimeAct::Acquit;
	}

	// 한 가지 일을 한다. PeopleBusy: 인물 쪽이 게임의 함수를 부르는 중이었다(아무것도 하지 않았다. 다음 틱에 다시 한다).
	std::vector<std::string> DoNow(const CrimeCommand& C, bool& PeopleBusy)
	{
		PeopleBusy = false;
		View seen;
		std::vector<PersonRow> rows;
		const ScanResult scan = Scan(seen, rows);
		if (scan == ScanResult::Busy)
		{
			PeopleBusy = true;
			return { "busy" };
		}
		if (scan != ScanResult::Ok)
			return { seen.Why };
		std::vector<std::string> out;
		switch (C.Act)
		{
		case CrimeAct::List: out = ListLines(seen); break;
		case CrimeAct::Clear: out = Clear(seen, C.Who); break;
		case CrimeAct::ReturnStolen: out = ReturnStolen(seen, C.Who); break;
		case CrimeAct::Absolve: out = ClearTraits(rows, C.Who, true); break;
		case CrimeAct::Acquit: out = ClearTraits(rows, C.Who, false); break;
		}
		if (C.Act == CrimeAct::List)
		{
			g_View = seen;
			return out;
		}
		if (!out.empty())
			(IsLordAct(C.Act) ? g_LastLords : g_LastCrime) = out.front();
		View after;		// 바꾼 뒤의 것을 창에 보인다
		std::vector<PersonRow> again;
		if (Scan(after, again) == ScanResult::Ok)
			g_View = after;
		return out;
	}

	// 흐린 글. 창의 너비에서 줄을 바꾼다.
	constexpr size_t k_MaxQueue = 8;		// 창이 쌓아 둘 명령의 수. 넘치면 받지 않고 결과 줄에 적는다(조용히 버리지 않는다. 2026-10-07 리뷰 R3)

	void Push(CrimeAct Act, const std::string& Who)
	{
		if (g_Queue.size() >= k_MaxQueue)
		{
			(IsLordAct(Act) ? g_LastLords : g_LastCrime) = NlCore::QueueFullText(k_MaxQueue);
			return;
		}
		g_Queue.push_back(CrimeCommand{ Act, Who });
	}
}

void NlCrime::Init(LogFn Log_)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
}

void NlCrime::GameTick(double Now, bool Visible)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return;
	if (g_Queue.empty() && (!Visible || Now < g_Next))		// 시각부터 본다(이 틱은 오브젝트 이벤트마다 불린다)
		return;
	const NlCore::ScopedFlag busy(g_Busy);
	if (!g_Queue.empty())
	{
		bool people_busy = false;
		DoNow(g_Queue.front(), people_busy);
		if (people_busy)		// 쌓인 일을 버리지 않는다. 다음 틱에 다시 한다
			return;
		g_Queue.pop_front();
		g_Next = Now + 1;
		return;
	}
	g_Next = Now + 1;
	View view;
	std::vector<PersonRow> rows;
	if (Scan(view, rows) != ScanResult::Busy)		// 인물 쪽이 바쁘면 앞에 읽은 것을 둔다
		g_View = view;
}

void NlCrime::Draw()
{
	std::lock_guard lock(g_Mutex);
	ImGui::SeparatorText("부랑자 (범죄자가 된 주민)");
	if (!g_View.Read)
		NlUi::Hint(g_View.Why.empty() ? "읽는 중입니다" : g_View.Why.c_str());
	else
	{
		ImGui::TextUnformatted(NlCore::CrimeSummary(static_cast<int>(g_View.Vagabonds.size()), g_View.Thugs, g_View.UnreadPeople).c_str());
		ImGui::BeginDisabled(g_View.Vagabonds.empty());
		if (ImGui::Button("부랑자 모두 주민으로 되돌리기"))
			Push(CrimeAct::Clear, "all");
		ImGui::EndDisabled();
		for (const Vagabond& who : g_View.Vagabonds)
		{
			ImGui::PushID(who.Uuid.c_str());
			if (NlCore::CanClear(who))
			{
				if (ImGui::SmallButton("되돌리기"))
					Push(CrimeAct::Clear, who.Uuid);
				ImGui::SameLine();
			}
			ImGui::TextUnformatted(NlCore::VagabondLine(who, g_View.Now).c_str());
			if (who.StolenGold > 0)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("훔친 것 되돌리기 (확인 전)"))
					Push(CrimeAct::ReturnStolen, who.Uuid);
			}
			ImGui::PopID();
		}
	}
	if (!g_LastCrime.empty())
		NlUi::Hint(g_LastCrime.c_str());
	NlUi::Hint("게임은 저녁(18:00)에 주민 가운데 몇을 범죄자(부랑자)로 만듭니다(무엇이 그들을 고르는지는 재지 못했습니다). '되돌리기'는 게임의 같은 함수로 그 지정을 풉니다. "
		"게임이 다시 고를 수 있습니다: 위의 '주민이 부랑자(범죄자)가 되지 않음'을 켠 저녁들에는 게임의 시도 셋이 모두 막혔습니다. 깡패는 되돌리지 않습니다(재지 못했습니다). "
		"'훔친 것 되돌리기'는 훔친 금화가 있는 부랑자의 줄에만 나옵니다: 게임의 함수를 부르는 것까지만 했고 효과는 확인 전입니다. "
		"범죄자의 지정은 세이브에 들어가는 자료입니다. 되돌리는 단추는 없습니다.");

	ImGui::SeparatorText("영주의 죄와 범죄 혐의");
	if (g_View.Read)
	{
		ImGui::TextUnformatted(NlCore::LordsLine(g_View.LordCount, static_cast<int>(g_View.Lords.size()), g_View.UnreadLords).c_str());
		ImGui::BeginDisabled(g_View.Lords.empty());
		if (ImGui::Button("모든 영주의 죄 지우기"))
			Push(CrimeAct::Absolve, "lords");
		ImGui::SameLine();
		if (ImGui::Button("모든 영주의 범죄 혐의 지우기 (확인 전)"))
			Push(CrimeAct::Acquit, "lords");
		ImGui::EndDisabled();
		for (const Lord& lord : g_View.Lords)
		{
			ImGui::PushID(lord.Uuid.c_str());
			ImGui::TextUnformatted(lord.Name.c_str());
			if (!lord.Sins.empty())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("죄 지우기"))
					Push(CrimeAct::Absolve, lord.Uuid);
			}
			if (!lord.Accusations.empty())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("혐의 지우기 (확인 전)"))
					Push(CrimeAct::Acquit, lord.Uuid);
			}
			// 긴 글은 이름 아래의 줄로 그린다(좁은 칸에 두지 않는다)
			ImGui::PushTextWrapPos(0.0f);
			if (!lord.Sins.empty())
				ImGui::TextDisabled("  죄: %s", lord.SinText.c_str());
			if (!lord.Accusations.empty())
				ImGui::TextDisabled("  혐의: %s", lord.AccusationText.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopID();
		}
	}
	if (!g_LastLords.empty())
		NlUi::Hint(g_LastLords.c_str());
	NlUi::Hint("게임에서 죄와 영주의 범죄 혐의는 특성입니다. 지우기는 그 특성을 뗍니다(인물 탭의 특성 떼기와 같은 길). 영주 둘의 죄 셋을 떼어 봤고, 그 가운데 하나에서 생각의 합이 오르는 것을 봤습니다. "
		"혐의의 특성을 가진 영주는 보지 못해 '혐의 지우기'는 해 보지 못했습니다(확인 전): 특성을 떼면 게임의 처벌 쪽이 어떻게 되는지도 모릅니다. 되돌리는 단추는 없습니다.");
}

std::vector<std::string> NlCrime::Do(const NlCore::CrimeCommand& Command)
{
	std::lock_guard lock(g_Mutex);
	if (g_Busy)
		return { "busy" };
	const NlCore::ScopedFlag busy(g_Busy);
	bool people_busy = false;
	return DoNow(Command, people_busy);
}
