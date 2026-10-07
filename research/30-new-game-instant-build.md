# 30. 새 게임의 영주관 배치에서 게임이 끝나는 것 — 즉시 건설 (2026-10-07)

## 증상

사용자의 게임(0.29.0, 0.30.0. 사용자의 치트·배율 설정)에서 **새 게임 → 영지 선택 → 최초 영주관 배치**를 누르면 GML 오류로 게임이 끝난다. 두 번 봤다(12:50, 17:46).

```
ERROR in action number 1 of Step Event0 for object o_game_map_controller:
I32 argument is undefined
gml_Script_anon_InspectionManager_…_531112662_… (line 154)   <- check_hall_inspection
gml_Script_anon_InspectionManager_…_485212661_… (line 143)   <- step
gml_Script_anon_Province_…_5241014775_… (line 1296)
gml_Object_o_game_map_controller_Step_0 (line 18)
```

`…__province.__inspection_manager` 의 `step()` 과 `check_hall_inspection()` 은 인자 없이 매 프레임 불린다(research/29). 오류는 `check_hall_inspection` 안의 154행에서 정수 인자가 undefined 였다는 것뿐이다.

## 가르기(한 번 켜서 새 게임을 되풀이. 사용자가 눌렀다)

실행 묶음(`session.ps1 start`: 사용자의 설정을 치워 아무것도 켜지 않은 상태)에서:

| 켠 것 | 결과 |
|---|---|
| 없음(대조) | 배치되고 게임이 이어진다 |
| **즉시 건설(`instant_build` = `inst:o_debug.is_instant_build_buildings` 1)만** | **영주관 배치에서 끝난다**(모듈 로그: `cheat state: none` → 창에서 켬 `cheat instant_build = 1: ok` → 끝남. 치트 파일 `on instant_build` 뿐) |

그래서 원인은 **영주관이 놓이기 전에 `is_instant_build_buildings` 가 1 인 것**이다. 다른 항목(건설 목록 모두 열기, 자원 바닥, 건설비 0 …)은 따로 가르지 않았다(즉시 건설 하나로 재현됐고, 사용자의 설정에는 즉시 건설이 들어 있었다).
게임 쪽의 까닭은 추정만 한다: 영주관이 놓이자마자 완성되면 시찰 관리자가 아직 없는 것(서기·사람·건설 기록)을 정수로 쓴다.

## 고침(0.30.1)

- 치트 표의 항목에 **먼저 있어야 하는 자리(`Cheat::Gate`)** 를 두었다. 즉시 건설의 자리는 `inst:o_game_map_controller.__province.__cached_hall`(영주관. research/07). 그 값이 읽히지 않거나 undefined·음수(noone)이면 깃발을 쓰지 않고 기다린다(항목 옆에 "영주관이 놓인 뒤에 적용", 로그 `cheat instant_build: waiting for …__cached_hall` 한 번).
  코어의 판단은 `NlCore::GateWaits`·`GateNote`(시험 `test_cheat`). 세이브를 불러오면 영주관이 있으므로 바로 쓴다.
- 확인: (여기에 적는다 — 고친 DLL 로 새 게임 → 영주관 배치 → 이어지는가, 배치 뒤 로그에 `cheat instant_build = 1: ok` 가 나는가, 그 뒤 놓은 건물이 바로 지어지는가)

## 남은 것

- `__cached_hall` 이 배치 전에 어떤 값인지(undefined 인지 noone 인지)는 로그로만 본다(기다리는 줄이 났는가).
- 다른 Toggle 항목(`build_all`, `build_free` 들)이 배치 전에 켜져 있어도 되는지는 재지 않았다(사용자의 설정에서는 즉시 건설이 함께 켜져 있었다).
