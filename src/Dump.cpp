#include "Dump.hpp"

#include "Finder.hpp"
#include "Game.hpp"
#include "core/Request.hpp"
#include "core/Schedule.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

using namespace Aurie;
using namespace YYTK;
using NlCore::Fixed;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	constexpr double k_SampleSeconds = 0.5;		// 상태를 재는 간격
	// 기록 줄의 한도. 종류마다 따로 센다(이벤트나 자주 바뀌는 watch 가 state 줄을 밀어내지 않게).
	constexpr int k_MaxStateLines = 300;
	constexpr int k_MaxWatchLines = 200;
	constexpr int k_MaxEventLines = 250;
	constexpr int k_MaxOwnedPerObject = 512;	// 오브젝트마다 훑는 인스턴스 수

	NlCore::Request g_Request;
	NlCore::Schedule g_Schedule;
	const NlDump::Limits g_Limits{};
	std::function<void(const std::string&)> g_Log;
	fs::path g_Dir;
	std::string g_Version;
	std::chrono::steady_clock::time_point g_Start;
	std::atomic<bool> g_Active = false;

	// 아래는 게임 스레드에서만 만진다.
	bool g_Finished = false;
	bool g_Busy = false;			// 덤프 도중 이벤트가 일어나 Tick 이 다시 들어오는 것을 막는다
	bool g_Announced = false;
	double g_LastSample = -1;
	int g_StateLines = 0;
	int g_WatchLines = 0;
	int g_EventLines = 0;
	bool g_RoomKnown = false;
	std::string g_Room;										// 지난번의 룸 이름. 못 얻었으면 빈 글
	std::vector<std::string> g_Present;						// 지난번에 인스턴스가 있던 오브젝트
	std::unordered_map<std::string, int> g_Counts;			// 지난번의 오브젝트별 인스턴스 수
	std::vector<std::string> g_WatchLast;					// watch 마다 지난번에 적은 글
	std::unordered_set<const void*> g_SeenCode;

	double Elapsed()
	{
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - g_Start).count();
	}

	// 벽시계 시각 HH:MM:SS. 사용자가 알려 준 때와 덤프의 때를 맞춰 보는 데 쓴다.
	std::string Clock()
	{
		const std::time_t now = std::time(nullptr);
		std::tm local{};
		localtime_s(&local, &now);
		char buf[16];
		std::strftime(buf, sizeof(buf), "%H:%M:%S", &local);
		return buf;
	}

	bool Skipped(const char* Section)
	{
		return std::find(g_Request.Skip.begin(), g_Request.Skip.end(), Section) != g_Request.Skip.end();
	}

	// 기록 한 줄을 적는다. Lines 가 Max 에 닿으면 "<What> trace truncated" 를 한 번 적고 그 뒤로는 적지 않는다.
	void Trace(const std::string& Line, int& Lines, int Max, const char* What)
	{
		if (Lines > Max)
			return;
		if (Lines++ == Max)
		{
			g_Log(std::string(What) + " trace truncated");
			return;
		}
		g_Log(Line);
	}

	// 코드 객체의 이름을 읽는다. YYToolkit 의 CCode 배치가 이 러너와 맞는지는 확인하지 못했다.
	// 잘못된 포인터를 읽어도 게임이 죽지 않게 SEH 로 감싸고, 읽은 것이 글처럼 생겼을 때만 쓴다.
	// (이 함수에는 소멸자가 있는 지역 변수를 두지 않는다.)
	bool SafeCodeName(CCode* Code, char* Buffer, size_t Size)
	{
		__try
		{
			const char* name = Code->GetName();
			if (!name)
				return false;
			size_t i = 0;
			for (; i + 1 < Size && name[i]; i++)
			{
				if (name[i] < 0x20 || name[i] > 0x7E)
					return false;
				Buffer[i] = name[i];
			}
			Buffer[i] = '\0';
			return i > 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
	}

	// 룸 이름, 오브젝트별 인스턴스 수, watch 의 값을 재고, 달라진 것만 로그에 적는다.
	void Sample(double Now)
	{
		if (!g_Announced)
		{
			g_Announced = true;
			g_Log("objects " + std::to_string(NlGame::Objects().size()));
		}

		std::string changes;

		if (!Skipped("room"))
		{
			const std::string room = NlGame::RoomName();
			if (!g_RoomKnown || room != g_Room)
			{
				changes += " room=" + (room.empty() ? std::string("?") : room);
				g_Room = room;
				g_RoomKnown = true;
			}
		}

		std::vector<std::string> present;
		g_Counts.clear();
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
			g_Counts[object.Name] = count;
			if (count > 0)
				present.push_back(object.Name);
		}

		for (const std::string& name : present)
			if (std::find(g_Present.begin(), g_Present.end(), name) == g_Present.end())
				changes += " +" + name;
		for (const std::string& name : g_Present)
			if (std::find(present.begin(), present.end(), name) == present.end())
				changes += " -" + name;
		if (!changes.empty())
			Trace("state t=" + Fixed(Now, 1) + changes, g_StateLines, k_MaxStateLines, "state");
		g_Present = std::move(present);

		for (size_t i = 0; i < g_Request.Watches.size(); i++)
		{
			RValue value;
			const std::string text = NlGame::Resolve(g_Request.Watches[i], value) ? NlGame::Describe(value) : "\"kind\":\"missing\"";
			if (text == g_WatchLast[i])
				continue;
			g_WatchLast[i] = text;
			Trace("watch t=" + Fixed(Now, 1) + " " + g_Request.Watches[i] + " {" + text + "}", g_WatchLines, k_MaxWatchLines, "watch");
		}
	}

	// 인스턴스를 오브젝트별로 모은다. 인스턴스 수가 적은 오브젝트부터 본다. instance_number 와 instance_find 는
	// 자식 오브젝트의 인스턴스도 포함하므로(매뉴얼), 그래야 인스턴스가 부모가 아니라 자기 오브젝트의 이름으로 적힌다.
	std::vector<NlDump::InstanceRef> CollectInstances()
	{
		std::vector<std::pair<int, const NlGame::Object*>> order;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const auto it = g_Counts.find(object.Name);
			if (it != g_Counts.end() && it->second > 0)
				order.push_back({ it->second, &object });
		}
		std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::vector<NlDump::InstanceRef> refs;
		std::unordered_set<int64_t> seen;
		for (const auto& [count, object] : order)
		{
			int own = 0;
			for (int n = 0; n < count && n < k_MaxOwnedPerObject; n++)
			{
				RValue id;
				if (!NlGame::Call("instance_find", { RValue(object->Index), RValue(static_cast<double>(n)) }, id))
					continue;
				if (NlGame::IsNumber(id) && id.ToDouble() < 0)	// noone (-4)
					continue;
				if (!seen.insert(id.m_i64).second)
					continue;
				refs.push_back({ object->Name, own++, id });
			}
		}
		return refs;
	}

	// 오브젝트마다 인스턴스 수와, 첫 인스턴스의 변수를 한 단계 적는다.
	void WriteInstances(std::ostream& Out, const std::vector<NlDump::InstanceRef>& Refs)
	{
		Out << ",\"instances\":{";
		bool first = true;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const auto it = g_Counts.find(object.Name);
			const int count = it == g_Counts.end() ? 0 : it->second;
			if (count <= 0)
				continue;

			int own = 0;
			const NlDump::InstanceRef* sample = nullptr;
			for (const NlDump::InstanceRef& ref : Refs)
			{
				if (ref.Object != object.Name)
					continue;
				if (!sample)
					sample = &ref;
				own++;
			}

			Out << (first ? "" : ",") << "\n" << Quote(object.Name) << ":{\"count\":" << count << ",\"own\":" << own << ",\"members\":{";
			first = false;

			RValue names;
			if (sample && NlGame::Call("variable_instance_get_names", { sample->Id }, names) && names.IsArray())
			{
				bool first_member = true;
				for (const RValue& name : names.ToVector())
				{
					if (!name.IsString())
						continue;
					RValue value;
					if (!NlGame::Call("variable_instance_get", { sample->Id, name }, value))
						continue;
					Out << (first_member ? "" : ",") << "\n" << Quote(name.ToString()) << ":{" << NlGame::Describe(value) << "}";
					first_member = false;
				}
			}
			Out << "}}";
			Out.flush();
		}
		Out << "\n}";
	}

	// 덤프 하나를 쓴다. 덜 위험한 것부터 쓴다: 전역 목록, 전역 찾기, ds 찾기, 인스턴스, 스크립트 호출.
	// 구역이 끝날 때마다 파일을 비우고 로그에 적는다. 도중에 죽어도 어디까지 왔는지 남는다.
	bool Run(const std::string& Name, int Seq, double Now, bool WithScripts)
	{
		const std::string file = "NlToyBox.dump." + Name + ".json";
		std::ofstream out(g_Dir / file, std::ios::trunc | std::ios::binary);
		if (!out.is_open())
		{
			g_Log("dump failed: cannot open " + file);
			return false;
		}
		const std::string tag = "dump " + Name;
		g_Log(tag + " start seq " + std::to_string(Seq) + " t=" + Fixed(Now, 1) + " at " + Clock());

		out << "{\"module_dump\":2,\"module_version\":" << Quote(g_Version) << ",\"name\":" << Quote(Name)
			<< ",\"seq\":" << Seq << ",\"elapsed_seconds\":" << Number(Now) << ",\"room\":" << Quote(g_Room)
			<< ",\"delay_seconds\":" << g_Request.DelaySeconds << ",\"repeat_seconds\":" << Number(g_Request.RepeatSeconds)
			<< ",\"limits\":{\"max_depth\":" << g_Limits.MaxDepth << ",\"max_visited\":" << g_Limits.MaxVisited
			<< ",\"max_array\":" << Number(g_Limits.MaxArray) << ",\"max_hits\":" << g_Limits.MaxHits
			<< ",\"max_ds_keys\":" << Number(g_Limits.MaxDsKeys) << ",\"max_ds_id\":" << g_Limits.MaxDsId
			<< ",\"max_instances\":" << g_Limits.MaxInstances << "}";

		out << ",\"present\":{";
		for (size_t i = 0; i < g_Present.size(); i++)
			out << (i ? "," : "") << Quote(g_Present[i]) << ":" << g_Counts[g_Present[i]];
		out << "}";

		out << ",\"watch\":{";
		for (size_t i = 0; i < g_Request.Watches.size(); i++)
			out << (i ? "," : "") << Quote(g_Request.Watches[i]) << ":{" << g_WatchLast[i] << "}";
		out << "}";

		CInstance* global = NlGame::Global();
		out << ",\"global_status\":" << Quote(global ? "AURIE_SUCCESS" : "no global instance");

		out << ",\"globals\":{";
		if (global)
			NlGame::WriteMembers(out, RValue(global), true, true);
		out << "\n}";
		out.flush();
		g_Log(tag + ": globals");

		NlDump::Finder finder(out, g_Request, g_Limits);
		finder.FindGlobal(global);
		g_Log(tag + ": find_global visited " + std::to_string(finder.Visited()));

		if (Skipped("ds"))
			finder.Skip("find_ds");
		else
			finder.FindDataStructures();
		g_Log(tag + ": find_ds visited " + std::to_string(finder.Visited()));

		if (Skipped("instances"))
		{
			out << ",\"instances\":{}";
			finder.Skip("find_instances");
		}
		else
		{
			const std::vector<NlDump::InstanceRef> refs = CollectInstances();
			WriteInstances(out, refs);
			finder.FindInstances(refs);
		}
		g_Log(tag + ": find_instances visited " + std::to_string(finder.Visited()));

		// 스크립트 호출은 맨 뒤다. 게임 스크립트는 인자가 맞지 않으면 GML 오류로 게임을 끝낸다(단계 0 실측).
		out << ",\"scripts\":[";
		bool first = true;
		for (const NlCore::ScriptCall& call : g_Request.Scripts)
		{
			if (!WithScripts)
				break;

			std::vector<RValue> args;
			std::string args_json;
			for (const std::string& arg : call.Args)
			{
				double number = 0;
				const bool numeric = NlCore::ParseNumber(arg, number);
				args.push_back(numeric ? RValue(number) : RValue(std::string_view(arg)));
				args_json += std::string(args_json.empty() ? "" : ",") + (numeric ? Number(number) : Quote(arg));
			}

			out << (first ? "" : ",") << "\n{\"name\":" << Quote(call.Name) << ",\"args\":[" << args_json << "]";
			out.flush();	// 호출이 게임을 죽이면 어느 스크립트였는지 남는다
			first = false;

			if (!global)
			{
				out << ",\"status\":\"no global instance\"}";
				continue;
			}

			RValue result;
			const AurieStatus status = NlGame::Yytk()->CallGameScriptEx(result, call.Name, global, global, args);
			out << ",\"status\":" << Quote(AurieStatusToString(status));
			if (AurieSuccess(status))
				out << ",\"result\":{" << NlGame::Describe(result) << "}";
			out << "}";
			out.flush();
		}
		out << "\n]}\n";
		out.close();

		const double end = Elapsed();
		g_Log(tag + " done seq " + std::to_string(Seq) + " t=" + Fixed(end, 1) + " took " + Fixed(end - Now, 1) + " at " + Clock());
		return true;
	}

	void Step(double Now)
	{
		Sample(Now);

		const std::string name = g_Schedule.Due(Now);
		if (name.empty())
			return;

		const bool last = !g_Schedule.Repeats();	// 되풀이하지 않으면 이 덤프가 마지막이다
		const bool ok = Run(name, g_Schedule.Count() + 1, Now, last);
		g_Schedule.Finished(Elapsed());
		if (!ok)
		{
			g_Finished = true;
			return;
		}
		if (last)
		{
			g_Finished = true;
			g_Log("dump done");
		}
	}
}

void NlDump::Init(const fs::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log)
{
	const fs::path request_path = ModuleDir / "NlToyBox.probe.txt";
	std::ifstream in(request_path);
	if (!in.is_open())
		return;

	g_Log = std::move(Log);
	g_Dir = ModuleDir;
	g_Version = Version;
	g_Start = std::chrono::steady_clock::now();
	g_Request = NlCore::ParseRequest(in);
	in.close();

	// 요청은 한 번만 쓴다. 지워 두면 이 실행이 어떻게 끝나든 다음 실행에서 덤프가 돌지 않는다.
	std::error_code ec;
	fs::remove(request_path, ec);
	g_Log(ec ? "request not removed: " + ec.message() : std::string("request consumed"));

	if (!g_Request.Errors.empty())
	{
		for (const std::string& error : g_Request.Errors)
			g_Log("request error: " + error);
		g_Log("dump failed: bad request");
		return;
	}

	g_Schedule = NlCore::Schedule(g_Request.DelaySeconds, g_Request.RepeatSeconds, g_Request.KeepLast);
	g_WatchLast.assign(g_Request.Watches.size(), "");
	g_Log("dump requested: delay " + std::to_string(g_Request.DelaySeconds) + "s, repeat " + Fixed(g_Request.RepeatSeconds, 0)
		+ "s, keep " + std::to_string(g_Request.KeepLast)
		+ ", find " + std::to_string(g_Request.FindValues.size() + g_Request.FindNames.size())
		+ ", scripts " + std::to_string(g_Request.Scripts.size()));
	g_Active = true;
}

void NlDump::Tick(CCode* Code)
{
	if (!g_Active.load(std::memory_order_relaxed) || g_Finished || g_Busy)
		return;

	const double now = Elapsed();

	if (g_Request.TraceEvents && Code && g_SeenCode.insert(Code).second)
	{
		char name[160];
		if (SafeCodeName(Code, name, sizeof(name)))
			Trace("event t=" + Fixed(now, 1) + " " + name, g_EventLines, k_MaxEventLines, "event");
	}

	if (now - g_LastSample < k_SampleSeconds)
		return;
	g_LastSample = now;

	g_Busy = true;
	Step(now);
	g_Busy = false;
}
