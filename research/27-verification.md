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
