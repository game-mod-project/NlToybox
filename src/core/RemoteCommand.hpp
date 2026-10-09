#pragma once
// 켜져 있는 게임에 묻는 파일(NlToyBox.ask.txt)의 한 줄. 러너에 기대지 않는다. 스펙: 치트 메뉴 §14.
//   ask <주소>                                      값 하나
//   about <주소>                                    값 하나. 메서드이면 묶인 스크립트의 이름과 묶인 곳이 있는지도
//   list <주소> [as=map|list] [max=N]               그릇의 자식들
//   tree <주소> [depth=N] [max=N]                   자식들을 깊이 N 까지
//   find [name=글] [value=수] [in=global,inst,ds]   이름·값으로 찾기
//   refine value=수                                 앞의 find 결과 가운데 지금 값이 그 수인 것만
//   write <주소>=<수>                               써 넣는다(남긴다)
//   poke <주소>=<수>                                써 넣고, 읽고, 되돌린다
//   record <스크립트 이름|메서드의 주소>            그 함수의 호출을 기록한다
//   unrecord <스크립트 이름|all>,  records [스크립트 이름]
//   statics <주소> [max=N]                          구조체를 만든 생성자의 이름과, 정적 메서드들의 이름(부모 생성자의 것까지). list 에는 나오지 않는다
//   override <스크립트 이름|메서드의 주소> <n:수|b:0|1|u> [skip]
//                                                   그 함수가 돌려주는 값을 바꾼다(훅). skip 이면 원래 함수를 부르지 않는다
//   unoverride <스크립트 이름|all>                  바꾸기를 그만둔다(훅은 남고 원래대로 지나간다)
//   call <스크립트 이름> [인자…]                    게임 스크립트를 부른다(이름에 gml_Script_ 가 없으면 붙인다)
//   method <메서드의 주소> [인자…]                  메서드를 부른다. 묶인 곳이 없으면 주소의 부모에 묶어 부른다
//   treecall <스크립트 이름> [depth=N] [max=N]      인자 없는 스크립트를 부르고 돌려준 값의 자식들을 늘어놓는다(인자가 없다는 것을 본 스크립트에만)
//   economy <gold_add|gold_set|all> amount=<수>     금화를 더한다·맞춘다, 모든 자원을 더한다(경제 패널과 같은 길)
//   economy <add|set> resource=<번호> amount=<수>   자원 하나를 더한다·맞춘다(신성 반지는 0번)
//   economy floor resource=<번호> amount=<수>       그 자원의 최소값을 정한다(0 이면 지운다). gold_floor amount=<수> 는 금화의 것
//                                                   채우는 것은 cheat resource_floor on 일 때다(research/18)
//   person list [all=1],  person show <uuid>        사람들의 목록, 한 사람의 값(인물 패널과 같은 것)
//   person <uuid|lords|people> <할 일> [index=N] [amount=N] [name=글]
//                                                   인물 패널과 같은 길로 고친다. 할 일: skill_set, skill_add, skills_max, need_set, needs_fill,
//                                                   age_set, happy, cure, trait_add, trait_remove, knowledge_all, knowledge_add,
//                                                   money_add, item_add, equip name=<장비 묶음> (core/PeoplePlan)
//   person <uuid> pregnancy_next | birth | grow_up,  person lords birth,  person <uuid> conceive name=<아버지의 uuid>
//                                                   임신의 다음 단계, 출산까지, 아이를 어른으로(나이 18), 임신 시작(core/FamilyPlan. research/24)
//   person <uuid> role name=<king|steward|scholar|instructor|general|duelist|politician|schemer|socialite|priest|trader|producer|teacher>
//                                                   역할 프리셋을 입힌다(한 사람을 짚어서만): 능력치를 올리고(내리지 않는다) 해로운 특성을 떼고
//                                                   재능을 붙인다(core/RolePlan). 답은 한 것과 하지 못한 것의 수다
//   person spawn_soldier amount=<1..20>             플레이어의 병사를 만든다(게임의 디버그 함수. research/13)
//   person spawn <soldier|knight|peasant|slave|lord>  디버그 소환기로 플레이어의 사람 하나를 마우스 자리에 만든다
//   person spawn lord [gender=male|female] [age=<18..80>] [culture=<이름>] [role=<Id>]
//                                                   영주는 성별·나이·문화·역할 프리셋을 정해 만들 수 있다(research/35). 정하지 않은 것은 게임에 맡긴다
//   person <uuid> culture_set name=<문화의 이름>    플레이어의 영주의 문화를 바꾼다(게임의 set_culture)
//   preset <normal|easy|sandbox|god>                치트 표의 확인된 항목의 묶음을 건다(모드창의 프리셋과 같다)
//   time <pause|resume>                             게임의 시간을 멈춘다, 다시 흐르게 한다
//   world <cooldowns_clear|bishop>                  이벤트 쿨다운을 0 으로 쓴다, 주교를 부른다(research/14)
//   world events [group=<묶음>] [find=<글>],  world event_cancel,  world event_end kind=<raid|prophecy|conspiracy|guest|unrest>
//                                                   이벤트의 표, 예약 취소, 확인된 가족의 끝내기(research/29)
//   world event name=<이름>,  world event_now name=<이름>
//                                                   그 이벤트를 예약한다(감독이 뽑는 날에 온다), 바로 일으킨다(게임의 조건 함수가 참일 때만. research/32)
//   map show | set <열쇠>=<수> … | regenerate | restore | preset save|load|delete name=<이름> | seed <수|random>
//                                                   지도 탭: 생성기 화면에서 영지의 생성 설정 17개를 읽고 쓰고 다시 생성한다(core/MapPlan. research/31)
//   library list [find=<글>]                        도서관의 책(요약과 있는 책들. find 를 주면 그 글이 든 지식 모두. research/33)
//   library add name=<지식>,  library add_all,  library remove name=<지식>,  library undo
//                                                   책 한 권을 넣는다, 없는 책을 모두 넣는다, 한 권을 뺀다, 이 실행에서 모듈이 넣은 책을 뺀다(게임의 change_books)
//   diplomacy list                                  왕국들과 지금의 관계(그쪽이 우리를, 우리가 그쪽을)
//   diplomacy <uuid|all> <friends|neutral|hostile> [side=them|us|both]   그 관계가 될 때까지 왕의 평판에 게임의 디버그 평판을 하나씩 붙인다(research/19)
//   diplomacy <uuid> opinion amount=<개수> [side=…]  디버그 평판을 그 개수만큼 붙인다(양수는 좋은 것, 음수는 나쁜 것. -40 ~ 40)
//   diplomacy <uuid> pact name=<peace|trade|defence>  그 왕국과 협정을 맺는다(게임의 협정 함수. 양쪽에 쓰인다)
//                                                   queue=1 을 붙이면 창의 단추처럼 쌓기만 한다(틱이 조금씩 한다. 결과와 실패는 diplomacy list 의 끝에 나온다)
//   court list                                      플레이어의 영주들: 왕에 대한 충성, 따르는 사람의 수, 서로를 보는 평판(research/20)
//   court <uuid|lords> loyal [goal=<수>]            왕을 보는 평판을 목표(기본 100)까지 올린다(게임의 디버그 평판을 하나씩 붙인다. 나쁜 것이 붙어 있으면 그것부터 뗀다)
//   court <uuid|lords> like about=<uuid|lords|king> [goal=<수>]   그 사람을 보는 평판을 목표까지 올린다
//   court <uuid|lords> opinion about=<…> amount=<개수>            디버그 평판을 그 개수만큼 움직인다(양수는 올린다, 음수는 내린다. -40 ~ 40)
//   court <uuid|lords> clear about=<…>              붙여 둔 디버그 평판을 모두 뗀다
//   court <uuid|lords> release                      그 영주를 따르는 사람들의 충성 대상을 지운다(queue=1 은 diplomacy 와 같다)
//   court <uuid> opinion … 은 한 짝씩만 받는다(여럿을 한꺼번에 내리지 않는다). loyal 은 게임이 충성을 따지는 영주에게만 간다
//   court bishop <like|opinion|clear> about=<uuid|lords|king> …   주교가 그 사람을 보는 평판(주교는 lords 에 들지 않고, 대상으로는 삼지 않는다. research/21)
//   traits [find=<글>] [max=<수>]                   게임의 특성들: 이름, 화면 이름, 설명의 앞부분(모듈이 게임의 현지화 파일에서 읽은 것). 인물 패널의 찾기 칸도 그 글이 된다
//                                                   find 는 이름, 화면 이름, 설명의 글 어디에든 들어 있으면 맞는다
//   page <영역의 키>                                모드창의 영역을 고른다(explorer, economy, build, …)
//   ui click x=<수> y=<수>                          모드창에 마우스 누름을 넣는다(Dear ImGui 의 입력 큐에. 진짜 마우스는 건드리지 않는다). 자리는 shot 으로 본다
//   ui type text=<글>,  ui key name=<enter|tab|escape|backspace>
//                                                   잡혀 있는 입력 칸에 글자를 넣는다, 키를 눌렀다 뗀다(창의 입력 칸을 시험하려고 둔다)
//   cheat <치트의 Id> on|off                        치트 표의 항목을 켜고 끈다(모드창의 체크와 같다)
//   state,  shot <이름>,  window open|close
// 인자: n:<수>  s:<글> 또는 s:{공백이 든 글}  b:0|1  u(undefined)  p:<주소>(그 주소의 값)

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	struct RemoteArg
	{
		char Kind = 0;			// 'n' 수, 's' 글, 'b' 불리언, 'u' undefined, 'p' 주소의 값
		double Number = 0;
		std::string Text;
	};

	struct RemoteCommand
	{
		std::string Verb;								// 비어 있으면 빈 줄이나 주석이다
		std::string Target;								// 주소, 스크립트 이름, 화면 이름, open|close
		double Number = 0;								// write, poke, refine 의 수
		std::map<std::string, std::string> Options;		// key=value 들
		std::vector<RemoteArg> Args;					// call, method 의 인자
		std::string Error;								// 비어 있지 않으면 읽지 못했다
	};

	RemoteCommand ParseRemoteLine(const std::string& Line);

	// 옵션의 수. 없거나 수가 아니면 Fallback.
	double OptionNumber(const RemoteCommand& Command, const std::string& Key, double Fallback);

	// 묻는 파일을 집는다: Ask 를 Taken 으로 이름을 바꾼 뒤(됐을 때만) 줄들을 읽고 Taken 을 지운다.
	// 이름을 바꾸지 못하면(없다, 다른 프로그램이 잡고 있다) 거짓이고 줄을 주지 않는다. 읽고 나서 지우지 못해 같은 요청을
	// 두 번 실행하는 일이 없게 한다(요청은 게임의 함수를 부를 수 있다). 남아 있던 Taken 은 실행 도중 끊긴 요청이므로 버린다.
	bool TakeRemoteRequest(const std::filesystem::path& Ask, const std::filesystem::path& Taken, std::vector<std::string>& Lines);

	// 모듈이 뜰 때 남아 있던 요청을 버린다(죽은 도구가 남긴 호출이 다음 실행에서 불리지 않게).
	void DropStaleRemoteRequest(const std::filesystem::path& Ask, const std::filesystem::path& Taken);
}
