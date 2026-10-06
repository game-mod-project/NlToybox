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
3. [x] 독립 검토 → 고쳤다(실행 2 와 겹쳐 돌았다. 지적은 실행 3 전에 반영했다).
4. [x] 실행 2: 명령으로 다음 단계·여럿의 출산·어른으로를 확인하고, 표의 셋이 값을 쓰고 되돌리는 것을 봤다.
   맨 끝에 `begin_pregnant()`를 처음 불렀다: **게임이 끝났다.**
5. [x] 실행 3: 화면과 단추, 새 방식의 임신 시작(아버지 + 1/3기의 특성), 임신 시작과 출산을 48번 되풀이해 유산을 셌다.
6. [ ] 실행 4: 마지막 DLL 의 적재 판정과 화면. 문서(`research/24`, CLAUDE, README, 스펙, 이 계획의 결과) → develop 으로 머지.

## 검토에서 볼 것

- 임신 중이 아닌 사람, 아이, 남성, 죽은 사람에게 눌렀을 때 게임의 함수를 부르지 않는가.
- 다음 단계 함수를 부른 뒤 단계가 본 대로(1→2, 2→3, 3→0) 바뀌지 않았을 때 더 부르지 않는가.
- 임신 시작이 실패했을 때 적어 둔 아버지의 uuid 가 남지 않는가. 아버지로 죽은 사람·아이·여성·자기 자신이 들어가지 않는가.
- 여럿의 출산이 임신하지 않은 영주를 "못 했다"로 세지 않는가.
- 창의 글이 재지 않은 것(임신 금지의 뜻, 게임 변수의 효과)을 사실처럼 말하지 않는가.

## 결과

모듈 0.25.2. 잰 것은 `research/24-pregnancy.md`.

- **됐다**: 임신의 다음 단계, 바로 출산(한 사람, 임신한 영주 모두), 임신 시작(아버지를 골라), 아이를 어른으로. 인물 탭의 "임신·성장"과 원격 `person … pregnancy_next|birth|conceive|grow_up`.
- **확인으로 올린 것**: `no_miscarriage`(켠 채 임신 25번에 유산 0, 끈 채 27번에 5). 신 묶음에 넣었다.
- **길을 바꾼 것**: 임신 시작. 게임의 `begin_pregnant()`는 첫 호출에서 게임을 끝냈다(사용자에게 미리 알린 위험한 호출. 저장하지 않았다).
  아버지의 uuid 를 적고 1/3기의 특성을 붙이는 길로 바꿨고 49번 모두 출산이나 유산까지 갔다.
- **틀렸던 것**: 3/3기의 다음 호출 뒤 단계가 없어진 것만 보고 "출산했습니다"라고 적었다(실행 2 에서 유산이었다). 사람의 수로 가르게 고쳤다.
- **독립 검토**: Critical 0, Important 5, Minor 9. 고친 것 — 게임의 판정을 읽는 형, 임신 시작의 뒤처리와 아버지 칸의 재확인, 확인 전 표시, 플레이어의 영주로 좁히기, 결과의 글 둘.
- **확인 전으로 남은 것**: `pregnancy_chance`(모듈의 임신 시작은 그 확률을 타지 않는다), `safe_childbirth`(끈 채 22번의 출산에도 어머니가 죽지 않았다).
- **재지 않은 것**: 하루가 넘어갈 때 임신이 스스로 나아가는지, 주민의 임신, `pregnant_forbid`가 얼마나 가는지.
- **미룬 것**(검토의 Minor): 여럿의 출산이 읽지 못한 영주를 말없이 뺀다, 아버지 목록에 여성과 아이가 그대로 나온다, 결과 글이 이름으로 시작하는지로 갈래를 가린다,
  출산 뒤 어머니가 살아 있는지 보지 않는다, 상수와 열쇠 목록을 따라 적은 시험.
