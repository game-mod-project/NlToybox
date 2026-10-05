#pragma once
// 훅을 건 함수의 호출 기록. 러너에 기대지 않는다. 스펙: 치트 메뉴 §14(호출 기록기).
// 호출은 모두 세고, 글로 남기는 것(표본)은 처음 몇 개와 인자의 꼴(수와 형)이 처음 나온 것뿐이다.
// 자주 불리는 함수를 기록해도 글을 만드는 비용이 쌓이지 않는다.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace NlCore
{
	// 인자의 수와 형으로 만든 열쇠. Kinds: 인자마다의 형 번호.
	uint64_t ShapeKey(const int* Kinds, int Count);

	struct CallSample
	{
		size_t Index = 0;			// 몇 번째 호출이었나(1 부터)
		std::string Shape;			// "(number, string)"
		std::string Args;			// "(50, \"wood\")"
		std::string Result;
	};

	class CallLog
	{
	public:
		explicit CallLog(size_t KeepFirst = 6, size_t MaxSamples = 40) : m_KeepFirst(KeepFirst), m_MaxSamples(MaxSamples) {}

		// 호출 하나를 센다. 이 호출을 글로 남겨 달라는 뜻이면 호출의 번호(1 부터)를, 아니면 0 을 돌려준다.
		size_t Note(uint64_t Key);
		// Note 가 번호를 돌려준 호출의 글.
		void Sample(size_t Index, uint64_t Key, std::string Shape, std::string Args, std::string Result);
		void Clear();

		size_t Calls() const { return m_Calls; }
		size_t ShapeCount() const { return m_Shapes.size(); }
		// 사람이 읽는 글. 줄마다 앞에 Indent 를 붙인다.
		std::string Format(const std::string& Indent) const;

	private:
		struct ShapeInfo
		{
			std::string Text;
			size_t Count = 0;
		};

		size_t m_KeepFirst, m_MaxSamples;
		size_t m_Calls = 0;
		std::map<uint64_t, ShapeInfo> m_Shapes;
		std::vector<CallSample> m_Samples;
	};
}
