#pragma once
// 값과 이름으로 찾기. 스펙: 데이터 오버레이 §3.7.
// 구조체와 배열은 너비 우선으로 내려가고, 닿은 가장 얕은 깊이를 적어 둔다. 깊은 길로 먼저 닿아서 빠지는 것이 없다.
// 한도와 통계는 구역(전역, ds, 인스턴스)마다 따로 센다. 한 구역이 한도에 걸려도 다음 구역은 제 몫을 본다.

#include "Game.hpp"
#include "core/PathTable.hpp"
#include "core/Request.hpp"

#include <deque>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace NlDump
{
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
		// 한도는 Request.Bounds 에서 읽는다.
		Finder(std::ostream& Out, const NlCore::Request& Request);

		// 저마다 JSON 구역 하나를 통째로 쓴다: ,"find_global":{…}  ,"find_ds":{…}  ,"find_instances":{…}
		void FindGlobal(YYTK::CInstance* GlobalInstance);
		void FindDataStructures();
		void FindInstances(const std::vector<InstanceRef>& Refs);

		// 구역을 건너뛰었다고 쓴다.
		void Skip(const char* Section);

		// 방금 끝난 구역에서 방문한 값의 수와 맞은 것의 수.
		size_t Visited() const { return m_Visited; }
		size_t Matches() const { return m_Matches; }

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
		bool TakeHit();
		void Hit(int Parent, const std::string& Segment, const char* Why, const YYTK::RValue& Value);
		void NestedHit(int Parent, const std::string& Segment, bool IsMap, double Id);
		void TooLong(const std::string& Path, double Length);
		void Visit(const YYTK::RValue& Value, int Parent, const std::string& Segment, const std::string& Name, int Depth);
		void Expand(const Pending& Item);
		void Drain();
		void WalkMap(int Id, int Root, bool& SelfSeen);
		void WalkList(int Id, int Root, bool& SelfSeen);

		std::ostream& m_Out;
		const NlCore::Request& m_Request;
		const NlCore::Limits& m_Limits;
		NlCore::PathTable m_Paths;
		std::deque<Pending> m_Queue;
		std::unordered_map<const void*, int> m_Depth;	// 구조체·배열마다 닿은 가장 얕은 깊이 (구역을 넘어 이어진다)

		// 아래는 구역마다 다시 센다 (Begin).
		size_t m_Visited = 0;
		size_t m_Containers = 0;
		size_t m_DepthCut = 0;			// 깊이 한도에서 멈춘 구조체·배열의 수
		size_t m_Matches = 0;			// 맞은 것의 수 (적지 못한 것도 센다)
		size_t m_Written = 0;			// 적은 히트의 수
		size_t m_EnumFailed = 0;		// 멤버 열거가 오류로 끝난 구조체의 수
		size_t m_EnumShort = 0;			// 러너가 말한 멤버 수보다 적게 본 구조체의 수
		bool m_HitsCut = false;
		bool m_Truncated = false;
		bool m_TooLongCut = false;
		std::vector<std::pair<std::string, double>> m_TooLong;	// 길어서 들어가지 않은 배열·ds 의 경로와 길이
	};
}
