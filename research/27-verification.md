# 27 — 확인 캠페인: 확인 전 항목 50개를 플레이에서 재다

2026-10-07. 모듈 0.27.3, 아덴 5일차 아침 세이브(`아덴_Autosave_Morning_day_5_date_6_10_2026_time_19_11`). 실행 묶음(`tools/session.ps1`)으로 켜고 원격 명령으로만 쟀다(게임의 저장은 꺼져 있다).
치트 표 77항목 가운데 `Verified = false` 인 50개가 대상이다. 보는 법은 항목마다 다르다: 화면(`shot`), 게임의 함수가 돌려주는 수(`method`), 시간을 흘린 앞뒤의 값(`ask`).
**게임 창을 앞으로 가져오거나 진짜 마우스·키보드를 쓰지 않는다**(사용자의 지시. 처음 몇 번은 그렇게 했고 그 뒤로는 하지 않았다). 게임의 창이 보이는 수는 그 창이 부르는 함수를 직접 불러 잰다.

## 실행 1 (0.27.3, `verify-run1`)

### 화면으로 본 것 (멈춘 채, 모드창을 닫고 `shot`)

| 항목 | 켠 화면 | 판정 |
|---|---|---|
| `hide_gui` | HUD(자원, 영주 초상, 아래 단추 줄, 도움말)가 모두 사라졌다. 끄자 돌아왔다 | **확인** |
| `hide_names` | 인물 위의 이름표(Shchitka, Kira, Daven)가 사라졌다. HUD 는 그대로 | **확인** |
| `show_grid` | 격자가 보이지 않았다(일시정지, 건설 모드 아님) | 확인 전 |
| `hide_popups`, `hide_bubbles` | 그 순간 화면에 알림 글과 말풍선이 없어 가리지 못했다 | 확인 전 |
| `game_debug` + `debug_managers`·`traits_windows`·`production_window`·`debug_log` | 큰 스위치와 함께 켜도(멈춘 채 2초, 흐르는 채 3초) 게임의 디버그 창이 뜨지 않았다. `o_data.current_debug_mode` 는 2 | 확인 전 |

### 게임의 함수로 본 것

- `hire_price_factor`(`o_debug.soldier_hiring_price_skill_factor`, 원래 5): 고용 창의 값은 `SoulBasic.get_soldier_cost()`가 돌려주는 수와 같았다(전투 4 인 주민 100, 수감자 45. 창을 열어 봤다).
  변수를 1·20 으로 써도(쓰인 것을 `ask`로 봤다) 그 함수는 100·45 그대로였고 다시 연 창도 그대로였다. **그 함수는 이 변수를 읽지 않거나 값이 영혼에 굳어 있다.** 확인 전으로 둔다.
- `cheat` 로 쓴 값은 다음 틱에 써진다: 같은 요청 묶음의 `ask`·`method`는 그 전에 돈다. 바꾼 뒤 2초를 두고 따로 묻는다.

### 시간을 흘려 본 것

- `rest_decrease`(`debug_rest_decrease_per_hour`) 0 과 `piety_restore` ×3 을 켠 채 06:58 → 00:00(17시간. 배속 24 는 실제 1초에 게임 24분이라 30초면 11시간이다)을 흘렸다:
  휴식(욕구 2)은 70명 가운데 39명이 100 그대로, 28명이 70~78 로 줄었다(평소의 감소와 같은 규모). 신앙심(욕구 3)은 98.2 에서 85~91 로 줄었다.
  **밤이 섞인 긴 창이라 가리지 못했다.** 휴식은 낮의 두 창(켬·끔)으로 다시 쟀다(아래). 신앙 회복은 기도한 사람을 가리지 못해 재지 못했다.
- `rest_decrease` 를 낮의 두 창으로 다시 쟀다(같은 70명. 배속 1): 08:00→10:00 은 변수 0, 10:00→12:00 은 원래 값 3.
  변수 0 의 창에서는 60명이 그대로, 영주 둘(Barra, Amelora)이 7.0 줄고 주민 여덟이 2~4 늘었다. 변수 3 의 창에서는 68명이 그대로, 둘이 1.2 늘고 **아무도 줄지 않았다.**
  **이 변수는 시간당 감소를 바로 정하지 않는다**(값이 0 일 때 줄고 3 일 때 줄지 않았다. 줄어드는 것은 활동에 매인 것으로 보인다). 확인 전으로 둔다. 자료: `A0/A1/B1.tsv`(scratchpad. 레포에 싣지 않는다).
### 되풀이해 센 것

- `safe_childbirth`: 변수 `pregnancy_mother_die` 0.1, `trait_death_in_childbirth` 0.1, `pregnancy_miscarriage_chance` 0.2 (원래 값) 인 채 Amelora(`hard_childbirth` 특성)에게 임신 시작 → 출산을 30번 되풀이했다
  (아버지 Barra. 시간은 멈춘 채. 출산 뒤의 `pregnant_forbid` 를 떼고 다시): **출산 27, 유산 3, 어머니의 죽음 0.** 영주가 10 → 37 명이 됐다.
  research/24 의 22번과 합쳐 끈 채 출산 49번에 죽음 0 이다. 변수가 출산마다 죽을 확률 0.1 이라면 0.9^49 ≈ 0.6% 의 일이다. **변수의 뜻이 추정과 다르거나 다른 조건이 있다.** 가릴 수 없다(확인 전으로 둔다).
  부작용: 어머니에게 `sin_forbidden_child`(금지된 아이)와 `homeless` 특성이 붙고 기분이 떨어졌다(아버지가 남편이 아니다).
- **자정에 게임의 "통계" 창(경제 보고서)이 떠 게임이 멈춘다**: `o_time_controller.is_fully_paused` 참, `is_hand_pause` 거짓, `is_important_notification_pause_enabled` 1, `time_warp_before_pause` 1.
  `time resume`(게임의 `set_time_speed`)으로는 풀리지 않았고 `is_important_notification_pause_enabled` 를 0 으로 써도 그대로였다.
  **`is_fully_paused` 를 거짓으로 쓰고 `__set_warp(1)` 을 걸자 창을 둔 채 시간이 흘렀다**(창의 통계 관리자 `__statistics_window_manager` 에는 닫는 함수가 없다. 창을 닫는 것은 게임 창을 눌러야 한다).
  시간을 흘리는 스크립트는 `time_warp` 가 0 으로 두 번 읽히면 그렇게 푼다.
- `cheat` 와 `method __set_warp` 의 차례: `time resume` 은 게임의 첫 속도(x1)로 풀므로 `__set_warp` 는 그 뒤에 건다(앞에 걸면 x1 로 덮인다).

## 실행 2 (0.27.3, `verify-run2`, 대조): 같은 세이브(아덴 4일차 저녁 `time_8_23`)에서 아무것도 켜지 않고 흘렸다

실행 3(켬)과 견주려는 셋: `thug_days`(범죄자가 깡패가 되기까지의 날 x10), `no_old_age_death`(영주의 노사 끔), `donation_runes`(헌금 반지 10).
대조의 할 일: 플레이어의 주민 둘을 게임이 부르는 꼴로 범죄자로 만들고(`method inst:o_dummy:<n>.c_criminal.set_criminal_scum b:1 b:1`), 영주 Barra 를 95세로(`person <uuid> age_set amount=95`), 배속 24 로 7일차 저녁까지.

- **스크립트의 두 실수**(처음 7분을 버렸다): `ask`의 글 답은 따옴표가 붙는다(`= string "5c61…"`) — uuid 를 견줄 때 따옴표를 뗀다. **배속 24 는 아침에 1 로 돌아온다**(두 번 봤다: 6일차 11:03, 7일차 11:03 쯤에 "warp was 1").
  `o_time_controller`에 `super_night_speed_try_to_start`·`try_to_stop`이 있다(밤의 빠른 속도를 게임이 아침에 되돌리는 것으로 보이지만 그 함수를 잰 것은 아니다). 시간을 흘리는 스크립트는 **배속이 바라는 수와 다르면(0 만이 아니라) 다시 건다.**
  멈춤은 **08:01** 에 났다(6일차, 7일차. `is_fully_paused` 참). 실행 1 의 자정의 통계 창과 다른 것이다(아래의 off2 에서 화면을 떴다).
- 결과(5일차 13:36 → 7일차 20:04. Barra 는 4일차 16:50 부터 95세):
  - 제가 만든 범죄자 둘(Rhea, Audric. `__criminal_begin_time` 480971 = 5일차 13:36): 2일 6시간 뒤에도 `__is_dummy_thug` 0. **깡패가 되지 않았다.**
  - **게임이 5일차 18:00 에 만든 범죄자 셋**(Draga, Ratomir, Goritsa. `__criminal_begin_time` 496801): 7일차 20:04 에 `__is_dummy_thug` **true**. 창의 "2일째 깡패".
    `become_thug`(인자 맞춤 1, 직접 호출 0곳)의 기록은 6일차 03:46 쯤부터 걸었는데 **0번** 불렸다. 그 셋이 언제 깡패가 됐는지(지정과 함께인지, 7일차 18:00 인지)는 가리지 못했다.
    **`set_criminal_scum(true, true)`로 직접 만든 범죄자와 게임이 만든 범죄자는 다르게 간다**(범죄 관리자 `__criminal_manager`에는 범죄자의 목록이 없다. 무엇이 다른지 모른다). 실행 3 은 불러오자마자 `become_thug`·`set_criminal_scum`·`push_dummy_crime`에 기록을 건다.
  - Barra: 96세로 살아 있다(`c_status.__is_dead` false). 3일 동안 노사가 없어 **대조만으로는 `no_old_age_death`를 가를 수 없다.** `ComponentAgingOld`의 `get_old_age()`는 50 을 돌려주고(593번), `process_aging`은 `()`와 `(bool)`로 불린다(751번/2.3일).
    `global.__gameplay_vars.game_death_chance_mult` 1 이 있다(뜻은 추정. 쓰지 않았다).
  - 신성 반지: 7 → 7(2.3일). 교회(`building.stone_church`, `inst:o_building:19`)는 있지만 설교가 정해져 있지 않다(`c_church.__preach` undefined). 주민의 신앙심이 0~6 이어도 반지는 늘지 않았다.
    설교의 자료 `inst:o_data.__preach_data.__preach_list[n]`(10개. `__name`, `__cost` 50, `__donate_to_listeners` 5, `__start_hour` 8). `c_church.set_preach`는 인자를 둘 받는다(기계어: `mov r10d, r9d` 뒤 `cmp esi, 2`). 순서는 모른다.
    `o_debug`의 헌금 셋: `church_donation_runes` 1, `church_donation_runes_threshold` 3, `church_donation_runes_fanatic` 2.
- 게임의 오류 파일은 이 실행 동안 424바이트 늘었다(새 오류 없음. 09:44 기준).
- 실행 2 의 뒤쪽(7일차 20:04 → 10일차 06:42)에서 더 잰 것(`flow.ps1`: 2초마다 `state`와 값 몇 개를 읽으며 배속 24):
  - **`no_tree_growth`(`inst:o_debug.is_disable_trees_grow`)**: 나무 관리자 `inst:o_game_map_controller.__main_local_map.__trees_manager`의 `__hours_passed`는 끈 채 시간마다 1 씩 올라 10 에서 0 으로 돌아왔다
    (7일차 20:10 0 → 8일차 05:01 9 → 06:50 0 → 15:32 9 → 16:28 0 → …. `__count_trees_per_period` 1). **켠 채에는 9일차 05:07 의 3 이 10일차 06:42 까지 3 그대로였다**(25시간).
    나무의 수(`__quad_tree_trees.__cached_all_objects`)는 끈 채 24시간에도, 켠 채 25시간에도 741 그대로였다(빈 자리 `__spot_list` 23, 베인 나무 0). 그래서 본 것은 "성장의 시계가 선다"까지다. 켠 것을 끄자 다시 흘렀는지는 재지 않았다.
  - **`hide_events`(`is_display_event_disabled`)**: 켠 채에도 9일차 08:01 에 "신제국" 이야기 창(계속하기 단추)이 떠서 멈췄다(`refs\ui\pause-on1-1.png`. 끈 채의 6·7·8일차 08:01 과 같다). 확인 전으로 둔다
    (그 창이 이 깃발이 말하는 "이벤트"인지 모른다).
  - **아침마다 08:01 에 멈추는 것은 "신제국" 이야기 창이다**(자정의 통계 창이 아니다. 이 세이브에서는 자정에 멈추지 않았다). 11:02~11:03 에는 배속이 1 로 돌아왔다(날마다. 켠 채도 같다).
  - 게임의 오류 파일의 경고 `oh no, soul isn't character or die!`: Barra(95세로 만든 영주)에게 8일차 05:00 쯤부터 8번(16시간), 손님 Hepamten 에게 2번. 뜻은 모른다(노사의 길과 관계있는지는 실행 3 에서 켠 채의 수와 견준다).
- **`set_preach`의 꼴**(실행의 맨 마지막에 불렀다): `set_preach(설교 자료, "uuid")`로 부르자 119번째 줄에서 `get_uuid`를 찾지 못해 게임이 끝났다
  (`Unable to get variable get_uuid from object 000001A1783A8000`). **설교자 인자는 uuid 글이 아니라 영혼(`get_uuid`가 있는 구조체)이다.** 순서는 못 가렸다(실행 3 의 끝에 (영혼, 자료)로 부른다).

## 실행 3 (0.27.3, `verify-run3`, 켬): 같은 세이브에서 `thug_days`·`no_old_age_death`·`donation_runes 10` 을 켜고 4일차 17:00 → 8일차 21:19

불러오자마자 `become_thug`·`set_criminal_scum`·`push_dummy_crime`에 기록을 걸고, 대조와 같이 범죄자 둘(Rhea, Audric)과 Barra 95세, 그리고 주민 Hilda 도 95세로(치트는 `o_character`에만 쓰므로 주민은 깃발이 그대로인 대조다).

- **`thug_days`(x10 → `dummy_criminal_days_to_thug` 20)**: 게임이 만든 범죄자 Moirin 이 **3일째에 깡패**였다(`crime list`: 부랑자 3명, 깡패 1명). 기록: `become_thug(true)` 3번, `set_criminal_scum(true, true)` 5번·`(true)` 6번, `push_dummy_crime()` 11번.
  **20일로 써 둬도 3일 안에 깡패가 됐다.** 이 변수가 깡패 전환을 정하지 않거나(다른 길: `dummy_join_to_thug_when_punish`·`when_enslave`?) 다른 조건이 있다. 확인 전으로 두되 "효과를 보지 못했다"로 적는다.
  대조(실행 2)에서는 기록을 늦게 걸어 `become_thug` 0번이었다. 게임이 만든 범죄자가 깡패가 되는 길이 `become_thug(true)`인 것은 이 실행에서 봤다.
  제 범죄자 둘(Rhea, Audric)은 8일차에 목록에서 사라졌다(대조에서는 남아 있었다). Hilda 도 사라졌다. **주민 34명 가운데 12명쯤이 떠났다**(대조에서는 늘었다). 치트 셋과 기록이 그 까닭인지는 모른다(이 실행은 하나뿐이다. Barra 의 기분이 58, 생각의 합 23 으로 낮았다).
- **`no_old_age_death`(`__debug_is_can_die_of_old_age` false. Barra 에게 써진 것을 봤다)**: Barra 97세로 살아 있다. **게임의 오류 파일에 경고가 0건**이다(4일 동안. 대조에서는 같은 구간(8일차 05:00~21:00)에 Barra 에게 `oh no, soul isn't character or die!` 8건).
  그 경고가 노사의 길에서 나는 것이라면 깃발이 그 길을 막은 것이다. 그러나 **죽음 자체는 어느 쪽에서도 보지 못했다**(영주는 그 길로 죽지 않는 것일 수 있다: "soul isn't character or die"). 한 번씩이라 확인으로 올리지 않는다.
  95세로 만든 주민 Hilda 는 목록에서 사라졌다(죽었는지 떠났는지 가리지 못했다. `person list`는 시체가 남은 사람에게만 `dead`를 붙인다).
- **`set_preach(영혼, 설교 자료)`가 맞는 순서다**(실행의 끝에 `method inst:o_building:19.c_church.set_preach p:inst:o_character:0.__soul p:inst:o_data.__preach_data.__preach_list[0]`):
  `c_church.__preach`가 구조체가 됐다: `__preach "preach_of_hope"`, `__preacher_uuid`(Barra), `__church_uuid`, `__number_of_listeners` 1, `__is_started`·`__is_finished`·`__is_player_inited`·`__is_rings_returned` false, `__reason` 0.
  게임은 끝나지 않았다. 설교는 `__start_hour` 8 이므로 그 뒤의 흐름(아래)에서 반지를 본다.
- 설교를 건 채 8일차 21:19 → 9일차 10:52 를 흘렸다(`flow-preach-on.log`): 07:35 까지 `__is_started` false 그대로였고 **08:01 에 `c_church.__preach`가 undefined 로 돌아갔다**(설교가 시작되지 않고 지워졌다. `__is_player_inited` false 였다).
  반지 7 그대로. 걸어 둔 `preach_complete`의 기록은 0번. 게임의 창이 설교를 정할 때 무엇을 더 하는지(`__is_player_inited`?)를 모른다. `donation_runes`는 확인 전으로 둔다.
- 실행 3 의 판정: 적재 통과, 게임의 오류 파일에 새 경고·오류 0, 새 세이브 없음(저장 꺼 둠).

## 캠페인의 결론 (실행 1~3. 승인 6번 가운데 3번을 썼다)

| 항목 | 결과 | `Verified` |
|---|---|---|
| `hide_gui`, `hide_names` | 화면에서 봤다(실행 1) | **참** |
| `build_all` | 사용자의 플레이에서 봤다 | **참** |
| `no_tree_growth` | 켜면 나무 관리자의 성장 시계가 선다(25시간). 나무의 수가 느는 것은 두 상태 모두 못 봤다 | **참**(메모에 그 한도를 적었다) |
| `thug_days` | 20 으로 써도 3일째 깡패. 효과를 보지 못했다 | 거짓 |
| `no_old_age_death` | 끈 채에도 켠 채에도 95세 영주가 4일 동안 살았다. 켠 채에는 경고가 사라졌다 | 거짓 |
| `donation_runes` | 설교 없이는 반지가 늘지 않고, 직접 건 설교는 08:00 에 지워졌다 | 거짓 |
| `hide_events` | 켠 채에도 08:00 의 이야기 창이 떠서 멈췄다 | 거짓 |
| `hire_price_factor` | `get_soldier_cost()`가 변수를 따르지 않았다 | 거짓 |
| `rest_decrease` | 시간당 감소를 바로 정하지 않는다(0 일 때 줄고 3 일 때 안 줄었다) | 거짓 |
| `safe_childbirth` | 끈 채 출산 49번에 죽음 0 이라 가릴 수 없다 | 거짓 |
| `piety_restore`, `production_free`, 전투 스위치, 3층(디버그 창 들) | 재지 않았다 | 거짓 |

시간을 흘리는 실험에서 배운 것(도구에 넣었다): `ask`의 글 답에는 따옴표가 붙는다, 배속은 날마다 11:00 쯤 1 로 돌아온다(바라는 수와 다르면 다시 건다), 아침 08:01 의 멈춤은 이야기 창이다(`is_fully_paused` 를 0 으로 쓰고 다시 건다),
`records <이름>`은 주소를 받는다(짧은 이름은 아무것도 찾지 못한다. 전부는 `records`), `c_church.set_preach(영혼, 설교 자료)`.
