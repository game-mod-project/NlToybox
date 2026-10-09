# 지도 탭 — 새 게임의 영지 지도 생성 설정(17값)과 프리셋 (설계. 2026-10-07)

조사: `research/31-map-generator.md`(영지마다의 생성 설정 17개, 변수 55개의 파일, `regenerate_map()` 의 기록). 바탕: 0.30.1(`develop` a21cb70).
사용자의 선택(2026-10-07): 범위 **A**(런타임 17값 + 씨앗 + 프리셋. 파일 `config.json` 의 변수 55개는 넣지 않는다), 접근 **1**(값 편집 + 명시적 "다시 생성". 프리셋은 값만 채운다).

## 1. 목표와 범위

- 새 게임의 **생성기 화면**(영지 선택 뒤, 영주관 배치 전)에서 모드창의 "지도" 영역이 그 영지의 생성 설정 17개(지형 5, 가장자리 막힘 4, 자원 8)를 보이고 고치게 하고, "다시 생성"으로 게임이 부르는 함수 그대로 지도를 다시 생성시킨다.
- 프리셋: 이름으로 저장·불러오기·지우기. 불러오면 값만 채운다(생성은 단추로). 막힘 4개는 프리셋에 넣지 않는다(영지의 모양이다).
- (2026-10-10 의 확인에서 생성기가 `get_generator_seed()` 를 읽지 않아 **씨앗 고정은 뺐다**. research/31. 아래의 줄은 설계 때의 것이다.) 씨앗 고정은 **확인 전**의 기능이다: 게임의 `regenerate_map()` 이 안에서 씨앗을 무작위로 되돌리므로 `get_generator_seed()` 의 반환값을 훅으로 바꾸는 길만 남는다. 생성기가 그 함수를 읽는 것을 기록으로 본 뒤에만 켠다(못 보면 뺀다).
- 넣지 않는 것: `config.json` 의 변수 55개(데이터 오버레이의 일), 변형(`__generator_configs_name`: swamp 들. 배열 쓰기가 모듈에 없다), 게임 안(영주관을 놓은 뒤)의 지도 바꾸기, 세터 호출(`set_lakes` 들. 기록한 것은 하나뿐이다).
- 원칙은 CLAUDE.md 그대로: 그리는 쪽은 러너를 부르지 않는다, 쓴 뒤 다시 읽어 판정한다, 게임이 부르는 꼴을 본 함수만 부른다(`regenerate_map()` 인자 없음), 게임 파일의 값을 레포에 옮기지 않는다.

## 2. 파일과 책임

| 파일 | 책임 |
|---|---|
| `src/core/MapPlan.hpp/.cpp`(새) | 설정 표 17줄, 범위 당기기, 프리셋 파일의 읽기·쓰기·검사, 원격 낱말, 결과의 글, "화면이 아니면 쓰지 않는다"의 판단. 시험 `tests/native/test_map.cpp` |
| `src/MapGen.hpp/.cpp`(새, `NlMapGen`) | 생성기 화면의 판정, 스냅샷(틱), 값 쓰기, 다시 생성, 원래대로, 프리셋 파일, 지도 패널 그리기, 원격 |
| `src/core/CheatTable`·`src/Menu.cpp` | 왼쪽 목록에 영역 `Area::Map`("지도". `Panel = true`, 표의 항목 없음). 틱의 차례에 `NlMapGen::Tick(now, visible && page == Area::Map)` |
| `src/core/RemoteCommand.cpp`·`src/Remote.cpp` | `map …` 의 읽기(`ParseMapLine`)와 처리 |
| `src/ModuleMain.cpp`, `CMakeLists.txt`, `tests/native/main.cpp`·`common.hpp` | 등록 |

## 3. 설정 표(`core/MapPlan`)

한 줄: `{ 열쇠, 구조체의 칸 이름, 화면 이름, 갈래, 최소, 최대, 프리셋에 넣는가 }`. 자리는 모두 `global.__new_game_initializer.__choosed_province.__initial_area.__generator_settings.<칸>`(상수 `k_Settings`).

| 열쇠 | 칸 | 화면 이름 | 갈래 | 범위 | 프리셋 |
|---|---|---|---|---|---|
| `lakes` | `__lakes` | 호수 | 지형 | 0~5(창의 6단계) | 예 |
| `hills` | `__hills` | 언덕 | 지형 | 0~3(창의 4단계) | 예 |
| `hills_distribution` | `__hills_distribution` | 언덕 배치 | 지형 | 0~2(0 중심, 1 가장자리, 2 무작위) | 예 |
| `mountains` | `__mountains` | 산 | 지형 | 0~5 | 예 |
| `river` | `__river` | 강 | 지형 | 0~1 | 예 |
| `blocked_up`·`blocked_down`·`blocked_left`·`blocked_right` | `__blocked_*` | 막힘 위·아래·왼쪽·오른쪽 | 막힘 | 0~9(범위 미확인) | 아니오 |
| `berry`·`bush`·`clay`·`fertile`·`hop`·`iron`·`plants`·`tree` | `__berry` … | 열매·덤불·점토·비옥지·홉·철·식물·나무 | 자원 | 0~9(범위 미확인. 본 값 0~5) | 예 |

- 함수: `const std::vector<MapKnob>& MapKnobs()`, `const MapKnob* FindMapKnob(열쇠)`, `double ClampMapValue(const MapKnob&, double)`(수가 아니면 최소로, 범위 밖은 당긴다. 정수로 내림), `const char* MapGroupWord(갈래)`, `std::string HillsDistributionWord(수)`.
- 프리셋: `struct MapPreset { std::string Name; std::map<std::string, double> Values; double Seed = -1; }`. 파일 `mods\Aurie\NlToyBox.maps.txt`(사용자의 설정. 도구가 지우지 않는다). 줄: `preset <이름> lakes=1 hills=2 hills_distribution=0 mountains=5 river=0 berry=1 bush=4 clay=1 fertile=0 hop=0 iron=1 plants=3 tree=1 seed=-1`.
  `ParseMapPresets(istream) -> vector<MapPreset>`(틀린 줄은 버린다: 이름이 없거나 빈칸·`=` 가 든 이름, 모르는 열쇠, 수가 아닌 값. 같은 이름은 뒤의 것이 이긴다. 범위 밖의 수는 당긴다), `FormatMapPresets(vector) -> string`(이름순), `GoodPresetName(글)`(비지 않고 32자 안, 빈칸·`=`·제어 문자 없음).
  `UpsertPreset(vector&, MapPreset)`, `ErasePreset(vector&, 이름) -> bool`.
- 글: `MapReport(종류, 결과, 상세)`: 쓰기('w' 쓴 수, 'f' 쓰지 못한 열쇠), 다시 생성('d' 불렀다, 'f' 부르지 못했다, 'n' 화면이 아니다), 원래대로('d' 되돌린 수, 'n' 원래 값을 읽지 못했다), 프리셋('s' 저장, 'l' 불러옴(채운 수), 'x' 지움, 'm' 그 이름이 없다, 'b' 이름이 틀렸다).
- 판단: `bool MapScreen(bool InitializerActive, bool SettingsRead)`(둘 다 참일 때만 화면이다. 아니면 쓰지도 부르지도 않는다).

## 4. 모듈(`src/MapGen.cpp`)

- 화면의 판정: `global.__new_game_initializer.__is_active`(불리언)가 참이고 `k_Settings` 가 구조체로 읽히면(research/31: 생성기 화면에서 `in_game 0`, `o_main_menu 0`, 초기화기 활성). 게임 안(`InGame()` 참)이면 화면이 아니다.
- 스냅샷(탭이 보일 때 1초마다. 글과 수만): 화면인가, 17값, `__stashed_settings` 의 17값(밑줄 없는 이름), 씨앗(`global.__new_game_initializer.__generator_seed`), 프리셋의 이름들(파일은 Init 과 저장·지움 뒤에 읽는다), 마지막 한 일.
- 창의 입력은 큐의 일로: `SetValue{열쇠, 수}`(틱이 `NlAccess::WriteNumber` 로 그 칸에 쓰고 다시 읽는다. 범위는 코어가 당긴다), `Regenerate`(`NlAccess::CallMethod(k_Preview + ".regenerate_map", {}, …)`. 부르기 전 로그. 부른 뒤 스냅샷을 새로 읽는다), `Restore`(stashed 의 17값을 칸에 쓴다), `PresetSave{이름}`(지금의 17값 가운데 프리셋에 넣는 13개 + 씨앗을 파일에), `PresetLoad{이름}`(값을 칸에 쓴다. 생성하지 않는다), `PresetDelete{이름}`.
  `k_Preview = "global.__new_game_initializer.__map_preview_controller"`.
- 화면이 아니면 큐의 일을 'n' 으로 답하고 버린다(값을 쓰지 않는다).
- 씨앗 고정(확인 전): 체크와 수. 켜면 `NlRecorder::Override` 로 `global.__new_game_initializer.get_generator_seed` 의 반환값을 그 수로(치트 표의 `HookNumber` 와 같은 수단. 끄면 `Unoverride`). **생성기가 `get_generator_seed()` 를 읽는 것을 확인 절차에서 보기 전에는 체크를 꺼진 채 "(확인 전)"으로 둔다.** 못 보면 이 항목을 뺀다.
- 재진입 가드와 제 뮤텍스(Season 과 같다). 다시 생성은 게임의 지도 생성이라 오브젝트 이벤트가 많이 난다: 그동안의 틱은 `g_Busy` 로 막는다.

## 5. 창(지도 영역)

```
----- 영지의 생성 설정 -----          (생성기 화면이 아니면 이 한 줄: "새 게임의 지도 화면(영주관 배치 전)에서만 됩니다")
지형   호수 [1] [-][+]   언덕 [2] [-][+]   언덕 배치 [중심 v]   산 [5] [-][+]   강 [x]
막힘   위 [2] [-][+]  아래 [0] [-][+]  왼쪽 [3] [-][+]  오른쪽 [0] [-][+]        (영지의 모양. 프리셋에 들지 않는다)
자원   열매 [1] 덤불 [4] 점토 [1] 비옥지 [0] 홉 [0] 철 [1] 식물 [3] 나무 [1]   각각 [-][+]   (단계의 효과는 확인 전)
[다시 생성]  [원래대로]  [새로 읽기]        씨앗 고정 [ ] [-1]   (확인 전)
----- 프리셋 -----
이름 [________] [저장]      평야  [불러오기] [지우기]
                            산악  [불러오기] [지우기]
마지막 한 일: …
```

- 수는 `[-]`·`[+]` 단추로 한 칸씩(범위 안에서). 언덕 배치는 콤보(중심·가장자리·무작위), 강은 체크. 누르면 큐에 `SetValue` 하나.
- "원래대로"는 영지의 원래 값(`__stashed_settings`)으로. "새로 읽기"는 스냅샷을 바로 다시 읽는다(1초를 기다리지 않게).
- 도움말: 값은 게임의 생성기 창과 같은 자리에 쓴다, 다시 생성은 게임의 함수(`regenerate_map()`), 자원 8개는 창에 없는 값이라 단계의 효과는 확인 전, 막힘은 프리셋에 들지 않는다, 씨앗 고정은 확인 전.
- 글꼴의 제약: 기호를 쓰지 않는다.

## 6. 원격

- `map show`: 첫 줄 "화면: 예/아니오", 그 뒤 `<열쇠> <수> (원래 <수>)` 17줄, `seed <수>`, 프리셋 이름들.
- `map set <열쇠>=<수> [<열쇠>=<수> …]`(한 줄에 여럿. 모르는 열쇠·수가 아닌 값은 거절), `map regenerate`, `map restore`, `map preset save|load|delete name=<이름>`, `map seed <수|random>`(씨앗 고정의 체크와 수. 확인 전에는 거절의 글).
- 읽기는 `ParseMapLine`(시험), 처리는 `src/Remote.cpp` 의 표 한 줄.

## 7. 확인 절차(켜기 1번. 사용자가 생성기 화면까지. 영주관은 놓지 않는다)

1. 탭이 영지의 값을 보인다(아덴: 호수 1, 언덕 2, 산 5, 강 0, 철 1, 점토 1 …). `map show` 가 같다.
2. `map set lakes=3` → `map regenerate` → 지도가 바뀌고, 게임의 "지도 재생성"으로 생성기 창을 열면 호수 슬라이더가 3 이다(값이 게임의 길을 탄다).
3. `map set iron=4 clay=4` → 다시 생성 → 광산·점토 자리가 늘었는지 사용자가 센다(자원 단계의 첫 측정. 늘지 않으면 창의 "확인 전"을 그대로 둔다).
4. `map restore` → 원래 값. `map preset save name=test` → 파일의 줄; `map preset load name=test` → 값만 채워지고 생성은 없다; `map preset delete name=test`.
5. 씨앗: `record global.__new_game_initializer.get_generator_seed` 를 걸고 "생성" 한 번 → 읽히면 훅을 켜 두 번 생성해 같은 지도인지 사용자가 본다. 안 읽히면 씨앗 고정을 뺀다(스펙의 그 줄을 지운다).
6. 창의 단추(`ui click`)로 호수 `[+]` 한 번 → 값이 2 가 되고 게임의 창에도 2.

## 8. 시험(`tests/native/test_map.cpp`)

- 표: 17줄, 열쇠 고유, 칸 이름이 `__` 로 시작, 범위(지형 다섯의 최대 5·3·2·5·1), 프리셋에 드는 것 13개(막힘 넷은 아니다).
- `ClampMapValue`: 범위 밖·소수·NaN. `HillsDistributionWord`.
- 프리셋: 읽고 쓰면 같다, 틀린 줄 버리기, 같은 이름은 뒤의 것, 범위 당김, `GoodPresetName`, `UpsertPreset`·`ErasePreset`.
- 원격: `map show`, `map set lakes=3 iron=4`, `map set bogus=1`(거절), `map set lakes=x`(거절), `map regenerate`, `map preset save name=a`, `map preset load`(이름 없음 → 거절), `map seed 12345`, `map seed random`.
- `MapScreen`, `MapReport` 의 글.

## 9. 문서와 버전

- CLAUDE.md: "지도 탭" 항목(자리, 함수, 씨앗의 제약, 자원 단계의 미확인), 파일 지도에 `MapGen.cpp`, 원격 줄에 `map …`. `research/31` 에 확인의 결과. 버전 0.32.0(develop 이 0.31.x 로 올라가 바꿨다).

## 10. 게임 켜기

- 1번(§7). 씨앗 고정의 확인은 같은 켜기에서.
