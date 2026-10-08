#include "common.hpp"

void RunBuildingTests()
{
	Test("건물의 거주 칸: 병영은 건물 종류의 이름으로 알아본다", [] {
		// 게임이 주는 건물 종류의 이름(171개 가운데. research/09 의 목록)
		for (const char* name : { "barrack_10x6", "barrack_6x10", "barrack_10x6_grade_2", "barrack_6x10_grade_3" })
			CHECK(IsBarracksName(name));
	});

	Test("건물의 거주 칸: 노예 막사와 주택·영주 저택·영주관은 병영이 아니다", [] {
		for (const char* name : { "slaves_barrack_up", "slaves_barrack_bottom", "hut_8x8", "hut_10x6_grade_5", "hunter_hut",
				"lord_house_6x10", "stone_lord_house_10x6", "stone_hall", "castle" })
			CHECK(!IsBarracksName(name));
		// 이름의 앞이 통째로 "barrack_" 일 때만이다
		for (const char* name : { "", "barrack", "barracks_soldiers", "Barrack_10x6", "my_barrack_10x6" })
			CHECK(!IsBarracksName(name));
	});
}
