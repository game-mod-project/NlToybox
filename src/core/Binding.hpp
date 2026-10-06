#pragma once
// 메서드를 어떤 self 로 부를지 정한다. 러너에 기대지 않는다. 근거: research/07, 매뉴얼의 method·method_get_self.

namespace NlCore
{
	// 메서드를 가진 것(주소의 부모).
	enum class OwnerKind { Global, Struct, Instance, Other };		// Other: 배열의 원소, ds 의 값

	enum class Binding
	{
		AsIs,		// 이미 묶인 곳이 있다. 그대로 부른다
		ToOwner,	// 묶인 곳이 없다. 그것을 가진 구조체나 인스턴스에 묶어 부른다(생성자의 정적 메서드)
		Refuse,		// 묶인 곳도 없고 묶을 곳도 없다. 부르지 않는다
	};

	// 묶인 곳이 없는 메서드를 그대로 부르면 self 가 부른 쪽(전역)이 된다. 본문이 인스턴스의 변수를 읽으면 GML 오류로 게임이 끝나고,
	// 쓰면 전역에 변수가 생긴다. 가진 것이 구조체나 인스턴스가 아니면 묶을 곳을 알 수 없으므로 부르지 않는다.
	constexpr Binding ChooseBinding(bool Bound, OwnerKind Owner)
	{
		if (Bound)
			return Binding::AsIs;
		return Owner == OwnerKind::Struct || Owner == OwnerKind::Instance ? Binding::ToOwner : Binding::Refuse;
	}
}
