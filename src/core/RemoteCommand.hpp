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
//   economy <add|set> resource=<번호> amount=<수>   자원 하나를 더한다·맞춘다
//   person list [all=1],  person show <uuid>        사람들의 목록, 한 사람의 값(인물 패널과 같은 것)
//   person <uuid|lords|people> <할 일> [index=N] [amount=N] [name=글]
//                                                   인물 패널과 같은 길로 고친다. 할 일: skill_set, skill_add, skills_max, need_set, needs_fill,
//                                                   age_set, happy, cure, trait_add, trait_remove, knowledge_all, knowledge_add,
//                                                   money_add, item_add, equip name=<장비 묶음> (core/PeoplePlan)
//   person spawn_soldier amount=<1..20>             플레이어의 병사를 만든다(게임의 디버그 함수. research/13)
//   person spawn <soldier|knight|peasant|slave|lord>  디버그 소환기로 플레이어의 사람 하나를 마우스 자리에 만든다
//   preset <normal|easy|sandbox|god>                치트 표의 확인된 항목의 묶음을 건다(모드창의 프리셋과 같다)
//   time <pause|resume>                             게임의 시간을 멈춘다, 다시 흐르게 한다
//   world <cooldowns_clear|bishop>                  이벤트 쿨다운을 0 으로 쓴다, 주교를 부른다(research/14)
//   page <영역의 키>                                모드창의 영역을 고른다(explorer, economy, build, …)
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
