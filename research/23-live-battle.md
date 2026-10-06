# 23. 사용자의 게임에서 본 실제 전투 (침입)

2026-10-06 · 게임 0.5588.9777.0 · 모듈 0.24.1 · **사용자가 하고 있던 게임**(실행 묶음이 아니다. 사용자의 설정 그대로이고, 저장도 사용자의 것이다)

플레이어의 사람이 낀 싸움을 처음으로 봤다. 지금까지는 싸움을 붙이는 길을 찾지 못해 전투의 배율 넷이 "게임이 받는 수가 바뀌는 것까지"였고
아군의 호출은 한 번도 없었다(`research/16`). 사용자가 침입이 왔다고 알려 줘서, 켜져 있는 게임에 읽기와 `record`만 썼다
(값을 쓰지 않았고, 게임의 함수를 부르지 않았고, 게임을 끄지 않았다). 자료는 `refs\runtime\battle-live1.*`(추적 안 함).

## 상황

- 침입자: 진영 `raid`, 갈래 2(병사) 일곱 명. `o_dummy`.
- 플레이어: 병사 26명, 분대 하나(10명. `__is_direct_control` 참).
- 사용자가 켜 둔 전투 항목: `ally_power` 2, `ally_toughness` 3, `enemy_power` 0.5, `enemy_toughness` 0.3. 아군 무적은 꺼져 있었다.

## 흐름

읽기는 실제 4초쯤마다(게임 시각으로 4분쯤마다) 했다. 사이의 일은 보지 못했다.

| 게임 시각 | 침입자 | 전투의 수 | 싸우는 아군 | 본 것 |
|---|---|---|---|---|
| 5일 18:05 ~ 18:12 | 7 | 2 | 4 | 전투 둘은 훈련이다(`__cached_is_training` 참. 아군끼리). 침입자의 기분 63, 통증 0 |
| 18:15 | 7 | 3 | 14 | 침입자 일곱이 모두 전투에 들었다(`c_battle.__battle`이 구조체). 기분 55 |
| 18:19 | 7 | 2 | 12 | |
| 18:23 | 5 | 1 | 10 | 둘이 죽었다. 남은 것의 통증 1, 1, 6, 9, 그리고 21(이 하나는 전투에서 빠져 있었다). 기분 41 ~ 48 |
| 18:26 | 2 | 1 | 10 | 통증 6 인 하나의 기분 32 |
| 18:30 | 0 | 0 | 0 | 일곱이 모두 죽었다 |

- 싸움은 게임 시각으로 15분쯤이었다. **죽은 아군 0명.** 새로 통증이 생긴 아군은 한 명(1)뿐이고, 다른 아군의 통증(3 ~ 17)은 싸움 전의 훈련에서 온 것이 그대로였다.
- 후퇴와 항복은 없었다(`__is_retreat`, `__is_surrender`가 참인 것을 보지 못했고, 그 함수들도 불리지 않았다).

## 배율 넷: 게임이 받은 수

| 함수 | 싸움 전 | 싸움 뒤 | 표본 |
|---|---|---|---|
| `get_combat_level_in_battle` 아군(x2, 상한 20) | 310번 | 404번 | `12 => 20`, `13 => 20` |
| 〃 적(x0.5) | 0번 | 50번 | 표본을 받지 못했다(아래) |
| `get_mortal_pain_threshold` 아군(x3) | 648번 | 998번 | `40 => 120` |
| 〃 그 밖(x0.3) | 343번 | 515번 | `40 => 12` |
| `take_damage` | 102번 | 134번 | `("bruise"·"bruise_light", 구조체, true) -> true` |
| `battle_hit` | 115번 | 168번 | `(구조체, 배열, undefined, 5, 0.1) -> 구조체` |

- **아군의 전투 기술이 실제 싸움에서 두 배(상한 20)로 넘어갔다.** 싸움 전의 310번은 훈련이다(훈련도 이 함수를 부른다). 싸움 동안 아군 94번, 적 50번.
- **이 싸움으로 배율의 효과를 말할 수는 없다.** 대조가 없고(배율 없이 같은 싸움을 하지 않았다) 병사 26명이 일곱을 상대했다.
- 침입자의 통증에 대해 본 것: 살아서 싸우던 침입자의 통증은 9 이하였고, 21 이 된 하나는 전투에서 빠져 있다가 다음 읽기에 죽어 있었다.
  12(40 x 0.3)를 넘기고도 싸우던 침입자는 보지 못했다. 읽기의 간격이 넓어 "12 에서 죽는다"고 적을 수는 없다.
- 적의 전투 기술의 표본이 없는 까닭: **기록의 표본은 기록을 건 뒤의 처음 여섯뿐이다.** 싸움 전에 걸어 훈련의 것 여섯이 찼다. 적의 표본을 받으려면 싸움이 붙은 뒤에 `record`를 다시 보낸다.

## 새로 본 꼴 (게임이 부르는 대로)

| 함수 | 꼴 | 불린 수 |
|---|---|---|
| `SoulBasic.get_bravery_threshold` | `() -> 200, 65, 100, 34, 70` | 541 |
| `SoulBasic.get_bravery_recovery_threshold` | 불리지 않았다 | 0 |
| `SoulBasic.__get_dodge_chance` | `(구조체) -> 28, 30, 33.6` | 52 |
| `SoulBasic.get_battle_lottery_tickets_factor` | `() -> 1.9, 0.46, 0.37` | 46 |
| `SoulBasic.is_pain_shock` | `() -> false` | 2 |
| `ComponentBattle.attack` | `(ref, undefined, undefined, true) -> undefined` | 51 |
| `ComponentBattle.get_close_combat_support_power` | `() -> -1, 0` | 51 |
| `ComponentBattle.set_current_battle` | `(구조체, 구조체) -> undefined` | 17 |
| `ComponentBattle.set_surrender` | `(false) -> undefined` | 7 |
| `ComponentBattle.__try_to_retreat_or_surrender`, `__set_retreat` | 불리지 않았다 | 0 |
| `BattleSquad.get_average_moral` | `() -> 63, 100` | 51 |
| `BattleSquad.all_squad_surrender`, `start_battle_win_animation`, `is_squad_can_surrender` | 불리지 않았다 | 0 |

이름에서 읽은 뜻은 추정이다. 값의 뜻(회피 확률이 백분율인가, 추첨의 배율이 무엇에 곱해지는가)은 재지 않았다.

## 자리

- 전투의 목록: `inst:o_game_map_controller.battle_debugger.__array_of_battles`(훈련도 든다. 하나의 안: `__array_of_teams[2]`, `__cached_is_training`, `__cached_is_hunt`, `__uuid`).
- 분대: `inst:o_game_map_controller.battle_squads_manager.__array_of_squads[n]`(`__array_of_actors`, `__cached_average_moral`, `__is_enemy_to_player`, `__is_destroyed`,
  `__faction`, `__commander`, `orders`). 정적 메서드에 `get_average_moral`, `is_in_battle`, `get_number_of_battle_ready_actors`, `all_squad_surrender`가 있다.
- 사람의 전투 구성요소: `inst:o_dummy:<n>.c_battle`(`__battle`: 전투 중이면 구조체, 아니면 `undefined`. `__squad`, `__team`, `__is_retreat`, `__is_surrender`, `__retreat_time`).
- 영혼: `__cached_pain`(통증. `-4`는 아직 셈하지 않음), `__cached_brave`, `__moral`(기분).
- **분대의 사기는 분대원의 기분으로 보인다**: 적 분대의 `get_average_moral()`이 63 이었고 침입자 일곱의 `__soul.__moral`이 63 이었다. 싸우는 동안 침입자의 기분이 63 → 55 → 45 → 32 로 내려갔다.

## 다음에 할 수 있는 것

- 사기: `BattleSquad.get_average_moral`이 수를 돌려준다. 분대를 아군과 적으로 가르는 길(`__is_enemy_to_player`)이 있으면 값을 바꿀 수 있다(훅의 self 가 분대다. 지금의 가리기는 영혼의 주소뿐이다).
  게임이 그 수로 무엇을 하는지(후퇴, 항복)는 이번에 보지 못했다: 그 함수들이 불리지 않았다.
- 배율의 효과: 배율을 끈 싸움과 견줘야 한다. 침입이 다시 오면 한 번은 끄고 본다(사용자의 게임이므로 사용자가 정한다).
- 싸움이 붙은 뒤에 `record`를 다시 보내 적의 표본을 받는다.

## 지켜보는 도구에서 배운 것

- `list <그릇> max=5`가 찾던 칸(`__array_of_squads`)을 잘랐다(자식이 여섯이었다). 칸 하나를 볼 때는 `ask`로 그 칸을 바로 묻거나 `max`를 넉넉히 준다.
- 감시를 고쳐 다시 켜는 사이에 싸움이 지나갈 수 있다. 이번에는 처음 켠 것이 싸움을 다 받았다. 걸어 둔 `record`는 게임 안에 남으므로 스크립트를 다시 켜도 호출 수는 이어진다.
- 끝난 뒤에는 내가 건 기록만 멈춘다(`unrecord <주소>`. 치트의 훅은 건드리지 않는다. `unrecord all`은 치트가 건 것의 기록까지 멈춘다).
