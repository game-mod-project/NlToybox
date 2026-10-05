#pragma once
// 인물·영주·인구 패널(스펙: 치트 메뉴 §8. 자리와 게임의 함수는 research/11).
// 사람들의 목록과 고른 사람의 값을 틱에서 글로 떠 두고, 창의 명령은 다음 틱이 실행한 뒤 다시 읽는다. 그리는 쪽은 러너를 부르지 않는다.
// 치트 표의 인구 항목(배고픔 없음, 피로 없음, 모든 욕구, 언제나 행복, 노화로 죽지 않음)도 여기서 한다: 플레이어의 사람을 조금씩 돌며 쓴다.

#include "core/PeoplePlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlPeople
{
	using LogFn = std::function<void(const std::string&)>;

	void Init(LogFn Log);

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
	std::vector<std::string> List(bool All);
	// 한 사람의 값. 인물 패널도 그 사람을 고른다.
	std::vector<std::string> Show(const std::string& Uuid);
}
