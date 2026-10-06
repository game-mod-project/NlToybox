# 인구: 임신·출생·성장 — 실행 계획

**목표:** 임신의 단계를 넘기고 바로 출산시키고, 임신을 시작하고, 아이를 어른으로 만들고, 임신·출산의 확률을 바꾼다.

**스펙:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(인구·욕구: "출생 즉시, 임신 확률, 성장 속도, 사망 방지"). 사용자 요청 2026-10-06("인구(임신·출생), 월드 순으로").

**게임 실행:** 5번 승인(최소값 칸 수정의 확인과 함께). 실행 1 사용.

## 근거 (실행 1, `preg-session1`. `research/24`)

| 쓰는 것 | 어디서 봤나 |
|---|---|
| 임신의 단계는 특성 `pregnant_st1`·`st2`·`st3`, 출산 뒤 어머니에게 `pregnant_forbid` | 사용자의 세이브의 임신한 영주, 게임의 특성 목록 |
| 임신 구성요소 `__soul.__pregnancy`(ComponentPregnancy): `__father_soul_uuid`, `__sex_in_row` | `list`, `statics`. 주민(`o_dummy`)에게도 있다 |
| `debug_pregnancy_next_stage()` 인자 없음 | 본문의 기계어(argc 를 옮기지 않는다). **세 번 불러 1 → 2 → 3 → 출산**(아이가 영주로 생김)을 봤다. 오류 없음 |
| `begin_pregnant()`, `is_can_pregant()`, `spawn_kid()`, `miscarriage()` 인자 없음 | 기계어. `spawn_kid() -> 구조체`는 출산 때 게임이 불렀다. **`begin_pregnant`는 아직 부르지 않았다** |
| `SoulBasic.get_gender() -> 1 \| 0` | 게임이 334번 불렀다. 임신한 영주가 1, 아버지가 0 |
| 아이를 어른으로: `set_age(18)` | 15, 16 에서는 `kid` 그대로, 18 에서 게임이 `kid`를 떼고 `untitled_lord`를 붙였다(진영 `player_untitled`) |
| 게임 변수 `pregnancy_chance`(0.5), `pregnancy_from_dummy_chance`, `pregnancy_miscarriage_chance`, `pregnancy_mother_die`, `trait_death_in_childbirth` | `global.__gameplay_vars`의 목록. 게임이 따르는지는 모른다 |

## 파일

- `src/core/FamilyPlan.hpp/.cpp` (새로): `PregnancyStage`, `IsKid`, `AfterStageCall`, `BirthNeedsCall`, `StageReport`, `CanConceive`, `CanFather`, `GoodUuid`, 게임 변수의 열쇠 셋.
- `src/core/PeoplePlan.*`: `PersonAct::PregnancyNext`·`Birth`·`GrowUp`·`Conceive`(낱말, 검사. 여럿에게는 `lords birth`만).
- `src/People.cpp`: `One()`의 네 갈래, 여럿의 출산은 임신한 사람만, 인물 탭의 "임신·성장", 영주 탭의 "임신한 영주 모두 출산", 성별 읽기.
- `src/core/CheatTable.cpp`, `src/Production.cpp`: `pregnancy_chance`(배율), `no_miscarriage`, `safe_childbirth`(0 으로). 확인 전.
- `src/Economy.cpp`: 안내 글의 줄 바꿈(0.24.2 의 화면에서 오른쪽이 잘렸다).

## 차례

1. [x] 실행 1: 구조, 다음 단계 함수(출산까지), 성장, 게임 변수의 조사.
2. [x] 시험 먼저 → 코어·명령·표·화면 구현. 빌드와 시험 묶음 넷.
3. [ ] 독립 검토(게임을 켜기 전) → 고친다.
4. [ ] 실행 2: 명령과 화면으로 다음 단계·바로 출산·어른으로·여럿의 출산을 확인하고, 표의 셋이 값을 쓰고 되돌리는지 본다.
   **맨 끝에 임신 시작(`begin_pregnant`의 첫 호출)을 한 번** 부른다(오류 창이 뜰 수 있다).
5. [ ] 결과에 맞춰 고치고 실행 3 에서 다시 본다. 마지막 DLL 의 적재 판정.
6. [ ] 문서(`research/24`, CLAUDE, README, 스펙, 이 계획의 결과) → develop 으로 머지.

## 검토에서 볼 것

- 임신 중이 아닌 사람, 아이, 남성, 죽은 사람에게 눌렀을 때 게임의 함수를 부르지 않는가.
- 다음 단계 함수를 부른 뒤 단계가 본 대로(1→2, 2→3, 3→0) 바뀌지 않았을 때 더 부르지 않는가.
- 임신 시작이 실패했을 때 적어 둔 아버지의 uuid 가 남지 않는가. 아버지로 죽은 사람·아이·여성·자기 자신이 들어가지 않는가.
- 여럿의 출산이 임신하지 않은 영주를 "못 했다"로 세지 않는가.
- 창의 글이 재지 않은 것(임신 금지의 뜻, 게임 변수의 효과)을 사실처럼 말하지 않는가.

## 결과

(실행 뒤에 적는다.)
