#include "Menu.hpp"

#include "Access.hpp"
#include "Build.hpp"
#include "Jobs.hpp"
#include "Mines.hpp"
#include "Season.hpp"
#include "Cheats.hpp"
#include "Court.hpp"
#include "Crime.hpp"
#include "Diplomacy.hpp"
#include "Economy.hpp"
#include "Events.hpp"
#include "People.hpp"
#include "World.hpp"
#include "Explorer.hpp"
#include "Tweaks.hpp"
#include "Ui.hpp"
#include "core/AskPath.hpp"
#include "core/CheatState.hpp"
#include "core/CheatTable.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <vector>

using NlCore::Area;
using NlCore::Shortest;
using Clock = std::chrono::steady_clock;

namespace
{
	std::filesystem::path g_StatePath;
	std::string g_Version;
	std::function<void(const std::string&)> g_Log;
	Clock::time_point g_Start;

	std::atomic<int> g_Page = static_cast<int>(Area::Explorer);
	std::atomic<bool> g_InGame = false;

	NlCore::CheatState g_State;		// 상태 파일에 적힌(적을) 것. 게임 스레드만 만진다
	bool g_SavePending = false;
	double g_SaveAt = 0, g_NextState = 0;

	// 시험 설정의 ask= 와 poke=. 화면을 뜨기 5초 전에 한 번 한다.
	std::vector<std::string> g_Asks, g_Pokes;
	bool g_TestsDone = true;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	double Seconds()
	{
		return std::chrono::duration<double>(Clock::now() - g_Start).count();
	}

	void Ask(const std::string& Path)
	{
		YYTK::RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Read(NlCore::ParseAskPath(Path), value, why))
		{
			Log("ask " + Path + " : " + why);
			return;
		}
		const NlAccess::Row row = NlAccess::Describe({}, value);
		Log("ask " + Path + " = " + row.Type + " " + row.Text);
	}

	// "<주소>=<수>": 써 넣고, 다시 읽고, 원래 값으로 되돌린다.
	void Poke(const std::string& Line)
	{
		const size_t eq = Line.rfind('=');
		double wanted = 0, old = 0, read = 0, back = 0;
		if (eq == std::string::npos || !NlCore::ParseNumber(NlCore::Trim(Line.substr(eq + 1)), wanted))
		{
			Log("poke " + Line + " : cannot read the line");
			return;
		}
		const std::string path = NlCore::Trim(Line.substr(0, eq));
		if (!NlAccess::ReadNumber(path, old))
		{
			Log("poke " + path + " : not a readable number");
			return;
		}

		// 처음 쓰는 빌트인이 게임을 끝내면 어느 쓰기였는지 남게, 쓰기 전에 한 줄 적는다.
		Log("poke " + path + ": old " + Shortest(old) + ", writing " + Shortest(wanted));
		std::string why, why_back;
		const bool stuck = NlAccess::WriteNumber(path, wanted, why);
		const bool have = NlAccess::ReadNumber(path, read);
		const bool restored = NlAccess::WriteNumber(path, old, why_back);
		const bool have_back = NlAccess::ReadNumber(path, back);
		Log("poke " + path + ": old " + Shortest(old) + " -> wrote " + Shortest(wanted) + ", read " + (have ? Shortest(read) : "?")
			+ (stuck ? " (stuck)" : " (not stuck: " + why + ")") + "; restored " + (have_back ? Shortest(back) : "?")
			+ (restored ? "" : " (restore failed: " + why_back + ")"));
	}

	void RunTests()
	{
		if (g_TestsDone)
			return;
		const double ready = NlUi::SecondsSinceReady();
		if (ready < 0 || ready < NlUi::ShotSeconds() - 5)
			return;
		g_TestsDone = true;

		Log("ui tests start");
		for (const std::string& path : g_Asks)
			Ask(path);
		for (const std::string& line : g_Pokes)
			Poke(line);
		Log("ui tests done");
	}

	void Save()
	{
		std::ofstream out(g_StatePath, std::ios::trunc);
		out << NlCore::FormatCheatState(g_State);
	}

	// 그 영역에 보여 줄 것이 있는가. 없는 영역은 목록에서 흐리게 보인다.
	bool HasContent(Area Where)
	{
		return NlCore::GetArea(Where).Panel || NlCheats::HasItems(Where) || NlCore::HasKnobs(Where);
	}
}

void NlMenu::Init(const std::filesystem::path& ModuleDir, const std::string& Version, std::function<void(const std::string&)> Log_)
{
	g_StatePath = ModuleDir / "NlToyBox.cheats.txt";
	g_Version = Version;
	g_Log = std::move(Log_);
	g_Start = Clock::now();

	if (std::ifstream in(g_StatePath); in)
		g_State = NlCore::KeepKnown(NlCore::ParseCheatState(in));
	NlCheats::Init(g_Log, g_State);
	NlExplorer::Init(g_Log, g_State.Pins, g_State.Locks);
	NlEconomy::Init(g_Log, g_State.Floors);

	// 시험 설정(NlToyBox.ui.txt). 평소에는 아무 줄도 없다.
	for (const std::string& page : NlUi::TestValues("page"))
		if (const NlCore::AreaInfo* area = NlCore::FindArea(page))
			g_Page = static_cast<int>(area->Id);
	for (const std::string& path : NlUi::TestValues("path"))
		NlExplorer::Navigate(path);
	g_Asks = NlUi::TestValues("ask");
	g_Pokes = NlUi::TestValues("poke");
	g_TestsDone = g_Asks.empty() && g_Pokes.empty();
}

void NlMenu::GameTick()
{
	const double now = Seconds();
	const bool visible = NlUi::Visible();
	const Area page = static_cast<Area>(g_Page.load());

	NlExplorer::GameTick(now, visible && page == Area::Explorer);
	NlCheats::GameTick(now, visible);
	NlEconomy::GameTick(now, visible && page == Area::Economy);
	NlBuild::GameTick(now);
	NlJobs::GameTick(now);
	NlWorld::GameTick(now);
	NlEvents::Tick(now, visible && page == Area::Events);
	NlSeason::Tick(now, visible && page == Area::World);
	NlMines::Tick(now);
	NlDiplomacy::GameTick(now, visible && page == Area::Diplomacy);
	NlCrime::GameTick(now, visible && page == Area::Crime);
	NlCourt::GameTick(now, visible && (page == Area::Lord || page == Area::Religion));		// 종교 패널의 주교와의 평판도 같은 모듈이 한다
	NlPeople::GameTick(now, visible && (page == Area::Person || page == Area::Lord || page == Area::People || page == Area::Knowledge || page == Area::Items || page == Area::Army));
	if (visible && now >= g_NextState)
	{
		g_NextState = now + 1;
		g_InGame = NlAccess::InGame();
	}
	RunTests();

	// 바뀐 것은 0.8초 뒤에 적는다(값을 끄는 동안 파일을 되풀이해 쓰지 않는다).
	const bool cheats = NlCheats::TakeChanges(g_State.On, g_State.Numbers);
	const bool explorer = NlExplorer::TakeChanges(g_State.Pins, g_State.Locks);
	const bool floors = NlEconomy::TakeChanges(g_State.Floors);
	if (cheats || explorer || floors)
	{
		g_SavePending = true;
		g_SaveAt = now + 0.8;
	}
	if (g_SavePending && now >= g_SaveAt)
	{
		g_SavePending = false;
		Save();
	}
}

void NlMenu::Draw()
{
	// 상태 줄
	ImGui::TextDisabled("NlToyBox %s", g_Version.c_str());
	ImGui::SameLine();
	ImGui::TextUnformatted(g_InGame ? "게임 화면" : "게임 화면이 아님 (스위치는 게임을 시작하면 적용됩니다)");
	const int active = NlCheats::ActiveCount() + NlExplorer::ActiveLocks();
	ImGui::SameLine();
	ImGui::TextDisabled("켠 것 %d", active);
	ImGui::SameLine();
	ImGui::BeginDisabled(active == 0);
	if (ImGui::SmallButton("모두 끄기"))
	{
		NlCheats::ReleaseAll();
		NlExplorer::ReleaseAll();
	}
	ImGui::EndDisabled();
	ImGui::Separator();

	// 왼쪽: 영역의 목록. 아직 채우지 않은 영역은 흐리게, 몇 단계인지 적는다.
	const Area page = static_cast<Area>(g_Page.load());
	ImGui::BeginChild("areas", ImVec2(150, 0), ImGuiChildFlags_Borders);
	for (const NlCore::AreaInfo& area : NlCore::Areas())
	{
		const bool ready = HasContent(area.Id);
		const std::string label = ready ? area.Label : std::string(area.Label) + " (" + std::to_string(area.Stage) + "단계)";
		ImGui::BeginDisabled(!ready);
		if (ImGui::Selectable(label.c_str(), page == area.Id) && ready)
			g_Page = static_cast<int>(area.Id);
		ImGui::EndDisabled();
	}
	ImGui::EndChild();

	// 오른쪽: 고른 영역
	ImGui::SameLine();
	ImGui::BeginChild("panel", ImVec2(0, 0));
	switch (page)
	{
	case Area::Explorer: NlExplorer::Draw(); break;
	case Area::Time: NlCheats::DrawTime(); break;
	case Area::Economy:
		// 표의 항목(거래, 창고 용량)과 배율을 먼저 그린다. 자원의 표가 길어 그 아래에 두면 보이지 않는다.
		if (NlCheats::HasItems(page))
			NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlEconomy::Draw();
		break;
	case Area::Person:
		NlPeople::DrawPerson();
		break;
	case Area::Lord:
		NlPeople::DrawLords();
		NlTweaks::DrawArea(page);
		NlCourt::Draw();		// 영주끼리의 평판과 왕에 대한 충성(src/Court.cpp)
		break;
	case Area::Knowledge:
		// 표의 항목(연구 시간)과 배율(교본 경험)을 먼저, 그 아래에 지식을 주는 패널.
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlPeople::DrawKnowledge();
		break;
	case Area::Items:
		NlPeople::DrawItems();
		break;
	case Area::Presets:
		NlCheats::DrawPresets();
		break;
	case Area::Crime:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		NlCrime::Draw();
		break;
	case Area::World:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		NlWorld::DrawWorld();
		break;
	case Area::Events:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlEvents::Draw();
		break;
	case Area::Religion:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlWorld::DrawReligion();
		NlCourt::DrawBishop();
		break;
	case Area::Diplomacy:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlDiplomacy::Draw();
		break;
	case Area::Army:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlPeople::DrawArmy();
		break;
	case Area::People:
		// 표의 항목(욕구, 이주민)을 먼저, 그 아래에 지금 한 번 하는 단추들.
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlPeople::DrawPeople();
		break;
	case Area::Util:
		NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		ImGui::Separator();
		NlWorld::DrawUtil();
		break;
	default:
		// 표의 항목이 없고 배율만 있는 영역(지식)에서는 "N단계에서 채웁니다"를 적지 않는다.
		if (NlCheats::HasItems(page) || !NlCore::HasKnobs(page))
			NlCheats::DrawArea(page);
		NlTweaks::DrawArea(page);
		break;
	}
	ImGui::EndChild();
}

bool NlMenu::SetPage(const std::string& Key)
{
	const NlCore::AreaInfo* area = NlCore::FindArea(Key);
	if (!area)
		return false;
	g_Page = static_cast<int>(area->Id);
	return true;
}
