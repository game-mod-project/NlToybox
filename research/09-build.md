# 09 — 건설: 건설 조건, 건설비, 업그레이드

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`, 모듈 `0.6.2`(커밋 `8c5d906`). 게임을 켠 횟수: 1
(실행 묶음 `stage3c-session1`, 18:28~19:00). 사용자가 시험용 세이브를 불러와 일시정지한 채 건설 창을 다뤘다.

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
- 비용에 0 을 쓰면 남는다(`write …altar…[1]=0` → `read 0 (stuck)`. 30 으로 되돌렸다). **0 으로 쓴 뒤 실제로 자원이 들지 않는지, 업그레이드 단추가 켜지는지는 보지 못했다.**
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

## 치트로 굳힌 것 (모듈 0.7.0. 게임에서는 아직 확인하지 않았다)

- 치트 표에 종류 둘: `Hook`(함수가 돌려주는 값을 불리언으로 바꾼다. `NlRecorder::Override`), `Custom`(모듈의 코드가 한다).
- `build_any` "건설 조건 없이 짓기 (지식)": `…__knowledge_manager.is_have_knowledge_to_upgrade_building` → 참. 원격으로 건 것은 플레이에서 봤다(위). 표의 항목으로 켠 것은 아직.
- `build_marks` "건설 창의 잠금 표시 지우기": `…is_knowledge_unlocked` → 참. 확인 전.
- `build_free` "건설·업그레이드 비용 없음": `src/Build.cpp`가 건물 종류 171개의 등급별 비용을 0 으로 쓰고 처음 본 값을 장부(`core/CostBook`)에 적는다. 끄면 되돌린다. 확인 전.
- 원격 명령 `cheat <Id> on|off`(모드창의 체크와 같다).

## 확인하지 못한 것

- 비용을 0 으로 쓴 뒤: 건설 창의 표시, 실제로 드는 자원, 업그레이드 단추.
- `is_knowledge_unlocked`를 참으로 바꾸면 건설 창의 빨간 표시와 안내가 사라지는가. 지식 창과 연구에 무슨 일이 있는가.
- 지식 말고 다른 건설 조건(자리, 광산의 조건, 수 제한)을 푸는 것. 지식에 막힌 업그레이드가 있는가.
- 건물 종류를 게임이 언제 다시 만드는가(다른 세이브를 불러올 때). 0 으로 쓴 비용이 세이브에 들어가는가.
- 즉시 건설과 비용의 관계(멈춘 채로는 자원이 빠지지 않았다).

## 이 실행이 게임에 남긴 것

시험용 세이브에서: 제단 1, 창고 4, 곡창·무기고 등 건물 8채를 더 지었다. 바꾸기는 모두 끄고(`unoverride all`) 제단의 비용은 30 으로 되돌린 뒤 껐다.
자동 저장에 이 상태가 들어 있을 수 있다. 켜기 전의 세이브 사본은 `backups\saves\20261005-182809`에 있다.
