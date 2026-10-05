#include "CallLog.hpp"

#include <algorithm>
#include <utility>

namespace NlCore
{
	uint64_t ShapeKey(const int* Kinds, int Count)
	{
		// FNV-1a. 수와 형을 차례로 섞는다.
		uint64_t key = 14695981039346656037ull;
		const auto mix = [&](uint64_t value) {
			key ^= value;
			key *= 1099511628211ull;
		};
		mix(static_cast<uint64_t>(Count) + 1);
		for (int i = 0; i < Count && Kinds; i++)
			mix(static_cast<uint64_t>(static_cast<int64_t>(Kinds[i])) + 0x100);
		return key;
	}

	size_t CallLog::Note(uint64_t Key)
	{
		m_Calls++;
		const auto [it, fresh] = m_Shapes.try_emplace(Key);
		it->second.Count++;
		return (fresh || m_Calls <= m_KeepFirst) && m_Samples.size() < m_MaxSamples ? m_Calls : 0;
	}

	void CallLog::Sample(size_t Index, uint64_t Key, std::string Shape, std::string Args, std::string Result)
	{
		const auto it = m_Shapes.find(Key);
		if (it != m_Shapes.end() && it->second.Text.empty())
			it->second.Text = Shape;
		m_Samples.push_back({ Index, std::move(Shape), std::move(Args), std::move(Result) });
	}

	void CallLog::Clear()
	{
		m_Calls = 0;
		m_Shapes.clear();
		m_Samples.clear();
	}

	std::string CallLog::Format(const std::string& Indent) const
	{
		std::string text = Indent + "calls " + std::to_string(m_Calls) + ", shapes " + std::to_string(m_Shapes.size()) + "\n";

		// 많이 나온 꼴부터. 수가 같으면 글의 차례로(열쇠의 차례는 뜻이 없다).
		std::vector<const ShapeInfo*> shapes;
		for (const auto& [key, info] : m_Shapes)
			shapes.push_back(&info);
		std::sort(shapes.begin(), shapes.end(), [](const ShapeInfo* a, const ShapeInfo* b) {
			return a->Count != b->Count ? a->Count > b->Count : a->Text < b->Text;
		});
		for (const ShapeInfo* info : shapes)
			text += Indent + "shape " + (info->Text.empty() ? "(?)" : info->Text) + " x" + std::to_string(info->Count) + "\n";

		// 표본은 호출의 차례로. 겹쳐 불리면(되부름) 안쪽 호출이 먼저 끝나 먼저 들어온다.
		std::vector<const CallSample*> samples;
		for (const CallSample& sample : m_Samples)
			samples.push_back(&sample);
		std::sort(samples.begin(), samples.end(), [](const CallSample* a, const CallSample* b) { return a->Index < b->Index; });
		for (const CallSample* sample : samples)
			text += Indent + "#" + std::to_string(sample->Index) + " " + sample->Args + " -> " + sample->Result + "\n";
		return text;
	}
}
