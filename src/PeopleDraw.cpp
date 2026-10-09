#include "PeopleInternal.hpp"

#include "PeopleAccess.hpp"
#include "TraitText.hpp"
#include "Ui.hpp"
#include "core/EconomyPlan.hpp"
#include "core/FamilyPlan.hpp"
#include "core/Localization.hpp"
#include "core/RolePlan.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <algorithm>

using namespace YYTK;
using namespace NlPeopleAccess;
using namespace NlPeople::Internal;
using NlCore::PersonAct;
using NlCore::PersonCommand;
using NlCore::PersonRow;
using NlCore::Shortest;

// 그리는 쪽(러너를 부르지 않는다: 청만 쌓고 틱이 한다). 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다.
namespace NlPeople::Internal
{
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

	// 흐린 글. 창의 너비에서 줄을 바꾼다(긴 안내 글이 창 밖으로 잘리지 않게).
	void DrawLast()
	{
		if (!g_Now.Last.empty())
			NlUi::Hint(g_Now.Last);
	}

	// "마우스 자리에 소환" 단추들. 누르면 틱이 디버그 소환기를 부른다(그리는 쪽은 청만 쌓는다).
	void DrawSpawnHere(std::initializer_list<NlCore::SpawnKind> Kinds)
	{
		ImGui::TextUnformatted("마우스 자리에 소환");
		for (const NlCore::SpawnKind kind : Kinds)
		{
			ImGui::SameLine();
			if (ImGui::Button((std::string(NlCore::SpawnLabel(kind)) + " +1").c_str()) && g_SpawnKinds.size() < 20)
				g_SpawnKinds.push_back(kind);
		}
	}

	// "마우스 자리에 영주 소환": 성별·나이·문화·역할을 골라 두고 만든다(research/35). 고른 것은 이 실행 안에서만 든다.
	// 아무것도 고르지 않으면 지금까지의 소환(소환기의 __spawn_lord)과 같은 길로 간다. 그리는 쪽은 청만 쌓는다.
	void DrawSpawnLord()
	{
		ImGui::SeparatorText("마우스 자리에 영주 소환");
		ImGui::SetNextItemWidth(120);
		if (ImGui::BeginCombo("성별##lord_spawn", NlCore::LordGenderLabel(g_LordSpawn.Gender)))
		{
			for (const NlCore::LordGender gender : { NlCore::LordGender::Any, NlCore::LordGender::Male, NlCore::LordGender::Female })
				if (ImGui::Selectable(NlCore::LordGenderLabel(gender), gender == g_LordSpawn.Gender))
					g_LordSpawn.Gender = gender;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		double typed = g_LordSpawn.Age, age = 0;
		if (NlUi::InputNumber("나이 (0 = 게임에 맡김, 18~80)##lord_spawn", "lord_spawn_age", g_LordSpawn.Age, "%.0f", typed) && NlCore::SpawnAgeValue(typed, age))
			g_LordSpawn.Age = age;

		// 문화: 게임에서 읽은 목록. 고른 이름이 이 게임에 없으면(다른 세이브, 게임 갱신) 게임에 맡김으로 되돌린다.
		const CultureName* picked = nullptr;
		for (const CultureName& one : g_Now.Cultures)
			if (one.Name == g_LordSpawn.Culture)
				picked = &one;
		if (!picked)
			g_LordSpawn.Culture.clear();
		ImGui::SetNextItemWidth(170);
		if (ImGui::BeginCombo("문화##lord_spawn", picked ? picked->Label.c_str() : "게임에 맡김"))
		{
			if (ImGui::Selectable("게임에 맡김", picked == nullptr))
				g_LordSpawn.Culture.clear();
			for (const CultureName& one : g_Now.Cultures)
				if (ImGui::Selectable((one.Label + "##" + one.Name).c_str(), picked == &one))
					g_LordSpawn.Culture = one.Name;
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		const NlCore::RolePreset* role = g_LordSpawn.Role.empty() ? nullptr : NlCore::FindRole(g_LordSpawn.Role);
		ImGui::SetNextItemWidth(200);
		if (ImGui::BeginCombo("역할 프리셋##lord_spawn", role ? role->Label : "없음"))
		{
			if (ImGui::Selectable("없음", role == nullptr))
				g_LordSpawn.Role.clear();
			for (const NlCore::RolePreset& one : NlCore::RolePresets())
				if (ImGui::Selectable((std::string(one.Label) + "##" + one.Id).c_str(), role == &one))
					g_LordSpawn.Role = one.Id;
			ImGui::EndCombo();
		}

		if (ImGui::Button("영주 +1"))
		{
			// 아무것도 정하지 않았으면 지금까지의 길로(소환기). 정한 것이 있으면 그것과 함께 쌓는다.
			if (NlCore::IsPlainLordSpawn(g_LordSpawn))
			{
				if (g_SpawnKinds.size() < 20)
					g_SpawnKinds.push_back(NlCore::SpawnKind::Lord);
			}
			else if (g_LordSpawns.size() < 20)
				g_LordSpawns.push_back(g_LordSpawn);
		}
		NlUi::Hint("플레이어의 영주 하나를 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 되돌릴 수 없습니다. "
			"성별은 게임이 영주를 만드는 함수에 그대로 넘깁니다(이름과 외모가 그 성별로 만들어집니다). 문화는 만든 뒤 게임의 세터(set_culture)로 바꿉니다: "
			"영주의 문화와 외모의 문화 이름이 바뀌고 그 문화의 방언이 더해집니다. 이름은 게임이 처음 붙인 대로입니다(게임의 소환기도 이름의 문화를 따로 뽑습니다). "
			"나이는 어른만(18~80), 역할 프리셋은 '인물'의 것과 같습니다(능력치를 올리고 재능을 붙입니다). 고르지 않은 것은 게임에 맡깁니다.");
	}

	// 특성의 설명을 풍선 글로(바로 앞에 그린 것 위에 마우스가 있을 때).
	void TraitTooltip(const std::string& Name)
	{
		const std::string& about = NlTraitText::Hint(Name);
		if (!about.empty() && ImGui::BeginItemTooltip())
		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
			ImGui::TextUnformatted(about.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}

	// 역할 프리셋: 고르면 그 사람에게서 무엇이 바뀌는지 보이고, '이 역할로'를 누르면 틱이 입힌다.
	// 미리 보기는 읽어 둔 값(Detail)으로 셈한다(core 의 PlanRole. 러너를 부르지 않는다). 틱은 적용할 때 값을 다시 읽어 다시 셈한다.
	void DrawRole(const PersonRow& Row, const Detail& One)
	{
		const std::vector<NlCore::RolePreset>& roles = NlCore::RolePresets();
		if (g_RolePick < 0 || static_cast<size_t>(g_RolePick) >= roles.size())
			g_RolePick = 0;
		ImGui::SetNextItemWidth(200);
		if (ImGui::BeginCombo("##role", roles[g_RolePick].Label))
		{
			for (size_t i = 0; i < roles.size(); i++)
				if (ImGui::Selectable(roles[i].Label, static_cast<int>(i) == g_RolePick))
					g_RolePick = static_cast<int>(i);
			ImGui::EndCombo();
		}
		const NlCore::RolePreset& role = roles[g_RolePick];
		const NlCore::RoleTodo todo = NlCore::PlanRole(role, One.Skills, One.Traits);
		ImGui::SameLine();
		ImGui::BeginDisabled(todo.Empty());
		if (ImGui::Button("이 역할로"))
			Push(PersonAct::Role, Row.Uuid, -1, 0, role.Id);
		ImGui::EndDisabled();
		// 되돌리기: 이 실행에서 이 사람에게 입힌 것이 있을 때만(기억은 이 실행 안에서만 남는다).
		const bool remembered = g_RoleMemory.count(Row.Uuid) > 0 && !g_RoleMemory.at(Row.Uuid).Empty();
		ImGui::SameLine();
		ImGui::BeginDisabled(!remembered);
		if (ImGui::Button("되돌리기"))
			Push(PersonAct::RoleUndo, Row.Uuid, -1, 0, std::string());
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", remembered ? "이 실행에서 입힌 역할 프리셋을 되돌립니다: 능력치는 전 값으로, 붙인 특성은 떼고 뗀 특성은 다시 붙입니다"
				: "이 실행에서 이 사람에게 입힌 역할 프리셋이 없습니다 (기억은 이 실행 안에서만 남습니다)");
		// 요약은 제 줄에 둔다(단추 옆에 두자 기본 너비의 창에서 오른쪽이 잘렸다. 0.24.0 의 화면에서 봤다).
		NlUi::Hint(NlCore::RolePreview(todo));
		if (g_RoleLastFor == Row.Uuid && !g_RoleLast.empty())
			NlUi::Hint("마지막 결과 - " + g_RoleLast);

		// 능력치: 프리셋의 것을 차례대로. 지금 값이 더 높으면 그대로 둔다.
		// 한 능력치의 글("… 4 -> 20")이 줄 사이에서 갈리지 않게 조각마다 따로 그리고, 다음 조각이 들어갈 때만 옆에 둔다.
		const std::vector<NlCore::NamedKey>& skills = NlCore::SkillNames();
		std::vector<std::string> items = { "능력치:" };
		for (const NlCore::RoleSkill& wanted : role.Skills)
			for (size_t i = 0; i < skills.size(); i++)
			{
				if (std::string(skills[i].Key) != wanted.Skill)
					continue;
				const double now = i < One.Skills.size() ? One.Skills[i] : k_Unknown;
				std::string item = std::string(NlTraitText::SkillLabel(i)) + " ";
				if (now == k_Unknown)
					item += "없음";		// 그 사람에게 없는 능력치(주민은 전투만 있다)
				else if (now >= wanted.Level)
					item += NumberText(now, 0) + " (그대로)";
				else
					item += NumberText(now, 0) + " -> " + std::to_string(wanted.Level);
				items.push_back(std::move(item));
			}
		const float gap = ImGui::GetStyle().ItemSpacing.x * 2;
		const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
		for (size_t i = 0; i < items.size(); i++)
		{
			ImGui::TextUnformatted(items[i].c_str());
			if (i + 1 < items.size() && ImGui::GetItemRectMax().x + gap + ImGui::CalcTextSize(items[i + 1].c_str()).x < right)
				ImGui::SameLine(0.0f, gap);
		}

		// 특성: 한 줄에 하나(설명은 풍선 글로). 좁은 창에서도 읽히게 표의 칸에 두지 않는다.
		// 뗄 것을 먼저 보인다(먼저 떼기도 한다). 가진 것만 한 줄씩 그리고, 이 사람에게 없는 것은 흐린 한 줄로 모은다.
		ImGui::TextUnformatted("뗄 특성 (이 역할에 해로운 것. 가진 것만 뗍니다):");
		ImGui::Indent();
		std::string absent;
		bool owned = false;
		for (const char* name : role.Remove)
		{
			if (!NlCore::Has(One.Traits, name))
			{
				absent += (absent.empty() ? "" : ", ") + NlTraitText::Label(name);
				continue;
			}
			owned = true;
			ImGui::Text("뗌    %s", NlTraitText::Label(name).c_str());
			TraitTooltip(name);
			const char* note = NlCore::RemoveNote(name);
			if (note[0])
			{
				ImGui::Indent();
				NlUi::Hint(note);		// 제 줄에, 창의 너비에서 줄을 바꾼다
				ImGui::Unindent();
			}
		}
		if (!owned)
			ImGui::TextDisabled("뗄 것이 없습니다");
		if (!absent.empty())
			NlUi::Hint("이 사람에게 없는 것: " + absent);
		ImGui::Unindent();
		ImGui::TextUnformatted("붙일 특성:");
		ImGui::Indent();
		for (const char* name : role.Add)
		{
			if (NlCore::Has(One.Traits, name))
				ImGui::TextDisabled("있음    %s", NlTraitText::Label(name).c_str());
			else if (!NlTraitText::Known(name))
				ImGui::TextDisabled("게임에 없음    %s", name);
			else
				ImGui::Text("붙임    %s", NlTraitText::Label(name).c_str());
			TraitTooltip(name);
		}
		ImGui::Unindent();
		NlUi::Hint("능력치는 올리기만 합니다(이미 더 높은 것과 프리셋에 없는 것은 그대로 둡니다). 특성은 게임의 설명 글을 읽고 골랐고, 재능마다의 효과를 플레이에서 재지는 않았습니다. "
			"한 번에 되돌리는 단추는 없습니다: 붙인 특성은 아래 '특성'에서 하나씩 떼고, 능력치는 아래 '능력치'에서 내립니다.");
	}

	// 임신·성장(core/FamilyPlan, research/24). 그리는 쪽은 읽어 둔 값으로 판단하고 청만 쌓는다.
	void DrawFamily(const PersonRow& Row, const Detail& One)
	{
		if (!NlCore::IsPlayersLord(Row))
		{
			NlUi::Hint("임신·성장의 단추는 플레이어의 영주에게만 둡니다 (주민·손님·다른 진영에게는 게임의 함수를 불러 본 적이 없습니다).");
			return;
		}
		const int stage = NlCore::PregnancyStage(One.Traits);
		const bool kid = NlCore::IsKid(One.Traits);
		const bool forbid = NlCore::Has(One.Traits, NlCore::k_PregnantForbid);
		const char* gender = One.Gender == NlCore::k_Female ? "여성" : One.Gender == NlCore::k_Male ? "남성" : "성별을 읽지 못했습니다";
		if (stage > 0)
			ImGui::Text("%s, 임신 %d/3기", gender, stage);
		else
			ImGui::Text("%s%s%s", gender, kid ? ", 아이" : "", forbid ? ", 출산 뒤의 임신 금지가 붙어 있습니다" : "");
		ImGui::BeginDisabled(stage == 0);
		if (ImGui::Button("임신 다음 단계"))
			Push(PersonAct::PregnancyNext, Row.Uuid);
		ImGui::SameLine();
		if (ImGui::Button("바로 출산"))
			Push(PersonAct::Birth, Row.Uuid);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!kid);
		if (ImGui::Button("어른으로 (18세)"))
			Push(PersonAct::GrowUp, Row.Uuid);
		ImGui::EndDisabled();
		if (forbid)
		{
			ImGui::SameLine();
			if (ImGui::Button("임신 금지 떼기"))
				Push(PersonAct::TraitRemove, Row.Uuid, -1, 0, NlCore::k_PregnantForbid);
		}

		// 임신 시작: 아버지를 고른다(플레이어의 다른 영주. 성별과 나이는 틱이 누를 때 다시 본다).
		std::string why;
		if (NlCore::CanConceive(One.Gender, One.Traits, why))
		{
			const PersonRow* father = g_FatherPick.empty() ? nullptr : FindRow(g_FatherPick);
			if (father && (father->Uuid == Row.Uuid || father->Dead || !NlCore::IsPlayers(*father) || !father->Character))
				father = nullptr;
			ImGui::SetNextItemWidth(160);
			if (ImGui::BeginCombo("##father", father ? father->Name.c_str() : "아버지를 고르세요"))
			{
				for (const PersonRow& row : g_Now.People)
					if (NlCore::IsPlayers(row) && row.Character && !row.Dead && row.Uuid != Row.Uuid && ImGui::Selectable((row.Name + "##" + row.Uuid).c_str(), father == &row))
						g_FatherPick = row.Uuid;
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!father);
			if (ImGui::Button("임신 시키기") && father)
				Push(PersonAct::Conceive, Row.Uuid, -1, 0, father->Uuid);
			ImGui::EndDisabled();
		}
		NlUi::Hint("'임신 다음 단계'와 '바로 출산'은 게임의 디버그 함수를 부릅니다(1/3기, 2/3기, 3/3기, 출산). 게임의 확률을 그대로 타서 유산으로 끝날 수 있습니다"
			"('인구·욕구'의 '유산 없음'을 켜 두면 나지 않았습니다). '임신 시키기'는 아버지를 적고 임신 1/3기를 붙입니다. "
			"'어른으로'는 나이를 18 로 맞춥니다: 게임이 아이를 소영주로 만들어 영주 목록에서 빠집니다. 되돌리는 단추는 없습니다.");
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
		ImGui::BeginDisabled(one.Age == k_Unknown || g_AgeInputFor != who);		// 나이를 읽지 못한 사람에게 앞 사람의 수로 부르지 않는다
		ImGui::SetNextItemWidth(90);
		ImGui::InputInt("##age", &g_AgeInput);
		ImGui::SameLine();
		if (ImGui::Button("이 나이로"))
			Push(PersonAct::AgeSet, who, -1, g_AgeInput);
		ImGui::EndDisabled();
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
		NlUi::Hint("기분은 게임이 생각의 합으로 다시 셈합니다. '행복하게'는 게임의 디버그용 생각(+100, 하루)을 붙입니다. 능력치는 0~20, 나이는 1~120 입니다.");

		// 짧은 것을 위에 둔다(역할 프리셋의 미리 보기가 길어 그 아래의 것은 스크롤해야 보였다).
		ImGui::SeparatorText("임신·성장");
		DrawFamily(Row, one);

		ImGui::SeparatorText("역할 프리셋");
		DrawRole(Row, one);

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
				ImGui::Text("%s (%s)", NlTraitText::SkillLabel(i), skills[i].Key);
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
					Push(PersonAct::NeedSet, who, static_cast<int>(i), NlCore::k_FillAll);		// 상한까지(읽은 상한을 보내지 않는다)
				ImGui::SameLine();
				if (ImGui::SmallButton("0 으로"))
					Push(PersonAct::NeedSet, who, static_cast<int>(i), 0);
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("특성");
		if (g_TraitScroll)
		{
			ImGui::SetScrollHereY(0.0f);
			g_TraitScroll = false;
		}
		for (size_t i = 0; i < one.Traits.size(); i++)
		{
			ImGui::PushID(200 + static_cast<int>(i));
			ImGui::BeginDisabled(NlCore::IsProtectedTrait(one.Traits[i]));		// 종과 죽음의 특성은 떼지 않는다
			if (ImGui::SmallButton("떼기"))
				Push(PersonAct::TraitRemove, who, -1, 0, one.Traits[i]);
			ImGui::EndDisabled();
			ImGui::PopID();
			ImGui::SameLine();
			ImGui::TextUnformatted(NlTraitText::Label(one.Traits[i]).c_str());
			TraitTooltip(one.Traits[i]);
		}
		ImGui::SetNextItemWidth(180);
		ImGui::InputText("찾기 (명칭, 게임의 이름, 설명의 글)", g_TraitFilter, sizeof(g_TraitFilter));
		ImGui::SameLine();
		ImGui::Checkbox("전체 특성 목록", &g_TraitListOpen);
		if (g_TraitFilter[0] || g_TraitListOpen)
		{
			// 게임의 특성 전부(찾는 글이 있으면 맞는 것만). 표가 길어 스크롤되는 칸 안에 둔다.
			int shown = 0;
			if (ImGui::BeginChild("trait_list", ImVec2(0, 280), ImGuiChildFlags_Borders))
			{
				// 한 특성에 두 줄: 단추와 명칭(게임의 이름), 그 아래에 설명. 설명을 표의 칸에 두면 좁은 창에서 한 글자 너비가 된다(0.22.1 의 화면에서 봤다).
				for (const std::string& name : NlTraitText::Shown())
				{
					const std::string& caption = NlTraitText::Caption(name);
					const std::string& about = NlTraitText::Hint(name);
					// 이름, 명칭, 설명 어디에든 들어 있으면 맞는다.
					if (!NlCore::TraitMatches(g_TraitFilter, name, caption) && !NlCore::TraitMatches(g_TraitFilter, "", about))
						continue;
					shown++;
					if (NlCore::Has(one.Traits, name))
						ImGui::TextDisabled("있음");
					else
					{
						ImGui::PushID(name.c_str());
						ImGui::BeginDisabled(NlCore::IsProtectedTrait(name));		// 종과 죽음의 특성은 붙이지 않는다
						if (ImGui::SmallButton("붙이기"))
							Push(PersonAct::TraitAdd, who, -1, 0, name);
						ImGui::EndDisabled();
						ImGui::PopID();
					}
					ImGui::SameLine();
					if (NlTraitText::Titled(name))
					{
						// 이름의 줄이 없는 특성: 설명의 제목을 흐리게 보인다(게임이 화면에 쓰는 이름과 다를 수 있다).
						ImGui::TextDisabled("%s", caption.c_str());
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("설명의 제목입니다 (게임 파일에 이 특성의 이름 줄이 없습니다)");
					}
					else
						ImGui::TextUnformatted(caption.empty() ? "-" : caption.c_str());
					ImGui::SameLine();
					ImGui::TextDisabled("(%s)", name.c_str());
					if (!about.empty())
					{
						ImGui::Indent();
						NlUi::Hint(about);		// 칸의 너비에서 줄을 바꾼다
						ImGui::Unindent();
					}
				}
				if (!shown)
					ImGui::TextDisabled("명칭, 게임의 이름, 설명 어디에도 그 글이 든 특성이 없습니다.");
			}
			ImGui::EndChild();
			ImGui::TextDisabled("%d개 (게임의 특성 %d개)", shown, static_cast<int>(NlTraitText::Names().size()));
		}
		NlUi::Hint(("명칭과 설명은 게임의 한국어 글입니다. 한국어가 비어 있는 것은 영어로 보입니다(" + NlTraitText::GetNotes().Names + ". " + NlTraitText::GetNotes().Hints + "). "
			"설명의 '(값)'은 게임의 글에 {…} 로 적혀 있는 자리입니다(게임이 화면에서 채워 넣는 자리로 보입니다). "
			"이름의 줄이 없는 특성은 설명의 제목을 흐린 글씨의 명칭으로 보이고(여러 특성이 한 설명을 함께 쓰면 같은 제목이 됩니다), 그것도 없으면 게임의 이름만 보입니다. "
			"종과 죽음의 특성(human, dead 같은 것)은 붙이거나 뗄 수 없습니다.").c_str());
	}

	size_t CountPlayers(bool Characters)
	{
		size_t count = 0;
		for (const PersonRow& row : g_Now.People)
			count += NlCore::IsPlayers(row) && row.Character == Characters;
		return count;
	}

	// 왼쪽의 사람 목록. 인물·지식·아이템 패널이 함께 쓴다(고른 사람도 함께 쓴다). 돌려주는 것: 고른 사람(없으면 nullptr).
	const PersonRow* DrawWho(bool LordsOnly)
	{
		ImGui::BeginChild("who", ImVec2(210, 0), ImGuiChildFlags_Borders);
		if (!LordsOnly)
			ImGui::Checkbox("주민·손님도 보기", &g_ShowAll);
		for (const PersonRow& row : g_Now.People)
		{
			if ((LordsOnly || !g_ShowAll) && !(NlCore::IsPlayers(row) && row.Character))
				continue;
			const std::string label = row.Name + "  " + KindText(row) + "##" + row.Uuid;
			ImGui::BeginDisabled(row.Dead);
			if (ImGui::Selectable(label.c_str(), g_Selected == row.Uuid))
				g_Selected = row.Uuid;
			ImGui::EndDisabled();
		}
		ImGui::EndChild();
		ImGui::SameLine();
		return g_Selected.empty() ? nullptr : FindRow(g_Selected);
	}

	// 고른 사람의 값을 아직 읽지 못했으면 참(그 글을 그린다).
	bool DetailPending(const PersonRow& Row)
	{
		ImGui::Text("%s  (%s)", Row.Name.c_str(), KindText(Row).c_str());
		if (g_Now.One.Ready && g_Now.One.Uuid == Row.Uuid)
			return false;
		ImGui::TextDisabled("값을 읽는 중입니다.");
		return true;
	}
}

void NlPeople::DrawPerson()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(false);
	ImGui::BeginChild("one", ImVec2(0, 0));
	if (!row)
		ImGui::TextDisabled("왼쪽에서 사람을 고르세요.");
	else
		DrawDetail(*row);
	DrawLast();
	ImGui::EndChild();
}

void NlPeople::DrawKnowledge()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(true);		// 지식은 영주가 가진다
	if (row && !(NlCore::IsPlayers(*row) && row->Character))
		row = nullptr;							// 다른 패널에서 주민이나 손님을 골라 둔 채 왔다
	ImGui::BeginChild("one", ImVec2(0, 0));
	NlUi::Hint("지식은 영주가 가집니다. 준 지식은 되돌릴 수 없고 세이브에 남습니다. 교과서 지식은 능력치도 올리고 재능과 별명이 붙을 수 있습니다. "
		"모든 지식을 가진 영주가 있으면 지식으로 잠겨 있던 건물(창고, 사원, 무기고에서 봤습니다)을 지을 수 있습니다.");
	if (!row)
		ImGui::TextDisabled("왼쪽에서 영주를 고르세요.");
	else if (!DetailPending(*row))
	{
		const Detail& one = g_Now.One;
		const std::string who = row->Uuid;
		ImGui::Text("가진 지식 %s / %d", NumberText(one.KnowledgeCount, 0).c_str(), static_cast<int>(g_Now.Knowledge.size()));
		ImGui::SameLine();
		if (ImGui::Button("이 영주에게 모든 지식 주기"))
			Push(PersonAct::KnowledgeAll, who);

		ImGui::SetNextItemWidth(160);
		ImGui::InputText("이름의 일부로 찾아 하나 주기", g_KnowledgeFilter, sizeof(g_KnowledgeFilter));
		if (g_KnowledgeFilter[0])
		{
			int shown = 0;
			for (const KnowledgeName& knowledge : g_Now.Knowledge)
			{
				if (knowledge.Name.empty() || (knowledge.Caption.find(g_KnowledgeFilter) == std::string::npos && knowledge.Name.find(g_KnowledgeFilter) == std::string::npos))
					continue;
				if (shown++ >= 14)
				{
					ImGui::TextDisabled("더 있습니다. 이름을 더 적어 주세요.");
					break;
				}
				ImGui::PushID(knowledge.Name.c_str());
				if (ImGui::SmallButton("주기"))
					Push(PersonAct::KnowledgeAdd, who, -1, 0, knowledge.Name);
				ImGui::PopID();
				ImGui::SameLine();
				ImGui::Text("%s  (%s, %s)", knowledge.Caption.c_str(), knowledge.Name.c_str(), knowledge.Category.c_str());
			}
			if (!shown)
				ImGui::TextDisabled("그런 이름의 지식이 없습니다(화면의 이름이나 게임의 영문 이름. 예: 광산, mine).");
		}
	}

	// 영주 전원에게 주는 것은 맨 아래에 따로, 두 단계로 둔다(한 번의 잘못된 클릭으로 모든 영주가 바뀌지 않게).
	ImGui::SeparatorText("영주 전원");
	ImGui::Checkbox("되돌릴 수 없다는 것을 압니다", &g_BulkKnowledgeArmed);
	ImGui::SameLine();
	ImGui::BeginDisabled(!g_BulkKnowledgeArmed);
	if (ImGui::Button("영주 전원에게 모든 지식 주기"))
	{
		Push(PersonAct::KnowledgeAll, "lords");
		g_BulkKnowledgeArmed = false;
	}
	ImGui::EndDisabled();
	DrawLast();
	ImGui::EndChild();
}

void NlPeople::DrawArmy()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;
	int soldiers = 0;
	for (const PersonRow& row : g_Now.People)
		soldiers += NlCore::IsPlayers(row) && !row.Character && !row.Dead && row.Strata == 2;		// 병사의 갈래는 2 다(research/13)
	ImGui::Text("플레이어의 병사 %d명", soldiers);
	for (const int amount : { 1, 5, 10 })
	{
		ImGui::SameLine();
		ImGui::PushID(amount);
		if (ImGui::Button(("+" + std::to_string(amount)).c_str()))
			g_SpawnQueued = (std::min)(g_SpawnQueued + amount, NlCore::k_SoldierBatchMax);		// windows.h 의 min 매크로를 피한다
		ImGui::PopID();
	}
	DrawSpawnHere({ NlCore::SpawnKind::Soldier, NlCore::SpawnKind::Knight });
	ImGui::Separator();
	ImGui::TextUnformatted("병사 전원에게 장비 지급 (선호 장비를 정하고 없는 장비를 넣는다)");
	bool first = true;
	for (const NlCore::Loadout& loadout : NlCore::Loadouts())
	{
		if (!first)
			ImGui::SameLine();
		first = false;
		if (ImGui::Button(loadout.Label))
			Push(PersonAct::Equip, "people", -1, 0, loadout.Key);
	}
	NlUi::Hint("플레이어의 병사(고용한 병사와 기사도)마다 게임의 '선호 장비'를 그 묶음으로 바꾸고, 그 갑옷·무기·방패 가운데 없는 것을 소지품에 하나씩 넣습니다(넣으면 바로 착용됩니다). "
		"'아무 장비나'는 선호만 바꾸고 장비는 넣지 않습니다. 앞의 선호 장비는 남기지 않으므로 되돌릴 수 없습니다. "
		"게임은 선호 장비에 없는 장비를 무기고로 돌려보내는 것으로 보입니다. 바꾼 선호 장비가 여덟 시간 넘게 남는 것까지 봤습니다.");
	NlUi::Hint("+1, +5, +10 은 게임의 디버그 함수로 병사를 만듭니다: 지도 가장자리에 나타나 마을로 걸어오고 게임의 군대 창에 전사로 올라옵니다(단검, 갑옷 없음). "
		"'마우스 자리에 소환'은 게임의 디버그 소환기를 부릅니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다(병사는 경갑과 창). "
		"병영의 정원과 임금은 따지지 않습니다(재지 않았습니다). 되돌릴 수 없고, 저장하면 세이브에 남을 것으로 보입니다.");
	DrawLast();
}

void NlPeople::DrawItems()
{
	std::lock_guard lock(g_Mutex);
	if (NotReady())
		return;

	const PersonRow* row = DrawWho(false);
	ImGui::BeginChild("one", ImVec2(0, 0));
	if (!row)
		ImGui::TextDisabled("왼쪽에서 사람을 고르세요.");
	else if (!DetailPending(*row))
	{
		const Detail& one = g_Now.One;
		const std::string who = row->Uuid;
		ImGui::Text("소지금 %s", one.Money == k_Unknown ? "-" : NlCore::Thousands(one.Money).c_str());
		for (const double amount : { 100.0, 1000.0, 10000.0, -100.0 })
		{
			ImGui::SameLine();
			ImGui::PushID(static_cast<int>(amount));
			if (ImGui::SmallButton((std::string(amount > 0 ? "+" : "") + NlCore::Thousands(amount)).c_str()))
				Push(PersonAct::MoneyAdd, who, -1, amount);
			ImGui::PopID();
		}
		NlUi::Hint("소지품과 소지금은 게임의 함수로 바꿉니다(게임의 인물 창에 보입니다). 세이브에 남습니다. 영지 창고의 자원은 '경제'에 있습니다. "
			"갑옷과 무기는 수만 바뀌고 착용은 바뀌지 않습니다. 착용 중인 것은 뺄 수 없습니다.");

		ImGui::SeparatorText("소지품");
		if (ImGui::BeginTable("items", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			// 0 번은 신성 반지다: 영주의 소지품 0 번 칸의 수를 게임의 character_runes_get_count 가 그대로 돌려준다(research/18).
			for (size_t i = 0; i < g_Now.Resources.size() && i < one.Items.size(); i++)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(g_Now.Resources[i].c_str());
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(NumberText(one.Items[i], 0).c_str());
				ImGui::TableNextColumn();
				ImGui::PushID(300 + static_cast<int>(i));
				for (const double amount : { 1.0, 10.0, 100.0 })
				{
					if (ImGui::SmallButton(("+" + NlCore::Fixed(amount, 0)).c_str()))
						Push(PersonAct::ItemAdd, who, static_cast<int>(i), amount);
					ImGui::SameLine();
				}
				ImGui::BeginDisabled(!(one.Items[i] > 0) || NlCore::IsEquipped(static_cast<int>(i), one.Equipped));		// 착용 중인 장비는 빼지 않는다
				if (ImGui::SmallButton("0 으로"))
					Push(PersonAct::ItemAdd, who, static_cast<int>(i), -NlCore::k_GiftMax);
				ImGui::EndDisabled();
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
	}
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
	NlUi::Hint("손님과 다른 진영의 영주에게는 가지 않습니다. 한 사람씩 고치려면 '인물'에서 고릅니다.");
	ImGui::Checkbox("되돌릴 수 없다는 것을 압니다##birth", &g_BulkBirthArmed);
	ImGui::SameLine();
	ImGui::BeginDisabled(!g_BulkBirthArmed);
	if (ImGui::Button("임신한 영주 모두 출산"))
	{
		Push(PersonAct::Birth, "lords");
		g_BulkBirthArmed = false;
	}
	ImGui::EndDisabled();
	NlUi::Hint("임신한 영주마다 게임의 다음 단계 함수를 출산까지 부릅니다. 게임의 확률을 그대로 타서 유산으로 끝날 수 있습니다('인구·욕구'의 '유산 없음'을 켜 두면 나지 않았습니다).");
	DrawSpawnLord();
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
	NlUi::Hint("계속 유지하려면 위의 항목을 켭니다. 인구는 '날마다 추가 이주민'에 수를 넣으면 다음 이주 때(저녁) 그만큼 더 옵니다.");
	DrawSpawnHere({ NlCore::SpawnKind::Peasant, NlCore::SpawnKind::Slave });
	NlUi::Hint("게임의 디버그 소환기로 플레이어의 주민이나 노예 하나를 바로 만듭니다: 단추를 누른 그 자리(모드창 아래의 지도)에 나타납니다. 집과 일자리는 따지지 않습니다.");
	DrawLast();
}

