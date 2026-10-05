#include "Access.hpp"

#include "Game.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <cmath>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::AskPath;
using NlCore::PathStep;
using NlCore::Shortest;

namespace
{
	// ds 형 상수. 이 러너에서 맞는 것을 만들고 지워서 확인했다(research/02).
	constexpr double k_DsMap = 1, k_DsList = 2;

	struct Cursor			// 따라가는 동안 든 값
	{
		Holder Kind = Holder::None;
		RValue Value;
	};

	bool Truthy(const char* Name, const std::vector<RValue>& Args)
	{
		return NlGame::CallNumber(Name, Args, 0) > 0;
	}

	RValue Str(const std::string& Value)
	{
		return RValue(std::string_view(Value));
	}

	// 0 이상의 정수인 수인가(ds 번호).
	bool WholeNumber(const RValue& Value, double& Out)
	{
		if (!NlGame::IsNumber(Value))
			return false;
		const double number = Value.ToDouble();
		if (!(number >= 0) || number != std::floor(number))
			return false;
		Out = number;
		return true;
	}

	// 오브젝트의 번호. 없으면 -1.
	double ObjectIndex(const std::string& Name)
	{
		for (const NlGame::Object& object : NlGame::Objects())
			if (object.Name == Name)
				return object.Index;
		return -1;
	}

	bool DsExists(double Id, double Type)
	{
		return Truthy("ds_exists", { RValue(Id), RValue(Type) });
	}

	// 전역의 멤버를 이름으로 찾는다. Game::Resolve 가 쓰던 길이다(열거. 없는 이름으로 GetInstanceMember 를 부르지 않는다).
	bool MemberOfGlobal(const std::string& Name, RValue& Out)
	{
		CInstance* global = NlGame::Global();
		if (!global)
			return false;

		bool found = false;
		NlGame::Yytk()->EnumInstanceMembers(RValue(global), [&](const char* MemberName, RValue* Value) -> bool
		{
			if (!MemberName || !Value || Name != MemberName)
				return false;
			Out = *Value;
			found = true;
			return true;	// 찾았으니 그만 돈다
		});
		return found;
	}

	// ds_map 의 키. 글로 된 키가 없고 그 글이 수이면 수로 된 키도 본다(battle_params 의 "0", "1").
	bool MapKey(double Map, const std::string& Name, RValue& Key)
	{
		Key = Str(Name);
		if (Truthy("ds_map_exists", { RValue(Map), Key }))
			return true;

		double number = 0;
		if (!NlCore::ParseNumber(Name, number))
			return false;
		Key = RValue(number);
		return Truthy("ds_map_exists", { RValue(Map), Key });
	}

	bool Root(const AskPath& Path, Cursor& Out, std::string& Why)
	{
		if (!Path.Error.empty())
		{
			Why = Path.Error;
			return false;
		}
		if (Path.Root == "global")
		{
			CInstance* global = NlGame::Global();
			if (!global)
			{
				Why = "no global instance";
				return false;
			}
			Out = { Holder::Global, RValue(global) };
			return true;
		}
		if (Path.Root == "inst")
		{
			const double object = ObjectIndex(Path.Object);
			if (object < 0)
			{
				Why = "no such object: " + Path.Object;
				return false;
			}
			if (NlGame::CallNumber("instance_number", { RValue(object) }, 0) <= Path.Number)
			{
				Why = "no instance of " + Path.Object + " at " + Shortest(Path.Number);
				return false;
			}
			RValue id;
			if (!NlGame::Call("instance_find", { RValue(object), RValue(Path.Number) }, id)
				|| (NlGame::IsNumber(id) && id.ToDouble() < 0))		// noone (-4)
			{
				Why = "instance_find failed for " + Path.Object;
				return false;
			}
			Out = { Holder::Instance, id };
			return true;
		}

		// map:<번호>, list:<번호>: 번호를 든다. 그 번호의 ds 가 있는지는 단계(@, #)나 List 가 본다.
		Out = { Holder::None, RValue(Path.Number) };
		return true;
	}

	// 한 단계를 읽는다.
	bool Step(const Cursor& From, const PathStep& S, Cursor& To, std::string& Why)
	{
		RValue next;
		double id = 0;
		switch (S.Kind)
		{
		case '.':
			if (From.Kind == Holder::Global)
			{
				if (!MemberOfGlobal(S.Name, next))
				{
					Why = "no global named " + S.Name;
					return false;
				}
			}
			else if (From.Kind == Holder::Instance)
			{
				const RValue name = Str(S.Name);
				if (!Truthy("variable_instance_exists", { From.Value, name })
					|| !NlGame::Call("variable_instance_get", { From.Value, name }, next))
				{
					Why = "no instance variable " + S.Name;
					return false;
				}
			}
			else if (From.Value.IsStruct())
			{
				const RValue name = Str(S.Name);
				if (!Truthy("variable_struct_exists", { From.Value, name })
					|| !NlGame::Call("variable_struct_get", { From.Value, name }, next))
				{
					Why = "no member " + S.Name;
					return false;
				}
			}
			else
			{
				Why = "." + S.Name + ": not a struct or an instance";
				return false;
			}
			break;

		case '[':
			if (!From.Value.IsArray() || S.Index >= NlGame::ArrayLength(From.Value)
				|| !NlGame::Call("array_get", { From.Value, RValue(S.Index) }, next))
			{
				Why = "[" + Shortest(S.Index) + "]: not an array or out of range";
				return false;
			}
			break;

		case '@':
		{
			RValue key;
			if (!WholeNumber(From.Value, id) || !DsExists(id, k_DsMap) || !MapKey(id, S.Name, key)
				|| !NlGame::Call("ds_map_find_value", { RValue(id), key }, next))
			{
				Why = "@" + S.Name + ": no such ds_map or key";
				return false;
			}
			break;
		}

		case '#':
			if (!WholeNumber(From.Value, id) || !DsExists(id, k_DsList)
				|| S.Index >= NlGame::CallNumber("ds_list_size", { RValue(id) }, 0)
				|| !NlGame::Call("ds_list_find_value", { RValue(id), RValue(S.Index) }, next))
			{
				Why = "#" + Shortest(S.Index) + ": no such ds_list or out of range";
				return false;
			}
			break;

		default:
			Why = "unknown step";
			return false;
		}

		To = { NlAccess::Classify(next), next };
		return true;
	}

	// 뿌리에서 앞의 Steps 단계까지 따라간다.
	bool Resolve(const AskPath& Path, size_t Steps, Cursor& Out, std::string& Why)
	{
		Cursor at;
		if (!Root(Path, at, Why))
			return false;
		for (size_t i = 0; i < Steps; i++)
		{
			Cursor next;
			if (!Step(at, Path.Steps[i], next, Why))
				return false;
			at = next;
		}
		Out = at;
		return true;
	}

	// 같은 값인가(쓴 뒤에 다시 읽어 견준다).
	bool Same(const RValue& A, const RValue& B)
	{
		if (A.IsString() || B.IsString())
			return A.IsString() && B.IsString() && A.ToString() == B.ToString();
		return NlGame::IsNumber(A) && NlGame::IsNumber(B) && A.ToDouble() == B.ToDouble();
	}

	// 수를 원래 값의 형에 맞춰 만든다(불리언은 불리언으로, 정수형은 정수로).
	RValue NumberLike(const RValue& Old, double Number)
	{
		if (Old.m_Kind == VALUE_BOOL)
			return RValue(Number != 0);
		if ((Old.m_Kind == VALUE_INT32 || Old.m_Kind == VALUE_INT64) && Number == std::floor(Number))
			return RValue(static_cast<int64_t>(Number));
		return RValue(Number);
	}
}

bool NlAccess::Open(const AskPath& Path, RValue& Out, Holder& Kind, std::string& Why)
{
	Cursor at;
	if (!Resolve(Path, Path.Steps.size(), at, Why))
		return false;
	Out = at.Value;
	Kind = at.Kind;
	return true;
}

bool NlAccess::Follow(const RValue& From, const std::vector<PathStep>& Steps, RValue& Out, std::string& Why)
{
	Cursor at{ Classify(From), From };
	for (const PathStep& step : Steps)
	{
		Cursor next;
		if (!Step(at, step, next, Why))
			return false;
		at = next;
	}
	Out = at.Value;
	return true;
}

bool NlAccess::SetNumber(const RValue& In, const PathStep& S, double Number, std::string& Why)
{
	const Cursor parent{ Classify(In), In };
	Cursor old;
	if (!Step(parent, S, old, Why))
		return false;			// 없는 것을 만들지 않는다
	if (!NlGame::IsNumber(old.Value))
	{
		Why = "not a number (" + old.Value.GetKindName() + ")";
		return false;
	}

	const RValue value = NumberLike(old.Value, Number);
	RValue ignored;
	if (S.Kind == '.' && In.IsStruct())
		NlGame::Call("variable_struct_set", { In, Str(S.Name), value }, ignored);
	else if (S.Kind == '[' && In.IsArray())
		NlGame::Call("array_set", { In, RValue(S.Index), value }, ignored);
	else
	{
		Why = "can only set a struct member or an array element";
		return false;
	}

	Cursor again;
	if (!Step(parent, S, again, Why))
		return false;
	if (!Same(again.Value, value))
	{
		Why = "did not stick";
		return false;
	}
	return true;
}

bool NlAccess::Read(const AskPath& Path, RValue& Out, std::string& Why)
{
	Holder ignored = Holder::None;
	return Open(Path, Out, ignored, Why);
}

bool NlAccess::Write(const AskPath& Path, const RValue& Value, std::string& Why)
{
	if (!Path.Error.empty() || Path.Steps.empty())
	{
		Why = Path.Error.empty() ? "nothing to write: the path has no steps" : Path.Error;
		return false;
	}

	// 없는 것을 만들지 않는다: 지금 읽히는 자리에만 쓴다.
	const size_t last = Path.Steps.size() - 1;
	const PathStep& step = Path.Steps[last];
	Cursor parent, old;
	if (!Resolve(Path, last, parent, Why) || !Step(parent, step, old, Why))
		return false;

	RValue ignored, key;
	double id = 0;
	switch (step.Kind)
	{
	case '.':
		if (parent.Kind == Holder::Global)
			NlGame::Call("variable_global_set", { Str(step.Name), Value }, ignored);
		else if (parent.Kind == Holder::Instance)
			NlGame::Call("variable_instance_set", { parent.Value, Str(step.Name), Value }, ignored);
		else
			NlGame::Call("variable_struct_set", { parent.Value, Str(step.Name), Value }, ignored);
		break;
	case '[':
		NlGame::Call("array_set", { parent.Value, RValue(step.Index), Value }, ignored);
		break;
	case '@':
		if (WholeNumber(parent.Value, id) && MapKey(id, step.Name, key))
			NlGame::Call("ds_map_replace", { RValue(id), key, Value }, ignored);
		break;
	case '#':
		if (WholeNumber(parent.Value, id))
			NlGame::Call("ds_list_replace", { RValue(id), RValue(step.Index), Value }, ignored);
		break;
	}

	// 반환값이 없는 빌트인들이다. 뿌리부터 다시 읽어 판정한다.
	RValue again;
	if (!Read(Path, again, Why))
		return false;
	if (!Same(again, Value))
	{
		Why = "did not stick";
		return false;
	}
	return true;
}

bool NlAccess::ReadNumber(const std::string& Path, double& Out)
{
	RValue value;
	std::string why;
	if (!Read(NlCore::ParseAskPath(Path), value, why) || !NlGame::IsNumber(value))
		return false;
	Out = value.ToDouble();
	return true;
}

bool NlAccess::WriteNumber(const std::string& Path, double Number, std::string& Why)
{
	const AskPath path = NlCore::ParseAskPath(Path);
	RValue old;
	if (!Read(path, old, Why))
		return false;
	if (!NlGame::IsNumber(old))
	{
		Why = "not a number (" + old.GetKindName() + ")";
		return false;
	}
	return Write(path, NumberLike(old, Number), Why);
}

bool NlAccess::WriteString(const std::string& Path, const std::string& Text, std::string& Why)
{
	const AskPath path = NlCore::ParseAskPath(Path);
	RValue old;
	if (!Read(path, old, Why))
		return false;
	if (!old.IsString())
	{
		Why = "not a string (" + old.GetKindName() + ")";
		return false;
	}
	return Write(path, Str(Text), Why);
}

Holder NlAccess::Classify(const RValue& Value)
{
	if (Value.IsArray())
		return Holder::Array;
	if (Value.IsStruct())
		return Holder::Struct;
	if (Value.m_Kind == VALUE_REF && Truthy("instance_exists", { Value }))
		return Holder::Instance;
	return Holder::None;
}

NlAccess::Row NlAccess::Describe(const PathStep& Step, const RValue& Value)
{
	Row row;
	row.Step = Step;
	row.Name = Step.Kind == '.' || Step.Kind == '@' ? Step.Name : NlCore::FormatStep(Step);

	if (Value.IsString())
	{
		row.Type = "string";
		row.IsString = true;
		row.Raw = Value.ToString();
		row.Text = NlCore::Quote(row.Raw, 80);
	}
	else if (Value.IsArray())
	{
		row.Type = "array";
		row.IsContainer = true;
		row.Text = "[" + Shortest(NlGame::ArrayLength(Value)) + "]";
	}
	else if (Value.IsStruct())
	{
		// 메서드도 YYToolkit 에서는 구조체로 보인다(VALUE_OBJECT). 빌트인으로 가린다.
		if (Truthy("is_method", { Value }))
		{
			row.Type = "method";
			row.Text = "함수";
		}
		else
		{
			row.Type = "struct";
			row.IsContainer = true;
			row.Text = "{...}";
		}
	}
	else if (Value.m_Kind == VALUE_REF)
	{
		row.Type = "ref";
		row.IsContainer = Truthy("instance_exists", { Value });
		row.Text = row.IsContainer ? "인스턴스" : "ref";
	}
	else if (Value.m_Kind == VALUE_UNDEFINED || Value.m_Kind == VALUE_UNSET)
		row.Type = "undefined";
	else if (NlGame::IsNumber(Value))
	{
		row.Type = Value.GetKindName();
		row.IsNumber = true;
		row.IsBool = Value.m_Kind == VALUE_BOOL;
		row.Number = Value.ToDouble();
		row.Text = row.IsBool ? (row.Number != 0 ? "true" : "false") : Shortest(row.Number);
	}
	else
		row.Type = Value.GetKindName();
	return row;
}

double NlAccess::ForEachChild(const RValue& Value, Holder Kind, const std::function<bool(const PathStep&, const RValue&)>& Visit)
{
	double id = 0;
	switch (Kind)
	{
	case Holder::Global:
	case Holder::Struct:
	{
		if (!Value.IsStruct())
			return -1;
		double count = 0;
		// 콜백이 참을 돌려주면 열거가 멈춘다(YYToolkit MI_Public.cpp 의 EnumInstanceMembers).
		NlGame::Yytk()->EnumInstanceMembers(Value, [&](const char* MemberName, RValue* Member) -> bool
		{
			count++;
			if (!MemberName || !Member)
				return false;
			return !Visit({ '.', MemberName, 0 }, *Member);
		});
		return count;
	}

	case Holder::Instance:
	{
		RValue names;
		if (!NlGame::Call("variable_instance_get_names", { Value }, names) || !names.IsArray())
			return -1;
		const std::vector<RValue> list = names.ToVector();
		for (const RValue& name : list)
		{
			RValue member;
			if (!name.IsString() || !NlGame::Call("variable_instance_get", { Value, name }, member))
				continue;
			if (!Visit({ '.', name.ToString(), 0 }, member))
				break;
		}
		return static_cast<double>(list.size());
	}

	case Holder::Array:
	{
		if (!Value.IsArray())
			return -1;
		const double length = NlGame::ArrayLength(Value);
		for (double i = 0; i < length; i++)
		{
			RValue item;
			if (!NlGame::Call("array_get", { Value, RValue(i) }, item))
				continue;
			if (!Visit({ '[', "", i }, item))
				break;
		}
		return length < 0 ? -1 : length;
	}

	case Holder::Map:
	{
		if (!WholeNumber(Value, id) || !DsExists(id, k_DsMap))
			return -1;
		RValue keys;
		if (!NlGame::Call("ds_map_keys_to_array", { RValue(id) }, keys) || !keys.IsArray())
			return 0;		// 있는 ds_map 인데 키의 배열을 얻지 못했다: 빈 것으로 본다
		const std::vector<RValue> list = keys.ToVector();
		for (const RValue& key : list)
		{
			RValue item;
			if (!NlGame::Call("ds_map_find_value", { RValue(id), key }, item))
				continue;
			const std::string name = key.IsString() ? key.ToString()
				: NlGame::IsNumber(key) ? Shortest(key.ToDouble()) : key.GetKindName();
			if (!Visit({ '@', name, 0 }, item))
				break;
		}
		return static_cast<double>(list.size());
	}

	case Holder::List:
	{
		if (!WholeNumber(Value, id) || !DsExists(id, k_DsList))
			return -1;
		const double size = NlGame::CallNumber("ds_list_size", { RValue(id) }, 0);
		for (double i = 0; i < size; i++)
		{
			RValue item;
			if (!NlGame::Call("ds_list_find_value", { RValue(id), RValue(i) }, item))
				continue;
			if (!Visit({ '#', "", i }, item))
				break;
		}
		return size;
	}

	default:
		return -1;
	}
}

bool NlAccess::List(const AskPath& Path, Holder As, size_t Limit, std::vector<Row>& Rows, size_t& Total, std::string& Why)
{
	Rows.clear();
	Total = 0;

	RValue value;
	Holder kind = Holder::None;
	if (!Open(Path, value, kind, Why))
		return false;
	if (kind == Holder::None)
	{
		// 수는 ds 번호로만 열 수 있다. map:<번호> 와 list:<번호> 는 뿌리가 방식을 말한다.
		if (Path.Steps.empty() && Path.Root == "map")
			As = Holder::Map;
		else if (Path.Steps.empty() && Path.Root == "list")
			As = Holder::List;
		if (!NlGame::IsNumber(value) || (As != Holder::Map && As != Holder::List))
		{
			Why = "not a container";
			return false;
		}
		kind = As;
	}

	const double count = ForEachChild(value, kind, [&](const PathStep& step, const RValue& child) {
		if (Rows.size() < Limit)
			Rows.push_back(Describe(step, child));
		return true;		// 끝까지 센다
	});
	if (count < 0)
	{
		Why = kind == Holder::Map ? "no such ds_map" : kind == Holder::List ? "no such ds_list" : "not a container";
		return false;
	}
	Total = static_cast<size_t>(count);

	// 이름이 있는 것은 이름순으로(열거의 차례는 실행마다 다를 수 있다). 배열과 리스트는 자리순 그대로.
	if (kind != Holder::Array && kind != Holder::List)
		std::stable_sort(Rows.begin(), Rows.end(), [](const Row& a, const Row& b) { return a.Name < b.Name; });
	return true;
}

bool NlAccess::InstanceIdentity(const AskPath& Path, int64_t& Out)
{
	Cursor at;
	std::string why;
	if (Path.Root != "inst" || !Root(Path, at, why))
		return false;
	Out = at.Value.m_i64;		// Search.cpp 와 Dump.cpp 가 인스턴스를 가리는 바로 그 값이다
	return true;
}

std::vector<NlAccess::RootObject> NlAccess::LiveObjects()
{
	std::vector<RootObject> live;
	for (const NlGame::Object& object : NlGame::Objects())
	{
		const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
		if (count > 0)
			live.push_back({ object.Name, count });
	}
	return live;
}

int NlAccess::InstanceCount(const std::string& Object)
{
	const double index = ObjectIndex(Object);
	return index < 0 ? 0 : static_cast<int>(NlGame::CallNumber("instance_number", { RValue(index) }, 0));
}

bool NlAccess::InGame()
{
	return InstanceCount("o_character") > 0 && InstanceCount("o_main_menu") == 0;
}

namespace
{
	// 메서드, 그것에 대해 아는 것, 그것을 가진 것(주소의 부모)을 얻는다.
	bool ResolveMethod(const AskPath& Path, RValue& Method, RValue& Owner, NlAccess::MethodInfo& Info, std::string& Why)
	{
		if (!NlAccess::Read(Path, Method, Why))
			return false;
		if (!Truthy("is_method", { Method }))
		{
			Why = "not a method";
			return false;
		}

		// script_get_name 은 메서드도 받는다(research/07 에서 이름이 나왔다). 안 되면 method_get_index 를 거친다.
		RValue name, index;
		Info.Script.clear();
		if (NlGame::Call("script_get_name", { Method }, name) && name.IsString())
			Info.Script = name.ToString();
		if (Info.Script.empty() && NlGame::Call("method_get_index", { Method }, index)
			&& NlGame::Call("script_get_name", { index }, name) && name.IsString())
			Info.Script = name.ToString();

		// method_get_self: 묶인 인스턴스나 구조체. 없으면 undefined(매뉴얼).
		RValue self;
		Info.Bound = NlGame::Call("method_get_self", { Method }, self) && self.m_Kind != VALUE_UNDEFINED && self.m_Kind != VALUE_UNSET;

		// 메서드에 닿은 마지막 단계가 멤버('.')일 때만 부모가 그것을 가진 것이다. 배열의 원소나 ds 의 값이면 가진 것을 모른다.
		NlCore::OwnerKind owner = NlCore::OwnerKind::Other;
		if (!Path.Steps.empty())
		{
			Holder kind = Holder::None;
			if (!NlAccess::Open(NlCore::ParentPath(Path), Owner, kind, Why))
				return false;
			if (Path.Steps.back().Kind == '.')
				owner = kind == Holder::Struct ? NlCore::OwnerKind::Struct : kind == Holder::Instance ? NlCore::OwnerKind::Instance
					: kind == Holder::Global ? NlCore::OwnerKind::Global : NlCore::OwnerKind::Other;
		}
		Info.How = NlCore::ChooseBinding(Info.Bound, owner);
		return true;
	}
}

bool NlAccess::AboutMethod(const AskPath& Path, MethodInfo& Out, std::string& Why)
{
	RValue method, owner;		// 이 함수 안에서만 든다
	return ResolveMethod(Path, method, owner, Out, Why);
}

bool NlAccess::CallMethod(const AskPath& Path, const std::vector<RValue>& Args, RValue& Result, std::string& Why)
{
	RValue method, owner;
	MethodInfo info;
	if (!ResolveMethod(Path, method, owner, info, Why))
		return false;
	if (info.How == NlCore::Binding::Refuse)
	{
		Why = "an unbound method whose owner is not a struct or an instance (it would run with self = global)";
		return false;
	}
	if (info.How == NlCore::Binding::ToOwner)
	{
		// method(구조체나 인스턴스, 함수): 거기에 묶인 새 메서드(매뉴얼).
		RValue bound;
		if (!NlGame::Call("method", { owner, method }, bound) || !Truthy("is_method", { bound }))
		{
			Why = "could not bind the method to its owner";
			return false;
		}
		method = bound;
	}

	// method_call(메서드, 인자의 배열). 인자가 있는 호출은 이 꼴로 됐다(research/07). 매뉴얼은 인자가 없으면 배열을 빼도 된다고 하지만,
	// 배열 없이 부른 호출이 됐는지는 확인하지 못했다. 확인된 길 하나로 간다(빈 배열).
	RValue args;
	if (Args.empty())
	{
		if (!NlGame::Call("array_create", { RValue(0.0) }, args))
		{
			Why = "array_create failed";
			return false;
		}
	}
	else
		args = RValue(Args);

	if (!NlGame::Call("method_call", { method, args }, Result))
	{
		Why = "method_call failed";
		return false;
	}
	return true;
}
