# 19. 외교: 관계의 종류와 왕의 평판

치트 메뉴의 외교 단계의 조사. 게임 버전 `0.5588.9777.0`, 2026-10-06.
실행 묶음 `eco-session1`(모듈 0.17.1. 답: `refs/runtime/eco-session1.answer.txt`). 세이브 "아덴"(4일차 06:00 ~ 15:25). 백그라운드, 저장하지 않았다.
아래에서 `fm` = `inst:o_game_map_controller.__factions_manager`, `pf` = `fm.__player_faction`, `X` = `fm.__array_of_factions[25]`(이웃 왕국 "크래스터").

## 세력

- `fm.__array_of_factions[57]`. 왕국은 `__system_name`이 `faction.new.name.<수>`인 24개다(`__tags` 9 또는 4105). 플레이어는 45번(`player`, uuid `0a9e8ec091695bb9`).
  나머지는 게임의 꾸러미다: `forest_bandits`, `mountain_bandits`, `wolves`, `raid`, `traders`, `holy_synod`, `inquisition`, `free_lords`, `unique_guests`, `player_village`, `player_untitled`,
  `rebellious_*`, `mercenaries`, `migrants`, `main_faceless` …
- `Faction.get_caption()`(인자 없음. 불러서 봤다) → 현지화된 이름: `X` "크래스터", `pf` "아덴". `get_name()`·`get_raw_caption()`은 열쇠(`"faction.new.name.22"`)를 준다.
  세력의 도시(`__cached_town.__name`)도 같은 열쇠다.

## 관계의 종류 (FactionsAlliesMatrix)

- `fm.__allies_matrix`(`FactionsAlliesMatrix`): `__matrix.<세력 A 의 uuid>.<세력 B 의 uuid>` = A 가 B 를 보는 관계(수). **방향이 있다**(플레이어 → X 는 3, X → 플레이어는 7 이었다).
  `__is_faction_enemy_for_all.<uuid>`(11칸: 도적, 늑대, 습격, 반란 꾸러미 등).
- 수의 뜻(`relationship_to_string(수)`. 게임이 `(int64)`로 13번 불렀고, 0 ~ 8 을 넣어 불렀다):
  **0 allies, 1 enemies, 2 deadly enemies, 3 neutrals, 4 friends, 5 vassal, 6 sir, 7 opponent**(8 은 `"UNDEFINED! 8"`).
- 게임이 부른 꼴(기록. 9시간 반): `get_relationship(세력, 세력) -> int64` 5,139번, `set_relationship(세력, 세력, int64) -> undefined` 905번,
  `is_enemy_to_all(세력) -> 불리언` 4,388번, `relationship_to_string(int64) -> 글` 13번. `set_enemy_for_all` 0번.
- `Faction.get_relation_with(세력) -> int64`는 수치가 아니라 **이 종류**를 준다(`get_relationship`을 거친다: 그 함수의 답을 1 로 바꾸자 1 이 나왔다).
- **`Faction.is_enemy_with(세력)`는 `get_relationship`을 거치지 않는다**: 그 답을 1(enemies)이나 4(friends)로 바꿔도, 행렬의 칸에 1 을 써도 그대로였다.
  관계가 2(deadly enemies)인 세력(도적, 그리고 평판을 −67 로 내린 왕국)에는 참이었다. 무엇을 읽는지는 가리지 못했다.

## 게임이 관계를 다시 셈한다

- `Faction.__update_relations(세력) -> undefined`(955번)가 `set_relationship(이 세력, 그 세력, 종류)`를 부른다. 종류는 **이 세력의 왕이 그 세력의 왕에게 갖는 평판**에서 나온다:
  그 안에서 `OpinionMinds.get_opinion(구조체) -> 수`가 불린다(플레이어의 왕 → X 의 왕 10, X 의 왕 → 플레이어의 왕 −7).
- **행렬에 바로 쓴 값은 남지 않는다**: `set_relationship(pf, X, 1)`로 칸이 3 → 1 이 됐지만 `pf.__update_relations(X)`를 부르자 3 으로 돌아왔다.
- 평판은 왕의 인물 자료에 있다: `…__soul.__character_soul.__opinions`(`OpinionMinds`): `__opinion_minds[]`(붙은 평판: `__character_uuid`, `__generic_opinion_mind`, `__lifetime`, `__opinion_modify_factor`),
  메서드 `opinion_attach`, `get_opinion`, `is_enemy`, `is_friends`, `is_deadly_enemy`, `detach_generic_opinion_mind`, `get_opinion_minds_by_generic` …
- 다른 세력과의 관계의 캐시 `…__province.__other_faction_relation_cache.<1|2|3>.<uuid>`(−7)는 평판을 바꿔도 그대로였다(캐시다. 쓰지 않는다).

## 평판을 붙이는 게임의 함수

- 게임의 자료에 평판의 종류가 337개 있다(`inst:o_data.opinion_mind_*`). 디버그용:

| 자료 | `__opinion_modify` | `__duration` | `__stack_limit` |
|---|---|---|---|
| `opinion_mind_debug_positive` | +5 | −4(끝이 없다) | 50 |
| `opinion_mind_debug_negative` | −5 | −4 | 50 |
| `opinion_mind_debug_good` | +10 | 345600(나흘) | 999 |
| `opinion_mind_debug_bad` | −10 | 345600 | 999 |

  외교의 것: `opinion_mind_declare_peace`(+20, 20일), `opinion_mind_defence_alliance`(+15), `opinion_mind_king_gift`(0. 함수로 셈하는 것으로 보인다) …
- **`Faction.attach_opinion_about_faction(대상 세력, 평판의 자료) -> 구조체`**. 게임이 `(구조체, 구조체) -> 구조체`로 한 번 불렀다. 인자 맞춤은 4.
  `X.attach_opinion_about_faction(pf, o_data.opinion_mind_debug_positive)`를 그 꼴로 부르자 구조체가 돌아왔고(안에서 `opinion_attach(구조체, 구조체, 1, undefined) -> 구조체`),
  X 의 왕의 평판이 −7 → −2, `X.__update_relations(pf)` 뒤 X → 플레이어의 관계가 **7(opponent) → 3(neutrals)**.
- 문턱(이 한 쌍에서 5씩 붙이며 본 것): −2 ~ 23 은 neutrals, **28 에서 friends**(58 까지 friends). 다른 왕국에 −5 씩: 첫 번째에서 opponent, 네 번째(−20 쯤)에서 **enemies**,
  아홉 번째(−45 쯤)에서 **deadly enemies**. 그때 그 왕국의 `is_enemy_with(pf)`와 `is_deadly_enemy_with(pf)`가 참이 됐다(플레이어 쪽의 `is_enemy_with`는 거짓 그대로: 방향이 있다).
  그 왕국의 마지막 평판은 −67 이었다(−5 × 12 = −60 보다 7 이 더 내려갔다. 무엇이 더 붙었는지는 보지 못했다).

## 협정 (FactionsAgreementMatrix)

- `fm.__agreement_matrix`: `set_agreement`(인자 맞춤 4), `get_agreements`(없음), `reset_agreement`(5), `reset_agreements_with_all_factions`(3), `is_has_agreement`.
  게임이 부른 꼴: `is_has_agreement(세력, 세력, int64) -> 불리언` 1,195번(종류는 4 와 192). `set_agreement`·`reset_agreement`는 0번.
  `Faction.is_declared_peace_with(세력)` 165번, `is_declared_defence_alliance_with(세력)` 199번, `is_declared_trade_agreement_with(세력)` 31번(모두 거짓).
- 플레이어의 협정 칸(`__matrix.<uuid>`)은 비어 있었다.

## 기계어로 읽은 인자 수 (부르지 않은 것들)

`force_neutrality` 1, `is_can_be_force_neutraled`·`is_was_force_neutrality_recently` 없음, `detach_opinion_about_faction` 인자를 받는다, `reset_vassal` 1, `get_strength` 2 까지,
`is_can_attack_or_help_against_faction` 3, `order_66` 2, `set_enemy_for_factions_without_tags` 1, `set_tags`·`remove_tags`·`is_has_tags` 1, `get_faction` 1(게임은 글로 부른다: `"holy_synod"`),
`get_by_uuid` 1, `get_factions_gui_struct`·`get_agreement_matrix`·`get_allies_matrix` 없음.
`add_to_alliance_with_leader`·`vassalise_by_faction`·`is_can_be_attacked`는 "인자를 옮기지 않는다"로 읽혔지만 이름으로는 인자를 받을 것 같다(읽는 창이 들머리 40줄이다). 믿지 않는다.

## 세이브

- "아덴" 세이브에 `"opinion_minds"`, `"allies_matrix"`, `"agreement_matrix"`, `"is_faction_enemy_for_all"` 열쇠가 없다(밑줄이 있든 없든 0곳). 평판은 버퍼로 저장되는 것으로 보인다
  (`OpinionMinds.get_buffer_for_serialize`). **붙인 평판이 세이브에 남는지는 재지 않았다.**

## 둘째 실행에서 본 것 (`eco-session2`, 모듈 0.18.0)

- 게임이 부른 꼴(06:00 ~ 10:20): `Faction.get_king_character_soul() -> 구조체` 34번, `Faction.is_destroyed() -> false` 19,785번. `get_caption()`은 0번(창이 그릴 때만 부르는 것으로 보인다. 직접 불러서는 됐다).
- **평판을 떼는 함수**: `X.detach_opinion_about_faction(pf, o_data.opinion_mind_debug_positive) -> undefined`. 디버그 평판 둘을 붙인 뒤(평판 −7 → 3) 한 번 부르자 −2 가 됐다(**하나를 뗀다**).
  아무것도 붙어 있지 않을 때 부르면 어떻게 되는지는 보지 않았다. 그래서 패널에는 떼는 단추를 두지 않았다(반대쪽을 붙여 상쇄한다).

## 만든 것 (모듈 0.19.0)

- 외교 패널(`src/Diplomacy.cpp`, `core/DiplomacyPlan`): 왕국 24개의 이름(`get_caption()`)과 관계의 종류(행렬의 칸을 읽는다: 그쪽이 우리를, 우리가 그쪽을).
- 왕국마다 "우호"·"중립"·"적대"와 "+5"·"-5", 위에 "모든 왕국과 우호로"·"중립으로". 평판을 갖는 쪽의 `attach_opinion_about_faction(대상 세력, 디버그 평판)`을 부르고
  `__update_relations(대상 세력)`으로 다시 셈하게 한 뒤 행렬의 칸을 다시 읽는다. 바라는 관계가 될 때까지 한 걸음씩 한다(`NlCore::StepToward`. 한쪽에 40개까지).
  적대는 철천지원수(2)까지 내린다(`is_enemy_with`가 참이 되는 관계). 동맹·봉신·주군(0, 5, 6)은 건드리지 않는다.
- 원격: `diplomacy list`, `diplomacy <uuid|all> <friends|neutral|hostile> [side=them|us|both]`, `diplomacy <uuid> opinion amount=<수>`. `queue=1`이면 창의 단추처럼 쌓기만 한다.

## 확인 실행 (모듈 0.19.0, 2026-10-06)

실행 묶음 `dip-session1`(답: `refs/runtime/dip-session1.answer.txt`). 06:00 ~ 12:20, 저장하지 않고 껐다(새 세이브 파일 없음). 적재 판정 통과. 백그라운드.

| 한 것 | 본 것 |
|---|---|
| `diplomacy list` | 왕국 24개가 현지화된 이름으로(라크리아, 하라우, 크래스터 …). 크래스터만 "그쪽이 우리를 대립", 나머지는 양쪽 중립 |
| `diplomacy <크래스터> friends side=them` | "크래스터: 그쪽이 우리를 대립 -> 우호 (평판 +5 를 7번)" (−7 + 35 = 28) |
| `diplomacy <크래스터> friends`(양쪽) | "그쪽이 우리를 이미 우호입니다", "우리가 그쪽을 중립 -> 우호 (평판 +5 를 3번)" (10 + 15 = 25). `get_relation_with`가 양쪽 4 |
| `diplomacy <라크리아> hostile side=them` | "중립 -> 철천지원수 (평판 -5 를 9번)". `get_relation_with` 2, `is_enemy_with(pf)` 참 |
| `diplomacy <라크리아> neutral side=them` | "철천지원수 -> 중립 (평판 +5 를 10번)". 3, `is_enemy_with` 거짓 |
| `diplomacy <크래스터> opinion amount=-10 side=us` | "우리가 그쪽을 우호 -> 중립 (평판 -5 를 2번)" |
| `opinion amount=2`, `all hostile`, `all opinion`, 모르는 대상, 없는 uuid, `allies` | 모두 거부(없는 uuid 는 "그 왕국이 없습니다") |
| `diplomacy <소피아> friends queue=1`(창의 단추와 같은 길) | "일 2개를 쌓았습니다". 4초 뒤 소피아가 양쪽 우호. 패널에 결과 줄(`refs/ui/m1-diplomacy2.png`) |
| 여섯 시간을 흘림(게임이 `__update_relations`를 337번 불렀다) | 크래스터(그쪽 우호), 소피아(양쪽 우호) **그대로** |
| `diplomacy all neutral` | "48/48 개가 됐습니다"(우호이던 셋만 −5 를 한 번씩, 나머지는 "이미 중립") |
| `diplomacy all friends side=them queue=1` | "일 24개를 쌓았습니다". 20초 뒤 24개 모두 "그쪽이 우리를 우호" |

- 문턱을 더 좁혔다: 우리가 그쪽을 보는 평판 10 에서 +15(25)에 friends 가 됐다. 앞의 실행에서 23 은 neutrals 였으므로 **friends 는 25 부터**로 보인다(두 쌍에서).
- 로그: 이 실행의 외교 줄은 290줄이었다(호출마다 한 줄, 일마다 한 줄).
- **우호가 되기까지 드는 개수는 왕국마다 달랐다**: `all friends side=them`(모두 "중립"에서 시작)의 로그(`refs/runtime/dip-session1.log`)에서 왕국마다 1개 ~ 15개가 들었다
  (레킨 15, 페로누크 14, 레토스 12. 24개 가운데 13개가 7개 이상). 크래스터의 두 번(−7 → 28 에 7개, 10 → 25 에 3개)은 하나가 5 씩이었으므로,
  다른 왕국들은 처음의 평판이나 문턱, 또는 하나가 움직이는 크기가 다른 것이다(어느 쪽인지는 가리지 못했다). 그래서 패널은 수치를 약속하지 않고 개수와 관계만 적는다.

## 협정을 쓰는 함수 (같은 실행의 마지막 호출)

- 협정의 종류(판정 함수가 `is_has_agreement(세력, 세력, 수)`에 넘기는 수. 다시 기록해서 봤다): `is_declared_peace_with` **4**, `is_declared_trade_agreement_with` **8**,
  `is_declared_defence_alliance_with` **192**.
- **`FactionsAgreementMatrix.set_agreement(pf, X, 4) -> undefined`**(게임이 부르는 것은 기록하지 못했다. 인자 맞춤은 4. 셋만 넘겨 처음 불렀다): 게임이 끝나지 않았고,
  `__matrix.<pf>.<X>`와 `__matrix.<X>.<pf>`가 둘 다 4 가 됐고(**양쪽에 쓴다**), `pf.is_declared_peace_with(X)`와 `X.is_declared_peace_with(pf)`가 4(참)를 돌려줬다.
  방어 동맹의 판정은 거짓 그대로였다. 관계의 종류는 바뀌지 않았다(3 / 4).
- 넷째 인자가 무엇인지(기한?), 이미 든 협정에 다른 협정을 쓰면 더해지는지 바뀌는지, 게임이 그 협정을 어떻게 따르는지는 재지 않았다. 푸는 함수 `reset_agreement`(인자 5)는 부르지 않았다.

## 마지막 확인 실행 (모듈 0.20.1, 2026-10-06)

실행 묶음 `dip-session2`(답: `refs/runtime/dip-session2.answer.txt`). 06:00 ~ 09:20, 저장하지 않고 껐다(새 세이브 파일 없음). 적재 판정 통과. 백그라운드.
검토의 지적을 고친 빌드다: 결과는 개수로 적고("좋은 평판 7개"), 일의 처음에 그 왕국이 망했는지와 양쪽에 왕이 있는지를 묻는다.

| 한 것 | 본 것 |
|---|---|
| `economy add resource=0 amount=5`, 바닥 둘, `cheat resource_floor on` | 반지 7 → 12, 금화 3000, 나무 6000 (경제 0.18.1 의 것이 그대로 된다) |
| `diplomacy <크래스터> friends queue=1` | 쌓은 일 2개. `diplomacy list`의 끝: "2개 가운데 바꾼 것 2개 …", "그쪽이 우리를 대립 -> 우호 (좋은 평판 7개)", "우리가 그쪽을 중립 -> 우호 (좋은 평판 3개)" |
| `diplomacy <라크리아> hostile side=them`, `neutral side=them` | "중립 -> 철천지원수 (나쁜 평판 9개)", "철천지원수 -> 중립 (좋은 평판 10개)" |
| `opinion amount=-2 side=us`, `amount=2 side=us` | "중립 -> 중립 (나쁜 평판 2개)", "(좋은 평판 2개)" |
| `all hostile`, `opinion amount=41`, `amount=2.5`, 없는 uuid | 모두 거부 |
| `diplomacy <크래스터> pact name=peace` 두 번 | "평화 협정을 맺었습니다", "이미 평화 협정이 있습니다". 양쪽의 `is_declared_peace_with`가 4(참). 목록의 협정 칸 "평화 (4)" |
| `pact name=peace queue=1`(소피아), `all pact`, `name=war`, `side=them` | 쌓은 일 1개 → "소피아: 평화 협정을 맺었습니다". 나머지 셋은 거부 |
| 세 시간을 흘림 | 크래스터 양쪽 우호, 두 왕국의 평화 협정 **그대로** |
| `pact name=trade`(8 을 처음 넘겼다) | "교역 협정을 맺었습니다". 칸 12, `is_declared_trade_agreement_with`가 8(참), 평화도 그대로 |
| `pact name=defence`(192 를 처음 넘겼다) | "방어 동맹을 맺었습니다". 칸 204, `is_declared_defence_alliance_with`가 참, 교역·평화도 그대로 |
| 그 뒤 `__update_relations`를 양쪽으로 | 관계의 종류는 우호(4) 그대로다. **방어 동맹을 맺어도 관계의 종류가 allies(0)가 되지는 않는다** |

- `set_agreement`에 지금의 비트를 더한 수(12, 204)를 넘겨도 된다. 넘긴 수가 그대로 칸에 앉았다(더해지는지 바뀌는지는 여전히 가리지 않았다: 언제나 더한 수를 넘긴다).
- 게임이 그 협정을 어떻게 따르는지(침공을 하지 않는가, 방어 동맹의 원군, 교역 협정의 상단, 기한)는 보지 못했다. 세계 지도의 표시로도 보지 못했다(게임 창을 누르지 않는 실행이었다).

## 머지한 빌드의 확인 실행 (모듈 0.20.2, 2026-10-06)

실행 묶음 `dip-session3`(답: `refs/runtime/dip-session3.answer.txt`). 06:00 ~ 08:00, 저장하지 않고 껐다(새 세이브 파일 없음). 적재 판정 통과. 백그라운드.
둘째 검토의 지적을 고친 빌드다(묻지 못한 것은 실패로, 실행 중의 겹침 셈을 뺐다, 평판의 수의 글, 원격의 모르는 열쇠 거부).

| 한 것 | 본 것 |
|---|---|
| 경제: 반지 +5, 바닥 둘, `amount=1e12`, 켜기 | 7 → 12, "최소값으로 쓸 수 없는 수입니다 (10억까지)", 금화 3000·나무 6000 |
| `diplomacy <크래스터> friends queue=1` | 목록의 끝: "2개 가운데 한 것 2개, 그대로 둔 것 0개, 안 된 것 0개"와 두 줄(좋은 평판 7개, 3개) |
| `<라크리아> hostile side=them`, `neutral side=them` | "중립 -> 철천지원수 (나쁜 평판 9개)", "철천지원수 -> 중립 (좋은 평판 10개)" |
| `<라크리아> opinion amount=-2 side=us`, `amount=2` | "우리가 그쪽을 보는 평판에 나쁜 평판 2개를 붙였습니다 (관계는 중립 그대로)", 좋은 평판도 같은 꼴 |
| `hostile sdie=them`, `friends queue=0`, `all hostile`, `amount=41`, 없는 uuid(바로, 쌓기) | 모두 거부("diplomacy takes only side=, amount=, name= and queue=1: sdie" …, "그 왕국이 없습니다") |
| `<크래스터> pact name=peace` 두 번, `<소피아> pact name=defence queue=1`, `<소피아> pact name=trade` | 맺음, "이미 평화 협정이 있습니다", 쌓은 일 → "소피아: 방어 동맹을 맺었습니다", 교역. 소피아의 칸 200("교역, 방어 동맹"), 게임의 판정 셋이 참 |
| 두 시간을 흘림 | 크래스터 양쪽 우호와 평화, 소피아의 협정 그대로. 그동안 게임이 스스로 고르노(대립)와 레토스(적)의 관계를 바꿨다(내가 건드리지 않은 왕국이다) |
| 로그 | "alive check ok: destroyed false, king struct, ours struct"(일마다 한 줄), "pact cell -1 -> 4 (asked 4)", 붙이는 호출 33줄과 다시 셈하는 호출 33줄 |

## 같은 평판은 50개까지만 센다 (같은 실행의 마지막 호출들)

우리 왕이 하라우의 왕에게 갖는 평판에 좋은 디버그 평판을 40개, 이어서 15개 붙였다(`opinion amount=40 side=us`, `amount=15 side=us`). 게임은 끝나지 않았다.

| 때 | `get_opinion`(우리 왕 → 하라우의 왕) | 우리 왕의 `__opinion_minds`의 원소 수 |
|---|---|---|
| 처음 | 10 | 33 |
| 40개 뒤 | 210 (10 + 40 × 5) | 73 (+40) |
| 15개 더(모두 55개) | **260** (10 + 50 × 5) | **83** (+10) |

- **하나가 5 씩이고, 붙일 때마다 `__opinion_minds`에 원소가 하나 는다.** 51번째부터는 함수가 여전히 구조체를 돌려주지만 평판이 오르지 않고 원소도 늘지 않는다(자료의 `__stack_limit` 50).
- 그래서 **평판의 수(`평판+`·`평판-`, 원격 `opinion`)는 한도에서 붙지 않은 것을 "붙였습니다"라고 적는다**(위의 15개 가운데 10개만 붙었는데 "15개를 붙였습니다"). 0.20.2 의 알려진 틀림이다.
  고치는 길은 재어졌다: 붙이기 앞뒤로 그 왕의 `__opinion_minds`의 원소 수를 견준다(상대 왕의 것은 `get_king_character_soul()`이 돌려준 구조체에서 읽어야 한다. 그 구조체의 꼴은 아직 보지 않았다).
- 목표가 있는 일(우호·중립·적대)은 관계를 다시 읽으므로 한도에 걸리면 "한도까지 붙였지만 바라는 관계가 되지 않았습니다"로 끝난다(틀리게 적지 않는다).
- 문턱 하나가 더 재어졌다: 평판 210 과 260 은 friends 다(allies 가 되지 않는다).

