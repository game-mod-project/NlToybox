# Norland Configurable Rules — 로드맵

- 문서 버전: v0.2 (2026-10-04)
- 원본: `Norland_Configurable_Rules_Mod_Plan.md` v0.1. 이 문서는 원본을 Phase 0 실측에 맞게 고쳐 쓴 것이다.
- 대상: Norland `0.5588.9777.0` (Steam buildid `25211575`)
- 근거: `research/00-game-structure.md`, 2026-10-04의 데이터 파일 조사(JSON 4,499개와 exe 문자열 대조)

## 1. 목표 (원본 유지)

Norland의 게임 데이터와 규칙을 외부 설정에서 고칠 수 있게 한다. 단순한 치트가 아니라
"게임 규칙을 사용자가 다시 설계할 수 있게 하는 틀"이 최종 목표다.

원본의 세 수준은 **무엇을** 바꾸는가의 분류로 남긴다.

| 수준 | 내용 |
|---|---|
| Level 1 | 수치: 비용, 생산량, 시간, 능력치, 배율 |
| Level 2 | 규칙: 조건, 요구 기술, 이벤트 조건, 외교·전투·상속 규칙 |
| Level 3 | 확장: 새 건물·생산·이벤트·AI 행동, 다른 모드를 위한 API |

개발 순서는 이 수준이 아니라 **어떤 수단으로** 닿는가로 정한다(§4, §6).

## 2. 실측 전제 (원본 §2를 대체)

원본은 Unity·BepInEx·Harmony·Reflection을 전제로 했다. 실제는 다르다.

| 항목 | 실측 |
|---|---|
| 엔진 | GameMaker, YYC 네이티브 컴파일. Assembly도 바이트코드도 없다. GML 디컴파일은 불가능하다 |
| 로더 | Aurie v2.0.2 + YYToolkit v5.0.0c. exe를 패치해 붙는다. 이 레포의 도구로 설치·복원한다 |
| 모듈 | C++ DLL(`NlToyBox.dll`). 빌트인 호출과 이름으로 스크립트 찾기가 된다 |
| 게임 스레드 진입점 | `EVENT_OBJECT_CALL`. 프레임 콜백(`EVENT_FRAME`)은 오지 않는다 |
| 이름 | 스크립트 이름 40,813개와 변수 이름이 남아 있다. 리플렉션은 없다 |
| 데이터 파일 | JSON 4,499개와 CSV가 게임 폴더에 그대로 있다. exe가 파일 이름과 키 이름을 직접 참조한다 |
| 내장 디버그 UI | ImGui 확장과 `imgui_debug_*` 창, `Variables Editor`가 릴리스 빌드에 있다. 켜는 방법은 모른다 |
| 공식 모드 지원 | 없다 |

## 3. 원본에서 바뀐 것

| 원본 | 처리 |
|---|---|
| §2 중요 전제 (Unity, BepInEx, Harmony) | §2로 대체 |
| §3 모드 구조 (`Core/`, `Patches/*.cs`, `UI/`, `API/`) | 폐기. 구조는 하위 프로젝트마다 스펙에서 정한다 |
| §4~§18 기능 모듈 16개 | 유지하되 §5의 도달 가능성 표로 정리 |
| §19 Preset | 유지. 하위 프로젝트 1이 구현한다 |
| §20 UI 설계 | 하위 프로젝트 3으로 미룬다. 프레임 훅이 없어 방법부터 정해야 한다 |
| §21 설정 파일 구조 (모듈별 `config/*.json`) | 프리셋 + 카탈로그로 바꾼다. 예시의 식별자는 실제 이름으로 고친다 |
| §22 Patch 구조 (`*.cs`) | 폐기. 코드 훅은 하위 프로젝트 4에서 스크립트 이름 단위로 다시 설계한다 |
| §23 Data Registry (Reflection) | 두 가지로 나눈다: 파일의 키 목록(카탈로그)과 런타임 전역 변수 덤프 |
| §24 버전 호환성 | 유지하고 구체화: exe 버전·해시, 데이터 파일 스냅샷 대조 |
| §25 Save 안전성 | 전제 수정. 파일 오버레이는 세이브를 건드리지 않는다. 바뀐 값이 세이브에 굳는지를 확인 대상으로 둔다 |
| §26 Phase 0·1 | 완료 (`docs/superpowers/specs/2026-10-04-nltoybox-phase0-workspace-design.md`) |
| §27 우선순위, §28 MVP | §7로 다시 배치 |
| §30 위험 | §8로 갱신 |

## 4. 수단

값이나 규칙에 닿는 길은 네 가지다. 아래로 갈수록 어렵고 게임 갱신에 약하다.

| 수단 | 무엇을 하나 | 필요한 것 | 닿는 범위 |
|---|---|---|---|
| 파일 오버레이 | 게임 폴더의 JSON·CSV 값을 고친다 | 없음 (exe 패치도 필요 없다) | 파일에 있는 수치 |
| 런타임 접근 | 모듈이 메모리의 변수를 읽고 쓴다 | 패치된 exe, 변수의 위치 | exe 안에만 있는 수치 |
| 스크립트 훅 | 게임 스크립트의 호출을 가로채 인자·반환값을 바꾼다 | 스크립트의 역할을 이름과 동작으로 알아내야 한다 | 규칙 (Level 2) |
| 편집 UI | 게임 안에서 값을 보고 고친다 | 그릴 자리 (내장 디버그 창 또는 직접 건 프레임 훅) | 위 세 가지의 조작면 |

## 5. 모듈별 도달 가능성

"파일"은 관련 키가 데이터 파일에 있다는 뜻이다. **게임이 그 값을 실제로 쓰는지는 하위 프로젝트 1의
단계 0에서 확인한다.** `gameplay_variables.json`의 범주는 최상위 키 이름 기준이고, 범주 안의 내용은
일부만 조사했다.

| 원본 모듈 | 파일에 있는 것 | exe 안에만 있는 것 |
|---|---|---|
| Character | `gameplay_variables.json`: `skill.increase_in_use.*`, `actor.*`, `trait`, `game.death_chance_mult` | 기본 능력치 범위(`actor_skills_total_level_min/max`), 노화(`ComponentAging`, `__age_threshold`) |
| Lord | `gameplay_variables.json`: `free_lord`, `genius_king`, `loyalty`, `prestige` 범주 | 개별 Lord의 상태(런타임 인스턴스) |
| Family / Succession | `gameplay_variables.json`: `pregnancy` 범주 | 상속 규칙 (코드) |
| Population | `gameplay_variables.json`: `migration`, `slave` 범주. `debug_params.json`: `fair_trade.migrants_*` | 인구 증감 규칙 (코드) |
| Buildings | `debug_params.json`: `building_resources`(101개 키), `building_duration_factor`. `building_constructor\buildings\*.json`: `number_of_living_places`, `repair_cost_per_level` | 노동자 수(`__number_of_workers_*`), 생산량 |
| Resources | `debug_params.json`: `product_count`, `fair_trade.fair_trade_purchase/sale`. 자원 39종 | 자원 정의 |
| Production | `debug_params.json`: `production_cost`. `gameplay_variables.json`: `farm.<작물>.duration`, `grow_cost` | 투입→산출 레시피(`production_resources`), 수율(`mill_rye_flour_yield`) |
| Economy | `debug_params.json`: `fair_trade`, `budget_money`. `gameplay_variables.json`: `bribe`, `tavern`, `knowledge.price_*` | 가격 결정 규칙 (코드) |
| Knowledge | `knowledge\technology\**\*.json` 121개: 요구 인구, 선행 지식, 해금 대상, 효과 | 금화 비용(`knowledge_price_*`) |
| Warfare | `battle_params.json`: `attack`, `defence`, `pain`, `limit`, `battle_dodge_base`. `gameplay_variables.json`: `squad`, `soldier`, `combat_training`, `moral`, `mind`, `battle` | 전투 규칙 (코드) |
| Diplomacy | `gameplay_variables.json`: `global_map`(예: `ai_economy.initial_budget`), `bandits` | 외교 행동·관계 규칙 (코드) |
| Religion | `gameplay_variables.json`: `church` 범주. `knowledge\technology\cultural_knowledge\*.json` | 종교 규칙 (코드) |
| Events | `director_params.json`: 이벤트별 `tickets`, `cooldown_days`, `min/max_population`, 그룹 쿨다운 | 이벤트의 조건·효과 (코드) |
| AI | 없음 | 전부 (코드) |
| World | `building_constructor\NewWorld.json`, `NewWorldParams.csv`, `generator\genmap\config.json` | 시작 조건 일부 |
| Time | `NewWorld.json`: `normal_season_days`, `extreme_season_days` | 게임 속도(`game_speed_slower_div`, `gameplay_hour_frames`, `__time_scale`) |

`gameplay_variables.json`의 값 150개 가운데 34개는 키 이름이 exe 문자열에 없다(`prestige.*`,
`migration.*`, `loyalty.*` 등). 지금은 쓰이지 않는 낡은 키일 수 있다.

exe에는 `gameplay_variables.json`의 키와 같은 꼴의 이름이 파일보다 훨씬 많다. 파일이 코드 기본값 위의
덮어쓰기 층이고 파일에 키를 추가하면 읽힌다면, 파일 오버레이가 닿는 범위가 크게 넓어진다.
**이것은 가설이다.** 단계 0에서 확인한다.

## 6. 하위 프로젝트

각각 스펙 → 계획 → 구현의 한 바퀴를 따로 돈다.

### 1. 데이터 오버레이

- 내용: 프리셋을 바닐라 파일 위에 입히고 되돌린다. 카탈로그로 지원 키를 관리한다.
- 먼저 할 실측(단계 0): 파일 값이 게임에 반영되는가, 런타임 전역 변수는 어떻게 생겼는가, 파일에 없는 키를 넣으면 읽히는가.
- 스펙: `docs/superpowers/specs/2026-10-04-data-overlay-design.md`
- 단계 1(오버레이 도구)의 계획: `docs/superpowers/plans/2026-10-05-data-overlay-stage1.md`. 설계가 기대는 실측과,
  2026-10-05에 받은 참조 문서(`Norland_Modding_Reference.md`)를 설치본과 맞춰 본 결과는 `research/03-data-files.md`.
- 닿는 것: Level 1 가운데 파일에 있는 수치, 이벤트 빈도.

### 2. 런타임 접근

- 내용: 모듈이 설정에 따라 exe 안의 변수를 쓴다(게임 속도, 노화, 노동자 수 등).
- 시작 조건: 단계 0의 전역 변수 덤프가 있다.
- 위험: 변수마다 쓰는 시점과 부작용이 다르다. 하나씩 실험해야 한다.

### 3. 편집 UI

- 내용: 게임 안에서 값을 보고 고치는 창.
- 먼저 정할 것: 내장 디버그 창을 켤 수 있는가(`is_debug_forced`, `command_line_parameters_init`),
  아니면 Present 훅을 직접 걸고 ImGui를 올리는가.
- 단서(확인하지 않았다): Steam 토론 "Any way to edit stats (cheat/hack)?"의 3월 14일 글에 `-debug`로 켜고
  인물을 누른 뒤 Ctrl+D를 누르면 디버그 창이 열린다고 적혀 있다. exe 문자열에서 `-debug`는 GameMaker 러너의
  옵션 목록(`-trace`, `-noaudio` 등) 사이에 있고, 러너의 `parameter_count`·`parameter_string`도 있다.
  Norland가 그것으로 디버그 창을 켜는지는 재 보지 않았다. 이 하위 프로젝트의 첫 실측 대상이다.
- 시작 조건: 하위 프로젝트 1 또는 2가 조작할 대상을 제공한다.

### 4. 규칙과 확장

- 내용: 스크립트 훅으로 규칙을 바꾸고(Level 2), 새 콘텐츠와 Mod API를 다룬다(Level 3).
- 시작 조건: 앞의 셋이 서 있고, 스크립트 한두 개를 훅으로 바꾸는 실험이 성공했다.
- 지금은 설계하지 않는다. 대상 스크립트의 동작을 모르는 상태에서 쓰는 설계는 추측이다.

## 7. MVP 다시 배치

원본 §28의 열 항목을 수단별로 나누면 다음과 같다.

| # | 항목 | 하위 프로젝트 | 근거 |
|---|---|---|---|
| 1 | Gold | 1 (미정) | `budget_money`. 실측: 런타임의 `default_budget_money`가 되지만 화면의 시작 금화는 그 값이 아니었다(`research/02-new-game-state.md`). 시작 금화의 출처를 찾은 뒤에 정한다 |
| 2 | Resources | 1 (시작량·거래가) | `product_count`, `fair_trade` |
| 3 | Production | 1 (생산 비용, 작물 시간) / 2 (레시피) | `production_cost`, `farm.*` |
| 4 | Building Cost | 1 | `building_resources`, `building_duration_factor` |
| 5 | Building Production | 2 | 파일에 없음 |
| 6 | Knowledge Requirement | 1 | `knowledge\technology\*.json` |
| 7 | Character Skills | 1 (성장 속도) / 2 (기본값) | `skill.increase_in_use.*` |
| 8 | Character Aging | 2 | 파일에는 `death_chance_mult` 정도 |
| 9 | Combat | 1 | `battle_params.json` |
| 10 | Game Speed | 2 (가설이 맞으면 1) | 파일에 없음 |

원본의 MVP 성공 기준(모드 로딩 → 설정 읽기 → 값 적용 → 새 게임에서 확인 → 세이브·로드에 문제 없음)은
하위 프로젝트 1의 완료 기준으로 옮긴다.

참조 문서(2026-10-05)는 MVP를 파일 단위로 다시 묶는다: `gameplay_variables.json` → `debug_params.json` →
`knowledge\technology\` → 전투 → 맵. 단계 1의 카탈로그는 앞의 넷을 다룬다. 실측으로 바로잡은 것: 전투 수치는 문서가 적은
`battle_settings\`가 아니라 `battle_params.json`에 있고, 문서의 `maps\map_N.json`은 이 빌드에 없다(`.map_template`은 범위 밖이다).
문서가 첫 순위로 든 `bribe`는 다섯 값 가운데 `give_rings`만 런타임(`global.__gameplay_vars`)에 이름이 있다(`research/03-data-files.md`).

## 8. 위험

| 위험 | 대응 |
|---|---|
| 파일의 값을 게임이 쓰지 않는다 (코드 기본값이 따로 있다) | 단계 0에서 먼저 잰다. 반영되는 키만 카탈로그에 올린다 |
| 게임 갱신으로 키·파일이 바뀐다 | 스냅샷과 카탈로그를 게임 버전별로 둔다. 구조가 달라지면 적용을 거부한다 |
| Steam 갱신·무결성 검사가 오버레이를 지운다 | 상태 점검이 알려 준다. 다시 적용한다 |
| 비표준 JSON (trailing comma, 뒤 공백 키) | 파싱해 다시 쓰지 않는다. 값의 자리만 고친다 |
| YYToolkit v5.0.0c가 베타다 (프레임 훅 없음) | 하위 프로젝트 1은 모듈 없이도 동작한다. 모듈은 측정에만 쓴다 |
| 규칙이 코드에 있다 (Level 2·3) | 하위 프로젝트 4로 미룬다. 스크립트 훅 실험이 먼저다 |
| 바뀐 값이 세이브에 굳는다 | 단계 0에서 본다. 굳는 키는 카탈로그에 표시한다 |
| AGPL-3.0 (Aurie, YYToolkit) | 모듈을 배포하거나 레포를 공개하기 전에 검토한다. 파일 오버레이 도구는 무관하다 |

## 9. 원본에서 유지하는 원칙

- UI보다 설정 파일이 먼저다. 값이 안정적으로 바뀌는 것을 확인한 뒤 UI를 만든다.
- 프리셋으로 설정 묶음을 바꾼다.
- 게임 버전을 확인하고, 맞지 않으면 적용하지 않는다.
- 원본을 덮어쓰기 전에 백업한다. 언제든 바닐라로 되돌릴 수 있어야 한다.
- AI와 이벤트 편집은 후순위다.

## 10. 미결

- 프로젝트 이름. 원본은 "Norland Configurable Rules"와 "NCore"를 쓰고 레포는 `NlToyBox`다. 지금은 레포 이름을 그대로 쓴다.
- 프리셋의 배포 형태(이 레포에 둘지, 따로 나눌지).
- 하위 프로젝트 2와 3의 순서. 단계 0의 결과와 내장 디버그 창 조사 결과를 보고 정한다.
