#pragma once
// 러너에 닿는 얇은 층. 빌트인 호출과 이미 써 본 YYTK 인터페이스만 쓴다(러너 내부 구조체의 배치에 기대지 않는다).
// Init 을 뺀 나머지는 게임 스레드에서만 부른다.

#include <YYTK_Shared.hpp>

#include <ostream>
#include <string>
#include <vector>

namespace NlGame
{
	struct Object
	{
		std::string Name;
		double Index = 0;	// object_index. 빌트인에 수로 넘긴다
	};

	void Init(YYTK::YYTKInterface* Yytk);
	YYTK::YYTKInterface* Yytk();

	// 전역 인스턴스. 못 얻으면 nullptr.
	YYTK::CInstance* Global();

	// 빌트인을 부른다. 실패하면 거짓.
	bool Call(const char* Name, const std::vector<YYTK::RValue>& Args, YYTK::RValue& Result);

	// 수를 돌려주는 빌트인. 실패하거나 수가 아니면 Fallback.
	double CallNumber(const char* Name, const std::vector<YYTK::RValue>& Args, double Fallback);

	// 수(불리언 포함)인가. 문자열·구조체·배열·undefined 는 아니다.
	bool IsNumber(const YYTK::RValue& Value);

	// 게임의 오브젝트 전부. 처음 부를 때 0 번부터 object_exists 가 참인 동안 이름을 모은다.
	const std::vector<Object>& Objects();

	// 지금 룸의 이름. 못 얻으면 빈 글이다(한 번 실패하면 다시 시도하지 않는다).
	std::string RoomName();

	// "global.a.b" 를 따라간다. 없으면 거짓.
	bool Resolve(const std::string& Path, YYTK::RValue& Out);

	// 배열 길이. 못 얻으면 음수.
	double ArrayLength(const YYTK::RValue& Value);

	// 값 하나를 "kind":…[,"value":…] 로 적는다(중괄호 없이). 구조체와 배열의 속은 보지 않는다.
	std::string Describe(const YYTK::RValue& Value);

	// 구조체의 멤버를 "이름":{…},… 로 적는다. Expand 면 구조체 멤버를 한 단계 더 편다.
	void WriteMembers(std::ostream& Out, const YYTK::RValue& Object, bool Expand, bool FlushEach);
}
