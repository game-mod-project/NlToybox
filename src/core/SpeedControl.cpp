#include "SpeedControl.hpp"

#include "Text.hpp"

#include <cmath>
#include <utility>

namespace NlCore
{
	namespace
	{
		constexpr double k_SampleSeconds = 0.1;		// 흐름을 재고 시험을 한 걸음 나아가는 간격
		constexpr double k_HoldSeconds = 0.015;		// 계속 쓰기의 간격. 프레임마다 한 번쯤이다
		constexpr double k_GapSeconds = 0.3;		// 이보다 오래 재지 않았으면 흐름의 창을 비운다
	}

	SpeedControl::SpeedControl(SpeedPaths Paths, SpeedIo Io)
		: m_Paths(std::move(Paths)), m_Io(std::move(Io)), m_Rate(1.0), m_Trial(static_cast<int>(m_Paths.Handles.size()))
	{
	}

	void SpeedControl::Press(double Factor)
	{
		if (Factor <= 0 || m_Trial.Running())
			return;
		m_Wanted = Factor;
		m_Pressed = true;
	}

	void SpeedControl::Release()
	{
		m_ReleaseAsked = true;
	}

	void SpeedControl::Log(const std::string& Line) const
	{
		if (m_Io.Log)
			m_Io.Log(Line);
	}

	std::string SpeedControl::HandlePath(int Index) const
	{
		if (Index < 0 || Index >= static_cast<int>(m_Paths.Handles.size()))
			return "";
		const SpeedHandle& handle = m_Paths.Handles[Index];
		if (!handle.Path.empty())
			return handle.Path;

		double at = 0;
		if (!m_Io.Read(handle.IndexPath, at) || at < 0)
			return "";
		return handle.ArrayPath + "[" + Shortest(std::floor(at)) + "]";
	}

	// 쓰기 전의 값을 적어 둔다. 이미 적어 둔 자리는 그대로 둔다(처음 값이 원래 값이다).
	void SpeedControl::Remember(const std::string& Path)
	{
		double before = 0;
		if (!Path.empty() && !m_Saved.count(Path) && m_Io.Read(Path, before))
			m_Saved[Path] = before;
	}

	// 써 둔 자리를 모두 원래 값으로 되돌린다. Except 는 남긴다. 되돌리지 못한 것이 있으면 거짓.
	bool SpeedControl::RestoreAll(const std::string& Except)
	{
		bool ok = true;
		for (auto it = m_Saved.begin(); it != m_Saved.end();)
		{
			if (!Except.empty() && it->first == Except)
			{
				++it;
				continue;
			}
			std::string why;
			if (!m_Io.Write(it->first, it->second, why))
			{
				ok = false;
				Log("speed restore " + it->first + " = " + Shortest(it->second) + ": " + why);
			}
			it = m_Saved.erase(it);
		}
		return ok;
	}

	// 시간 컨트롤러가 없다(로딩 중, 메뉴로 나감). 써 둔 값은 컨트롤러와 함께 사라졌다.
	void SpeedControl::Lost()
	{
		m_Rate.Reset();
		m_LastSample = -1;
		m_Flow = 0;
		m_Warp = 0;
		m_Holding = false;
		m_Saved.clear();
		m_AppliedPath.clear();
		if (m_ReleaseAsked)
			m_Wanted = 0;
		m_Pressed = false;
		m_ReleaseAsked = false;

		if (m_Trial.Running())
		{
			m_Trial.Cancel();
			m_Wanted = 0;
			m_Note = "시간 컨트롤러가 사라져 시험을 그만두었습니다.";
			Log("speed trial cancelled: time controller gone");
		}
		m_Lost = m_Wanted > 0;		// 돌아오면 다시 건다
	}

	// 배율 단추를 눌렀다.
	void SpeedControl::Begin()
	{
		if (m_Trial.Running())
			return;
		if (m_Trial.State() == SpeedTrial::Phase::Done)
		{
			Apply();
			return;
		}

		m_Rate.Reset();		// 기준은 지금부터 잰다. 누르기 전의 표본에는 멈춰 있던 때가 섞여 있을 수 있다
		if (!m_Trial.Start(m_Warp))
		{
			m_Note = "시간이 멈춰 있어 손잡이를 시험할 수 없습니다. 게임 화면에서 일시정지를 풀고 다시 누르세요.";
			m_Wanted = 0;
			return;
		}
		m_AttemptsLogged = 0;
		m_TrialCandidate = -1;
		m_Note = "손잡이를 시험하는 중입니다(길어도 12초). 그동안 게임 속도가 잠깐 바뀝니다.";
		Log("speed trial start: time_warp " + Shortest(m_Warp) + ", probe " + Shortest(m_Trial.ProbeWarp()));
	}

	// 시험을 한 걸음 나아간다.
	void SpeedControl::Step(double Now)
	{
		// 후보의 자리는 그 후보를 시작할 때 한 번 정한다. 도중에 자리가 바뀌어도 쓴 자리를 되돌린다.
		if (m_TrialCandidate != m_Trial.Candidate())
		{
			m_TrialCandidate = m_Trial.Candidate();
			m_TrialPath = HandlePath(m_TrialCandidate);
		}
		double current = 0;
		const bool readable = !m_TrialPath.empty() && m_Io.Read(m_TrialPath, current);
		const SpeedStep step = m_Trial.Tick(Now, readable, current, m_Flow);

		if (step.What == SpeedStep::Kind::Write)
		{
			Remember(m_TrialPath);
			m_Holding = true;
			m_HoldPath = m_TrialPath;
			m_HoldValue = step.Value;
		}
		else
		{
			m_Holding = false;
			if (step.What == SpeedStep::Kind::Restore)
				RestoreAll();
		}

		for (; m_AttemptsLogged < m_Trial.Attempts().size(); m_AttemptsLogged++)
		{
			const SpeedAttempt& attempt = m_Trial.Attempts()[m_AttemptsLogged];
			Log("speed trial " + m_Paths.Handles[attempt.Candidate].Label + ": "
				+ (attempt.Readable ? "old " + Shortest(attempt.Old) + ", flow " + Fixed(attempt.Rate, 2) + "/s"
					+ (attempt.Matched ? " -> matched" : " -> no effect, restored") : "not readable"));
		}

		switch (m_Trial.State())
		{
		case SpeedTrial::Phase::Done:
			m_ChosenLabel = m_Paths.Handles[m_Trial.Candidate()].Label;
			Log("speed trial done: " + m_ChosenLabel + (m_Trial.Sticky() ? " (write once)" : " (keep writing)")
				+ ", unit flow " + Fixed(m_Trial.UnitRate(), 2) + "/s");
			Apply();
			break;
		case SpeedTrial::Phase::Failed:
			m_Note = "후보 가운데 속도를 바꾸는 것이 없었습니다. 시도마다의 값은 NlToyBox.log 에 있습니다.";
			m_Wanted = 0;
			Log("speed trial failed: unit flow " + Fixed(m_Trial.UnitRate(), 2) + "/s");
			break;
		case SpeedTrial::Phase::Aborted:
			m_Note = "시간이 멈춰 있어 시험을 그만두었습니다. 일시정지를 풀고 다시 누르세요.";
			m_Wanted = 0;
			Log("speed trial aborted: time stopped");
			break;
		default:
			break;
		}
	}

	// 고른 손잡이에 바라는 배율을 건다.
	void SpeedControl::Apply()
	{
		if (m_Wanted <= 0 || m_Trial.State() != SpeedTrial::Phase::Done)
			return;

		const std::string path = HandlePath(m_Trial.Candidate());
		if (path.empty())
		{
			m_Holding = false;
			m_Note = "손잡이의 자리를 읽지 못했습니다.";
			return;
		}
		RestoreAll(path);		// 다른 자리에 써 둔 것이 있으면 먼저 되돌린다(자리가 바뀌는 손잡이)
		Remember(path);

		std::string why;
		const bool ok = m_Io.Write(path, m_Wanted, why);
		m_AppliedPath = path;
		m_Holding = ok && !m_Trial.Sticky();
		m_HoldPath = path;
		m_HoldValue = m_Wanted;
		m_Note = ok ? "x" + Shortest(m_Wanted) + " 을 걸었습니다." : "써지지 않음: " + why;
		Log("speed x" + Shortest(m_Wanted) + " via " + m_ChosenLabel + " (" + path + ")" + (ok ? "" : ": " + why));
	}

	void SpeedControl::DoRelease()
	{
		m_ReleaseAsked = false;
		m_Pressed = false;
		m_Holding = false;
		m_Wanted = 0;
		m_AppliedPath.clear();

		if (m_Trial.Running())
		{
			m_Trial.Cancel();
			RestoreAll();
			m_Note = "시험을 그만두고 원래 값으로 되돌렸습니다.";
			Log("speed trial cancelled");
			return;
		}
		if (m_Saved.empty())
		{
			m_Note.clear();
			return;
		}
		const bool ok = RestoreAll();
		m_Note = ok ? "게임의 속도로 되돌렸습니다." : "되돌리지 못한 값이 있습니다(NlToyBox.log 참고).";
		Log("speed released");
	}

	void SpeedControl::Tick(double Now, bool Watching)
	{
		// 계속 쓰기는 프레임마다 한 번쯤 한다. 게임이 값을 자주 되돌리면 이래야 먹는다.
		if (m_Holding && Now >= m_NextHold)
		{
			m_NextHold = Now + k_HoldSeconds;
			std::string why;
			m_Io.Write(m_HoldPath, m_HoldValue, why);
		}

		const bool busy = m_Trial.Running() || m_Wanted > 0 || m_Pressed || m_ReleaseAsked;
		if (Now < m_NextSample || (!Watching && !busy))
			return;
		m_NextSample = Now + k_SampleSeconds;

		double game_time = 0;
		m_TimeFound = m_Io.Read(m_Paths.GameTime, game_time);
		if (!m_TimeFound)
		{
			Lost();
			return;
		}
		if (m_LastSample >= 0 && Now - m_LastSample > k_GapSeconds)
			m_Rate.Reset();		// 재지 않은 동안(창이 닫혀 있었다)을 흐름에 섞지 않는다
		m_LastSample = Now;
		m_Rate.Add(Now, game_time);
		m_Flow = m_Rate.PerSecond();
		if (!m_Io.Read(m_Paths.Warp, m_Warp))
			m_Warp = 0;

		if (m_Lost)
		{
			m_Lost = false;		// 컨트롤러가 다시 생겼다. 걸어 두었던 배율을 다시 건다
			if (!m_ReleaseAsked)
				Apply();
		}
		if (m_ReleaseAsked)
			DoRelease();
		if (m_Pressed)
		{
			m_Pressed = false;
			Begin();
		}

		if (m_Trial.Running())
			Step(Now);
		else if (m_Wanted > 0 && m_Trial.State() == SpeedTrial::Phase::Done)
		{
			// 자리가 바뀌는 손잡이(time_speed_variants 의 지금 자리)는 자리를 따라간다.
			const std::string path = HandlePath(m_Trial.Candidate());
			if (!path.empty() && path != m_AppliedPath)
				Apply();
		}
	}
}
