#include "Recorder.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/AskPath.hpp"
#include "core/CallLog.hpp"
#include "core/Text.hpp"

#include <array>
#include <mutex>
#include <utility>

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
		bool Recording = false;
		std::string Name;
		PFUNC_YYGMLScript Target = nullptr;		// 훅을 건 함수
		PFUNC_YYGMLScript Original = nullptr;	// 원래 함수로 가는 트램펄린
		NlCore::CallLog Log;
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
		if (Value.IsNumberConvertible())
			return NlCore::Shortest(Value.ToDouble());
		return KindName(kind);
	}

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

		RValue& out = original(Self, Other, Result, Count, Args);

		if (sample)
		{
			std::lock_guard lock(g_Mutex);
			slot.Log.Sample(sample, key, std::move(shape), std::move(args), Brief(out));
		}
		return out;
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

bool NlRecorder::Watch(const std::string& Target, std::string& Name, std::string& Why)
{
	std::string given = Target;

	// 주소이면 그 자리의 메서드가 묶인 스크립트의 이름을 묻는다.
	const NlCore::AskPath path = NlCore::ParseAskPath(Target);
	if (path.Error.empty() && !path.Steps.empty())
	{
		NlAccess::MethodInfo info;
		if (!NlAccess::AboutMethod(path, info, Why))
			return false;
		if (info.Script.empty())
		{
			Why = "the method has no script name: " + Target;
			return false;
		}
		given = info.Script;
	}

	PFUNC_YYGMLScript fn = nullptr;
	if (!FindScript(given, Name, fn, Why))
		return false;

	std::lock_guard lock(g_Mutex);
	int free_slot = -1;
	for (int i = 0; i < k_Slots; i++)
	{
		if (g_Slots[i].Used && g_Slots[i].Target == fn)
		{
			// 이미 훅이 걸려 있다. 기록을 비우고 다시 시작한다.
			g_Slots[i].Log.Clear();
			g_Slots[i].Recording = true;
			return true;
		}
		if (!g_Slots[i].Used && free_slot < 0)
			free_slot = i;
	}
	if (free_slot < 0)
	{
		Why = "no free hook slot (" + std::to_string(k_Slots) + " in use)";
		return false;
	}

	Slot& slot = g_Slots[free_slot];
	slot.Name = Name;
	slot.Target = fn;
	slot.Original = nullptr;
	slot.Log.Clear();
	const AurieStatus status = MmCreateHook(g_Module, "NlToyBox.rec." + std::to_string(free_slot), reinterpret_cast<PVOID>(fn),
		reinterpret_cast<PVOID>(k_Detours[free_slot]), reinterpret_cast<PVOID*>(&slot.Original));
	if (!AurieSuccess(status) || !slot.Original)
	{
		// 실패한 자리는 다시 쓰지 않는다: 같은 훅 이름("NlToyBox.rec.N")을 Aurie 에 두 번 주지 않는다(Aurie 가 실패한 이름을
		// 표에 남기는지는 모른다. 소스가 레포에 없다). 자리는 넉넉하다.
		slot.Used = true;
		slot.Recording = false;
		slot.Name = Name + " (hook failed)";
		slot.Target = nullptr;
		slot.Original = nullptr;
		Why = AurieSuccess(status) ? "MmCreateHook gave no trampoline" : std::string("MmCreateHook ") + AurieStatusToString(status);
		return false;
	}
	slot.Used = true;
	slot.Recording = true;
	Log("record " + Name + ": hooked (slot " + std::to_string(free_slot) + ")");
	return true;
}

int NlRecorder::Unwatch(const std::string& Name)
{
	const std::string name = NlCore::ScriptRoutineName(Name);
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
	const std::string name = NlCore::ScriptRoutineName(Name);
	std::lock_guard lock(g_Mutex);
	std::string text;
	for (const Slot& slot : g_Slots)
		if (slot.Used && (Name.empty() || slot.Name == name))
			text += slot.Name + (slot.Recording ? "" : " (stopped)") + "\n" + slot.Log.Format("  ");
	return text;
}
