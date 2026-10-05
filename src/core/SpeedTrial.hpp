#pragma once
// 게임 속도의 손잡이를 시험으로 고른다. 러너에 기대지 않는다: 값을 읽고 쓰는 일은 바깥이 한다. 스펙: 치트 메뉴 §9.
// 후보 하나마다: 원래 값을 적어 두고, HoldSeconds 동안 시험 값을 계속 쓰게 하고, 흐름이 기대만큼 달라졌으면 고른다.
// 고른 뒤에는 쓰기를 멈추게 하고 SettleSeconds 뒤에도 흐름이 남는지 본다(남으면 한 번 쓰기, 아니면 계속 쓰기).
// 시험 값은 요청한 배율이 아니라 지금 time_warp 의 2배다. 요청한 배율이 지금 속도와 같으면 "달라지지 않았다"와
// "먹었다"를 가릴 수 없기 때문이다.

#include <vector>

namespace NlCore
{
	struct SpeedStep			// Tick 이 바깥에 시키는 일
	{
		enum class Kind { None, Write, Restore };
		Kind What = Kind::None;
		int Candidate = -1;
		double Value = 0;
	};

	struct SpeedAttempt			// 후보 하나를 시험한 기록(로그에 적는다)
	{
		int Candidate = -1;
		bool Readable = false;	// 값을 읽을 수 있었는가
		double Old = 0;			// 시험 전의 값
		double Rate = 0;		// 시험 끝의 흐름
		bool Matched = false;
	};

	class SpeedTrial
	{
	public:
		enum class Phase { Idle, Holding, Settling, Done, Failed, Aborted };

		explicit SpeedTrial(int Candidates, double HoldSeconds = 2.0, double SettleSeconds = 1.5);

		// 시험을 시작한다. BaseRate: 지금의 흐름. BaseWarp: 지금의 time_warp.
		// 둘 중 하나라도 0 이하이거나(멈춰 있다) 이미 시험 중이면 거짓.
		bool Start(double BaseRate, double BaseWarp);

		// 0.1초마다 부른다. Current: 지금 후보(Candidate())의 지금 값. 읽지 못했으면 Readable 이 거짓.
		// 돌려주는 값은 바깥이 할 일이다: Write 는 그 값을 계속 써 넣기 시작하라, Restore 는 쓰기를 멈추고 그 값으로 되돌려라,
		// None 은 쓰기를 멈춰라.
		SpeedStep Tick(double Now, bool Readable, double Current, double Rate);

		Phase State() const { return m_Phase; }
		bool Running() const { return m_Phase == Phase::Holding || m_Phase == Phase::Settling; }
		int Candidate() const { return m_Candidate; }		// 지금 시험하는(또는 고른) 후보. 없으면 -1
		bool Sticky() const { return m_Sticky; }			// Done: 한 번 쓰면 남는가
		double SavedOld() const { return m_Old; }			// 고른 후보의 원래 값
		double ProbeWarp() const { return m_Probe; }		// 시험에 쓰는 값
		double UnitRate() const { return m_BaseWarp > 0 ? m_BaseRate / m_BaseWarp : 0; }	// 배율 1 의 흐름
		const std::vector<SpeedAttempt>& Attempts() const { return m_Attempts; }

	private:
		void Next();

		int m_Count;
		double m_Hold, m_Settle;
		Phase m_Phase = Phase::Idle;
		int m_Candidate = -1;
		bool m_Began = false;		// 지금 후보에 쓰기 시작했는가
		bool m_Sticky = false;
		double m_Old = 0, m_Until = 0;
		double m_BaseRate = 0, m_BaseWarp = 0, m_Probe = 0;
		std::vector<SpeedAttempt> m_Attempts;
	};
}
