#include "common.hpp"

void RunEconomyTests()
{
	Test("경제: 자원의 이름과 갈래의 이름", [] {
		CHECK_STR(ResourceKey("resource.wood"), "wood");
		CHECK_STR(ResourceKey("wood"), "wood");
		CHECK_STR(ResourceKey(""), "");
		CHECK_STR(ResourceLabel("wood"), "나무 (wood)");
		CHECK_STR(ResourceLabel("something_new"), "something_new");
		CHECK_STR(CategoryLabel("food"), "음식");
		CHECK_STR(CategoryLabel("resources"), "자원");		// 게임의 화면이 쓰는 말(research/08 의 화면)
		CHECK_STR(CategoryLabel("herbs"), "식물");
		CHECK_STR(CategoryLabel("unknown"), "unknown");
	});

	Test("경제: 명령을 변화량으로 푼다", [] {
		const std::vector<double> counts = { 0, 300, 0, 5 };
		const std::vector<int> stocked = { 1, 2, 3 };
		const auto one = [&](EconomyAct act, int resource, double amount, double gold = 3000) {
			return PlanEconomy({ act, resource, amount }, gold, counts, counts, stocked);
		};
		// 금화: 더하기는 그대로, 맞추기는 차이만큼
		const auto add = one(EconomyAct::GoldAdd, -1, 1000);
		CHECK(add.size() == 1 && add[0].Resource == -1 && add[0].Delta == 1000);
		CHECK(one(EconomyAct::GoldSet, -1, 5000)[0].Delta == 2000 && one(EconomyAct::GoldSet, -1, 100)[0].Delta == -2900);
		CHECK(one(EconomyAct::GoldSet, -1, 3000).empty());					// 이미 그 수다
		CHECK(one(EconomyAct::GoldSet, -1, -50)[0].Delta == -3000);			// 음수로 맞추지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -5000)[0].Delta == -3000);		// 0 아래로 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -1000, 0).empty());				// 0 에서 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, -10, -50).empty());				// 이미 음수면 더 내리지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, 100, -50)[0].Delta == 100);
		CHECK(one(EconomyAct::GoldAdd, -1, 0.4).empty() && one(EconomyAct::GoldAdd, -1, 1.6)[0].Delta == 2);	// 정수로
		// 자원
		const auto wood = one(EconomyAct::ResourceAdd, 1, 50);
		CHECK(wood.size() == 1 && wood[0].Resource == 1 && wood[0].Delta == 50);
		CHECK(one(EconomyAct::ResourceAdd, 3, -100)[0].Delta == -5);
		CHECK(one(EconomyAct::ResourceSet, 1, 120)[0].Delta == -180 && one(EconomyAct::ResourceSet, 2, 7)[0].Delta == 7);
		CHECK(one(EconomyAct::ResourceAdd, 4, 10).empty() && one(EconomyAct::ResourceAdd, -1, 10).empty() && one(EconomyAct::ResourceSet, 99, 10).empty());
		// 모든 자원: 갈래에 든 것만(0번은 들지 않았다), 변화가 있는 것만
		const auto all = one(EconomyAct::AllAdd, -1, 100);
		CHECK(all.size() == 3 && all[0].Resource == 1 && all[1].Resource == 2 && all[1].Delta == 100 && all[2].Resource == 3);
		const auto less = one(EconomyAct::AllAdd, -1, -5);
		CHECK(less.size() == 2 && less[0].Resource == 1 && less[0].Delta == -5 && less[1].Resource == 3 && less[1].Delta == -5);
		// 수가 아니거나 터무니없으면 아무것도 하지 않는다
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::quiet_NaN()).empty());
		CHECK(one(EconomyAct::GoldAdd, -1, std::numeric_limits<double>::infinity()).empty() && one(EconomyAct::AllAdd, -1, 1e12).empty());

		// 지금 수가 정수가 아니어도 넘기는 변화량은 정수다. 줄여도 0 아래로 가지 않는다.
		CHECK(one(EconomyAct::GoldSet, -1, 5000, 3000.5)[0].Delta == 2000);
		const std::vector<double> half = { 0, 5.5 };
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, -10 }, 0, half, half, { 1 })[0].Delta == -5);
		CHECK(PlanEconomy({ EconomyAct::ResourceSet, 1, 0 }, 0, half, half, { 1 })[0].Delta == -5);
		// 읽은 수가 수가 아니면 부르지 않는다(NaN 을 게임의 함수에 넘기지 않는다).
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(one(EconomyAct::GoldAdd, -1, 100, nan).empty() && one(EconomyAct::GoldSet, -1, 100, nan).empty());
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, 10 }, 0, { 0, nan }, { 0, nan }, { 1 }).empty());
		// 맞추기의 차이가 터무니없이 크면 하지 않는다.
		CHECK(one(EconomyAct::GoldSet, -1, 0, 3e9).empty());
		// 건드려도 되는 목록에 없는 자원은 하나씩도 건드리지 않는다(신성 반지 0번은 패널이 EconomyTargets 로 목록에 넣는다).
		CHECK(one(EconomyAct::ResourceAdd, 0, 5).empty() && one(EconomyAct::ResourceSet, 0, 5).empty());
		// 예약된 몫은 줄이지 않는다: 줄일 수 있는 양은 예약되지 않은 수까지다. 더하는 것은 그대로다.
		const std::vector<double> total = { 0, 300 }, unreserved = { 0, 250 };
		CHECK(PlanEconomy({ EconomyAct::ResourceSet, 1, 0 }, 0, total, unreserved, { 1 })[0].Delta == -250);
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, -1000 }, 0, total, unreserved, { 1 })[0].Delta == -250);
		CHECK(PlanEconomy({ EconomyAct::ResourceAdd, 1, 10 }, 0, total, unreserved, { 1 })[0].Delta == 10);
		// 같은 번호가 두 번 들어 있어도 한 번만 한다. 범위 밖 번호는 건너뛴다.
		CHECK(PlanEconomy({ EconomyAct::AllAdd, -1, 5 }, 0, total, total, { 1, 1, 7 }).size() == 1);
	});

	Test("경제: 앞뒤의 수로 다 들어가지 않은 것을 가린다", [] {
		// 함수의 반환값이 "적용된 양"인지는 모른다(본 반환은 둘 다 청한 수와 같았다). 앞뒤의 수를 견준다.
		const std::vector<EconomyChange> done = { { -1, 1000 }, { 1, 50 }, { 2, 10 }, { 9, 5 } };
		const auto shorts = EconomyShortfall(done, 3000, { 0, 300, 0 }, 4000, { 0, 300, 10 });
		CHECK(shorts.size() == 2 && shorts[0].Resource == 1 && shorts[0].Asked == 50 && shorts[0].Applied == 0);
		CHECK(shorts[1].Resource == 9);		// 뒤의 수를 읽지 못한 것을 다 들어갔다고 하지 않는다
		const auto gold = EconomyShortfall({ { -1, 1000 } }, 3000, {}, 3600, {});
		CHECK(gold.size() == 1 && gold[0].Resource == -1 && gold[0].Applied == 600);
		CHECK(EconomyShortfall({ { -1, -500 }, { 1, -20 } }, 3000, { 0, 300 }, 2500, { 0, 280 }).empty());
	});

	Test("경제: 원격 명령의 낱말", [] {
		EconomyAct act = EconomyAct::GoldAdd;
		CHECK(ParseEconomyAct("gold_set", act) && act == EconomyAct::GoldSet && !NeedsResource(act));
		CHECK(ParseEconomyAct("add", act) && act == EconomyAct::ResourceAdd && NeedsResource(act));
		CHECK(ParseEconomyAct("set", act) && act == EconomyAct::ResourceSet && NeedsResource(act));
		CHECK(ParseEconomyAct("all", act) && act == EconomyAct::AllAdd && !NeedsResource(act));
		CHECK(ParseEconomyAct("gold_add", act) && act == EconomyAct::GoldAdd && !ParseEconomyAct("gold", act) && !ParseEconomyAct("", act));
	});

	Test("경제: 하나씩은 신성 반지에도, 모두에게는 갈래의 자원에만", [] {
		// 신성 반지는 자원 0번 rune 이다(현지화 main.csv 의 resource.rune = "신성 반지"). 번호가 아니라 이름으로 찾는다.
		CHECK(RingResource({ "rune", "wood", "food" }) == 0 && RingResource({ "wood", "rune" }) == 1);
		CHECK(RingResource({ "wood" }) == -1 && RingResource({}) == -1);
		CHECK_STR(ResourceLabel("rune"), "신성 반지 (rune)");
		const std::vector<int> stocked = { 1, 2 };
		CHECK(EconomyTargets(EconomyAct::ResourceAdd, stocked, 0) == (std::vector<int>{ 1, 2, 0 }));
		CHECK(EconomyTargets(EconomyAct::ResourceSet, stocked, 0) == (std::vector<int>{ 1, 2, 0 }));
		CHECK(EconomyTargets(EconomyAct::FloorSet, stocked, 0) == (std::vector<int>{ 1, 2, 0 }));
		CHECK(EconomyTargets(EconomyAct::AllAdd, stocked, 0) == stocked);			// "모든 자원 +100"이 반지를 100개 만들지 않는다
		CHECK(EconomyTargets(EconomyAct::ResourceAdd, stocked, -1) == stocked);		// 반지의 자리를 모르면 넣지 않는다
		CHECK(EconomyTargets(EconomyAct::ResourceAdd, { 0, 1 }, 0) == (std::vector<int>{ 0, 1 }));	// 두 번 넣지 않는다
		// 반지가 갈래에 들어 있어도(다른 빌드) "모든 자원"은 반지를 받지 않는다(검토의 지적: 잰 것에 기대지 않고 코드로 막는다).
		CHECK(EconomyTargets(EconomyAct::AllAdd, { 0, 1, 2 }, 0) == (std::vector<int>{ 1, 2 }));
		CHECK(PlanEconomy({ EconomyAct::AllAdd, -1, 100 }, 0, { 7, 300 }, { 7, 300 }, EconomyTargets(EconomyAct::AllAdd, { 0, 1 }, 0)).size() == 1);
		// 반지를 넣은 목록으로는 0번도 하나씩 바뀐다
		const std::vector<double> counts = { 7, 300 };
		const auto ring = PlanEconomy({ EconomyAct::ResourceAdd, 0, 5 }, 0, counts, counts, EconomyTargets(EconomyAct::ResourceAdd, { 1 }, 0));
		CHECK(ring.size() == 1 && ring[0].Resource == 0 && ring[0].Delta == 5);
		CHECK(PlanEconomy({ EconomyAct::ResourceSet, 0, 3 }, 0, counts, counts, { 1, 0 })[0].Delta == -4);
		const auto all = PlanEconomy({ EconomyAct::AllAdd, -1, 100 }, 0, counts, counts, EconomyTargets(EconomyAct::AllAdd, { 1 }, 0));
		CHECK(all.size() == 1 && all[0].Resource == 1);
	});

	Test("경제: 최소값 유지는 모자란 만큼만 더한다", [] {
		const double nan = std::numeric_limits<double>::quiet_NaN(), inf = std::numeric_limits<double>::infinity();
		const std::vector<double> free = { 2, 300, 0, 5.5 };
		const std::vector<int> allowed = { 0, 1, 2, 3 };
		// 바닥보다 적은 것만, 모자란 만큼(정수로 올려서). 금화는 -1.
		const auto changes = PlanFloors({ { -1, 5000 }, { 1, 250 }, { 2, 40 }, { 3, 6 }, { 0, 10 } }, 3000, free, allowed);
		CHECK(changes.size() == 4);
		CHECK(changes[0].Resource == -1 && changes[0].Delta == 2000);
		CHECK(changes[1].Resource == 2 && changes[1].Delta == 40);
		CHECK(changes[2].Resource == 3 && changes[2].Delta == 1);		// 5.5 → 6: 올림
		CHECK(changes[3].Resource == 0 && changes[3].Delta == 8);
		// 바닥과 같거나 많으면 건드리지 않는다. 줄이지 않는다.
		CHECK(PlanFloors({ { -1, 3000 }, { 1, 300 }, { 1, 10 } }, 3000, free, allowed).empty());
		// 0 이하, 수가 아닌 바닥, 터무니없는 바닥은 유지하지 않는다. 바닥은 정수로 읽는다(0.4 는 0, 2.6 은 3).
		CHECK(PlanFloors({ { -1, 0 }, { 1, -5 }, { 2, nan }, { 3, 1e12 }, { -1, inf }, { 2, 0.4 } }, 0, free, allowed).empty());
		CHECK(PlanFloors({ { 2, 2.6 } }, 0, free, allowed)[0].Delta == 3);
		// 읽은 수가 수가 아니면 하지 않는다(NaN 을 게임의 함수에 넘기지 않는다)
		CHECK(PlanFloors({ { -1, 100 } }, nan, free, allowed).empty());
		CHECK(PlanFloors({ { 1, 100 } }, 0, { 0, nan }, allowed).empty());
		// 허락되지 않은 자원, 범위 밖의 번호는 하지 않는다
		CHECK(PlanFloors({ { 2, 40 }, { 9, 40 }, { -2, 40 } }, 0, free, { 1 }).empty());
		// 같은 자원이 두 번 있으면 한 번만
		CHECK(PlanFloors({ { 2, 40 }, { 2, 80 } }, 0, free, allowed).size() == 1);
		// 음수인 금화도 바닥까지 채운다
		CHECK(PlanFloors({ { -1, 100 } }, -50, free, allowed)[0].Delta == 150);
		// 바닥이 한도(10억)와 같으면 한다. 채울 양이 한도를 넘으면 하지 않는다(게임의 함수에 그런 수를 넘기지 않는다).
		CHECK(PlanFloors({ { -1, 1e9 } }, 0, free, allowed)[0].Delta == 1e9);
		CHECK(PlanFloors({ { -1, 1e9 } }, -50, free, allowed).empty());
		// 기준은 예약되지 않은 수(화면의 수)다: 전체 300 가운데 50 이 예약됐으면 바닥 280 에 30 을 더한다.
		CHECK(PlanFloors({ { 1, 280 } }, 0, { 0, 250 }, { 1 })[0].Delta == 30);
		// 같은 자원이 두 번이면 앞의 것만 본다(앞의 것이 이미 채워져 있어도 뒤의 것으로 채우지 않는다).
		CHECK(PlanFloors({ { 1, 10 }, { 1, 400 } }, 0, { 0, 300 }, { 1 }).empty());
	});

	Test("경제: 최소값 유지 한 바퀴의 결과를 숨기지 않는다", [] {
		// (지키는 바닥, 채우려던 것, 부른 것, 불렀지만 청한 만큼 바뀌지 않은 것, 호출이 안 된 까닭)
		FloorRound round = FloorReport(0, 0, 0, 0, "");
		CHECK(round.Ok && round.Note == "지킬 최소값이 없습니다 (이 게임에 없는 자원뿐입니다)");
		round = FloorReport(4, 0, 0, 0, "");
		CHECK(round.Ok && round.Note == "최소값 4개를 지키는 중");
		round = FloorReport(4, 2, 2, 0, "");
		CHECK(round.Ok && round.Note == "최소값 4개를 지키는 중 (방금 2개를 채웠습니다)");
		// 부르지 못한 것과, 불렀지만 수가 바뀌지 않은 것은 실패다(검토의 지적: 부른 횟수를 "채웠습니다"라고 적지 않는다).
		round = FloorReport(4, 3, 1, 0, "no such script");
		CHECK(!round.Ok && round.Note == "최소값: 채우려던 3개 가운데 2개를 부르지 못했습니다 (no such script)");
		round = FloorReport(4, 3, 3, 2, "");
		CHECK(!round.Ok && round.Note == "최소값: 채우려던 3개 가운데 2개는 불러도 수가 청한 만큼 바뀌지 않았습니다");
		round = FloorReport(4, 3, 2, 1, "x");
		CHECK(!round.Ok && round.Note == "최소값: 채우려던 3개 가운데 1개를 부르지 못했고 (x) 1개는 불러도 수가 청한 만큼 바뀌지 않았습니다");
	});

	Test("경제: 최소값의 열쇠와 명령", [] {
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(GoodFloorKey("gold") && GoodFloorKey("wood_blanks") && GoodFloorKey("rune") && GoodFloorKey("r2"));
		CHECK(!GoodFloorKey("") && !GoodFloorKey("a b") && !GoodFloorKey("a=b") && !GoodFloorKey("#3") && !GoodFloorKey("Wood")
			&& !GoodFloorKey(std::string(41, 'a')));
		// 저장할 바닥: 0 이하와 수가 아닌 것은 "유지 안 함"(0). 정수로.
		CHECK(FloorValue(250.4) == 250 && FloorValue(0.4) == 0 && FloorValue(0.5) == 1 && FloorValue(-3) == 0 && FloorValue(nan) == 0);
		// 한도(10억)를 넘는 수와 유한하지 않은 수는 당기지 않고 버린다(검토의 지적): 잘못 친 수로 10억이 채워지지 않게. 넣을 때는 까닭을 말한다(GoodFloorAmount).
		const double inf = std::numeric_limits<double>::infinity();
		CHECK(FloorValue(1e9) == 1e9 && FloorValue(1e9 + 1) == 0 && FloorValue(1e12) == 0 && FloorValue(inf) == 0 && FloorValue(-inf) == 0);
		CHECK(GoodFloorAmount(250) && GoodFloorAmount(0) && GoodFloorAmount(-3) && GoodFloorAmount(1e9));
		CHECK(!GoodFloorAmount(1e9 + 1) && !GoodFloorAmount(1e12) && !GoodFloorAmount(inf) && !GoodFloorAmount(-inf) && !GoodFloorAmount(nan));
		EconomyAct act = EconomyAct::GoldAdd;
		CHECK(ParseEconomyAct("floor", act) && act == EconomyAct::FloorSet && NeedsResource(act) && IsFloorAct(act));
		CHECK(ParseEconomyAct("gold_floor", act) && act == EconomyAct::GoldFloor && !NeedsResource(act) && IsFloorAct(act));
		CHECK(!IsFloorAct(EconomyAct::GoldAdd) && !IsFloorAct(EconomyAct::ResourceSet) && !IsFloorAct(EconomyAct::AllAdd));
		// 바닥을 정하는 명령은 변화량을 내지 않는다(모자란 것은 틱이 채운다)
		CHECK(PlanEconomy({ EconomyAct::FloorSet, 1, 50 }, 0, { 0, 0 }, { 0, 0 }, { 1 }).empty());
		CHECK(PlanEconomy({ EconomyAct::GoldFloor, -1, 50 }, 0, { 0, 0 }, { 0, 0 }, { 1 }).empty());
		// 원격 명령
		const RemoteCommand floor = ParseRemoteLine("economy floor resource=1 amount=250");
		CHECK(floor.Error.empty() && floor.Target == "floor" && floor.Number == 250 && floor.Options.at("resource") == "1");
		CHECK(ParseRemoteLine("economy gold_floor amount=5000").Error.empty());
		CHECK(!ParseRemoteLine("economy floor amount=5").Error.empty() && !ParseRemoteLine("economy gold_floor resource=1 amount=5").Error.empty());
	});

	Test("경제: 최소값 칸의 편집 - 치는 동안은 들고 있다가 칸을 떠날 때 한 번 넣는다", [] {
		// 사용자 보고(2026-10-06): 자원마다의 최소값을 칸에서 정할 수 없다. 칸이 "Enter 를 눌렀을 때만 참"에 기대고 있었는데
		// Dear ImGui 의 수 입력 칸은 그것을 지원하지 않는다(InputScalar 의 단언). Enter 말고는 수를 넣을 길이 없었고 칸을 떠나면 친 수가 버려졌다.
		NumberEdit edit;
		double out = -1;
		// 2, 20, 200 을 치는 동안에는 넣지 않는다(치는 도중의 수로 창고를 채우지 않게)
		CHECK(!StepNumberEdit(edit, true, 2, false, true, out));
		CHECK(!StepNumberEdit(edit, true, 20, false, true, out));
		CHECK(!StepNumberEdit(edit, false, 20, false, true, out));		// 잡혀 있기만 한 프레임
		CHECK(!StepNumberEdit(edit, true, 200, false, true, out));
		CHECK(edit.Has && edit.Value == 200);
		// 칸을 떠나면(Enter, Tab, 다른 곳을 누름) 마지막에 친 수를 한 번 넣는다
		CHECK(StepNumberEdit(edit, false, 0, true, false, out) && out == 200);
		CHECK(!edit.Has);
		CHECK(!StepNumberEdit(edit, false, 0, true, false, out));			// 두 번 넣지 않는다
		// 치는 프레임에 바로 떠나도 넣는다
		CHECK(StepNumberEdit(edit, true, 7, true, false, out) && out == 7 && !edit.Has);
		// 치지 않고 떠나면 넣지 않는다
		CHECK(!StepNumberEdit(edit, false, 0, true, false, out));
		// 0 도 친 수다(지운다는 뜻). 넣는다
		CHECK(!StepNumberEdit(edit, true, 0, false, true, out));
		CHECK(StepNumberEdit(edit, false, 99, true, false, out) && out == 0);
		// 치다 만 수가 남았는데 칸이 잡혀 있지도 떠나지도 않았으면(창이 닫혔다) 버린다
		CHECK(!StepNumberEdit(edit, true, 55, false, true, out));
		CHECK(!StepNumberEdit(edit, false, 0, false, false, out) && !edit.Has);
	});
}
