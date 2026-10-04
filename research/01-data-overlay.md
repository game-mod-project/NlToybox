# 01 — 데이터 오버레이 단계 0 실측

조사일 2026-10-04. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 3. 모두 **메인 메뉴**에서 쟀다(새 게임을 시작하지 않았다).
덤프 원본은 `refs/runtime/dump-*.json`에 있다(추적 안 함). 다시 재려면 `tools/probe.ps1`을 쓴다.

## 답

| 질문 | 답 | 근거 |
|---|---|---|
| 1. 런타임 전역 변수는 어떻게 생겼나 | 4,737개. 구조체 4,553, 수 105, 배열 58, 불리언 14, 그 밖 7 | `dump-vanilla.globals.json` |
| 2a. `debug_params.json`의 값이 반영되나 | **확인하지 못함** | `budget_money`를 2345로 바꿨으나 메인 메뉴에서는 전역에서 닿는 곳 어디에도 2345가 없고, `budget_money`·`production_cost`·`building_resources`·`product_count`·`fair_trade_purchase`라는 이름의 멤버도 없다. `gml_Script_budget_default_money_get()`은 `undefined`를 돌려줬다 |
| 2b. `gameplay_variables.json`의 값이 반영되나 | **그렇다** | `global_map.ai_economy.initial_budget`를 700 → 745로 바꾸자 `global.__gameplay_vars.global_map_ai_economy_initial_budget`가 700 → 745가 됐다 |
| 2c. `battle_params.json`의 값이 반영되나 | **확인하지 못함** | `battle_dodge_base`라는 이름의 멤버가 메인 메뉴의 전역에서 닿는 곳에 없다 |
| 3. 파일에 없는 키를 넣으면 읽히나 | **아니다** (이 경로로는) | `game` 객체에 `"speed_slower_div": 47`을 넣었으나 `global.__gameplay_vars.game_speed_slower_div`는 2 그대로였다. 게임은 오류 없이 켜졌다(모르는 키는 무시된다) |

"전역에서 닿는 곳"은 전역 변수에서 구조체와 배열을 따라 깊이 6까지, 길이 64 이하의 배열만 들어가 본 범위다.
그 범위의 값은 38,682개였고 찾기는 한도에 걸리지 않고 끝났다.

2a와 2c가 "아니다"가 아니라 "확인하지 못함"인 이유: 그 값들이 새 게임을 시작할 때 읽힐 수 있고, 전역이 아니라
오브젝트 인스턴스의 변수에 있을 수도 있다. 메인 메뉴에서는 어느 쪽인지 가릴 수 없다.

3의 한계: 런타임 이름 `game_speed_slower_div`가 파일에서 `game.speed_slower_div`가 아니라 다른 중첩
(`game_speed.slower_div` 등)에 대응할 가능성은 시험하지 않았다.

## 파일 키와 런타임 경로의 대응 (`gameplay_variables.json`)

- 런타임의 자리는 `global.__gameplay_vars`다. 중첩이 없는 평평한 구조체이고 멤버는 420개다.
- 멤버 이름은 파일의 키 경로를 `_`로 이은 것이다.
  `global_map.ai_economy.initial_budget` → `global_map_ai_economy_initial_budget`,
  `game.death_chance_mult` → `game_death_chance_mult`.
- 파일의 값 150개 가운데 **94개**가 그 이름으로 런타임에 있고, 94개 모두 값이 파일과 같다.
  반영을 직접 본 것은 그중 하나(`initial_budget`)다. 나머지 93개는 이름과 값이 일치한다는 것까지만 확인했다.
- 파일에만 있는 것 56개. 런타임 구조체에 대응하는 이름이 없다. 예: `actor_child_age_exp_limit`,
  `actor_panic_run`, `bribe_add_opinion`, `bribe_cooldown`, `building_breakdown_chance`, `camera_smooth_move_speed`.
  쓰이지 않는 낡은 키이거나 다른 자리로 읽히는 키다(어느 쪽인지 확인하지 않았다).
- 런타임에만 있는 것 **326개**. 파일에는 없고 코드가 기본값을 준다. 예:

  | 이름 | 값 |
  |---|---|
  | `game_speed_slower_div` | 2 |
  | `gameplay_hour_frames` | 3600 |
  | `game_expirience_mult` | 1 |
  | `farm_pig_grow_duration` | 4 |
  | `farm_pig_grow_cost` | 250 |
  | `farm_fertility_max` | 1.2 |
  | `mill_rye_flour_yield` | 1 |
  | `actor_addictions_ale_roll_start` | 7 |

  이 326개는 파일로는 바꾸지 못했다(질문 3). 하위 프로젝트 2(런타임 접근)의 첫 대상이다:
  모듈이 `global.__gameplay_vars`의 멤버를 직접 쓰면 되는지 시험할 가치가 있다.

## 이름으로 찾은 것

| 이름 | 메인 메뉴에서 |
|---|---|
| `initial_budget`, `ai_economy` | `__gameplay_vars`의 멤버 이름 안에 들어 있다(`global_map_ai_economy_*` 11개) |
| `game_speed_slower_div`, `gameplay_hour_frames`, `farm_pig_grow_duration`, `mill_rye_flour_yield`, `death_chance_mult` | `__gameplay_vars`에 있다 |
| `budget_money`, `default_budget_money`, `battle_dodge_base`, `production_cost`, `building_resources`, `product_count`, `fair_trade_purchase` | 멤버 이름으로는 없다 |
| `budget_money_get`, `budget_money_change`, `budget_default_money_get`, `budget_default_money_set`, `get_dodge_chance_by_combat_skills`, `bf_unit_get_dodge_chance` | 전역에 함수(스크립트) 참조로 있다 |

전역 구조체 4,553개의 대부분은 이런 스크립트 함수 참조로 보인다(추정. 멤버가 비어 있다).

## 덤프 기능에 대해 알게 된 것

- **게임 스크립트를 인자가 틀린 채 부르면 게임이 끝난다.** 실행 1에서 `gml_Script_resource_default_count_get`에
  문자열 `"wood"`를 넘기자 `unable to convert string "wood" to integer` 오류 창이 뜨고 게임이 종료됐다.
  이 스크립트는 정수(자원 번호)를 받는다. 인자의 형을 모르는 스크립트는 부르지 않는다.
- 그래서 모듈은 찾기를 먼저 하고 스크립트 호출을 맨 뒤에 한다. 호출이 게임을 끝내도 그 앞의 결과는 파일에 남는다.
- `probe.ps1`은 게임이 도중에 끝났을 때 반쯤 쓰인 덤프를 가져오고, 요청 파일을 지우고, 실패로 끝났다(실행 1에서 확인).
- 찾기의 한도: 처음 값(방문 40만, 배열 2,048)으로는 소리·스프라이트·지도 점 같은 긴 배열에 한도를 다 쓰고
  끝까지 돌지 못했다. 방문 200만, 배열 64 이하로 바꾸니 38,682개 방문으로 완주했다.
- 덤프는 모듈이 적재되고 60초 뒤에 시작한다(요청 파일의 `delay_seconds`). 덤프 자체에 걸린 시간은 재지 않았다.
- 게임 종료: 실행 2와 3에서 `Stop-NlGame`이 `closed`를 돌려줬다(게임 창에 `WM_CLOSE`, 정상 종료).
- `global.instance_update_order`는 실행마다 값이 달랐다(5, 0, 1). 비교할 때 무시한다.
- 흔치 않은 값으로 찾을 때 우연한 일치가 있다. 745는 소리 에셋 배열에, 2345는 스프라이트 배열과 지도 점의 y에 있었다.

## 단계 1에 주는 결론

- **`gameplay_variables.json`은 오버레이 대상으로 확정이다.** 카탈로그의 첫 묶음은 런타임에 이름이 있는 94개다.
  파일에만 있는 56개는 카탈로그에 올리지 않는다.
- `debug_params.json`, `battle_params.json`, `director_params.json`, `knowledge\technology\*.json`은 아직 미확인이다.
  새 게임을 시작한 상태에서 재야 한다. 그러려면 사용자가 새 게임을 시작해 주거나, 모듈이 새 게임 진입을
  감지해 덤프하도록 바꿔야 한다.
- 파일에 없는 값(게임 속도 등)은 파일 오버레이로 닿지 않는다. 하위 프로젝트 2의 몫이다.
- 바뀐 값이 세이브에 굳는지는 보지 않았다.

## 확인하지 못한 것

- 2a, 2c와 그 밖의 데이터 파일(위).
- `gameplay_variables.json`의 93개 값 각각의 반영(이름·값 일치만 확인).
- 파일에만 있는 56개 키가 쓰이는지.
- 질문 3의 다른 중첩 경로.
- 새 게임을 시작한 뒤의 전역과 인스턴스 변수.
- `__gameplay_vars`의 값을 런타임에 바꾸면 게임이 그 값을 쓰는지(읽기만 했다).
