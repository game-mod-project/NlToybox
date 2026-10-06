#include "common.hpp"

void RunTextTests()
{
	Test("Trim 은 양끝의 공백과 줄바꿈을 뗀다", [] {
		CHECK_STR(Trim("  a b \r\n"), "a b");
		CHECK_STR(Trim(" \t "), "");
	});

	Test("Quote 는 따옴표, 역슬래시, 제어 문자를 JSON 으로 쓴다", [] {
		CHECK_STR(Quote("a\"b\\c\n\x01"), "\"a\\\"b\\\\c\\n\\u0001\"");
	});

	Test("Quote 는 긴 글을 UTF-8 글자 경계에서 자른다", [] {
		// 3바이트 글자 넷(12바이트). 7바이트에서 자르면 두 글자(6바이트)만 남아야 한다.
		const std::string text = "\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80\xEA\xB0\x80";
		CHECK_STR(Quote(text, 7), "\"\xEA\xB0\x80\xEA\xB0\x80...\"");
	});

	Test("Quote 는 잘못된 UTF-8 바이트를 물음표로 바꾼다", [] {
		CHECK_STR(Quote("a\xFF" "b"), "\"a?b\"");
	});

	Test("Number 는 정수는 정수로, 유한하지 않은 수는 글로 쓴다", [] {
		CHECK_STR(Number(2345), "2345");
		CHECK_STR(Number(0.5), "0.5");
		CHECK_STR(Number(std::nan("")), "\"nan\"");
		CHECK_STR(Number(std::numeric_limits<double>::infinity()), "\"inf\"");
		CHECK_STR(Number(-std::numeric_limits<double>::infinity()), "\"-inf\"");
	});

	Test("Fixed 는 소수 자릿수를 맞춘다", [] {
		CHECK_STR(Fixed(12.345, 1), "12.3");
		CHECK_STR(Fixed(7, 1), "7.0");
	});

	Test("ParseNumber 는 글 전체가 수일 때만 받는다", [] {
		double value = 0;
		CHECK(ParseNumber("60", value) && value == 60);
		CHECK(ParseNumber("-1.5", value) && value == -1.5);
		CHECK(!ParseNumber("60s", value));
		CHECK(!ParseNumber("", value));
		CHECK(!ParseNumber("abc", value));
	});

	Test("Contains 는 부분 일치를 본다. 빈 낱말은 아무것에도 맞지 않는다", [] {
		CHECK(Contains("battle_battle_dodge_base", "dodge_base"));
		CHECK(!Contains("dodge", "dodge_base"));
		CHECK(!Contains("abc", ""));
	});

	Test("Shortest 는 다시 읽으면 같은 수가 되는 가장 짧은 글을 쓴다", [] {
		// 구조체의 주소를 적는 글: 기록의 표본과 ask 의 답에서 "어느 구조체인가"를 견주는 데 쓴다(research/20).
		CHECK_STR(PointerText(0), "@0");
		CHECK_STR(PointerText(0x1a2b3c), "@1a2b3c");
		CHECK_STR(PointerText(0x7ff6ab00cdef), "@7ff6ab00cdef");
		CHECK_STR(Shortest(0.83), "0.83");
		CHECK_STR(Shortest(3), "3");
		CHECK_STR(Shortest(-4), "-4");
		CHECK_STR(Shortest(0.1 + 0.2), "0.30000000000000004");
		CHECK_STR(Shortest(std::numeric_limits<double>::quiet_NaN()), "nan");
		CHECK_STR(Shortest(-std::numeric_limits<double>::infinity()), "-inf");
		for (const double value : { 36550.20449999981, 1e20, -0.000123, 57.4 })
		{
			double again = 0;
			CHECK(ParseNumber(Shortest(value), again) && again == value);
		}
	});

	Test("Thousands 는 정수로 맞춰 세 자리마다 쉼표를 넣는다", [] {
		// Shortest 는 큰 수를 지수로 쓴다(100000 → "1e+05"). 창에 보이는 금화와 자원의 수는 이것으로 쓴다.
		CHECK_STR(Thousands(0), "0");
		CHECK_STR(Thousands(999), "999");
		CHECK_STR(Thousands(1000), "1,000");
		CHECK_STR(Thousands(3600.4), "3,600");
		CHECK_STR(Thousands(100000), "100,000");
		CHECK_STR(Thousands(1e6), "1,000,000");
		CHECK_STR(Thousands(-1250000), "-1,250,000");
		CHECK_STR(Thousands(-0.2), "0");
		CHECK_STR(Thousands(std::numeric_limits<double>::infinity()), "inf");
	});

	Test("ScriptRoutineName 은 접두 없는 이름에 gml_Script_ 를 붙인다", [] {
		// 접두 없는 이름은 러너에서 다른 루틴을 가리킨다(research/07). 부르거나 훅을 걸 이름은 하나뿐이어야 한다.
		CHECK_STR(ScriptRoutineName("budget_money_get"), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("gml_Script_budget_money_get"), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("  budget_money_get "), "gml_Script_budget_money_get");
		CHECK_STR(ScriptRoutineName("gml_Script_"), "");
		CHECK_STR(ScriptRoutineName(""), "");
		CHECK_STR(ScriptRoutineName("a b"), "");
	});

	Test("Has 는 글의 목록에 그 글이 있는지 본다", [] {
		// 특성의 목록에 이름이 있는지 보는 데 쓴다(가족·역할·인물이 같은 것을 따로 두고 있었다. 2026-10-07 리뷰).
		const std::vector<std::string> traits = { "beauty_pretty", "coward", "pregnant_st1" };
		CHECK(Has(traits, "coward"));
		CHECK(Has(traits, std::string("pregnant_st1")));
		CHECK(!Has(traits, "cowar"));
		CHECK(!Has(traits, ""));
		CHECK(!Has(std::vector<std::string>(), "coward"));
	});

	Test("IsUuid 는 소문자 16진수 열여섯 자만 받는다", [] {
		// 게임의 __soul.__uuid 와 세력의 uuid 가 그 꼴이다(외교·궁정·가족이 같은 검사를 따로 두고 있었다).
		CHECK(IsUuid("0123456789abcdef"));
		CHECK(!IsUuid("0123456789ABCDEF"));		// 대문자
		CHECK(!IsUuid("0123456789abcde"));		// 열다섯 자
		CHECK(!IsUuid("0123456789abcdef0"));		// 열일곱 자
		CHECK(!IsUuid("0123456789abcdeg"));		// 16진수가 아닌 글자
		CHECK(!IsUuid(""));
		CHECK(!IsUuid("all"));
	});

	Test("QueueFullText 는 창의 명령 큐가 가득 찼을 때 결과 줄에 적을 글이다", [] {
		// 범죄·월드의 큐가 넘치면 아무 말 없이 버렸다(2026-10-07 리뷰 R3). 넘친 것을 사용자가 알게 한다.
		CHECK_STR(QueueFullText(8), "쌓인 명령이 8개를 넘어 받지 않았습니다. 잠시 뒤에 다시 누르세요");
		CHECK_STR(QueueFullText(4), "쌓인 명령이 4개를 넘어 받지 않았습니다. 잠시 뒤에 다시 누르세요");
	});

	Test("ScopedFlag 는 사는 동안 깃발을 세우고 죽으면 내린다", [] {
		// 재진입 가드: 틱이 부른 게임의 함수가 틱을 다시 부르면 안쪽은 아무것도 하지 않는다(패널 다섯이 같은 구조체를 따로 두고 있었다).
		bool busy = false;
		{
			const ScopedFlag guard(busy);
			CHECK(busy);
			{
				const ScopedFlag inner(busy);		// 겹쳐도 사는 동안은 참
				CHECK(busy);
			}
			CHECK(!busy);		// 안쪽이 죽으면 내린다(겹쳐 쓰지 않는다: 바깥은 g_Busy 를 보고 들어오지 않는다)
		}
		CHECK(!busy);
	});
}
