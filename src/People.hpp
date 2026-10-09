#pragma once
// 인물·영주·인구 패널(스펙: 치트 메뉴 §8. 자리와 게임의 함수는 research/11).
// 사람들의 목록과 고른 사람의 값을 틱에서 글로 떠 두고, 창의 명령은 다음 틱이 실행한 뒤 다시 읽는다. 그리는 쪽은 러너를 부르지 않는다.
// 치트 표의 인구 항목(배고픔 없음, 피로 없음, 모든 욕구, 언제나 행복, 노화로 죽지 않음)도 여기서 한다: 플레이어의 사람을 조금씩 돌며 쓴다.

#include "core/PeoplePlan.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace NlPeople
{
	using LogFn = std::function<void(const std::string&)>;

	// GameDir: 게임 폴더. 그 안의 localization\main.csv 에서 특성의 화면 이름(한국어)을 읽어 둔다(게임 파일의 글을 레포에 싣지 않는다. research/20).
	void Init(LogFn Log, const std::filesystem::path& GameDir);

	// 게임 스레드의 틱. Active: 이 파일의 패널(인물, 영주, 인구, 지식, 아이템) 가운데 하나가 보이는가(보일 때만 목록과 값을 새로 읽는다).
	void GameTick(double Now, bool Active);

	// 그리는 쪽.
	void DrawPerson();		// 인물: 한 사람을 골라 고친다
	void DrawLords();		// 영주: 플레이어의 영주 전원에게 한꺼번에
	void DrawPeople();		// 인구·욕구: 플레이어의 사람 전원에게 한꺼번에
	void DrawKnowledge();	// 지식: 영주에게 지식을 준다(research/12)
	void DrawItems();		// 아이템: 한 사람의 소지금과 소지품
	void DrawArmy();		// 군대: 병사를 만든다(research/13)

	// 원격 명령(창 없이 같은 길을 태운다). 게임 스레드에서 부른다. 돌려주는 것: 답의 줄들.
	std::vector<std::string> Do(const NlCore::PersonCommand& Command);
	// 플레이어의 병사를 Count 명 만든다(몇 명이 되는지는 core/PeoplePlan 의 SoldierBatch 가 정한다: 1~20).
	std::vector<std::string> SpawnSoldiers(double Count);
	// 게임의 디버그 소환기로 플레이어의 사람 하나를 마우스가 가리키는 지도의 자리에 만든다.
	std::vector<std::string> SpawnHere(NlCore::SpawnKind Kind);
	// 성별·나이·문화·역할을 정해 플레이어의 영주 하나를 마우스 자리에 만든다(core 의 LordSpawn. research/35). 돌려주는 것: 만든 것의 줄과 입힌 일마다의 줄.
	std::vector<std::string> SpawnLord(const NlCore::LordSpawn& Options);
	std::vector<std::string> List(bool All);
	// 한 사람의 값. 인물 패널도 그 사람을 고른다.
	std::vector<std::string> Show(const std::string& Uuid);
	// 게임의 특성들: 이름과 화면 이름. Find: 이름이나 화면 이름의 일부(빈 글이면 모두). Max: 줄의 한도.
	// 인물 패널의 찾기 칸도 그 글로 맞추고 전체 목록을 펼친다(page person 뒤 shot 으로 창을 볼 수 있게).
	std::vector<std::string> Traits(const std::string& Find, size_t Max);
	// 특성의 보일 이름: "화면 이름 (게임의 이름)". 화면 이름이 없으면 게임의 이름만(범죄 패널이 죄와 혐의를 그렇게 적는다). 러너를 부르지 않는다.
	std::string TraitName(const std::string& Name);
	// 사람들을 다시 읽어 그 줄들을 준다(영주의 호감·충성 패널이 쓴다). Busy: 인물 쪽이 게임의 함수를 부르는 중에 다시 들어왔다(잠깐 뒤에 다시 하면 된다).
	// Failed: 읽지 못했다(Why 에 까닭).
	enum class RowsResult { Ok, Busy, Failed };
	RowsResult Rows(std::vector<NlCore::PersonRow>& Out, std::string& Why);
}
