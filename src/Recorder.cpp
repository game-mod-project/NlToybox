#include "Recorder.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CallLog.hpp"
#include "core/Hooks.hpp"
#include "core/Text.hpp"

#include <array>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

using namespace Aurie;
using namespace YYTK;

namespace
{
	constexpr int k_Slots = 64;				// 한 실행에 훅을 걸 수 있는 함수의 수. 자리는 다시 쓰지 않는다(24개는 한 실행에서 다 썼다. research/07)
	constexpr int k_MaxKinds = 16;			// 꼴을 볼 때 보는 인자의 수
	constexpr int k_KindMask = 0x0ffffff;	// m_Kind 에서 형만 남긴다(VALUE_UNSET 의 폭. YYTK_Shared_Types.hpp 199행)

	struct Slot
	{
		bool Used = false;
		bool Failed = false;					// 훅을 걸다 실패했다. Target 은 걸려던 함수로 남긴다(같은 함수에 다시 걸지 않게)
		bool Recording = false;
		std::string Name;
		PFUNC_YYGMLScript Target = nullptr;		// 훅을 건 함수
		PFUNC_YYGMLScript Original = nullptr;	// 원래 함수로 가는 트램펄린
		NlCore::CallLog Log;
		// 돌려주는 값을 바꾼다(스펙 §3 의 수단 D). 게임 스레드만 읽고 쓴다.
		bool Override = false;
		NlRecorder::Forced Value;
		// Who 로 가린 바꾸기가 self 를 견준 결과(걸린 호출과 지나간 호출의 수). 가려지는지를 재는 데 쓴다.
		uint64_t Matched = 0, Passed = 0;
	};

	AurieModule* g_Module = nullptr;
	NlRecorder::LogFn g_Log;
	std::recursive_mutex g_Mutex;		// 기록(Slot::Log)을 지킨다
	Slot g_Slots[k_Slots];

	void Log(const std::string& Line)
	{
		if (g_Log)
			g_Log(Line);
	}

	const char* KindName(int Kind)
	{
		switch (Kind)
		{
		case VALUE_REAL: return "number";
		case VALUE_STRING: return "string";
		case VALUE_ARRAY: return "array";
		case VALUE_PTR: return "ptr";
		case VALUE_UNDEFINED: return "undefined";
		case VALUE_OBJECT: return "struct";		// 메서드와 인스턴스도 여기에 든다
		case VALUE_INT32: return "int32";
		case VALUE_INT64: return "int64";
		case VALUE_BOOL: return "bool";
		case VALUE_REF: return "ref";
		default: return "other";
		}
	}

	// 값 하나를 짧은 글로. 훅 안에서 부른다: 빌트인을 부르지 않고 RValue 의 멤버 함수만 쓴다.
	std::string Brief(const RValue& Value)
	{
		const int kind = static_cast<int>(Value.m_Kind) & k_KindMask;
		if (kind == VALUE_UNDEFINED || kind == k_KindMask)		// k_KindMask 는 VALUE_UNSET 의 값이기도 하다
			return "undefined";
		if (Value.IsString())
			return NlCore::Quote(Value.ToString(), 60);
		if (Value.IsArray())
			return "array";
		if (Value.IsStruct())
			return "struct";
		if (kind == VALUE_REF)
			return "ref";
		if (kind == VALUE_BOOL)
			return Value.ToDouble() != 0 ? "true" : "false";
		if (Value.IsNumberConvertible())
			return NlCore::Shortest(Value.ToDouble());
		return KindName(kind);
	}

	// 플레이어의 영혼의 주소들(SetPlayerSelves). 게임 스레드에서만 읽고 쓴다(틱과 훅).
	NlCore::SelfSet g_PlayerSelves;

	// 훅을 건 함수가 불릴 때마다 온다(게임 스레드).
	RValue& Handle(int Index, CInstance* Self, CInstance* Other, RValue& Result, int Count, RValue** Args)
	{
		Slot& slot = g_Slots[Index];
		const PFUNC_YYGMLScript original = slot.Original;

		size_t sample = 0;
		uint64_t key = 0;
		std::string shape, args;
		if (slot.Recording)
		{
			int kinds[k_MaxKinds];
			const int seen = Count < 0 || !Args ? 0 : (Count > k_MaxKinds ? k_MaxKinds : Count);
			for (int i = 0; i < seen; i++)
				kinds[i] = Args[i] ? static_cast<int>(Args[i]->m_Kind) & k_KindMask : -1;
			key = NlCore::ShapeKey(kinds, seen);
			{
				std::lock_guard lock(g_Mutex);
				sample = slot.Log.Note(key);
			}
			if (sample)
			{
				// 인자의 글은 부르기 전에 만든다(불린 쪽이 인자를 바꿀 수 있다).
				shape = "(";
				args = "(";
				for (int i = 0; i < seen; i++)
				{
					if (i)
					{
						shape += ", ";
						args += ", ";
					}
					shape += kinds[i] < 0 ? "null" : KindName(kinds[i]);
					args += Args[i] ? Brief(*Args[i]) : "null";
				}
				if (Count > seen)
				{
					shape += ", ...";
					args += ", ...";
				}
				shape += ")";
				args += ")";
			}
		}

		// 돌려줄 값을 바꾼다. 수, 불리언, undefined 뿐이다. 바꾼 값은 언제나 결과 자리(Result)에 두고 Result 를 돌려준다:
		// 게임이 반환 참조를 읽든 Result 를 읽든(YYToolkit 의 CallGameScriptEx 는 Result 만 읽는다) 같은 값을 본다.
		const NlRecorder::Forced value = slot.Value;
		// 누구의 호출에 걸지(Who). self 가 플레이어의 영혼인지는 틱이 넣어 둔 주소와 견준다(빌트인을 부르지 않는다).
		const bool forced = slot.Override
			&& NlCore::HookApplies(value.Who, value.Who != 'a' && g_PlayerSelves.Has(reinterpret_cast<std::uintptr_t>(Self)));
		if (slot.Override && value.Who != 'a')
			(forced ? slot.Matched : slot.Passed)++;
		const auto make = [&value]() {
			return value.Kind == 'n' ? RValue(value.Number) : value.Kind == 'b' ? RValue(value.Number != 0) : RValue();
		};
		// 해제하지 않고 덮어쓴다. 원래 함수가 채우지 않은 Result 는 부른 쪽이 초기화했는지 알 수 없어서 해제하면 안 된다.
		// 수·불리언·undefined 는 가진 것이 없으므로 그대로 옮겨 적어도 된다.
		const auto write_raw = [&make](RValue& Target) {
			const RValue made = make();
			std::memcpy(static_cast<void*>(&Target), static_cast<const void*>(&made), sizeof(RValue));
		};

		if (forced && value.Skip && value.Kind != 'x')
		{
			// 원래 함수를 부르지 않는다. 들어올 때 Result 에 무엇이 있었는지 표본에 남긴다(부른 쪽이 초기화하는지 잰다).
			const int came = static_cast<int>(Result.m_Kind) & k_KindMask;
			write_raw(Result);
			if (sample)
			{
				std::lock_guard lock(g_Mutex);
				slot.Log.Sample(sample, key, std::move(shape), std::move(args),
					std::string("(skipped, Result came as ") + (came == k_KindMask ? "unset" : KindName(came)) + ") => " + Brief(Result));
			}
			return Result;
		}

		RValue& out = original(Self, Other, Result, Count, Args);

		// 원래 함수가 결과 자리가 아닌 다른 값을 돌려주는 일이 있는지 표본에 남긴다(이 러너에서 재지 않은 것이다).
		const bool same = &out == &Result;
		std::string result = sample ? Brief(out) + (same ? "" : " [returned another value, not Result]") : std::string();
		if (forced && value.Kind == 'x')
		{
			// 원래 함수가 돌려준 수에 배율을 곱한다. 수가 아니면(undefined, 구조체) 그대로 지나간다.
			const int kind = static_cast<int>(out.m_Kind) & k_KindMask;
			const bool number = kind == VALUE_REAL || kind == VALUE_INT32 || kind == VALUE_INT64;
			if (number)
			{
				// 정수형으로 돌아온 수는 곱한 값이 정수이면 정수형으로 돌려준다(부르는 쪽이 형을 볼 수 있다. Access 의 NumberLike 와 같은 뜻).
				const double product = NlCore::ScaleResult(out.ToDouble(), value.Number, value.Whole);
				const bool integral = kind != VALUE_REAL && product == static_cast<double>(static_cast<int64_t>(product));
				const RValue scaled = integral ? RValue(static_cast<int64_t>(product)) : RValue(product);
				if (same)
					Result = scaled;		// 원래 함수가 채운 수다. 가진 것이 없다
				else
					std::memcpy(static_cast<void*>(&Result), static_cast<const void*>(&scaled), sizeof(RValue));
				if (sample)
					result += " => " + Brief(Result);
			}
			if (sample)
			{
				std::lock_guard lock(g_Mutex);
				slot.Log.Sample(sample, key, std::move(shape), std::move(args), std::move(result));
			}
			return number ? Result : out;
		}
		if (forced)
		{
			if (same)
				Result = make();		// 원래 함수가 채운 값이다. 해제하고 바꾼다(러너의 해제·복사. 빌트인이 아니다)
			else
				write_raw(Result);		// 원래 함수가 Result 를 채웠는지 모른다. 해제하지 않는다
			if (sample)
				result += " => " + Brief(Result);
		}
		if (sample)
		{
			std::lock_guard lock(g_Mutex);
			slot.Log.Sample(sample, key, std::move(shape), std::move(args), std::move(result));
		}
		return forced ? Result : out;
	}

	// 훅마다 제 자리의 번호를 아는 함수가 있어야 한다(같은 함수를 여러 훅에 쓰면 어느 스크립트인지 모른다).
	template <int Index>
	RValue& Detour(CInstance* Self, CInstance* Other, RValue& Result, int Count, RValue** Args)
	{
		return Handle(Index, Self, Other, Result, Count, Args);
	}

	template <int... Index>
	constexpr std::array<PFUNC_YYGMLScript, sizeof...(Index)> MakeDetours(std::integer_sequence<int, Index...>)
	{
		return { &Detour<Index>... };
	}

	constexpr std::array<PFUNC_YYGMLScript, k_Slots> k_Detours = MakeDetours(std::make_integer_sequence<int, k_Slots>{});

	// 스크립트의 함수를 이름으로 찾는다. "gml_Script_x" 이름으로만 찾는다: 접두 없는 이름에도 러너가 스크립트 범위의 번호를 주지만
	// 그것은 다른 루틴이다(research/07. 그 이름으로 부른 호출이 이 함수에 걸린 훅에 오지 않았다).
	// 번호가 100000 미만이면 빌트인, 500000 이상이면 확장 함수다(YYToolkit MI_Public.cpp 55~66행). 스크립트만 받는다.
	bool FindScript(const std::string& Given, std::string& Name, PFUNC_YYGMLScript& Fn, std::string& Why)
	{
		const std::string name = NlCore::ScriptRoutineName(Given);
		int index = -1;
		if (!name.empty())
			NlGame::Yytk()->GetNamedRoutineIndex(name.c_str(), &index);
		if (index < 100000 || index >= 500000)
		{
			Why = "no such script: " + Given;
			return false;
		}

		CScript* script = nullptr;
		if (!AurieSuccess(NlGame::Yytk()->GetScriptData(index - 100000, script)) || !script || !script->m_Functions
			|| !script->m_Functions->m_ScriptFunction)
		{
			Why = "the script has no function: " + name;
			return false;
		}
		Name = name;
		Fn = script->m_Functions->m_ScriptFunction;
		return true;
	}
}

void NlRecorder::Init(AurieModule* Module, LogFn Log_)
{
	g_Module = Module;
	g_Log = std::move(Log_);
}

namespace
{
	// 대상(스크립트의 이름이나 메서드의 주소)의 함수에 걸린 훅의 자리. 없으면 건다. 못 걸면 nullptr 이고 Why 에 까닭.
	// g_Mutex 를 든 채로 부르지 않는다(러너를 부른다).
	Slot* Acquire(const std::string& Target, std::string& Name, std::string& Why)
	{
		std::string given = Target;

		// 주소이면 그 자리의 메서드가 묶인 스크립트의 이름을 묻는다.
		const NlCore::AskPath path = NlCore::ParseAskPath(Target);
		if (path.Error.empty() && !path.Steps.empty())
		{
			NlAccess::MethodInfo info;
			if (!NlAccess::AboutMethod(path, info, Why))
				return nullptr;
			if (info.Script.empty())
			{
				Why = "the method has no script name: " + Target;
				return nullptr;
			}
			given = info.Script;
		}

		PFUNC_YYGMLScript fn = nullptr;
		if (!FindScript(given, Name, fn, Why))
			return nullptr;

		std::vector<NlCore::HookSlot> view(k_Slots);
		for (int i = 0; i < k_Slots; i++)
			view[i] = { g_Slots[i].Used, g_Slots[i].Failed, reinterpret_cast<const void*>(g_Slots[i].Target) };
		int free_slot = -1;
		switch (NlCore::PickHookSlot(view, reinterpret_cast<const void*>(fn), free_slot))
		{
		case NlCore::SlotPick::Existing:
			return &g_Slots[free_slot];		// 이미 훅이 걸려 있다
		case NlCore::SlotPick::Failed:
			// 실패한 자리는 다시 쓰지 않는다. 같은 함수에 되풀이해 걸면(치트 표의 항목은 0.5초마다 다시 해 본다) 자리가 그만큼 없어진다.
			Why = "the hook on " + Name + " failed earlier in this run";
			return nullptr;
		case NlCore::SlotPick::Full:
			Why = "no free hook slot (" + std::to_string(k_Slots) + " in use)";
			return nullptr;
		case NlCore::SlotPick::Free:
			break;
		}

		Slot& slot = g_Slots[free_slot];
		slot.Name = Name;
		slot.Target = fn;
		slot.Original = nullptr;
		slot.Recording = false;
		slot.Override = false;
		slot.Log.Clear();
		const AurieStatus status = MmCreateHook(g_Module, "NlToyBox.rec." + std::to_string(free_slot), reinterpret_cast<PVOID>(fn),
			reinterpret_cast<PVOID>(k_Detours[free_slot]), reinterpret_cast<PVOID*>(&slot.Original));
		if (!AurieSuccess(status) || !slot.Original)
		{
			// 실패한 자리는 다시 쓰지 않는다: 같은 훅 이름("NlToyBox.rec.N")을 Aurie 에 두 번 주지 않는다(Aurie 가 실패한 이름을
			// 표에 남기는지는 모른다. 소스가 레포에 없다). 자리는 넉넉하다.
			slot.Used = true;
			slot.Failed = true;
			slot.Name = Name + " (hook failed)";
			slot.Original = nullptr;
			Why = AurieSuccess(status) ? "MmCreateHook gave no trampoline" : std::string("MmCreateHook ") + AurieStatusToString(status);
			return nullptr;
		}
		slot.Used = true;
		Log("hook " + Name + ": hooked (slot " + std::to_string(free_slot) + ")");
		return &slot;
	}

	// 대상이 주소이면 그 자리의 메서드가 묶인 스크립트의 이름, 아니면 정식 스크립트 이름. 못 얻으면 빈 글.
	std::string ScriptOf(const std::string& Target)
	{
		const NlCore::AskPath path = NlCore::ParseAskPath(Target);
		if (path.Error.empty() && !path.Steps.empty())
		{
			NlAccess::MethodInfo info;
			std::string why;
			return NlAccess::AboutMethod(path, info, why) ? info.Script : std::string();
		}
		return NlCore::ScriptRoutineName(Target);
	}

	std::string ForcedText(const NlRecorder::Forced& Value)
	{
		if (Value.Kind == 'x')
			return "x" + NlCore::Shortest(Value.Number) + (Value.Whole ? " (whole numbers stay whole)" : "");
		const std::string text = Value.Kind == 'n' ? NlCore::Shortest(Value.Number) : Value.Kind == 'b' ? (Value.Number != 0 ? "true" : "false") : "undefined";
		return text + (Value.Skip ? " (skip: the original is not called)" : "")
			+ (Value.Who == 'p' ? " (only when self is one of the player's souls)" : Value.Who == 'o' ? " (only when self is not one of the player's souls)" : "");
	}
}

void NlRecorder::SetPlayerSelves(std::vector<std::uintptr_t> Selves)
{
	g_PlayerSelves.Replace(std::move(Selves));
}

bool NlRecorder::Watch(const std::string& Target, std::string& Name, std::string& Why)
{
	Slot* slot = Acquire(Target, Name, Why);
	if (!slot)
		return false;

	// 기록을 비우고 다시 시작한다. 걸어 둔 바꾸기는 그대로 둔다.
	std::lock_guard lock(g_Mutex);
	slot->Log.Clear();
	slot->Recording = true;
	return true;
}

bool NlRecorder::Override(const std::string& Target, const Forced& Value, std::string& Name, std::string& Why)
{
	Slot* slot = Acquire(Target, Name, Why);
	if (!slot)
		return false;

	slot->Value = Value;
	slot->Override = true;
	slot->Matched = slot->Passed = 0;
	{
		std::lock_guard lock(g_Mutex);
		slot->Recording = true;		// 바꾼 호출이 보이게 기록도 한다(이미 하고 있으면 그대로 잇는다)
	}
	Log("override " + Name + " -> " + ForcedText(Value));
	return true;
}

int NlRecorder::Unoverride(const std::string& Name)
{
	const std::string name = Name == "all" ? std::string() : ScriptOf(Name);
	int stopped = 0;
	for (Slot& slot : g_Slots)
		if (slot.Used && slot.Override && (Name == "all" || slot.Name == name))
		{
			slot.Override = false;
			stopped++;
			Log("override " + slot.Name + ": off");
		}
	return stopped;
}

bool NlRecorder::Overriding(const std::string& Name)
{
	for (const Slot& slot : g_Slots)
		if (slot.Used && slot.Override && slot.Name == Name)
			return true;
	return false;
}

int NlRecorder::Unwatch(const std::string& Name)
{
	const std::string name = Name == "all" ? std::string() : ScriptOf(Name);
	std::lock_guard lock(g_Mutex);
	int stopped = 0;
	for (Slot& slot : g_Slots)
		if (slot.Used && slot.Recording && (Name == "all" || slot.Name == name))
		{
			slot.Recording = false;
			stopped++;
		}
	return stopped;
}

std::string NlRecorder::Report(const std::string& Name)
{
	const std::string name = Name.empty() ? std::string() : ScriptOf(Name);
	std::lock_guard lock(g_Mutex);
	std::string text;
	for (const Slot& slot : g_Slots)
		if (slot.Used && (Name.empty() || slot.Name == name))
			text += slot.Name + (slot.Recording ? "" : " (stopped)") + "\n"
				+ (slot.Override ? "  override -> " + ForcedText(slot.Value) + "\n" : "")
				+ (slot.Override && slot.Value.Who != 'a' ? "  applied to " + std::to_string(slot.Matched) + " call(s), let " + std::to_string(slot.Passed) + " pass\n" : "")
				+ slot.Log.Format("  ");
	return text;
}