# 09 — 건설: 건설 조건, 건설비, 업그레이드

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`. 게임을 켠 횟수: 3.
조사 실행 `stage3c-session1`(18:28~19:00, 모듈 `0.6.2` 커밋 `77cb7f4`): 사용자가 시험용 세이브를 불러와 일시정지한 채 건설 창을 다뤘다.
확인 실행 `stage3c-session2`(19:30~19:55, `0.7.1`)와 `stage3c-session3`(19:57~20:11, `0.8.0`)은 아래 "확인 실행"에 적었다.

답은 `refs/runtime/stage3c-session1.answer.txt`, 로그는 `stage3c-session1.log`, 화면은 `refs/ui/c-*.png`(추적 안 함).

## 된 것

- **0.6.2 가 적재된다.** `적재 판정: 통과`, 종료 코드 0.
- **함수가 돌려주는 값을 바꾼다.** `override gml_Script_budget_money_get n:1` → `call`이 `-> number 1`, 표본 `() -> 3500 => 1`. `unoverride` 뒤에는 3500.
  원래 함수가 결과 자리가 아닌 다른 값을 돌려준 표본은 없었다(`[returned another value, not Result]`가 한 번도 붙지 않았다).
- **정적 메서드의 이름을 늘어놓는다.** `statics inst:o_building.generic` → `instanceof GenericBuilding`, 메서드 134개. 수에는 `not a struct`.
- **잠긴 건물을 짓는다.** `override …__knowledge_manager.is_have_knowledge_to_upgrade_building b:1`을 건 채 사용자가 잠겨 있던 창고 4채와 곡창·무기고를 지었다
  (`o_building` 20 → 27, `inst:o_building:20.raw_caption` = `building.storage`, HUD 의 용량: 음식 250 → 1250, 원자재 300 → 1500, 전쟁 물자 20 → 130).

## 건설 조건

- **건설 창은 건물마다 `KnowledgeManager.is_have_knowledge_to_upgrade_building(건물 이름, 등급)`에 묻는다**(`inst:o_game_map_controller.__knowledge_manager`의 정적 메서드.
  스크립트 `gml_Script_anon_KnowledgeManager_gml_GlobalScript_KnowledgeManager_1704020529_…`). 등급 0 이 "짓기"다. 불리언을 돌려준다.
- 바꾸지 않았을 때의 답(이 세이브): `altar@0` 참, `library@0` 참 / `church_small@0`, `church@0`, `gallows@0`, `chancellery@0`, `brewery@0`, `tavern@0`, `market@0`, `storage@0` 거짓.
  거짓인 것들이 건설 창에서 빨갛게 보이던 건물이다(서비스 탭의 처형대·사원·석조 사원·사무국, 창고 탭의 창고·곡창·무기고).
- 업그레이드 등급의 답: `hut_6x10@1~4`, `barrack_6x10@1~2`, `brewery@1`, `castle@1`, `lord_house_6x10@1`, `mill@1` 모두 참. 이 세이브에서 지식에 막힌 업그레이드는 보지 못했다.
- **참으로 바꾸면 지어진다. 건설 창의 표시는 남는다.** 카드는 여전히 빨갛고 "이 건물을 건설하기 위한 지식이 담긴 책을 가져오기 위해, 우리는 인구의 20이(가) 필요하다"가 보였다(`refs/ui/c-red1.png`).
  그 안내는 다른 길이다: 커서를 올리면 `get_technology_list_to_upgrade_building(이름, 0) -> array`가 불리고 그 안에서 `is_knowledge_unlocked(지식) -> false`가 나온다.
  `is_knowledge_unlocked`도 참으로 바꿔 봤지만 그 뒤의 건설 창은 화면으로 받지 못했다(지식 창이 그 함수를 122번 불렀다. 지식 창에도 영향이 간다).
- 건물 종류에 달린 조건(`__allow_to_build_function_list`)은 171개 가운데 `mine` 하나뿐이다. `is_allow_to_build()`는 불리언이 아니라 구조체를 돌려준다(바꾸지 않는다).
  금지(`__is_forbidden_construction`)는 `hall_storage_right` 하나, 수 제한(`__limit`)은 `library` 1, `castle` 1, 나머지는 `inf`.
- `o_debug.is_can_build_all_buildings`는 이 실행에서 꺼져 있었다. 목록만 푸는 스위치다(`research/07`).

## 건설비와 업그레이드

- **건물 종류 171개는 ds_map 하나에 있다**(이 실행에서는 `map:142@<이름>`. 번호는 실행마다 다를 수 있다). 스크립트로 얻는다:
  `gml_Script_building_generic_get_array_of_all_buildings()` → 이름 171개의 배열(구조체가 아니다), `gml_Script_get_generic_building(이름)` → 구조체(`instanceof GenericBuilding`).
  같은 종류의 건물들은 한 구조체를 함께 쓴다(`map:142@altar`의 칸에 쓰자 놓여 있던 제단의 `generic`에서도 같은 값이 읽혔다).
- **비용의 자리**: `<건물 종류>.__construction_cost`(`instanceof GenericBuildingConstructionCostCollection`)`.levels[등급]` = `undefined` 또는
  `{ money, resources.__array_of_resource_quantity[39] }`. 제단: `levels[1]`만 있고 나무(1) 30. 주택(`hut_6x10`): `levels[1]`~`[5]`.
- **업그레이드의 비용은 같은 종류의 다음 등급이다.** `hut_6x10.levels[2]` = 나무(1) 10, 목재(32) 5. 사용자의 화면(1등급 주택의 업그레이드: 나무 10, 목재 5)과 같다.
- **사용자가 본 "업그레이드 불가"의 까닭은 자원이다**: "자원이 부족하여 이 건물을 업그레이드할 수 없습니다"(주택, 영주의 도시 주택. 목재가 0 이었다).
  그때 `unavailable_struct("hint_cant_upgrade_building_need_resources", undefined)`가 불렸다.
- 게임이 부르는 꼴(기록): `generic.get_construction_money(등급) -> 수`, `generic.get_construction_resources(등급) -> 구조체`, `generic.is_has_next_upgrade_stage(등급) -> 불리언`,
  `generic.get_max_level() -> 수`(5, 1, 3 …), `generic.get_grade_building(등급) -> 구조체`, `cm.get_building_gui_struct(이름) -> 구조체`,
  `cm.get_number_of_free_buildings(이름) -> 0`, `km.get_technology_value("constructing_less_log"|"constructing_less_iron"|"constructing_less_update_cost") -> undefined`.
- 비용에 0 을 쓰면 남는다(`write …altar…[1]=0` → `read 0 (stuck)`. 30 으로 되돌렸다). 0 으로 쓴 뒤 자원이 들지 않는 것과 업그레이드가 눌리는 것은 확인 실행에서 봤다(아래).
- 건설 자원은 놓는 순간에 빠지지 않는다: 제단을 놓았을 때 영지 창고의 `change`가 한 번도 불리지 않았다(멈춘 채였다).
  즉시 건설을 켠 채 창고들을 지은 뒤에도 나무는 315 그대로였다(멈춘 채였다. 시간을 흘린 뒤는 보지 않았다).

## 처음 잰 것

- `BuildingComponentConstruction.is_complicated_upgrade`와 `is_under_upgrade`는 초당 수천 번 불린다(15초에 12,000~25,000번). 기록을 걸어 두지 않는다.
- `KnowledgeManager`의 정적 메서드 47개 가운데: `is_have_knowledge_to_upgrade_building`, `get_technology_list_to_upgrade_building`, `is_have_knowledge_to_produce`,
  `is_knowledge_unlocked`, `is_knowledge_available_by_state`(`(struct) -> false`), `get_technology_value`, `is_action_active`(`("clay_road") -> false`), `get_current_population_tier`.
- `ConstructionManager`: `get_free_buildings_of`, `get_number_of_free_buildings`, `get_next_free_building_token(이름) -> undefined`, `remove_free_token`.
  `FreeBuildingTokenCollection`: `add`, `remove`, `is_has_free_buildings`, `get_token_by_name`. 무료 건물 토큰의 구조는 보지 않았다.
- `BuildingComponentConstruction`(메서드 54): `build_instantly`, `upgrade_begin`, `is_resources_not_enough_for_construction`, `get_remaining_resources_for_construction`, `set_resources`.
  이 실행에서 한 번도 불리지 않았다(멈춘 채였다).
- 건물 종류의 업그레이드 등급은 따로 된 종류이기도 하다: `hut_6x10_grade_2`~`_grade_5`, `brewery_v2_grade_2` …(`__grades_buildings`에 이름이 있다).
- 전역 탐색(`find in=global`)은 건물 종류를 찾지 못한다(ds 안에 있다). `find … in=ds`로 찾았다.

## 치트로 굳힌 것 (모듈 0.8.1)

- 치트 표에 종류 둘: `Hook`(함수가 돌려주는 값을 불리언으로 바꾼다. `NlRecorder::Override`), `Custom`(모듈의 코드가 한다).
- `build_any` "건설 조건 없이 짓기 (지식)": `…__knowledge_manager.is_have_knowledge_to_upgrade_building` → 참. **확인.**
- `build_free` "건설·업그레이드 비용 없음": `src/Build.cpp`가 건물 종류 171개의 등급별 비용을 0 으로 쓰고 처음 본 값을 장부(`core/CostBook`)에 적는다. 끄면 되돌린다. **확인.**
- `instant_upgrade` "건물 즉시 업그레이드": 업그레이드 중인 건물에 `c_construction.build_instantly()`를 부른다. **확인.**
- `build_marks` "건설 창의 잠금 표시 지우기": `…is_knowledge_unlocked` → 참. 확인 전(건설 창을 화면으로 받지 못했다).
- 확인 전의 `Hook`·`Custom` 항목은 켠 채 저장돼 있어도 꺼진 채로 시작한다(`KeepKnown`).
- `build_free`: 되돌릴 값을 적은 자리에만 0 을 쓰고, 쓴 뒤 다시 읽어 남았는지 본다. 되돌린 자리만 장부에서 지우고 남은 자리는 다시 되돌린다(간격 2초에서 60초까지).
- 훅 항목은 켠 것과 실제로 걸린 것을 틱마다 견준다. 걸다 실패한 함수에는 그 실행에서 다시 걸지 않는다.
- 원격 명령 `cheat <Id> on|off`(모드창의 체크와 같다).

## 확인 실행 (2026-10-05, 게임을 켠 횟수 2)

`stage3c-session2`(19:30~19:55, 모듈 0.7.1)와 `stage3c-session3`(19:57~20:11, 모듈 0.8.0). 답과 로그는 `refs/runtime/stage3c-session2.*`, `stage3c-session3.*`, 화면은 `refs/ui/s2-*.png`(추적 안 함).
사용자가 시험용 세이브를 불러와 모드창에서 건설 영역의 항목을 직접 켜고 건물을 짓고 올렸다. 둘 다 `적재 판정: 통과`.

### 건설 조건 (build_any)

- 표의 항목으로 켰다: 로그 `cheat build_any: overriding gml_Script_anon_KnowledgeManager_…_1704020529_…`. 기록 `("pig_farm", 0) -> false => true`, `("mine", 0) -> false => true`.
- 사용자가 잠겨 있던 **돼지 농장**과 **석조 사원**을 지었다(`o_building` 19 → 21. `raw_caption`이 `building.pig_farm`, `building.stone_church`).
- 원격 `unoverride all` 뒤 0.5초 안에 다시 걸렸다: `cheat build_any: overriding … again (it was turned off elsewhere)`.

### 건설비와 업그레이드 조건 (build_free)

- 로그 `build: zeroed 160 cost value(s) in 171 building type(s), book 160`(써지지 않은 칸 없음). 뿌리부터 다시 읽어도 0 이었다
  (`map:142@hut_6x10.__construction_cost.levels[2].resources.__array_of_resource_quantity[1]` 10 → 0, `[32]` 5 → 0, 제단 30 → 0).
- **자원이 모자라도 업그레이드가 눌린다.** 목재(32)가 0 인데 주택 11채(`lord_house_10x6` 4, `hut_8x8` 3, `hut_10x6` 4)가 업그레이드 중이 됐다.
  병영(`barrack_6x10`)은 3등급까지 올랐다(2등급: 목재 10 + 기와 20, 3등급: 돌 20 + 기와 20. 재고는 모두 0 이었다).
- **자원이 들지 않는다.** 돼지 농장(짓기 나무 30, 2·3등급 각 30)을 짓고 3등급까지 올리고 석조 사원을 짓는 동안 나무 300, 금화 3500 그대로였다.
  (나무는 한 번 315 → 300 이 됐다. 그때 HUD 의 원자재가 `315/300`(빨강)에서 `300/300`이 됐다. 용량에 맞춰진 것으로 보이지만 까닭은 재지 않았다.)
- 끄면 돌아온다: `build: restored 160/160 cost value(s)`, 세 자리가 10, 5, 30. 다시 켜면 `zeroed 160 … book 160`.
- **0 으로 쓴 비용은 세이브에 남지 않는다.** 켠 채로 자동 저장(19:45)과 사용자의 저장(19:55)이 있었다. 그동안 `GenericBuilding`의 `get_data_for_serialize`·`deserialize`·`reset_construction_cost`와
  `GenericBuildingConstructionCostCollection`의 `get_data_for_serialize`·`deserialize`·`reset`·`set`은 한 번도 불리지 않았다(기록 0번).
  게임을 다시 켜 그 세이브를 치트 없이 불러오니 비용은 원래 값이었다(10, 5, 30, 돼지 농장 30). 올린 등급과 지은 건물은 세이브에 남아 있었다.
  `o_debug`의 스위치(`is_instant_build_buildings`, `is_can_build_all_buildings`)도 다시 켠 뒤에는 거짓이었다.

### 즉시 업그레이드 (instant_upgrade)

- 게임의 즉시 건설(`o_debug.is_instant_build_buildings`)은 업그레이드를 끝내지 않는다(켠 채로 주택들이 업그레이드 중으로 남았다. 사용자가 보고 이 항목을 청했다).
- 건물의 건설 구성요소는 `inst:o_building:<n>.c_construction`(`BuildingComponentConstruction`): `__construction_status`(평소 0, 업그레이드 중 3. 3 인 건물에서 `is_under_upgrade()`가 참, 0 에서 거짓),
  `__construction_progress`(0~1. `get_progress_cap()` → 1), `__construction_time`, `__construction_resources`. 건물의 등급은 `inst:o_building:<n>.__level`.
- `build_instantly()`: 본문(`0x142A224A0`)이 인자를 읽지 않는다. 결과를 `undefined`로 두고, 제 변수 하나에 1 을 쓰고, 제 메서드 하나를 인자 없이 부른다.
  부르면 기록에 `build_instantly () -> undefined`, `__check_progress_is_complete () -> undefined`, `get_progress_cap () -> 1`, `set_status (0) -> undefined`가 남는다.
- **업그레이드 중인 건물에 부르면 바로 끝난다.** 11번 주택: `__level` 1 → 2, 상태 3 → 0. 이어서 주택 10채, 돼지 농장(1 → 3), 병영(1 → 3)에서 같았다(원격 `method`로 모두 19번).
- 사용자가 업그레이드를 누를 때 게임이 부르는 것: `upgrade_begin () -> undefined`(8번 기록).
- **모듈의 항목으로 켰다(0.8.0).** 이 항목만 켠 채 사용자가 주택 세 채(5, 6, 9번)의 업그레이드를 눌렀다. 로그 `build: calling build_instantly on o_building:5 (under upgrade)` …, 등급 2 → 3.
- **자원이 들지 않는다.** 비용 없음을 끈 채였다(주택 3등급: 나무 10 + 도기 5. 도기 5개는 `economy add resource=36 amount=5`로 넣었다). 누른 뒤에도, 일시정지를 풀어 약 2,150 게임초를 흘린 뒤에도
  나무 300, 도기 5(예약 없음), 금화 3500 그대로였다. 다만 단추를 누르려면 자원이 있어야 한다(없으면 비용 없음과 함께 켠다).

### 그 밖에 본 것

- 건물 종류의 ds_map 은 세 실행 모두 `map:142`였다.
- 등급별 비용(이 빌드): `pig_farm` 1~3등급 나무 30씩(4등급 없음), `barrack_6x10` 1: 나무 20 / 2: 목재(32) 10 + 기와(37) 20 / 3: 돌(29) 20 + 기와 20,
  `hut_10x6` 1: 나무 10 / 2: 나무 10 + 목재 5 / 3: 나무 10 + 도기(36) 5 / 4: 나무 10 + 기와 20 / 5: 돌 10 + 기와 20, `lord_house_10x6` 1: 나무 20 / 2: 돌 20 + 기와 20, `castle` 2: 돌 40 + 기와 40.
- 게임은 잡은 오류를 `%LOCALAPPDATA%\Strategy\catched_errors_<버전>.txt`에 적는다(불러오기와 저장의 이름·시각도 있다). `_struct_copy_from is not a struct!` 경고가 이 실행들에서 6번 났다
  (19:48 에 5번, 20:04 에 1번). 같은 경고가 모드를 만들기 전인 2026-09-24 에도 6번 있었다. 치트 때문이라고 볼 근거는 없다. 원인은 가리지 못했다.
- 원격 `method inst:o_time_controller.set_time_speed n:0`이 손으로 멈춘 게임을 풀었다(`research/08`의 꼴 그대로).

## 확인하지 못한 것

- `build_marks`: `is_knowledge_unlocked`를 참으로 바꾸면 건설 창의 빨간 표시와 안내가 사라지는가. 지식 창과 연구에 무슨 일이 있는가(사용자가 켠 동안 127번 불렸다. 화면은 받지 못했다).
- 지식 말고 다른 건설 조건(자리, 광산의 조건, 수 제한)을 푸는 것.
- 같은 실행에서 다른 세이브를 불러올 때 게임이 건물 종류를 다시 만드는가(게임을 다시 켜면 원래 값이다. 위).
- 게임의 즉시 건설 스위치가 자원을 언제 빼는가. 짓는 중·수리·옮기기(`__construction_status`의 다른 값)에 `build_instantly`를 부르면 어떻게 되는가(모듈은 3 에만 부른다).
- "복잡한 업그레이드"(`is_complicated_upgrade`)가 무엇인가(본 건물은 모두 거짓이었다).
- `_struct_copy_from is not a struct!` 경고의 원인.

## 이 실행들이 게임에 남긴 것

시험용 세이브(`아덴_Autosave_Evening_day_1_date_5_10_2026_time_17_58`. 사용자가 19:55 에 같은 이름으로 저장했다)에는 조사 실행에서 지은 것 말고 돼지 농장과 석조 사원이 더 있고
주택·병영·돼지 농장의 등급이 올라 있다. 세 번째 실행에서 도기 5개를 넣고 주택 세 채를 더 올렸다(저장하지 않았다. 20:10 의 자동 저장 `아덴_Autosave_Evening_day_2_…_20_10`에는 들어 있을 수 있다).
켜기 전의 세이브 사본: `backups\saves\20261005-182809`(조사 실행 전), `20261005-193006`(두 번째 실행 전), `20261005-195723`(세 번째 실행 전).
