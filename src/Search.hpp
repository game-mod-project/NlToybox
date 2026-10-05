#pragma once
// 이름이나 값으로 게임의 값을 찾는다. 스펙: 치트 메뉴 §7. 게임 스레드의 틱에서만 부른다.
// 한 번의 호출 안에서 끝낸다(RValue 를 틱 너머로 들지 않으려고). 그동안 게임이 멈춘다.

#include <string>
#include <vector>

namespace NlSearch
{
	struct Spec
	{
		std::string Name;			// 이름의 일부(대소문자를 가리지 않는다). 비면 이름을 보지 않는다
		bool HasValue = false;
		double Value = 0;			// HasValue 면 이 수와 같은 값만
		bool Globals = true, Instances = true, Ds = false;
	};

	struct Hit
	{
		std::string Path;			// AskPath 의 주소
		std::string Type, Text;
	};

	struct Result
	{
		std::vector<Hit> Hits;
		size_t Visited = 0;
		bool Truncated = false;		// 한도(방문 수, 시간, 결과 수)에 걸려 일부만 봤다
		size_t Skipped = 0;			// 한도(깊이, 배열의 길이, 오브젝트마다의 인스턴스 수) 때문에 들어가지 않은 그릇의 수
		double Seconds = 0;
	};

	// 이름과 값 가운데 하나는 있어야 한다(둘 다 없으면 빈 결과). 둘 다 있으면 둘 다 맞는 것만.
	Result Run(const Spec& Spec);

	// 지금 값이 Value 인 것만 남긴다.
	void Refine(std::vector<Hit>& Hits, double Value);
}
