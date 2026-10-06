#pragma once
// 훅을 다루는 쪽(src/Recorder.cpp, src/Cheats.cpp)의 판단 가운데 러너에 기대지 않는 것.

#include "Knobs.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace NlCore
{
	// 훅 항목(CheatKind::Hook, HookNumber, HookScale)을 이번 틱에 어떻게 할지.
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
	// 0 이하의 값도 그대로 둔다: "없음"을 0 이나 음수로 돌려주는 함수의 표식을 깨지 않는다.
	inline double ScaleResult(double Value, double Factor, bool Whole)
	{
		if (!std::isfinite(Factor) || !(Factor > 0) || !(Value > 0))
			return Value;
		return Whole ? Scale(Value, Factor) : Value * Factor;
	}

	// 훅 항목이 지금 바라는 값까지 걸려 있는가. Applied: 이 항목이 걸었다. Scale: 배율 항목(HookScale)이다.
	// 켜져 있는 배율 항목은 걸어 둔 배율(AppliedNumber)이 창의 배율(Number)과 같아야 한다. 다르면 같은 훅에 새 배율을 다시 건다.
	// 끄는 중(On 이 거짓)에는 배율을 견주지 않는다: 걸어 둔 것이 있으면 끈다. ChooseHookStep 의 Applied 로 넘긴다.
	constexpr bool HookCurrent(bool On, bool Applied, bool Scale, double AppliedNumber, double Number)
	{
		return Applied && (!On || !Scale || AppliedNumber == Number);
	}

	// 걸기에 실패한 뒤 "이 항목이 걸었다"로 남길지. 앞서 건 바꾸기가 살아 있으면(Live) 남긴다: 다음 틱에 다시 걸어 보고,
	// 끄면 그 바꾸기를 이름으로 끈다. 남기지 않으면 앞 배율의 바꾸기가 주인 없이 남는다.
	constexpr bool AppliedAfterFailure(bool Live)
	{
		return Live;
	}

	// 바꾸기를 누구의 호출에 걸지. 'a': 모두. 'p': self 가 플레이어의 것일 때만. 'o': 플레이어의 것이 아닐 때만.
	constexpr bool HookApplies(char Who, bool Mine)
	{
		return Who == 'p' ? Mine : Who == 'o' ? !Mine : true;
	}

	// 배율을 거는 바꾸기('x')가 이번 호출에 곱할 배율. 'a': 언제나 Number. 'p': self 가 플레이어의 것이면 Number, 아니면 Other. 'o': 그 반대.
	// 1 이면 곱하지 않는다(원래 값이 그대로 지나간다). Known: 플레이어의 것들의 주소를 알고 있다(묶음이 비어 있지 않다).
	// 가리는 바꾸기는 주소를 모르는 동안 아무에게도 곱하지 않는다: 묶음이 비면 모두가 "플레이어의 것이 아님"으로 읽혀 아군이 적의 배율을 받는다.
	constexpr double HookFactor(char Who, bool Mine, double Number, double Other, bool Known)
	{
		if (Who != 'p' && Who != 'o')
			return Number;
		if (!Known)
			return 1;
		return (Who == 'p') == Mine ? Number : Other;
	}

	// ScaleResult 에 위쪽 한도를 더한 것. Cap 이 0 보다 크면 올린 값이 Cap 을 넘지 않게 한다(번호로 쓰이는 수준 같은 값).
	// 원래 값이 이미 Cap 을 넘으면 낮추지 않고, 내리는 배율은 한도와 무관하다.
	inline double ScaleCapped(double Value, double Factor, bool Whole, double Cap)
	{
		const double scaled = ScaleResult(Value, Factor, Whole);
		if (!(Cap > 0) || !(scaled > Cap) || !(scaled > Value))		// 수가 아닌 값(NaN)은 견줌이 모두 거짓이다: 그대로 지나간다
			return scaled;
		return Value > Cap ? Value : Cap;
	}

	// 훅이 self 와 견줄 주소들(플레이어의 영혼). 틱이 통째로 갈아 끼우고 훅이 찾는다. 0 은 담지 않는다.
	class SelfSet
	{
	public:
		void Replace(std::vector<std::uintptr_t> Values)
		{
			std::sort(Values.begin(), Values.end());
			Values.erase(std::unique(Values.begin(), Values.end()), Values.end());
			if (!Values.empty() && Values.front() == 0)
				Values.erase(Values.begin());
			m_Sorted = std::move(Values);
		}
		bool Has(std::uintptr_t Value) const
		{
			return Value != 0 && std::binary_search(m_Sorted.begin(), m_Sorted.end(), Value);
		}
		size_t Size() const { return m_Sorted.size(); }

	private:
		std::vector<std::uintptr_t> m_Sorted;
	};

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
