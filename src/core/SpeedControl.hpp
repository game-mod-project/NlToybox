#pragma once
// 게임 속도를 건다. 값을 읽고 쓰는 일은 바깥이 준 함수로 한다(러너에 기대지 않는다). 스펙: 치트 메뉴 §9.
// 흐름을 재고(Rate), 처음 누를 때 손잡이를 시험으로 고르고(SpeedTrial), 고른 손잡이에 배율을 써 넣는다.
// 이 모듈이 쓴 자리는 쓰기 전의 값과 함께 적어 둔다. 되돌릴 때는 그 자리에 그 값을 쓴다.

#include "Rate.hpp"
#include "SpeedTrial.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	struct SpeedIo
	{
		std::function<bool(const std::string& Path, double& Out)> Read;						// 수를 읽는다. 없으면 거짓
		std::function<bool(const std::string& Path, double Value, std::string& Why)> Write;	// 있는 자리에만 쓴다
		std::function<void(const std::string& Line)> Log;
	};

	struct SpeedHandle			// 속도를 정할지도 모르는 값 하나
	{
		std::string Label;
		std::string Path;		// 값의 주소. 비어 있으면 ArrayPath[IndexPath 의 값] 이다(자리가 바뀔 수 있다)
		std::string ArrayPath, IndexPath;
	};

	struct SpeedPaths
	{
		std::string GameTime;				// 흐르는 값
		std::string Warp;					// 지금의 배율(멈춰 있으면 0)
		std::vector<SpeedHandle> Handles;	// 시험하는 차례대로
	};

	class SpeedControl
	{
	public:
		SpeedControl(SpeedPaths Paths, SpeedIo Io);

		// 창에서 부른다. 값을 읽거나 쓰지 않는다(다음 Tick 이 한다).
		void Press(double Factor);		// 배율 단추
		void Release();					// 게임에 맡김. 시험 중이면 그만둔다

		// 게임 스레드에서 틱마다 부른다(스스로 간격을 둔다). Watching: 흐름을 보여 줄 창이 열려 있는가.
		void Tick(double Now, bool Watching);

		// 스냅샷(Tick 이 채운다)
		bool TimeFound() const { return m_TimeFound; }
		double Flow() const { return m_Flow; }				// 실제 1초에 게임 시간이 느는 양
		double Warp() const { return m_Warp; }
		double UnitRate() const { return m_Trial.UnitRate(); }	// 배율 1 의 흐름. 시험 전에는 0
		double Wanted() const { return m_Wanted; }			// 건(걸려는) 배율. 없으면 0
		bool Busy() const { return m_Trial.Running(); }		// 시험 중
		bool Chosen() const { return m_Trial.State() == SpeedTrial::Phase::Done; }
		bool Sticky() const { return m_Trial.Sticky(); }	// 고른 손잡이가 한 번 쓰면 남는가
		const std::string& ChosenLabel() const { return m_ChosenLabel; }
		const std::string& Note() const { return m_Note; }

	private:
		void Log(const std::string& Line) const;
		std::string HandlePath(int Index) const;
		void Remember(const std::string& Path);
		bool RestoreAll(const std::string& Except = "");
		void Lost();
		void Begin();
		void Step(double Now);
		void Apply();
		void DoRelease();

		SpeedPaths m_Paths;
		SpeedIo m_Io;
		Rate m_Rate;
		SpeedTrial m_Trial;

		double m_Wanted = 0;
		bool m_Pressed = false, m_ReleaseAsked = false;		// 창이 올리고 Tick 이 내린다
		bool m_Holding = false;								// 계속 써 넣는 중인가
		std::string m_HoldPath;
		double m_HoldValue = 0;
		double m_NextSample = 0, m_NextHold = 0, m_LastSample = -1;
		std::map<std::string, double> m_Saved;				// 이 모듈이 쓴 자리 → 쓰기 전의 값
		int m_TrialCandidate = -1;							// m_TrialPath 가 어느 후보의 것인가
		std::string m_TrialPath;							// 시험 중인 후보의 자리(그 후보를 시작할 때 정한다)
		std::string m_AppliedPath;							// 배율을 마지막으로 건 자리
		bool m_Lost = false;								// 배율을 건 채로 시간 컨트롤러가 사라졌다
		size_t m_AttemptsLogged = 0;

		bool m_TimeFound = false;
		double m_Flow = 0, m_Warp = 0;
		std::string m_ChosenLabel, m_Note;
	};
}
