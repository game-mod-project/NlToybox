# 치트 메뉴 6단계: 외교·종교·이벤트·월드 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "외교", "종교", "이벤트", "월드"를 모드창에서 되게 한다. 군대 단계에서 넘긴 것(아군 무적의 실전 확인, 충성도) 가운데 같은 실행에서 잴 수 있는 것은 함께 잰다.

**Architecture:** 앞 단계들과 같은 길이다. 게임의 관리자(`research/14`)에서 값은 주소로 쓰고(B), 인자 없는 디버그 함수는 단추로 부르고(C), 판정은 훅으로 바꾼다(D).
한 번 하는 일(매복 일으키기, 주교 부르기, 이벤트 쿨다운 지우기)은 패널의 단추와 원격 명령으로 낸다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`·`load-save.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(외교, 종교, 월드, 이벤트의 줄), §10(6).

## 아는 것과 모르는 것

아는 것(`research/14`. 첫 조사 실행): 관리자의 이름과 메서드, 기계어로 읽은 인자 수, 몇 개의 읽기 함수가 돌려준 값, 세력·이벤트 쿨다운·게임 조건의 자료.

모르는 것(Task 1 의 둘째 실행이 잰다): 게임이 세력·이벤트·습격·충성도 함수를 부르는 꼴, 인자 없는 디버그 함수(매복, 주교)를 불렀을 때 일어나는 일, 이벤트 쿨다운을 0 으로 쓴 효과, 계절·날씨 관리자의 꼴.

## Global Constraints

- 앞 단계와 같다: `gml_Script_` 이름으로만, 꼴을 본 함수만, 위험한 호출은 요청의 맨 뒤에 따로, 훅 자리 64개, 게임은 3번까지(첫 번은 썼다), 세이브의 사본, 자동 저장 시각을 넘기지 않는다.
- **인자 맞춤(N)이 있는 함수는 인자가 N 개인 함수로 본다**(`CLAUDE.md`). 인자 없이 부르는 것은 본문이 `argc`를 옮기지 않는 함수뿐이다.

## Review Focus

1. 인자 없는 디버그 함수(매복, 주교)를 게임 화면이 아닐 때나 되풀이해 부르는 일.
2. 이벤트 쿨다운·게임 조건에 쓴 값이 세이브에 남는 것을 알리지 않는 일.
3. 재지 않은 효과를 창의 글이 사실로 적는 일.
4. 훅을 건 판정이 플레이어가 아닌 세력에도 걸리는 일.

---

### Task 1: 조사 (게임 2회. 첫 번은 썼다)

- [x] 첫 실행(`stage6-session1`): 관리자의 `statics`·`tree`, 인자 수, 읽기 함수. 내가 인자 없이 부른 충성도 함수 때문에 GML 오류로 끝났다.
- [ ] 둘째 실행: 기록(세력, 감독, 습격, 충성도, 종교, 계절·날씨), 이벤트 쿨다운 쓰기, 인자 없는 디버그 함수(주교, 매복)를 맨 뒤에 하나씩. `research/14`에 적는다.

### Task 2: 치트로 굳힌다 (Task 1 의 답으로 채운다)

- [ ] `src/core`에 시험과 함께(RED → GREEN), 치트 표의 항목(영역: 외교, 종교, 이벤트, 월드)과 패널·원격 명령.

### Task 3: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토 → Critical·Important 를 시험과 함께 고친다. 확인 실행. `research/14`, `CLAUDE.md`, `README.md`, 스펙. `git merge --no-ff` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 외교·종교·월드·이벤트 줄이 Task 1·2 에 있다. 잴 수 없는 것은 적고 넘긴다.
- **빈칸**: Task 2 는 Task 1 의 답이 있어야 쓸 수 있다.
