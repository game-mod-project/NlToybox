#include "common.hpp"

void RunWorldTests()
{
	Test("월드: 한 번 하는 일과 원격 명령", [] {
		WorldAct act = WorldAct::CooldownsClear;
		CHECK(ParseWorldAct("bishop", act) && act == WorldAct::BishopSend);
		CHECK(ParseWorldAct("cooldowns_clear", act) && act == WorldAct::CooldownsClear);
		CHECK(!ParseWorldAct("ambush", act) && !ParseWorldAct("", act));		// 궁수 매복은 불러서 게임이 끝났다(research/14). 넣지 않는다
		CHECK(std::string(WorldActWord(WorldAct::BishopSend)) == "bishop" && std::string(WorldActWord(WorldAct::CooldownsClear)) == "cooldowns_clear");
		// 계절의 일(research/25): 보기, 미루기, 지금 단계 끝내기. 날씨를 일으키는 함수(__start_rain 들)는 꼴을 보지 못해 넣지 않는다.
		CHECK(ParseWorldAct("season", act) && act == WorldAct::SeasonShow);
		CHECK(ParseWorldAct("season_delay", act) && act == WorldAct::SeasonDelay);
		CHECK(ParseWorldAct("season_end", act) && act == WorldAct::SeasonEnd);
		CHECK(std::string(WorldActWord(WorldAct::SeasonShow)) == "season" && std::string(WorldActWord(WorldAct::SeasonEnd)) == "season_end");
		CHECK(!ParseWorldAct("rain", act) && !ParseWorldAct("season_", act));
		// 지금 저장(save)과 이벤트 골라 일으키기(event. 이름을 받는다). 2026-10-07, research/28.
		CHECK(ParseWorldAct("save", act) && act == WorldAct::SaveNow && std::string(WorldActWord(WorldAct::SaveNow)) == "save");
		CHECK(ParseWorldAct("event", act) && act == WorldAct::EventForce && std::string(WorldActWord(WorldAct::EventForce)) == "event");
		CHECK(WorldActNeedsName(WorldAct::EventForce) && !WorldActNeedsName(WorldAct::SaveNow) && !WorldActNeedsName(WorldAct::BishopSend));
		// 낱말의 목록(틀린 낱말에 답할 글)은 표에서 만든다.
		CHECK(WorldActWords() == "cooldowns_clear, bishop, season, season_delay, season_end, save, event");
		// 게임의 자료를 바꾸는 일인가(보기는 읽기만 한다).
		CHECK(!WorldActChanges(WorldAct::SeasonShow) && WorldActChanges(WorldAct::SeasonDelay) && WorldActChanges(WorldAct::SeasonEnd)
			&& WorldActChanges(WorldAct::CooldownsClear) && WorldActChanges(WorldAct::BishopSend) && WorldActChanges(WorldAct::SaveNow) && WorldActChanges(WorldAct::EventForce));
		// 이벤트의 이름: 게임의 시스템 이름(글자·숫자·밑줄). 빈 글과 공백·점은 받지 않는다.
		CHECK(GoodEventName("u_guest_bard") && GoodEventName("raid_bandits") && !GoodEventName("") && !GoodEventName("u guest") && !GoodEventName("a.b") && !GoodEventName("name=x"));
		// 원격: world save, world event name=<이름>. event 는 이름이 꼭 있어야 하고, 다른 일은 이름을 받지 않는다.
		CHECK(ParseRemoteLine("world save").Error.empty() && ParseRemoteLine("world save").Target == "save");
		const RemoteCommand forced = ParseRemoteLine("world event name=u_guest_bard");
		CHECK(forced.Error.empty() && forced.Target == "event" && forced.Options.at("name") == "u_guest_bard");
		CHECK(!ParseRemoteLine("world event").Error.empty() && !ParseRemoteLine("world event name=").Error.empty() && !ParseRemoteLine("world bishop name=x").Error.empty());
		// 지금 저장의 글: 게임의 저장이 꺼져 있으면 부르지 않는다. 불렀으면 새 파일의 이름을 적고, 아직 없으면 그렇게 말한다.
		CHECK_STR(SaveNowReport('d', ""), "게임의 저장이 꺼져 있어 저장하지 않았습니다 (유틸의 '게임의 저장 끄기'를 끈 뒤에)");
		CHECK_STR(SaveNowReport('u', ""), "게임의 저장이 꺼져 있는지 읽지 못해 저장하지 않았습니다");
		CHECK_STR(SaveNowReport('f', "no such script"), "게임의 저장 함수를 부르지 못했습니다 (no such script)");
		CHECK_STR(SaveNowReport('n', ""), "게임의 저장 함수를 불렀습니다 (새 파일은 아직 보이지 않습니다)");
		CHECK_STR(SaveNowReport('s', "A_Autosave_Morning_day_8.norland"), "저장했습니다: A_Autosave_Morning_day_8.norland");
		// 이벤트 일으키기의 글: 없는 이름, 쓰지 못함, 써 둠(감독이 다음에 뽑을 때 고른다. research/28: 써 둔 u_guest_bard 가 그날 뽑혀 쿨다운에 올랐다).
		CHECK_STR(ForceEventReport("u_guest_bard", 'n', ""), "u_guest_bard: 게임에 그 이름의 이벤트가 없습니다");
		CHECK_STR(ForceEventReport("u_guest_bard", 'w', "why"), "u_guest_bard: 강제 이벤트에 쓰지 못했습니다 (why)");
		CHECK_STR(ForceEventReport("u_guest_bard", 'd', ""), "u_guest_bard: 강제 이벤트로 써 두었습니다. 게임의 감독이 다음에 이벤트를 뽑을 때(하루 한 번, 오후) 이것을 고릅니다");

		// 이벤트 쿨다운 한 칸: 0 보다 큰 수에만 0 을 쓴다
		CHECK(ShouldClearCooldown(true, 19) && ShouldClearCooldown(true, 0.5));
		CHECK(!ShouldClearCooldown(true, 0) && !ShouldClearCooldown(true, -4) && !ShouldClearCooldown(false, 19));
		CHECK(!ShouldClearCooldown(true, std::numeric_limits<double>::quiet_NaN()));

		// 주교: 있는지 읽지 못했거나 이미 있으면 부르지 않는다
		CHECK(ChooseBishopStep(true, false) == BishopStep::Call);
		CHECK(ChooseBishopStep(true, true) == BishopStep::AlreadyHere);
		CHECK(ChooseBishopStep(false, false) == BishopStep::Unknown && ChooseBishopStep(false, true) == BishopStep::Unknown);

		// 쿨다운 지우기의 보고: 쓴 것, 쓰지 못한 것, 열지 못한 것을 그대로 적는다(검토의 지적)
		const ClearResult none;									// 열지 못했다
		const ClearResult two{ true, 2, 0 }, one{ true, 1, 0 }, empty{ true, 0, 0 }, part{ true, 1, 2 };
		CHECK(CooldownReport(two, one) == "이벤트 쿨다운 2개와 묶음 쿨다운 1개를 0 으로 썼습니다");
		CHECK(CooldownReport(empty, empty) == "지울 쿨다운이 없습니다 (0 보다 큰 칸이 없습니다)");
		CHECK(CooldownReport(part, one) == "이벤트 쿨다운 1개와 묶음 쿨다운 1개를 0 으로 썼습니다. 쓰지 못한 칸: 이벤트 2, 묶음 0");
		CHECK(CooldownReport(two, none) == "이벤트 쿨다운 2개를 0 으로 썼습니다. 묶음 쿨다운은 읽지 못했습니다");		// 앞의 쓰기를 숨기지 않는다
		CHECK(CooldownReport(none, one) == "묶음 쿨다운 1개를 0 으로 썼습니다. 이벤트 쿨다운은 읽지 못했습니다");
		CHECK(CooldownReport(none, none) == "이벤트 쿨다운을 읽지 못했습니다");
		CHECK(CooldownTouched(two, none) && CooldownTouched(none, part) && CooldownTouched(ClearResult{ true, 0, 3 }, empty));
		CHECK(!CooldownTouched(empty, empty) && !CooldownTouched(none, none));

		RemoteCommand c = ParseRemoteLine("world bishop");
		CHECK(c.Error.empty() && c.Verb == "world" && c.Target == "bishop");
		CHECK(ParseRemoteLine("world cooldowns_clear").Error.empty());
		for (const char* word : { "season", "season_delay", "season_end" })
			CHECK(ParseRemoteLine(std::string("world ") + word).Error.empty() && ParseRemoteLine(std::string("world ") + word).Target == word);
		CHECK(ParseRemoteLine("world rain").Error.find("season_delay") != std::string::npos);		// 틀린 낱말에는 되는 낱말을 알려 준다
		CHECK(!ParseRemoteLine("world").Error.empty() && !ParseRemoteLine("world ambush").Error.empty() && !ParseRemoteLine("world bishop now").Error.empty());
	});

	Test("계절: 남은 시간의 글, 미루기와 끝내기에 쓸 시작 시각, 붙들기", [] {
		// 게임은 지금 단계의 남은 시간을 시작 시각에서 셈한다(research/25: 시작을 86400 뒤로 쓰자 남은 시간이 86400 늘었다).
		// 잰 값: 지금 537321.83, 시작 374400, 단계의 길이 345600 일 때 게임의 함수가 182678.17 을 돌려줬다.
		CHECK(std::fabs(PhaseRemain(537321.83, 374400, 345600) - 182678.17) < 0.01);

		// 남은 시간의 글: 날과 시간. 한 시간이 안 되면 그렇게 말하고, 셈할 수 없는 수는 물음표다.
		CHECK(SpanText(528278.17) == "6일 2시간");
		CHECK(SpanText(86400) == "1일" && SpanText(90000) == "1일 1시간" && SpanText(7200) == "2시간" && SpanText(3599) == "1시간 미만" && SpanText(0) == "1시간 미만");
		CHECK(SpanText(-5) == "0" && SpanText(std::numeric_limits<double>::infinity()) == "?" && SpanText(std::nan("")) == "?");

		// 미루기: 시작 시각을 뒤로 민다. 지금보다 뒤의 시각은 쓰지 않는다(지나간 시간이 음수가 되지 않게). 밀 것이 없으면 쓰지 않는다.
		double out = 0;
		CHECK(DelaySeasonStart(537321.83, 374400, 86400, out) && out == 460800);
		CHECK(DelaySeasonStart(537321.83, 500000, 86400, out) && out == 537321.83);		// 지금까지만
		CHECK(!DelaySeasonStart(537321.83, 537321.83, 86400, out));						// 이미 지금이다
		CHECK(!DelaySeasonStart(537321.83, 600000, 86400, out));						// 시작이 지금보다 뒤다: 건드리지 않는다
		CHECK(!DelaySeasonStart(537321.83, 374400, 0, out) && !DelaySeasonStart(537321.83, 374400, -5, out));
		CHECK(!DelaySeasonStart(std::nan(""), 374400, 86400, out) && !DelaySeasonStart(537321.83, std::nan(""), 86400, out)
			&& !DelaySeasonStart(537321.83, 374400, std::numeric_limits<double>::infinity(), out));

		// 미룬 결과의 글: 청한 만큼 밀렸으면 그만큼, 지금에 막혀 덜 밀렸으면 실제로 밀린 만큼을 말한다(검토 I2: 30분만 밀고 "하루 미뤘습니다"라고 했다).
		CHECK(DelayReport(86400, 86400) == "1일 미뤘습니다.");
		CHECK(DelayReport(1800, 86400) == "1시간 미만만 미뤘습니다(이 단계의 시작이 지금이 됐습니다. 더는 밀 수 없습니다).");
		CHECK(DelayReport(18000, 86400) == "5시간만 미뤘습니다(이 단계의 시작이 지금이 됐습니다. 더는 밀 수 없습니다).");

		// 끝내기는 0 보다 앞의 시작 시각을 쓰지 않는다: 게임을 시작한 지 단계의 길이만큼 지나지 않았으면 끝낼 수 없다(그런 시작 시각을 게임에서 본 적이 없다).
		CHECK(PhaseEndTooEarly(200000, 345600, 60) && !PhaseEndTooEarly(345540, 345600, 60) && !PhaseEndTooEarly(537321.83, 345600, 60));
		CHECK(!EndPhaseStart(200000, 100000, 345600, 60, out));
		CHECK(EndPhaseStart(345540, 100000, 345600, 60, out) && out == 0);

		// 끝내기: 남은 시간이 Lead 초가 되게 시작 시각을 당긴다. 이미 그만큼밖에 남지 않았으면 쓰지 않는다.
		CHECK(EndPhaseStart(537321.83, 374400, 345600, 60, out) && std::fabs(PhaseRemain(537321.83, out, 345600) - 60) < 1e-6 && out < 374400);
		CHECK(!EndPhaseStart(537321.83, 191781.83, 345600, 60, out));					// 남은 시간이 딱 60초
		CHECK(!EndPhaseStart(537321.83, 100000, 345600, 60, out));						// 이미 지났다
		CHECK(!EndPhaseStart(537321.83, 374400, 0, 60, out) && !EndPhaseStart(537321.83, 374400, -1, 60, out));		// 길이를 읽지 못했다
		CHECK(!EndPhaseStart(537321.83, 374400, 345600, -1, out) && !EndPhaseStart(std::nan(""), 374400, 345600, 60, out));

		// 붙들기: 켠 뒤 처음 본 "지나간 시간"을 기억하고, 그 시간이 그대로이게 시작 시각을 따라 민다.
		const PlaceKey here{ 0x1000, 7 }, other_map{ 0x2000, 7 }, other_game{ 0x1000, 9 };		// 관리자 구조체의 주소와 지도 관리 인스턴스(지어낸 수)
		SeasonHold hold;
		CHECK(!StepSeasonHold(hold, true, 1000, 400, 0, here, out) && hold.Has && hold.Elapsed == 600);		// 처음 본 틱에는 쓰지 않는다
		CHECK(!StepSeasonHold(hold, true, 1000.4, 400, 0, here, out));										// 1초가 안 되는 차이는 쓰지 않는다
		CHECK(StepSeasonHold(hold, true, 1060, 400, 0, here, out) && out == 460);							// 60초가 흘렀다: 시작을 60 뒤로
		CHECK(!StepSeasonHold(hold, true, 1060, 460, 0, here, out));											// 쓴 뒤에는 쓸 것이 없다
		// 단계가 바뀌었거나(게임이 넘겼다, 또는 '끝내기'를 눌렀다) 시각이 거꾸로 갔으면(다른 세이브) 다시 기억한다.
		CHECK(!StepSeasonHold(hold, true, 1100, 1090, 1, here, out) && hold.Elapsed == 10 && hold.Phase == 1);
		CHECK(!StepSeasonHold(hold, true, 500, 100, 1, here, out) && hold.Elapsed == 400);
		// 자리가 바뀌었으면 다시 기억한다: 같은 단계, 더 뒤의 시각이라도 다른 세이브(지도 관리 인스턴스가 다르다)나 다른 지도(관리자 구조체가 다르다)의 것이면
		// 앞의 게임에서 기억한 "지나간 시간"으로 쓰지 않는다(검토 I1: 3일 지난 게임에서 붙들다가 6시간 지난 세이브를 불러오면 그 세이브의 계절이 당겨졌다).
		SeasonHold loaded;
		CHECK(!StepSeasonHold(loaded, true, 1000, 400, 1, here, out) && loaded.Elapsed == 600);
		CHECK(!StepSeasonHold(loaded, true, 5000, 4900, 1, other_game, out) && loaded.Elapsed == 100);		// 쓰지 않고 새 값을 기억한다
		CHECK(StepSeasonHold(loaded, true, 5060, 4900, 1, other_game, out) && out == 4960);					// 그 뒤로는 새 게임의 값으로 붙든다
		CHECK(!StepSeasonHold(loaded, true, 5060, 300, 1, other_map, out) && loaded.Elapsed == 4760);			// 다른 지도의 관리자
		// 쓰는 값은 지금보다 뒤가 아니다(기억한 지나간 시간은 음수가 아니다).
		for (double now : { 1000.0, 1300.0, 9000.0 })
			if (StepSeasonHold(loaded, true, now + 6000, 300, 1, other_map, out))
				CHECK(out <= now + 6000);

		// 시작 시각이 지금보다 뒤로 읽히면 지나간 시간을 0 으로 본다.
		SeasonHold odd;
		CHECK(!StepSeasonHold(odd, true, 100, 300, 0, here, out) && odd.Elapsed == 0);
		CHECK(StepSeasonHold(odd, true, 100, 300, 0, here, out) && out == 100);
		// 끄면 잊는다. 꺼진 동안에는 쓰지 않는다. 읽지 못한 수로는 아무것도 하지 않는다.
		CHECK(!StepSeasonHold(hold, false, 2000, 100, 1, here, out) && !hold.Has);
		CHECK(!StepSeasonHold(hold, true, std::nan(""), 100, 1, here, out) && !hold.Has);
		// 밖에서 시작 시각을 바꿨으면(미루기) 잊게 한다: 다음 틱에 새 값을 기억한다.
		SeasonHold moved;
		StepSeasonHold(moved, true, 1000, 400, 0, here, out);
		ForgetSeasonHold(moved);
		CHECK(!StepSeasonHold(moved, true, 1000, 900, 0, here, out) && moved.Elapsed == 100);

		// 쓴 뒤의 확인: 게임의 함수가 돌려주는 남은 시간이 바라던 쪽으로 움직였는가(1초 넘게). 시작 시각이 써졌다는 것만으로 됐다고 하지 않는다.
		CHECK(RemainMoved(true, 182678, 269078) && !RemainMoved(true, 182678, 182678) && !RemainMoved(true, 182678, 182678.5) && !RemainMoved(true, 182678, 100));
		CHECK(RemainMoved(false, 182678, 60) && !RemainMoved(false, 182678, 182678) && !RemainMoved(false, 60, 182678));
		CHECK(!RemainMoved(true, std::nan(""), 5) && !RemainMoved(false, 5, std::nan("")));

		// 상태의 글: 가혹한 계절이 아닐 때는 올 때까지, 가혹한 계절일 때는 끝날 때까지. 이름이 없으면 이름 없이. (이름은 지어낸 것이다)
		CHECK(SeasonLine(false, "별비", 528278.17, 873878.17) == "가혹한 계절(별비)까지 6일 2시간");
		CHECK(SeasonLine(true, "별비", 0, 100000) == "가혹한 계절(별비) 중입니다. 끝나기까지 1일 3시간");
		CHECK(SeasonLine(false, "", 7200, 0) == "가혹한 계절까지 2시간");
		// 광산의 매장량 붙들기: 켠 동안 광산마다 본 가장 큰 값을 기억하고, 줄었으면 그 값으로 되돌려 쓴다(게임이 캘 때마다 1 씩 줄였다: 18 -> 17).
		std::map<std::string, double> kept;
		CHECK(!KeepStock(kept, "69_156", 18, out) && kept["69_156"] == 18);		// 처음 본 값은 기억만 한다
		CHECK(KeepStock(kept, "69_156", 17, out) && out == 18);					// 줄었다: 되돌려 쓴다
		CHECK(!KeepStock(kept, "69_156", 18, out));								// 그대로다
		CHECK(!KeepStock(kept, "69_156", 25, out) && kept["69_156"] == 25);		// 늘었으면 그 값을 기억한다
		CHECK(!KeepStock(kept, "12_40", 3, out) && KeepStock(kept, "12_40", 0, out) && out == 3 && kept.size() == 2);		// 광산마다 따로
		CHECK(!KeepStock(kept, "x", std::nan(""), out) && !KeepStock(kept, "y", -1, out) && kept.size() == 2);			// 수가 아니거나 음수인 칸은 건드리지 않는다
		// 켤 때 이미 0 인 광산은 0 을 기억한다(되살리지 않는다).
		CHECK(!KeepStock(kept, "empty", 0, out) && kept["empty"] == 0 && !KeepStock(kept, "empty", 0, out));
		// 기억한 수가 어느 게임·어느 지도의 것인지: 자리가 바뀌었거나 시각이 거꾸로 갔으면 모두 잊는다(같은 자리의 열쇠를 가진 다른 세이브의 광산에 앞의 수를 쓰지 않는다).
		StockBook book;
		EnterStockPlace(book, here, 1000);
		KeepStock(book.Kept, "69_156", 18, out);
		EnterStockPlace(book, here, 1060);
		CHECK(book.Kept.size() == 1);											// 같은 자리, 시각이 앞으로: 그대로
		EnterStockPlace(book, other_game, 2000);
		CHECK(book.Kept.empty() && !KeepStock(book.Kept, "69_156", 5, out));		// 다른 세이브: 5 를 18 로 되돌려 쓰지 않는다
		EnterStockPlace(book, other_map, 2100);
		CHECK(book.Kept.empty());
		KeepStock(book.Kept, "1_1", 9, out);
		EnterStockPlace(book, other_map, 500);
		CHECK(book.Kept.empty());												// 시각이 거꾸로 갔다

		// 단계의 글: 게임의 단계는 0 부터다. 창에는 1 부터 센다.
		CHECK(PhaseNote(0, 182678.17) == "단계 1, 이 단계는 2일 2시간 남음" && PhaseNote(2, 3000) == "단계 3, 이 단계는 1시간 미만 남음");
	});
}
