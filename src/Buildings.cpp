#include "Buildings.hpp"

#include "Access.hpp"
#include "Game.hpp"

using namespace YYTK;
using NlAccess::Holder;

namespace
{
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
	// k_AllNames 의 스크립트가 읽는 전역(research/34). 부팅 중에는 아직 없다.
	constexpr const char* k_Storage = "global.__building_storage";
}

bool NlBuildings::Ready()
{
	RValue storage;		// 이 함수 안에서만 든다
	std::string why;
	// 없는 전역은 읽기에서 거짓이 된다(만들지 않는다). 있어도 undefined 면 스크립트가 그것의 멤버를 읽다 끝난다.
	return NlAccess::Read(NlCore::ParseAskPath(k_Storage), storage, why) && storage.m_Kind != VALUE_UNDEFINED && storage.m_Kind != VALUE_UNSET;
}

bool NlBuildings::ForEachType(const std::function<bool(const std::string&, const RValue&)>& Visit, std::string& Why)
{
	// 스크립트를 부르기 전에 그것이 읽는 전역이 있는지 본다: 없을 때 부르면 게임이 GML 오류로 끝난다(메뉴에서도 쓰는 일이 부팅 중에 불러 그랬다. 2026-10-08).
	if (!Ready())
	{
		Why = "the game has not loaded its buildings yet";
		return false;
	}
	RValue names;		// 이 함수 안에서만 든다
	if (!NlGame::CallScript(k_AllNames, {}, names) || !names.IsArray())
	{
		Why = "the list of buildings is not available";
		return false;
	}
	NlAccess::ForEachChild(names, Holder::Array, [&](const NlCore::PathStep&, const RValue& name) {
		if (!name.IsString())
			return true;
		RValue generic;
		if (!NlGame::CallScript(k_Generic, { name }, generic) || !generic.IsStruct())
			return true;
		return Visit(name.ToString(), generic);
	});
	return true;
}
