# 28 — 되돌리기 셋: 역할 프리셋 되돌리기, 깡패 되돌리기, 왕국 평판 떼기 (그리고 저장·이벤트의 꼴)

2026-10-07. 모듈 0.28.0, 아덴 4일차 저녁 세이브(`time_8_23`). 실행 묶음 5번째(`verify-run5`). 원격 명령만 썼다(게임은 백그라운드. 스크립트 `verify5.ps1`, `save_shape.ps1`).
사용자가 고른 "신규 기능 검토"의 첫 묶음: 바로 만들 수 있는 셋(가)과, 한 번 켜서 꼴을 봐야 하는 둘(나: 지금 저장, 이벤트 골라 일으키기)의 기록.

## 역할 프리셋 되돌리기 — 확인

- Barra(전투 5, 지휘 9. 특성에 `stupidity` 등 6개)에게 `person <uuid> role name=general` → "능력치 2개를 올리고, 특성 7개를 붙였습니다"(전투 20, 지휘 20. fearless, protector, accurate_archer, spear_master, shield_master, terrifying, leader).
- `person <uuid> role_undo` → "능력치 2개를 되돌리고, 특성 7개를 뗐습니다". `person show`: 전투 5, 지휘 9, 특성은 입히기 전의 여섯 그대로.
- 기억이 없을 때(입히기 전, 되돌린 뒤): "되돌릴 역할 프리셋의 기억이 없습니다 (이 실행에서 입힌 것만 되돌립니다)". 기억은 모듈의 메모리에만 있다(`g_RoleMemory`).
- 장군 프리셋은 뗄 특성이 없는 사람이라 "뗐던 것을 다시 붙이기"는 이 실행에서 타지 않았다(코어 시험으로만 봤다).

## 왕국 평판 떼기 — 확인

- 라크리아(그쪽이 우리를 중립 3, 우리가 그쪽을 중립 3)에게 `diplomacy <uuid> opinion amount=3 side=them`(좋은 3개), `opinion amount=-2 side=us`(나쁜 2개)를 붙인 뒤 `diplomacy <uuid> clear side=both`:
  "그쪽이 우리를 보는 평판에서 좋은 평판 3개를 뗐습니다 (관계는 중립 -> 대립)", "우리가 그쪽을 보는 평판에서 나쁜 평판 2개를 뗐습니다 (관계는 중립 그대로)".
- 다시 `clear side=them`: "뗄 디버그 평판이 없습니다". **붙은 것이 없을 때 `detach_opinion_about_faction`을 불러도 탈이 없다**(왕의 `__opinion_minds`의 수가 그대로라 그것으로 끝을 안다).
- **떼고 나니 관계가 중립에서 대립이 됐다**(그쪽이 우리를 보는 쪽). 붙이기 전에는 중립(3)이었다. 떼기의 `__update_relations`가 평판을 다시 셈했을 때 바탕의 평판이 음수였던 것으로 보인다
  (붙일 때는 좋은 평판이 그것을 가렸다). 떼기는 "붙이기 전의 관계"를 약속하지 않는다: 게임이 지금의 평판으로 다시 셈한 관계가 된다. 창의 글이 앞뒤의 관계를 적는 까닭이다.

## 깡패 되돌리기 — 확인 전(깡패가 안 생겼다)

- 7일차 20:13 까지 흘렸지만 이 실행에서는 부랑자 셋(Ulrich 2일째, Ada 1일째, Tola 오늘부터)에 깡패가 없었다(실행 2·3 에서는 7~8일차에 있었다. 게임이 만드는 때가 실행마다 다르다).
- 코드는 들어 있다: 지정을 푼 뒤 `__is_dummy_thug`에 0 을 쓰고 두 깃발을 다시 읽는다. 단추와 결과의 글에 "확인 전"을 남겼다.

## 이벤트 감독 — 꼴만

- `inst:o_game_map_controller.__game_director`(GameDirector)의 함수: `__try_to_determine_and_start_event`, `get_super_priority_events_or_available_events`, `is_event_cooldown_is_ready`, `get_event_cooldown_days_left`,
  `apply_event_cooldowns`, `is_event_group_cooldown_is_ready`, `get_event_group_cooldown_days_left`, `__is_can_apply_super_priority_to_event_in_phase`, `__reset_event_group_super_priority_counter`, `__refresh_delayed_events_after_params_reload`.
- `inst:o_data.__game_director_events_data`: `__debug_forced_event` undefined, `__events`·`__events_by_name`·`__groups`는 ds_map 번호(255, 256, 257), `__params`, `__test_*_inited` 배열 넷(예언 6, 습격 7, 보상 9, 특별 손님 15).
- 쿨다운에 보이는 이벤트 이름: `reward_hostage_lord`, `u_guest_dog_seller`, `u_guest_escaped_lord`, `u_guest_incognito_maniac`.
- `__debug_forced_event`에 글을 쓰는 것은 원격 `write`로는 안 된다(수만 받는다). `__try_to_determine_and_start_event`는 7일차 20:13 → 23:11 에 0번 불렸다(언제 불리는지 모른다. 아침 08:00 의 이야기 창과 맞물려 있을 수 있다).
  **"이벤트 골라 일으키기"는 다음으로**: 쓸 값(이름 글인지 `__events`의 구조체인지)과 감독이 뽑는 때를 알아야 한다.

## 시간을 흘리며 본 것

- 배속이 1 로 돌아오는 때가 11:02 만이 아니다: 4일차 22:05 에도 돌아왔다(그 뒤 5·6·7일차는 11:02~11:03). 흘리는 스크립트는 바라는 배속과 다르면 다시 건다(그래서 문제가 없었다).
- 07:00 쯤(7일차 06:58)에도 멈춤이 났다(08:01 의 이야기 창과 별개. 무엇인지 보지 않았다).

## 자동 저장의 꼴 — 받았다 (`save_shape.ps1`. 세이브 파일 하나가 생겼다)

- 저장을 켜 둔 채(`cheat no_autosave off`) 9일차 06:00 을 넘기자 **`gml_Script_save_game(0, 1) -> undefined`** 가 한 번 불렸다(기록 `(int64, int64) x1`, `#1 (0, 1)`).
  파일 `아덴_Autosave_Morning_day_8_date_7_10_2026_time_11_4.norland`이 생겼다(게임의 날 세기는 모듈의 `day 9`가 아니라 `day_8`이다: 게임은 첫날을 0 으로 센다).
- 두 인자의 뜻은 모른다(0 = 자동 저장?, 1 = 아침?). 저녁의 자동 저장과 사람이 누르는 저장의 꼴을 더 보면 가릴 수 있다. **"지금 저장" 단추는 그 뒤에**(`save_game`을 본 적 없는 인자로 부르지 않는다).
- `global.__game_load_operator`(GameLoadOperator)의 함수 가운데 쓸모 있어 보이는 것: `get_quick_save`, `save_map_file`, `__get_save_file_name`, `save_delete`,
  **`load_save_file_from_active_game`·`check_can_load_save_from_active_game`**(게임 안에서 세이브를 불러오는 길. 메인 메뉴로 나가지 않고 다음 실험을 시작할 수 있을지 모른다. 부르지 않았다).
- 앞의 두 번(5일차·8일차 06:00)은 흘리는 바퀴가 06:00 을 지나친 뒤에 저장을 켜서 받지 못했다: **06:00 앞에서는 배속 4 로 낮춰 1초마다 본다**(배속 24 는 한 바퀴에 게임 48분이다).
- 그 사이 감독의 `__try_to_determine_and_start_event`가 3번 불렸다(7일차 23:11 → 9일차 06:23): `(3, undefined, true, struct@…)`, `(2, 3, true, struct@…)`, `(undefined, 2, true, struct@…)` -> undefined.
  넷째 인자의 구조체 주소가 같다(이벤트의 자료나 국면으로 보인다. 추정). 이벤트 영역의 "골라 일으키기"는 이 꼴과 `__debug_forced_event`에 쓸 값을 더 본 뒤에.

## 실행의 판정

- 적재 판정 통과. 게임의 오류 파일: 자동 저장 1건, 경고 1건(`oh no, soul isn't character or die!`. 이번에는 아무도 늙히지 않았다. 뜻은 여전히 모른다). 설정 되돌림 확인.
