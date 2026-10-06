# 17. 장비: 선호 장비와 병과

장비 지급 단계의 조사. 게임 버전 `0.5588.9777.0`, 모듈 0.16.1, 2026-10-06.
실행 묶음 `equip-session1`, `equip-session2`(답: `refs/runtime/equip-session*.answer.txt`). 세이브 "아덴"(4일차 06:00). 게임 창과 마우스는 건드리지 않았다(백그라운드).

## 왜 넣어 준 장비가 벗겨지는가

- 병사의 소지품에 중갑(7)·검(12)·방패(15)를 넣으면 그 자리에서 착용된다(`__equipment.__cached_armor`·`__cached_first_arm`·`__cached_second_arm`의 `__resource`가 7, 12, 15).
  게임은 넣을 때 `ComponentEquipment.refresh_equipment()`(인자 없음)를 부른다.
- 몇 시간 뒤(06:00 에 넣은 것이 한 번은 10:00 전에, 한 번은 09:11 과 13:02 사이에) 장비가 벗겨지고 **영지 창고에 그대로 생긴다**(창고의 7·12·15 가 0 → 1).
  그 사이에 게임이 부른 것: `give_possible_equipment(구조체) -> undefined` 18번, `get_missing_equipment(구조체, false) -> 구조체` 18번, `clean_cache_and_state()` 18번,
  `get_unnecessary_equipment() -> 구조체` 43번. 필요 없는 장비를 돌려보내는 것이다.
- **무엇이 필요한지는 영혼마다의 선호 장비가 정한다**: `__soul.__preferred_equipment`(구조체: `__name`, `__armor_resource[0]`, `__weapon_resource[0]`, `__is_need_shield`, `__is_need_knife`).
  -1 은 없음, -2 는 아무거나. 읽은 값:

| 누구 | `__preferred_equipment.__name` | 갑옷 / 무기 / 방패 | 장비 |
|---|---|---|---|
| 영주들 | `__any__` | -2 / -2 / true | 경갑 6, 나무 망치 10 을 계속 갖고 있다 |
| 소환기의 기사(`__spawn_knight`) | `__any__` | -2 / -2 / true | 경갑·검·방패가 하루 종일 남았다 |
| 반란 함수로 만든 병사 | `__empty__` | -1 / -1 / false | 단검만. 넣어 준 것은 벗겨진다 |
| 소환기의 병사(`__spawn_soldier`) | `__empty__`(13:02 와 17:37 에 읽었다) | -1 / -1 / false | 가지고 태어난 경갑·검·방패가 하루 종일 남았다(까닭은 모른다) |

## 선호 장비의 자료와 게임의 세터

- 묶음들: `inst:o_data.__preferred_equipment_data`(`PreferredEquipmentData`: `set`, `get`, `get_array_of_equipments`, `get_random_soldier_class`).
  멤버: `__any`(-2 / -2 / 방패), `__empty`(-1 / -1), `__h_swordman`(7 / 12 / 방패), `__h_axeman`(7 / 13 / 방패), `__h_spearman`(7 / 11 / 방패), `__h_hammerhead`(7 / 10 / 방패),
  `__axeman`(6 / 13 / 방패), `__hammerhead`(6 / 10 / 방패), `__guard`(-1 / 10), `__heavy_archer`(7 / 9) … 한 묶음의 `__armor[0]`·`__weapon[0]`은 장비 구조체다(`__resource`, `__unit_equipment_alias`).
- 병과: `inst:o_data.__soldier_class_data.__classes.<이름>`(`SoldierClass`. 35개: `heavy_swordman`, `light_spearman`, `lord`, `unknown`, `wolf` …).
  한 병과는 `__equipment`(선호 장비의 묶음 하나), `__combat_skill_range`, `__system_name`을 갖는다. `heavy_swordman`은 전투 기술 13~17, 중갑·검·방패.
  **병과의 구조체는 영혼들이 함께 쓴다**: 병사 A 의 `__soldier_class.__equipment.__is_need_shield`에 1 을 쓰자 병사 B, 영주, `__preferred_equipment_data.__empty`의 것이 모두 1 이 됐다(되돌렸다).
  만든 병사 넷과 영주의 병과는 모두 `unknown`이다. 영혼의 병과에 값을 쓰지 않는다(모두에게 번진다).
- 영혼의 메서드(인자 수는 기계어로): `set_preferred_equipment(1)`, `get_preferred_equipment()`, `reset_preferred_equipment()`, `set_soldier_class(1)`, `get_soldier_class()`, `get_equipment()`.
  게임이 스스로 부른 꼴(기록): `set_preferred_equipment(구조체) -> undefined`(11시간에 33번), `get_preferred_equipment() -> 구조체`(2,254번), `reset_preferred_equipment() -> undefined`(6번),
  `set_soldier_class(구조체) -> undefined`(8번. 노예 상단이 온 뒤).
- **`set_preferred_equipment(…__preferred_equipment_data.__h_swordman)`을 그 꼴로 부르자 `-> undefined`, 그 병사의 `__preferred_equipment.__name`이 `"h_swordman"`이 됐고 8시간 반 뒤에도 그대로였다.**
  게임이 그동안 다른 영혼들에게 세터를 불렀지만 이 값은 덮이지 않았다.
- 선호 장비만 정하고 장비를 주지 않은 병사는 단검 그대로였다(창고에 중갑·검·방패를 둘씩 넣어 두었지만 네 시간 뒤 창고는 0 이 됐고 그 병사는 받지 못했다. 누가 가져갔는지는 보지 못했다).
- 선호 장비를 `h_swordman`으로 정하고 중갑·검·방패를 넣은 병사는 4시간 반 뒤(13:02 → 17:37)에도 착용하고 있었다. 같은 동안 `__empty__`인 소환기의 병사도 제 장비를 갖고 있었으므로
  **이것만으로는 선호 장비가 벗겨짐을 막는다고 말할 수 없다**(벗겨짐은 두 번 모두 아침 9~10시 쯤이었다). 아침부터 하루를 돌려 대조와 함께 본다(아래 확인 실행).

## 그 밖에 읽은 것

- `ComponentEquipment`의 메서드와 인자 수: `give_possible_equipment`(1 까지), `get_missing_equipment`(2 까지), `refresh_equipment`·`get_unnecessary_equipment`·`clean_cache_and_state`·
  `__remove_all_equipment`·`get_armor`·`get_first_arm`·`get_array_of_unit_equipment`·`send_refresh_event`(없음), `forced_set_first_arm`·`set_unarmed`(인자를 받는다), `get_equipment_cost`(1 까지).
- `give_possible_equipment`를 `u skip`으로 건너뛰게 하자(모두에게) 인자 없는 호출 13번이 건너뛰어졌다(`Result came as undefined`). 그 실험은 병사의 번호(`inst:o_dummy:<n>`)가 밀려 결과를 읽지 못했다.
  사람을 여러 시간 따라갈 때는 번호가 아니라 uuid 로 다시 찾는다.
- 전투 장비 편집기 `gm.__battle_equipment_editor`: `__array_of_equipment`(unarmed, sword, axe, spear, hammer, knife, no_armor, light_armor, heavy_armor, shield, bow, crossbow)와 상처의 이름 23개.

## 저녁 자동 저장은 18시보다 이르다

- `equip-session2`를 17:37(게임 시각 409047)까지 돌리자 게임이 `아덴_Autosave_Evening_day_4_date_6_10_2026_time_8_59.norland`를 만들었다. 16:30(405023)에는 아직 없었다.
  **저녁 자동 저장은 16:30 과 17:37 사이에 난다.** 그 파일에는 시험 상태가 들어 있다(만든 병사 넷, 넣은 장비, 창고에 넣은 장비). 지울지는 사용자가 정한다.
  시험 값이 든 실행은 게임 시각 16:30(405000) 전에 끈다.

## 확인하지 못한 것

- 선호 장비를 정한 병사의 장비가 아침의 되돌리기를 넘겨 남는가(확인 실행에서 본다). 며칠 뒤에도 남는가. 세이브에 남는가.
- 소환기의 병사가 `__empty__`인데도 제 장비를 갖고 있는 까닭. 창고의 장비를 누가 가져갔는가. 병과를 바꾸는 `set_soldier_class`의 효과.
