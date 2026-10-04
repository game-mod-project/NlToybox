#include "Dump.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unordered_set>
#include <vector>

using namespace Aurie;
using namespace YYTK;

namespace
{
	struct ScriptCall
	{
		std::string Name;
		std::vector<std::string> Args;
	};

	constexpr size_t k_MaxString = 200;		// 문자열 값은 이 바이트에서 자른다
	constexpr int k_MaxDepth = 6;			// 찾기가 내려가는 깊이
	constexpr size_t k_MaxVisited = 400000;	// 찾기가 방문하는 값의 수
	constexpr double k_MaxArray = 2048;		// 이보다 긴 배열은 들어가지 않는다
	constexpr size_t k_MaxHits = 300;

	YYTKInterface* g_Yytk = nullptr;
	std::function<void(const std::string&)> g_Log;
	fs::path g_DumpPath;
	std::vector<ScriptCall> g_Scripts;
	std::vector<double> g_FindValues;
	std::vector<std::string> g_FindNames;
	int g_DelaySeconds = 60;
	std::chrono::steady_clock::time_point g_Start;
	std::atomic<bool> g_Active = false;
	std::atomic<bool> g_Done = false;

	std::string Trim(const std::string& Text)
	{
		const size_t begin = Text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
			return "";
		return Text.substr(begin, Text.find_last_not_of(" \t\r\n") - begin + 1);
	}

	// JSON 문자열로 쓴다. 길면 UTF-8 글자 경계에서 자르고, 잘못된 바이트는 '?' 로 바꾼다.
	std::string Quote(std::string Text)
	{
		if (Text.size() > k_MaxString)
		{
			size_t cut = k_MaxString;
			while (cut > 0 && (static_cast<unsigned char>(Text[cut]) & 0xC0) == 0x80)
				cut--;
			Text.resize(cut);
			Text += "...";
		}

		std::string out = "\"";
		for (size_t i = 0; i < Text.size();)
		{
			const unsigned char c = static_cast<unsigned char>(Text[i]);
			const size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
			bool valid = len != 0 && i + len <= Text.size();
			for (size_t k = 1; valid && k < len; k++)
				valid = (static_cast<unsigned char>(Text[i + k]) & 0xC0) == 0x80;

			if (!valid) { out += '?'; i++; continue; }
			if (len > 1) { out.append(Text, i, len); i += len; continue; }

			switch (c)
			{
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (c < 0x20) { char buf[8]; snprintf(buf, sizeof(buf), "\\u%04x", c); out += buf; }
				else out += static_cast<char>(c);
			}
			i++;
		}
		return out + "\"";
	}

	std::string Number(double Value)
	{
		if (!std::isfinite(Value))
			return Value != Value ? "\"nan\"" : (Value > 0 ? "\"inf\"" : "\"-inf\"");
		char buf[40];
		snprintf(buf, sizeof(buf), "%.17g", Value);
		return buf;
	}

	// 배열 길이. 못 얻으면 음수.
	double ArrayLength(const RValue& Value, CInstance* Global)
	{
		RValue length;
		const AurieStatus status = g_Yytk->CallBuiltinEx(length, "array_length", Global, Global, { Value });
		return AurieSuccess(status) ? length.ToDouble() : -1;
	}

	// 값 하나를 "kind":…[,"value":…] 로 적는다(중괄호 없이). 구조체와 배열의 속은 보지 않는다.
	std::string Describe(const RValue& Value, CInstance* Global)
	{
		if (Value.IsStruct())
			return "\"kind\":\"struct\"";
		if (Value.IsArray())
			return "\"kind\":\"array\",\"length\":" + Number(ArrayLength(Value, Global));
		if (Value.IsString())
			return "\"kind\":\"string\",\"value\":" + Quote(Value.ToString());
		if (Value.IsNumberConvertible())
			return "\"kind\":" + Quote(Value.GetKindName()) + ",\"value\":" + Number(Value.ToDouble());
		return "\"kind\":" + Quote(Value.GetKindName());
	}

	// 구조체의 멤버를 "이름":{…},… 로 적는다. Expand 면 구조체 멤버를 한 단계 더 편다.
	void WriteMembers(std::ostream& Out, const RValue& Object, CInstance* Global, bool Expand, bool FlushEach)
	{
		bool first = true;
		g_Yytk->EnumInstanceMembers(Object, [&](const char* Name, RValue* Value) -> bool
		{
			Out << (first ? "" : ",") << "\n" << Quote(Name ? Name : "") << ":{";
			first = false;

			if (!Value)
				Out << "\"kind\":\"error\"";
			else if (Expand && Value->IsStruct())
			{
				Out << "\"kind\":\"struct\",\"members\":{";
				WriteMembers(Out, *Value, Global, false, false);
				Out << "}";
			}
			else
				Out << Describe(*Value, Global);

			Out << "}";
			if (FlushEach)
				Out.flush();	// 도중에 죽어도 어디까지 왔는지 남는다
			return false;		// 거짓을 돌려줘야 다음 멤버로 넘어간다
		});
	}

	struct Finder
	{
		std::ostream& Out;
		CInstance* Global;
		std::unordered_set<const void*> Seen;
		size_t Visited = 0;
		size_t Hits = 0;
		bool Truncated = false;

		void Hit(const std::string& Path, const char* Why, const RValue& Value)
		{
			if (Hits >= k_MaxHits) { Truncated = true; return; }
			Out << (Hits == 0 ? "" : ",") << "\n{\"path\":" << Quote(Path) << ",\"why\":\"" << Why << "\"," << Describe(Value, Global);
			if (Value.IsStruct())
			{
				Out << ",\"members\":{";
				WriteMembers(Out, Value, Global, false, false);
				Out << "}";
			}
			Out << "}";
			Out.flush();
			Hits++;
		}

		void Walk(const RValue& Value, const std::string& Path, const std::string& Name, int Depth)
		{
			if (Truncated)
				return;
			if (++Visited > k_MaxVisited) { Truncated = true; return; }

			for (const std::string& wanted : g_FindNames)
				if (Name == wanted)
					Hit(Path, "name", Value);

			if (Value.IsStruct())
			{
				const void* identity = Value.ToObject();
				if (Depth >= k_MaxDepth || !identity || !Seen.insert(identity).second)
					return;
				g_Yytk->EnumInstanceMembers(Value, [&](const char* MemberName, RValue* Member) -> bool
				{
					const std::string name = MemberName ? MemberName : "";
					if (Member)
						Walk(*Member, Path + "." + name, name, Depth + 1);
					return false;
				});
				return;
			}

			if (Value.IsArray())
			{
				const double length = ArrayLength(Value, Global);
				if (Depth >= k_MaxDepth || length <= 0 || length > k_MaxArray)
					return;
				const std::vector<RValue> items = Value.ToVector();
				for (size_t i = 0; i < items.size(); i++)
					Walk(items[i], Path + "[" + std::to_string(i) + "]", "", Depth + 1);
				return;
			}

			if (Value.IsString() || !Value.IsNumberConvertible())
				return;
			const double number = Value.ToDouble();
			for (const double wanted : g_FindValues)
				if (number == wanted)
					Hit(Path, "value", Value);
		}
	};

	void Run()
	{
		const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_Start).count();

		std::ofstream out(g_DumpPath, std::ios::trunc | std::ios::binary);
		out << "{\"module_dump\":1,\"delay_seconds\":" << g_DelaySeconds << ",\"elapsed_seconds\":" << Number(elapsed);

		CInstance* global_instance = nullptr;
		const AurieStatus global_status = g_Yytk->GetGlobalInstance(&global_instance);
		const bool have_global = AurieSuccess(global_status) && global_instance;
		out << ",\"global_status\":" << Quote(AurieStatusToString(global_status));

		out << ",\"globals\":{";
		if (have_global)
			WriteMembers(out, RValue(global_instance), global_instance, true, true);
		out << "\n}";

		out << ",\"scripts\":[";
		bool first = true;
		for (const ScriptCall& call : g_Scripts)
		{
			std::vector<RValue> args;
			std::string args_json;
			for (const std::string& arg : call.Args)
			{
				char* end = nullptr;
				const double number = std::strtod(arg.c_str(), &end);
				const bool numeric = !arg.empty() && end && *end == '\0';
				args.push_back(numeric ? RValue(number) : RValue(std::string_view(arg)));
				args_json += std::string(args_json.empty() ? "" : ",") + (numeric ? Number(number) : Quote(arg));
			}

			out << (first ? "" : ",") << "\n{\"name\":" << Quote(call.Name) << ",\"args\":[" << args_json << "]";
			out.flush();	// 호출이 게임을 죽이면 어느 스크립트였는지 남는다
			first = false;

			if (!have_global)
			{
				out << ",\"status\":\"no global instance\"}";
				continue;
			}

			RValue result;
			const AurieStatus status = g_Yytk->CallGameScriptEx(result, call.Name, global_instance, global_instance, args);
			out << ",\"status\":" << Quote(AurieStatusToString(status));
			if (AurieSuccess(status))
				out << ",\"result\":{" << Describe(result, global_instance) << "}";
			out << "}";
			out.flush();
		}
		out << "\n]";

		out << ",\"find\":{\"hits\":[";
		Finder finder{ out, global_instance };
		if (have_global && (!g_FindValues.empty() || !g_FindNames.empty()))
			finder.Walk(RValue(global_instance), "global", "", 0);
		out << "\n],\"visited\":" << finder.Visited << ",\"truncated\":" << (finder.Truncated ? "true" : "false") << "}";

		out << "}\n";
		out.close();

		g_Log("dump done");
	}
}

void NlDump::Init(YYTKInterface* Yytk, const fs::path& ModuleDir, std::function<void(const std::string&)> Log)
{
	std::ifstream in(ModuleDir / "NlToyBox.probe.txt");
	if (!in.is_open())
		return;

	g_Yytk = Yytk;
	g_Log = std::move(Log);
	g_DumpPath = ModuleDir / "NlToyBox.dump.json";
	g_Start = std::chrono::steady_clock::now();

	std::string line;
	while (std::getline(in, line))
	{
		line = Trim(line);
		const size_t eq = line.find('=');
		if (line.empty() || line[0] == '#' || eq == std::string::npos)
			continue;

		const std::string key = Trim(line.substr(0, eq));
		const std::string value = Trim(line.substr(eq + 1));

		if (key == "delay_seconds")
		{
			const int parsed = std::atoi(value.c_str());
			if (parsed >= 0)
				g_DelaySeconds = parsed;
		}
		else if (key == "find")
			g_FindValues.push_back(std::strtod(value.c_str(), nullptr));
		else if (key == "find_name" && !value.empty())
			g_FindNames.push_back(value);
		else if (key == "script")
		{
			// 이름|인자|인자…
			ScriptCall call;
			size_t begin = 0;
			while (true)
			{
				const size_t bar = value.find('|', begin);
				const std::string part = Trim(value.substr(begin, bar == std::string::npos ? std::string::npos : bar - begin));
				if (begin == 0)
					call.Name = part;
				else
					call.Args.push_back(part);
				if (bar == std::string::npos)
					break;
				begin = bar + 1;
			}
			if (!call.Name.empty())
				g_Scripts.push_back(std::move(call));
		}
	}

	g_Log("dump requested: delay " + std::to_string(g_DelaySeconds) + "s, scripts " + std::to_string(g_Scripts.size())
		+ ", find " + std::to_string(g_FindValues.size() + g_FindNames.size()));
	g_Active = true;
}

void NlDump::Tick()
{
	if (!g_Active.load(std::memory_order_relaxed) || g_Done.load(std::memory_order_relaxed))
		return;
	if (std::chrono::steady_clock::now() - g_Start < std::chrono::seconds(g_DelaySeconds))
		return;
	if (g_Done.exchange(true))
		return;

	Run();
}
