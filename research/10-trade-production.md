# 10 — 거래, 창고 용량, 생산

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 2(+ 사용자가 직접 켠 한 번).
조사 실행 `stage3d-session1`(20:28~21:02, 모듈 `0.8.1`): 사용자가 세이브를 불러와 플레이했다(건물 21 → 48채, 상인과 거래 한 번).
확인 실행 `stage3d-session2`(21:11~21:48, 모듈 `0.9.0`)는 아래 "확인 실행"에 적었다.

답은 `refs/runtime/stage3d-session1.answer.txt`, 로그는 `stage3d-session1.log`, 화면은 `refs/ui/d1-*.png`(추적 안 함).

## 관리자들

`inst:o_game_map_controller`의 멤버 80개 가운데: `__trade_manager`(`TradeManager`), `__trade_agreements_manager`, `__salary_manager_new`(`SalaryManagerNEW`),
`__economics_manager`(`EconomicsMenuManager`), `__production_orders_manager`(`ProductionOrderManager`), `__province`(`Province`. 그 안에 `__warehouse`: `ProvinceWarehouse`, `__budget`: `ProvinceBudget`).

## 거래

- **기본 가격**: `…__trade_manager.__fair_trade_default_price_buy[39]`·`_sell[39]`(나무 8/4, 룬 100/50, 고기 60/40 …). `__resource_price_factor[39]`는 모두 1 이었다.
- **게임이 값을 얻는 길**(기록): `buy_default_get(자원) -> 수`, `sell_default_get(자원) -> 수`(자원은 `int64`나 수로 온다), `get_price_factor(자원) -> 1`.
  거래 탁자의 상품(`…__trade_table.__trade_sides[0|1].__trade_products[갈래][n]`: `TradeProductResource`)은 값을 들고 있지 않다(`__count`, `__resource`뿐).
  `__get_raw_price(방향) -> 수`(0 판매, 1 구매. 에일: 30, 40), `__get_raw_price_spread() -> 10`(구매 − 판매), `get_price(방향, 수) -> 수`로 그때그때 셈한다.
  거래 창이 열려 있는 동안 `buy_default_get` 1,152번, `sell_default_get` 2,168번, `get_price_factor` 3,291번이 불렸다.
- **기술 차이**: `__trade_table.get_price_factor_by_skills() -> 0.4`. 화면의 "+40% 이익, 사유: 무역 스킬 차이"와 같다.
- **열려 있는 탁자의 칸은 값을 다시 읽지 않는다.** 거래 창이 열린 채 `_sell[28]`(당근)을 5 → 50 으로 썼지만 칸의 값은 6 그대로였다. 칸은 창을 열 때 만들어진다(`init_products`).
- **시장 깊이**: `…__trade_manager.__prices_manager`(`MarketSaturationManager`). `__market_depth[39]`(나무 130, 룬 15, 철 11 …), `__market_saturation[39]`(모두 0),
  `__market_depth_base_factor` 2, `__market_relaxation_factor` 0.2, `__low_price_saturation_factor` 1.5.
  기록: `get_market_depth(자원) -> 15`, `get_market_saturation(자원) -> 0`, `get_price_factor_by_saturation(자원, 수) -> 1`, `get_player_sell_price_factor_by_saturation(수, 수) -> 1`, `get_low_price_factor_max() -> 0.2`.
- **상단**: `is_caravan_exist() -> true`, `is_caravan_arrived() -> 불리언`. 거래가 맺어질 때 `__trade_table.deal() -> undefined`, 끝날 때 `trade_end()`.
- 이름 있는 스크립트 `resource_price_get`은 직접 호출이 0곳이고 기록도 0번이었다. `resources_array_get_total_price(배열) -> 수`는 불린다.

## 창고 용량

- **용량을 돌려주는 함수만 바꾸면 화면만 바뀐다.** `…__province.__warehouse.get_total_capacity_for_category(갈래 이름) -> 수`(`"food"` → 250)를 99999 로 바꾸자 HUD 가 `음식: 160/99999`가 됐다.
  하지만 자정의 부패 처리(`spoilage_process()`)는 원래 용량으로 깎았다: 나무 3,300 → 3,000(그때의 원자재 용량).
- **부패 처리는 넘친 양의 4분의 1 을 깎는다**(한 번 잰 것): 나무 5,000, 용량 3,000 에서 `spoilage_process()`를 부르자 4,500 이 됐고 `__spoilage_resources_today[1]`이 500 이 됐다.
  캐시(`__cached_total_capacity_for_storage_type.raw`)를 30,000 으로 써 둔 채였다. 캐시는 부패 처리가 읽는 값이 아니다.
- **실제 용량의 자리**: `inst:o_data.__building_warehouse_data.__generic_warehouses.<종류>.__capacity_in_categories.<갈래>.{capacity, priority, spoilage_factor}`.
  종류와 갈래(이 빌드): `hall`(armory 20, food 250, herbs 300, liquid 100, raw 300, resources 50), `storage`(armory 20, food 100, herbs 100, liquid 100, raw 300, resources 50. food·herbs 의 `spoilage_factor` 0.1),
  `granary`(food 300, herbs 300), `armory`(armory 30), `default`(비어 있다). 영지 창고의 `__warehouse_type`은 `hall`과 같은 구조체다.
- **그 값을 올리면 실제로 넘치지 않는다.** `storage`와 `hall`의 `raw.capacity`를 300 → 3,000 으로 쓰고 `clean_cached_total_capacity_for_storage_type()`(인자 없음. 게임이 창고가 바뀔 때마다 부른다)를 부른 뒤
  `spoilage_process()`를 불렀다: 나무 4,500 그대로, 오늘 썩은 양 0. HUD 는 `원자재: 4500/30000`.

## 생산

- **생산 점수의 비용**: `gml_Script_resource_production_points_cost_get(자원) -> 수`(직접 호출 13곳). `inst:o_province_controller.production_cost[자원]` × 3600 이다
  (맥주 1 → 3600, 에일 2 → 7200, 밀주 0.6 → 2160, 종이 5 → 18000). 배열에 쓰면 남는다.
- **건물의 생산 구성요소**: `inst:o_building:<n>.c_production`(`BuildingComponentProduction`, 메서드 60여 개. 농장과 돼지 농장은 `__c_farm`, `__c_pig_farm`이 더 있다).
  값: `__production_points`, `__current_order`(주문한 자원. 없으면 -4), `__upgrade_factor`(돼지 농장 3등급 1.2), `__perfomance_factor`.
  기록: `collect_production_points(일꾼, 초) -> 점수`(75.7초 → 107.0점), `get_worker_base_performance_factor(일꾼) -> 1`, `get_worker_bonus_performance_factor(일꾼) -> 1`,
  `__get_upgrade_factor() -> 1.2`, `get_performance_factor() -> 구조체`, `get_production_points() -> 수`, `is_can_produce_product(자원) -> 불리언`, `__is_not_enough_resources() -> false`.
- **조리법**: 건물 종류의 `__production`(`GenericBuildingProduction`)`.__map_of_production`(ds_map 의 번호. 열쇠는 만드는 자원의 번호) →
  `{ pile_of_raw_resources.__array_of_resource_quantity[39], produced_resource.{__quantity, __resource} }`.
  작업장: 망치(10) = 철 1 + 목재 1, 칼(14) = 철 1, 도구(5) = 철 1 + 목재 1. 용광로: 강철(22) = 철 1 + 석탄 1. 광산: 철(4) 1개(재료 없음). 돼지 농장: 고기(23) 250. 약초상: 약초(24), 열매(33).
  `__workplace.__time_range`는 [8, 18](일하는 시각), `__workers.__number_of_workers_initial`·`_per_level`.
- **생산 주문이 없으면 만들지 않는다.** 사용자가 지은 광산·용광로·작업장·약초상은 일꾼이 둘씩 있었지만 `__current_order`가 -4 였고 낮(09:52)에도 `__production_points`가 0 이었다.
  주문은 `…__production_orders_manager`(`order_create`, `find_order_by_resource`, `get_list_of_orders`. 꼴은 보지 못했다)가 다룬다.
- 건물의 작업장 구성요소: `c_workplace`(`BuildingComponentWorkplace`): `__array_of_workers`, `__limit_number_of_workers`, `get_max_number_of_workers`, `set_limit_number_of_workers`, `hire_worker`.

## 임금

- `…__salary_manager_new.__salary[4]`와 `__slave_salary[11]`은 이 세이브에서 모두 0 이었다. `get_salary_legacy`·`get_slaves_salary_legacy`는 한 번도 불리지 않았다. 재지 못했다.

## 처음 잰 것

- **한 실행에 훅을 64개까지 건다. 이 실행에서 다 썼다**(`no free hook slot (64 in use)`). 후보를 넓게 걸면 30분쯤에 바닥난다. 기록은 고른 것에만 건다.
- `record`는 이미 기록 중인 함수에 다시 걸면 표본을 비우고 새로 받는다. 바꾼 뒤의 호출을 보려면 다시 건다(표본은 꼴마다 처음 몇 개만 남는다).
- 게임이 인자 없이 부르는 것을 기록한 `spoilage_process`, `clean_cached_total_capacity_for_storage_type`를 `method`로 직접 불러 그 자리에서 결과를 봤다.
- 사용자가 게임의 속도 단추를 누르면 `__set_warp`로 건 배속은 바로 풀린다(24 를 걸고 5초 뒤 1 이었다).

## 치트로 굳힌 것 (모듈 0.9.1)

- 치트 표에 종류 둘: `HookScale`(함수가 돌려주는 수에 창에서 정한 배율을 곱한다. `NlRecorder`의 `Forced` `'x'`), `CustomScale`(모듈의 코드가 그 배율로 한다).
- 함수의 결과에 곱하는 것(게임의 자료를 쓰지 않는다): `buy_price`(`buy_default_get`), `sell_price`(`sell_default_get`), `market_depth`(`get_market_depth`),
  `production_time`(`resource_production_points_cost_get`), `worker_performance`(`get_worker_base_performance_factor`).
- 자료에 쓰는 것(`src/Production.cpp`. 처음 본 값의 장부, 쓴 뒤 다시 읽기, 끄면 되돌리기): `storage_capacity`(창고 종류의 용량 15칸), `production_amount`(조리법의 만들어지는 수 34칸),
  `production_free`(조리법의 재료 47칸을 0 으로).
- 원격 명령 `override <대상> x:<배율> [whole]`, `cheat <Id> <수>`.
- 배율 7개(건설 비용 등)를 제 영역의 패널로 옮기고 "배율" 영역을 없앴다. 쓰이지 않던 속도 시험 코드를 지웠다.

## 확인 실행 (2026-10-05, `stage3d-session2`, 모듈 0.9.0)

답과 로그는 `refs/runtime/stage3d-session2.*`, 화면은 `refs/ui/v1-*.png`. `적재 판정: 통과`. 사용자가 20:45 의 자동 저장을 불러왔다. 항목은 원격 `cheat <Id> <수>`로 켰다.

### 세이브에 남는가 (게임을 다시 켜기 전에 쟀다)

- 사용자가 21:05 에 게임을 직접 켜 20:45 의 자동 저장을 불러왔다. 그 세이브는 당근 판매가 50·룬 구매가 10·시장 깊이 계수 20 을 써 둔 채 저장된 것인데, 읽으니 5·100·2 였다.
- **세이브 파일은 머리(버전, 이름) 뒤가 평문 JSON 이다.** 그 파일에서 `fair_trade_default_price`, `capacity_in_categories`, `generic_warehouses`, `production_cost`,
  `map_of_production`, `produced_resource`, `construction_cost`는 0건이었다. `market_depth` 4건, `market_saturation` 1건, `prices_manager` 1건은 있다(시장의 상태는 저장된다. 우리는 그 값을 쓰지 않는다).
- 그러므로 자료에 쓰는 항목(창고 용량, 조리법)은 세이브에 남지 않는다.

### 창고 용량 (storage_capacity) — 확인

- `cheat storage_capacity 10` → 로그 `capacity: wrote 15 value(s) to x10, 15 hold it, book 15`, 이어 `clean_cached_total_capacity_for_storage_type`를 부른다.
  15칸이 모두 정확히 10배였고(두 번 곱한 칸 없음) 그 뒤로 다시 쓰지 않았다. `get_total_capacity_for_category("raw")` 3,000 → 30,000, `("food")` 2,050 → 20,500. HUD `원자재: 4500/30000`.
- 그 채로 `spoilage_process()`를 불렀다: 나무 4,500(원래 용량 3,000) 그대로, 오늘 썩은 양 0.
- 끄면 `wrote 15 value(s) to the first values … book 0`, 값이 300·250·30 으로, 함수의 답이 3,000 으로 돌아왔다.

### 거래 (buy_price, sell_price, market_depth) — 함수까지만

- 0.5·3·10 으로 켠 채 게임의 함수를 불렀다: `buy_default_get(0)` 100 → 50, `(1)` 8 → 4, `(25)` 5 → 3(정수로 남는다), `sell_default_get(28)` 5 → 15, `(23)` 40 → 120,
  `get_market_depth(1)` 130 → 1,300, `(35)` 0 → 0. 자료의 배열은 그대로였다(100, 5). 끄면 100·5·130.
- 이 실행에는 상단이 없었다(`is_caravan_exist() -> false`). **거래 창의 값과 치르는 금화로는 보지 못했다.**

### 생산 (production_time, worker_performance, production_amount) — 확인

- 함수 수준: 게임이 스스로 부른 `resource_production_points_cost_get(25)`가 `1260 => 126`(35,793번). `get_worker_base_performance_factor`가 `1 => 5`,
  그 결과 `collect_production_points(일꾼, 72.2초) -> 380.1`(조사 실행에서는 75.7초에 107.0).
- 자료: `production amount: wrote 34 value(s) to x5`, 작업장의 조리법에서 만들어지는 수가 1 → 5. 그 뒤로 다시 쓰지 않았다.
- **산출**: 0.1·5·5 를 함께 켠 날, 사용자가 광산에 철 주문을 넣은 뒤(12:50~13:38 사이) 18시까지 광산이 철 1,176개를 만들었다(창고 665 + 건물 안 511. 17:16 에 건물 안 488).
  바닐라의 산출은 시간당 1.4개쯤이다(일꾼 둘 × 초당 1.055점 ÷ 5,400점). 셋 가운데 둘만 먹었다면 다섯 시간에 많아야 350개다. 셋이 다 먹으면 1,760개까지다.
  같은 날 식물은 2 → 1,105, 당근은 115 → 192 가 됐다.
- 배율을 하나씩 켜 가며 가르는 측정은 하지 못했다(다음 날 아침 광산의 일꾼이 09:49 까지 일을 시작하지 않았다).

### 생산 재료 없음 (production_free) — 판정까지만

- `production inputs: wrote 47 value(s) to 0`. 철 0·목재 0 인데 작업장의 `is_can_produce_product(14)`(칼)·`(10)`(망치)가 참이었다. 끄자 거짓이 되고 재료가 1 로 돌아왔다. 다시 켜면 참.
- 재료가 드는 물건이 실제로 만들어지는 것은 보지 못했다(작업장에 주문이 없었다).

### 그 밖에 본 것

- 모드창: 왼쪽 목록에서 "배율"이 없어지고 영주·지식이 켜졌다. 건설·생산 패널에 새 항목(체크, 배율, 상태 글)과 그 아래 "배율" 구획의 "건물 건설 비용"이 그려진다.
  경제 패널에서는 새 항목이 자원 39줄 아래에 있어 보이지 않았다 → 표의 항목과 배율을 자원의 표 위로 올렸다(0.9.1. 화면으로는 다시 보지 않았다).
- 이야기 이벤트의 창("신제국", 4일 차 08:00)이 뜨면 게임이 멈춘다. 사용자가 "계속하기"를 눌러야 풀린다(원격으로는 누를 수 없다).
- `__set_warp(24)`로 건 배속이 두 번 풀렸다(한 번은 30초 안에 1 로). 누가 풀었는지는 모른다.
- 게임의 오류 기록에 `_struct_copy_from is not a struct!`가 10번 났다(사용자가 즉시 건설·건설 조건을 켜고 지은 때. `research/09`와 같은 경고).

## 확인하지 못한 것

- **가격·거래량의 배율이 거래 창의 값과 실제로 치르는 금화에 반영되는가**(상단이 있을 때 거래 창을 열어 본다). 구매가를 내리고 판매가를 올려 판매가가 구매가를 넘을 때 게임이 어떻게 받는가.
- **생산 재료 없음을 켠 채 재료가 드는 물건이 만들어지고 재고가 줄지 않는가.**
- 생산 배율 셋을 하나씩 켰을 때의 산출(함께 켠 산출과 함수 수준으로만 봤다).
- 임금, 세금(봉신), 유지비(군대), 노동력(일꾼 수의 조건). 조사한 세이브에 값이 없었다. 항목도 만들지 않았다.
- 시장 깊이가 언제 다시 셈해지는가(`__market_depth_calculate`).
- 0.9.1 의 경제 패널(항목을 자원의 표 위로 올린 것)의 화면.

## 이 실행들이 게임에 남긴 것

조사 실행에서 시험으로 쓴 값은 끄기 전에 되돌렸다: 당근 판매가(50 → 5), 룬 구매가(10 → 100), 시장 깊이 계수(20 → 2), 광산의 철 생산 수(10 → 1), `storage`·`hall`의 원자재 용량(3,000 → 300),
넣은 나무(5,000 가운데 부패 처리로 800 이 깎였고 나머지를 빼 300 으로). 그 값들은 세이브에 남지 않는다(위). 20:45 의 자동 저장에는 그때 넣어 둔 나무가 들어 있다(불러오니 4,500).
확인 실행에서는 항목을 모두 끄고(자료 96칸을 되돌리고) 껐다. 그 실행의 자동 저장 둘(`아덴_Autosave_Evening_day_3_…_21_26`, `…_Morning_day_3_…_21_35`)에는 배율을 켠 하루의 산출(철 665 등)이 들어 있다.
사용자의 플레이는 저장하지 않고 껐다. 켜기 전의 세이브 사본: `backups\saves\20261005-202837`, `20261005-211141`.
