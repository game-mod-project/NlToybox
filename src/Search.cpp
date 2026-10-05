#include "Search.hpp"

#include "Access.hpp"
#include "Game.hpp"
#include "core/PathTable.hpp"
#include "core/Text.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <unordered_set>

using namespace YYTK;
using NlAccess::Holder;
using NlCore::PathStep;
using Clock = std::chrono::steady_clock;

namespace
{
	// 한도. 스펙 §7.
	constexpr int k_MaxDepth = 10;
	constexpr size_t k_MaxVisited = 3'000'000;
	constexpr size_t k_MaxHits = 500;
	constexpr double k_MaxArray = 4096;
	constexpr double k_MaxSeconds = 3.0;
	constexpr int k_MaxPerObject = 64;			// 오브젝트마다 들어가는 인스턴스 수
	constexpr int k_MaxDsId = 20000;			// ds 번호를 훑는 범위. 게임 안에서 본 가장 큰 번호는 1,467 이다(research/02)
	constexpr int k_MaxMissing = 500;			// 이만큼 잇달아 비어 있으면 그만 훑는다(번호는 0 부터 빈틈없이 배정돼 있었다)

	char LowerChar(char C)
	{
		return C >= 'A' && C <= 'Z' ? static_cast<char>(C - 'A' + 'a') : C;
	}

	std::string Lower(std::string Text)
	{
		for (char& c : Text)
			c = LowerChar(c);
		return Text;
	}

	// Text 안에 LowerPart 가 있는가(영문 대소문자를 가리지 않는다). LowerPart 는 이미 소문자다.
	bool ContainsNoCase(const std::string& Text, const std::string& LowerPart)
	{
		if (LowerPart.size() > Text.size())
			return false;
		for (size_t at = 0; at + LowerPart.size() <= Text.size(); at++)
		{
			size_t k = 0;
			while (k < LowerPart.size() && LowerChar(Text[at + k]) == LowerPart[k])
				k++;
			if (k == LowerPart.size())
				return true;
		}
		return false;
	}

	struct Pending
	{
		RValue Value;
		Holder Kind;
		int Node;
		int Depth;
	};

	// 너비 우선으로 내려간다. 모든 RValue 는 Run 이 끝나기 전에 사라진다.
	class Walker
	{
	public:
		explicit Walker(const NlSearch::Spec& Spec) : m_Spec(Spec), m_Name(Lower(Spec.Name)), m_Start(Clock::now()) {}

		// 뿌리 하나와 그 아래를 다 본다. 뿌리 자체는 맞는 것으로 치지 않는다.
		void Root(const RValue& Value, Holder Kind, const std::string& Address)
		{
			if (m_Result.Truncated)
				return;
			m_Queue.push_back({ Value, Kind, m_Paths.Add(-1, Address), 0 });
			while (!m_Queue.empty() && !m_Result.Truncated)
			{
				const Pending item = m_Queue.front();		// Offer 가 큐에 더 넣으므로 사본으로 든다
				m_Queue.pop_front();
				NlAccess::ForEachChild(item.Value, item.Kind, [&](const PathStep& step, const RValue& child) {
					Offer(child, item.Node, step, item.Depth + 1);
					return !m_Result.Truncated;
				});
			}
			m_Queue.clear();
		}

		// 한도 때문에 들어가지 않은 뿌리를 센다.
		void Skip()
		{
			m_Result.Skipped++;
		}

		NlSearch::Result Finish()
		{
			m_Result.Seconds = Elapsed();
			return std::move(m_Result);
		}

	private:
		double Elapsed() const
		{
			return std::chrono::duration<double>(Clock::now() - m_Start).count();
		}

		void Offer(const RValue& Value, int Parent, const PathStep& Step, int Depth)
		{
			m_Result.Visited++;
			if (m_Result.Visited > k_MaxVisited || ((m_Result.Visited & 0xFFF) == 0 && Elapsed() > k_MaxSeconds))
			{
				m_Result.Truncated = true;
				return;
			}

			// 이름은 멤버와 ds_map 의 키에만 있다. 배열과 리스트의 원소는 값으로만 맞는다.
			const bool name_ok = m_Name.empty() || ((Step.Kind == '.' || Step.Kind == '@') && ContainsNoCase(Step.Name, m_Name));
			const bool value_ok = !m_Spec.HasValue || (NlGame::IsNumber(Value) && Value.ToDouble() == m_Spec.Value);
			if (name_ok && value_ok)
			{
				if (m_Result.Hits.size() >= k_MaxHits)
				{
					m_Result.Truncated = true;
					return;
				}
				const NlAccess::Row row = NlAccess::Describe(Step, Value);
				m_Result.Hits.push_back({ m_Paths.Path(Parent) + NlCore::FormatStep(Step), row.Type, row.Text });
			}

			// 구조체와 배열만 내려간다. ref(인스턴스)는 인스턴스 뿌리에서 따로 본다.
			const Holder kind = Value.IsArray() ? Holder::Array : Value.IsStruct() ? Holder::Struct : Holder::None;
			if (kind == Holder::None)
				return;
			if (Value.m_Pointer && m_Seen.count(Value.m_Pointer))
				return;			// 너비 우선이라 처음 닿은 길이 가장 얕다
			if (Depth >= k_MaxDepth || (kind == Holder::Array && NlGame::ArrayLength(Value) > k_MaxArray))
			{
				m_Result.Skipped++;		// 한도 때문에 들어가지 않는다. "없다"를 믿으면 안 되는 자리다
				return;
			}
			if (Value.m_Pointer)
				m_Seen.insert(Value.m_Pointer);
			m_Queue.push_back({ Value, kind, m_Paths.Add(Parent, NlCore::FormatStep(Step)), Depth });
		}

		const NlSearch::Spec& m_Spec;
		std::string m_Name;
		Clock::time_point m_Start;
		NlCore::PathTable m_Paths;
		std::deque<Pending> m_Queue;
		std::unordered_set<const void*> m_Seen;
		NlSearch::Result m_Result;
	};
}

NlSearch::Result NlSearch::Run(const Spec& Spec)
{
	Walker walker(Spec);
	if (Spec.Name.empty() && !Spec.HasValue)
		return walker.Finish();

	if (Spec.Globals)
		if (CInstance* global = NlGame::Global())
			walker.Root(RValue(global), Holder::Global, "global");

	if (Spec.Instances)
	{
		// 인스턴스 수가 적은 오브젝트부터 본다. instance_number 와 instance_find 는 자식 오브젝트의 인스턴스도
		// 포함하므로(매뉴얼), 그래야 인스턴스가 부모가 아니라 자기 오브젝트의 이름으로 적힌다(Dump.cpp 의 CollectInstances 와 같다).
		std::vector<std::pair<int, const NlGame::Object*>> order;
		for (const NlGame::Object& object : NlGame::Objects())
		{
			const int count = static_cast<int>(NlGame::CallNumber("instance_number", { RValue(object.Index) }, 0));
			if (count > 0)
				order.push_back({ count, &object });
		}
		std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::unordered_set<int64_t> seen;
		for (const auto& [count, object] : order)
			for (int n = 0; n < count; n++)
			{
				RValue id;
				if (!NlGame::Call("instance_find", { RValue(object->Index), RValue(static_cast<double>(n)) }, id))
					continue;
				if ((NlGame::IsNumber(id) && id.ToDouble() < 0) || !seen.insert(id.m_i64).second)		// noone, 이미 본 것
					continue;
				if (n >= k_MaxPerObject)
					walker.Skip();		// 오브젝트마다 앞의 k_MaxPerObject 개만 들어간다. 나머지는 센다
				else
					walker.Root(id, Holder::Instance, "inst:" + object->Name + (n > 0 ? ":" + std::to_string(n) : ""));
			}
	}

	if (Spec.Ds)
	{
		// ds 형 상수 1(map)과 2(list)는 이 러너에서 맞다(research/02).
		int missing = 0;
		for (int id = 0; id < k_MaxDsId && missing < k_MaxMissing; id++)
		{
			const RValue number(static_cast<double>(id));
			const bool map = NlGame::CallNumber("ds_exists", { number, RValue(1.0) }, 0) > 0;
			const bool list = NlGame::CallNumber("ds_exists", { number, RValue(2.0) }, 0) > 0;
			missing = map || list ? 0 : missing + 1;
			if (map)
				walker.Root(number, Holder::Map, "map:" + std::to_string(id));
			if (list)
				walker.Root(number, Holder::List, "list:" + std::to_string(id));
		}
	}
	return walker.Finish();
}

void NlSearch::Refine(std::vector<Hit>& Hits, double Value)
{
	std::erase_if(Hits, [&](Hit& hit) {
		double now = 0;
		if (!NlAccess::ReadNumber(hit.Path, now) || now != Value)
			return true;
		hit.Text = NlCore::Shortest(now);
		return false;
	});
}
