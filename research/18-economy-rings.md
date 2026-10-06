# 18. 경제: 신성 반지와 최소값 유지

사용자의 요청(2026-10-06): 경제에 신성 반지의 값 관리와 자원마다의 최소값 유지를 넣는다.
게임 버전 `0.5588.9777.0`. 실행 묶음 `eco-session1`(모듈 0.17.1. 답: `refs/runtime/eco-session1.answer.txt`), 세이브 "아덴"(4일차 06:00 ~ 15:25).
게임 창과 마우스는 건드리지 않았다(백그라운드). 저장하지 않고 껐다(새 세이브 파일은 생기지 않았다).

## 신성 반지는 자원 0번이다

- 게임의 현지화(`localization\main.csv` 1008행): `resource.rune` = "Holy rings" = "신성 반지". 런타임의 이름은 `rune`(`global.__resource_caption[0]` = `"resource.rune"`).
  경제 패널은 0번을 "룬"이라 부르고 건드리지 않았다(창고의 어느 갈래에도 들지 않는다).
- **영지의 반지는 영지 창고의 0번 칸에 있다**: `inst:o_game_map_controller.__province.__warehouse.__warehouse.__total__[0]` 7, `.__no_reserve__[0]` 7,
  `…__province.__warehouse.get(0) -> 7`. 게임의 화면에서 금화(1901) 옆에 반지 그림과 함께 7 이 보인다(`refs/ui/j1-before.png`).
- **다른 자원과 같은 함수로 바뀐다**: `…__province.__warehouse.change(0, 5) -> 5` 뒤 `__total__[0]`·`__no_reserve__[0]`·`get(0)`이 12, **화면의 반지도 12**(`refs/ui/j1-ring12.png`).
- 예산(`…__province.__budget`)에는 금화(`__budget.__no_reserve__`·`__total__`)만 있다. 반지의 칸은 없다.
- 게임 조건에 `__is_crutch_rings_to_gold_converted` 1 이 있다(세이브의 `"is_rings_to_gold_converted":1.0`). 무엇을 바꾼 표식인지는 재지 않았다.

## 영주의 반지

- 영주(`o_character`)의 소지품 `__soul.__inventory.__resources[0]`이 그 사람의 반지다. 다섯 영주 모두 0 이었다(소지금 `__money`는 0 ~ 493).
- `__soul.__inventory.change(0, 2)`(게임이 다른 번호로 부르는 꼴: `ComponentInventory.change(자원 번호, 변화량)`. `research/12`) 뒤 `__resources[0]`과 `get(0)`이 2.
  그 뒤 아홉 시간 동안 게임이 부른 **`gml_Script_character_runes_get_count(구조체)`가 2 를 돌려줬다**(587번 가운데 그 영주의 것. 나머지는 0).
- 게임이 부른 꼴(기록. 06:00 ~ 15:25): `character_runes_add(구조체, 수) -> undefined` 41번, `character_runes_del(구조체, 수) -> undefined` 57번(표본의 수는 모두 0),
  `character_runes_get_count(구조체) -> 수` 587번. 0번: `character_runes_add_from_budget`, `character_runes_del_to_budget`, `character_runes_transit`.
- 기계어로 읽은 인자 수: `character_runes_add` 2, `character_runes_del` 2 까지(인자 맞춤), `character_runes_del_all` 1, `character_runes_get_count` 1,
  `character_runes_del_to_budget` 1, `character_runes_transit` 3, `get_budget_transition_ring_count` 5, `get_cost_by_runes` 1. 모두 직접 호출이 있다(`script_calls.py`: 1 ~ 30곳).
  `character_runes_add`가 받는 구조체가 무엇인지(영혼인가)는 가리지 않았다. 그래서 그 스크립트는 부르지 않고 소지품의 `change`를 쓴다.

## 만든 것 (모듈 0.18.0)

- 경제 패널의 금화 아래에 **신성 반지** 줄: +1, +10, +100, 맞추기. 창고의 `change(반지의 번호, 변화량)`으로 바꾼다. 반지의 번호는 0 이라고 적지 않고 자원의 열쇠 `rune`의 자리로 찾는다(`NlCore::RingResource`).
  "모든 자원 +N"은 반지를 건드리지 않는다(`NlCore::EconomyTargets`: 하나씩 하는 것만 반지를 받는다).
- **최소값 유지**(치트 표의 `resource_floor`): 금화, 신성 반지, 갈래의 자원마다 '최소' 칸에 바닥을 적으면 틱이 1초마다 화면의 수(예약되지 않은 수)를 보고
  모자란 만큼 채운다(`NlCore::PlanFloors`. 채우는 길은 단추와 같다: `budget_money_change`, 창고의 `change`). 줄이지 않는다.
  바닥은 상태 파일의 `floor <자원의 열쇠|gold>=<수>` 줄에 남는다. 원격: `economy floor resource=<번호> amount=<수>`, `economy gold_floor amount=<수>`, `cheat resource_floor on|off`.
- 아이템 패널과 `person <uuid> item_add index=0`이 신성 반지도 받는다(전에는 0번을 거부했다).

## 확인 실행 (모듈 0.18.0, 2026-10-06)

실행 묶음 `eco-session2`(답: `refs/runtime/eco-session2.answer.txt`). 06:00 ~ 10:20, 저장하지 않고 껐다(새 세이브 파일 없음). 적재 판정 통과. 백그라운드.
처음: 금화 1901, 반지 7, 나무 4560, 당근 192.5(자원의 수는 정수가 아닐 수 있다).

| 한 것 | 본 것 |
|---|---|
| `economy add resource=0 amount=5` | 반지 7 → 12 (`__total__`도 12). 화면의 반지 12 (`refs/ui/k1-ring12.png`) |
| `economy set resource=0 amount=3` | 12 → 3 |
| `economy all amount=100` | 38개를 불렀다. 나무 4660, 당근 292.5, 돌 100. **반지는 3 그대로** |
| `economy add resource=0 amount=-100` | 3 → 0 (0 아래로 가지 않는다) |
| 바닥 넷(반지 10, 나무 6000, 당근 400, 금화 3000)을 정함. 스위치는 꺼진 채 | "…('최소값 유지'가 꺼져 있어 채우지 않습니다)". 3초 뒤에도 그대로 |
| 없는 자원 번호(99), 번호 없음, 바닥이 없는 자원에 0 | "그 자원에는 최소값을 둘 수 없습니다", 원격의 오류, "최소값이 없습니다: 돌 (stone)" |
| `cheat resource_floor on` | 3초 안에 금화 3000, 반지 10, 나무 6000, 당근 400.5 |
| 나무를 100 으로, 금화를 50 으로 낮춤 | 3초 안에 다시 6000, 3000. 패널: "최소값 4개를 지키는 중"(`refs/ui/k1-floors.png`) |
| 나무 +500 | 6500 그대로(줄이지 않는다) |
| 네 시간을 흘림(06:00 → 10:20) | 당근 400.5 그대로(먹힌 만큼 채워졌다), 금화 3000, 반지 10 |
| `cheat resource_floor off` 뒤 나무를 100 으로 | 100 그대로(채우지 않는다) |
| `economy floor resource=1 amount=0` 두 번 | "최소값을 지웠습니다: 나무 (wood)", "최소값이 없습니다: 나무 (wood)" |
| `person <Barra> item_add index=0 amount=3` | 소지품 0번 칸 3. 네 시간 동안 `character_runes_get_count`가 그 영주에게 3 을 돌려줬고(13번 가운데), 끝에도 3 |

- 상태 파일(그 실행의 것): `floor carrot=400`, `floor gold=3000`, `floor rune=10`(지운 나무는 없다).
- 로그: 네 시간(실제 30초쯤) 동안 `[floor]` 호출 줄은 자원마다 한 번이었다(같은 것을 채우는 줄은 30초에 한 번만 적는다).

## 확인하지 못한 것

- 게임을 다시 켠 뒤 상태 파일의 바닥이 그대로 걸리는 것(읽고 쓰는 것은 시험이 본다. `resource_floor`를 확인 항목으로 올린 뒤의 실행에서 본다).
- 영주의 반지를 준 것이 게임의 인물 창에 보이는가(수와 게임의 읽기 함수까지만 봤다). `character_runes_add`가 받는 구조체.
- 반지의 값(상단에서 사고파는 가격)을 바꾸는 길. `get_cost_by_runes(1)`, `get_budget_transition_ring_count(5)`의 뜻.
