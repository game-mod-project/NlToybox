# 05 — 모드창 (게임 안에서 배율을 조절한다)

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`, 모듈 `0.3.0`. 게임을 켠 횟수: 8. 모두 메인 메뉴까지만 갔다.
화면은 모듈이 게임의 프레임을 직접 떠서 확인했다(`tools/ui-check.ps1`, `refs/ui/*.png`. 추적 안 함).

## 된 것

- **게임 안에 모듈의 창이 뜬다.** F8로 여닫는다. Dear ImGui(v1.91.9)로 그린다.
- **슬라이더로 배율을 바꾸면 실행 중인 게임의 값에 바로 써진다.** 파일을 고치지 않는다.
- 배율은 `mods\Aurie\NlToyBox.settings.txt`에 자동으로 저장되고 다음 실행에 다시 적용된다.

| 배율 | 써 넣는 자리 | 써진 값 |
|---|---|---|
| 건물 건설 비용 | `debug_params.json`을 읽은 ds_map의 `building_resources` → 건물마다의 ds_list → `[자원, 수량]`의 수량 | 163개 |
| 시작 자원 | 같은 ds_map의 `product_count` | 3개 |
| 교본의 능력치 경험 | 지식을 담은 ds_map에서 `__category`가 `textbooks`인 구조체의 `__upgrade_skill[n].value` | 24개 |
| 뇌물 비용, 자유 영주 체류 기간, 교회·선술집 수용 인원 | `global.__gameplay_vars`의 `bribe_give_rings`, `free_lord_stay_duration`, `church_max_capacity`, `tavern_max_capacity` | 1개씩 |

써 넣은 뒤 다시 읽어 그 값인지 확인한다. 201개 모두 남았다(`0 did not stick`).

## 처음 잰 것

- `os_get_info()`의 `video_d3d11_swapchain`으로 게임의 스왑체인을 얻을 수 있다. 가상 함수표 8번(Present)에 Aurie의 `MmCreateHook`으로
  훅을 걸었고 첫 시도에 됐다. YYToolkit 소스(`Hooks.cpp`, `MI_Internal.cpp`)가 쓰는 것과 같은 방법이다.
- **Present는 게임 스레드에서 불린다.** 그리는 스레드와 오브젝트 이벤트 콜백의 스레드 번호가 같았다(실행마다).
- 뒤 버퍼는 1920x1080, `DXGI_FORMAT_B8G8R8A8_UNORM`(87), 멀티샘플 없음.
- **YYToolkit의 `EVENT_WNDPROC` 콜백은 오지 않는다.** 한 실행 동안 0번 불렸다(F8을 창 메시지로 보냈을 때도).
  그래서 모듈이 창 프로시저를 직접 바꿔 걸어(`SetWindowLongPtr`) 입력을 받는다. 그렇게 하자 F8과 마우스 메시지가 왔다.
- `debug_params.json`을 읽은 ds_map은 150번, 지식의 ds_map은 193번이었다(단계 0b와 같다). 모듈은 번호가 아니라 키 이름으로 찾는다.
- 데이터가 읽히기 전(모듈이 적재되고 20여 초)에는 대상이 없다. 모듈이 2초마다 다시 찾는다.
- 구조체에 `variable_struct_exists`, `variable_struct_get`, `variable_struct_set`이 되고, ds_list에 `ds_list_replace`, ds_map에
  `ds_map_replace`가 된다(이 러너에서 처음 써 봤다).

## 확인하지 못한 것

- **써 넣은 값을 게임이 따르는가.** 메모리의 값이 바뀐 것까지만 봤다. 게임을 플레이해 봐야 안다. 항목마다 다를 수 있다
  (게임이 그 값을 다른 곳에 미리 옮겨 두었으면 써 넣어도 달라지지 않는다).
- 새 게임을 시작하거나 세이브를 불러온 뒤에도 써 넣은 값이 남는가. 모듈이 2초마다 다시 써 넣게 해 두었으나 게임 안에서는 보지 않았다.
- 실제 마우스로 모드창을 누를 때 그 클릭이 밑의 게임에도 전달되는가. 모드창이 가져간 메시지는 게임에 넘기지 않게 했지만,
  게임이 메시지가 아닌 다른 방법으로 마우스를 읽으면 막지 못한다.
- 슬라이더는 모듈 안에서 ImGui에 입력을 넣어 움직였다(배율 1.00 → 3.22, 값 163개에 써짐, 설정 저장). 실제 마우스로는 해 보지 않았다.
  창 메시지로 보낸 마우스 입력이 모듈에 닿는 것은 봤다.
- 게임이 F8을 다른 데 쓰는가.
- **게임이 켜지다 멈춘 일이 한 번 있었다(8번 중 1번).** `aurie.log`가 YYToolkit의 `Using LEA pattern for RI ending`에서 끝났고
  3분 동안 더 나아가지 않았다. 모듈의 `ModuleInitialize`는 불리기 전이었다(`NlToyBox.log` 없음). 게임 창이 뜨지 않아 강제로 껐다.
  바로 다시 켜자 같은 파일로 정상이었다(`check-load.ps1` PASS). 원인은 모른다. YYToolkit은 이 줄 다음에 러너의 초기화 코드에
  중단점을 걸고 게임이 거기 닿기를 기다린다(`Zeus-x64.cpp`). 모드창을 넣기 전에도 이런 일이 있었는지는 재지 않았다.
