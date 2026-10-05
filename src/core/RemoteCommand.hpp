#pragma once
// 켜져 있는 게임에 묻는 파일(NlToyBox.ask.txt)의 한 줄. 러너에 기대지 않는다. 스펙: 치트 메뉴 §14.
//   ask <주소>                                      값 하나
//   list <주소> [as=map|list] [max=N]               그릇의 자식들
//   tree <주소> [depth=N] [max=N]                   자식들을 깊이 N 까지
//   find [name=글] [value=수] [in=global,inst,ds]   이름·값으로 찾기
//   refine value=수                                 앞의 find 결과 가운데 지금 값이 그 수인 것만
//   write <주소>=<수>                               써 넣는다(남긴다)
//   poke <주소>=<수>                                써 넣고, 읽고, 되돌린다
//   record <스크립트 이름|메서드의 주소>            그 함수의 호출을 기록한다
//   unrecord <스크립트 이름|all>,  records [스크립트 이름]
//   call <스크립트 이름> [인자…]                    게임 스크립트를 부른다
//   method <메서드의 주소> [인자…]                  메서드를 그것이 묶인 구조체에서 부른다
//   state,  shot <이름>,  window open|close
// 인자: n:<수>  s:<글> 또는 s:{공백이 든 글}  b:0|1  u(undefined)  p:<주소>(그 주소의 값)

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
}
