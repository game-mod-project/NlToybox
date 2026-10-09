#include "MapPlan.hpp"

#include "Text.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace NlCore
{
	namespace
	{
		using G = MapGroup;
		// research/31. 지형의 범위는 창의 단계 수(호수·산 6단계, 언덕 4단계, 언덕 배치 3, 강 2). 막힘·자원은 범위 미확인(본 값 0~5) — 0~9 로 둔다.
		const std::vector<MapKnob> k_Knobs = {
			{ "lakes", "__lakes", "호수", G::Terrain, 0, 5, true },
			{ "hills", "__hills", "언덕", G::Terrain, 0, 3, true },
			{ "hills_distribution", "__hills_distribution", "언덕 배치", G::Terrain, 0, 2, true },
			{ "mountains", "__mountains", "산", G::Terrain, 0, 5, true },
			{ "river", "__river", "강", G::Terrain, 0, 1, true },
			{ "blocked_up", "__blocked_up", "막힘 위", G::Blocked, 0, 9, false },
			{ "blocked_down", "__blocked_down", "막힘 아래", G::Blocked, 0, 9, false },
			{ "blocked_left", "__blocked_left", "막힘 왼쪽", G::Blocked, 0, 9, false },
			{ "blocked_right", "__blocked_right", "막힘 오른쪽", G::Blocked, 0, 9, false },
			{ "berry", "__berry", "열매", G::Resource, 0, 9, true },
			{ "bush", "__bush", "덤불", G::Resource, 0, 9, true },
			{ "clay", "__clay", "점토", G::Resource, 0, 9, true },
			{ "fertile", "__fertile", "비옥지", G::Resource, 0, 9, true },
			{ "hop", "__hop", "홉", G::Resource, 0, 9, true },
			{ "iron", "__iron", "철", G::Resource, 0, 9, true },
			{ "plants", "__plants", "식물", G::Resource, 0, 9, true },
			{ "tree", "__tree", "나무", G::Resource, 0, 9, true },
		};
		constexpr const char* k_NotScreen = "새 게임의 지도 화면(영주관 배치 전)에서만 됩니다";
	}

	const std::vector<MapKnob>& MapKnobs()
	{
		return k_Knobs;
	}

	const MapKnob* FindMapKnob(const std::string& Key)
	{
		for (const MapKnob& knob : k_Knobs)
			if (Key == knob.Key)
				return &knob;
		return nullptr;
	}

	double ClampMapValue(const MapKnob& Knob, double Value)
	{
		if (!std::isfinite(Value))
			return Knob.Min;
		const double whole = std::floor(Value);
		return whole < Knob.Min ? Knob.Min : whole > Knob.Max ? Knob.Max : whole;
	}

	const char* MapGroupWord(MapGroup Group)
	{
		switch (Group)
		{
		case MapGroup::Terrain: return "지형";
		case MapGroup::Blocked: return "막힘";
		default: return "자원";
		}
	}

	std::string HillsDistributionWord(double Value)
	{
		if (Value == 0) return "중심";
		if (Value == 1) return "가장자리";
		if (Value == 2) return "무작위";
		return "?";
	}

	std::string MapFieldPath(const MapKnob& Knob)
	{
		return std::string(k_MapSettingsPath) + "." + Knob.Field;
	}

	std::string MapStashedPath(const MapKnob& Knob)
	{
		return std::string(k_MapSettingsPath) + ".__stashed_settings." + Knob.Key;
	}

	bool MapScreen(bool InitializerActive, bool SettingsRead, bool PreviewRead, bool InGame)
	{
		return InitializerActive && SettingsRead && PreviewRead && !InGame;
	}

	bool ValidSeed(double Value)
	{
		return std::isfinite(Value) && Value >= 0 && std::floor(Value) == Value;
	}

	bool SeedHookVerified()
	{
		return false;		// research/31: 생성기가 get_generator_seed() 를 읽는지 못 봤다. 본 뒤에 참으로
	}

	std::string MapReport(MapAct Act, char Outcome, const std::string& Detail)
	{
		if (Outcome == 'n')
			return k_NotScreen;
		switch (Act)
		{
		case MapAct::Set:
			switch (Outcome)
			{
			case 'w': return Detail + " 을 썼습니다";
			case 'k': return "모르는 열쇠: " + Detail;
			default: return "쓰지 못했습니다: " + Detail;
			}
		case MapAct::Regenerate:
			return Outcome == 'd' ? "다시 생성했습니다 (regenerate_map)" : "다시 생성을 부르지 못했습니다 (" + Detail + ")";
		case MapAct::Restore:
			return Outcome == 'd' ? "원래 값으로 되돌렸습니다 (" + Detail + "개)" : "원래 값을 읽거나 쓰지 못한 칸이 있습니다 (" + Detail + "개)";
		case MapAct::PresetSave:
			switch (Outcome)
			{
			case 's': return "프리셋을 저장했습니다: " + Detail;
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "프리셋 파일에 쓰지 못했습니다";
			}
		case MapAct::PresetLoad:
			switch (Outcome)
			{
			case 'l': return "프리셋을 불러왔습니다: " + Detail + " - 값만 채웠습니다. 생성은 '다시 생성'으로";
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "그 이름의 프리셋이 없습니다: " + Detail;
			}
		case MapAct::PresetDelete:
			switch (Outcome)
			{
			case 'x': return "프리셋을 지웠습니다: " + Detail;
			case 'b': return "프리셋의 이름이 틀렸습니다 (비지 않고 32자 안, 빈칸과 = 없이)";
			default: return "그 이름의 프리셋이 없습니다: " + Detail;
			}
		case MapAct::Seed:
			switch (Outcome)
			{
			case 'd': return "씨앗을 고정했습니다: " + Detail;
			case 'r': return "씨앗 고정을 풀었습니다 (무작위)";
			case 'f': return "씨앗 고정을 걸지 못했습니다 (" + Detail + ")";
			default: return "씨앗 고정은 확인 전입니다 (research/31)";
			}
		default:
			return std::string();
		}
	}

	bool GoodPresetName(const std::string& Name)
	{
		if (Name.empty() || Name.size() > 32)
			return false;
		for (const unsigned char c : Name)
			if (c == ' ' || c == '=' || c < 0x20 || c == 0x7f)
				return false;
		return true;
	}

	namespace
	{
		// "열쇠=수" 를 읽는다. 모르는 열쇠나 수가 아닌 값은 거짓(Why). 막힘 열쇠와 seed 는 Out 에 넣지 않고 참(Skip).
		bool ReadPair(const std::string& Word, MapPreset& Out, bool& Skip, std::string& Why)
		{
			Skip = false;
			const size_t eq = Word.find('=');
			if (eq == std::string::npos || eq == 0)
			{
				Why = "expected key=value: " + Word;
				return false;
			}
			const std::string key = Word.substr(0, eq), text = Word.substr(eq + 1);
			double value = 0;
			if (!ParseNumber(text, value))
			{
				Why = "not a number: " + Word;
				return false;
			}
			if (key == "seed")
			{
				Out.Seed = ValidSeed(value) ? value : -1;		// 틀린 씨앗은 무작위로(줄은 버리지 않는다)
				Skip = true;
				return true;
			}
			const MapKnob* knob = FindMapKnob(key);
			if (!knob)
			{
				Why = "unknown key: " + key;
				return false;
			}
			if (!knob->InPreset)
			{
				Skip = true;		// 막힘은 프리셋에 들지 않는다
				return true;
			}
			Out.Values[key] = ClampMapValue(*knob, value);
			return true;
		}
	}

	std::vector<MapPreset> ParseMapPresets(std::istream& In)
	{
		std::vector<MapPreset> out;
		std::string line;
		while (std::getline(In, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();
			std::istringstream words(line);
			std::string head, name;
			if (!(words >> head) || head != "preset" || !(words >> name) || !GoodPresetName(name))
				continue;
			MapPreset preset;
			preset.Name = name;
			bool ok = true;
			std::string word, why;
			while (ok && words >> word)
			{
				bool skip = false;
				ok = ReadPair(word, preset, skip, why);
			}
			if (ok)
				UpsertPreset(out, std::move(preset));
		}
		return out;
	}

	std::string FormatMapPresets(const std::vector<MapPreset>& Presets)
	{
		std::vector<const MapPreset*> sorted;
		for (const MapPreset& preset : Presets)
			sorted.push_back(&preset);
		std::sort(sorted.begin(), sorted.end(), [](const MapPreset* A, const MapPreset* B) { return A->Name < B->Name; });
		std::string out = "# NlToyBox 의 지도 프리셋. 지도 탭에서 저장하면 여기에 쓰인다. 줄: preset <이름> <열쇠>=<수> … seed=<수>(-1 은 무작위).\n";
		for (const MapPreset* preset : sorted)
		{
			out += "preset " + preset->Name;
			for (const MapKnob& knob : MapKnobs())
			{
				const auto found = preset->Values.find(knob.Key);
				if (knob.InPreset && found != preset->Values.end())
					out += std::string(" ") + knob.Key + "=" + Shortest(found->second);
			}
			out += " seed=" + Shortest(preset->Seed) + "\n";
		}
		return out;
	}

	void UpsertPreset(std::vector<MapPreset>& Presets, MapPreset Preset)
	{
		for (auto it = Preset.Values.begin(); it != Preset.Values.end();)
		{
			const MapKnob* knob = FindMapKnob(it->first);
			if (!knob || !knob->InPreset)
				it = Preset.Values.erase(it);
			else
			{
				it->second = ClampMapValue(*knob, it->second);
				++it;
			}
		}
		for (MapPreset& have : Presets)
			if (have.Name == Preset.Name)
			{
				have = std::move(Preset);
				return;
			}
		Presets.push_back(std::move(Preset));
	}

	bool ErasePreset(std::vector<MapPreset>& Presets, const std::string& Name)
	{
		for (auto it = Presets.begin(); it != Presets.end(); ++it)
			if (it->Name == Name)
			{
				Presets.erase(it);
				return true;
			}
		return false;
	}

	const MapPreset* FindPreset(const std::vector<MapPreset>& Presets, const std::string& Name)
	{
		for (const MapPreset& preset : Presets)
			if (preset.Name == Name)
				return &preset;
		return nullptr;
	}

	bool MapCommandFromParts(const std::string& Target, const std::map<std::string, std::string>& Options, MapCommand& Out, std::string& Why)
	{
		Out = MapCommand{};
		if (Target == "show") { Out.Act = MapAct::Show; return true; }
		if (Target == "regenerate") { Out.Act = MapAct::Regenerate; return true; }
		if (Target == "restore") { Out.Act = MapAct::Restore; return true; }
		if (Target == "set")
		{
			Out.Act = MapAct::Set;
			for (const auto& [key, text] : Options)
			{
				const MapKnob* knob = FindMapKnob(key);
				double value = 0;
				if (!knob)
				{
					Why = "map set: unknown key " + key + " (keys: lakes, hills, hills_distribution, mountains, river, blocked_up/down/left/right, berry, bush, clay, fertile, hop, iron, plants, tree)";
					return false;
				}
				if (!ParseNumber(text, value))
				{
					Why = "map set: not a number: " + key + "=" + text;
					return false;
				}
				Out.Sets.emplace_back(key, ClampMapValue(*knob, value));
			}
			if (Out.Sets.empty())
			{
				Why = "map set needs <key>=<number> …";
				return false;
			}
			return true;
		}
		if (Target == "preset")
		{
			const auto op = Options.find("op"), name = Options.find("name");
			if (op == Options.end() || name == Options.end())
			{
				Why = "map preset needs save|load|delete name=<name>";
				return false;
			}
			if (op->second == "save") Out.Act = MapAct::PresetSave;
			else if (op->second == "load") Out.Act = MapAct::PresetLoad;
			else if (op->second == "delete") Out.Act = MapAct::PresetDelete;
			else
			{
				Why = "map preset needs save|load|delete";
				return false;
			}
			if (!GoodPresetName(name->second))
			{
				Why = "map preset: bad name (1-32 bytes, no space or =)";
				return false;
			}
			Out.Name = name->second;
			return true;
		}
		if (Target == "seed")
		{
			const auto value = Options.find("value");
			if (value == Options.end())
			{
				Why = "map seed needs <number|random>";
				return false;
			}
			Out.Act = MapAct::Seed;
			if (value->second == "random")
			{
				Out.SeedRandom = true;
				return true;
			}
			double seed = 0;
			if (!ParseNumber(value->second, seed))
			{
				Why = "map seed: not a number: " + value->second;
				return false;
			}
			if (!ValidSeed(seed))
			{
				Why = "map seed: 0 이상의 정수만 (random 은 무작위): " + value->second;
				return false;
			}
			Out.Seed = seed;
			return true;
		}
		Why = "map needs one of: show, set, regenerate, restore, preset, seed";
		return false;
	}
}
