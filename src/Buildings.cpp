#include "Buildings.hpp"

#include "Access.hpp"
#include "Game.hpp"

using namespace YYTK;
using NlAccess::Holder;

namespace
{
	constexpr const char* k_AllNames = "gml_Script_building_generic_get_array_of_all_buildings";
	constexpr const char* k_Generic = "gml_Script_get_generic_building";
}

bool NlBuildings::ForEachType(const std::function<bool(const std::string&, const RValue&)>& Visit, std::string& Why)
{
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
