#include "Game.hpp"

#include "core/Text.hpp"

using namespace Aurie;
using namespace YYTK;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	YYTKInterface* g_Yytk = nullptr;
	std::vector<NlGame::Object> g_Objects;	// RValue 를 담지 않는다(정적 저장 기간의 RValue 를 두지 않는다)
	bool g_ObjectsBuilt = false;
	bool g_RoomFailed = false;
}

void NlGame::Init(YYTKInterface* Yytk)
{
	g_Yytk = Yytk;
}

YYTKInterface* NlGame::Yytk()
{
	return g_Yytk;
}

CInstance* NlGame::Global()
{
	CInstance* global = nullptr;
	return AurieSuccess(g_Yytk->GetGlobalInstance(&global)) ? global : nullptr;
}

bool NlGame::Call(const char* Name, const std::vector<RValue>& Args, RValue& Result)
{
	CInstance* global = Global();
	return global && AurieSuccess(g_Yytk->CallBuiltinEx(Result, Name, global, global, Args));
}

bool NlGame::IsNumber(const RValue& Value)
{
	return !Value.IsString() && !Value.IsStruct() && !Value.IsArray() && Value.IsNumberConvertible();
}

double NlGame::CallNumber(const char* Name, const std::vector<RValue>& Args, double Fallback)
{
	RValue result;
	if (!Call(Name, Args, result) || !IsNumber(result))
		return Fallback;
	return result.ToDouble();
}

const std::vector<NlGame::Object>& NlGame::Objects()
{
	if (g_ObjectsBuilt)
		return g_Objects;
	g_ObjectsBuilt = true;

	for (int i = 0; i < 4096; i++)
	{
		const RValue index(static_cast<double>(i));
		if (CallNumber("object_exists", { index }, 0) <= 0)
			break;

		RValue name;
		if (!Call("object_get_name", { index }, name) || !name.IsString())
			break;
		g_Objects.push_back({ name.ToString(), static_cast<double>(i) });
	}
	return g_Objects;
}

std::string NlGame::RoomName()
{
	if (g_RoomFailed)
		return "";

	// 전역 빌트인 변수는 인스턴스 없이 읽는다(YYToolkit 위키 GetBuiltin). 이 게임에서 빌트인 변수 표의
	// 초기화가 성공한 것은 aurie.log 의 GetBuiltinInformation => AURIE_SUCCESS 로 확인했다.
	RValue room, name;
	if (!AurieSuccess(g_Yytk->GetBuiltin("room", nullptr, NULL_INDEX, room))
		|| !Call("room_get_name", { room }, name) || !name.IsString())
	{
		g_RoomFailed = true;
		return "";
	}
	return name.ToString();
}

bool NlGame::Resolve(const std::string& Path, RValue& Out)
{
	CInstance* global = Global();
	if (!global || Path.rfind("global.", 0) != 0)
		return false;

	RValue current(global);
	size_t begin = 7;	// "global." 다음
	while (true)
	{
		const size_t dot = Path.find('.', begin);
		const std::string name = Path.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin);
		if (!current.IsStruct())
			return false;

		RValue* member = nullptr;
		if (!AurieSuccess(g_Yytk->GetInstanceMember(current, name.c_str(), member)) || !member)
			return false;
		current = *member;

		if (dot == std::string::npos)
			break;
		begin = dot + 1;
	}
	Out = current;
	return true;
}

double NlGame::ArrayLength(const RValue& Value)
{
	return CallNumber("array_length", { Value }, -1);
}

std::string NlGame::Describe(const RValue& Value)
{
	if (Value.IsStruct())
		return "\"kind\":\"struct\"";
	if (Value.IsArray())
		return "\"kind\":\"array\",\"length\":" + Number(ArrayLength(Value));
	if (Value.IsString())
		return "\"kind\":\"string\",\"value\":" + Quote(Value.ToString());
	if (Value.IsNumberConvertible())
		return "\"kind\":" + Quote(Value.GetKindName()) + ",\"value\":" + Number(Value.ToDouble());
	return "\"kind\":" + Quote(Value.GetKindName());
}

void NlGame::WriteMembers(std::ostream& Out, const RValue& Object, bool Expand, bool FlushEach)
{
	bool first = true;
	g_Yytk->EnumInstanceMembers(Object, [&](const char* Name, RValue* Value) -> bool
	{
		Out << (first ? "" : ",") << "\n" << Quote(Name ? Name : "") << ":{";
		first = false;

		if (!Value)
			Out << "\"kind\":\"error\"";
		else if (Expand && Value->IsStruct())
		{
			Out << "\"kind\":\"struct\",\"members\":{";
			WriteMembers(Out, *Value, false, false);
			Out << "}";
		}
		else
			Out << Describe(*Value);

		Out << "}";
		if (FlushEach)
			Out.flush();	// 도중에 죽어도 어디까지 왔는지 남는다
		return false;		// 거짓을 돌려줘야 다음 멤버로 넘어간다
	});
}
