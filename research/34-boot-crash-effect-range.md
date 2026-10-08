# 34. 좋은 효과의 범위를 켠 채로 게임을 켜면 부팅 중에 끝나던 것 (2026-10-08)

## 증상

사용자의 실행(0.31.3. 20:08). 게임이 메인 메뉴에 닿기 전에 GML 오류 창으로 끝났다:
`Step Event1` of `o_debug_timer`, "I32 argument is unset", `gml_Script_building_generic_get_array_of_all_buildings (line 60)`.

## 증거

- 모듈 로그(`NlToyBox.log`)의 마지막 줄이 `good effect ranges: walking the game data for the first time` 이었다(그 줄은 걷기 전에 적는다. 그 걷기가 건물 종류의 이름을 얻는 스크립트를 부른다).
- `aurie.log`: 20:07:56 에 YYToolkit 이 뜨고 20:08:11 에 "The GameMaker runtime has encountered an error! / I32 argument is unset". 스택에 `NlToyBox.dll` 의 주소들이 있다.
- 게임의 기록(`catched_errors_*.txt`)에 그 실행의 "Menu Opened" 가 없다: 메뉴가 뜨기 전이었다.
- 사용자의 `NlToyBox.cheats.txt` 에 `num effect_range=4` 가 있었다(19:05 에 저장됨. 그 뒤로 사용자가 게임을 켠 첫 실행이 이것이다).
- 오류의 오브젝트(`o_debug_timer`)는 모듈의 틱이 얹힌 이벤트일 뿐이다(틱은 오브젝트 이벤트의 콜백에서 돈다).

## 원인

`effect_range` 는 메인 메뉴에서도 쓰는 일이다(`JobDef::AnyScreen`. 0.31.0, research/32). 켠 채 저장돼 있으면 첫 틱부터 걸었다. 그 걷기가 부르는 스크립트는 한 줄이다:

    return global.__building_storage.get_all_names();

- 기계어(va `0x14246D150`, 103줄): 전역 변수 하나를 얻고(`g_pGlobal` 의 가상 함수, 변수 번호의 자리 `0x1476158F8`), 줄 번호 60 을 적고, 그 값의 멤버(번호의 자리 `0x1475D1AE8`)를 읽어 인자 0개로 부른다.
- 이름은 exe 에서 읽었다: YYC 는 변수 번호를 `{ const char* 이름; int 번호; }` 꼴로 들고, 본문이 읽는 것은 번호의 자리다. 그 8바이트 앞의 포인터가 이름을 가리킨다
  (`__building_storage`, `get_all_names`. 스크립트의 것은 `gml_Script_building_generic_get_array_of_all_buildings`).
- 부팅 중에는 그 전역이 아직 없다(`unset`). 그 값의 멤버를 읽으려다 "I32 argument is unset" 로 끝난다.

**왜 앞의 확인에서 보지 못했나**: 실행 묶음(`session.ps1 -Action start`)은 사용자의 설정을 치우고 켠다. 그 항목은 늘 메뉴가 뜬 뒤에 켰다. 켠 채로 부팅하는 경우를 한 번도 밟지 않았다.

## 건물의 저장소

- `global.__building_storage` 는 구조체(`BuildingStorage`)이고 칸은 `__map` 하나다(ds_map 의 번호. 그 실행에서 142).
  메서드 17개: `load`, `__parse_directory`, `__parse_single_file`, `__load_building_from_data`, `get`, `get_all_names`, `get_all_buildings`, `get_or_create`, `add_building`, `create_building`, `remove_building`,
  `rename`, `duplicate_building`, `save`, `save_building`, `create_unique_name`, `__get_building_filename`.
- **전역이 먼저 생기고 종류는 나중에 채워진다.** 확인 실행 1: 전역이 생긴 직후의 걷기(20:19:27)는 효과를 0개 찾았고 탈은 없었다. 메뉴가 뜬 것은 20:19:36, 15초 뒤의 걷기(20:19:42)가 8칸을 썼다.
- 메인 메뉴에서의 `state`: `in_game 0`, `o_main_menu 1`.

## 고침 (0.31.4)

- `NlBuildings::Ready()`: `global.__building_storage` 가 있고 undefined 가 아닌가. `ForEachType` 은 그것이 거짓이면 스크립트를 부르지 않는다(어느 길로 와도).
- 엔진(`src/Jobs.cpp`): 게임 화면 밖에서는 메뉴에서도 쓰는 일만, 게임이 건물 자료를 올린 뒤에만 한다(`NlCore::JobMayRun`). 기다리는 동안은 실패로 세지 않고 1초마다 다시 본다. 로그에 한 번 적는다
  (`…: waiting until the game has loaded its buildings`). 항목 옆에는 "게임이 자료를 읽은 뒤에 적용".
- 걸었는데 쓸 자리가 하나도 없으면 "다 썼다"로 치지 않는다(`NlCore::WalkFoundNothing`): 주기(15초)를 기다리지 않고 곧 다시 걷는다(`Retry`).
- 건물 효과의 로그(`build: N building effect(s) before any write`)는 효과가 보이는 걷기에서 한 번 적는다(부팅 중에 0개로 적고 그쳤다).

## 확인

켜기 2번. 둘 다 `session.ps1 -Action start -KeepSettings`(사용자의 설정 그대로: `effect_range=4` 가 켜진 채)로 켜고, 메뉴에서 껐다(세이브는 불러오지 않았다).
둘 다 적재 판정 통과, 게임의 오류 파일에 ERROR 없음, 새 세이브 없음, 사용자의 설정 파일 둘의 해시가 앞뒤로 같다. 답은 `refs/runtime/boot-fix-run1.*`·`boot-fix-run2.*`(추적 안 함).

| 실행 | 빌드 | 기다림 | 첫 걷기 | Menu Opened | 써짐 |
|---|---|---|---|---|---|
| 1 | `7dc7e82` | 20:19:06 | 20:19:27 (효과 0개. "다 썼다"가 됐다) | 20:19:36 | 20:19:42 `wrote 8 value(s) to x4` |
| 2 | `4600144` (배포본) | 20:25:26 | 20:25:45 `nothing to write yet` | 20:25:53 | 20:25:54 효과 10줄의 로그와 `wrote 8 value(s) to x4` |

기다렸다는 줄이 남았다는 것은 그 틱에 전역이 없었다는 것이다: 고치기 전의 코드는 그 틱에 스크립트를 불렀다.

## 보지 않은 것

- 0.31.3 으로 그 끝남을 다시 일으켜 보지는 않았다(사용자의 실행 한 번의 로그가 근거다).
- 켠 채로 부팅한 뒤 세이브를 불러와 사각형이 넓어지는 것. 메뉴에서 써진 뒤의 길은 research/32 에서 본 것과 같다.
- 종류가 반쯤 채워진 때의 걷기: 찾은 것만 쓰고 "다 썼다"가 된다. 나머지는 다음 주기(15초)의 걷기가 쓴다(엔진은 주기마다 훑어 바뀐 것만 쓴다).

## 남긴 규칙

- 게임 화면 밖(부팅 중, 메인 메뉴, 불러오는 중)에서 게임 스크립트를 부르는 길을 만들 때는 **그 스크립트가 읽는 전역이 있는지부터 본다**. 무엇을 읽는지는 기계어와 exe 의 변수 이름 표로 본다.
- 켠 것이 파일에 저장되는 기능은 **켠 채로 부팅해** 확인한다(`session.ps1 -Action start -KeepSettings`). 메뉴에서 켜 보는 것만으로는 첫 틱의 길을 밟지 않는다.
