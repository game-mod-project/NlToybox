# 06 — 치트 메뉴 (탐색기, 내장 스위치, 게임 속도)

조사일 2026-10-05. 대상: Norland `0.5588.9777.0`, 모듈 `0.4.0`. 게임을 켠 횟수: 2. **둘 다 메인 메뉴까지만 갔다**(새 게임을 시작하지 않았다).

- 실행 1 (14:02~14:07): 모듈이 적재되기 전에 멈췄다. 아래 "켜다가 멈춘 것".
- 실행 2 (14:33~14:36, 커밋 `0722cb7`): 통과. **아래의 답은 실행 2에서 나왔다.**

화면은 `refs/ui/cheat-menu-1.png`(추적 안 함). 다시 재려면:

    pwsh -File tools/ui-check.ps1 -Name cheat-menu-1 -ShotSeconds 80 -TimeoutSec 240 -Page explorer -Path inst:o_time_controller `
      -Ask '<주소>,<주소>,…' -Poke '<주소>=<수>,…'

`-Ask`와 `-Poke`의 목록은 작은따옴표로 싼다(PowerShell 이 쉼표 목록을 인자 여럿으로 푼다).

## 된 것

- **0.4.0 이 적재된다.** 적재 판정 넷이 통과했고 게임은 정상 종료됐다(`closed`).
- **치트 메뉴가 그려진다.** 왼쪽에 영역 17개(인물·영주·지식·아이템·프리셋은 흐리고 단계가 적혀 있다), 오른쪽에 탐색기.
  한글이 깨지지 않고 표가 창 안에 들어온다.
- **탐색기가 인스턴스의 변수를 늘어놓는다.** `inst:o_time_controller`의 자식 43개(단계 0b의 덤프가 본 수와 같다). 메서드는 `method`로 가려진다.
- **주소로 읽고 쓴다.** 쓴 값이 모두 남았고(`stuck`) 원래 값으로 되돌아왔다.

| 주소 | 결과 | 쓴 길 |
|---|---|---|
| `inst:o_time_controller.time_warp_max` | 100 → 101 → 100 | `variable_instance_set` |
| `inst:o_time_controller.time_speed_variants[3]` | 12 → 7.5 → 12 | `array_set` |
| `global.__gameplay_vars.bribe_give_rings` | 10 → 11 → 10 | `variable_struct_set` |
| `global.default_falloff_max` | 11000 → 11001 → 11000 | `variable_global_set` |
| `global.__dotobjWriteTangents` (불리언) | 0 → 1 → 0 | `variable_global_set`에 불리언으로 |

쓰고, 읽고, 되돌리는 것은 한 틱 안에서 했다. 그 사이에 게임의 코드는 돌지 않았다.

## 처음 잰 것

- `variable_instance_exists`, `variable_instance_set`, `array_set`, `variable_global_set`, `is_method`가 이 러너에서 된다
  (매뉴얼의 인자 그대로). `array_set`은 넘긴 배열 그 자체에 쓴다(사본에 쓰지 않는다).
- `o_time_controller.time_speed_variants`는 `1, 3, 6, 12`다. `time_speed_index`는 0, `__debug_custom_wrap`은 -4.
- 메뉴에서 `time_warp`는 0, `__game_time`은 28836 이다(단계 0b와 같다. 시간이 흐르지 않는다).
- `o_data.current_debug_mode`는 `int64` 2 다. 뜻은 모른다.
- 메뉴에는 `o_debug`가 없다(`no instance of o_debug at 0`). 스위치는 게임을 시작해야 걸린다.
- 그리는 스레드와 게임 스레드의 번호가 이번에도 같았다(50404).

## 켜다가 멈춘 것 (두 번째)

실행 1 은 게임 창이 뜨지 않았고 240초 뒤 강제로 껐다. `NlToyBox.log`가 만들어지지 않았다.

- `aurie.log`는 17줄 1,384바이트이고 마지막 줄이 `[14:02:55] [trace] Using LEA pattern for RI ending`이다.
  정상 실행의 `aurie.log` 앞 17줄도 정확히 1,384바이트다. 줄의 꼴과 차례가 같다.
- 정상 실행에서 `NlToyBox.dll`은 그보다 한참 뒤(`Got process window!` 다음, 60번째 줄)에 적재된다.
  **이 멈춤은 이 모듈의 코드가 한 줄도 돌기 전에 났다.**
- `research/05`가 적은 것과 같은 자리다. 지금까지 켠 것 가운데 두 번이다(모드창 8번 중 1번, 이번 2번 중 1번).
- **`research/05`의 "이 줄 다음에 중단점을 걸고 기다린다"는 틀렸다.** 소스(`Zeus-x64.cpp` 115~116행)에서 그 줄 바로 다음은
  `Found lea_call_pair …`를 적는 줄이고 사이에 코드가 없다. 그 줄은 실행의 첫 `[debug]` 줄이다(앞 17줄은 모두 `[trace]`).
  러너 인터페이스를 기다리는 곳은 `MI_Aurie.cpp` 103행이고 10초 뒤 `Failed to await runner interface creation!`을 적고 끝난다.
- 원인은 모른다. 멈춘 곳이 Aurie 의 로그 출력 안인지(그 줄이 첫 `[debug]` 줄이다), 로그가 늦게 써져 마지막 줄이 진행 위치가 아닌지 가리지 못했다.
  Aurie 의 소스는 레포에 없다.
- **다음에 또 나면 강제로 끄기 전에 Aurie 콘솔 창의 글을 받아 둔다**(`refs/phase0/aurie-console.txt`처럼). 콘솔의 마지막 줄이 둘을 가른다.

대응: `NlToyBox.log`가 없고 `aurie.log`가 그 줄에서 끝나면 모듈과 무관하다. 다시 켠다(사용자에게 승인받은 횟수 안에서).

## 실행 2 뒤에 고친 것 (게임에서는 아직 돌지 않았다)

실행 2 는 커밋 `0722cb7`의 DLL 이었다. 그 뒤 전체 검토의 지적을 고쳤다(`7b5ae8a`, `b3f5e2f`, `305fe01`).

- 게임 속도: 기준 흐름을 누른 뒤에 새로 잰다. 쓴 자리마다 쓰기 전의 값을 적어 두고 그 자리로 되돌린다. 시험 중에도 끌 수 있다.
  논리를 `src/core/SpeedControl`로 옮겨 가짜 세계로 시험한다(네이티브 시험 57개).
- 잠금: 잠근 인스턴스가 바뀌면 푼다.
- 찾기: 한도 때문에 들어가지 않은 그릇의 수를 보인다.
- `ui-check.ps1`: 앞선 실행이 남긴 설정 사본(`*.kept`)이 있으면 거부한다.

## 확인하지 못한 것

- **효과 전부.** 스위치 31개가 게임을 바꾸는지, 게임 속도의 네 후보 가운데 어느 것이 속도를 정하는지. 게임 화면에서만 볼 수 있다.
- 게임 화면에서의 탐색기: 찾기에 걸리는 시간, 잠금, 인스턴스가 여럿인 오브젝트(`o_character`)의 훑기.
- `instance_exists`에 ref 를 넘기는 길. `o_time_controller`의 자식에 ref 가 있었는지 로그로는 알 수 없다.
- `is_debug_enabled`를 켜면 게임의 디버그 창이 뜨는가.
- 계속 쓰기가 게임의 일시정지를 막는가.
- 치트로 바뀐 값이 세이브에 굳는가.
- 실행 2 뒤에 고친 것이 게임에서 도는가(네이티브 시험과 빌드로만 봤다).

## 플레이에서 볼 것

게임을 켜서 새 게임을 시작하거나 세이브를 불러온 뒤 F8 을 누른다. 세이브를 쓰기 전에 `tools/saves-backup.ps1`으로 사본을 뜬다.

1. **탐색기**: `뿌리` → `o_time_controller`를 눌러 값이 움직이는 것을 본다. 수 하나를 고치고 Enter 를 친다.
2. **스위치**: `건설·생산`에서 "건물 즉시 건설"을 켜고 건물을 하나 놓는다. 바로 지어지는가.
3. **속도**: `시간`에서 일시정지를 푼 채로 `x5`를 누른다. 12초 안에 "x5 을 걸었습니다"가 뜨고 "기준의 5.00배"에 가까워지는가.
   "후보 가운데 속도를 바꾸는 것이 없었습니다"가 뜨면 `mods\Aurie\NlToyBox.log`의 `speed trial` 줄들이 다음 손잡이를 정하는 근거다.
4. **찾기**: 금화가 보이는 수(예: 3000)를 `찾기`의 값에 넣어 찾고, 돈을 쓴 뒤 새 수로 "다시 거르기"를 한다. 남는 주소가 금화의 자리다.

먹는 것과 안 먹는 것을 알려 주면 표(`src/core/CheatTable.cpp`)의 `Verified`를 올리거나 다음 수단으로 고친다.
