# NlToyBox 치트 메뉴 — 설계

- 작성일: 2026-10-05
- 대상: Norland `0.5588.9777.0` (Steam buildid `25211575`), 모듈 `0.3.0` → `0.4.0`
- 상태: 대화에서 설계 승인(2026-10-05). 1·2단계의 계획은 `docs/superpowers/plans/2026-10-05-cheat-menu-stage1-2.md`
  - 1·2단계 구현(2026-10-05). 메뉴에서 잰 것은 `research/06-cheat-menu.md`. **효과는 아직 플레이에서 보지 않았다**
  - 구현 뒤 전체 검토로 정정: §4(`SpeedControl`), §7(잠금과 찾기의 한도), §9(기준을 누른 뒤에 잰다, 쓴 자리로 되돌린다)
- 앞선 문서: `2026-10-04-configurable-rules-roadmap.md`의 하위 프로젝트 2(런타임 접근)·3(편집 UI)·4(규칙)를 이 문서가 구체화한다

## 1. 목적

게임 안의 모드창(F8)에서 쓰는 치트 메뉴를 만든다. 사용자가 준 18개 영역(자원·경제, 생산·건설, 캐릭터, 캐릭터 편집, Lord,
인구, 음식·욕구, Knowledge, 아이템·장비, 군대·전투, 외교, 종교, 시간, 월드·맵, 이벤트, 편의·디버그, 프리셋, 메뉴 구조)을 **전부** 만든다.

- 항목을 "안 되는 것"으로 분류하지 않는다. 영역은 "어떤 수단으로 닿는가"로만 나눈다(§3). 한 수단으로 안 먹으면 다음 수단으로 내려간다.
- "된다"는 말은 플레이에서 효과를 본 뒤에만 쓴다. 그 전에는 "써 넣었다"까지만 말한다. 치트 표의 항목마다 확인 여부를 적는다(§6).
- 사용자의 목록이 스스로 별도 모드로 돌린 것(본격적인 맵 에디터, 지형 편집, 건물 위치 편집, 월드 생성기)은 이 문서에 넣지 않는다.

완료의 뜻: 영역마다 사용자의 목록에 있는 항목이 모드창에 있고, 플레이에서 효과를 봤다.

## 2. 실측한 사실

이름과 값은 새 게임 덤프(`refs/runtime/stage0b-run2.late1.json`, 인물 4·건물 2)와 exe 문자열(`refs/exe_strings.txt`)에서 봤다.
**아래 손잡이들을 바꿨을 때 게임이 어떻게 되는지는 아직 재지 않았다.** 이 문서의 단계들이 그것을 잰다.

### 2.1 지금 모듈이 하는 것 (0.3.0)

| 사실 | 근거 |
|---|---|
| Present 훅과 F8 모드창(Dear ImGui)이 동작한다. Present 는 게임 스레드에서 불린다 | `research/05-mod-window.md` |
| 배율 7개는 데이터 파일을 읽은 ds_map, `global.__gameplay_vars`, 지식 구조체에 써진다. 201개 모두 남는다 | 같은 문서, `mods\Aurie\NlToyBox.log` |
| 게임을 시작하면 그 값의 일부가 `o_debug`·`o_province_controller`의 변수로 옮겨진다. `building_resources`가 게임 안에서 어디로 가는지는 찾지 못했다 | `research/02-new-game-state.md` |
| 구조체에 `variable_struct_exists/get/set`, ds 에 `ds_map_replace`·`ds_list_replace`, 인스턴스에 `variable_instance_get_names/get` 이 된다 | `research/02`, `research/05` |
| 게임 스크립트를 인자가 틀린 채 부르면 게임이 GML 오류로 끝난다 | `CLAUDE.md` (실측) |

### 2.2 게임에 들어 있는 개발자 도구

| 사실 | 근거 |
|---|---|
| `o_debug`(게임 화면에서만 있다)에 변수 127개. 스위치: `is_instant_build_buildings`, `is_can_build_all_buildings`, `is_resources_edit_mode`, `is_combat_without_injuries`, `is_disable_dodge`, `is_disable_equipment_destroy`, `is_surrender_disable`, `is_rebellions_can_started`(1), `is_show_traits_windows`, `is_show_debug_managers`, `is_debug_enabled`(0) 등. 수: `debug_building_duration_factor`(0.5), `debug_rest_decrease_per_hour`(3), `debug_piety_decrease_per_hour`(0.83), `church_donation_runes`(1) 등 | 덤프 `instances.o_debug.members` |
| 게임의 ImGui(`imgui` 인스턴스 1개)가 돌고 있다. 게임 폴더의 `imgui.ini`가 실행 때마다 갱신된다 | 덤프 `present`, 파일 시각 |
| 디버그 창 이름: `DEBUG SPAWNER`, `Debug managers`, `Debug Battle`, `Debug Hour`, `Edit mode##resource_count`, `Reset##event_cooldown_`, `Force end##cooldown` | exe 문자열 |
| 인물 인스턴스(`o_character`, `o_dummy`): `__soul`, `get_skills`, `get_traits`, `get_minds`, `get_inventory`, `get_equipment`, `is_show_debug_window`, `starving_hours`, `imgui_pretty_inspector_debug` | 덤프 `instances.o_character.members` |
| 건물 인스턴스(`o_building`): `__level`, `add_level`, `set_level`, `get_warehouse`, `__warehouse`, `c_production`, `c_construction`, `is_show_debug_window` | 덤프 `instances.o_building.members` |
| `o_game_map_controller`: `__trade_manager`, `__knowledge_manager`, `__factions_manager`, `__game_director`, `__economics_manager`, `__unlocker_manager`, `__province`, `__battle_equipment_editor` 등 구조체 60여 개 | 덤프 |
| 함수 이름: `budget_money_change`, `budget_money_get`, `change_budget`, `get_treasury_gold`, `add_resource`, `trait_attach`, `trait_detach`, `cure_all_disease`, `unlock_all_articles`, `is_knowledge_unlocked`, `try_to_force_start`, `debug_spawn_army`, `instant_spawn_hired_mercenaries`, `rebellion_debug_spawn_player_dummy`, `set_time_speed`, `is_in_fog_of_war` | exe 문자열 |
| `o_data.current_debug_mode` = 2, `current_debug_mode_on_init` = 0. 뜻은 모른다 | 덤프 |

### 2.3 시간

`o_time_controller`의 값을 메뉴 덤프 넷과 게임 안 덤프 셋에서 봤다.

| | 메인 메뉴 | 게임 안(속도 1) |
|---|---|---|
| `time_warp`, `time_warp_new` | 0, 0 | 1, 1 |
| `__time_warp` / `__delta_time_factor` | 0 / 0.07~1.93 | 둘이 같다(0.33, 1.78, 1.79) |
| `is_fully_paused` | 1 | 0 |
| `__game_time` | 28836 (멈춰 있다) | 31125 → 33834 → 36550 |
| `time_warp_min` / `time_warp_max` | 0 / 100 | 0 / 100 |
| `time_speed_index` / `time_speed_variants` | 0 / 배열(4) | 0 / 배열(4) |
| `__debug_custom_wrap` | -4 | -4 |

- 세 표본 모두 `__time_warp` = `time_warp` × `__delta_time_factor`다. `time_warp`가 속도의 입력이라는 단서다(써 보지는 않았다).
- 속도 1에서 `__game_time`은 실제 1초에 약 57.4 늘었다(47.3초 동안 2716.6).
- **메뉴에서는 시간이 흐르지 않는다.** 속도의 효과는 게임 화면에서만 잴 수 있다.

### 2.4 YYToolkit

| 사실 | 근거 |
|---|---|
| 인터페이스에 `CallGameScriptEx`, `GetNamedRoutinePointer`, `GetScriptData`, `EnumInstanceMembers`가 있다 | `YYTK_Shared_Interface.hpp` |
| 스크립트 함수의 꼴은 `RValue& (CInstance* Self, CInstance* Other, RValue& Result, int ArgumentCount, RValue* Arguments[])`다 | `YYTK_Shared_Types.hpp` 237행 `PFUNC_YYGMLScript` |
| `MmCreateHook`으로 Present 에 훅을 걸었다. **게임 스크립트에는 아직 걸어 보지 않았다** | `src/Ui.cpp`, `research/05` |

### 2.5 새로 쓰는 빌트인

모두 exe 문자열에 이름이 있고 매뉴얼(Context7 `/yoyogames/gamemaker-manual`)에서 인자를 확인했다. **이 러너에서 불러 본 적은 없다**(2단계의 실행에서 잰다).

| 빌트인 | 매뉴얼의 꼴 |
|---|---|
| `variable_instance_set(instance_id, name, val)` | 반환 없음. 없는 변수면 만든다 |
| `variable_instance_exists(instance_id, name)` | 불리언 |
| `array_set(variable, index, value)` | 반환 없음 |
| `typeof(variable)` | `"number"`, `"string"`, `"array"`, `"bool"`, `"int32"`, `"int64"`, `"ptr"`, `"undefined"`, `"null"`, `"method"`, `"struct"`, `"ref"`, `"unknown"` |
| `variable_global_set(name, val)`, `instance_exists(obj)` | 이름만 확인했다. 쓰기 전에 매뉴얼을 본다 |

## 3. 수단

GameMaker 게임의 상태는 인스턴스 변수·구조체·배열·ds 에 있고 규칙은 스크립트다. 모듈은 변수를 읽고 쓰고, 스크립트를 이름으로 찾고,
함수에 훅을 걸 수 있다(§2). 그래서 영역마다 다른 것은 손잡이를 찾는 데 드는 일뿐이다.

| | 수단 | 내용 | 처음 쓰는 단계 |
|---|---|---|---|
| A | 내장 스위치 | `o_debug`의 변수를 켠다. 일은 게임의 코드가 한다 | 2 |
| B | 값 쓰기·잠금 | 게임이 읽는 살아 있는 자리에 쓴다. 잠그면 0.1초마다 다시 쓴다 | 2 |
| C | 게임 함수 호출 | 게임의 함수·메서드를 부른다. 인자의 꼴은 게임이 스스로 부를 때 **기록해서** 그대로 따른다(호출 기록기) | 3 |
| D | 스크립트 훅 | 스크립트의 인자나 반환값을 바꾼다(비용 0, 소비 0, 무적) | 3 |

- 한 항목은 A → B → C → D의 차례로 시도한다. 위의 수단이 먹으면 아래로 내려가지 않는다.
- C 의 규칙: 인자의 수와 형을 기록으로 확인한 함수만 부른다. 기록이 없는 함수는 부르지 않는다(§2.1의 마지막 줄).
- D 는 C 의 기록기와 같은 훅 위에 선다. 3단계가 처음 걸어 보고, 안 걸리면 그때 방법을 바꾼다(이 문서를 고친다).

## 4. 구조

```
Present 훅 (게임 스레드)                 EVENT_OBJECT_CALL 콜백 (게임 스레드)
  Ui.cpp ── Menu::Draw                     ModuleMain ── Menu::GameTick
              ├ Explorer::Draw  ◄─ 스냅샷 ──┤  Explorer::GameTick ── Access ── 러너
              └ Cheats::Draw    ── 명령 ───►└  Cheats::GameTick   ── Access ── 러너
```

- **러너를 건드리는 호출은 `GameTick`에서만 한다.** 그리는 쪽(`Draw`)은 스냅샷(글)만 읽고 명령을 큐에 넣는다.
  Present 가 게임 스레드에서 불리는 것은 쟀지만(§2.1), Present 안에서 빌트인을 부르는 것은 재지 않았다.
- 두 쪽이 나누는 상태는 재귀 뮤텍스 하나로 감싼다(지금은 같은 스레드라 다툼이 없다. 달라져도 깨지지 않게 한다).
- **`RValue`를 틱을 넘겨 들고 있지 않는다.** 스냅샷과 찾기 결과는 경로(글)와 값의 글만 담는다. 구조체는 GC 가 걷어 갈 수 있다.

| 단위 | 하는 일 | 기대는 것 |
|---|---|---|
| `src/core/AskPath` | 주소 형식을 읽고 쓴다(§5) | 없음 |
| `src/core/CheatTable` | 치트 표(§6): 항목의 이름·영역·주소·종류 | `AskPath` |
| `src/core/CheatState` | 치트 상태 파일(`NlToyBox.cheats.txt`)을 읽고 쓴다 | 없음 |
| `src/core/Rate` | 흐름의 빠르기를 잰다(게임 시간이 실제 1초에 얼마나 느는가) | 없음 |
| `src/Access` | 주소를 따라가 읽고, 쓰고, 자식을 늘어놓는다. 값을 글로 적는다 | 러너, `AskPath` |
| `src/Search` | 이름·값으로 찾기, 다시 거르기 | `Access` |
| `src/Explorer` | 탐색기(§7): 훑기, 고치기, 즐겨찾기와 잠금, 찾기 | `Access`, `Search` |
| `src/Cheats` | 치트 표의 항목을 적용하고(켜기, 수, 훅, 배율) 영역의 패널을 그린다. 속도는 게임의 함수로 걸고 흐름은 `Rate`로 잰다 | `Access`, `CheatTable`, `Recorder`, `Rate` |
| `src/Production` | 자료를 돌며 배율을 쓰는 항목(창고 용량, 조리법). 3나-3 | `Access`, `Cheats`, `core/CostBook` |
| `src/core/PeoplePlan` | 인물 패널의 러너와 무관한 부분: 능력치·욕구의 이름, 명령의 꼴, 대상 고르기, 쓰는 수 다듬기, 표 항목이 사람을 도는 바퀴 | 없음 |
| `src/People` | 인물·영주·인구 패널과 표의 인구 항목(4단계). 사람을 uuid 로 가리키고 틱에서 읽고 쓴다 | `Access`, `Cheats`, `core/PeoplePlan` |
| `src/Menu` | 왼쪽 목록과 오른쪽 패널, 상태 줄, 상태 파일 | `Explorer`, `Cheats`, `Tweaks`, `CheatState` |

`src/core`의 것은 `tests/native`에서 시험한다. 기존 `Tweaks`(배율 7개)는 2단계에서는 "배율" 항목으로 그대로 두고 3단계에서 영역으로 옮긴다.

## 5. 주소

앞 세션이 만든 `AskPath`를 쓴다. 치트 표, 잠금, 즐겨찾기, 찾기 결과, 시험 설정이 모두 이 글 하나로 값을 가리킨다.

```
global.a.b[3].c                          전역 → 구조체 멤버 → 배열 원소
inst:o_debug.is_x                        오브젝트의 첫 인스턴스의 변수
inst:o_building:1.generic                두 번째 인스턴스
map:150@building_resources@woodcutter_lvl_1#0#1    ds_map 의 키, ds_list 의 자리
map:44@{building.menu.x}                 점이 든 키는 중괄호로 싼다
inst:o_production_manager.ppm_map_of_process@key   수(ds 번호)인 변수 뒤의 @ 와 # 는 그 번호의 ds 로 들어간다
```

따라가는 법(단계의 종류 × 지금 든 값):

| 단계 | 든 값 | 읽기 | 쓰기 |
|---|---|---|---|
| `.name` | 전역 | 멤버 열거에서 이름을 찾는다(`Game::Resolve`가 쓰던 길) | `variable_global_set` |
| `.name` | 인스턴스(`inst:` 뿌리, 또는 `instance_exists`가 참인 ref) | `variable_instance_exists` → `variable_instance_get` | `variable_instance_set` |
| `.name` | 구조체 | `variable_struct_exists` → `variable_struct_get` | `variable_struct_set` |
| `[i]` | 배열 | `array_length` → `array_get` | `array_set` |
| `@key` | ds_map 번호 | `ds_exists` → `ds_map_exists` → `ds_map_find_value` | `ds_map_replace` |
| `#i` | ds_list 번호 | `ds_exists` → `ds_list_size` → `ds_list_find_value` | `ds_list_replace` |

- 없는 것을 만들지 않는다: 쓰기는 읽기가 성공한 자리에만 한다.
- 쓴 뒤에는 뿌리부터 다시 따라가 읽는다. 그 값이 아니면 "써지지 않음"이다.
- ds 번호를 표에 적지 않는다(실행마다 같았지만 기대지 않는다. `CLAUDE.md`). 표의 주소는 `global.`과 `inst:`로 시작한다.
- `inst:<오브젝트>:<n>`의 n 은 실행마다 다른 인스턴스를 가리킬 수 있다. 그래서 파일에서 불러온 잠금은 꺼진 채로 시작한다(§6.3).

## 6. 치트 표

### 6.1 항목

```cpp
struct Cheat {
    const char* Id;        // 상태 파일의 이름
    Area Where;            // 왼쪽 목록의 어느 영역인가
    const char* Label;     // 창에 보이는 이름
    const char* Path;      // §5 의 주소
    CheatKind Kind;        // Toggle | Number
    double On, Off;        // Toggle: 켤 때와 끌 때 써 넣는 값. Off 는 덤프에서 본 원래 값이다
    double Min, Max;       // Number 의 범위
    bool Verified;         // 플레이에서 효과를 봤는가
    const char* Help;      // 변수 이름에서 읽은 뜻. Verified 가 아니면 추정이다
};
```

- 영역의 패널은 표에서 그린다. 항목을 더하는 일은 표에 한 줄을 더하는 일이다.
- `Verified`가 거짓인 항목은 창에서 이름 옆에 `(?)`가 붙고, 올리면 변수 이름과 "효과 확인 전"이 보인다.
  플레이에서 효과를 보면 참으로 바꾸고 `research/`에 적는다. 안 먹으면 다음 수단으로 고친다(§3).
- 표의 주소가 모두 읽히는지, `Id`가 겹치지 않는지는 네이티브 시험이 본다.

### 6.2 적용

- 켠 Toggle 과 값을 정한 Number 는 0.5초마다 대상에 써 넣는다(새 게임이나 불러오기로 `o_debug`가 다시 만들어져도 따라간다).
  끄면 원래 값을 한 번 써 넣은 뒤 손을 뗀다: Toggle 은 표의 `Off`, Number 는 처음 본 값이다.
  (Toggle 이 처음 본 값을 쓰지 않는 까닭: 켠 채로 저장된 값을 처음 보면 "원래 값"이 켠 값이 된다.)
- 대상이 없으면(메뉴에서는 `o_debug`가 없다) "게임을 시작하면 적용"이라고 보인다.
- 잠금은 0.1초마다 쓴다. 대상이 없으면 "대상 없음"으로 남는다.

### 6.3 상태 파일

`mods\Aurie\NlToyBox.cheats.txt`. 사용자의 설정이다(도구가 지우지 않는다. `ui-check.ps1`은 치워 두었다가 되돌린다).

```
on is_instant_build_buildings
num debug_rest_decrease_per_hour=0
pin inst:o_time_controller.time_warp
lock inst:o_character:0.starving_hours=0
```

- `on`·`num`은 다음 실행에 다시 적용된다. `lock`은 **꺼진 채로** 불러온다(§5의 마지막 줄).
- 읽을 수 없는 줄, 표에 없는 `Id`, 읽히지 않는 주소는 버린다.
- 기존 `NlToyBox.settings.txt`(배율 7개)는 그대로 둔다.

## 7. 탐색기

게임의 아무 값이나 보고 고치는 창이다. 그 자체로 치트이고, 다른 영역의 손잡이를 찾는 도구다.

- **훑기**: 주소 줄 + 지금 든 것의 자식 표(이름, 종류, 값). 컨테이너를 누르면 들어가고 `↑`로 나온다.
  뿌리 화면에는 `global`과 인스턴스가 있는 오브젝트들이 있다. 한 번에 컨테이너 하나만 늘어놓는다(전역은 4,700여 개다. 이름 거르기를 둔다).
  자식이 2,000개 이하면 0.5초마다 새로 읽고, 넘으면 "새로 고침"을 누를 때만 읽는다.
- **수인 변수를 ds 로 열기**: 수 옆의 `map`·`list`를 누르면 그 번호의 ds 로 들어간다(`ds_exists`가 참일 때만).
- **고치기**: 수·불리언·글의 값을 눌러 고친다. 결과는 "써 넣음" 또는 "써지지 않음"이다.
- **잠금**: 값을 그 수로 묶어 둔다. **즐겨찾기**: 주소를 목록에 두고 값을 계속 본다. 둘 다 상태 파일에 남는다.
  `inst:` 주소의 잠금은 잠글 때의 인스턴스를 적어 두고, 같은 주소가 다른 인스턴스를 가리키게 되면 푼다
  (`instance_find`의 n 번째는 앞의 인스턴스가 사라지면 다음 인스턴스가 된다).
- **찾기**: 이름의 일부 또는 수의 값으로 찾는다(범위: 전역, 인스턴스. ds 는 고르면 포함). 결과는 주소의 목록이다.
  **다시 거르기**: 결과 가운데 지금 값이 주어진 수인 것만 남긴다(금화가 3000일 때 3000 을 찾고, 써서 2950 이 되면 2950 으로 거른다).
- 찾기는 한 틱 안에서 끝낸다(덤프가 1.6~2.1초 걸렸다. 그동안 화면이 멈춘다). 한도: 깊이 10, 방문 300만, 배열 4,096, 3초, 결과 500.
  한도에 걸리면 "일부만 봤다"고 알린다. 깊이·배열의 길이·오브젝트마다의 인스턴스 수(64) 때문에 들어가지 않은 그릇은 세어 보인다.

## 8. 메뉴 구조와 영역별 항목

왼쪽 목록: 탐색기 · 경제 · 건설·생산 · 인물 · 영주 · 인구·욕구 · 지식 · 아이템 · 군대·전투 · 외교 · 종교 · 시간 · 월드 · 이벤트 · 유틸 · 프리셋. ("배율"은 3나-3 에서 없앴다. 배율 7개는 제 영역의 패널에 그린다.)
아직 채우지 않은 영역은 흐리게 보이고 몇 단계인지 적는다.

"손잡이"는 §2에서 이름을 본 것이다. "찾는다"는 그 단계에서 탐색기의 찾기로 주소를 찾는다는 뜻이다.

| 영역 | 사용자 목록의 항목 | 수단과 손잡이 | 단계 |
|---|---|---|---|
| 경제 | 금화 추가·설정 | C `budget_money_change`. 금화의 자리는 값으로 찾는다 | 3 |
| | 자원 추가(전부·하나), 최대치 | A `is_resources_edit_mode`, C `add_resource`, B 창고(`get_warehouse`, `__warehouse`) | 2(A), 3 |
| | 구매가 감소, 판매가 증가, 거래량 | D 배율: `__trade_manager.buy_default_get`·`sell_default_get`, `__prices_manager.get_market_depth`가 돌려주는 수에 곱한다(`research/10`) | 3 |
| | 세금, 임금, 유지비 | `__salary_manager_new.__salary[4]`·`__slave_salary[11]`. 조사한 세이브에 값이 없어 재지 못했다(봉신·군대가 있는 세이브에서 다시 본다) | 4 이후 |
| 건설·생산 | 건설 즉시, 모든 건물 건설 | A `is_instant_build_buildings`, `is_can_build_all_buildings` | 2 |
| | 건설 시간 | B `o_debug.debug_building_duration_factor` | 2 |
| | 건설비 무료, 업그레이드 무료·즉시 | B 건설 비용의 자리를 찾는다(`building_resources`의 게임 안 자리). 안 되면 D. C `set_level` | 3 |
| | 생산량·생산 속도·재료 소비·작업 효율 | D 배율: `resource_production_points_cost_get`, `get_worker_base_performance_factor`. B: 조리법(`__production.__map_of_production`)의 수와 재료 | 3 |
| | 작업 효율, 노동력 무시 | B 건물의 `c_workplace`, `__cached_efficiency` 둘레에서 찾는다. 안 되면 D | 3 |
| 인물 | 능력치 올리기·내리기·설정·최대, 경험치 | B `__soul.__skills.__level.<이름>`에 바로 쓴다(0~20. 게임의 인물 창에 보인다). 경험 점수(`__points`)와 경험 배율은 아직 | 4(됨) |
| | 건강, 피로, 스트레스, 행복, 충성, 관계, 욕구 | 욕구: B `__soul.__motive.__motive[6]`. 행복: C `Minds.attach_generic_mind`(디버그용 생각 +100). 건강: C `cure_all_disease`·`cure_bleeding` + 부상 특성 떼기. 충성·관계는 아직 | 4(충성·관계 빼고 됨) |
| | 나이, 성별, 특성 더하기·빼기, 소속, 직업, 장비 | 나이: C `set_age`. 특성: C `Traits.trait_attach`·`trait_detach`(게임의 282개 이름). 성별·소속·직업·장비는 아직 | 4(나이·특성 됨) |
| 영주 | 전원 능력치·충성·호감 최대, 상태 회복 | 인물의 것을 플레이어 진영(`__faction.__system_name == "player"`)의 산 `o_character` 전부에 건다: 능력치 20, 욕구, 행복, 치료. 충성·호감은 아직 | 4(됨) |
| | 영입 비용 0, 체류 시간, 영입·해고·이동 | B `free_lord_stay_duration`(배율로 있음). 영입·해고·이동과 비용은 재지 못했다 | 5 이후 |
| 인구·욕구 | 인구 추가·제거·상한 | 추가: B 이주 관리자의 `__next_day_migrants_bonus`(다음 이주 때 그만큼 더 온다). 소환 함수는 인자의 형을 몰라 쓰지 않았다. 제거·상한은 아직 | 4(추가 됨) |
| | 배고픔·피로·스트레스 없음, 욕구 충족 | B 모듈이 플레이어의 사람을 돌며 욕구 칸을 상한으로 써 둔다(배고픔 없음, 피로 없음, 모든 욕구). 행복은 디버그용 생각을 붙여 둔다 | 4(됨) |
| | 식량·음료 소비 없음 | 욕구를 채워 두면 먹지 않는다(2.6시간 동안 음식의 수가 그대로였다. 하루를 돌린 대조는 아직) | 4(확인 전) |
| | 출생 즉시, 임신 확률, 성장 속도, 노화 정지, 사망 방지 | 노화로 죽지 않음: B 인물마다의 `__aging.__old.__debug_is_can_die_of_old_age`(써지는 것까지 봤다). 임신·출생·성장은 아직(`ComponentPregnancy.begin_pregnant`, `debug_pregnancy_next_stage`가 있다) | 4(일부) |
| 지식 | 전부·하나 해금, 즉시 완료, 선행조건 무시 | B `__knowledge_manager`, `__knowledge_unlockeds`. 안 되면 C | 5 |
| | 연구 시간·비용·속도, 책 효과 | B 지식 구조체의 값(교본 경험은 있다) | 5 |
| 아이템 | 아이템·장비 생성·삭제·수량, 최고 등급 지급 | C `add_resource`, 인물의 `get_inventory`·`get_equipment`, `__battle_equipment_editor` | 5 |
| 군대·전투 | 부상 없는 전투, 회피 끔, 장비 파손 끔, 항복 끔 | A `is_combat_without_injuries`, `is_disable_dodge`, `is_disable_equipment_destroy`, `is_surrender_disable` | 2 |
| | 병사 추가·제거·수, 모집 비용 0·즉시 | C `debug_spawn_army`, `instant_spawn_hired_mercenaries`, B `soldier_hiring_price_skill_factor` | 5 |
| | 공격·방어·피해 배율, 받는 피해 0, 사기 | B 전투 값(`o_debug.battle_*`, `battle_params`가 옮겨진 자리), D | 5 |
| | 전투 즉시 승리, 적 사기 0 | B 전투 중인 분대의 사기를 찾는다. 안 되면 C | 5 |
| 외교 | 관계·호감 설정, 동맹·전쟁·평화 강제 | B `__factions_manager` 안에서 찾는다. 강제는 C | 6 |
| | 반란 끔, 외교 비용 0, 성공률 100% | A `is_rebellions_can_started`, B·D | 2(A), 6 |
| 종교 | 영향력·전환·설교 효과·비용·수용량 | B `church_donation_runes*`, `church_max_capacity`(있음), `debug_piety_decrease_per_hour`, `__preach_data` | 2(일부), 6 |
| 시간 | 게임 속도 0.25~50배 | B `o_time_controller`의 후보 넷을 차례로 써 보고 `__game_time`의 흐름으로 판정한다(§9) | 2 |
| | 이동 속도, 쿨다운 제거 | B 를 찾는다. 안 되면 D | 6 |
| 월드 | 지도·지역 공개, 자원 위치, 이동 제한 | D `is_in_fog_of_war`, A `is_fast_action_task_on_global_map`, B `o_global_map` | 2(A), 6 |
| | 광산·자원 생성 | C (기록해서) | 6 |
| 이벤트 | 강제 실행, 쿨다운 제거, 확률 100% | C `try_to_force_start`, B `o_data.__game_director_events_data.__params`(자리를 쟀다. `research/02`) | 6 |
| 유틸 | UI 숨기기, 디버그 표시, 게임의 디버그 창 | A `is_hide_*`, `is_gw_gui_draw_disabled`, `is_debug_enabled`, `is_show_debug_managers` | 2 |
| | 저장, 시간 정지, 인물·아이템 검색 | C `o_time_controller.pause`(기록해서), 탐색기의 찾기 | 7 |
| 프리셋 | God, Sandbox, Easy, Normal(전부 끔) | 표 항목의 묶음. 상태 파일을 통째로 갈아 끼운다 | 7 |

## 9. 게임 속도

메뉴에서는 시간이 멈춰 있어 미리 잴 수 없다(§2.3). 그래서 판정을 기능 안에 넣는다.

1. 모듈은 0.1초마다 `__game_time`을 읽어 최근 1초의 흐름(실제 1초에 얼마나 느는가)을 안다.
2. 사용자가 처음 배율을 누르면 후보를 차례로 시험한다: `time_warp_new` → `time_warp` → `__debug_custom_wrap` → `time_speed_variants[time_speed_index]`.
   **기준 흐름은 누른 뒤 1.2초 동안 새로 잰다.** 누르기 전에 재 둔 흐름에는 멈춰 있던 때(일시정지, 닫혀 있던 창)가 섞여 있을 수 있다.
   **시험에 쓰는 값은 누른 배율이 아니라 지금 `time_warp`의 2배다.** 누른 배율이 지금 속도와 같으면 "달라지지 않았다"와 "먹었다"를 가릴 수 없다.
   후보 하나마다: 원래 값을 적어 두고, 2초 동안 시험 값을 계속 써 넣고(프레임마다 한 번쯤), 흐름이 누르기 전의 2배에서 ±25% 안이면 채택한다.
   아니면 원래 값을 되돌리고 다음 후보로 간다. 읽을 수 없는 후보는 쓰지 않고 넘어간다.
3. 채택한 뒤 써 넣기를 멈추고 1.5초 뒤에도 흐름이 남는지 본다. 남으면 "한 번 쓰기", 안 남으면 "계속 쓰기"다. 그 방식으로 누른 배율을 건다.
4. 넷 다 아니면 창에 그렇게 알리고 시도마다의 값을 로그에 적는다. 그 기록으로 다음 손잡이를 정한다.

- 흐름이 0 일 때(일시정지, 메뉴)는 시험하지 않고 "일시정지를 풀고 누르세요"라고 알린다. 시험 중에 흐름이 0 이 되면 그만두고 원래 값을 되돌린다.
- 이 모듈이 쓴 자리는 쓰기 전의 값과 함께 적어 둔다. "게임에 맡김"은 적어 둔 자리를 모두 그 값으로 되돌린다.
  `time_speed_variants`의 자리는 사용자가 게임의 속도 단추를 누르면 바뀐다. 바뀌면 쓴 자리를 되돌리고 새 자리에 건다.
- 시험 중에 "게임에 맡김"이나 "모두 끄기"를 누르면 시험을 그만두고 되돌린다.
- 시간 컨트롤러가 사라졌다 돌아오면(새 게임, 불러오기) 걸어 둔 배율을 다시 건다.
- "계속 쓰기"가 게임의 일시정지를 막는지는 모른다. 첫 플레이의 기록을 보고 고친다.
- 창에는 지금의 흐름(기준의 몇 배인가)과 채택한 후보의 이름을 보인다.

## 10. 단계

단계마다 빌드 → 네이티브 시험 → 배포 → 플레이에서 확인 → 안 먹는 항목을 다음 수단으로 고친다. 단계마다 계획 문서를 따로 쓴다.

| 단계 | 내용 | 끝났다는 것 |
|---|---|---|
| 1 | 기반: 주소, 접근 층, 치트 표와 상태 파일, 적용·잠금, 왼쪽 목록 | 네이티브 시험 통과. 메뉴에서 주소 읽기·쓰기·되돌리기가 로그에 남는다 |
| 2 | 탐색기, 내장 스위치(§8의 "2"), 게임 속도 | 플레이에서 탐색기로 값을 고치고, 스위치와 속도의 효과를 본다 |
| 3가 | 원격 질의(§14)와 호출 기록기. 켜져 있는 게임에 파일로 묻고, 쓰고, 스크립트의 호출을 기록한다 | 메뉴에서 묻기·쓰기·기록·화면 뜨기가 된다. 계획: `plans/2026-10-05-cheat-menu-stage3a-remote.md` |
| 3나-1 | 경제 패널: 금화와 영지 창고의 자원을 게임의 함수로 바꾼다. 게임 속도를 게임의 함수로 건다. 계획: `plans/2026-10-05-cheat-menu-stage3b-economy.md` | 금화·자원·게임 속도의 효과를 화면에서 봤다(`research/08`. 0.6.1 까지 확인) |
| 3나-2 | 건설비 무료, 건설 조건 제거, 업그레이드 조건 제거, 즉시 업그레이드. 계획: `plans/2026-10-05-cheat-menu-stage3c-build.md` | 됐다(모듈 0.8.1, `research/09`). 조건은 지식의 판정 함수를 바꿔 풀고, 비용은 건물 종류의 등급별 비용을 0 으로 쓰고, 업그레이드는 게임의 `build_instantly()`로 끝낸다. 사용자가 잠긴 건물을 짓고 자원 없이 올리는 것을 봤다. 건설 창의 잠금 표시 지우기만 확인 전 |
| 3나-3 | 거래, 창고 용량, 생산. 배율 7개를 제 영역으로, 속도 시험의 정리. 계획: `plans/2026-10-05-cheat-menu-stage3d-production.md` | 됐다(모듈 0.9.1, `research/10`). 창고 용량과 생산 배율 셋(시간·작업 효율·생산량)은 플레이에서 확인. 거래 배율은 함수까지만(상단이 없었다), 생산 재료 없음은 게임의 판정까지만. 임금·세금·유지비·노동력은 재지 못해 만들지 않았다 |
| 4 | 인물, 영주, 인구·욕구. 계획: `plans/2026-10-06-cheat-menu-stage4-people.md` | 됐다(모듈 0.10.1, `research/11`). 한 사람의 능력치·욕구·나이·특성·행복·치료, 영주 전원·사람 전원의 일괄, 욕구 유지·언제나 행복·추가 이주민을 플레이에서 확인했다(능력치는 게임의 인물 창으로). 노화로 죽지 않음은 깃발까지만. 충성·관계, 영입, 인구 줄이기, 임신·출생, 성별·소속·직업·장비는 재지 못해 만들지 않았다 |
| 5 | 지식, 아이템, 군대·전투 | 해금, 아이템 지급, 병사·전투 항목의 효과를 본다 |
| 6 | 외교, 종교, 이벤트, 월드 | 관계 설정, 종교 값, 이벤트 강제 실행, 지도 공개의 효과를 본다 |
| 7 | 프리셋, 유틸 | 18개 영역의 항목이 모두 `Verified`다 |

## 11. 오류와 안전

- 대상이 없거나 형이 다르면 쓰지 않고 그 까닭을 창에 보인다. 게임이 죽는 길(없는 ds 번호에 ds 함수, 틀린 인자의 스크립트)은 부르기 전에 막는다.
- 치트를 끄면 처음 본 값으로 되돌린다. "모두 끄기"는 켠 것을 전부 되돌린다.
- 치트로 바뀐 값이 세이브에 굳는지는 모른다(`research/03`의 "확인하지 못한 것"). 세이브를 쓰는 실행 전에는 `tools/saves-backup.ps1`으로 사본을 뜬다.
- 모드창이 닫혀 있어도 켜 둔 치트와 잠금은 적용된다. 아무것도 켜지 않았으면 모듈은 게임의 값에 손대지 않는다.

## 12. 시험

- 네이티브(`tools/test-native.ps1`): `AskPath`(읽기·쓰기·부모), `CheatTable`(주소가 읽힌다, `Id`가 겹치지 않는다), `CheatState`(읽고 쓰면 같다, 나쁜 줄을 버린다), `Rate`.
- 게임을 켜는 확인(`tools/ui-check.ps1`, 한 번에 몰아서): 시험 설정에 `page=`(패널 고르기), `ask=<주소>`(값을 로그에), `poke=<주소>=<수>`(쓰고, 다시 읽고, 되돌린다)를 더한다.
  메뉴에서 볼 수 있는 것: 적재 판정 넷, 새 창의 화면, 주소 읽기, 인스턴스 변수·배열 원소 쓰기가 남는지.
  `ui-check.ps1`이 적재 판정(`check-load.ps1`과 같은 줄)도 함께 낸다. 실행 한 번으로 둘을 본다.
- 효과: 플레이에서 본다. 본 것은 `research/06-cheat-menu.md`에 적고 표의 `Verified`를 올린다.

## 14. 원격 질의와 호출 기록기 (3가)

금화, 창고, 건설 비용이 앉은 자리는 게임 화면에만 있다. 사용자가 탐색기로 찾게 하지 않고, 켜져 있는 게임에 **도구가 파일로 묻는다.**
사용자는 게임을 켜서 새 게임을 시작해 두기만 한다. 한 번의 실행 안에서 몇 번이든 묻고, 써 보고, 화면을 떠서 확인한다.

- **통로**: 모듈이 0.25초마다 `mods\Aurie\NlToyBox.ask.txt`를 본다. 있으면 줄들을 읽고 지운 뒤 게임 스레드에서 차례로 실행하고,
  답을 `NlToyBox.answer.txt`에 이어 쓴다(`# <id>` … `# done <id>`). 도구(`tools/ask.ps1`)는 임시 파일에 쓴 뒤 이름을 바꿔 놓는다.
- **명령**: `ask`, `list`, `tree`(깊이 N 까지), `find`·`refine`(이름·값으로 찾기), `write`(남긴다)·`poke`(되돌린다), `state`,
  `shot <이름>`(게임이 그린 프레임을 파일로), `window open|close`, `record`·`unrecord`·`records`, `call`·`method`,
  `about`(메서드가 묶인 스크립트, 구조체를 만든 생성자), `statics`(정적 메서드의 이름), `treecall`(인자 없는 스크립트가 돌려준 값), `economy`·`page`,
  `override`·`unoverride`(함수가 돌려주는 값을 바꾼다).
- **반환값 바꾸기(수단 D)**: 기록기의 훅이 원래 함수를 부른 뒤 결과를 수·불리언·`undefined`로 바꿔 돌려준다(`NlRecorder::Override`). `skip`이면 원래 함수를 부르지 않는다.
  그 함수가 무엇을 돌려주는지 기록으로 본 뒤에만 건다. 켜져 있는 게임에서 `override`로 먼저 풀어 보고, 풀린 것을 치트 표의 훅 항목으로 굳힌다.
- **호출 기록기(수단 C 의 준비)**: 스크립트의 함수에 `MmCreateHook`으로 훅을 걸고, 게임이 스스로 부를 때의 인자(수와 형, 값)와 반환값을 적는다.
  전역 스크립트는 이름으로(`gml_Script_budget_money_get`), 메서드는 주소로(`inst:o_building.set_level`) 가리킨다.
  메서드가 묶인 스크립트는 `script_get_name`으로 묻는다. 함수는 YYToolkit 의 `GetScriptData` → `CScript::m_Functions->m_ScriptFunction`에서 얻는다
  (`CallGameScriptEx`가 쓰는 길이고, 단계 0 에서 이 길로 스크립트를 불렀다. `research/01`).
  한 번 건 훅은 떼지 않는다(그 함수가 호출 스택에 있을 때 떼면 죽는다). 기록만 멈춘다. 한 실행에 64개까지(자리는 다시 쓰지 않는다. 24개는 첫 실행에서 다 썼다).
- **부르기의 규칙**: `call`과 `method`는 인자의 수와 형을 확인한 함수에만 쓴다(§3). 수는 기록이나 본문의 기계어로, 형은 기록으로 본다(`research/07`).
  부르기 전에 답 파일에 한 줄을 남긴다(죽으면 어디였는지 남는다). 스크립트는 `gml_Script_` 이름으로만 부른다(접두 없는 이름은 다른 루틴이다).
  메서드는 `NlAccess::CallMethod`로 부른다: 묶인 곳이 없는 정적 메서드는 주소의 부모에 묶는다. `about <주소>`가 부르지 않고 그것을 알려 준다.
- **잡히는 함수인가**: exe 에 직접 호출이 0곳인 함수는 참조로만 불리거나 부르는 곳마다 본문이 들어가 있다. 뒤의 경우 훅에 게임의 호출이 오지 않는다.
  `tools/re/script_calls.py`로 먼저 센다. 0곳이면 기록 0번을 "안 불렸다"고 읽지 않는다.
- **같은 요청을 두 번 실행하지 않는다**: 묻는 파일은 이름을 바꿔 집은 것만 실행하고, 모듈이 뜰 때 남아 있던 요청은 버린다.
- **실행 묶음**: `tools/session.ps1 start`가 사용자의 배율·치트 설정을 `*.kept`로 치우고 게임을 켠다(값을 재는 동안 치트가 걸려 있으면 안 된다).
  `stop`이 게임을 끄고, 설정을 되돌리고, 답과 로그를 `refs\runtime\`으로 옮긴다.

## 13. 모르는 것

- §2.2의 스위치와 함수가 실제로 무엇을 하는가. 이름에서 읽은 뜻뿐이다.
- ~~`is_debug_enabled`를 켜면 게임의 디버그 창이 뜨는가~~ → 그것만으로는 뜨지 않았다(`research/07`). `current_debug_mode`와의 관계는 모른다.
- ~~`variable_instance_set`, `array_set`, `variable_global_set`이 이 러너에서 되는가~~ → 된다(`research/06-cheat-menu.md`).
  `instance_exists`에 ref 를 넘기는 길은 아직 보지 못했다.
- ~~게임 속도의 손잡이(§9)~~ → `o_time_controller.__set_warp(배속)`(`research/08`). 멈춘 채 부르면 어떻게 되는지, 정수가 아닌 배속은 모른다.
- ~~게임 스크립트에 `MmCreateHook`이 걸리는가~~ → 걸린다(`research/07`).
- ~~금화, 창고, 능력치가 앉은 자리~~ → `research/07`. 금화는 `budget_money_change`로 바꾼다(화면에서 확인). 욕구의 자리는 아직 모른다.
- ~~영지 창고의 `change`를 묶어 부르면 화면이 따라오는가~~ → 따라온다(`research/08`). 거래·임금·건설비·생산의 자리에 쓴 값을 게임이 따르는가는 모른다.
- 용량을 넘긴 재고에 게임이 무엇을 하는가. 창고 용량을 올리는 길. "건설 목록 모두 열기"가 풀지 못하는 건설 조건이 무엇인가.
- 세이브를 불러올 때의 흐름, 치트로 바뀐 값이 세이브에 굳는가.
- 모드창을 누른 클릭이 밑의 게임에 전달되는가(`research/05`의 "확인하지 못한 것").
