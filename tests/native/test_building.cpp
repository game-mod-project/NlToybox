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

	Test("건물의 효과: 값의 부호로 좋은 것과 나쁜 것을 가른다", [] {
		// 게임에서 본 값(research/32): 교회 7, 작은 교회 5, 제단·공공장소 3 / 대장간·용광로·벌목장 -4, 훈련장 -8
		for (const double value : { 7.0, 5.0, 3.0, 0.5 })
			CHECK(EffectSideOf(value) == EffectSide::Good);
		for (const double value : { -4.0, -8.0, -0.5 })
			CHECK(EffectSideOf(value) == EffectSide::Bad);
		CHECK(EffectSideOf(0) == EffectSide::None);
		CHECK(EffectSideOf(std::numeric_limits<double>::quiet_NaN()) == EffectSide::None);
		CHECK(EffectSideOf(std::numeric_limits<double>::infinity()) == EffectSide::None);
	});

	Test("건물의 효과: 처음 본 부호를 기억한다(0 으로 써 둔 뒤에도 나쁜 효과로 안다)", [] {
		EffectSides sides;
		CHECK(sides.Of("forge", -4) == EffectSide::Bad);
		CHECK(sides.Of("forge", 0) == EffectSide::Bad);			// 모듈이 0 으로 써 둔 값을 다시 봤다: 되돌릴 자리를 놓치지 않는다
		CHECK(sides.Of("church", 7) == EffectSide::Good && sides.Of("church", 0) == EffectSide::Good);
		CHECK(sides.Of("forge", 9) == EffectSide::Bad);			// 기억한 것은 바뀌지 않는다
		CHECK(sides.Of("statue_1", 0) == EffectSide::None);		// 처음부터 0 인 것은 기억하지 않는다
		CHECK(sides.Of("statue_1", 5) == EffectSide::Good);		// 나중에 값이 보이면 그때 기억한다
		CHECK(sides.Of("statue_1", -5) == EffectSide::Good);
	});

	Test("건물 자료의 일: 게임 화면 밖에서는 게임이 자료를 올린 뒤에만 한다", [] {
		// JobMayRun(게임 화면인가, 메뉴에서도 쓰는 일인가, 게임이 건물 자료를 올렸는가)
		// 게임 화면에서는 언제나 한다.
		CHECK(JobMayRun(true, false, true) && JobMayRun(true, true, true) && JobMayRun(true, false, false));
		// 게임 화면이 아니면 보통의 일은 하지 않는다(자료가 있어도).
		CHECK(!JobMayRun(false, false, true) && !JobMayRun(false, false, false));
		// 메뉴에서도 쓰는 일(좋은 효과의 범위): 메인 메뉴에서는 한다. 게임이 건물 자료를 올린 뒤다.
		CHECK(JobMayRun(false, true, true));
		// 부팅 중에는 하지 않는다. 그 항목을 켠 채 저장해 두고 게임을 켜자 첫 틱에 건물 종류를 얻는 스크립트를 불러 게임이 끝났다
		// ("I32 argument is unset": 스크립트가 읽는 global.__building_storage 가 아직 없었다. 2026-10-08. research/34).
		CHECK(!JobMayRun(false, true, false));
	});

	Test("건물 자료의 일: 걸었는데 쓸 자리가 하나도 없으면 다 쓴 것이 아니다", [] {
		// WalkFoundNothing(되돌리는 중인가, 바라는 값을 가진 자리의 수, 써지지 않은 자리의 수)
		// 부팅 중에 건물의 저장소(global.__building_storage)는 생겼는데 종류가 아직 채워지지 않은 때가 있었다: 첫 걷기가 효과 0개를 찾았고
		// 15초 뒤의 걷기가 8칸을 썼다(2026-10-08. research/34). 그 첫 걷기를 "다 썼다"로 치지 않고 곧 다시 걷는다.
		CHECK(WalkFoundNothing(false, 0, 0));
		// 쓴 자리가 있으면 다 쓴 것이다. 써지지 않은 자리가 있으면 그것은 실패로 따로 다룬다.
		CHECK(!WalkFoundNothing(false, 8, 0) && !WalkFoundNothing(false, 0, 2) && !WalkFoundNothing(false, 3, 1));
		// 되돌리는 중에는 자리가 없어도 된다(되돌릴 것이 없다).
		CHECK(!WalkFoundNothing(true, 0, 0) && !WalkFoundNothing(true, 5, 0));
	});
}
