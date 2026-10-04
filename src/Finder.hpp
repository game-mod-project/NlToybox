#pragma once
// 값과 이름으로 찾기. 스펙: 데이터 오버레이 §3.7.
// 구조체와 배열은 너비 우선으로 내려가고, 닿은 가장 얕은 깊이를 적어 둔다. 깊은 길로 먼저 닿아서 빠지는 것이 없다.

#include "Game.hpp"
#include "core/PathTable.hpp"
#include "core/Request.hpp"

#include <deque>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace NlDump
{
	struct Limits
	{
		int MaxDepth = 6;				// 찾기가 내려가는 깊이
		size_t MaxVisited = 2000000;	// 찾기가 방문하는 값의 수(구역을 통틀어)
		double MaxArray = 64;			// 이보다 긴 배열과 ds_list 는 들어가지 않는다
		size_t MaxHits = 300;			// 구역마다 적는 수
		double MaxDsKeys = 20000;		// 키가 이보다 많은 ds_map 은 들어가지 않는다
		int MaxDsId = 100000;			// ds 번호를 0 부터 여기까지 모두 본다. 가장 큰 번호를 덤프에 적어 범위가 모자라지 않았는지 본다
		int MaxInstances = 16;			// 오브젝트마다 찾기가 들어가는 인스턴스 수
	};

	// 찾기가 들어갈 인스턴스 하나.
	struct InstanceRef
	{
		std::string Object;
		int Number = 0;			// 그 오브젝트의 인스턴스 가운데 몇 번째인가
		YYTK::RValue Id;		// instance_find 가 돌려준 값. 빌트인에 그대로 넘긴다
	};

	class Finder
	{
	public:
		Finder(std::ostream& Out, const NlCore::Request& Request, const Limits& Bounds);

		// 저마다 JSON 구역 하나를 통째로 쓴다: ,"find_global":{…}  ,"find_ds":{…}  ,"find_instances":{…}
		void FindGlobal(YYTK::CInstance* GlobalInstance);
		void FindDataStructures();
		void FindInstances(const std::vector<InstanceRef>& Refs);

		// 구역을 건너뛰었다고 쓴다.
		void Skip(const char* Section);

		size_t Visited() const { return m_Visited; }

	private:
		struct Pending
		{
			YYTK::RValue Value;
			int Node;
			int Depth;
		};

		bool Wanted() const;
		bool MatchesName(const std::string& Name) const;
		void Begin(const char* Section);
		void End(const std::string& Extra);
		void Hit(int Parent, const std::string& Segment, const char* Why, const YYTK::RValue& Value);
		void NestedHit(int Parent, const std::string& Segment, bool IsMap, double Id);
		void Visit(const YYTK::RValue& Value, int Parent, const std::string& Segment, const std::string& Name, int Depth);
		void Expand(const Pending& Item);
		void Drain();
		bool WalkMap(int Id, int Root, bool& SelfSeen);
		bool WalkList(int Id, int Root, bool& SelfSeen);

		std::ostream& m_Out;
		const NlCore::Request& m_Request;
		Limits m_Limits;
		NlCore::PathTable m_Paths;
		std::deque<Pending> m_Queue;
		std::unordered_map<const void*, int> m_Depth;	// 구조체·배열마다 닿은 가장 얕은 깊이
		size_t m_Visited = 0;
		size_t m_Containers = 0;
		size_t m_DepthCut = 0;			// 깊이 한도에서 멈춘 구조체·배열의 수
		size_t m_ArraySkipped = 0;		// 길어서 들어가지 않은 배열의 수
		size_t m_SectionHits = 0;
		bool m_HitsCut = false;
		bool m_Truncated = false;
	};
}
