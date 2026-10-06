#pragma once
// 모듈이 기억해 둔 값이 어느 게임·어느 지도의 것인지 가리는 표식(src/World.cpp 의 붙들기 둘이 쓴다).
// 관리자 구조체의 주소와 지도 관리 인스턴스(o_game_map_controller)를 함께 본다. 둘 중 하나라도 달라지면 다른 자리로 보고 기억한 값을 버린다:
// 다른 세이브를 불러왔거나 다른 지도로 간 것일 수 있다(그때 무엇이 바뀌는지를 재지는 않았다. 달라진 것이 보이면 버리는 쪽을 택한다).

#include <cstdint>

namespace NlCore
{
	struct PlaceKey
	{
		std::uintptr_t Struct = 0;		// 관리자 구조체의 주소
		long long Instance = 0;			// 지도 관리 인스턴스를 가리는 수
		bool operator==(const PlaceKey&) const = default;
	};
}
