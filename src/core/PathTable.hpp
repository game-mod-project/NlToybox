#pragma once
// 찾기가 지나온 길을 적어 둔다. 맞은 것이 나올 때만 경로 글을 만든다(값마다 글을 들고 다니지 않으려고).

#include <string>
#include <utility>
#include <vector>

namespace NlCore
{
	class PathTable
	{
	public:
		// Segment 는 앞의 구분자를 포함한다: "global", ".name", "[3]". 뿌리의 Parent 는 -1 이다. 돌려주는 값은 노드 번호.
		int Add(int Parent, std::string Segment)
		{
			m_Nodes.push_back({ Parent, std::move(Segment) });
			return static_cast<int>(m_Nodes.size()) - 1;
		}

		// 뿌리부터 Node 까지 이어 붙인 글. Node 가 -1 이면 빈 글.
		std::string Path(int Node) const
		{
			std::vector<const std::string*> parts;
			for (int at = Node; at >= 0; at = m_Nodes[at].Parent)
				parts.push_back(&m_Nodes[at].Segment);

			std::string path;
			for (auto it = parts.rbegin(); it != parts.rend(); ++it)
				path += **it;
			return path;
		}

	private:
		struct Node
		{
			int Parent;
			std::string Segment;
		};

		std::vector<Node> m_Nodes;
	};
}
