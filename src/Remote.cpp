#include "Remote.hpp"

#include "Access.hpp"
#include "Cheats.hpp"
#include "Economy.hpp"
#include "Game.hpp"
#include "Menu.hpp"
#include "Recorder.hpp"
#include "Search.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/RemoteCommand.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <vector>

using namespace Aurie;
using namespace YYTK;
using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::RemoteCommand;
using NlCore::Shortest;
using Clock = std::chrono::steady_clock;

namespace
{
	std::filesystem::path g_Dir, g_Ask, g_Taken, g_Answer;
	std::string g_Version;
	std::function<void(const std::string&)> g_Log;
	Clock::time_point g_Start, g_NextPoll;
	std::ofstream g_Out;						// 지금 쓰는 답 파일. 요청 하나를 처리하는 동안만 열려 있다
	std::vector<NlSearch::Hit> g_Hits;			// 마지막 find 의 결과(refine 이 거른다). 글뿐이다

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 답 한 줄. 줄마다 내보낸다(명령이 게임을 끝내도 그 앞까지는 남는다).
	void Say(const std::string& Line)
	{
		g_Out << Line << '\n';
		g_Out.flush();
	}

	std::string RowText(const NlAccess::Row& Row)
	{
		return Row.Name + "  " + Row.Type + (Row.Text.empty() ? "" : "  " + Row.Text);
	}

	Holder AsOption(const RemoteCommand& C)
	{
		const auto it = C.Options.find("as");
		if (it == C.Options.end())
			return Holder::None;
		return it->second == "map" ? Holder::Map : it->second == "list" ? Holder::List : Holder::None;
	}

	void DoAsk(const RemoteCommand& C)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(C.Target), value, why))
		{
			Say("  : " + why);
			return;
		}
		const NlAccess::Row row = NlAccess::Describe({}, value);
		Say("  = " + row.Type + (row.Text.empty() ? "" : " " + row.Text));
	}

	void DoList(const RemoteCommand& C)
	{
		std::vector<NlAccess::Row> rows;
		size_t total = 0;
		std::string why;
		const size_t max = static_cast<size_t>(NlCore::OptionNumber(C, "max", 400));
		if (!NlAccess::List(NlCore::ParseAskPath(C.Target), AsOption(C), max, rows, total, why))
		{
			Say("  : " + why);
			return;
		}
		for (const NlAccess::Row& row : rows)
			Say("  " + RowText(row));
		Say("  (" + std::to_string(total) + " in all)");
	}

	// 자식들을 깊이 MaxDepth 까지. ref(다른 인스턴스)로는 넘어가지 않는다.
	void Tree(const AskPath& Path, Holder As, int Depth, int MaxDepth, size_t& Budget, const std::string& Indent)
	{
		std::vector<NlAccess::Row> rows;
		size_t total = 0;
		std::string why;
		if (!NlAccess::List(Path, As, 400, rows, total, why))
		{
			if (Depth == 0)
				Say("  : " + why);
			return;
		}
		for (const NlAccess::Row& row : rows)
		{
			if (Budget == 0)
			{
				Say(Indent + "(max reached)");
				return;
			}
			Budget--;
			Say(Indent + RowText(row));
			if (row.IsContainer && row.Type != "ref" && Depth + 1 < MaxDepth)
				Tree(NlCore::ChildPath(Path, row.Step), Holder::None, Depth + 1, MaxDepth, Budget, Indent + "  ");
		}
		if (total > rows.size())
			Say(Indent + "(" + std::to_string(total) + " in all)");
	}

	void DoFind(const RemoteCommand& C)
	{
		NlSearch::Spec spec;
		const auto name = C.Options.find("name");
		if (name != C.Options.end())
			spec.Name = name->second;
		spec.HasValue = C.Options.count("value") > 0;
		spec.Value = NlCore::OptionNumber(C, "value", 0);
		const auto in = C.Options.find("in");
		if (in != C.Options.end())
		{
			spec.Globals = in->second.find("global") != std::string::npos;
			spec.Instances = in->second.find("inst") != std::string::npos;
			spec.Ds = in->second.find("ds") != std::string::npos;
		}

		NlSearch::Result result = NlSearch::Run(spec);
		g_Hits = std::move(result.Hits);
		const size_t max = static_cast<size_t>(NlCore::OptionNumber(C, "max", 200));
		for (size_t i = 0; i < g_Hits.size() && i < max; i++)
			Say("  " + g_Hits[i].Path + "  " + g_Hits[i].Type + "  " + g_Hits[i].Text);
		Say("  (" + std::to_string(g_Hits.size()) + " hits, " + std::to_string(result.Visited) + " visited, " + NlCore::Fixed(result.Seconds, 2)
			+ "s, " + std::to_string(result.Skipped) + " skipped" + (result.Truncated ? ", truncated" : "") + ")");
	}

	void DoRefine(const RemoteCommand& C)
	{
		NlSearch::Refine(g_Hits, C.Number);
		for (const NlSearch::Hit& hit : g_Hits)
			Say("  " + hit.Path + "  " + hit.Type + "  " + hit.Text);
		Say("  (" + std::to_string(g_Hits.size()) + " left)");
	}

	// write 는 남기고 poke 는 되돌린다.
	void DoWrite(const RemoteCommand& C, bool Restore)
	{
		double old = 0, read = 0, back = 0;
		if (!NlAccess::ReadNumber(C.Target, old))
		{
			Say("  : not a readable number");
			return;
		}
		Say("  old " + Shortest(old) + ", writing " + Shortest(C.Number));		// 쓰다가 죽으면 여기까지 남는다
		std::string why, why_back;
		const bool stuck = NlAccess::WriteNumber(C.Target, C.Number, why);
		const bool have = NlAccess::ReadNumber(C.Target, read);
		std::string line = "  read " + (have ? Shortest(read) : std::string("?")) + (stuck ? " (stuck)" : " (not stuck: " + why + ")");
		if (Restore)
		{
			const bool restored = NlAccess::WriteNumber(C.Target, old, why_back);
			const bool have_back = NlAccess::ReadNumber(C.Target, back);
			line += "; restored " + (have_back ? Shortest(back) : std::string("?")) + (restored ? "" : " (restore failed: " + why_back + ")");
		}
		Say(line);
		Log("remote " + C.Verb + " " + C.Target + " = " + Shortest(C.Number) + (stuck ? "" : ": " + why));
	}

	bool BuildArgs(const std::vector<NlCore::RemoteArg>& Args, std::vector<RValue>& Out, std::string& Why)
	{
		for (const NlCore::RemoteArg& arg : Args)
		{
			switch (arg.Kind)
			{
			case 'n': Out.push_back(RValue(arg.Number)); break;
			case 's': Out.push_back(RValue(std::string_view(arg.Text))); break;
			case 'b': Out.push_back(RValue(arg.Number != 0)); break;
			case 'u': Out.push_back(RValue()); break;
			case 'p':
			{
				RValue value;
				if (!NlAccess::Read(NlCore::ParseAskPath(arg.Text), value, Why))
					return false;
				Out.push_back(value);
				break;
			}
			default:
				Why = "unknown argument kind";
				return false;
			}
		}
		return true;
	}

	void SayResult(const RValue& Result)
	{
		const NlAccess::Row row = NlAccess::Describe({}, Result);
		Say("  -> " + row.Type + (row.Text.empty() ? "" : " " + row.Text));
	}

	// 게임 스크립트를 부른다. 인자의 수와 형은 기록으로 확인한 것만 쓴다(부르는 쪽의 책임이다).
	void DoCall(const RemoteCommand& C)
	{
		std::vector<RValue> args;		// 이 함수 안에서만 든다
		std::string why;
		CInstance* global = NlGame::Global();
		if (!global || !BuildArgs(C.Args, args, why))
		{
			Say("  : " + (global ? why : std::string("no global instance")));
			return;
		}
		// 정식 이름으로만 부른다. 접두 없는 이름은 다른 루틴을 가리킨다(research/07).
		const std::string name = NlCore::ScriptRoutineName(C.Target);
		int index = -1;
		if (!name.empty())
			NlGame::Yytk()->GetNamedRoutineIndex(name.c_str(), &index);
		if (index < 100000 || index >= 500000)
		{
			Say("  : no such script: " + C.Target);
			return;
		}

		Say("  calling " + name + " with " + std::to_string(args.size()) + " arguments");		// 죽으면 여기까지 남는다
		Log("remote call " + name + " (" + std::to_string(args.size()) + " arguments)");
		RValue result;
		const AurieStatus status = NlGame::Yytk()->CallGameScriptEx(result, name, global, global, args);
		if (AurieSuccess(status))
			SayResult(result);
		else
			Say(std::string("  : ") + AurieStatusToString(status));
	}

	// 메서드를 부른다. 묶인 곳이 없는 메서드(생성자의 정적 메서드)는 주소의 부모에 묶어 부른다(NlAccess::CallMethod).
	void DoMethod(const RemoteCommand& C)
	{
		RValue result;
		std::vector<RValue> args;
		std::string why;
		const AskPath path = NlCore::ParseAskPath(C.Target);
		NlAccess::MethodInfo info;
		if (!NlAccess::AboutMethod(path, info, why) || !BuildArgs(C.Args, args, why))
		{
			Say("  : " + why);
			return;
		}
		// 무엇을 어떤 self 로 부르는지 부르기 전에 남긴다(죽으면 여기까지 남는다).
		const std::string how = info.How == NlCore::Binding::AsIs ? "self as bound" : info.How == NlCore::Binding::ToOwner ? "self bound to the owner" : "refused";
		Say("  calling " + (info.Script.empty() ? std::string("the method") : info.Script) + " (" + how + ") with "
			+ std::to_string(args.size()) + " arguments");
		Log("remote method " + C.Target + " (" + std::to_string(args.size()) + " arguments, " + how + ")");
		if (NlAccess::CallMethod(path, args, result, why))
			SayResult(result);
		else
			Say("  : " + why);
	}

	// 값이 무엇인지. 메서드이면 그것이 묶인 스크립트의 이름과 묶인 곳이 있는지도 적는다. 부르지 않는다.
	void DoAbout(const RemoteCommand& C)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		const AskPath path = NlCore::ParseAskPath(C.Target);
		if (!NlAccess::Read(path, value, why))
		{
			Say("  : " + why);
			return;
		}
		const NlAccess::Row row = NlAccess::Describe({}, value);
		Say("  = " + row.Type + (row.Text.empty() ? "" : " " + row.Text));
		// 구조체를 만든 생성자의 이름(instanceof. 매뉴얼). 생성자로 만든 것이 아니면 "struct" 따위가 나온다.
		RValue made_by;
		if (value.IsStruct() && NlGame::Call("instanceof", { value }, made_by) && made_by.IsString())
			Say("  instanceof " + made_by.ToString());
		NlAccess::MethodInfo info;
		if (NlAccess::AboutMethod(path, info, why))
		{
			Say("  script " + (info.Script.empty() ? std::string("?") : info.Script));
			Say(std::string("  self ") + (info.Bound ? "bound" : "unbound"));
			Say(std::string("  method would ") + (info.How == NlCore::Binding::AsIs ? "call it as bound"
				: info.How == NlCore::Binding::ToOwner ? "bind it to the owner (the parent of the path) and call"
				: "refuse: unbound, and the owner is not a struct or an instance"));
		}
	}

	// 값의 자식들을 깊이 MaxDepth 까지(주소 없이 값에서 바로). ref(다른 인스턴스)로는 넘어가지 않는다.
	void TreeValue(const RValue& Value, int Depth, int MaxDepth, size_t& Budget, const std::string& Indent)
	{
		const Holder kind = NlAccess::Classify(Value);
		if (kind != Holder::Struct && kind != Holder::Array)
			return;
		NlAccess::ForEachChild(Value, kind, [&](const NlCore::PathStep& step, const RValue& child) {
			if (Budget == 0)
				return false;
			Budget--;
			const NlAccess::Row row = NlAccess::Describe(step, child);
			Say(Indent + RowText(row));
			if (row.IsContainer && row.Type != "ref" && Depth + 1 < MaxDepth)
				TreeValue(child, Depth + 1, MaxDepth, Budget, Indent + "  ");
			return true;
		});
	}

	// 인자 없는 스크립트를 부르고 돌려준 값을 늘어놓는다. 돌려준 값은 이 틱 안에서만 든다.
	void DoTreeCall(const RemoteCommand& C)
	{
		const std::string name = NlCore::ScriptRoutineName(C.Target);
		Say("  calling " + name + " with 0 arguments");		// 죽으면 여기까지 남는다
		Log("remote treecall " + name);
		RValue result;
		if (!NlGame::CallScript(name, {}, result))
		{
			Say("  : no such script: " + C.Target);
			return;
		}
		SayResult(result);
		// 자기를 가리키는 구조체가 있어도 깊이와 수로 멈춘다(되부름이 스택을 넘기지 않게 깊이에 상한을 둔다).
		size_t budget = static_cast<size_t>(std::clamp(NlCore::OptionNumber(C, "max", 300), 1.0, 5000.0));
		TreeValue(result, 0, static_cast<int>(std::clamp(NlCore::OptionNumber(C, "depth", 2), 1.0, 8.0)), budget, "    ");
		if (budget == 0)
			Say("    (max reached)");
	}

	// 구조체의 정적 메서드들의 이름. list 에는 구조체 자신의 변수만 나온다. 정적 구조체는 static_get 으로 얻고,
	// 정적 구조체의 static_get 은 부모 생성자의 정적 구조체다. 뿌리의 것은 undefined 다(매뉴얼).
	void DoStatics(const RemoteCommand& C)
	{
		RValue node;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(C.Target), node, why))
		{
			Say("  : " + why);
			return;
		}
		// static_get 과 instanceof 는 구조체(메서드 포함)에만 부른다. 그 밖의 값에 대한 동작은 매뉴얼에 없다.
		if (!node.IsStruct())
		{
			Say("  : not a struct");
			return;
		}
		RValue made_by;
		if (NlGame::Call("instanceof", { node }, made_by) && made_by.IsString())
			Say("  instanceof " + made_by.ToString());

		size_t budget = static_cast<size_t>(std::clamp(NlCore::OptionNumber(C, "max", 400), 1.0, 5000.0));
		for (int level = 0; level < 8; level++)
		{
			RValue next;
			if (!NlGame::Call("static_get", { node }, next) || !next.IsStruct())
				break;
			Say("  # static " + std::to_string(level));
			bool cut = false;
			const double count = NlAccess::ForEachChild(next, Holder::Struct, [&](const NlCore::PathStep& step, const RValue& child) {
				if (budget == 0)
				{
					cut = true;
					return false;
				}
				budget--;
				Say("    " + RowText(NlAccess::Describe(step, child)));
				return true;
			});
			if (cut)
				Say("    (max reached)");
			else if (count <= 0)
				Say("    (nothing)");
			node = next;
		}
	}

	// 함수가 돌려주는 값을 바꾼다(NlRecorder::Override).
	void DoOverride(const RemoteCommand& C)
	{
		NlRecorder::Forced value;
		value.Kind = C.Args.empty() ? 'u' : C.Args[0].Kind;
		value.Number = C.Args.empty() ? 0 : C.Args[0].Number;
		value.Skip = C.Options.count("skip") > 0;
		value.Whole = C.Options.count("whole") > 0;
		std::string name, why;
		if (NlRecorder::Override(C.Target, value, name, why))
			Say("  overriding " + name);
		else
			Say("  : " + why);
	}

	// 경제 패널과 같은 길로 금화·자원을 바꾼다(NlEconomy::Do).
	void DoEconomy(const RemoteCommand& C)
	{
		NlCore::EconomyCommand command;
		if (!NlCore::ParseEconomyAct(C.Target, command.Act))
		{
			Say("  : unknown economy command");
			return;
		}
		command.Amount = C.Number;
		command.Resource = static_cast<int>(NlCore::OptionNumber(C, "resource", -1));
		Say("  running economy " + C.Target + " " + Shortest(C.Number));		// 죽으면 여기까지 남는다
		Say("  " + NlEconomy::Do(command));
	}

	void DoState()
	{
		double game_time = 0, warp = 0;
		const bool have_time = NlAccess::ReadNumber("inst:o_time_controller.__game_time", game_time);
		NlAccess::ReadNumber("inst:o_time_controller.time_warp", warp);
		Say("  module " + g_Version + ", up " + NlCore::Fixed(std::chrono::duration<double>(Clock::now() - g_Start).count(), 1) + "s");
		Say(std::string("  in_game ") + (NlAccess::InGame() ? "1" : "0"));
		Say("  game_time " + (have_time ? Shortest(game_time) : std::string("?")) + ", time_warp " + Shortest(warp));
		for (const char* object : { "o_main_menu", "o_debug", "o_province_controller", "o_character", "o_dummy", "o_building" })
			Say(std::string("  ") + object + " " + std::to_string(NlAccess::InstanceCount(object)));
	}

	void Execute(const RemoteCommand& C)
	{
		if (!C.Error.empty())
		{
			Say("  : " + C.Error);
			return;
		}
		if (C.Verb == "ask")
			DoAsk(C);
		else if (C.Verb == "about")
			DoAbout(C);
		else if (C.Verb == "economy")
			DoEconomy(C);
		else if (C.Verb == "cheat")
		{
			const bool ok = C.Args.empty() ? NlCheats::Set(C.Target, C.Number != 0) : NlCheats::SetNumber(C.Target, C.Args[0].Number);
			Say(ok ? "  cheat " + C.Target + (C.Args.empty() ? (C.Number != 0 ? " on" : " off") : " = " + Shortest(C.Args[0].Number))
				: "  : cannot set " + C.Target + (C.Args.empty() ? " (it needs a number)" : " (it takes no number)"));
		}
		else if (C.Verb == "page")
			Say(NlMenu::SetPage(C.Target) ? "  page " + C.Target : "  : unknown page");
		else if (C.Verb == "list")
			DoList(C);
		else if (C.Verb == "tree")
		{
			size_t budget = static_cast<size_t>(NlCore::OptionNumber(C, "max", 300));
			Tree(NlCore::ParseAskPath(C.Target), AsOption(C), 0, static_cast<int>(NlCore::OptionNumber(C, "depth", 2)), budget, "  ");
		}
		else if (C.Verb == "find")
			DoFind(C);
		else if (C.Verb == "refine")
			DoRefine(C);
		else if (C.Verb == "write" || C.Verb == "poke")
			DoWrite(C, C.Verb == "poke");
		else if (C.Verb == "record")
		{
			std::string name, why;
			if (NlRecorder::Watch(C.Target, name, why))
				Say("  recording " + name);
			else
				Say("  : " + why);
		}
		else if (C.Verb == "statics")
			DoStatics(C);
		else if (C.Verb == "treecall")
			DoTreeCall(C);
		else if (C.Verb == "override")
			DoOverride(C);
		else if (C.Verb == "unoverride")
		{
			const int stopped = NlRecorder::Unoverride(C.Target);
			Say(stopped > 0 ? "  stopped " + std::to_string(stopped) : "  : nothing is overridden under that name");
		}
		else if (C.Verb == "unrecord")
			Say("  stopped " + std::to_string(NlRecorder::Unwatch(C.Target)));
		else if (C.Verb == "records")
		{
			const std::string report = NlRecorder::Report(C.Target);
			Say(report.empty() ? "  (nothing recorded)" : report.substr(0, report.size() - 1));		// 끝의 줄바꿈은 Say 가 붙인다
		}
		else if (C.Verb == "call")
			DoCall(C);
		else if (C.Verb == "method")
			DoMethod(C);
		else if (C.Verb == "state")
			DoState();
		else if (C.Verb == "shot")
		{
			NlUi::RequestShot(g_Dir / ("NlToyBox.shot." + C.Target + ".bmp"));
			Say("  shot requested: NlToyBox.shot." + C.Target + ".bmp");
		}
		else if (C.Verb == "window")
		{
			NlUi::SetVisible(C.Target == "open");
			Say("  window " + C.Target);
		}
	}
}

void NlRemote::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log_)
{
	g_Dir = ModuleDir;
	g_Ask = ModuleDir / "NlToyBox.ask.txt";
	g_Taken = ModuleDir / "NlToyBox.ask.txt.taken";
	g_Answer = ModuleDir / "NlToyBox.answer.txt";
	// 앞 실행이 남긴 요청은 실행하지 않는다(죽은 도구가 남긴 호출이 평소 플레이의 첫 틱에 불리지 않게).
	NlCore::DropStaleRemoteRequest(g_Ask, g_Taken);
	g_Version = Version;
	g_Log = std::move(Log_);
	g_Start = Clock::now();
	g_NextPoll = g_Start;
}

void NlRemote::GameTick()
{
	const Clock::time_point now = Clock::now();
	if (now < g_NextPoll)
		return;
	g_NextPoll = now + std::chrono::milliseconds(250);

	std::error_code ec;
	if (!std::filesystem::exists(g_Ask, ec))
		return;

	// 도구는 임시 파일에 쓴 뒤 이름을 바꿔 놓는다. 여기서 보이면 다 쓰인 것이다.
	// 이름을 바꿔 집은 것만 실행한다: 읽고 나서 지우지 못하면 같은 호출이 0.25초마다 되풀이된다.
	std::vector<std::string> lines;
	if (!NlCore::TakeRemoteRequest(g_Ask, g_Taken, lines))
		return;

	std::string id = "-";
	g_Out.open(g_Answer, std::ios::app);
	for (const std::string& line : lines)
	{
		const std::string text = NlCore::Trim(line);
		if (text.rfind("id ", 0) == 0)
		{
			id = NlCore::Trim(text.substr(3));
			Say("# " + id);
			continue;
		}
		const RemoteCommand command = NlCore::ParseRemoteLine(text);
		if (command.Verb.empty())
			continue;
		Say("> " + text);
		Execute(command);
	}
	Say("# done " + id);
	g_Out.close();
	Log("remote request " + id + ": " + std::to_string(lines.size()) + " lines");
}
