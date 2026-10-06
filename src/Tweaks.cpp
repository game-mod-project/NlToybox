#include "Tweaks.hpp"

#include "Game.hpp"
#include "core/Knobs.hpp"
#include "core/Text.hpp"

#include <imgui.h>

#include <chrono>
#include <fstream>
#include <map>
#include <sstream>

using namespace YYTK;
using NlCore::ClampFactor;
using NlCore::Fixed;
using NlCore::Scale;
using Clock = std::chrono::steady_clock;

namespace
{
	enum class Target
	{
		BuildingCost,		// debug_params.json 의 building_resources: 건물 → [[자원, 수량], …]
		StartResources,		// debug_params.json 의 product_count: 자원 → 수량
		BookExp,			// 지식 가운데 교본의 upgrade_skill[n].value
		GameplayVar,		// global.__gameplay_vars 의 멤버 하나
	};

	struct Knob
	{
		const char* Id;
		const char* Label;
		const char* Help;
		Target What;
		const char* Var;			// GameplayVar 일 때 멤버 이름
		double Factor = 1;			// 창에서 정한 배율
		double Applied = 1;			// 마지막으로 써 넣은 배율
		int Written = -2;			// 마지막에 닿은 값의 수. -1 은 대상을 찾지 못했다, -2 는 아직 해 보지 않았다
		int Unstuck = 0;			// 그 가운데 써 넣었는데 다시 읽으니 그 값이 아니었던 것
	};

	Knob g_Knobs[] = {
		{ "building_cost", "건물 건설 비용", "건물을 지을 때 드는 자원의 양", Target::BuildingCost, nullptr },
		{ "start_resources", "시작 자원", "새 게임을 시작할 때 주어지는 자원(나무, 당근, 약).\n새 게임을 시작하기 전에 정한다", Target::StartResources, nullptr },
		{ "book_exp", "교본의 능력치 경험", "교본을 읽어 얻는 능력치 경험", Target::BookExp, nullptr },
		{ "bribe_cost", "뇌물 비용", nullptr, Target::GameplayVar, "bribe_give_rings" },
		{ "free_lord_stay", "자유 영주 체류 기간", nullptr, Target::GameplayVar, "free_lord_stay_duration" },
		{ "church_capacity", "교회 수용 인원", nullptr, Target::GameplayVar, "church_max_capacity" },
		{ "tavern_capacity", "선술집 수용 인원", nullptr, Target::GameplayVar, "tavern_max_capacity" },
	};

	// ds 형 상수. 이 러너에서 맞는 것을 만들고 지워서 확인했다(research/02).

	std::filesystem::path g_SettingsPath;
	std::function<void(const std::string&)> g_Log;
	bool g_TestMode = false;

	std::map<std::string, double> g_Base;		// 대상의 원래 값. 처음 본 때의 값이다
	double g_DebugRoot = -1;					// debug_params.json 을 읽은 ds_map
	double g_Knowledge = -1;					// 지식을 담은 ds_map
	bool g_Dirty = false;						// 배율이 바뀌었다. 다음 틱에 바로 써 넣는다
	Clock::time_point g_NextApply, g_SaveAt;
	bool g_SavePending = false;
	int g_Unstuck = 0;							// 이번 적용에서 써 넣은 값이 남지 않은 수

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	bool Exists(double Id, double Type)
	{
		return NlGame::CallNumber("ds_exists", { RValue(Id), RValue(Type) }, 0) > 0;
	}

	bool MapHas(double Map, const char* Key)
	{
		return NlGame::CallNumber("ds_map_exists", { RValue(Map), RValue(Key) }, 0) > 0;
	}

	// 구조체의 멤버. 없으면 거짓.
	bool Member(const RValue& Struct, const char* Name, RValue& Out)
	{
		if (!Struct.IsStruct() || NlGame::CallNumber("variable_struct_exists", { Struct, RValue(Name) }, 0) <= 0)
			return false;
		return NlGame::Call("variable_struct_get", { Struct, RValue(Name) }, Out);
	}

	// 데이터 파일을 읽은 ds_map 을 키 이름으로 찾는다. 번호는 실행마다 같았지만 기대지 않는다(research/02).
	void Locate()
	{
		if (g_DebugRoot >= 0 && !(Exists(g_DebugRoot, NlGame::k_DsMap) && MapHas(g_DebugRoot, "building_resources")))
			g_DebugRoot = -1;
		if (g_Knowledge >= 0 && !(Exists(g_Knowledge, NlGame::k_DsMap) && MapHas(g_Knowledge, "skill_combat_1")))
			g_Knowledge = -1;
		if (g_DebugRoot >= 0 && g_Knowledge >= 0)
			return;

		int missing = 0;
		for (int id = 0; id < NlGame::k_MaxDsId && missing < NlGame::k_MaxMissing && (g_DebugRoot < 0 || g_Knowledge < 0); id++)
		{
			if (!Exists(id, NlGame::k_DsMap))
			{
				missing++;
				continue;
			}
			missing = 0;
			if (g_DebugRoot < 0 && MapHas(id, "building_resources") && MapHas(id, "product_count"))
			{
				g_DebugRoot = id;
				Log("tweak found debug_params map " + std::to_string(id));
			}
			if (g_Knowledge < 0 && MapHas(id, "skill_combat_1"))
			{
				RValue entry;
				if (NlGame::Call("ds_map_find_value", { RValue(static_cast<double>(id)), RValue("skill_combat_1") }, entry) && entry.IsStruct())
				{
					g_Knowledge = id;
					Log("tweak found knowledge map " + std::to_string(id));
				}
			}
		}
	}

	// 바탕 값을 기억해 두고(처음 본 값), 배율을 곱한 값을 돌려준다.
	double Wanted(const std::string& Key, double Current, double Factor)
	{
		const auto [it, inserted] = g_Base.try_emplace(Key, Current);
		return Scale(it->second, Factor);
	}

	// ds_map 의 키들.
	std::vector<RValue> Keys(double Map)
	{
		RValue keys;
		if (!NlGame::Call("ds_map_keys_to_array", { RValue(Map) }, keys) || !keys.IsArray())
			return {};
		return keys.ToVector();
	}

	int ApplyBuildingCost(double Factor)
	{
		RValue buildings;
		if (g_DebugRoot < 0 || !NlGame::Call("ds_map_find_value", { RValue(g_DebugRoot), RValue("building_resources") }, buildings)
			|| !NlGame::IsNumber(buildings) || !Exists(buildings.ToDouble(), NlGame::k_DsMap))
			return -1;

		int written = 0;
		for (const RValue& key : Keys(buildings.ToDouble()))
		{
			RValue list;
			if (!key.IsString() || !NlGame::Call("ds_map_find_value", { buildings, key }, list) || !NlGame::IsNumber(list)
				|| !Exists(list.ToDouble(), NlGame::k_DsList))
				continue;
			const int count = static_cast<int>(NlGame::CallNumber("ds_list_size", { list }, 0));
			for (int i = 0; i < count; i++)
			{
				// 원소는 [자원 이름, 수량] 의 ds_list 다.
				RValue pair, amount;
				if (!NlGame::Call("ds_list_find_value", { list, RValue(static_cast<double>(i)) }, pair) || !NlGame::IsNumber(pair)
					|| !Exists(pair.ToDouble(), NlGame::k_DsList) || NlGame::CallNumber("ds_list_size", { pair }, 0) < 2)
					continue;
				if (!NlGame::Call("ds_list_find_value", { pair, RValue(1.0) }, amount) || !NlGame::IsNumber(amount))
					continue;
				const double wanted = Wanted("building/" + key.ToString() + "/" + std::to_string(i), amount.ToDouble(), Factor);
				if (wanted != amount.ToDouble())
				{
					RValue ignored;
					NlGame::Call("ds_list_replace", { pair, RValue(1.0), RValue(wanted) }, ignored);
					g_Unstuck += NlGame::CallNumber("ds_list_find_value", { pair, RValue(1.0) }, wanted - 1) != wanted;
				}
				written++;
			}
		}
		return written;
	}

	int ApplyStartResources(double Factor)
	{
		RValue counts;
		if (g_DebugRoot < 0 || !NlGame::Call("ds_map_find_value", { RValue(g_DebugRoot), RValue("product_count") }, counts)
			|| !NlGame::IsNumber(counts) || !Exists(counts.ToDouble(), NlGame::k_DsMap))
			return -1;

		int written = 0;
		for (const RValue& key : Keys(counts.ToDouble()))
		{
			RValue amount;
			if (!key.IsString() || !NlGame::Call("ds_map_find_value", { counts, key }, amount) || !NlGame::IsNumber(amount))
				continue;
			const double wanted = Wanted("start/" + key.ToString(), amount.ToDouble(), Factor);
			if (wanted != amount.ToDouble())
			{
				RValue ignored;
				NlGame::Call("ds_map_replace", { counts, key, RValue(wanted) }, ignored);
				g_Unstuck += NlGame::CallNumber("ds_map_find_value", { counts, key }, wanted - 1) != wanted;
			}
			written++;
		}
		return written;
	}

	int ApplyBookExp(double Factor)
	{
		if (g_Knowledge < 0)
			return -1;

		int written = 0;
		for (const RValue& key : Keys(g_Knowledge))
		{
			RValue entry, category, skills;
			if (!key.IsString() || !NlGame::Call("ds_map_find_value", { RValue(g_Knowledge), key }, entry) || !entry.IsStruct())
				continue;
			if (!Member(entry, "__category", category) || !category.IsString() || category.ToString() != "textbooks")
				continue;
			if (!Member(entry, "__upgrade_skill", skills) || !skills.IsArray())
				continue;

			const int count = static_cast<int>(NlGame::ArrayLength(skills));
			for (int i = 0; i < count; i++)
			{
				RValue skill, value;
				if (!NlGame::Call("array_get", { skills, RValue(static_cast<double>(i)) }, skill) || !Member(skill, "value", value)
					|| !NlGame::IsNumber(value))
					continue;
				const double wanted = Wanted("book/" + key.ToString() + "/" + std::to_string(i), value.ToDouble(), Factor);
				if (wanted != value.ToDouble())
				{
					RValue ignored, again;
					NlGame::Call("variable_struct_set", { skill, RValue("value"), RValue(wanted) }, ignored);
					g_Unstuck += !(Member(skill, "value", again) && NlGame::IsNumber(again) && again.ToDouble() == wanted);
				}
				written++;
			}
		}
		return written;
	}

	// Vars: global.__gameplay_vars. 틱마다 한 번 찾아 넘겨받는다(전역 4,700여 개를 열거해야 찾는다).
	int ApplyGameplayVar(const RValue& Vars, const char* Name, double Factor)
	{
		const RValue& vars = Vars;
		RValue value;
		if (!Member(vars, Name, value) || !NlGame::IsNumber(value))
			return -1;
		const double wanted = Wanted(std::string("var/") + Name, value.ToDouble(), Factor);
		if (wanted != value.ToDouble())
		{
			RValue ignored, again;
			NlGame::Call("variable_struct_set", { vars, RValue(Name), RValue(wanted) }, ignored);
			g_Unstuck += !(Member(vars, Name, again) && NlGame::IsNumber(again) && again.ToDouble() == wanted);
		}
		return 1;
	}

	int Apply(const Knob& K, const RValue& Vars)
	{
		switch (K.What)
		{
		case Target::BuildingCost: return ApplyBuildingCost(K.Factor);
		case Target::StartResources: return ApplyStartResources(K.Factor);
		case Target::BookExp: return ApplyBookExp(K.Factor);
		case Target::GameplayVar: return ApplyGameplayVar(Vars, K.Var, K.Factor);
		}
		return -1;
	}

	void Save()
	{
		if (g_TestMode)
			return;
		std::map<std::string, double> values;
		for (const Knob& knob : g_Knobs)
			values[knob.Id] = knob.Factor;
		std::ofstream out(g_SettingsPath, std::ios::trunc);
		out << NlCore::FormatSettings(values);
	}

	void Changed()
	{
		g_Dirty = true;
		g_SavePending = true;
		g_SaveAt = Clock::now() + std::chrono::milliseconds(800);
	}
}

void NlTweaks::Init(const std::filesystem::path& ModuleDir, std::function<void(const std::string&)> Log_,
	const std::vector<std::string>& TestSets)
{
	g_SettingsPath = ModuleDir / "NlToyBox.settings.txt";
	g_Log = std::move(Log_);
	g_NextApply = Clock::now();

	std::map<std::string, double> values;
	if (std::ifstream in(g_SettingsPath); in)
		values = NlCore::ParseSettings(in);
	for (const std::string& set : TestSets)
	{
		const size_t colon = set.find(':');
		double factor = 1;
		if (colon != std::string::npos && NlCore::ParseNumber(set.substr(colon + 1), factor))
		{
			values[set.substr(0, colon)] = ClampFactor(factor);
			g_TestMode = true;
		}
	}

	std::string loaded;
	for (Knob& knob : g_Knobs)
	{
		const auto it = values.find(knob.Id);
		if (it == values.end())
			continue;
		knob.Factor = it->second;
		if (knob.Factor != 1)
			loaded += std::string(" ") + knob.Id + "=" + Fixed(knob.Factor, 2);
	}
	g_Dirty = !loaded.empty();
	Log(std::string("tweak settings") + (g_TestMode ? " (test, not saved):" : ":") + (loaded.empty() ? " all 1.00" : loaded));
}

void NlTweaks::GameTick()
{
	const Clock::time_point now = Clock::now();
	if (g_SavePending && now >= g_SaveAt)
	{
		g_SavePending = false;
		Save();
	}
	if (!g_Dirty && now < g_NextApply)
		return;
	g_NextApply = now + std::chrono::seconds(2);		// 게임이 값을 다시 만들면(새 게임 등) 다시 써 넣는다
	g_Dirty = false;

	// 배율이 모두 1 이고 건드린 적도 없으면 게임의 값에 손대지 않는다.
	bool needed = false;
	for (const Knob& knob : g_Knobs)
		needed = needed || knob.Factor != 1 || knob.Applied != 1;
	if (!needed)
		return;

	Locate();
	RValue vars;		// 못 찾으면 undefined 로 남고, 그 항목은 "못 찾음"이 된다
	NlGame::Resolve("global.__gameplay_vars", vars);
	for (Knob& knob : g_Knobs)
	{
		if (knob.Factor == 1 && knob.Applied == 1)
			continue;
		g_Unstuck = 0;
		const int written = Apply(knob, vars);
		// 같은 결과를 되풀이해 적지 않는다(데이터가 읽히기 전에는 2초마다 "못 찾음"이다).
		if (written != knob.Written || g_Unstuck != knob.Unstuck || (written >= 0 && knob.Applied != knob.Factor))
			Log(std::string("tweak ") + knob.Id + " x" + Fixed(knob.Factor, 2) + ": "
				+ (written < 0 ? "target not found" : std::to_string(written) + " values, " + std::to_string(g_Unstuck) + " did not stick"));
		knob.Written = written;
		knob.Unstuck = g_Unstuck;
		if (written >= 0)
			knob.Applied = knob.Factor;
	}
}

void NlTweaks::DrawArea(NlCore::Area Where)
{
	if (!NlCore::HasKnobs(Where))
		return;
	ImGui::Spacing();
	ImGui::SeparatorText("배율 (1.00 이 원래 값. 바꾸면 바로 게임의 값에 써 넣습니다)");

	for (Knob& knob : g_Knobs)
	{
		if (NlCore::KnobArea(knob.Id) != Where)
			continue;
		ImGui::PushID(knob.Id);
		float factor = static_cast<float>(knob.Factor);
		ImGui::SetNextItemWidth(210);
		if (ImGui::SliderFloat("##factor", &factor, 0.1f, 5.0f, "x %.2f"))
		{
			knob.Factor = ClampFactor(factor);
			Changed();
		}
		ImGui::SameLine();
		if (ImGui::Button("1.0"))
		{
			knob.Factor = 1;
			Changed();
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(knob.Label);
		if (knob.Help && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", knob.Help);

		if (knob.Factor != 1 || knob.Applied != 1)
		{
			ImGui::SameLine();
			if (knob.Written < 0)
				ImGui::TextDisabled("(아직 게임 데이터를 찾지 못함)");
			else if (knob.Unstuck > 0)
				ImGui::TextDisabled("(값 %d개 가운데 %d개가 써지지 않음)", knob.Written, knob.Unstuck);
			else if (knob.Applied == knob.Factor)
				ImGui::TextDisabled("(값 %d개에 써 넣음)", knob.Written);
			else
				ImGui::TextDisabled("(써 넣는 중)");
		}
		ImGui::PopID();
	}

	ImGui::TextDisabled(g_TestMode ? "시험 중: 저장하지 않음" : "슬라이더를 Ctrl+클릭하면 수를 직접 넣을 수 있습니다. 써 넣은 값을 게임이 따르는지는 항목마다 다릅니다");
}
