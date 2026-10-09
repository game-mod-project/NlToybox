#include "common.hpp"

void RunCostBookTests()
{
	Test("건설비 장부: 처음 본 값을 기억하고 0 은 기억하지 않는다", [] {
		CostBook book;
		CHECK(book.Empty());
		book.Remember("altar", 1, 1, 30);
		book.Remember("altar", 1, -1, 0);			// 0 인 자리는 바꿀 것이 없다
		book.Remember("hut_6x10", 2, 32, 5);
		book.Remember("altar", 1, 1, 0);			// 0 으로 쓴 뒤 다시 보면 0 이 보인다. 처음 본 30 을 지킨다
		book.Remember("altar", 1, 1, 99);			// 이미 기억한 자리는 바꾸지 않는다
		CHECK(book.Size() == 2 && !book.Empty());
		CHECK(book.Entries()[0].Building == "altar" && book.Entries()[0].Level == 1 && book.Entries()[0].Slot == 1 && book.Entries()[0].Value == 30);
		CHECK(book.Entries()[1].Building == "hut_6x10" && book.Entries()[1].Slot == 32 && book.Entries()[1].Value == 5);
		book.Remember("altar", 2, 1, 7);			// 등급이 다르면 다른 자리다
		book.Remember("altar", 1, -1, 12);			// 금화(-1)도 한 자리다
		CHECK(book.Size() == 4);
		book.Clear();
		CHECK(book.Empty() && book.Entries().empty());
		book.Remember("altar", 1, 1, std::numeric_limits<double>::quiet_NaN());		// 수가 아니면 기억하지 않는다
		CHECK(book.Empty());
	});

	Test("건설비 장부: 되돌릴 값이 있는 자리인지 알려 주고, 되돌린 자리만 잊는다", [] {
		CostBook book;
		CHECK(book.Remember("altar", 1, 1, 30));			// 새로 기억했다. 0 으로 써도 된다
		CHECK(book.Remember("altar", 1, 1, 0));				// 이미 기억한 자리다
		CHECK(!book.Remember("altar", 1, 2, 0));			// 0 인 자리: 쓸 것이 없다
		// 유한하지 않은 값은 되돌릴 수 없다. 그런 자리는 0 으로 쓰지도 않는다(건물 데이터에 inf 가 있다: __limit. research/09).
		CHECK(!book.Remember("altar", 1, 3, std::numeric_limits<double>::infinity()));
		CHECK(book.Remember("hut_6x10", 2, 32, 5) && book.Remember("hut_6x10", 2, 1, 10));
		CHECK(book.Size() == 3);
		// 배율을 곱할 때는 지금 값이 아니라 처음 본 값(바탕)에 곱한다.
		double base = 0;
		CHECK(book.Find("hut_6x10", 2, 32, base) && base == 5);
		size_t index = 99;
		CHECK(book.Find("hut_6x10", 2, 1, base, &index) && base == 10 && index == 2 && book.Entries()[index].Slot == 1);
		base = 5;
		CHECK(!book.Find("hut_6x10", 2, 33, base) && !book.Find("altar", 1, 2, base) && base == 5);

		book.Forget({ 1, 0, 1 });							// 되돌린 자리만 잊는다. 못 되돌린 자리는 남아 다음에 다시 되돌린다
		CHECK(book.Size() == 1 && book.Entries()[0].Building == "hut_6x10" && book.Entries()[0].Slot == 32 && book.Entries()[0].Value == 5);
		book.Forget({ 1, 1 });								// 수가 맞지 않으면 아무것도 잊지 않는다
		CHECK(book.Size() == 1);
		book.Forget({ 0 });
		CHECK(book.Size() == 1);
		book.Forget({ 1 });
		CHECK(book.Empty());
	});

	Test("자료의 배율: 처음 본 값에 곱하고, 다시 훑어도 두 번 곱하지 않는다", [] {
		CostBook book;
		double wanted = -1;
		size_t index = 99;
		CHECK(PlanValue(book, "storage.raw", 0, -1, 300, 10, false, wanted, index) && wanted == 3000 && index == 0);
		CHECK(PlanValue(book, "storage.raw", 0, -1, 3000, 10, false, wanted, index) && wanted == 3000);		// 써 둔 값을 다시 봤다. 쓸 것이 없다
		CHECK(PlanValue(book, "storage.raw", 0, -1, 3000, 2, false, wanted, index) && wanted == 600);			// 켠 채 배율을 바꿨다: 바탕에 곱한다
		CHECK(PlanValue(book, "storage.raw", 0, -1, 300, 2, false, wanted, index) && wanted == 600);			// 게임이 자료를 다시 만들었다(같은 열쇠, 원래 값)
		CHECK(PlanValue(book, "storage.raw", 0, -1, 600, 1, false, wanted, index) && wanted == 300 && index == 0);	// 끄면 바탕으로 되돌린다
		CHECK(book.Size() == 1);

		// 되돌리는 중에 처음 보는 자리는 건드린 적이 없다. 0 이거나 유한하지 않은 값은 장부가 받지 않는다(쓰지 않는다).
		CHECK(!PlanValue(book, "hall.raw", 0, -1, 300, 1, false, wanted, index));
		CHECK(!PlanValue(book, "default.raw", 0, -1, 0, 10, false, wanted, index));
		CHECK(!PlanValue(book, "x.y", 0, -1, std::numeric_limits<double>::infinity(), 10, false, wanted, index));
		CHECK(book.Size() == 1);

		// 0 으로 쓰는 항목(생산 재료 없음). 0 이 된 자리도 장부로 알아보고, 끄면 되살린다.
		CHECK(PlanValue(book, "workshop", 14, 4, 1, 0, true, wanted, index) && wanted == 0 && index == 1);
		CHECK(PlanValue(book, "workshop", 14, 4, 0, 0, true, wanted, index) && wanted == 0 && index == 1);
		CHECK(PlanValue(book, "workshop", 14, 4, 0, 1, true, wanted, index) && wanted == 1 && index == 1);

		// 정수는 정수로 남는다(만들어지는 수 1 에 2.5 를 곱하면 3. 0 이 되지 않는다).
		CHECK(PlanValue(book, "mine", 4, -1, 1, 2.5, false, wanted, index) && wanted == 3);
		CHECK(PlanValue(book, "mine", 4, -1, 3, 0.1, false, wanted, index) && wanted == 1);
	});

	Test("다시 해 보기: 실패가 이어지면 간격을 두 배씩 늘리고 성공하면 처음으로 돌아간다", [] {
		Retry retry(2, 60);
		CHECK(retry.Due(0) && retry.Failures() == 0);
		retry.Failed(10);
		CHECK(!retry.Due(11.9) && retry.Due(12) && retry.Failures() == 1);
		retry.Failed(12);
		CHECK(!retry.Due(15.9) && retry.Due(16) && retry.Failures() == 2);
		for (int i = 0; i < 10; i++)
			retry.Failed(100);
		CHECK(!retry.Due(159.9) && retry.Due(160));			// 가장 긴 간격(60초)을 넘지 않는다
		retry.Succeeded();
		CHECK(retry.Due(100) && retry.Failures() == 0);
		retry.Failed(200);
		CHECK(!retry.Due(201.9) && retry.Due(202));			// 다시 처음 간격부터
	});
}
