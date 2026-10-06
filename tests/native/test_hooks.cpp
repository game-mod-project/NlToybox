#include "common.hpp"

void RunHooksTests()
{
	Test("훅 항목: 켠 것과 실제로 걸린 것이 어긋나면 다시 건다", [] {
		CHECK(ChooseHookStep(true, false, false) == HookStep::Apply);
		CHECK(ChooseHookStep(true, true, true) == HookStep::None);
		CHECK(ChooseHookStep(true, true, false) == HookStep::Apply);		// 다른 곳(원격 unoverride all)이 껐다. 체크가 켜져 있으면 다시 건다
		CHECK(ChooseHookStep(true, false, true) == HookStep::Apply);		// 원격이 걸어 둔 것을 이 항목의 값으로 맞춘다
		CHECK(ChooseHookStep(false, true, true) == HookStep::Remove);
		CHECK(ChooseHookStep(false, true, false) == HookStep::Remove);		// 걸었다는 표시를 지운다
		CHECK(ChooseHookStep(false, false, false) == HookStep::None);
		CHECK(ChooseHookStep(false, false, true) == HookStep::None);		// 이 항목이 걸지 않은 바꾸기(원격 override)는 건드리지 않는다
	});

	Test("훅 항목: 배율이 바뀌면 다시 걸고, 다시 걸다 실패해도 앞서 건 것을 끌 수 있다", [] {
		CHECK(HookCurrent(true, true, false, 0, 0));					// 고정값 훅: 걸었으면 그대로다
		CHECK(HookCurrent(true, true, true, 0.5, 0.5));
		CHECK(!HookCurrent(true, true, true, 0.5, 0.3));				// 배율이 바뀌었다. 다시 건다
		CHECK(!HookCurrent(true, false, true, 0.5, 0.5));
		CHECK(HookCurrent(false, true, true, 0.5, 0.3));				// 끄는 중에는 배율을 견주지 않는다(걸어 둔 것을 끈다)

		// 0.5 로 걸어 둔 채 0.3 으로 바꿨는데 다시 걸기에 실패했다(메뉴로 나가 주소가 풀리지 않는다). 앞서 건 바꾸기는 살아 있다.
		bool applied = true;
		const bool live = true;
		CHECK(ChooseHookStep(true, HookCurrent(true, applied, true, 0.5, 0.3), live) == HookStep::Apply);
		applied = AppliedAfterFailure(live);
		CHECK(applied);		// "걸었다"로 남는다. 그래야 끌 때 그 바꾸기를 이름으로 끈다(주인 없는 바꾸기가 남지 않는다)
		CHECK(ChooseHookStep(false, HookCurrent(false, applied, true, 0.5, 0.3), live) == HookStep::Remove);
		CHECK(!AppliedAfterFailure(false));								// 살아 있는 것이 없으면 걸지 않은 것이다
	});

	Test("훅의 배율: 원래 값에 곱하고, 정수로 남길지 고른다", [] {
		CHECK(ScaleResult(8, 0.1, true) == 1);					// 가격: 정수는 정수로 남고 0 이 되지 않는다
		CHECK(ScaleResult(100, 0.1, true) == 10 && ScaleResult(3600, 0.1, true) == 360 && ScaleResult(250, 100, true) == 25000);
		CHECK(ScaleResult(1, 5, false) == 5 && ScaleResult(1.2, 5, false) == 6);
		CHECK(ScaleResult(1, 0.5, false) == 0.5 && ScaleResult(1, 0.5, true) == 1);		// 계수는 그대로 곱한다. 정수로 남기면 1 아래로 내려가지 않는다
		// 0 이하의 값은 곱하지 않는다: "없음"을 0 이나 음수로 돌려주는 함수의 표식을 깨지 않는다.
		CHECK(ScaleResult(0, 10, true) == 0 && ScaleResult(-4, 2, true) == -4 && ScaleResult(-1, 0.5, false) == -1);
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(ScaleResult(5, nan, true) == 5 && ScaleResult(5, 0, true) == 5 && ScaleResult(5, -1, false) == 5);		// 쓸 수 없는 배율이면 원래 값 그대로
		CHECK(ScaleResult(5, std::numeric_limits<double>::infinity(), false) == 5);
	});

	Test("훅의 자리: 걸린 대상은 그 자리, 걸다 실패한 대상에는 새 자리를 주지 않는다", [] {
		const int a = 0, b = 0, c = 0, d = 0;
		std::vector<HookSlot> slots(3);
		int index = -9;
		CHECK(PickHookSlot(slots, &a, index) == SlotPick::Free && index == 0);
		slots[0] = { true, false, &a };
		slots[1] = { true, true, &b };						// 훅을 걸다 실패했다
		CHECK(PickHookSlot(slots, &a, index) == SlotPick::Existing && index == 0);
		// 같은 대상을 0.5초마다 다시 걸면 자리 64개가 32초에 없어진다. 실패한 대상은 그 실행에서 다시 걸지 않는다.
		CHECK(PickHookSlot(slots, &b, index) == SlotPick::Failed && index == 1);
		CHECK(PickHookSlot(slots, &c, index) == SlotPick::Free && index == 2);
		slots[2] = { true, false, &c };
		CHECK(PickHookSlot(slots, &d, index) == SlotPick::Full && index == -1);
		CHECK(PickHookSlot(slots, &c, index) == SlotPick::Existing && index == 2);
	});

	Test("전투: 아군과 적에게 따로 거는 배율", [] {
		// 한 함수에 거는 배율: self 가 플레이어의 것이면 Number, 아니면 Other('p'). 'a' 는 언제나 Number
		CHECK(HookFactor('a', false, 3, 0.5, true) == 3 && HookFactor('a', true, 3, 0.5, true) == 3);
		CHECK(HookFactor('p', true, 3, 0.5, true) == 3 && HookFactor('p', false, 3, 0.5, true) == 0.5);
		CHECK(HookFactor('o', true, 3, 0.5, true) == 0.5 && HookFactor('o', false, 3, 0.5, true) == 3);
		CHECK(HookFactor('p', false, 3, 1, true) == 1);		// 적 배율이 없으면 그대로 지나간다
		// 검토의 지적: 영혼의 주소를 아직 모르면(묶음이 비었다: 메뉴에서 막 들어왔다) 아무에게도 곱하지 않는다. 아군이 적의 배율을 받지 않게
		CHECK(HookFactor('p', false, 3, 0.5, false) == 1 && HookFactor('o', false, 3, 0.5, false) == 1);
		CHECK(HookFactor('a', false, 3, 0.5, false) == 3);		// 가리지 않는 바꾸기는 묶음과 무관하다
		// 수가 아닌 값은 한도가 있어도 그대로 지나간다
		CHECK(std::isnan(ScaleCapped(std::numeric_limits<double>::quiet_NaN(), 2, true, 20)));

		// 위쪽 한도: 올린 값은 한도에서 멈추고, 원래 한도를 넘던 값과 내린 값은 건드리지 않는다
		CHECK(ScaleCapped(10, 2, true, 20) == 20 && ScaleCapped(12, 2, true, 20) == 20 && ScaleCapped(7, 2, true, 20) == 14);
		CHECK(ScaleCapped(25, 2, true, 20) == 25);		// 원래 값이 이미 한도를 넘는다: 낮추지 않는다
		CHECK(ScaleCapped(30, 0.5, true, 20) == 15 && ScaleCapped(50, 0.5, true, 20) == 25);		// 내리는 배율은 한도와 무관하다
		CHECK(ScaleCapped(40, 3, true, 0) == 120 && ScaleCapped(40, 0.3, true, 0) == 12);			// 한도 없음
		CHECK(ScaleCapped(0, 3, true, 20) == 0 && ScaleCapped(-4, 3, true, 20) == -4);				// "없음"의 표식은 그대로

		// 아군 항목과 적 항목을 바꾸기 하나로 묶는다
		const SideScale none = PlanSides(false, 2, false, 0.5);
		CHECK(!none.On && none.Mine == 1 && none.Other == 1);
		const SideScale ally = PlanSides(true, 2, false, 0.5);
		CHECK(ally.On && ally.Mine == 2 && ally.Other == 1);
		const SideScale both = PlanSides(true, 2, true, 0.5);
		CHECK(both.On && both.Mine == 2 && both.Other == 0.5);
		CHECK(!PlanSides(true, 1, true, 1).On);			// 둘 다 1 이면 걸 것이 없다
		CHECK(!PlanSides(true, 0, false, 0).On && !PlanSides(true, -2, true, std::numeric_limits<double>::quiet_NaN()).On);
		CHECK(SameSides(both, PlanSides(true, 2, true, 0.5)) && !SameSides(both, ally) && !SameSides(both, PlanSides(true, 3, true, 0.5)));
	});

	Test("훅: 누구의 호출에 걸지와 self 의 묶음", [] {
		CHECK(HookApplies('a', true) && HookApplies('a', false));
		CHECK(HookApplies('p', true) && !HookApplies('p', false));
		CHECK(!HookApplies('o', true) && HookApplies('o', false));
		SelfSet set;
		CHECK(set.Size() == 0 && !set.Has(0x1000));
		set.Replace({ 0x3000, 0x1000, 0, 0x2000, 0x1000 });		// 0 은 버리고 겹친 것은 하나로
		CHECK(set.Size() == 3 && set.Has(0x1000) && set.Has(0x2000) && set.Has(0x3000));
		CHECK(!set.Has(0) && !set.Has(0x1800) && !set.Has(0x4000));
		set.Replace({});
		CHECK(set.Size() == 0 && !set.Has(0x1000));
	});
}
