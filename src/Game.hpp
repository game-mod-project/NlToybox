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

	// 게임 스크립트를 정식 이름("gml_Script_x". 접두가 없으면 붙인다)으로 부른다. self 와 other 는 전역이다.
	// 인자의 수와 형은 부르는 쪽이 책임진다: 틀리면 게임이 GML 오류로 끝난다. 없는 스크립트면 거짓.
	bool CallScript(const std::string& Name, const std::vector<YYTK::RValue>& Args, YYTK::RValue& Result);

	// 수(불리언 포함)인가. 문자열·구조체·배열·undefined 는 아니다.
	bool IsNumber(const YYTK::RValue& Value);

	// 불리언이 아닌 수인가(IsNumber 에서 VALUE_BOOL 을 뺀 것). 쿨다운·매장량처럼 수만 고칠 자리를 가릴 때 쓴다.
	bool IsRealNumber(const YYTK::RValue& Value);

	// RValue::m_Kind 에서 형만 남기는 가림(VALUE_UNSET 의 폭. YYTK_Shared_Types.hpp 199행). 가린 값이 k_KindMask 그대로면 VALUE_UNSET 이다.
	constexpr int k_KindMask = 0x0ffffff;

	// GML 의 ds 형 상수(ds_exists 의 둘째 인자). 출처: YoYoGames/GameMaker-HTML5 scripts/functions/Function_YoYo.js 36~41행.
	// 이 러너에서도 맞는지는 Finder 의 FindDataStructures 가 VerifyType 으로 확인한 뒤에만 쓴다.
	constexpr double k_DsMap = 1, k_DsList = 2;

	// ds 번호를 훑는 범위와, 이만큼 잇달아 비어 있으면 그만 훑는 수. 게임 안에서 본 가장 큰 번호는 1,467 이다(research/02). 번호는 0 부터 빈틈없이 배정돼 있었다.
	constexpr int k_MaxDsId = 20000, k_MaxMissing = 500;

	// 게임의 오브젝트 전부. 처음 부를 때 0 번부터 object_exists 가 참인 동안 이름을 모은다.
	const std::vector<Object>& Objects();

	// 지금 룸의 이름. 못 얻으면 빈 글이다(한 번 실패하면 다시 시도하지 않는다).
	std::string RoomName();

	// "global.a.b" 를 따라간다. 없으면 거짓.
	bool Resolve(const std::string& Path, YYTK::RValue& Out);

	// 러너가 말하는 구조체의 멤버 수. 알 수 없으면 -1.
	int MemberCount(const YYTK::RValue& Struct);

	// 배열 길이. 못 얻으면 음수.
	double ArrayLength(const YYTK::RValue& Value);

	// 값 하나를 "kind":…[,"value":…] 로 적는다(중괄호 없이). 구조체와 배열의 속은 보지 않는다.
	std::string Describe(const YYTK::RValue& Value);

	// 구조체의 멤버를 "이름":{…},… 로 적는다. Expand 면 구조체 멤버를 한 단계 더 편다.
	void WriteMembers(std::ostream& Out, const YYTK::RValue& Object, bool Expand, bool FlushEach);
}
