#include "PeopleAccess.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using NlCore::PersonRow;
using NlCore::Shortest;

namespace
{
	NlPeopleAccess::LogFn g_Log;

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}
}

void NlPeopleAccess::Init(LogFn Log_)
{
	g_Log = std::move(Log_);
}

namespace NlPeopleAccess
{
	std::string Base(const PersonRow& Row)
	{
		return std::string("inst:") + (Row.Character ? "o_character" : "o_dummy") + ":" + std::to_string(Row.Index);
	}

	bool FollowString(const RValue& From, const std::vector<PathStep>& Steps, std::string& Out)
	{
		RValue value;		// 이 함수 안에서만 든다
		std::string why;
		if (!NlAccess::Follow(From, Steps, value, why) || !value.IsString())
			return false;
		Out = value.ToString();
		return true;
	}

	bool FollowNumber(const RValue& From, const std::vector<PathStep>& Steps, double& Out)
	{
		RValue value;
		std::string why;
		if (!NlAccess::Follow(From, Steps, value, why) || !NlGame::IsNumber(value))
			return false;
		Out = value.ToDouble();
		return true;
	}

	// 그 사람의 __soul. 없으면 거짓.
	bool ReadSoul(const PersonRow& Row, RValue& Soul)
	{
		std::string why;
		return NlAccess::Read(NlCore::ParseAskPath(Base(Row) + ".__soul"), Soul, why) && Soul.IsStruct();
	}

	// 인자 없는 메서드를 부른다. 게임이 인자 없이 부르는 것을 기록한 것에만 쓴다(research/11).
	bool CallNoArgs(const std::string& Path, RValue& Result, std::string& Why)
	{
		return NlAccess::CallMethod(NlCore::ParseAskPath(Path), {}, Result, Why);
	}

	bool CallNumber(const std::string& Path, double& Out)
	{
		RValue result;
		std::string why;
		if (!CallNoArgs(Path, result, why) || !NlGame::IsNumber(result))
			return false;
		Out = result.ToDouble();
		return true;
	}

	// 그 자리의 인스턴스가 아직 그 사람인가(인물이 드나들면 n 번째가 다른 사람이 된다).
	bool StillThere(const PersonRow& Row, RValue& Soul)
	{
		std::string uuid;
		return ReadSoul(Row, Soul) && FollowString(Soul, { { '.', "__uuid", 0 } }, uuid) && uuid == Row.Uuid;
	}

	void ReadNumbers(const RValue& Soul, const std::vector<PathStep>& Steps, std::vector<double>& Out)
	{
		RValue list;
		std::string why;
		Out.clear();
		if (!NlAccess::Follow(Soul, Steps, list, why) || !list.IsArray())
			return;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& value) {
			Out.push_back(NlGame::IsNumber(value) ? value.ToDouble() : k_Unknown);
			return true;
		});
	}

	// 가진 특성의 이름들. 목록을 읽지 못하면 거짓(빈 목록과 가른다).
	bool ReadTraits(const RValue& Soul, std::vector<std::string>& Out)
	{
		RValue list;
		std::string why;
		Out.clear();
		if (!NlAccess::Follow(Soul, { { '.', "__traits", 0 }, { '.', "__list_of_traits", 0 } }, list, why) || !list.IsArray())
			return false;
		NlAccess::ForEachChild(list, Holder::Array, [&](const PathStep&, const RValue& name) {
			if (name.IsString())
				Out.push_back(name.ToString());
			return true;
		});
		return true;
	}

	// 착용 중인 장비의 자원 번호들(값 읽기). __soul.__equipment.__cached_armor·__cached_first_arm·__cached_second_arm 의 __resource(research/12: 6, 10, -1).
	void ReadEquipped(const RValue& Soul, std::vector<double>& Out)
	{
		Out.clear();
		for (const char* slot : { "__cached_armor", "__cached_first_arm", "__cached_second_arm" })
		{
			double resource = -1;
			FollowNumber(Soul, { { '.', "__equipment", 0 }, { '.', slot, 0 }, { '.', "__resource", 0 } }, resource);
			Out.push_back(resource);
		}
	}

	// 수 하나를 쓴다(있는 자리에만. 쓴 뒤 다시 읽어 확인한다).
	bool WriteAt(const std::string& Path, double Value, std::string& Note)
	{
		std::string why;
		if (NlAccess::WriteNumber(Path, Value, why))
			return true;
		Note = why;
		return false;
	}

	double NeedLimit(const std::string& Soul, int Index)
	{
		double limit = NlCore::k_NeedMax;
		NlAccess::ReadNumber(Soul + ".__motive.__motive_limit[" + std::to_string(Index) + "]", limit);
		return limit;
	}

	bool SetNeed(const std::string& Soul, int Index, double Asked, std::string& Note)
	{
		double value = 0;
		if (!NlCore::NeedValue(Asked, NeedLimit(Soul, Index), value))
		{
			Note = "욕구의 상한을 읽지 못했습니다";
			return false;
		}
		return WriteAt(Soul + ".__motive.__motive[" + std::to_string(Index) + "]", value, Note);
	}

	// 특성을 뗀다: Traits.trait_detach("이름"). 게임이 글 하나로 부르는 것을 기록했다.
	bool Detach(const PersonRow& Row, const std::string& Name, std::string& Note)
	{
		Log("people call trait_detach(" + Name + ") on " + Row.Uuid);		// 부르기 전에 남긴다
		RValue result;
		return NlAccess::CallMethod(NlCore::ParseAskPath(Base(Row) + ".__soul.__traits.trait_detach"), { RValue(std::string_view(Name)) }, result, Note);
	}

	// 특성을 붙인다: Traits.trait_attach("이름") -> uuid. 게임이 글 하나로 부르는 것을 기록했다. 붙인 뒤 다시 읽어 확인한다(Soul 은 다시 읽은 것이 된다).
	bool Attach(const PersonRow& Row, const std::string& Name, RValue& Soul, std::string& Note)
	{
		Log("people call trait_attach(" + Name + ") on " + Row.Uuid);		// 부르기 전에 남긴다
		RValue result;
		if (!NlAccess::CallMethod(NlCore::ParseAskPath(Base(Row) + ".__soul.__traits.trait_attach"), { RValue(std::string_view(Name)) }, result, Note))
			return false;
		if (!StillThere(Row, Soul))
		{
			Note = "그 자리의 사람이 바뀌었습니다";
			return false;
		}
		std::vector<std::string> traits;
		ReadTraits(Soul, traits);
		if (!NlCore::Has(traits, Name))
		{
			Note = "게임이 붙이지 않았습니다";		// 까닭은 재지 않았다(재능 71개를 붙일 때는 거부가 없었다. research/21)
			return false;
		}
		return true;
	}

}
