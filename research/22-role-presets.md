# 22. 인물의 역할 프리셋

2026-10-06 · 게임 0.5588.9777.0 · 모듈 0.24.0 · 세이브 `아덴 … Morning_day_3`(불러오기만 했다. 저장하지 않았다)

실행 둘(`refs\runtime\role-session1.answer.txt`, `role-session2.answer.txt`. 추적 안 함). 화면은 `refs\ui\role-before.png`, `role-after.png`, `role-barra.png`.

## 무엇을 만들었나

인물 탭에서 한 사람을 골라 역할 프리셋 하나를 입힌다: 그 역할의 능력치를 올리고(내리지 않는다), 그 역할에 해로운 특성을 떼고, 맞는 재능을 붙인다.
프리셋은 열셋이다(`src/core/RolePlan.cpp`의 표): `king`, `steward`, `scholar`, `instructor`, `general`, `duelist`, `politician`, `schemer`, `socialite`,
`priest`, `trader`, `producer`, `teacher`. 원격으로는 `person <uuid> role name=<Id>`.

사용자가 정한 것(2026-10-06): 핵심 능력치 20, 보조 15, **올리기만**. 표의 재능을 붙이고 **역할에 해로운 특성도 뗀다**. 무엇이 떼어지는지는 누르기 전에 보인다.

## 표를 고른 근거

- 특성의 뜻은 게임의 설명 글(힌트 파일)을 읽고 골랐다. 글은 레포에 옮기지 않았다. 프리셋이 드는 특성은 74개(표의 칸은 123개)이고,
  74개 모두 게임의 특성 목록(281개)에 있고, 게임이 알려 준 힌트의 열쇠(`trait_property_get(이름, 21)`. `research/20`)로 설명 줄이 있다.
- **재능마다의 효과를 플레이에서 재지는 않았다.** 아래 "본 것"의 둘(게임의 경고, 생각의 합)이 효과에 대해 아는 전부다.
- 넣지 않은 것: `politic`·`intriguan`·`duelist`(왕의 경쟁자나 위험한 손님에게 붙는 표식), `religious`·`religiosity_fanatic`(끝나지 않는 광신),
  `saint`(기한이 있다), `savant`(더 훈련할 수 없게 된다), `genius_king`(이웃 통치자를 미워하게 된다). 시험이 이 이름들이 표에 없는 것을 본다.
- 떼는 것은 설명에서 해로움을 읽은 아홉: `nervous`(모든 역할), `coward`·`pacifist`(싸우는 역할), `stupidity`(배우고 다스리는 역할),
  `contemptuous`·`sarcastic`·`cynic`·`envious`·`greedy`(사람을 대하는 역할).

## 능력치의 화면 이름

게임의 `localization\main.csv`에 `actor.skill.` 줄이 아홉 있다: `command`, `education`, `fight`, `knowledge`, `management`, `manners`, `negotiation`,
`oratory`, `points_remain`. **전투의 줄만 능력치의 열쇠(`combat`)와 이름이 다르다(`fight`).** 처음에는 여덟이 모두 `actor.skill.<열쇠>`라고 적었고
(확인하지 않은 것이었다) 독립 검토가 게임 파일을 읽어 잡았다. 고친 뒤 실행에서 `skill names from the game's main.csv: 8 of 8`.

## 실행 1: 열셋을 영주 넷에게

멈춘 채로 했다(마지막의 두 시간만 흘렸다).

| 본 것 | 결과 |
|---|---|
| 프리셋 13개를 영주 넷에게 입힘 | 하지 못한 것 0. 로그의 `trait_attach` 85번, `trait_detach` 15번(시험으로 붙이고 뗀 것 포함) |
| 해로운 특성을 붙인 뒤 프리셋 | `instructor`가 `nervous`·`coward`·`pacifist`·`stupidity`를, `socialite`가 `nervous`·`contemptuous`·`sarcastic`·`cynic`·`envious`·`greedy`를, `king`이 `greedy`·`stupidity`를 뗐다. 아홉 종 모두 떼어진다 |
| 올리기만 | `knowledge`를 20 으로 둔 영주에게 `producer`(`knowledge` 15): 20 그대로. `king`을 입힌 영주의 `negotiation` 12(표에 없다)도 그대로 |
| 같은 프리셋을 한 번 더 | "이미 그대로입니다 (바꾼 것이 없습니다)". 걸음 0 |
| 한 명령(한 틱)의 호출 수 | `king`: 쓰기 5, 떼기 2, 붙이기 8. `socialite`: 쓰기 2, 떼기 6, 붙이기 7. 탈이 없었다 |
| 한 사람에게 쌓인 재능 | 한 영주에게 프리셋 다섯(`socialite`, `scholar`, `schemer`, `priest`, `teacher`)을 겹쳐 재능 29개. 거부 없음 |
| 게임 시간 두 시간 뒤(367206 → 374490) | 능력치와 붙인 재능이 그대로, 뗀 특성은 돌아오지 않았다. 게임이 스스로 붙인 것이 둘 보였다(`inspired`, `wet_feet`) |
| 틀린 청 | 없는 이름에는 이름 열셋을 적은 답, `lords`에게는 "한 사람을 짚어라"는 답, 이름이 없으면 `needs name=<name>` |

`coward`를 가진 사람에게 맞서는 재능을 붙여 봤다(`fearless`, `brave`): **게임이 막지 않고 셋이 함께 붙는다.** 스스로 떼지도 않는다.
그래도 프리셋은 해로운 것을 먼저 뗀다(`RoleSteps`: 능력치, 떼기, 붙이기). 걸음마다 특성 목록을 다시 읽어 이미 없는 것을 떼거나 이미 있는 것을 붙이려 부르지 않는다.

두 시간 사이에 영주 넷의 나이가 모두 한 살 늘었다(39 → 40 …). 시간을 흘리지 않은 실행 2 에서는 프리셋을 입혀도 나이가 그대로였다.
프리셋에는 나이를 쓰는 걸음이 없다. 그 시각에 게임이 나이를 올린 까닭은 재지 않았다.

## 실행 2: 화면, 생각의 합

- 인물 탭의 "역할 프리셋": 고르는 칸, "이 역할로", 능력치의 줄("… 4 -> 20"), 붙일 특성(붙임·있음), 뗄 특성(뗌·없음), 마지막 결과의 줄이 그려진다.
  능력치의 이름은 게임의 것이다. **단추 옆의 요약 글이 창의 오른쪽에서 잘린다**(기본 너비에서 "… 특성 8개를 붙이고"까지만 보인다. 창을 넓히면 보인다).
  뗄 특성의 줄은 붙일 특성 아래라 스크롤해야 보인다.
- **`stupidity`를 떼면 생각의 합이 꼭 25 내려간다**: 한 영주에게서 그것만 떼자 74.94 → 49.94, 다시 붙이자 74.94. `greedy`를 붙였다 뗀 영주는 그대로(59.65)였다.
  `king`을 입힌 영주는 82.14 → 75.81 이었다(`stupidity`의 −25 에 왕의 재능 여덟 가운데 무엇인가가 +18.67. 어느 것인지는 가리지 않았다).
  `instructor`의 재능 넷, `producer`의 여덟, `socialite`의 일곱은 생각의 합을 바꾸지 않았다.
- 적재 판정: 통과(모듈 0.24.0).

## 게임의 경고 파일에 남은 것

`catched_errors_<버전>.txt`(읽기만 했다):

- 시간을 흘린 두 시간 사이에 `Trait action task target is undefined :: redeemer :: redeemer_absolution`이 두 번,
  `Musician trait action failed to start activity`가 한 번 났다. 붙인 재능(`redeemer`, `musician`)의 행동을 게임이 돌리려 했고 대상이나 할 자리를 찾지 못했다는 뜻으로 읽힌다(추정).
  게임이 잡아 적는 경고이고 게임은 계속 돌았다. 그 재능을 타고난 사람에게서도 나는 경고인지는 모른다.
- 켤 때마다 `не существует хинта :: hint_talent_<이름>`이 여럿 난다(`cutter`, `forecaster`, `charisma`, `diplomat`, `precise_language`, `protector`, `duelist_talent`).
  세이브를 불러오기 전의 시각이고 프리셋을 쓰지 않은 실행에도 있다. 게임이 `hint_talent_<이름>`으로 어림한 열쇠가 없다는 것이고, 그 특성들의 진짜 열쇠는 다르다
  (`hint_trait_cutter` 같은 것. `research/20`의 "열쇠를 이름으로 어림하지 않는다"와 같은 이야기다).

## 재지 않은 것

- 재능마다의 효과(전투, 생산, 설득에서의 차이).
- 붙인 특성과 올린 능력치가 세이브에 남는지(저장하지 않았다).
- 주민이나 손님에게 입혔을 때(영주 넷에게만 입혔다. 주민에게는 전투 말고 능력치가 없어 계획이 건너뛴다는 것은 시험으로만 봤다).
- `stupidity`가 아닌 해로운 특성 여덟 가운데 `greedy` 말고는 생각의 합을 따로 재지 않았다.

## 도구에서 배운 것

- `shot`에는 이름을 준다(`shot role-before`). 이름 없이 보내면 "shot needs a name"이고 찍히지 않는다(실행 1 의 화면 둘을 그래서 못 받았다).
  받은 화면은 `ask.ps1`이 `refs\ui\<이름>.png`로 옮긴다.
- 고른 사람의 값은 0.5초마다 다시 읽힌다. 명령 뒤의 화면은 2초쯤 두고 찍는다.
