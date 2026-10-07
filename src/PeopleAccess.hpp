#pragma once
// 사람(o_character 영주·손님, o_dummy 주민·노예)의 자료를 읽고 쓰는 도우미(research/11). 상태는 로그 함수뿐이다.
// 사람은 __soul.__uuid 로 가리킨다. 자리의 번호는 사람이 드나들면 밀린다: 쓰기 전에 StillThere 로 그 자리의 uuid 를 다시 본다.
// 2026-10-07 리팩토링 C 에서 src/People.cpp 에서 옮겼다. src/People*.cpp, src/Shield.cpp, src/Hold.cpp 가 쓴다.

#include "core/AskPath.hpp"
#include "core/PeoplePlan.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlPeopleAccess
{
	using LogFn = std::function<void(const std::string&)>;
	constexpr double k_Unknown = -1e9;		// 읽지 못한 수

	void Init(LogFn Log);

	std::string Base(const NlCore::PersonRow& Row);					// "inst:o_character:3" 꼴
	bool FollowString(const YYTK::RValue& From, const std::vector<NlCore::PathStep>& Steps, std::string& Out);
	bool FollowNumber(const YYTK::RValue& From, const std::vector<NlCore::PathStep>& Steps, double& Out);
	bool ReadSoul(const NlCore::PersonRow& Row, YYTK::RValue& Soul);	// 그 자리의 __soul
	bool CallNoArgs(const std::string& Path, YYTK::RValue& Result, std::string& Why);
	bool CallNumber(const std::string& Path, double& Out);
	// 그 자리에 아직 그 사람이 있는가(uuid 가 같다). 있으면 Soul 을 채운다.
	bool StillThere(const NlCore::PersonRow& Row, YYTK::RValue& Soul);
	void ReadNumbers(const YYTK::RValue& Soul, const std::vector<NlCore::PathStep>& Steps, std::vector<double>& Out);
	bool ReadTraits(const YYTK::RValue& Soul, std::vector<std::string>& Out);
	void ReadEquipped(const YYTK::RValue& Soul, std::vector<double>& Out);
	// 수를 쓰고 다시 읽어 확인한다. 실패하면 Note 에 까닭.
	bool WriteAt(const std::string& Path, double Value, std::string& Note);
	double NeedLimit(const std::string& Soul, int Index);
	bool SetNeed(const std::string& Soul, int Index, double Asked, std::string& Note);
	// 특성을 뗀다·붙인다(게임의 Traits.trait_detach·trait_attach). 붙인 뒤에는 Soul 을 다시 읽는다.
	bool Detach(const NlCore::PersonRow& Row, const std::string& Name, std::string& Note);
	bool Attach(const NlCore::PersonRow& Row, const std::string& Name, YYTK::RValue& Soul, std::string& Note);
}
