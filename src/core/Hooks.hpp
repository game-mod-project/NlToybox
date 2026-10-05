#pragma once
// 훅을 다루는 쪽(src/Recorder.cpp, src/Cheats.cpp)의 판단 가운데 러너에 기대지 않는 것.

#include "Knobs.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace NlCore
{
	// 훅 항목(CheatKind::Hook)을 이번 틱에 어떻게 할지.
	// On: 체크가 켜져 있다. Applied: 이 항목이 바꾸기를 걸었다. Live: 그 함수의 바꾸기가 지금 켜져 있다.
	enum class HookStep { None, Apply, Remove };

	constexpr HookStep ChooseHookStep(bool On, bool Applied, bool Live)
	{
		if (On)
			return Applied && Live ? HookStep::None : HookStep::Apply;		// 다른 곳(원격 unoverride)이 껐으면 다시 건다. 체크와 실제가 어긋나지 않게
		return Applied ? HookStep::Remove : HookStep::None;					// 이 항목이 걸지 않은 바꾸기(원격 override)는 건드리지 않는다
	}

	// 함수가 돌려준 수에 배율을 곱한 값(반환값에 배율을 거는 훅이 쓴다). Whole: 정수는 정수로 남기고 양수는 1 아래로 내리지 않는다
	// (가격 8 에 0.1 을 곱해도 1 이다. core/Knobs 의 Scale). 배율이 0 보다 큰 유한한 수가 아니면 원래 값 그대로.
	inline double ScaleResult(double Value, double Factor, bool Whole)
	{
		if (!std::isfinite(Factor) || !(Factor > 0))
			return Value;
		return Whole ? Scale(Value, Factor) : Value * Factor;
	}

	struct HookSlot
	{
		bool Used = false;
		bool Failed = false;			// 훅을 걸다 실패했다
		const void* Target = nullptr;	// 훅을 건(걸려던) 함수
	};

	enum class SlotPick { Existing, Failed, Free, Full };

	// Target 에 쓸 자리를 고른다. Existing: 이미 걸려 있다(Index 가 그 자리). Failed: 이 실행에서 걸다 실패했던 대상이다. 다시 걸지 않는다
	// (실패한 자리는 다시 쓰지 않으므로, 되풀이해 걸면 자리가 그만큼 없어진다). Free: 빈 자리(Index). Full: 빈 자리가 없다(Index 는 -1).
	inline SlotPick PickHookSlot(const std::vector<HookSlot>& Slots, const void* Target, int& Index)
	{
		int free_slot = -1;
		for (size_t i = 0; i < Slots.size(); i++)
		{
			if (Slots[i].Used && Slots[i].Target == Target)
			{
				Index = static_cast<int>(i);
				return Slots[i].Failed ? SlotPick::Failed : SlotPick::Existing;
			}
			if (!Slots[i].Used && free_slot < 0)
				free_slot = static_cast<int>(i);
		}
		Index = free_slot;
		return free_slot < 0 ? SlotPick::Full : SlotPick::Free;
	}
}
