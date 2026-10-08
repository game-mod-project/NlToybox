# 33. 도서관의 책: 넣기와 빼기 (2026-10-08)

질문: 게임의 도서관에 책을 모두 넣을 수 있는가. 넣은 것을 뺄 수 있는가.
방법: 켜기 3번(아덴 5일차 아침 세이브 `아덴_Autosave_Morning_day_5`). 첫 번(0.31.2)은 재기(읽기·`record`·시험 호출), 둘째(0.31.3)는 구현의 확인, 셋째는 그 뒤에 고친 빌드(`7466255`)의 적재 판정과 느낌표 이름의 확인.
답은 `refs/runtime/library-run1.*`·`library-run2.*`·`library-run3.*`, 화면은 `refs/ui/library-add-test.png`·`library-all.png`·`library-ui-1.png` ~ `library-ui-7.png`·`library-final.png`(추적 안 함).
세 실행 모두 모듈 로그에 오류 줄이 없고, 불러온 뒤 게임의 오류 파일에 새 줄이 없고, 새 세이브 파일이 생기지 않았고, 게임은 정상 종료했다. 둘째와 셋째 실행의 로그는 적재 판정(`Get-NlLoadFailures`)을 통과했다.

## 자리

- 도서관 관리자: `inst:o_game_map_controller.__library_manager`(`LibraryManager`). 칸은 열둘이다:
  `__books`(ds_map 의 번호. 두 실행 모두 346), `__knowledge_interactions`(ds_map 의 번호 347: 구조체 → 구조체), `__auto_set`(수 0), `__auto_study_map`(영주의 uuid → 수),
  `__knowledge_interaction_stash`, `__last_time_when_lord_interact`(영주의 uuid → 게임 시각), `__resources_unlock_by_knowledge`, 그리고 pub_sub 의 칸 다섯.
- **`__books` 는 지식 구조체 → 권수다**(`list map:346`: `struct  number  1`). 열쇠인 구조체는 게임의 지식 목록 `inst:o_data.__knowledge_data.__knowledge_list[n]`(121개)의 것과 같다(주소로 맞춰 봤다).
- 지식의 갈래(`__category`): `cultural_knowledge` 52, `economic` 41, `textbooks` 28. 이름(`__name`)은 소문자·숫자·밑줄이고 둘에는 느낌표가 있다
  (`41!building_wooden_wall_section`, `42!building_fortification_tower`).
- 세이브 파일: 도서관의 자료 안에 `"books":[{"count":1.0,"knowledge_name":"tech_clay"}, …]` 가 있다(그 세이브에는 여덟 권. 옆에 `interactions`, `stash`, `last_time_when_lord_interact`).
  불러온 직후 게임의 `get_books_count()` 가 8 을 답했다. `"books"` 라는 열쇠는 다른 데에도 있다(사람의 자료 안에 빈 배열 339곳, 세계 지도의 마을로 보이는 것 25곳에 다른 꼴의 구조체).

## 게임의 함수

`statics` 로 본 `LibraryManager` 의 메서드는 48개다. 꼴을 본 것만 적는다(스크립트 이름의 끝 번호는 이 빌드의 것이다).

| 함수 | 꼴 | 본 방법 |
|---|---|---|
| `change_books` (…3778020429) | `(지식 구조체, 1) -> undefined` | 게임의 호출(책 사본이 만들어질 때 한 번). 본문이 `argc` 를 읽는다 |
| `is_have_book` (…3998220436) | `(지식 구조체) -> 불리언` | 게임의 호출(하루 반에 72번) |
| `get_array_of_books` (…3878820431) | `() -> 배열` | 게임의 호출(4번). 인자 없음(기계어) |
| `get_books_count` (…3952520434) | `() -> 수` | 인자 없음(기계어). 불러서 수를 받았다: **책의 종류**를 센다(권수의 합이 아니다) |
| `get_books_map` (…3942820433), `get_books_list` (…3962520435) | 인자 없음(기계어) | 부르지 않았다 |
| `is_has_knowledge_and_available_lord` (…4019120437) | `() -> false` | 게임의 호출(23번). 불러 봐도 책이 9종일 때와 121종일 때 모두 거짓이었다 |
| `reset_interaction` (…2318420409) | `(지식 구조체) -> undefined` | 게임의 호출(석궁의 구조체로 한 번. 그 18시간에 석궁의 사본이 한 권 생겼고 `__knowledge_interactions` 의 칸이 1 → 0 이 됐다) |
| `set_interaction`, `__continue_autostudy`, `autoset_switch`, `get_learn_time` … | 모른다 | 하루 동안 한 번도 불리지 않았다. 부르지 않는다 |

- 영주의 지식 구성요소 `ComponentKnowledge.add_knowledge(지식 구조체) -> 불리언`: 게임이 **인자 하나로** 불렀다(통찰로 얻은 둘에 `true`, 석궁에 `false` 하나: 누구의 것인지는 보지 않았다).
  `research/12` 의 `(지식 구조체, true, true)` 는 연구가 끝날 때의 꼴이다. `get_knowledge_count()`(인자 없음)는 수를 준다.

## 직접 불러 본 것 (첫 실행)

- `change_books(K, 1)`: `is_have_book(K)` 가 참이 되고 `get_books_count()` 가 9 → 10. 한 번 더: 수는 10 그대로(그 책이 2권이 된다). `change_books(K, -1)`: 여전히 참. 또 `-1`: 거짓, 9(칸이 없어진다).
- 교과서(`skill_oratory_3`)와 문화(`flirt_art`)의 책도 들어간다. 그 세이브의 책 여덟은 모두 `economic` 이었다.
- 없는 110권을 한 요청으로 넣었다: 121 / 121, 게임의 오류 없음. 게임의 "새로 제작된 책 사본" 알림은 뜨지 않았다(그 알림은 영주가 사본을 다 썼을 때 게임이 낸다: 석궁의 것을 봤다).

## 구현과 확인 (둘째 실행. 0.31.3)

지식 탭의 둘째 탭 "도서관의 책"(`src/Library.cpp`, `core/LibraryPlan`)과 원격 `library list [find=] | add name= | add_all | remove name= | undo`.

- 읽기: `__books` 의 번호가 ds_map 인지와 크기를 본 뒤(빈 도서관과 읽지 못한 것을 가른다) 열쇠의 배열을 얻고, 열쇠마다 그 구조체의 `__name` 과 `ds_map_find_value(번호, 열쇠)` 의 권수를 읽는다.
  표의 종류 수와 게임의 `get_books_count()` 가 실행 내내 같았다(8, 9, 121).
- 넣기·빼기는 `change_books(지식 구조체, 1 | -1)` 한 번이다. 지식 구조체는 게임의 목록의 그 자리에서 얻고 그 자리의 이름을 다시 본다. **판정은 권수의 앞뒤다**(함수는 `undefined` 를 준다): 정확히 1 만큼 달라졌을 때만 됐다고 적는다.
- 원격: `library list` 8종(게임이 센 수 8). `add name=skill_oratory_3` → 9종, 다시 `add` → "이미 도서관에 있는 책", `remove` → 8종. 느낌표가 든 이름도 넣고 뺐다. 없는 이름과 도서관에 없는 책의 빼기는 거절한다.
  `add_all` → "책 113권을 도서관에 넣었습니다", 121종(게임 121, `get_array_of_books()` 의 길이 121, `is_have_book(목록의 0번)` 참). 다시 `add_all` → 넣을 책이 없다. `undo` → 113권을 빼고 8종(`is_have_book` 거짓).
- 창: 탭 "도서관의 책", 줄의 "넣기"(그 줄이 "빼기 1"이 되고 요약이 9종, 되돌리기 단추가 "(1권)"), "빼기", 찾기 칸에 `mine` 을 쳐 넣어 두 줄(토지 지식, 광산), "모든 책을 도서관에 넣기"(121종, 되돌리기 "(113권)"),
  "이 실행에서 넣은 책 빼기"를 눌러 확인했다.
- **되돌리기는 모듈이 넣은 것만 뺀다.** 모듈이 책을 모두 넣어 둔 채 시간을 18시간 흘리는 동안 게임이 석궁의 사본을 스스로 만들었다(영주가 쓰던 것이 끝났다): 요약이 "121종 (122권)". 되돌리기는 113권을 빼고 "9종 (9권)"을 남겼다
  (원래의 여덟과 게임이 만든 석궁 한 권). 모듈의 기억은 그 자리(도서관 관리자 구조체의 주소와 지도 관리 인스턴스. `core/PlaceKey`)의 것이고 파일에 남기지 않는다.

## 보지 못한 것

- **넣은 책으로 영주가 배우는 것.** 게임의 도서관 창에서 학습을 정하는 일(`set_interaction` 일 것이다. 꼴을 모른다)을 대신 하지 못했다. 책을 모두 넣고 영주 둘의 `__auto_study_map.<uuid>` 를 1 로 쓴 채 18시간을 흘렸다:
  `set_interaction` 과 `__continue_autostudy` 는 한 번도 불리지 않았다. 그사이 영주 하나의 지식이 2 → 4 가 됐는데 인물 기록의 틀이 `character_log.got_knowledge_from_insight` 였다(책이 아니라 통찰. 책으로 배운 기록의 틀은 `got_knowledge_from_book` 이다: 그 세이브의 2일차와 3일차에 있다).
  그 깃발의 뜻도 그래서 모른다(이름에서 읽은 것이다).
- 넣은 책이 세이브에 남는 것. 저장해 보지 않았다(실행 묶음은 게임의 저장을 끈다). 세이브의 `books` 가 그 표를 담는다는 것까지 봤다.
- 인물 기록의 자리: `inst:o_character:<n>.__soul.__character_soul.__logs_new.__logs_per_years[해][줄]` = `{ __day, __template, __vars[{ __var_name, __knowledge_name }] }`. 지식을 어떻게 얻었는지를 여기서 가린다.

## 느낌표가 든 지식 이름 (고침)

영주에게 지식 하나를 주는 일(`person <uuid> knowledge_add name=…`, 지식 탭의 "이름의 일부로 찾아 하나 주기")이 이름을 특성의 이름 검사(소문자·숫자·밑줄)로 봤다.
그래서 느낌표가 든 둘(`41!building_wooden_wall_section` 울타리, `42!building_fortification_tower` 방어탑)은 "지식 이름이 아닙니다"로 거절됐다("모든 지식 주기"는 게임의 `add_all_knowledge()` 라 그 둘도 줬다).
지식 주기는 지식의 이름 검사(`GoodKnowledgeName`: 느낌표도 받는다)를 쓰게 고쳤다. 셋째 실행: 지식 4개인 영주에게 `knowledge_add name=41!building_wooden_wall_section` → `get_knowledge_count()` 4 → 5,
다시 주면 "이미 가진 지식입니다". 특성의 이름은 여전히 느낌표를 받지 않는다(`trait_add name=41!brave` 는 읽기에서 거절).

## 시험용 누름과 탭

원격 `ui click` 으로 탭(`BeginTabItem`)을 한 번 누르면 넘어가지 않는다. Dear ImGui 의 탭은 `ImGuiButtonFlags_AllowOverlap` 항목이라 **앞 프레임에 이미 마우스가 올라와 있어야** 눌린다
(`ItemHoverable`: `g.HoveredIdPreviousFrame != id` 이면 거짓. `external/imgui/imgui_widgets.cpp` 의 `TabItemEx`). 시험용 누름은 옮김과 누름을 한 프레임에 넣으므로 첫 누름은 올려놓기만 한다.
같은 자리를 두 번 누른다. 보통의 단추와 입력 칸은 한 번이면 된다.
