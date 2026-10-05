#pragma once
// 전투의 배율(src/People.cpp 의 BattleTick)에서 러너에 기대지 않는 판단. 잰 것은 research/13, 16.

namespace NlCore
{
	// 한 함수에 거는 아군·적 배율. On 이 거짓이면 걸 것이 없다.
	struct SideScale
	{
		bool On = false;
		double Mine = 1;	// self 가 플레이어의 영혼일 때 곱한다
		double Other = 1;	// 그 밖의 호출에 곱한다
	};

	// 아군 항목과 적 항목(각각 켜져 있을 때만)을 바꾸기 하나로 묶는다. 0 이하이거나 수가 아닌 배율은 1 로 본다. 둘 다 1 이면 걸지 않는다.
	constexpr SideScale PlanSides(bool AllyOn, double Ally, bool EnemyOn, double Enemy)
	{
		SideScale out;
		out.Mine = AllyOn && Ally > 0 ? Ally : 1;
		out.Other = EnemyOn && Enemy > 0 ? Enemy : 1;
		out.On = out.Mine != 1 || out.Other != 1;
		return out;
	}

	// 걸어 둔 것과 바라는 것이 같은가(다르면 같은 훅에 다시 건다).
	constexpr bool SameSides(const SideScale& A, const SideScale& B)
	{
		return A.On == B.On && A.Mine == B.Mine && A.Other == B.Other;
	}
}
