# 12. 지식·아이템·군대

치트 메뉴 5단계의 조사. 게임 버전 `0.5588.9777.0`, 모듈 0.10.1, 2026-10-06.
실행 묶음 `stage5-session1`(답: `refs/runtime/stage5-session1.answer.txt`). 세이브 "아덴"(4일차 06:00). 세이브는 `tools/load-save.ps1`로 불러왔다.

## 지식

- 지식의 종류 121개는 `inst:o_data.__knowledge_data.__knowledge_list[121]`에 있다(`KnowledgeData`). 한 칸은 구조체다:
  `__name`(`"building_mine"`, `"skill_oratory_1"`, `"joy_of_education"`), `__caption_replaced`(화면의 이름: "광산", "설득. 1권", "깨우침의 즐거움"),
  `__category`(`economic` | `cultural_knowledge` | `textbooks`), `__description_replaced`, `__allow_to_upgrade`, `__allow_to_product`.
  `KnowledgeData.get_knowledge_by_name("이름") -> 구조체`(게임이 글 하나로 80번 불렀다).
- **지식은 영주가 가진다.** `inst:o_character:<n>.__soul.__character_soul.__knowledge`(`ComponentKnowledge`): `__array_of_knowledge`(지식 구조체들).

| 함수 | 꼴 (기록) | 본 것 |
|---|---|---|
| `add_knowledge(지식 구조체, 불리언, 불리언) -> true` | 연구가 끝날 때 게임이 `(구조체, true, true)`로 불렀다 | 한 영주에게 `__knowledge_list[40]`("깨우침의 즐거움")을 그 꼴로 주자 수가 3 → 4, `is_have_knowledge` 가 참이 됐다 |
| `add_all_knowledge()` | 인자 없음(기계어: `argc`를 옮기지 않는다) | 부르자 그 영주의 지식이 121개가 됐다(안에서 `add_knowledge(구조체)`를 121번 부른다). **게임의 지식 창이 지식마다 그 영주를 소유자로 보였다**(`refs/ui/w1-03.png`) |
| `is_have_knowledge(지식 구조체) -> 불리언` | 게임이 1,230번 | |
| `get_knowledge_count() -> 수` | 인자 없음 | 3, 4, 121 |
| `remove_knowledge(인자 2)` | 부르지 않았다 | |

- 모든 지식을 준 뒤: 교과서 지식이 능력치에 적용됐다("새 레벨을 획득했습니다! 설득: 7". 인물 창의 능력치가 13~18 이 됐다), 재능("직관적인 천재")과 별명("책벌레")이 생겼다.
  잠겨 있던 건물의 판정 `KnowledgeManager.is_have_knowledge_to_upgrade_building("storage"·"temple"·"armory", 0)`이 `true`가 됐다(훅 없이).
- `KnowledgeManager.__knowledge_unlockeds`는 35 → 51 이 됐다(121 이 아니다. 무엇을 세는지는 재지 않았다). `is_knowledge_unlocked`는 구조체로도 이름으로도 `false`였다(받는 것을 모른다).
- 연구 시간: `…__library_manager.get_learn_time(구조체, 구조체, 불리언) -> 11.9`(게임이 한 번 불렀다). `override … x:0.1`로 배율을 걸었지만 그 뒤로 불리지 않아 효과를 보지 못했다
  (연구를 새로 시작할 때 불리는 것으로 보인다. 추정).
- 연구 중인 것의 자료는 `…__library_manager.__knowledge_interactions`(ds_map. 열쇠가 구조체라 주소로 들어가지 못했다). `get_knowledge_intertaction_for_soul`·`set_interaction`·`reset_interaction`은 부르지 않았다.
- 책: `…__library_manager.__books`(ds_map 7칸), `get_books_count() -> 7`.

## 소지품과 소지금

`inst:o_character:<n>.__soul.__inventory`(`ComponentInventory`): `__money`, `__resources[39]`, `__books`.

| 함수 | 꼴 (기록) | 본 것 |
|---|---|---|
| `change(자원 번호, 변화량) -> 수` | 게임이 `(23, -0)` 같은 꼴로 94번 | `change(1, 50)` 뒤 `get(1)`이 50. **게임의 인물 창의 인벤토리에 나무 50 이 보였다**(`refs/ui/w1-04.png`) |
| `get(자원 번호) -> 수` | 2,952번 | |
| `change_money(변화량) -> 0` | 게임이 `(29)`, `(13)`, `(-40)`으로 100번 | 부르지 않았다(아래의 쓰기로 봤다) |
| `set_money(수)` | 게임이 `(489)`로 1번 | |
| `get_money() -> 수` | 인자 없음 | 493, 쓴 뒤 5493 |

- `__money`에 5493 을 바로 쓰자 `get_money()`가 5493 을 줬고 인물 창의 금화가 5493 이 됐다. 게임의 함수(`change_money`)가 있으므로 치트는 그것을 쓴다.
- 장비: `__soul.__equipment`(`ComponentEquipment`): `__cached_armor.__resource`(6 = 가벼운 갑옷), `__cached_first_arm.__resource`(10 = 나무 망치), `__cached_second_arm`.
  `give_possible_equipment()`(인자 없음 29번, 구조체 하나 16번), `forced_set_first_arm(인자 있음)`, `refresh_equipment()`. 부르지 않았다.

## 군대 (재지 못했다)

- 이 세이브에는 병영도 병사도 없다(`battle_squads_manager.__array_of_squads` 0칸, `get_hiring_time` 0번).
- 이름만 봤다: `…__solders_barracks_hiring_manager`(`SoldiersBarracksHiringManager`: `try_to_hire(인자 4)`, `instant_spawn_hired_mercenaries(인자 3)`, `get_hiring_time(인자 1)`,
  `__refresh_mercenary_market()`), `…__soldiers_barracks_manager`(`add_soldier`, `fire`, `get_array_of_soldiers`), `…__solders_barracks_new_army_manager`(`add_soldier`, `create_squads`),
  이름 있는 스크립트 `debug_spawn_army`, `rebellion_debug_spawn_player_soldier`, `get_army_strength`.
- 병사가 있는 세이브에서 다시 잰다.

## 이 실행이 게임에 남긴 것

- 저장하지 않고 껐다(자동 저장 시각 18시를 넘기지 않았다). 실행 중에 한 영주에게 모든 지식, 소지금 5,000, 나무 50 을 줬다.

## 확인하지 못한 것

- 연구 시간의 배율이 실제 연구를 빠르게 하는가. 연구 중인 것을 바로 끝내는 길.
- 지식을 빼는 길(`remove_knowledge`의 꼴). 책을 만드는 길.
- 장비를 주는 길. 군대와 전투의 모든 것.
