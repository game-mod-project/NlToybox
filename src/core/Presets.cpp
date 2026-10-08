#include "Presets.hpp"

#include "CheatTable.hpp"

namespace NlCore
{
	const std::vector<Preset>& Presets()
	{
		// 표의 항목 가운데 플레이에서 확인된 것만 쓴다(CheckPreset 과 시험이 지킨다).
		static const std::vector<Preset> presets = {
			{ "normal", "기본", "치트 표의 항목을 모두 끈다(탐색기의 잠금과 배율 7개는 그대로)", {} },		// 단추의 너비에 들어가는 이름(긴 이름은 잘렸다)
			{ "easy", "쉬움", "조금 빠르게 만들고 배고픔이 없다", {
				{ "production_time", 0.5 }, { "worker_performance", 2 }, { "storage_capacity", 2 }, { "hire_cost", 0.5 }, { "no_hunger", 0 } } },
			// 정원 배율 둘과 좋은 효과의 범위 배율은 플레이에서 잰 배율(x2)로 넣는다(research/32. 2026-10-08 에 사용자가 넣기로 했다).
			// 범위 배율은 건물이 만들어질 때 먹는다: 묶음을 건 뒤 세이브를 불러와야 이미 지은 건물에 보인다(패널의 위쪽 글이 알린다).
			// 설명은 단추 옆의 한 줄이다(길면 창의 오른쪽에서 잘린다). 든 항목은 패널이 그 아래에 늘어놓는다.
			{ "sandbox", "샌드박스", "짓고 만드는 데 걸림돌이 없고 집과 병영이 넓다", {
				{ "build_any", 0 }, { "build_free", 0 }, { "instant_upgrade", 0 }, { "instant_build", 0 },
				{ "production_time", 0.1 }, { "worker_performance", 5 }, { "production_amount", 2 }, { "storage_capacity", 10 }, { "hire_cost", 0.1 },
				{ "housing_capacity", 2 }, { "barrack_capacity", 2 }, { "effect_range", 2 } } },
			{ "god", "신", "샌드박스에서 생산량을 x5 로 올리고 사람들의 욕구와 기분을 채워 두고, 싸울 때 아군의 전투 기술을 두 배로 적의 것을 절반으로 하고, 유산이 없다", {
				{ "build_any", 0 }, { "build_free", 0 }, { "instant_upgrade", 0 }, { "instant_build", 0 },
				{ "production_time", 0.1 }, { "worker_performance", 5 }, { "production_amount", 5 }, { "storage_capacity", 10 }, { "hire_cost", 0.1 },
				{ "housing_capacity", 2 }, { "barrack_capacity", 2 }, { "effect_range", 2 },
				{ "needs_full", 0 }, { "always_happy", 0 }, { "ally_power", 2 }, { "enemy_power", 0.5 }, { "no_miscarriage", 0 } } },
		};
		return presets;
	}

	const Preset* FindPreset(const std::string& Key)
	{
		for (const Preset& preset : Presets())
			if (Key == preset.Key)
				return &preset;
		return nullptr;
	}

	const Preset* MatchPreset(const std::vector<CheatOn>& On)
	{
		for (const Preset& preset : Presets())
		{
			if (preset.Items.size() != On.size())
				continue;
			bool same = true;
			for (const PresetItem& item : preset.Items)
			{
				const Cheat* cheat = FindCheat(item.Id);
				bool found = false;
				for (const CheatOn& on : On)
					if (on.Id == item.Id)
						found = !cheat || !HasNumber(cheat->Kind) || on.Number == item.Number;
				same = same && found;
			}
			if (same)
				return &preset;
		}
		return nullptr;
	}

	bool CheckPreset(const Preset& Preset, std::string& Why)
	{
		for (size_t i = 0; i < Preset.Items.size(); i++)
		{
			const PresetItem& item = Preset.Items[i];
			const std::string id = item.Id ? item.Id : "";
			const Cheat* cheat = FindCheat(id);
			if (!cheat)
				return Why = id + ": not in the table", false;
			if (!cheat->Verified)
				return Why = id + ": not verified in play", false;
			if (cheat->Kind == CheatKind::Number)
				return Why = id + ": writes a value (not for presets)", false;
			for (size_t j = 0; j < i; j++)
				if (id == Preset.Items[j].Id)
					return Why = id + ": listed twice", false;
			if (HasNumber(cheat->Kind))
			{
				if (!(item.Number >= cheat->Min && item.Number <= cheat->Max) || !(item.Number > 0))
					return Why = id + ": factor out of range", false;
			}
			else if (item.Number != 0)
				return Why = id + ": takes no number", false;
		}
		return true;
	}
}
