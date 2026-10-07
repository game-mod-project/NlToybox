# 이벤트 탭 — 목록, 강제 발동, 예약 취소, 확인된 끝내기 (설계. 2026-10-07)

조사: `research/29-events.md`(이벤트 61개, 감독, 예약 취소의 확인, 진행 중인 것의 자리). 바탕: 0.29.1(리팩토링 C 뒤. `develop` 03c7df6).
사용자의 선택(2026-10-07): "해제"는 **예약한 강제 이벤트의 취소**와 **진행 중인 이벤트의 끝내기**(이벤트를 못 오게 끄는 것은 넣지 않는다), 크기는 B(목록 + 취소 + 확인된 끝내기), 열쇠가 없는 이벤트의 이름은 **모드가 한국어로 짓는다**.

## 1. 목표와 범위

- 모드창의 "이벤트" 영역에서 게임의 이벤트 61개를 **묶음·갈래·화면 이름·쿨다운**과 함께 보고, 줄마다 **일으키기**(지금의 길: 감독의 `__debug_forced_event` 에 구조체를 쓴다)를 누른다.
- **예약 취소**: 써 둔 강제 이벤트를 게임의 `reset_debug_forced_event()`(인자 없음)로 지운다. research/29 에서 확인했다.
- **진행 중인 것**: 습격·예언·음모·손님·소요의 상태를 읽어 보이고, **끝내기**는 게임에서 인자와 효과를 본 가족만 켠다(처음에는 다섯 모두 "확인 전"으로 꺼져 있다).
- 넣지 않는 것: 이벤트별 켜고 끄기(가중치·쿨다운 쓰기), 확인 전인 가족의 끝내기, 반란·매복·어두운 행동의 끝내기(관리자의 안쪽을 아직 읽지 않았다), 쿨다운 중인 이벤트가 강제로 오는지의 판정(확인 전이라고 창에 적는다).
- 동작의 원칙은 CLAUDE.md 그대로: 그리는 쪽은 러너를 부르지 않는다, 쓴 뒤 다시 읽어 판정한다, 게임이 부르는 꼴을 본 함수만 부른다, 본 적 없는 꼴로는 부르지 않는다.

## 2. 파일과 책임

| 파일 | 책임 |
|---|---|
| `src/core/EventPlan.hpp/.cpp`(새) | 이벤트 표 61줄, 열쇠의 규칙, 가족, 결과의 글, 끝내기 허용의 판단, 원격의 낱말. 러너에 기대지 않는다. 시험 `tests/native/test_events.cpp` |
| `src/Events.hpp/.cpp`(새, `NlEvents`) | 이벤트의 상태 스냅샷(틱이 읽는다), 일으키기·취소·끝내기(틱이 한다), 이벤트 패널 그리기, 쿨다운 지우기. World.cpp 의 이벤트 부분(이름 읽기 `ReadEventNames`, `ForceEventNow`, 쿨다운 지우기, `DrawEvents`)을 **그대로 옮긴다** |
| `src/World.cpp` | 주교 부르기, 지금 저장, 종교의 일 등록만 남는다. `WorldAct` 가운데 이벤트의 일은 `NlWorld::Do` 가 `NlEvents::Do` 로 바로 넘긴다(계절과 같은 꼴: `NlCore::IsEventAct`) |
| `src/core/WorldPlan` | `WorldAct` 에 `EventCancel`, `EventEnd`, `EventList` 를 더한다. `IsEventAct`, `WorldActNeedsKind`(끝내기는 `kind=` 를 받는다). `ForceEventReport` 는 EventPlan 으로 옮긴다 |
| `src/core/RemoteCommand.cpp` | `ParseWorldLine` 이 `events [group=] [find=]`, `event_cancel`, `event_end kind=<가족>` 을 읽는다 |
| `src/Remote.cpp` | `world` 의 처리 함수가 이벤트의 일을 `NlEvents::Do`·`NlEvents::List` 로 보낸다 |
| `src/Menu.cpp` | 이벤트 영역이 `NlEvents::Draw()` 를 그린다(지금의 `NlWorld::DrawEvents` 자리). 틱의 차례에 `NlEvents::Tick(now, visible && page == Area::Events)` 를 `NlWorld::GameTick` 바로 뒤에 둔다 |
| `src/ModuleMain.cpp` | `NlEvents::Init(log, gamedir)` 를 `NlWorld::Init` 뒤에 |
| `CMakeLists.txt`, `tests/native/main.cpp` | 새 파일 둘, 시험 묶음 하나 |

## 3. 이벤트 표(`core/EventPlan`)

한 줄: `{ 시스템 이름, 묶음, 갈래(0 BIG_THREAT, 1 SMALL_THREAT, 2 NEUTRAL, 3 GOOD), 화면 이름의 열쇠(빈 글이면 없음), 모드가 지은 한국어 이름(열쇠가 없을 때와 열쇠의 줄이 파일에 없을 때 쓴다), 가족 }`.
이름·묶음·갈래는 research/29 의 표(게임 파일 `director_params.json` 의 열쇠와 런타임의 `__type`. 값은 옮기지 않는다). 열쇠의 규칙(research/29 에서 본 것):

| 가족 | 열쇠 | 모드가 짓는 이름(열쇠가 없는 것) |
|---|---|---|
| 손님 `u_guest_*` 11 | `guest.<u_guest_ 뒤>` | 익명 넷은 `guest.incognito` 가 하나라 짓는다: 익명의 손님(철학자·주교·도적·살인마) |
| 습격 `raid_*` 7 | `raid.<raid_ 뒤>` | 숲 도적의 마을 공격·도시 공격, 산 도적의 도시 공격 |
| 예언 `prophecy_*` 6 | `prophecy.<prophecy_ 뒤>`(도적 기지는 게임의 오타 `prophecy.bandits_capm`) | — |
| 보상 | `map.reward_rich_migrants`·`_farm_grow_boost`·`_raw_collect_boost`·`_trade_request` | 보상: 물가 상승, 인질 영주, 영주 중립화, 약한 도적 기지, 행군 중인 적 분대. `ai_preach` 는 "AI 영주의 설교" |
| 음모 | `map.conspiracy` | — |
| 정치 | — | 후계자 요구(약·중·강), 왕위 요구(약·중·강), 정치가의 뇌물, 어두운 행동 |
| 반란 | — | 노예·문화·종교·충성 반란 |
| 세계 지도 | — | 봉신의 요구, 중립 강요, 왕의 조공 요구, 얼굴 없는 자의 도시 공격, 마을 공격, 봉신의 반란 |
| 그 밖 | — | 도적 이주민 |

- 모드가 지은 이름은 **추정**이다(게임의 글이 아니다). 창의 도움말과 `research/29` 에 그렇게 적는다. 원시 이름은 언제나 옆에 흐리게 보인다.
- 가족(진행 중의 자리와 끝내기의 단위): `Raid`(습격 일곱과 도적의 공격 셋, 늑대 습격 예언은 `Prophecy`), `Prophecy`, `Conspiracy`, `Guest`, `Unrest`(정치 여섯과 뇌물), `Rebellion`(반란 넷. 상태를 읽을 자리를 아직 모른다: 창에 "모름"), `None`(보상, 세계 지도, 어두운 행동, 도적 이주민).
- 함수: `const std::vector<EventRow>& EventTable()`, `const EventRow* FindEvent(name)`, `std::string EventCaptionKey(name)`(표의 열쇠), `std::string EventLabel(row, captions)`(열쇠의 줄이 있으면 그 글, 아니면 지은 이름, 그것도 없으면 원시 이름), `const char* EventTypeWord(type)`("큰 위협", "작은 위협", "보통", "좋음"), `const char* EventFamilyWord(family)`, `std::vector<std::string> EventGroups()`(표에서 모아 정렬).
- 끝내기의 후보(가족마다 하나. research/29. **처음에는 모두 `Verified = false`**): `Raid` `…__raids_manager.try_to_remove_raid`, `Prophecy` `…__province.__prophecy_manager.__reset_prophecy`, `Conspiracy` `…__province.__conspiracy.__reset_conspiracy`, `Guest` `…__province.__unique_guests.remove_guest_result`, `Unrest` `…__province.__politics_manager.__all_unrest_finish`.
  표의 줄: `{ 가족, 상태의 주소, 끝내는 함수의 주소, 인자의 꼴(글. 확인 전에는 빈 글), Verified }`. `bool EventEndAllowed(const EndRow&)` 는 `Verified` 일 때만 참.
- 결과의 글: `ForceEventReport`(지금 것), `CancelEventReport(outcome)`('n' 예약이 없었다, 'f' 함수를 부르지 못했다(까닭), 'u' 부른 뒤에도 남아 있다, 'd' 지웠다), `EndEventReport(family, outcome, detail)`('x' 확인 전, 'n' 진행 중이 아니다, 'f' 부르지 못했다, 'u' 부른 뒤에도 그대로다, 'd' 끝냈다).
- `EventStatusText(family, present, name)`: "없음" / "진행 중: <이름>" / "진행 중(이름을 읽지 못함)" / "모름".

## 4. 상태의 스냅샷과 틱(`src/Events.cpp`)

- 스냅샷(글과 수만): 게임의 이벤트 이름 목록(ds_map 의 열쇠. 지금처럼 게임 화면에서 한 번), 이벤트마다 쿨다운의 남은 날(`…__game_director.__events_cooldowns.<이름>`: 없으면 -1), 묶음마다의 남은 날(`__events_groups_cooldowns.<묶음>`), 예약된 이벤트의 이름(`inst:o_data.__game_director_events_data.__debug_forced_event` 가 구조체면 그 `__system_name`, 아니면 빈 글), 지연 생성의 수(`…__game_director.__delayed_events` 의 길이), 가족마다의 진행 중 상태(아래), 마지막 한 일의 글.
- 진행 중의 읽기(가족마다 주소 하나. 값이 undefined 거나 -4 면 "없음", 구조체면 "진행 중"이고 그 구조체의 `__system_name`·`__name` 가운데 글인 것을 이름으로):
  `Raid` `…__raids_manager.__current_raid`, `Prophecy` `…__province.__prophecy_manager.__current_prophecy`, `Conspiracy` `…__province.__conspiracy.__current_conspiracy`, `Guest` `…__province.__unique_guests.__current_guest`,
  `Unrest` `…__province.__politics_manager.__is_unrest_active_struct`(구조체 안의 불리언 가운데 참인 것이 있으면 "진행 중"이고 그 열쇠가 이름. 하나도 없으면 "없음". 열쇠의 뜻은 추정), `Rebellion` 은 읽지 않는다("모름").
- 틱: 패널이 보일 때 1초마다 읽는다(이름 목록은 비어 있을 때 5초마다. 지금과 같다). 보이지 않으면 스냅샷을 비운다(다시 열면 새로 읽는다). 큐의 일(일으키기, 취소, 끝내기, 쿨다운 지우기)은 보이지 않아도 한다. 재진입 가드(`g_Busy`)와 제 뮤텍스. 틱은 `NlWorld::GameTick` 바로 뒤.
- 일으키기: 지금의 `ForceEventNow`(쓰기 전 로그, 쓴 뒤 다시 읽어 같은 구조체인지).
- 취소: 예약을 먼저 읽어 비어 있으면 'n'. `NlAccess::CallMethod` 로 `inst:o_data.__game_director_events_data.reset_debug_forced_event` 를 인자 없이(부르기 전 로그). 뒤에 다시 읽어 undefined 면 'd', 아니면 'u'.
- 끝내기: 표의 줄이 `Verified` 가 아니면 부르지 않고 'x'. 진행 중이 아니면 'n'. 아니면 표의 꼴대로 부르고(부르기 전 로그) 상태를 다시 읽어 'd'/'u'.
- 쿨다운 지우기: 지금의 `ClearNumbers` 둘(World 에서 옮긴다).
- 창의 입력: 묶음 고르기, 찾기 칸(원시 이름과 화면 이름에서 찾는다. `NlCore::TraitMatches` 와 같은 꼴의 `EventMatches`), 가족마다의 끝내기 단추.

## 5. 창(이벤트 영역)

```
[표의 항목(치트 표·배율)]  — 지금처럼 위에
----- 예약 -----
예약된 이벤트: 음유시인 (u_guest_bard)  [예약 취소]        | 또는 "예약된 이벤트: 없음" (지연 생성 N건)
----- 이벤트 (61) -----
묶음 [전체 v]  찾기 [____]
| 묶음 | 이름                      | 갈래     | 쿨다운 | 묶음 쿨다운 |            |
| GUEST | 음유시인  u_guest_bard    | 보통     | -      | 1일        | [일으키기] |
| RAID  | 도적     raid_bandits     | 큰 위협  | -      | -          | [일으키기] |
…
----- 진행 중 -----
습격:   없음                                [끝내기 (확인 전)]
예언:   진행 중: cholera_epidemy             [끝내기 (확인 전)]
음모:   없음                                [끝내기 (확인 전)]
손님:   없음                                [끝내기 (확인 전)]
소요:   없음                                [끝내기 (확인 전)]
반란:   모름(읽을 자리를 아직 모른다)
----- 쿨다운 -----
[이벤트 쿨다운 지우기]
마지막 한 일: …
```

- 표는 `ImGui::BeginTable`(줄 61, 스크롤 영역 안. 창의 기본 너비에서 잘리지 않게 이름 칸을 가장 넓게). 쿨다운 중인 줄의 일으키기 옆에 "(쿨다운 N일)". 막지 않는다(강제로 써 두면 오는지는 확인 전. 도움말에 적는다).
- 표에 없는 게임의 이름은 맨 아래에 원시 이름과 "(표에 없음)"으로, 갈래·가족 없이. 게임에 없는 표의 줄은 보이지 않는다(그 수를 도움말에 적는다: "표의 N줄이 이 게임에 없습니다").
- 끝내기 단추는 `Verified` 가족만 눌린다. 꺼진 단추의 글은 "끝내기 (확인 전)". 반란은 단추가 없다.
- 도움말: 예약·취소의 뜻(감독이 오후에 뽑는다), 모드가 지은 이름은 추정, 쿨다운 중인 이벤트가 오는지는 확인 전, 끝내기는 가족마다 게임에서 본 뒤에만 켠다.
- 글꼴의 제약: 기호를 쓰지 않는다(한글·라틴-1 만).

## 6. 원격

- `world event name=<이름>`(그대로), `world event_cancel`, `world events [group=<묶음>] [find=<글>]`, `world event_end kind=<raid|prophecy|conspiracy|guest|unrest>`.
- `events` 의 답: 첫 줄 "예약: <이름 또는 없음> (지연 N)", 가족마다 한 줄 "<가족>: <상태>", 그 뒤 표의 줄 "<묶음>  <이름>  <화면 이름>  <갈래>  cd <남은 날 또는 ->"(묶음·찾기로 거른 것만). 끝에 "(N of 61)".
- `event_end` 는 `kind=` 가 없거나 모르는 글이면 `Fail`. 확인 전 가족이면 하지 않고 'x' 의 글.
- 낱말과 옵션의 읽기는 `ParseWorldLine`(시험). 하는 쪽은 `src/Remote.cpp` 의 `world` 처리에서 이벤트의 일을 `NlEvents` 로.

## 7. 끝내기의 확인 절차(가족마다. 코드가 아니라 실행의 규칙)

1. 그 이벤트가 **진행 중인 세이브**(사용자의 것)를 실행 묶음으로 불러온다(저장은 꺼진다). 상태의 주소를 읽어 "진행 중"을 본다.
2. 후보 함수에 `record` 를 걸고 시간을 조금 흘려 게임이 스스로 부르는 꼴을 받는다. 못 받으면 인자가 없는 함수인지 기계어로 가린다(`argc` 를 옮기지 않는가). 그래도 모르면 부르지 않는다.
3. 실행의 **맨 마지막에** 그 함수를 한 번 부르고(사용자에게 오류 창이 뜰 수 있다고 미리 알린다) 상태를 다시 읽는다. 시간을 조금 흘려 게임이 되돌리지 않는지 본다.
4. 된 것만 표의 `Verified` 와 인자의 꼴을 바꾸고 `research/29` 에 적는다. 그 전까지 단추와 원격은 'x'.
- 이번 묶음은 **확인된 끝내기 0개로 나갈 수 있다**. 세이브가 생기면 가족마다 켜기 한 번씩 따로 승인받는다.

## 8. 시험(`tests/native/test_events.cpp`)

- 표: 61줄, 이름이 겹치지 않는다, 묶음은 11개 가운데 하나, 갈래 0~3, 가족이 이름의 규칙과 맞는다(`u_guest_` → Guest, `raid_`·`*_bandits_attack_*` → Raid, `prophecy_` → Prophecy, `rebellion_` → Rebellion …), 열쇠가 있는 줄은 규칙대로(`guest.bard`, `raid.bandits`, `prophecy.bandits_capm` …), 열쇠가 없는 줄은 지은 이름이 비지 않는다.
- `EventLabel`: 열쇠의 줄이 있으면 그 글, 없으면 지은 이름, 그것도 없으면 원시 이름. `EventMatches`: 원시 이름과 화면 이름에서 대소문자 없이 찾는다.
- 끝내기: 다섯 가족의 줄이 있고 처음에는 모두 `Verified = false`, `EventEndAllowed` 는 그때 거짓. `EventStatusText` 의 네 꼴. `CancelEventReport`·`EndEventReport` 의 글.
- 원격: `world events`, `world events group=GUEST find=bard`, `world event_cancel`(옵션을 받지 않는다), `world event_end kind=raid`, `world event_end`(kind 없음 → 오류), `world event_end kind=x`(모르는 가족 → 오류). `IsEventAct`, `WorldActWords` 의 목록.

## 9. 문서와 버전

- CLAUDE.md: "종교·이벤트 패널" 항목의 이벤트 부분을 `src/Events.cpp` 로 고치고 파일 지도에 한 줄. 끝내기의 확인 절차(§7)를 한 줄로.
- `research/29`: 구현에서 본 것(창의 화면, 원격의 답, 표에 없는 이름의 수)과 확인된 끝내기.
- 버전 0.30.0. 보고서 `docs/superpowers/reviews/…maintainability-review.md` 에는 적지 않는다(기능이라 `research/29` 에).

## 10. 게임 켜기

- 1번: 적재 판정 + 실행 묶음에서 창(`window open`, `page events`, `shot`)과 원격(`world events`, `event name=`, `event_cancel`, `event_end kind=raid` 의 'x' 답)을 본다.
- 끝내기의 확인은 가족마다 1번씩, 세이브가 있을 때 따로 승인받는다(§7).
