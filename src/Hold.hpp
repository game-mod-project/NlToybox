#pragma once
// 표의 인구 항목(배고픔 없음, 피로 없음, 모든 욕구, 신앙심 채우기, 언제나 행복, 노화로 죽지 않음. research/11): 플레이어의 사람을 40명씩 0.25초마다 돌며 쓴다.
// 바퀴의 판단(노화 깃발을 끈 뒤 처음부터 온전한 한 바퀴로 되돌리기)은 core/PeoplePlan 의 HoldRound 에 있다.
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. People 의 틱 안에서만 불린다(People 의 잠금 아래. 제 뮤텍스는 없다).

#include "core/PeoplePlan.hpp"

#include <functional>
#include <string>
#include <vector>

namespace NlHold
{
	using LogFn = std::function<void(const std::string&)>;
	// 사람들을 다시 읽는다(People 의 Scan 뒤 목록의 사본). 못 읽으면 거짓.
	using RowsFn = std::function<bool(std::vector<NlCore::PersonRow>& Out)>;
	// 한 사람에게 행복 생각을 붙인다(People 의 One(Happy, Bulk)). 됐고 Note 가 비면 참.
	using HappyFn = std::function<bool(const NlCore::PersonRow& Row, std::string& Note)>;

	void Init(LogFn Log, RowsFn Rows, HappyFn Happy);
	// 게임 스레드의 틱(People::GameTick 이 부른다). 0.25초에 한 번.
	void Tick(double Now);
}
