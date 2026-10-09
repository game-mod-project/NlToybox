# 31. 지도 생성기 — 새 게임의 영지 지도를 정하는 값과 다시 생성하는 길 (2026-10-07. 스파이크)

질문: 새 게임의 영지 선택 뒤 "생성기" 창(호수·언덕·언덕 배치·산·강)이 바꾸는 값은 어디에 있고, 지형 밖의 자원(종류·양)도 값으로 있는가. 모드가 그 값을 쓰고 게임의 길로 다시 생성시킬 수 있는가.
방법: 켜기 1번(아무것도 켜지 않은 실행 묶음). 사용자가 생성기 화면까지 가고 "호수"를 한 칸 올려 "생성"을 눌렀다. 나는 읽기(`find`·`list`·`statics`)와 `record` 만 했다. 답은 `refs/runtime/map-generator-spike.*`.

## 값의 자리

- **생성기 화면의 표식**(그 실행의 `state`): `in_game 0`, `o_main_menu 0`, `o_debug 1`, `o_character 0`, `global.__new_game_initializer.__is_active` 가 `true`. 메인 메뉴에서는 `o_main_menu 1`, 게임 안에서는 `o_character` 가 있다. 모듈은 이 셋(`__is_active`, 설정 구조체가 읽힘, 게임 안이 아님)으로 화면을 가린다(`NlCore::MapScreen`).
- **영지(지역)마다의 생성 설정**: `global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings`(`GlobalMapAreaGeneratorSettings`). 칸 17개(모두 수):
  - 지형: `__lakes`(호수. 창의 6단계 `generator.quantity_names_1.0~5`), `__hills`(언덕. 4단계 `_2.0~3`), `__hills_distribution`(언덕 배치. 0 center, 1 edges, 2 random: `global.global_map_generator_hills_distribution_names`), `__mountains`(산. 6단계), `__river`(강. 0/1).
  - 가장자리 막힘: `__blocked_up`·`__blocked_down`·`__blocked_left`·`__blocked_right`(수. 아덴의 영지는 2·0·3·0).
  - **자원**: `__berry`, `__bush`, `__clay`, `__fertile`, `__hop`, `__iron`, `__plants`, `__tree`(수. 그 영지는 1·4·1·0·0·1·3·1). 창에는 없고 세계 지도의 영지 틀(`__global_map_template.__array_of_area[n].__generator_settings`)이 정한다.
  - `__stashed_settings`(같은 17개를 밑줄 없는 이름으로 둔 사본. `restore_settings()` 가 되돌릴 때 쓴다).
  - 메서드: `set_lakes`·`set_hills`·`set_hills_distribution`·`set_mountains`·`set_river`(수 하나. 슬라이더가 부른다: 호수 한 칸 올리자 `set_lakes(2)`), `get_*`, `get_mountainous`·`get_mountainous_name`·`get_mountainousness_level_color`, `apply_modificators()`(인자 없음. 구조체를 돌려준다), `import_params`, `restore_settings`, `create_resource_pile`, `get_blockers_array`, `get_data_for_serialize`·`deserialize`(세이브에 들어간다), `imguigml_control`.
  - 자원 8개에는 세터가 없다(바로 써야 한다). 지역 구조체에는 `__generator_configs_name`(배열. 적용할 변형의 이름. 아덴은 비어 있다), `__ground_color_name`("desert"), `__difficulty`, `__village_resources` 도 있다.
- **생성기의 변수 55개**: 게임 파일 `generator\genmap\config.json`(`configurations[0]` "final" 의 `variables[55]` {name, value}, `modificators[4]`: swamp·desert·without_river·many_mountains 가 변수 몇 개를 덮어쓴다). 런타임은 `global.generator_configs`(`__filename "generator/genmap/config"`). 변수의 이름(값은 옮기지 않는다):
  `size, seed, blocked_right/left/top/bottom, resources_on_hills, level_height, level_threshold, level_smooth_target_fill, level_smooth_second_level_factor, level_grow_factor_center/edges, lakes_count_min/max, lakes_size_min/max, river_enable, river_brody_level, bridges_count_per_size, bridges_max_count, bridges_roughness, bridges_size, brody_size, mountains_count_min/max, mountains_size_min/max, mountains_freq, mountains_height_min/max, forest_removing_threshold, stump_count, grass_count, trees_count, trees_distance, bush_count, bush_distance, stones_count, mines_count_min/max, fertile_count, hop_count, stonezone_count, clay_points, bush_resource_count, bush_per_area_count_min/max, berries_count, berries_bush_count_min/max, water_depth_freq, water_depth_min/max, turbulence_freq`.
  같은 폴더의 `final`(생성 그래프: `GenMapModify*` 클래스들의 목록. 60 KB)과 `assets.json`(`biomes`)도 생성기의 것이다.
- 씨앗: `global.__new_game_initializer.__generator_seed`(-1 = 무작위). `NewGameInitializer.set_generator_seed(수)`·`get_generator_seed()`.

## 다시 생성하는 길(기록)

- "지도 재생성" 단추는 생성기 창을 연다(`MapPreviewController.open_generator_settings(true)`). 창의 "생성"이 **`MapPreviewController.regenerate_map()`(인자 없음)** 을 부르고, 그 안에서 `NewGameInitializer.set_generator_seed(-1)` 과 `GlobalMapAreaGeneratorSettings.apply_modificators()`(→ 구조체) 가 불린 뒤 지도가 다시 생성된다. 그 사이 `__lakes` 가 1 → 2 로 바뀌어 있었다.
- `accept_configuration`·`import_params`·`restore_settings`·`create_resource_pile` 은 그동안 불리지 않았다.
- 미리 보기 제어기: `global.__new_game_initializer.__map_preview_controller`(`MapPreviewController`: `regenerate_map`, `open_generator_settings`, `close_generator_settings`, `accept_configuration`, `accept_configuration_and_go_next_step`, `clear_configuration`, `return_to_settings_of_new_game` …). 초기화기: `global.__new_game_initializer`(`NewGameInitializer`: `set_generator_seed`, `restore_choosed_province_generator_settings`, `is_should_generate_map`, `set_choosed_province` …).

## 판단(스파이크의 답)

- **된다.** 모드는 생성기 화면에서 지역의 설정 17개를 읽어 보이고, 17개를 칸에 바로 써 넣은 뒤(세터는 부르지 않는다: 기록한 것은 `set_lakes` 하나뿐이다), 게임이 부르는 꼴 그대로 `regenerate_map()` 을 부르면 그 값으로 지도가 다시 생긴다. 씨앗은 `set_generator_seed(수)` 로는 고정되지 않는다: `regenerate_map()` 이 안에서 `set_generator_seed(-1)` 을 부른다. 남는 길은 `get_generator_seed()` 의 반환값을 훅으로 바꾸는 것이고, 생성기가 그 함수를 읽는지는 확인 전이다.
- **자원의 단계가 실제로 몇 개를 만드는지는 재지 않았다**: 단계 → 변수 55개의 셈은 `apply_modificators` 안에 있다. 창에 없는 자원 8개는 영지 틀이 정하는 값이라 범위도 어림이다(본 값은 0~5). 기능을 만들 때 단계를 바꿔 생성한 지도의 광산·점토·열매 수를 세어 본다.
- 더 미세한 조절(광산 수의 최소·최대, 점토 자리의 수, 나무 수 …)은 파일 `generator\genmap\config.json` 의 변수 55개다: 데이터 오버레이(`tools/overlay.ps1`)의 카탈로그에 그 파일을 더하면 지금의 프리셋 길로 입히고 되돌릴 수 있다(게임을 켤 때 읽히므로 켜기 전에 입힌다).

## 구현(0.32.0. 지도 탭)

- `src/MapGen.cpp` + `core/MapPlan`. 17값의 단추, 다시 생성(`regenerate_map()`), 원래대로(stashed), 프리셋 파일, 원격 `map`. 씨앗 고정은 확인 전(꺼져 있다).
- 확인(Task 7 의 켜기 1): (여기에 적는다 — 스펙 §7 의 1~6: 탭의 값, `map set lakes=3` + 재생성 뒤 게임의 슬라이더, `iron=4 clay=4` 의 광산·점토 수, restore, 프리셋 저장·불러오기·지우기, `get_generator_seed` 의 기록, 창의 `[+]`. 켜기 2 의 적재 판정)

## 남은 것

- 자원 단계의 효과(단계마다 생기는 수), 범위의 상한. `__generator_configs_name` 에 변형의 이름(swamp 들)을 넣으면 적용되는가(배열 쓰기는 모듈에 아직 없다).
- `apply_modificators()` 가 돌려주는 구조체의 칸(변수 55개가 단계로 바뀐 값으로 보인다. 추정).
