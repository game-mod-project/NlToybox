#include "Explorer.hpp"

#include "Access.hpp"
#include "Search.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>

using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::Shortest;

namespace
{
	constexpr size_t k_MaxRows = 8000;		// 한 그릇에서 적는 자식의 수. 전역이 4,700여 개다(research/02)
	constexpr size_t k_LiveRows = 2000;		// 이보다 많으면 "새로 고침"을 누를 때만 다시 읽는다

	struct Watch			// 즐겨찾기 하나. 잠금 값이 있을 수 있다
	{
		std::string Path;
		bool HasLock = false;
		bool Locked = false;			// 지금 걸려 있는가
		double LockValue = 0;
		bool HasOwner = false;			// 잠글 때 주소가 가리키던 인스턴스를 적어 두었다(inst: 주소만)
		int64_t Owner = 0;
		// 아래는 GameTick 이 채우는 스냅샷
		bool Found = false;
		NlAccess::Row Row;
		std::string Note;
	};

	struct Command			// 창에서 한 일. 다음 틱에 실행한다
	{
		enum class Kind { Navigate, Refresh, WriteNumber, WriteString, Pin, Unpin, SetLock, Search, Refine };
		Kind What;
		std::string Path, Text;
		double Number = 0;
		bool Flag = false;
		Holder As = Holder::None;
		NlSearch::Spec Spec;
	};

	std::recursive_mutex g_Mutex;		// 아래 전부를 지킨다(지금은 그리는 스레드와 게임 스레드가 같다. research/05)
	NlExplorer::LogFn g_Log;
	std::vector<Command> g_Queue;

	// 훑기
	std::string g_Address;				// 지금 보는 그릇. 비어 있으면 뿌리 화면
	Holder g_As = Holder::None;			// 수를 ds 로 여는 방식
	std::vector<NlAccess::Row> g_Rows;
	size_t g_Total = 0;
	int g_Instances = 0;				// 주소가 inst: 일 때 그 오브젝트의 인스턴스 수
	std::string g_Error, g_Note;
	std::vector<NlAccess::RootObject> g_Roots;
	bool g_Stale = true;				// 다음 틱에 다시 읽는다
	bool g_FocusBrowse = false;			// 다음에 그릴 때 "훑기" 탭을 앞에 낸다
	bool g_Trace = false;				// 주소를 옮겼다. 처음 늘어놓을 때 로그에 적는다
	double g_NextView = 0, g_NextWatch = 0, g_NextLock = 0;

	// 즐겨찾기·잠금
	std::vector<Watch> g_Watches;
	bool g_Changed = false;

	// 찾기
	NlSearch::Result g_Found;
	bool g_Searched = false;
	std::string g_SearchNote;

	// 입력 칸
	char g_AddressBox[512] = "", g_Filter[64] = "", g_AddPin[512] = "", g_SearchName[64] = "", g_SearchValue[32] = "";
	bool g_ScopeGlobals = true, g_ScopeInstances = true, g_ScopeDs = false;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	void Push(Command Cmd)
	{
		g_Queue.push_back(std::move(Cmd));
	}

	void PushPath(Command::Kind What, const std::string& Path, double Number = 0, bool Flag = false)
	{
		Command command{ What };
		command.Path = Path;
		command.Number = Number;
		command.Flag = Flag;
		Push(std::move(command));
	}

	void Go(const std::string& Address, Holder As = Holder::None)
	{
		Command command{ Command::Kind::Navigate };
		command.Path = Address;
		command.As = As;
		Push(std::move(command));
		g_FocusBrowse = true;
	}

	// 단계를 뗄 때, 뗀 단계가 ds 의 것이면 부모(수)를 그 ds 로 연다.
	Holder AsFor(char StepKind)
	{
		return StepKind == '@' ? Holder::Map : StepKind == '#' ? Holder::List : Holder::None;
	}

	// 그 값이 든 그릇으로 간다.
	void GoToParent(const std::string& Path)
	{
		const AskPath path = NlCore::ParseAskPath(Path);
		if (!path.Error.empty() || path.Steps.empty())
			Go(Path);
		else
			Go(NlCore::FormatAskPath(NlCore::ParentPath(path)), AsFor(path.Steps.back().Kind));
	}

	Watch* FindWatch(const std::string& Path)
	{
		for (Watch& watch : g_Watches)
			if (watch.Path == Path)
				return &watch;
		return nullptr;
	}

	Watch& NeedWatch(const std::string& Path)
	{
		if (Watch* watch = FindWatch(Path))
			return *watch;
		Watch watch;
		watch.Path = Path;
		g_Watches.push_back(std::move(watch));
		g_Changed = true;
		return g_Watches.back();
	}

	// ---- 게임 스레드 ----

	void LoadView()
	{
		g_Rows.clear();
		g_Total = 0;
		g_Instances = 0;
		g_Error.clear();
		if (g_Address.empty())
		{
			g_Roots = NlAccess::LiveObjects();
			return;
		}

		const AskPath path = NlCore::ParseAskPath(g_Address);
		if (!path.Error.empty())
		{
			g_Error = path.Error;
			return;
		}
		if (path.Root == "inst")
			g_Instances = NlAccess::InstanceCount(path.Object);

		// 주소를 옮긴 뒤 처음 늘어놓을 때 앞뒤로 한 줄씩 적는다. 늘어놓다가 게임이 끝나면 어디였는지 남는다.
		bool trace = false;
		if (g_Trace)
		{
			YYTK::RValue value;		// 이 함수 안에서만 든다
			Holder kind = Holder::None;
			std::string ignored;
			trace = NlAccess::Open(path, value, kind, ignored);
			if (trace)
				Log("explorer view " + g_Address + ": listing");
		}

		std::string why;
		if (!NlAccess::List(path, g_As, k_MaxRows, g_Rows, g_Total, why))
			g_Error = why;
		if (trace)
		{
			g_Trace = false;
			Log("explorer view " + g_Address + ": " + (g_Error.empty() ? std::to_string(g_Total) + " children" : g_Error));
		}
	}

	void Snapshot(Watch& W)
	{
		YYTK::RValue value;		// 이 함수 안에서만 든다
		std::string why;
		W.Found = NlAccess::Read(NlCore::ParseAskPath(W.Path), value, why);
		W.Row = W.Found ? NlAccess::Describe({}, value) : NlAccess::Row{};
	}

	void Hold(Watch& W)
	{
		// 잠근 뒤에 같은 주소가 다른 인스턴스를 가리키게 됐으면(앞의 인스턴스가 사라졌다) 잠금을 푼다.
		// 그대로 두면 다른 인물의 같은 변수에 0.1초마다 써 넣게 된다.
		int64_t owner = 0;
		if (W.HasOwner && (!NlAccess::InstanceIdentity(NlCore::ParseAskPath(W.Path), owner) || owner != W.Owner))
		{
			W.Locked = false;
			W.HasOwner = false;
			W.Note = "인스턴스가 바뀌어 잠금을 풀었습니다";
			Log("explorer lock " + W.Path + " released: the instance changed");
			return;
		}

		double now = 0;
		if (!NlAccess::ReadNumber(W.Path, now))
		{
			W.Note = "대상 없음";
			return;
		}
		std::string why;
		if (now == W.LockValue || NlAccess::WriteNumber(W.Path, W.LockValue, why))
			W.Note = "잠금";
		else
			W.Note = "써지지 않음: " + why;
	}

	void Run(const Command& C)
	{
		std::string why;
		switch (C.What)
		{
		case Command::Kind::Navigate:
			g_Address = NlCore::Trim(C.Path);
			g_As = C.As;
			g_Rows.clear();
			g_Total = 0;
			g_Error.clear();
			g_Note.clear();
			g_Filter[0] = 0;
			std::snprintf(g_AddressBox, sizeof(g_AddressBox), "%s", g_Address.c_str());
			g_Stale = true;
			g_Trace = true;
			break;

		case Command::Kind::Refresh:
			g_Stale = true;
			break;

		case Command::Kind::WriteNumber:
		{
			const bool ok = NlAccess::WriteNumber(C.Path, C.Number, why);
			g_Note = C.Path + " = " + Shortest(C.Number) + (ok ? "  써 넣음" : "  써지지 않음: " + why);
			Log("explorer write " + C.Path + " = " + Shortest(C.Number) + (ok ? ": ok" : ": " + why));
			g_Stale = true;
			break;
		}

		case Command::Kind::WriteString:
		{
			const bool ok = NlAccess::WriteString(C.Path, C.Text, why);
			g_Note = C.Path + (ok ? "  써 넣음" : "  써지지 않음: " + why);
			Log("explorer write " + C.Path + " = " + NlCore::Quote(C.Text, 80) + (ok ? ": ok" : ": " + why));
			g_Stale = true;
			break;
		}

		case Command::Kind::Pin:
			if (NlCore::GoodPath(C.Path))
				NeedWatch(C.Path);
			else
				g_Note = "즐겨찾기에 넣을 수 없는 주소: " + C.Path;
			break;

		case Command::Kind::Unpin:
			if (std::erase_if(g_Watches, [&](const Watch& watch) { return watch.Path == C.Path; }) > 0)
				g_Changed = true;
			break;

		case Command::Kind::SetLock:
		{
			if (!NlCore::GoodPath(C.Path))
				break;
			Watch& watch = NeedWatch(C.Path);
			watch.HasLock = true;
			watch.LockValue = C.Number;
			watch.Locked = C.Flag;
			watch.HasOwner = C.Flag && NlAccess::InstanceIdentity(NlCore::ParseAskPath(C.Path), watch.Owner);
			watch.Note = C.Flag ? "" : "풀림";
			g_Changed = true;
			Log("explorer lock " + C.Path + " = " + Shortest(C.Number) + (C.Flag ? " on" : " off"));
			break;
		}

		case Command::Kind::Search:
			g_Found = NlSearch::Run(C.Spec);
			g_Searched = true;
			Log("explorer search '" + C.Spec.Name + "'" + (C.Spec.HasValue ? " value " + Shortest(C.Spec.Value) : "")
				+ ": " + std::to_string(g_Found.Hits.size()) + " hits, " + std::to_string(g_Found.Visited) + " visited, "
				+ NlCore::Fixed(g_Found.Seconds, 2) + "s, " + std::to_string(g_Found.Skipped) + " skipped"
				+ (g_Found.Truncated ? ", truncated" : ""));
			break;

		case Command::Kind::Refine:
			NlSearch::Refine(g_Found.Hits, C.Number);
			break;
		}
	}

	// ---- 그리는 쪽 (러너를 부르지 않는다) ----

	// 값 칸. 수와 불리언과 짧은 글은 고칠 수 있다(수는 칸을 떠날 때, 글은 Enter 로 써 넣는다).
	void DrawValue(const NlAccess::Row& Row, const std::string& Path)
	{
		if (Row.IsBool)
		{
			bool value = Row.Number != 0;
			if (ImGui::Checkbox("##b", &value))
				PushPath(Command::Kind::WriteNumber, Path, value ? 1 : 0);
		}
		else if (Row.IsNumber)
		{
			double typed = 0;
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (NlUi::InputNumber("##n", "value:" + Path, Row.Number, "%.10g", typed))		// 칸을 떠날 때 쓴다(Enter 로만 받던 것을 고쳤다. research/18 의 끝)
				PushPath(Command::Kind::WriteNumber, Path, typed);
		}
		else if (Row.IsString && Row.Raw.size() < 255)
		{
			char buffer[256];
			std::snprintf(buffer, sizeof(buffer), "%s", Row.Raw.c_str());
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::InputText("##s", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				Command command{ Command::Kind::WriteString };
				command.Path = Path;
				command.Text = buffer;
				Push(std::move(command));
			}
		}
		else
			ImGui::TextUnformatted(Row.Text.c_str());
	}

	void DrawRoots()
	{
		ImGui::TextDisabled("뿌리를 고르거나 주소를 넣으세요. map:<번호> 와 list:<번호> 도 됩니다.");
		if (ImGui::Selectable("global  (전역)"))
			Go("global");
		for (const NlAccess::RootObject& root : g_Roots)
		{
			const std::string label = root.Name + "  ×" + std::to_string(root.Count);
			if (ImGui::Selectable(label.c_str()))
				Go("inst:" + root.Name);
		}
	}

	void DrawBrowse()
	{
		if (ImGui::Button("뿌리"))
			Go("");
		ImGui::SameLine();
		if (ImGui::Button("위로") && !g_Address.empty())
		{
			const AskPath path = NlCore::ParseAskPath(g_Address);
			if (!path.Error.empty() || path.Steps.empty())
				Go("");
			else
				Go(NlCore::FormatAskPath(NlCore::ParentPath(path)), AsFor(path.Steps.back().Kind));
		}
		ImGui::SameLine();
		if (ImGui::Button("새로 고침"))
			Push({ Command::Kind::Refresh });
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##address", "주소 (예: inst:o_debug   global.__gameplay_vars   map:150)", g_AddressBox,
			sizeof(g_AddressBox), ImGuiInputTextFlags_EnterReturnsTrue))
			Go(g_AddressBox);

		if (!g_Error.empty())
			ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "열 수 없음: %s", g_Error.c_str());
		if (!g_Note.empty())
			ImGui::TextDisabled("%s", g_Note.c_str());

		if (g_Address.empty())
		{
			DrawRoots();
			return;
		}

		// 인스턴스가 여럿이면 옆의 것으로 넘긴다(같은 단계를 그대로 따라간다).
		const AskPath here = NlCore::ParseAskPath(g_Address);
		if (here.Error.empty() && here.Root == "inst" && g_Instances > 1)
		{
			AskPath other = here;
			if (ImGui::SmallButton("<") && here.Number > 0)
			{
				other.Number = here.Number - 1;
				Go(NlCore::FormatAskPath(other), g_As);
			}
			ImGui::SameLine();
			ImGui::Text("인스턴스 %d / %d", static_cast<int>(here.Number) + 1, g_Instances);
			ImGui::SameLine();
			if (ImGui::SmallButton(">") && here.Number + 1 < g_Instances)
			{
				other.Number = here.Number + 1;
				Go(NlCore::FormatAskPath(other), g_As);
			}
			ImGui::SameLine();
		}

		ImGui::SetNextItemWidth(220);
		ImGui::InputTextWithHint("##filter", "이름 거르기", g_Filter, sizeof(g_Filter));
		ImGui::SameLine();
		ImGui::TextDisabled("%zu개%s%s", g_Total, g_Total > g_Rows.size() ? " (일부만 적음)" : "",
			g_Total > k_LiveRows ? " - 많아서 자동으로 새로 읽지 않습니다" : "");

		// 거른 줄들(g_Rows 의 번호).
		std::vector<int> shown;
		const std::string filter = g_Filter;
		for (int i = 0; i < static_cast<int>(g_Rows.size()); i++)
			if (filter.empty() || g_Rows[i].Name.find(filter) != std::string::npos)
				shown.push_back(i);

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("rows", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("이름", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.5f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(shown.size()));
		while (clipper.Step())
			for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; line++)
			{
				const int index = shown[line];
				const NlAccess::Row& row = g_Rows[index];
				const std::string path = g_Address + NlCore::FormatStep(row.Step);
				// ID 는 이름으로 한다. 0.5초마다 다시 읽으면서 멤버가 늘면 번호는 다른 줄을 가리킨다(고치던 칸이 옮겨 간다).
				ImGui::PushID(row.Name.c_str());
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				if (row.IsContainer)
				{
					if (ImGui::Selectable(row.Name.c_str()))
						Go(path);
				}
				else
					ImGui::TextUnformatted(row.Name.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("%s", row.Type.c_str());

				ImGui::TableSetColumnIndex(2);
				DrawValue(row, path);

				ImGui::TableSetColumnIndex(3);
				if (row.IsNumber)
				{
					if (ImGui::SmallButton("핀"))
						PushPath(Command::Kind::Pin, path);
					ImGui::SameLine();
					if (ImGui::SmallButton("잠금"))
						PushPath(Command::Kind::SetLock, path, row.Number, true);
					// 0 이상의 정수는 ds 번호일 수 있다. 눌렀을 때 그 번호의 ds 가 없으면 "열 수 없음"이 뜬다.
					if (!row.IsBool && row.Number >= 0 && row.Number == std::floor(row.Number))
					{
						ImGui::SameLine();
						if (ImGui::SmallButton("map"))
							Go(path, Holder::Map);
						ImGui::SameLine();
						if (ImGui::SmallButton("list"))
							Go(path, Holder::List);
					}
				}
				ImGui::PopID();
			}
		ImGui::EndTable();
	}

	void DrawWatches()
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##addpin", "주소를 넣고 Enter (예: inst:o_time_controller.time_warp)", g_AddPin, sizeof(g_AddPin),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			PushPath(Command::Kind::Pin, NlCore::Trim(g_AddPin));
			g_AddPin[0] = 0;
		}
		if (!g_Note.empty())
			ImGui::TextDisabled("%s", g_Note.c_str());
		if (g_Watches.empty())
		{
			ImGui::TextWrapped("훑기나 찾기에서 '핀'을 누르면 여기에 모입니다. 잠그면 0.1초마다 그 값으로 다시 써 넣습니다. "
				"저장된 잠금은 다음 실행에 꺼진 채로 돌아옵니다.");
			return;
		}

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("watches", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("주소", ImGuiTableColumnFlags_WidthStretch, 4.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("잠금", ImGuiTableColumnFlags_WidthStretch, 3.0f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 0.8f);
		ImGui::TableHeadersRow();

		for (int i = 0; i < static_cast<int>(g_Watches.size()); i++)
		{
			const Watch& watch = g_Watches[i];
			ImGui::PushID(i);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			if (ImGui::Selectable(watch.Path.c_str()))
				GoToParent(watch.Path);

			ImGui::TableSetColumnIndex(1);
			if (watch.Found)
				DrawValue(watch.Row, watch.Path);
			else
				ImGui::TextDisabled("대상 없음");

			ImGui::TableSetColumnIndex(2);
			const bool can_lock = watch.HasLock || (watch.Found && watch.Row.IsNumber);
			ImGui::BeginDisabled(!can_lock);
			bool locked = watch.Locked;
			if (ImGui::Checkbox("##lock", &locked))
				PushPath(Command::Kind::SetLock, watch.Path, watch.HasLock ? watch.LockValue : watch.Row.Number, locked);
			ImGui::EndDisabled();
			if (watch.HasLock)
			{
				ImGui::SameLine();
				double typed = 0;
				ImGui::SetNextItemWidth(110);
				if (NlUi::InputNumber("##lv", "lock:" + watch.Path, watch.LockValue, "%.10g", typed))		// 칸을 떠날 때 건다
					PushPath(Command::Kind::SetLock, watch.Path, typed, watch.Locked);
				if (!watch.Note.empty())
				{
					ImGui::SameLine();
					ImGui::TextDisabled("%s", watch.Note.c_str());
				}
			}

			ImGui::TableSetColumnIndex(3);
			if (ImGui::SmallButton("삭제"))
				PushPath(Command::Kind::Unpin, watch.Path);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	void DrawSearch()
	{
		ImGui::SetNextItemWidth(240);
		ImGui::InputTextWithHint("##name", "이름의 일부 (예: gold)", g_SearchName, sizeof(g_SearchName));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160);
		ImGui::InputTextWithHint("##value", "값 (예: 3000)", g_SearchValue, sizeof(g_SearchValue));
		ImGui::SameLine();
		ImGui::Checkbox("전역", &g_ScopeGlobals);
		ImGui::SameLine();
		ImGui::Checkbox("인스턴스", &g_ScopeInstances);
		ImGui::SameLine();
		ImGui::Checkbox("ds", &g_ScopeDs);

		double value = 0;
		const std::string value_text = NlCore::Trim(g_SearchValue);
		const bool has_value = NlCore::ParseNumber(value_text, value);
		if (ImGui::Button("찾기"))
		{
			Command command{ Command::Kind::Search };
			command.Spec.Name = NlCore::Trim(g_SearchName);
			command.Spec.HasValue = has_value;
			command.Spec.Value = value;
			command.Spec.Globals = g_ScopeGlobals;
			command.Spec.Instances = g_ScopeInstances;
			command.Spec.Ds = g_ScopeDs;
			if (!value_text.empty() && !has_value)
				g_SearchNote = "값이 수가 아닙니다.";
			else if (command.Spec.Name.empty() && !has_value)
				g_SearchNote = "이름이나 값을 넣으세요.";
			else
			{
				g_SearchNote.clear();
				Push(std::move(command));
			}
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!g_Searched || !has_value);
		if (ImGui::Button("다시 거르기"))
			PushPath(Command::Kind::Refine, "", value);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("찾는 동안 게임이 잠깐 멈춥니다(길어도 3초).");
		if (!g_SearchNote.empty())
			ImGui::TextUnformatted(g_SearchNote.c_str());

		if (!g_Searched)
		{
			ImGui::TextWrapped("값으로 자리를 찾는 법: 금화가 3000 이면 값에 3000 을 넣어 찾습니다. 돈을 써서 2950 이 되면 "
				"값에 2950 을 넣고 '다시 거르기'를 누릅니다. 남은 것이 금화의 자리입니다.");
			return;
		}
		ImGui::Text("%zu개 찾음 (%zu개를 %.1f초에 봄)%s", g_Found.Hits.size(), g_Found.Visited, g_Found.Seconds,
			g_Found.Truncated ? " - 한도에 걸려 일부만 봤습니다" : "");
		if (g_Found.Skipped > 0)
			ImGui::TextDisabled("들어가지 않은 그릇 %zu개(깊이 10, 길이 4096 을 넘는 배열, 오브젝트마다 64번째 뒤의 인스턴스). "
				"여기에 없다고 게임에 없는 것은 아닙니다.", g_Found.Skipped);

		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY
			| ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
		if (!ImGui::BeginTable("hits", 4, flags))
			return;
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("주소", ImGuiTableColumnFlags_WidthStretch, 5.0f);
		ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		ImGui::TableSetupColumn("값", ImGuiTableColumnFlags_WidthStretch, 2.0f);
		ImGui::TableSetupColumn("##actions", ImGuiTableColumnFlags_WidthStretch, 0.6f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(g_Found.Hits.size()));
		while (clipper.Step())
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			{
				const NlSearch::Hit& hit = g_Found.Hits[i];
				ImGui::PushID(i);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::Selectable(hit.Path.c_str()))
					GoToParent(hit.Path);
				ImGui::TableSetColumnIndex(1);
				ImGui::TextDisabled("%s", hit.Type.c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(hit.Text.c_str());
				ImGui::TableSetColumnIndex(3);
				if (ImGui::SmallButton("핀"))
					PushPath(Command::Kind::Pin, hit.Path);
				ImGui::PopID();
			}
		ImGui::EndTable();
	}
}

void NlExplorer::Init(LogFn Log_, const std::vector<std::string>& Pins, const std::vector<NlCore::LockLine>& Locks)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	for (const std::string& pin : Pins)
		NeedWatch(pin);
	for (const NlCore::LockLine& line : Locks)
	{
		Watch& watch = NeedWatch(line.Path);
		watch.HasLock = true;
		watch.LockValue = line.Value;
		watch.Locked = false;		// 인스턴스의 차례가 실행마다 다를 수 있다. 사용자가 다시 건다(스펙 §5)
	}
	g_Changed = false;
}

void NlExplorer::GameTick(double Now, bool Shown)
{
	std::lock_guard lock(g_Mutex);

	std::vector<Command> queue;
	queue.swap(g_Queue);
	for (const Command& command : queue)
		Run(command);

	if (Now >= g_NextLock)
	{
		g_NextLock = Now + 0.1;
		for (Watch& watch : g_Watches)
			if (watch.Locked)
				Hold(watch);
	}
	if (!Shown)
		return;

	// 큰 그릇(전역)은 "새로 고침"을 누를 때만 다시 읽는다. 열지 못한 그릇은 0.5초마다 다시 해 본다(대상이 나중에 생긴다).
	if (g_Stale || (Now >= g_NextView && g_Total <= k_LiveRows))
	{
		g_Stale = false;
		g_NextView = Now + 0.5;
		LoadView();
	}
	if (Now >= g_NextWatch)
	{
		g_NextWatch = Now + 0.25;
		for (Watch& watch : g_Watches)
			Snapshot(watch);
	}
}

void NlExplorer::Draw()
{
	std::lock_guard lock(g_Mutex);
	if (!ImGui::BeginTabBar("explorer"))
		return;

	const ImGuiTabItemFlags focus = g_FocusBrowse ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
	g_FocusBrowse = false;
	if (ImGui::BeginTabItem("훑기", nullptr, focus))
	{
		DrawBrowse();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("즐겨찾기·잠금"))
	{
		DrawWatches();
		ImGui::EndTabItem();
	}
	if (ImGui::BeginTabItem("찾기"))
	{
		DrawSearch();
		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();
}

void NlExplorer::Navigate(const std::string& Address)
{
	std::lock_guard lock(g_Mutex);
	Go(Address);
}

bool NlExplorer::TakeChanges(std::vector<std::string>& Pins, std::vector<NlCore::LockLine>& Locks)
{
	std::lock_guard lock(g_Mutex);
	if (!g_Changed)
		return false;
	g_Changed = false;

	Pins.clear();
	Locks.clear();
	for (const Watch& watch : g_Watches)
	{
		Pins.push_back(watch.Path);
		if (watch.HasLock)
			Locks.push_back({ watch.Path, watch.LockValue });
	}
	return true;
}

int NlExplorer::ActiveLocks()
{
	std::lock_guard lock(g_Mutex);
	return static_cast<int>(std::count_if(g_Watches.begin(), g_Watches.end(), [](const Watch& watch) { return watch.Locked; }));
}

void NlExplorer::ReleaseAll()
{
	std::lock_guard lock(g_Mutex);
	for (Watch& watch : g_Watches)
		if (watch.Locked)
		{
			watch.Locked = false;
			watch.Note = "풀림";
		}
}
