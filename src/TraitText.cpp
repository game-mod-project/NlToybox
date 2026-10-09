#include "TraitText.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/Localization.hpp"
#include "core/PeoplePlan.hpp"
#include "core/RolePlan.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <mutex>
#include <unordered_map>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;

namespace
{
	std::recursive_mutex g_Mutex;		// 그리기(툴팁, 특성 목록)에서도 읽는다
	NlTraitText::LogFn g_Log;
	const std::string k_NoText;		// 없는 글을 참조로 돌려줄 때(RValue 가 아니라 정적으로 둬도 된다)
	std::vector<std::string> g_Names;		// 게임에 있는 특성의 이름(이름순. 이분 탐색에 쓴다)
	std::vector<std::string> g_Shown;		// 같은 이름들을 창에 보일 차례로(화면 이름이 있는 것을 그 이름순으로 먼저. core 의 TraitBefore)
	constexpr const char* k_TraitList = "inst:o_data.game_trait_list";		// 특성 이름 282개
	// 특성의 화면 이름(게임의 이름 -> 한국어 이름). 시작할 때 게임의 localization\main.csv 에서 읽는다(열쇠 "trait.<이름>", Korean 칸. 비면 English 칸).
	std::unordered_map<std::string, std::string> g_TraitCaptions;
	std::string g_TraitTextNote = "특성의 이름을 아직 읽지 않았습니다";		// 어디서 몇 개를 읽었는가, 또는 못 읽은 까닭
	// 힌트의 글(열쇠 -> 한국어 글. 비면 영어). 시작할 때 게임의 힌트 파일 셋에서 읽는다. 특성의 설명은 이 가운데 게임이 알려 준 열쇠의 것이다.
	std::unordered_map<std::string, std::string> g_Hints;
	// 게임의 속성 함수: (특성의 이름, 번호) -> 값. 게임이 (글, 정수)로 부르는 것을 기록했고 281개에 불러 봤다(research/20). 번호의 뜻은 core/Localization.
	constexpr const char* k_TraitProperty = "gml_Script_trait_property_get";
	struct TraitText
	{
		std::string Caption;	// 화면 이름(main.csv 의 것. 없으면 힌트의 제목)
		std::string Hint;		// 설명(힌트의 본문을 다듬은 것). 없으면 빈 글
		bool FromTitle = false;	// Caption 이 힌트의 제목에서 왔다(게임 파일에 이 특성의 이름 줄이 없다. 여러 특성이 한 힌트를 함께 쓰면 같은 제목이 된다)
	};
	std::unordered_map<std::string, TraitText> g_TraitTexts;		// 게임의 이름 -> 보일 글. 게임을 불러올 때마다 채운다(LoadTraitNames)
	std::string g_TraitHintNote = "설명을 아직 읽지 않았습니다";		// 설명이 몇 개 붙었는가, 또는 붙이지 않은 까닭(게임을 불러올 때마다 다시 적는다)
	std::string g_HintFilesNote;		// 힌트 파일을 읽은 결과(시작할 때 한 번)
	// 능력치의 화면 이름(능력치의 열쇠 -> 게임의 한국어 이름. 비면 영어). 시작할 때 main.csv 에서 읽는다(열쇠 "actor.skill.<열쇠>").
	// 읽지 못한 것은 여기 없고 그때는 모듈의 이름(SkillNames)을 보인다.
	std::unordered_map<std::string, std::string> g_SkillCaptions;
	// 문화의 화면 이름(게임의 이름 → 글). 열쇠는 게임의 문화 구조체의 __caption 에서 본 꼴이다("…culture.gwelts". research/35).
	constexpr const char* k_CultureCaptionPrefix = "main_menu.new_game.province.kingdom.culture.";
	std::unordered_map<std::string, std::string> g_CultureCaptions;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	// 특성의 화면 이름. 없으면 빈 글. 게임을 불러온 뒤에는 힌트의 제목으로 채운 것도 든다(g_TraitTexts).
	const std::string& TraitCaption(const std::string& Name)
	{
		const auto text = g_TraitTexts.find(Name);
		if (text != g_TraitTexts.end())
			return text->second.Caption;
		const auto found = g_TraitCaptions.find(Name);
		return found != g_TraitCaptions.end() ? found->second : k_NoText;
	}

	// 특성의 설명. 없으면 빈 글.
	const std::string& TraitHint(const std::string& Name)
	{
		const auto text = g_TraitTexts.find(Name);
		return text != g_TraitTexts.end() ? text->second.Hint : k_NoText;
	}

	// 그 특성의 명칭이 힌트의 제목에서 온 것인가(이름 줄이 없는 특성).
	bool TraitTitled(const std::string& Name)
	{
		const auto text = g_TraitTexts.find(Name);
		return text != g_TraitTexts.end() && text->second.FromTitle;
	}

	// 창과 답에 적을 글: "화면 이름 (게임의 이름)". 화면 이름이 없으면 게임의 이름만.
	std::string TraitLabel(const std::string& Name)
	{
		const std::string& caption = TraitCaption(Name);
		return caption.empty() ? Name : caption + " (" + Name + ")";
	}

	// 능력치의 보일 이름: 게임의 글을 읽었으면 그것, 아니면 모듈의 이름.
	const char* SkillLabel(size_t Index)
	{
		const NlCore::NamedKey& skill = NlCore::SkillNames()[Index];
		const auto found = g_SkillCaptions.find(skill.Key);
		return found != g_SkillCaptions.end() ? found->second.c_str() : skill.Label;
	}

	// 게임의 현지화 파일에서 특성의 화면 이름을 읽는다. 러너를 부르지 않는다(파일만 읽는다).
	void LoadTraitFiles(const std::filesystem::path& GameDir)
	{
		// 이름: 한쪽이 실패해도 다른 쪽(힌트)은 읽는다.
		g_TraitCaptions.clear();
		const std::filesystem::path file = GameDir / "localization" / "main.csv";
		std::ifstream in(file, std::ios::binary);
		std::string why;
		if (!in)
			g_TraitTextNote = "게임의 localization\\main.csv 를 열지 못했습니다";
		else
		{
			const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			std::unordered_map<std::string, std::string> rows;
			const std::string prefix = NlCore::TraitCaptionKey("");
			if (!NlCore::ReadLocalization(text, prefix, { "Korean", "English" }, rows, why))
				g_TraitTextNote = "게임의 localization\\main.csv 를 읽지 못했습니다 (" + why + ")";
			else
			{
				for (auto& [key, value] : rows)
					g_TraitCaptions.emplace(key.substr(prefix.size()), std::move(value));
				g_TraitTextNote = "게임의 localization\\main.csv 에서 특성의 이름 " + std::to_string(g_TraitCaptions.size()) + "개를 읽었습니다";
			}
			// 능력치의 이름도 같은 파일에 있다. 여덟 열쇠의 줄만 받는다(한 줄짜리 짧은 글만. 아니면 모듈의 이름을 쓴다).
			g_SkillCaptions.clear();
			std::unordered_map<std::string, std::string> skill_rows;
			std::string skill_why;
			if (NlCore::ReadLocalization(text, NlCore::SkillCaptionKey(""), { "Korean", "English" }, skill_rows, skill_why))
				for (const NlCore::NamedKey& skill : NlCore::SkillNames())
				{
					const auto row = skill_rows.find(NlCore::SkillCaptionKey(skill.Key));
					if (row != skill_rows.end() && !row->second.empty() && row->second.size() <= 48 && row->second.find_first_of("\r\n<{") == std::string::npos)
						g_SkillCaptions.emplace(skill.Key, row->second);
				}
			// 문화의 이름도 같은 파일에 있다: 문화 구조체의 __caption 이 드는 열쇠(k_CultureCaptionPrefix + 이름. research/35). 한 줄짜리 짧은 글만 받는다.
			g_CultureCaptions.clear();
			std::unordered_map<std::string, std::string> culture_rows;
			std::string culture_why;
			if (NlCore::ReadLocalization(text, k_CultureCaptionPrefix, { "Korean", "English" }, culture_rows, culture_why))
				for (auto& [key, value] : culture_rows)
					if (!value.empty() && value.size() <= 48 && value.find_first_of("\r\n<{") == std::string::npos)
						g_CultureCaptions.emplace(key.substr(std::string(k_CultureCaptionPrefix).size()), std::move(value));
		}

		// 힌트의 글: 게임의 locale_definition.json 이 드는 힌트 파일 셋. 같은 열쇠가 여러 파일에 있으면 먼저 읽은 것을 둔다
		// (특성의 설명의 열쇠는 한 파일에만 있었다. research/20).
		g_Hints.clear();
		size_t files = 0;
		std::string failed;		// 읽지 못한 파일과 까닭(로그에 남긴다)
		for (const char* name : { "hints_tutorial.csv", "hints_with_icons.csv", "hints.csv" })
		{
			std::ifstream hints(GameDir / "localization" / name, std::ios::binary);
			std::string hint_why = "cannot open";
			if (hints)
			{
				const std::string body((std::istreambuf_iterator<char>(hints)), std::istreambuf_iterator<char>());
				if (NlCore::ReadLocalization(body, "", { "Korean", "English" }, g_Hints, hint_why))
				{
					files++;
					continue;
				}
			}
			failed += std::string(failed.empty() ? "" : ", ") + name + " (" + hint_why + ")";
		}
		g_HintFilesNote = files == 0 ? std::string("게임의 힌트 파일을 읽지 못했습니다")
			: "게임의 힌트 파일 " + std::to_string(files) + "개에서 글 " + std::to_string(g_Hints.size()) + "개를 읽었습니다";
		if (!failed.empty())
			g_HintFilesNote += " (못 읽은 것: " + failed + ")";
		g_TraitHintNote = g_HintFilesNote;
	}

	// 게임의 속성 함수로 특성의 글 하나를 읽는다. 글이 아니면 거짓.
	bool TraitProperty(const std::string& Name, int Property, std::string& Out)
	{
		RValue result;		// 이 함수 안에서만 든다
		if (!NlGame::CallScript(k_TraitProperty, { RValue(std::string_view(Name)), RValue(static_cast<double>(Property)) }, result) || !result.IsString())
			return false;
		Out = result.ToString();
		return true;
	}

	// 특성마다 보일 이름과 설명을 채운다(게임을 불러올 때 한 번. 게임 스레드). 설명의 열쇠는 게임에 묻는다: 이름으로 어림하면 틀린다(44개가 다른 꼴이었다).
	void LoadTraitTexts(const std::vector<std::string>& Names)
	{
		g_TraitTexts.clear();
		// 이름(파일의 것)부터 채운다. 설명을 붙이지 못해도 이름은 보인다.
		for (const std::string& name : Names)
		{
			TraitText text;
			const auto caption = g_TraitCaptions.find(name);
			if (caption != g_TraitCaptions.end())
				text.Caption = caption->second;
			g_TraitTexts.emplace(name, std::move(text));
		}

		// 설명을 붙이지 않는 까닭(빈 글이면 붙인다). 까닭마다 글을 달리한다(재지 않은 것을 "다르다"고 적지 않는다).
		std::string why;
		// 번호의 배치가 잰 것과 같은지부터 본다. 확인할 특성은 화면 이름의 줄이 있는 것 가운데서 고른다(core 의 TraitLayoutProbes:
		// 이름순의 앞쪽인 "__…__" 꼴의 안쪽 특성에는 1번을 재지 않았다).
		const std::vector<std::string> probes = NlCore::TraitLayoutProbes(Names, g_TraitCaptions, 3);
		if (g_Hints.empty())
			why = "게임의 힌트 파일을 읽지 못해 설명이 없습니다";
		else if (probes.empty())
			why = "배치를 확인할 특성(이름의 줄이 있는 특성)이 없어 설명을 붙이지 않았습니다";
		else
		{
			Log("people call trait_property_get(name, 0 | 1) on " + std::to_string(probes.size()) + " traits (first: " + probes.front()
				+ "), then (name, 1) and (name, 21) on all " + std::to_string(Names.size()));		// 부르기 전에 남긴다
			for (const std::string& probe : probes)
			{
				std::string name, key;
				const bool asked = TraitProperty(probe, NlCore::k_TraitNameProperty, name) && TraitProperty(probe, NlCore::k_TraitCaptionKeyProperty, key);
				if (asked && NlCore::TraitLayoutOk(probe, name, key))
					continue;
				why = asked ? "게임의 속성 함수가 잰 것과 다르게 답해 설명을 붙이지 않았습니다" : "게임의 속성 함수를 부르지 못해 설명을 붙이지 않았습니다";
				Log("people: trait_property_get layout check failed on " + probe + ": " + (asked ? "0 -> \"" + name + "\", 1 -> \"" + key + "\"" : std::string("no string answer")));
				break;
			}
		}

		size_t keys = 0, found = 0, texts = 0, titles = 0, renamed = 0;
		if (why.empty())
		{
			// 화면 이름: 게임이 1번으로 알려 준 열쇠의 줄을 쓴다(대개 "trait.<이름>"이지만 aging 은 "trait.oldman"이었다. 안쪽 특성은 빈 글이다).
			// 묻지 못한 특성은 위에서 채운 것(그 이름의 줄)을 그대로 둔다. 무엇을 찾을지는 core 의 TraitCaptionRow 가 정한다.
			for (const std::string& name : Names)
			{
				std::string key;
				const bool asked = TraitProperty(name, NlCore::k_TraitCaptionKeyProperty, key);
				const std::string row = NlCore::TraitCaptionRow(name, asked, key);
				const auto caption = row.empty() ? g_TraitCaptions.end() : g_TraitCaptions.find(row);
				g_TraitTexts[name].Caption = caption != g_TraitCaptions.end() ? caption->second : std::string();
				renamed += asked && row != name && caption != g_TraitCaptions.end() ? 1 : 0;
			}
			// 먼저 열쇠를 모두 모은다: 그 대부분이 힌트 파일에 있어야 21번이 힌트의 열쇠다(양성 대조. core 의 HintKeysPlausible).
			std::vector<std::string> key_of(Names.size());
			for (size_t i = 0; i < Names.size(); i++)
				if (TraitProperty(Names[i], NlCore::k_TraitHintProperty, key_of[i]) && !key_of[i].empty())
				{
					keys++;
					found += g_Hints.count(key_of[i]) > 0 ? 1 : 0;
				}
				else
					key_of[i].clear();
			if (!NlCore::HintKeysPlausible(keys, found))
				why = "게임이 준 설명의 열쇠가 힌트 파일과 맞지 않아(" + std::to_string(keys) + "개 가운데 " + std::to_string(found) + "개) 설명을 붙이지 않았습니다";
			else
				for (size_t i = 0; i < Names.size(); i++)
				{
					const auto hint = key_of[i].empty() ? g_Hints.end() : g_Hints.find(key_of[i]);
					if (hint == g_Hints.end())
						continue;
					NlCore::HintText split = NlCore::SplitHint(hint->second);
					TraitText& text = g_TraitTexts[Names[i]];
					// 화면 이름의 줄이 없는 특성: 힌트의 제목을 명칭으로 쓴다(본문이 있고 짧을 때만. core 의 GoodHintTitle).
					if (text.Caption.empty() && NlCore::GoodHintTitle(split))
					{
						text.Caption = split.Title;
						text.FromTitle = true;
						titles++;
					}
					texts += split.Body.empty() ? 0 : 1;
					text.Hint = std::move(split.Body);
				}
		}
		g_TraitHintNote = !why.empty() ? why
			: "설명 " + std::to_string(texts) + "개 (게임이 알려 준 열쇠 " + std::to_string(keys) + "개 가운데 힌트 파일에 있는 것 " + std::to_string(found)
				+ "개. 설명의 제목을 명칭으로 쓴 것 " + std::to_string(titles) + "개)";
		Log("people: trait texts: " + std::to_string(Names.size()) + " traits, " + std::to_string(renamed) + " captions under another key, " + std::to_string(keys)
			+ " hint keys from the game, " + std::to_string(found) + " found in the hint files, " + std::to_string(texts) + " descriptions, " + std::to_string(titles)
			+ " captions from hint titles"
			+ (why.empty() ? "" : "; no descriptions: " + why));
	}

	void LoadTraitNames()
	{
		RValue list;
		std::string why;
		g_Names.clear();
		g_Shown.clear();
		if (!NlAccess::Read(NlCore::ParseAskPath(k_TraitList), list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (name.IsString() && NlCore::GoodTraitName(name.ToString()))
				g_Names.push_back(name.ToString());
			return true;
		});
		std::sort(g_Names.begin(), g_Names.end());
		g_Names.erase(std::unique(g_Names.begin(), g_Names.end()), g_Names.end());
		Log("people: " + std::to_string(g_Names.size()) + " trait names");
		// 보일 이름과 설명, 그리고 창에 보일 차례(TraitNames 는 이름순으로 둔다: 이분 탐색에 쓴다).
		LoadTraitTexts(g_Names);
		// 역할 프리셋이 드는 특성 가운데 이 게임에 없는 것(이 빌드에서는 없었다. 게임이 갱신되면 생길 수 있다). 그런 것은 붙이지 않고 결과에 적는다.
		std::vector<std::string> missing;
		for (const NlCore::RolePreset& role : NlCore::RolePresets())
			for (const std::vector<const char*>* side : { &role.Add, &role.Remove })
				for (const char* name : *side)
					if (!std::binary_search(g_Names.begin(), g_Names.end(), std::string(name)) && std::find(missing.begin(), missing.end(), name) == missing.end())
						missing.push_back(name);
		std::string names;
		for (const std::string& name : missing)
			names += " " + name;
		Log("people: role presets name " + std::to_string(missing.size()) + " trait(s) this game does not have" + (names.empty() ? "" : ":" + names));
		g_Shown = g_Names;
		std::sort(g_Shown.begin(), g_Shown.end(),
			[](const std::string& a, const std::string& b) { return NlCore::TraitBefore(a, TraitCaption(a), b, TraitCaption(b)); });
	}

}

void NlTraitText::Init(LogFn Log_, const std::filesystem::path& GameDir)
{
	std::lock_guard lock(g_Mutex);
	g_Log = std::move(Log_);
	LoadTraitFiles(GameDir);
	Log("people: " + g_TraitTextNote);
	Log("people: " + g_HintFilesNote);
	Log("people: skill names from the game's main.csv: " + std::to_string(g_SkillCaptions.size()) + " of " + std::to_string(NlCore::SkillNames().size()));
}

void NlTraitText::LoadForGame()
{
	std::lock_guard lock(g_Mutex);
	LoadTraitNames();
}

const std::vector<std::string>& NlTraitText::Names()
{
	std::lock_guard lock(g_Mutex);
	return g_Names;
}

const std::vector<std::string>& NlTraitText::Shown()
{
	std::lock_guard lock(g_Mutex);
	return g_Shown;
}

bool NlTraitText::Known(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return std::binary_search(g_Names.begin(), g_Names.end(), Name);
}

const std::string& NlTraitText::Caption(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return TraitCaption(Name);
}

const std::string& NlTraitText::Hint(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return TraitHint(Name);
}

bool NlTraitText::Titled(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return TraitTitled(Name);
}

std::string NlTraitText::Label(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	return TraitLabel(Name);
}

const char* NlTraitText::SkillLabel(size_t Index)
{
	std::lock_guard lock(g_Mutex);
	return ::SkillLabel(Index);
}

std::string NlTraitText::CultureLabel(const std::string& Name)
{
	std::lock_guard lock(g_Mutex);
	const auto found = g_CultureCaptions.find(Name);
	return found != g_CultureCaptions.end() ? found->second + " (" + Name + ")" : Name;
}

NlTraitText::Notes NlTraitText::GetNotes()
{
	std::lock_guard lock(g_Mutex);
	return { g_TraitTextNote, g_TraitHintNote, g_HintFilesNote };
}
