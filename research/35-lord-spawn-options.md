# 35. 소환할 영주의 성별·나이·문화·역할 (2026-10-10)

질문: 영주 탭의 "마우스 자리에 소환: 영주 +1"이 만드는 영주의 성별, 나이, 문화, 능력치·특성을 정할 수 있는가.
방법: 켜기 3번(세이브 `아덴_Autosave_Morning_day_4`. 5일차 아침의 것은 사용자의 플레이로 없어졌다). 첫 번(0.31.4)은 재기, 둘째(`81aa448`)는 구현의 확인, 셋째(`1873652`. 배포본)는 고친 글과 적재 판정.
답은 `refs/runtime/lord-spawn-run1.*`·`-run2.*`·`-run3.*`, 화면은 `refs/ui/lord-spawn-ui-1.png` ~ `-5.png`(추적 안 함).
세 실행 모두 모듈 로그에 오류 줄이 없고, 불러온 뒤 게임의 오류 파일에 새 줄이 없고, 새 세이브가 없고, 게임은 정상 종료했다. 세 번 모두 적재 판정을 통과했다.

## 소환기는 무엇을 하는가

게임을 켜기 전에 기계어와 exe 의 변수 이름 표로 읽었고(research/34 의 방법), 첫 실행에서 기록으로 맞춰 봤다.

- 소환기의 `__spawn_lord()`(411줄. 인자 수를 읽지 않는다)는 이것이 전부다:

      칸 = global.__current_camera 의 get_mouse_x()·get_mouse_y() 로 얻은 자리를
           …__current_local_map.get_graph().get_node_by_pos(x, y) 에 넘겨 얻는다
      …__province.debug_spawn_new_player_character(성별, 칸)

- 기록(영주 열 번): `get_mouse_x() -> 15735`, `get_mouse_y() -> 4539`(인자 없음, 수), `get_graph() -> 구조체`(지도의 `__graph` 칸과 주소가 같다),
  `get_node_by_pos(수, 수) -> 구조체`, `debug_spawn_new_player_character(int64, 구조체) -> 인스턴스`.
- **첫 인자가 성별이다**: 0 이면 `set_gender(0)` 과 이름 `m_name.…`, 1 이면 `set_gender(1)` 과 `f_name.…`. 여섯 번의 소환에서 모두 맞았다(`get_gender()` 의 1 = 여성과 같다. research/24).
- 만드는 함수(`Province.debug_spawn_new_player_character`. 466줄)가 읽는 이름: `souls_data`, `create_soul_for_lord`, `__factions_manager`, `__player_faction`, `__cultures_data`, `get_random_culture`, `get_display_x`·`y`, `mind_new_town_resident`.
  한 번의 소환에 `set_gender(int64)`, `set_age(수)`, `set_name(글)` 이 한 번씩 불린다. 나이는 19~35 로 봤다(19, 20, 25, 25, 26, 27, 27, 28, 35).
- **문화는 인자가 아니다.** 소환마다 `get_random_culture()`(인자 없음)가 두 번 불린다: **첫 번째가 영혼의 문화와 외모의 문화 이름, 두 번째가 이름의 문화**다
  (tanaya·gwelts → 문화 tanaya 에 이름 `f_name.gwelts.487`). 그래서 소환기가 만든 영주는 문화와 이름의 문화가 어긋날 수 있다.
- 소환기의 칸 `__is_culture_change`(거짓)·`__culture`(문화 구조체. 기본 tanaya)는 **영주에게는 듣지 않는다**: 켠 채 넷을 소환했는데 문화가 tanaya, voruns, voruns, makha 였다.

## 문화

- 자료: `inst:o_data.__cultures_data`(`CulturesData`). `__cultures_list`(배열 넷: gwelts, tanaya, makha, voruns. 게임이 무작위로 고르는 것), `__cultures_map`(구조체 다섯: 그 넷과 `rosters`), `__bosted_works`.
  문화 구조체: `__name`, `__caption`(화면 이름의 열쇠 `main_menu.new_game.province.kingdom.culture.<이름>`), `__color`, `__dialect`, `__hated_culture`, `__hint`, `__image`, `__language_book_name`, `__value_mind`.
- 영혼의 문화는 `__soul.__culture`(구조체)이고 외모(`__soul.__look`)가 `__culture_name` 과 `__gender` 를 따로 든다.
- **`SoulBasic.set_culture(문화 구조체)`**: 인자는 하나다(기계어: 첫 인자만 읽고, 뒤의 인자 검사처럼 보이는 것은 이벤트에 넘길 배열을 짓는 자리다).
  읽는 이름: `__culture`, `__look`, `set_culture_name`, `get_name`, `add_dialect`, `get_dialect`, `get_uuid`, `event_TARGET_soul_changed_culture`, `perform_event`.
  게임이 스스로 부르는 것은 보지 못했다(소환 열 번에 0번). 소환한 영주에게 한 번 불렀다: 문화 makha → gwelts, 외모의 문화 이름 gwelts, 방언 2 → 3, 반환 `undefined`, 이름과 외모의 해시는 그대로.
- `SoulBasic.set_gender(수)`(130줄)는 `__look.set_gender` 를 부른다. 만든 뒤에 불러 보지는 않았다(성별은 만들 때 정한다).

## 구현 (0.31.5)

- **성별**: 정하지 않았으면 지금까지처럼 `__spawn_lord()`. 정했으면 소환기가 하는 일을 같은 꼴로 한다(위의 네 호출. 성별은 `int64`). 칸을 얻지 못하면 만드는 함수를 부르지 않는다.
- **문화·나이·역할**: 만든 뒤에 그 영주에게 입힌다. 차례는 문화(`set_culture`), 나이(`set_age`. 18~80), 역할 프리셋(research/22 의 길). 새 영주는 `o_character` 의 uuid 를 앞뒤로 견줘 가린다
  (꼭 하나가 새로 생겼을 때만. `NlCore::NewcomerUuid`). 문화는 만들기 전에 게임의 목록에 있는지 본다.
- 문화 바꾸기는 한 사람에게 하는 일로도 있다(`person <uuid> culture_set name=<이름>`): 플레이어의 영주에게만, `__cultures_list` 의 넷만.
- 창: 영주 탭의 "마우스 자리에 영주 소환"에 성별, 나이, 문화, 역할 프리셋. 고른 것은 그 실행 안에서만 든다. 문화의 이름은 게임의 `main.csv` 에서 읽는다(카이덴, 타나야, 마카, 반).
- 원격 `person spawn lord [gender=male|female] [age=<18..80>] [culture=<이름>] [role=<Id>]`.

## 확인 (둘째 실행)

| 한 일 | 본 것 |
|---|---|
| `gender=female age=30 culture=gwelts role=king` | 이름 `f_name.gwelts.521`, `get_gender()` 1, 외모의 성별 1, `get_age()` 30, 문화와 외모의 문화 이름 gwelts(voruns 에서), 지휘·예절·화술 20, 지식·관리 15, 특성 여덟이 붙음 |
| `gender=male` 세 번 | 셋 모두 `m_name.…`, `get_gender()` 0. 나이는 게임이 정했다(34, 26, 34) |
| `gender=female` 두 번 | 둘 모두 `f_name.…`, `get_gender()` 1 (22, 25세) |
| `age=45` | `get_age()` 45 |
| `gender=male age=18 culture=tanaya` | 남성, 18세, 문화와 외모의 문화 이름 tanaya |
| `role=general` | "최고급 장군 프리셋 - 능력치 2개를 올리고, 특성 7개를 붙였습니다" |
| 아무것도 없이 | 지금까지의 줄("영주 하나를 만들었습니다 …") |
| `culture=rosters`, `culture=atlantis` | "게임에 없는 문화입니다". 영주가 생기지 않았다 |
| `culture_set` 을 영주에게, 같은 문화로 다시, 주민에게 | tanaya → voruns, "이미 그 문화입니다", "플레이어의 영주에게만 합니다" |
| 창: 여성, 나이 칸에 33 을 쳐 넣고 Tab, 마카, 최고급 학자, "영주 +1" | 여성 33세, 문화 makha(tanaya 에서), 교육·지식 20, 예절 15, 특성 일곱 |

소환한 영주 열둘을 둔 채 게임 시각 06:00 → 08:00 을 흘렸다: 게임의 오류 파일에 새 줄이 없다.

## 보지 않은 것

- **이름의 문화.** 문화를 바꿔도 이름은 게임이 처음 붙인 대로다(`f_name.tanaya.701` 인 makha 영주). 게임의 소환기도 이름의 문화를 따로 뽑으므로 그대로 뒀다. 이름을 그 문화의 것으로 짓는 게임의 함수는 찾지 않았다.
- 문화를 바꾼 뒤 외모의 그림이 달라지는가. 외모의 문화 이름은 바뀌지만 해시는 그대로였다(화면으로 견줘 보지는 않았다).
- 아이로 만드는 것(18세 미만). `rosters` 문화. 주민·손님의 문화 바꾸기.
- 바뀐 문화가 게임에서 무엇을 바꾸는가(방언이 하나 는 것까지 봤다. "문화가 바뀜" 이벤트를 누가 듣는지는 보지 않았다).
- 마우스가 지도 밖에 있을 때: 칸을 얻지 못하면 만들지 않게 해 두었지만 그 경우를 일으켜 보지는 않았다.

## 방법의 메모

- 게임 스크립트 하나의 본문이 읽는 이름을 한꺼번에 보려면: 함수의 주소(`tools/re/script_calls.py`)에서 첫 `ret` 뒤의 `int 3` 까지를 `dumpbin /disasm` 으로 뜨고,
  `mov r32,dword ptr [X]` 마다 `X - 8` 의 포인터가 가리키는 글을 읽는다(research/34). **함수의 끝을 정확히 잡아야 한다**: 넉넉히 뜨면 이웃 함수의 이름이 섞인다(처음에 그랬다).
- 기록의 표본은 여섯뿐이다. 소환을 되풀이할 때는 `record` 를 다시 보내 표본을 새로 받는다.
