# 08 — 경제 패널: 금화, 영지 창고의 자원, 게임 속도의 함수

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 2.

- 실행 1 (`stage3b-session1`, 17:22~17:37, 모듈 `0.6.0`, 커밋 `e06911d`): 아래 "된 것"과 "처음 잰 것". 메뉴에서 0.5.1 의 수정을 확인한 뒤, 사용자가
  **저장된 게임을 불러왔다**(앞 실행의 자동 저장: 금화 3500, 나무 350 — `research/07`에서 바로 쓴 값이 세이브에 남아 있었다). 일시정지한 채 값을 쟀다.
- 실행 2 (`stage3b-session2`, 17:45~18:01, 모듈 `0.6.1`, 커밋 `be5679a`): 실행 1 에서 찾은 결함을 고친 DLL 의 확인. 아래 "실행 2 에서 확인한 것".

답은 `refs/runtime/stage3b-session1.answer.txt`(요청 115개)·`stage3b-session2.answer.txt`(68개), 로그는 같은 이름의 `.log`,
화면은 `refs/ui/economy-*.png`·`before-change.png`·`after-change.png`·`time-061.png`·`instant-build-only.png`(추적 안 함).

## 된 것

- **0.6.0 이 적재된다.** `session.ps1 -Action status` → `적재 판정: 통과`, 종료 코드 0.
- **메뉴에서**: `economy gold_add amount=1000` → `게임 화면이 아닙니다`(아무것도 부르지 않는다). `call budget_default_money_get` → `calling gml_Script_budget_default_money_get`(접두가 붙는다).
- **`about`**: `inst:o_time_controller.set_time_speed`·`is_paused` → `self bound`, `method would call it as bound`.
  `inst:o_game_map_controller.__province.__warehouse.change` → **`self unbound`**, `method would bind it to the owner`.
  생성자의 정적 메서드는 `method_get_self`가 `undefined`다(`research/07`이 재지 못한 것).
- **인자 없는 메서드 호출이 된다.** `method inst:o_time_controller.is_paused` → `-> bool true`, 훅에 `() -> 1`(빈 배열을 넘기는 길).
- **묶어 부르는 호출이 된다.** `method …__province.__warehouse.change n:28 n:5` → `-> number 5`, 기록 `(28, 5) -> 5`, `__total__[28]` 200 → 205.
  `method(창고, 함수)`로 묶은 뒤 `method_call`.
- **줄이는 호출이 된다.** `gml_Script_budget_money_change n:-1` → 금화 3500 → 3499. `change n:28 n:-5` → `-> -5`, `__total__[28]` 205 → 200.
- **경제 명령(`NlEconomy::Do`)**:

  | 명령 | 답 |
  |---|---|
  | `economy gold_add amount=1000` | gold 3499 -> 4499 |
  | `economy gold_set amount=5000` | gold 4499 -> 5000 |
  | `economy gold_set amount=4000` | gold 5000 -> 4000 (읽기 함수도 4000) |
  | `economy add resource=28 amount=10` | resource 28 200 -> 210 |
  | `economy set resource=28 amount=120` | resource 28 210 -> 120 |
  | `economy add resource=99 amount=5`, `resource=0` | 바꿀 것이 없습니다 |
  | `economy add resource=1 amount=50` | resource 1 350 -> 400 (용량 300 을 넘겨 들어갔다) |
  | `economy all amount=5` | 38/38 개. 1~38 번이 모두 5 늘고 0번(rune)은 그대로 |

- **화면이 따라온다.** 금화 `4000 +500`, 음식 `112/250`, 원자재 `400/300`(빨간색)(`refs/ui/economy-resource.png`). 자원도 함수로 바꾸면 HUD 가 고쳐진다.
- **패널이 그려진다**(`refs/ui/economy-panel.png`): 금화 4,000 과 단추 셋, 입력 칸(지금 금화 4000 으로 채워져 있다), 갈래와 용량, 자원의 이름과 수, 단추. 한글이 깨지지 않는다.
- **패널의 단추가 된다.** 사용자가 돌(29)·석재 타일(38)·점토 타일(35)의 `+100`을 눌렀고 로그에 `economy call warehouse.change(29, +100)` … `economy: called 1/1`이 남았다.
  세 자원의 `__total__`이 100 이 됐다(창 → 큐 → 틱의 길).
- **불러온 세이브에서도 자리와 함수가 같다.** 이 실행 전체가 불러온 게임에서 돌았다.
- **게임 속도를 게임의 함수로 건다.**

  | 호출 | 뒤의 값 | 흐름(실제 1초에 게임 시각) |
  |---|---|---|
  | `method …set_time_speed n:1` (손으로 멈춘 채) | `time_speed_index` 1, `time_warp_new`·`time_warp` 3, `is_hand_pause` 거짓. 안에서 `__set_warp(3)`을 부른다(기록) | 176.6 |
  | `method …__set_warp n:24` | `time_warp`·`time_warp_new` 24, `time_speed_index`는 1 그대로 | 1,427.4 |
  | `method …set_time_speed n:0` | `time_warp` 1 | 60.5 |

  흐름이 배속에 비례한다(3 → 2.9배, 24 → 23.6배). `set_time_speed(번호)`는 `time_speed_variants`(1, 3, 6, 12)의 번호를 받고 일시정지를 푼다.
  `__set_warp(배속)`은 배속 그 자체를 받는다.

## 처음 잰 것

- **게임의 화면이 보여 주는 자원의 수는 예약되지 않은 수다.** `__no_reserve__` = `__total__` − 예약. 당근: `__total__` 120, `__no_reserve__` 112, HUD 112.
  `change(28, 5)`를 부른 순간 `__total__`은 5 늘고 `__no_reserve__`는 200 → 197 이 됐다(그때 8 이 예약됐다). 그 뒤의 호출에서는 예약 8 이 그대로였다.
- **`change`는 용량을 보지 않는다.** 나무 350 → 400(용량 300), 전쟁 물자 55/20. 넘긴 것은 화면에 빨갛게 보인다. 넘긴 채 두면 무슨 일이 있는지는 보지 않았다.
- 창고 용량의 캐시(`…__cached_total_capacity_for_storage_type.raw`)에 999 를 쓰면 남는다(멈춘 채 3초). 창고 종류의 값(`…__warehouse_type.__capacity_in_categories.raw.capacity`)은 300 그대로였다.
  화면에 반영되는지, 게임이 언제 다시 만드는지는 보지 못했다(300 으로 되돌렸다).
- 게임의 화면이 쓰는 갈래의 말: 음식, 액체, 자원, 전쟁 물자, 식물, 원자재(`refs/ui/capacity-999.png`). 패널의 이름을 여기에 맞췄다.
- 바로 쓴 값은 세이브에 남는다: `research/07`에서 금화에 쓴 3500 과 나무 350 이 자동 저장에 들어 있었다.
- `research/07`의 "`set_time_speed(1)`이 `time_speed_index`를 바꾸지 않았다"는 튜토리얼 알림으로 멈춘 때였다. 손으로 멈춘 때는 바뀌고 풀린다.
- `__set_warp(24)` 뒤 `__time_warp`는 8.07696 이었다. 뜻은 모른다.

## 실행 1 에서 찾은 결함과 고친 것 (모듈 0.6.1)

- **왼쪽 목록의 "경제"가 꺼져 있었다**(`경제 (3단계)`). 표의 경제 항목을 모두 빼자 "보여 줄 것이 없는 영역"으로 보였다. 원격 명령 `page economy`로는 열렸다.
  → 영역에 "제 패널이 있다"는 표식(`AreaInfo::Panel`)을 두고 목록이 그것을 본다. 시험이 탐색기·경제·시간·배율을 확인한다.
- **패널의 수가 게임의 화면과 달랐다**(당근 120 대 112). → 패널은 예약되지 않은 수를 보이고 예약이 있으면 옆에 적는다. 맞추기와 줄이기의 기준도 그 수다
  (`120 으로 맞추기`는 화면이 120 을 보이게 한다). 청한 만큼 바뀌었는지는 `__total__`의 앞뒤로 본다.
- 갈래의 이름을 게임의 화면에 맞췄다(물자 → 자원, 약초·작물 → 식물). 안내 글의 "용량을 넘기면 다 들어가지 않을 수 있다"는 틀렸다(들어간다).
- **시간 패널이 게임의 함수로 배속을 건다**: 단추 x1·x3·x6·x12·x24·x50 → `o_time_controller.__set_warp(배속)`. 후보 값을 차례로 써 보던 시험(2단계)은 창에서 뺐다
  (`core/SpeedControl`은 흐름을 재는 데만 쓴다. 시험 부분의 정리는 3나-2).

## 실행 2 에서 확인한 것 (0.6.1)

- **적재된다.** `적재 판정: 통과`, 종료 코드 0, 로그의 첫 줄 `NlToyBox 0.6.1 loaded`.
- **왼쪽 목록의 "경제"가 켜져 있다**(`refs/ui/economy-061.png`). 갈래의 이름이 "자원"으로 보인다.
- **패널의 수가 게임의 화면과 같다.** `economy add resource=28 amount=5` → `resource 28 200 -> 197 (total 205)`. `economy set resource=28 amount=120` →
  `197 -> 120 (total 128)`. 패널은 `120 (예약 8)`, HUD 는 120(`refs/ui/economy-061-reserve.png`).
- **시간 패널의 단추가 된다.** 사용자가 `x12`를 눌렀다: 로그 `speed call __set_warp(12)`, `speed: ok`. `time_warp` 12, 흐름은 실제 1초에 726.4(배속 1 의 12배).
  패널은 흐름 60.0 과 단추 x1~x50 을 보인다(`refs/ui/time-061.png`).
- **즉시 건설을 그것만 켜고 봤다.** `is_instant_build_buildings` 참, `is_can_build_all_buildings` 거짓인 채 사용자가 건축가 작업장(`building.builders`)과
  병영(`building.barrack_6x10`)을 놓았다. 놓자마자 "평소처럼 작업 중"이고 `c_construction.__construction_status` 0(다 지은 건물과 같다).
  **건설 자원은 그대로 든다**: 나무 350 → 315(`refs/ui/instant-build-only.png`).
- 표가 처음 그려지는 프레임에는 열의 너비가 좁게 나온다(`refs/ui/menu-061b.png`. 불러오는 도중에 뜬 화면). 다음 프레임부터는 바르다.

## 3나-2 의 자리 (읽기만 했다)

- 거래: `…__trade_manager.__prices_manager`의 `__market_depth[39]`, `__market_saturation[39]`, `__low_price_saturation_factor`(1.5), `__market_depth_base_factor`(2),
  `__market_relaxation_factor`(0.2), `__market_depth_kingdom_peoples_linear_factor`(0.3). 상단의 물건: `…__caravan.__trade_products[방향]`의 원소 `{__count, __resource, __type}`.
  `…__caravan_resources[자원]`은 `{x, y}`(나무: 10, 60. 뜻은 모른다). 값이 정해지는 식은 보지 못했다.
- 건물 종류의 목록: `inst:o_game_map_controller.buildings_data.__cache_map_by_name`(ds_map. 키 `castle_3_0_0`, `brewery_3_0_0`, `forge_3_0_0` …, 값은 구조체).
  `global.__building_asset_storage`(`__cached_array_of_asset_files[3860]`, `__cached_asset_by_name`).
- 생산: `inst:o_province_controller.production_cost[0..5]` = 2, 0.2, 0.3, 1, 1.5, 2.
- 시간: `pause`, `resume`, `next_time_speed`, `pause_toggle`은 모두 인스턴스에 묶인 메서드다(`about`). 인자는 재지 않았다(이 실행에서 게임이 부르지 않았다).

## 확인하지 못한 것

- 멈춘 채 `__set_warp`를 부르면 어떻게 되는가. 정수가 아닌 배속. 게임의 단추를 누르지 않고 배속이 얼마나 남는가(4초는 남았다).
- 용량을 넘긴 재고에 게임이 무엇을 하는가(상함, 알림). 창고 용량을 올리는 길.
- 거래·임금·건설비·건설 조건·생산의 자리에 쓴 값의 효과.
- 금화와 자원이 소수가 되는 일이 있는가(넘기는 변화량은 언제나 정수다).

## 이 실행이 게임에 남긴 것

불러온 게임(자동 저장에서 이어진 것)에서: 금화 3500 → 4000, 당근 200 → 125, 나무 350 → 406, 돌·석재 타일·점토 타일 105, 그 밖의 자원 5~10.
게임 속도를 배속 1 로 두고 껐다(일시정지는 풀려 있었다). 켜기 전의 세이브 사본은 `backups\saves\20261005-171133`에 있다.
실행 2 는 같은 세이브를 다시 불러왔다(금화 3500 에서 시작): 당근 200 → 128, 건물 둘을 더 지었다. 사본은 `backups\saves\20261005-174538`.
