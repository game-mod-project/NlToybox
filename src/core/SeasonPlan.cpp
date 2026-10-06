#include "SeasonPlan.hpp"

#include <cmath>

namespace NlCore
{
	namespace
	{
		constexpr double k_Hour = 3600, k_Day = 86400;

		bool Finite(double A, double B, double C = 0)
		{
			return std::isfinite(A) && std::isfinite(B) && std::isfinite(C);
		}
	}

	double PhaseRemain(double Now, double Start, double Duration)
	{
		return Duration - (Now - Start);
	}

	std::string SpanText(double Seconds)
	{
		if (!std::isfinite(Seconds))
			return "?";
		if (Seconds < 0)
			return "0";
		const long long days = static_cast<long long>(Seconds / k_Day);
		const long long hours = static_cast<long long>((Seconds - static_cast<double>(days) * k_Day) / k_Hour);
		if (days == 0 && hours == 0)
			return "1시간 미만";
		std::string out;
		if (days > 0)
			out = std::to_string(days) + "일";
		if (hours > 0)
			out += (out.empty() ? "" : " ") + std::to_string(hours) + "시간";
		return out;
	}

	bool DelaySeasonStart(double Now, double Start, double Seconds, double& Out)
	{
		if (!Finite(Now, Start, Seconds) || Seconds <= 0 || Start >= Now)
			return false;
		const double moved = Start + Seconds;
		Out = moved < Now ? moved : Now;
		return true;
	}

	bool EndPhaseStart(double Now, double Start, double Duration, double Lead, double& Out)
	{
		if (!Finite(Now, Start, Duration) || !std::isfinite(Lead) || Duration <= 0 || Lead < 0)
			return false;
		if (PhaseRemain(Now, Start, Duration) <= Lead)
			return false;
		Out = Now - Duration + Lead;
		return true;
	}

	bool StepSeasonHold(SeasonHold& Hold, bool On, double Now, double Start, double Phase, double& Write)
	{
		if (!On || !Finite(Now, Start, Phase))
		{
			Hold = SeasonHold{};
			return false;
		}
		if (!Hold.Has || Hold.Phase != Phase || Now < Hold.Seen)
		{
			Hold.Has = true;
			Hold.Elapsed = Now > Start ? Now - Start : 0;
			Hold.Phase = Phase;
			Hold.Seen = Now;
			return false;
		}
		Hold.Seen = Now;
		const double want = Now - Hold.Elapsed;
		if (std::fabs(want - Start) < 1)
			return false;
		Write = want;
		return true;
	}

	void ForgetSeasonHold(SeasonHold& Hold)
	{
		Hold = SeasonHold{};
	}

	bool RemainMoved(bool Later, double Before, double After)
	{
		if (!Finite(Before, After))
			return false;
		return Later ? After > Before + 1 : After < Before - 1;
	}

	std::string PhaseNote(double Phase, double Remain)
	{
		return "단계 " + std::to_string(static_cast<long long>(Phase) + 1) + ", 이 단계는 " + SpanText(Remain) + " 남음";
	}

	std::string SeasonLine(bool Extreme, const std::string& Name, double ToExtreme, double ToEnd)
	{
		const std::string what = Name.empty() ? std::string("가혹한 계절") : "가혹한 계절(" + Name + ")";
		return Extreme ? what + " 중입니다. 끝나기까지 " + SpanText(ToEnd) : what + "까지 " + SpanText(ToExtreme);
	}
}
