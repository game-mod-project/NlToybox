# 07 — 원격 질의와 호출 기록기, 게임 화면에서 찾은 것

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`, 모듈 `0.5.0`(커밋 `eca9898`). 게임을 켠 횟수: 1
(실행 묶음 `stage3-session1`, 15:50~16:12). 메뉴에서 통로와 훅을 확인한 뒤 사용자가 새 게임을 시작했고, 게임 화면에서 값과 함수를 찾았다.

답은 `refs/runtime/stage3-session1.answer.txt`(요청 64개, 명령 192줄), 로그는 `stage3-session1.log`, 화면은 `refs/ui/gold-*.png`·`wood-*.png`(추적 안 함). 다시 재려면:

    pwsh -File tools/session.ps1 -Action start
    & tools\ask.ps1 -Lines 'state', 'call gml_Script_budget_money_get'
    pwsh -File tools/session.ps1 -Action stop -Name <이름>

## 된 것

- **통로가 열린다.** 모듈이 적재되고 4.4초 뒤의 `state`에 답이 왔다. 메뉴가 뜨기 전(`o_main_menu 0`)에도 답한다.
- **묻기·쓰기.** `ask`, `list`, `tree`, `poke`가 메뉴에서 됐다(`poke inst:o_time_controller.time_warp_max=101` → `read 101 (stuck); restored 100`).
- **게임 스크립트에 훅이 걸린다.** `record gml_Script_budget_default_money_get` 뒤 `call`로 부르자 기록에 `calls 1, shape () x1`이 남았다.
  `MmCreateHook`이 YYC 스크립트 함수의 들머리에 걸리고 트램펄린으로 원래 함수가 돈다.
- **메서드에서 스크립트 이름을 얻는다.** `record inst:o_time_controller.set_time_speed` →
  `gml_Script_anon_gml_Object_o_time_controller_Create_0_595227106_gml_Object_o_time_controller_Create_0`(`script_get_name`이 메서드를 받는다).
- **화면을 뜬다.** `shot <이름>` → `refs\ui\<이름>.png`. 모드창이 닫혀 있어도 뜬다.
- **금화를 더한다(화면에서 효과를 봤다).** `call gml_Script_budget_money_change n:100` 뒤 HUD 가 `3600 +100`이 됐고
  `gml_Script_budget_money_get`이 3600 을 돌려줬다(`refs/ui/gold-plus100.png`).

## 게임 화면에서 찾은 것

### 금화

| 것 | 자리 또는 함수 | 잰 것 |
|---|---|---|
| 영지의 금화 | `inst:o_game_map_controller.__province.__budget.__budget.__no_reserve__`, `.__total__` | 새 게임 3000 |
| 읽기 | `gml_Script_budget_money_get()` | 인자 없이 3500(써 넣은 값). 안에서 `ProvinceBudget.get("__no_reserve__")`를 부른다(기록) |
| 바꾸기 | `gml_Script_budget_money_change(변화량)` | `(100) -> undefined`, 값 3500 → 3600, HUD 갱신 |
| 새 게임 조건 | `global.__main_menu_manager.__conditions_editor.__gold` | 3000 (난이도 `__difficulty` 1) |
| 인물의 금화 | `inst:o_character.__soul.__inventory.__money` | 493 |

- **값을 바로 쓰면 게임은 따르지만 화면은 따라오지 않는다.** `__no_reserve__`와 `__total__`에 3500 을 쓰자 게임의 읽기 함수는 3500 을 돌려줬지만
  HUD 는 3000 그대로였다(15분 넘게). HUD 는 값을 프레임마다 읽지 않고 `main_budget_change` 이벤트(구독자 28)로 고친다.
  `budget_money_change`를 부르면 값과 화면이 함께 바뀐다. **금화는 함수로 바꾼다.**
- `budget_money_change`의 인자가 하나라는 것은 본문의 기계어로 먼저 읽었다(아래 "인자 수를 exe 에서 읽는다").

### 자원

- 이름: `global.__resource_caption[39]`. 0 `rune`, 1 `wood`, 2 `food`, 3 `beer`, 4 `iron`, 5 `instruments`, 6 `light_armor`, 7 `heavy_armor`, 8 `bow`,
  9 `crossbow`, 10 `wooden_hammer`, 11 `wooden_spear`, 12 `sword`, 13 `battle_axe`, 14 `knife`, 15 `shield`, 16 `medicine`, 17 `coal`, 18 `nectar`,
  19 `paper`, 20 `hop`, 21 `rye`, 22 `steel`, 23 `meat`, 24 `herb`, 25 `sweden`, 26 `moonshine`, 27 `ale`, 28 `carrot`, 29 `stone`,
  30 `instruments_steel`, 31 `firewood`, 32 `wood_blanks`, 33 `berry`, 34 `herb_drink`, 35 `clay_tile`, 36 `clay_pot`, 37 `clay_roof_tile`, 38 `stone_tile`
  (값은 `"resource.<이름>"`).
- 갈래: `inst:o_data.__resource_categories_data.__categories.<food|liquid|resources|armory|herbs|raw>.__resources_in_category`.
  food 23·33·28·2·25, liquid 26·3·27·18·16·34, resources 5·30·17·22·19·4·31·32·36·37, armory 12·13·11·10·14·8·9·15·6·7, herbs 24·21·20, raw 1·29·35·38.
- 영지 창고(HUD 의 수): `inst:o_game_map_controller.__province.__warehouse.__warehouse.__no_reserve__[39]`, `.__total__[39]`.
  새 게임: `[1]` 300, `[10]` 5, `[16]` 5, `[28]` 200. 게임이 먹은 뒤 `[28]`이 185 였고 HUD 도 185 였다.
- 용량: `…__warehouse.__cached_total_capacity_for_storage_type.<갈래>`(armory 20, food 250, herbs 300, liquid 100, raw 300, resources 50)와
  `…__warehouse.__warehouse_type.__capacity_in_categories.<갈래>.capacity`.
- 건물의 창고는 영지 창고와 따로다. `inst:o_building`마다 `__warehouse.__warehouse.__no_reserve__[39]`가 있고, 영주관(`…__province.__cached_hall`)의 나무는 0 이었다.
- `gml_Script_building_warehouse_get(건물, 자원 번호, undefined)` → 수. 게임이 `(ref, number, undefined)`로 117번 불렀다.
- `gml_Script_building_warehouse_change(건물, 자원 번호, 변화량)` → 변화량. `(영주관, 1, 5) -> 5`로 영주관의 나무가 0 → 5 가 됐고, `-10`으로 되돌렸다.
  안에서 `건물.get_warehouse().change(자원 번호, 변화량)`을 부른다: 훅을 건 `Warehouse.change`에 `(1, 5) -> 5`가 남았다.
- 영지 창고의 `change`와 건물 창고의 `change`는 같은 함수다
  (`gml_Script_anon_Warehouse_gml_GlobalScript_ProvinceWarehouse_3640123903_…`. `record …__province.__warehouse.change`가 건 훅에 건물 창고의 호출이 잡혔다).
- **영지 창고에 값을 바로 쓰면 화면은 따라오지 않는다**(나무 `[1]`을 350 으로 썼고 HUD 는 300 이었다). 금화와 같다. `main_resource_change` 이벤트(구독자 11)가 있다.

### 그 밖의 자리 (읽기만 했다)

- 거래: `inst:o_game_map_controller.__trade_manager`의 `__fair_trade_default_price_buy[39]`·`_sell[39]`, `__resource_price_factor[39]`,
  `__caravan_resources[39]`, `__caravan_resources_not_for_sale[39]`, `__caravan.__caravan_budget`(220), `__prices_manager`.
  영지 안의 값: `…__province.__resources_manager.__resources_price[39]`, `.__resource_sale_limiter[39]`.
- 임금: `…__salary_manager_new.__salary[4]`, `.__slave_salary[11]`(모두 0 이었다).
- 건설: 건물의 `c_construction`(`__construction_progress`, `__construction_status`, `__construction_time`, `__construction_resources.__array_of_resource_quantity[39]`),
  건물 종류의 `generic.__construction_cost.levels[5]`(`money`, `resources.__array_of_resource_quantity[39]`), `generic.__is_instant_build`,
  `…__construction_manager.__free_buildings`(`__tokens`, `__replacement_map.<건물 이름>`).
- 생산: `inst:o_province_controller.production_cost[39]`, `default_resource_count[39]`, `…__production_orders_manager`.
- 인물: `inst:o_character.__soul.__skills.__level.<combat|command|education|knowledge|management|manners|negotiation|oratory>`,
  `__soul.__moral`, `__soul.__traits`, `__soul.__minds`, `__soul.__fealty`, `__soul.__aging`, `__soul.__equipment`.
- 지식: `…__knowledge_manager.__knowledge_unlockeds[19]`, `__permanent_knowledge_names`.
- 시간: `inst:o_time_controller`의 메서드 `pause`, `resume`, `pause_toggle`, `next_time_speed`, `set_time_speed`, `__set_warp`, `set_slowdown`, `set_time_flow`.
- 이벤트: `global.__pub_sub_controller.__events.__events_by_name`에 이름 764개. `main_budget_change`, `main_resource_change`, `building_warehouse_resource_change`,
  `salary_change`, `time_speed_set`, `time_warp_changed`, `pause_toggle` 등.
- `o_debug`의 변수 127개(목록은 답 파일). 실행 중 `is_can_build_all_buildings`, `is_instant_build_buildings`, `is_resources_edit_mode`가 참이었다
  (모듈은 `cheat state: none`으로 시작했다. 사용자가 모드창에서 켰다). 그 사이 `o_building`이 14 → 17 이 됐다. 효과는 아래 "사용자가 플레이에서 본 것".

## 처음 잰 것

- **접두 없는 이름으로 부르면 다른 루틴이 불린다.** `call budget_money_get`은 `-> undefined`였고 훅을 건 함수에 호출이 남지 않았다.
  `call gml_Script_budget_money_get`은 `-> number 3500`이고 기록에 `() -> 3500`이 남았다. `Code_Function_Find`가 접두 없는 이름에도 스크립트 범위의 번호를 준다.
  그 번호가 무엇인지는 모른다(죽지는 않았다). **부를 때와 훅을 걸 때는 `gml_Script_` 이름만 쓴다.**
- **직접 호출이 없는 함수는 기록할 수 없다.** exe 의 `.text`에서 그 함수로 가는 `call rel32`를 세면:

  | 함수 | 직접 호출 | 실행에서 |
  |---|---|---|
  | `pub_sub_event_perform` | 0 | 시각이 한 시간을 넘겨도 0번 |
  | `time_hour` | 0 | 0번 |
  | `resource_price_get` | 0 | (걸지 않았다) |
  | `budget_money_get` | 31 | 게임이 `(undefined) -> 3500`으로 2번 |
  | `budget_money_change` | 11 | 게임은 0번(금화가 바뀔 일이 없었다) |
  | `building_warehouse_get` | 12 | 117번 |
  | `building_warehouse_change` | 9 | 게임은 0번 |
  | `character_gold_add` | 18 | 0번 |
  | `time_string` | 12 | 0번 |

  이름 있는 스크립트 함수인데 직접 호출이 0곳이면 게임은 그 들머리로 오지 않는다. 부르는 곳마다 본문이 들어가 있다고 본다
  (추정: `gml_pragma("forceinline")`. 근거는 호출 0곳이라는 것과, 이벤트가 오가는 동안 기록이 0번이었다는 것뿐이다).
  메서드는 다르다: `Warehouse.change`(`gml_Script_anon_…_3640123903_…`)는 직접 호출이 0곳인데 훅에 잡혔다. 메서드는 참조로 불린다.
  **기록이 0번이라는 것은 "직접 호출이 있는 함수나 메서드가 그동안 안 불렸다"일 때만 뜻이 있다.** 훅을 걸기 전에 센다:

      py -3.14 tools/re/script_calls.py <Norland.exe> budget_money_change time_hour
- **인자 수를 exe 에서 읽는다.** YYC 의 스크립트 함수는 `(self, other, result, argc, argv)`를 받는다. 본문에서 `argc`를 보고 `argv[i]`를 꺼내는 곳을 세면 인자 수가 나온다
  (`dumpbin /disasm /range:<주소>,<끝> Norland.exe`. dumpbin 은 Build Tools 에 있다).
  - `budget_money_change`(`0x141058C80`): `argc > 0`이면 `argv[0]`, 아니면 `undefined`. 그것 하나를 `o_game_map_controller`에서 얻은 것의 메서드(인자 1)에 넘긴다.
  - `budget_money_get`(`0x14105ABC0`): `argv[0]`이 없거나 `undefined`이면 기본값을 넣는다(선택 인자).
  - `building_warehouse_change`(`0x142A574B0`): `argv[0]`의 멤버(메서드)를 인자 없이 부르고, 그 결과의 메서드를 `(argv[1], argv[2])`로 부른다.
  세 함수 모두 읽은 대로 불러서 맞았다. 형은 기계어로 알 수 없다. 게임의 호출 기록이나 같은 종류 함수의 기록으로 본다.
- **`method_call(메서드, [인자])`는 인자를 넘긴다.** `method inst:o_time_controller.set_time_speed n:1` 뒤 훅에 `(1) -> undefined`가 남았다.
- **인자 없는 `method`는 부른 것을 확인하지 못했다.** `method inst:o_time_controller.is_paused` → `-> undefined`(불리언이 나와야 한다).
  0.5.0 은 인자가 없으면 배열 없이 `method_call(메서드)`로 불렀다. 매뉴얼은 그 꼴을 허용한다("array_args … can be omitted if the method takes no arguments").
  **왜 `undefined`였는지는 모른다**: 그 메서드에 훅을 걸지 않아 불렸는지조차 보지 못했다. 다음 실행에서 `record <주소>` 뒤 `method`로 훅에 오는지 잰다.
- **생성자의 정적 메서드는 이름으로 읽힌다.** `…__province.__budget.change`, `.get`, `…__province.__warehouse.change`, `.get`이 `method`로 읽혔다.
  `list`에는 나오지 않는다(구조체 자신의 변수만 늘어놓는다). 오브젝트의 메서드(`o_building.get_warehouse` 등)는 `list`에 나온다.
- `set_time_speed(1)`은 멈춘 동안(튜토리얼 알림으로 멈춤, `is_hand_pause` 참, `time_warp` 0) `time_speed_index`를 바꾸지 않았다(0 그대로). 뜻은 모른다.
- `o_debug.is_debug_enabled`를 1 로 써도 다음 프레임에 개발자 창은 보이지 않았다(0 으로 되돌렸다). 게임에는 `imgui_debug_*`, `debug_spawner`, `pretty_debug_inspector`가 있다.
- `find`는 3,000,001 자리를 본 뒤 끊긴다. `global.__pub_sub_controller`의 구독자가 거의 모든 구조체로 이어져 있어 전역 탐색이 넓다. 값은 주소를 좁혀 찾는다.
- 훅 자리 24개를 한 실행에서 다 썼다(한 번 건 훅은 떼지 않는다).
- 게임 시각은 초다(`__game_time` 75000 = 20:50). 배속 1 에서 실제 1초에 약 44초가 흐른다.

## 실행 뒤에 고친 것 (모듈 0.5.1. 게임에서는 아직 확인하지 않았다)

- `call`과 `record`가 `gml_Script_` 이름으로만 찾는다(`NlCore::ScriptRoutineName`). 접두 없는 이름에는 붙인다.
- `method`가 `NlAccess::CallMethod`를 거친다: `method_get_self`가 `undefined`인 메서드는 그것을 가진 구조체나 인스턴스(주소의 부모)에
  `method(부모, 함수)`로 묶어 부른다. 묶인 곳도 없고 가진 것이 구조체나 인스턴스가 아니면(전역, 배열의 원소, ds 의 값) 부르지 않는다
  (`NlCore::ChooseBinding`. 그대로 부르면 self 가 전역이 된다). 인자는 늘 배열로 넘긴다(없으면 `array_create(0)`. 인자가 있는 호출에서 확인된 길 하나로 간다).
  매뉴얼의 꼴은 `method(struct_ref_or_instance_id, func)`, `method_call(method, [array_args], [offset], [num_args])`, `method_get_self(method)`(없으면 `undefined`)다.
  **`method_get_self`·`method`·`array_create`는 이 러너에서 아직 부른 적이 없다. 정적 메서드의 `method_get_self`가 정말 `undefined`인지도 재지 않았다.**
  부르기 전에 `about <주소>`로 본다(부르지 않는다). `method`는 무엇을 어떤 self 로 부르는지 부르기 전에 답에 적는다.
- 새 명령 `about <주소>`: 값의 형, 메서드이면 묶인 스크립트의 이름, 묶인 곳이 있는지, `method`가 어떻게 부를지.
- 훅 자리 24 → 64. 훅을 걸다 실패한 자리는 다시 쓰지 않는다.
- 묻는 파일은 이름을 바꿔 집은 것만 실행한다(`NlCore::TakeRemoteRequest`). 읽고 나서 지우지 못하면 같은 호출이 0.25초마다 되풀이될 수 있었다
  (이 실행의 요청 64개에서는 없었다). 모듈이 뜰 때 남아 있던 요청은 버린다.
- `session.ps1`: 켤 때 사용자에게 없던 설정 파일을 실행 묶음이 만들었으면 `stop`이 지운다(`NlToyBox.session.txt`가 "없었다"를 적어 둔다).
  이 실행에서 사용자가 모드창으로 스위치를 켰고, 사용자에게 치트 상태 파일이 없었다면 그 상태가 다음 플레이에 남았을 것이다.
- `tools/re/script_calls.py`: exe 에서 스크립트 함수의 주소와 직접 호출 수.

## 사용자가 플레이에서 본 것 (2026-10-05, 이 실행의 모드창)

| 스위치 | 본 것 |
|---|---|
| `inst:o_debug.is_instant_build_buildings` | **즉시 건설이 된다.** |
| `inst:o_debug.is_can_build_all_buildings` | 건설 목록은 풀린다. 조건에 걸리는 건물은 여전히 지을 수 없다. 어느 조건인지는 재지 않았다 |
| `inst:o_debug.is_resources_edit_mode` | 자원의 목록이 나오지 않아 아무것도 할 수 없었다. 쓸 수 없다 |

## 확인하지 못한 것

- 영지 창고의 `change`를 직접 부르는 것. 정적 메서드라 묶인 구조체가 없다. 구조체에 묶어 불러야 한다(`method(구조체, 함수)`). 화면이 따라오는지도 그때 본다.
- 접두 없는 이름이 가리키는 루틴이 무엇인지.
- 게임 속도를 메서드로 바꾸는 것(`set_time_speed`, `__set_warp`의 인자와 뜻).
- 거래·임금·건설비·생산의 자리에 쓴 값의 효과. 읽기만 했다.
- `is_resources_edit_mode`의 뜻.
- 세이브를 불러온 게임에서 같은 자리인지(새 게임에서만 쟀다).

## 이 실행이 게임에 남긴 것

사용자의 시험용 새 게임에서: 금화 3000 → 3600(쓰기 +500, 함수 +100), 영지 창고의 나무 300 → 350(쓰기). 영주관 창고의 나무와 `is_debug_enabled`는 되돌렸다.
켜기 전의 세이브 사본은 `backups\saves\20261005-155017`에 있다.
