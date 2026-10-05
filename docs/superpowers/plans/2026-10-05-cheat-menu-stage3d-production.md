# 치트 메뉴 3나-3: 거래·임금·창고 용량·생산 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 사용자가 준 목록의 "자원·경제"와 "생산·건설"에서 남은 것을 모드창에서 되게 한다: 구매가·판매가·거래량, 세금·임금·유지비, 창고 용량, 생산량·생산 시간·재료 소비·생산비·작업 효율·노동력.

**Architecture:** 3나-2 와 같은 길이다. 켜 둔 게임에서 `statics`·`record`로 그 판정을 내는 함수와 값의 자리를 찾고, `override`·`write`·`method`로 그 자리에서 풀어 본 뒤,
치트 표의 항목(`Toggle`·`Number`·`Hook`·`Custom`)으로 굳힌다. 수단은 A(게임의 스위치) → B(값 쓰기) → C(게임의 함수 부르기) → D(반환값 바꾸기)의 차례로 내려간다.

**Tech Stack:** C++20 모듈(Aurie v2.0.2, YYToolkit v5.0.0c, Dear ImGui), `tools/session.ps1`·`ask.ps1`, `tests/native`.

**Spec:** `docs/superpowers/specs/2026-10-05-cheat-menu-design.md` §8(경제, 건설·생산의 줄), §10(3나-3), §14(원격 명령).

## 아는 것과 모르는 것

아는 것(출처):

- exe 의 이름(`refs/exe_strings.txt`): 생성자 `TradeManager`(메서드 51), `TradeProduct`(28), `TradeProductResource`(19), `TradeSide`(22), `TradeTable`(29), `MarketSaturationManager`(25),
  `SalaryManagerNEW`(13), `ProvinceBudget`(10), `EconomicsDistributionTask`(53), `BuildingComponentProduction`(60), `BuildingComponentWorkplace`(55), `GenericBuildingProduction`(20),
  `ProductionOrder`(17), `ProductionOrderManager`(23), `ProvinceWarehouse`(16), `Warehouse`(32), `BuildingWarehouseType`(8), `BuildingStorage`(17).
  이름 있는 스크립트: `resource_price_get`(직접 호출 0곳), `resources_array_get_total_price`(3), `resource_get_production_count_per_item`(3),
  `resource_production_points_cost_get`(13)·`_set`(1), `building_generic_get_array_of_productions`(1), `imgui_debug_prices_setup`(1).
- 덤프에서 본 자리(스펙 §8): `…__trade_manager.__fair_trade_default_price_buy`·`_sell`(배열 39), `__salary_manager_new`, `__economics_manager`, `o_province_controller.production_cost`(배열 39), 건물의 `c_production`.
- 창고 용량: `…__province.__warehouse.__cached_total_capacity_for_storage_type.<갈래>`는 게임이 다시 채우는 캐시다(`research/08`).
- `o_debug`의 수: `debug_building_duration_factor` 같은 `debug_*` 값들(치트 표에 일부가 있다).

모르는 것(Task 1 이 잰다): 메서드의 이름과 꼴. 가격·임금·용량·생산량·생산 시간을 정하는 함수가 어느 것인가. 값을 쓰면 게임이 따르는가. 세이브에 남는가.

## Global Constraints

- 게임 스크립트는 `gml_Script_` 이름으로만 부르고 훅을 건다. 인자의 수와 형을 본 함수만 부른다(`record`의 기록이나 기계어). 자주 불리는 함수에는 기록을 오래 걸어 두지 않는다.
- 불리언·수를 돌려주는 함수만 `override`한다. 위험한 호출은 요청마다 따로, 그 요청의 맨 뒤에 둔다.
- 게임을 켜는 횟수는 사용자에게 승인받은 만큼만. 켜기 전에 세이브의 사본을 뜬다. 실행 중에는 사용자가 하는 것을 로그와 값으로 따라간다(차례를 주고 기다리지 않는다).
- 게임의 값을 고쳐 쓰는 항목은 세이브에 남는지를 잰다(켠 채 저장 → 게임을 다시 켜 치트 없이 불러와 읽는다).
- `Verified`는 플레이에서 효과를 본 뒤에만 참. 확인 전의 `Hook`·`Custom`은 꺼진 채로 시작한다.
- `refs/`, `backups/`, `downloads/`, `build/`는 커밋하지 않는다. 모드창의 글에는 한글과 라틴-1 만 쓴다.

## Review Focus

1. 값을 쓴 뒤 게임이 그 값을 다시 만드는 자리(캐시)에 "원래대로"가 낡은 값을 써 넣는 일.
2. 초당 수천 번 불리는 함수에 건 훅이 게임을 느리게 하는 일.
3. 가격·임금에 0 이나 음수를 썼을 때 게임이 나누거나 음수 재고를 만드는 일.
4. 켠 채 저장한 세이브에 고친 값이 남는 일.
5. 배율 7개를 옮긴 뒤 기존 `NlToyBox.settings.txt`가 읽히지 않는 일.

---

### Task 1: 조사 실행 (게임 1회)

**Files:** Create `research/10-trade-production.md`.

- [ ] **Step 1**: 배포(지금의 0.8.1), 세이브의 사본, `session.ps1 -Action start`, 적재 판정. 사용자가 세이브를 불러온다.
- [ ] **Step 2**: 관리자들의 이름과 메서드: `list inst:o_game_map_controller max=400`, `statics`를 `__trade_manager`, `__salary_manager_new`, `__economics_manager`, `__province.__warehouse`와 그 `.__warehouse`,
  생산 건물의 `c_production`·`c_workplace`·`generic`의 생산 부분에. 값의 나무(`tree`)를 가격 배열과 생산 주문에.
- [ ] **Step 3**: 이름으로 고른 후보에 `record`를 걸고 시간을 흘린다(생산은 스스로 돈다. 임금은 게임의 때에 치러진다. 거래는 상인이 있을 때 사용자가 거래 창을 연다). 기록에서 꼴과 반환값을 본다.
- [ ] **Step 4**: 그 자리에서 풀어 본다: 값 쓰기(`write`), 반환값 바꾸기(`override`), 게임의 함수 부르기(`method`). 화면(`shot`)과 창고·금화의 수로 효과를 본다.
- [ ] **Step 5**: 끄고(`session.ps1 -Action stop -Name stage3d-session1`) 잰 것을 `research/10-trade-production.md`에 적는다. 커밋.

### Task 2: 치트로 굳힌다 (Task 1 의 답으로 채운다)

- [ ] 치트 표에 항목을 더한다(영역: 경제, 건설·생산). 값 쓰기로 되는 것은 `Number`·`Toggle`, 판정은 `Hook`, 여럿을 돌며 쓰는 것은 `Custom`(`src/Build.cpp`의 꼴: 처음 본 값의 장부, 쓴 뒤 다시 읽기, `core/Retry`).
- [ ] 러너와 무관한 부분은 `src/core`에 두고 시험을 먼저 쓴다(RED → GREEN). 치트 표의 수를 세는 시험을 고친다.

### Task 3: 배율 7개를 영역으로, 속도 시험의 정리 (게임 없이)

- [ ] `src/core/SpeedTrial`과 `SpeedControl`의 시험 부분을 뺀다(창은 이미 `__set_warp`로 건다. 흐름을 재는 부분만 남긴다). 시험을 그에 맞게 줄인다.
- [ ] 배율 7개(`src/Tweaks.cpp`)를 제 영역의 패널에 그린다(건설 비용 → 건설·생산, 시작 자원 → 경제, 교본 경험 → 지식, 뇌물 → 외교, 자유 영주 체류 → 영주, 교회·선술집 수용 → 종교·인구).
  왼쪽 목록의 "배율"을 없앤다. `NlToyBox.settings.txt`의 형식은 그대로 둔다.

### Task 4: 검토, 확인 실행, 문서, 머지

- [ ] 독립 코드 검토(브랜치 전체) → Critical·Important 를 시험과 함께 고친다.
- [ ] 확인 실행(사용자 승인): 적재 판정, 새 항목마다 켜고 효과를 본다(사용자가 하는 것을 따라가며 값과 화면으로). 세이브에 남는지 잰다.
- [ ] `research/10`, `CLAUDE.md`, `README.md`, 스펙 §8·§10. 빌드·시험 넷·적재 판정 뒤 `git merge --no-ff feat/cheat-production` → `develop`.

## Self-Review

- **스펙 대조**: §8 의 경제 줄(구매가·판매가·거래량, 세금·임금·유지비, 자원 최대치)과 건설·생산 줄(생산량·생산 속도·재료 소비·생산비)이 Task 1·2 에, §10 의 "배율 7개를 표로", "속도 시험의 정리"가 Task 3 에 있다.
- **빈칸**: Task 2 는 Task 1 의 답이 있어야 쓸 수 있다. 어느 함수를 바꿀지 지금 적으면 추측이다. 꼴만 정했다.
- **Review Focus**: 1·4 는 Task 1 Step 4 와 Task 4 의 실행에서 잰다. 2 는 `record`의 호출 수로 본다. 3 은 Task 2 에서 수의 범위(표의 `Min`)로 막고 시험한다. 5 는 Task 3 의 시험.
