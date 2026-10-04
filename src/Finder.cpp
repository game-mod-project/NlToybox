#include "Finder.hpp"

#include "core/Text.hpp"

using namespace YYTK;
using NlCore::Number;
using NlCore::Quote;

namespace
{
	// 자가 점검의 표식. 찾기가 ds_map·ds_list 안을 실제로 보는지 확인하는 데 쓴다.
	constexpr const char* k_SelfKey = "nltoybox_selfcheck";
	constexpr double k_SelfMapValue = 918273645;
	constexpr double k_SelfListValue = 918273646;
	constexpr size_t k_MaxNested = 200;		// 이름으로 찾은 ds_map·ds_list 의 속을 적는 수

	const char* Bool(bool Value)
	{
		return Value ? "true" : "false";
	}

	// ds_map 의 키를 글로. 키는 문자열이 아닐 수도 있다.
	std::string KeyName(const RValue& Key)
	{
		if (Key.IsString())
			return Key.ToString();
		if (NlGame::IsNumber(Key))
			return Number(Key.ToDouble());
		return Key.GetKindName();
	}
}

namespace NlDump
{
	Finder::Finder(std::ostream& Out, const NlCore::Request& Request, const Limits& Bounds)
		: m_Out(Out), m_Request(Request), m_Limits(Bounds)
	{
	}

	bool Finder::Wanted() const
	{
		return !m_Request.FindValues.empty() || !m_Request.FindNames.empty();
	}

	bool Finder::MatchesName(const std::string& Name) const
	{
		for (const std::string& wanted : m_Request.FindNames)
			if (NlCore::Contains(Name, wanted))
				return true;
		return false;
	}

	void Finder::Begin(const char* Section)
	{
		m_SectionHits = 0;
		m_HitsCut = false;
		m_Out << ",\"" << Section << "\":{\"hits\":[";
	}

	void Finder::End(const std::string& Extra)
	{
		m_Out << "\n],\"visited\":" << m_Visited << ",\"containers\":" << m_Containers
			<< ",\"depth_cut\":" << m_DepthCut << ",\"array_skipped\":" << m_ArraySkipped
			<< ",\"hits_cut\":" << Bool(m_HitsCut) << ",\"truncated\":" << Bool(m_Truncated) << Extra << "}";
		m_Out.flush();
	}

	void Finder::Skip(const char* Section)
	{
		m_Out << ",\"" << Section << "\":{\"hits\":[],\"skipped\":true}";
		m_Out.flush();
	}

	void Finder::Hit(int Parent, const std::string& Segment, const char* Why, const RValue& Value)
	{
		if (m_SectionHits >= m_Limits.MaxHits)
		{
			m_HitsCut = true;
			return;
		}

		m_Out << (m_SectionHits == 0 ? "" : ",") << "\n{\"path\":" << Quote(m_Paths.Path(Parent) + Segment)
			<< ",\"why\":\"" << Why << "\"," << NlGame::Describe(Value);
		if (Value.IsStruct())
		{
			m_Out << ",\"members\":{";
			NlGame::WriteMembers(m_Out, Value, false, false);
			m_Out << "}";
		}
		m_Out << "}";
		m_Out.flush();
		m_SectionHits++;
	}

	// 이름으로 찾은 ds_map 항목이 중첩된 map·list 이면 그 속을 한 단계 적는다.
	void Finder::NestedHit(int Parent, const std::string& Segment, bool IsMap, double Id)
	{
		if (m_SectionHits >= m_Limits.MaxHits)
		{
			m_HitsCut = true;
			return;
		}

		const RValue ds(Id);
		m_Out << (m_SectionHits == 0 ? "" : ",") << "\n{\"path\":" << Quote(m_Paths.Path(Parent) + Segment)
			<< ",\"why\":\"nested\",\"kind\":\"" << (IsMap ? "ds_map" : "ds_list") << "\",\"id\":" << Number(Id) << ",\"members\":{";

		size_t written = 0;
		if (IsMap)
		{
			RValue keys;
			if (NlGame::Call("ds_map_keys_to_array", { ds }, keys) && keys.IsArray())
			{
				for (const RValue& key : keys.ToVector())
				{
					if (written >= k_MaxNested)
						break;
					RValue value;
					if (!NlGame::Call("ds_map_find_value", { ds, key }, value))
						continue;
					m_Out << (written ? "," : "") << "\n" << Quote(KeyName(key)) << ":{" << NlGame::Describe(value) << "}";
					written++;
				}
			}
		}
		else
		{
			const double size = NlGame::CallNumber("ds_list_size", { ds }, 0);
			for (int i = 0; i < size && written < k_MaxNested; i++)
			{
				RValue value;
				if (!NlGame::Call("ds_list_find_value", { ds, RValue(static_cast<double>(i)) }, value))
					continue;
				m_Out << (written ? "," : "") << "\n" << Quote(std::to_string(i)) << ":{" << NlGame::Describe(value) << "}";
				written++;
			}
		}

		m_Out << "}}";
		m_Out.flush();
		m_SectionHits++;
	}

	// 값 하나를 본다. 이름·값이 맞으면 적고, 구조체와 배열이면 큐에 넣는다.
	void Finder::Visit(const RValue& Value, int Parent, const std::string& Segment, const std::string& Name, int Depth)
	{
		if (m_Truncated)
			return;
		if (++m_Visited > m_Limits.MaxVisited)
		{
			m_Truncated = true;
			return;
		}

		if (!Name.empty() && MatchesName(Name))
			Hit(Parent, Segment, "name", Value);

		if (Value.IsStruct() || Value.IsArray())
		{
			if (Depth >= m_Limits.MaxDepth)
			{
				m_DepthCut++;
				return;
			}

			// 같은 것에 더 얕은 길로 다시 닿으면 다시 들어간다. 더 깊거나 같은 길이면 건너뛴다.
			const void* identity = Value.m_Pointer;
			if (identity)
			{
				const auto [it, inserted] = m_Depth.try_emplace(identity, Depth);
				if (!inserted)
				{
					if (it->second <= Depth)
						return;
					it->second = Depth;
				}
			}
			m_Queue.push_back({ Value, m_Paths.Add(Parent, Segment), Depth });
			return;
		}

		if (!NlGame::IsNumber(Value))
			return;
		const double number = Value.ToDouble();
		for (const double wanted : m_Request.FindValues)
			if (number == wanted)
				Hit(Parent, Segment, "value", Value);
	}

	void Finder::Expand(const Pending& Item)
	{
		m_Containers++;
		if (Item.Value.IsStruct())
		{
			NlGame::Yytk()->EnumInstanceMembers(Item.Value, [&](const char* MemberName, RValue* Member) -> bool
			{
				const std::string name = MemberName ? MemberName : "";
				if (Member)
					Visit(*Member, Item.Node, "." + name, name, Item.Depth + 1);
				return false;	// 거짓을 돌려줘야 다음 멤버로 넘어간다
			});
			return;
		}

		const double length = NlGame::ArrayLength(Item.Value);
		if (length <= 0)
			return;
		if (length > m_Limits.MaxArray)
		{
			m_ArraySkipped++;
			return;
		}
		const std::vector<RValue> items = Item.Value.ToVector();
		for (size_t i = 0; i < items.size(); i++)
			Visit(items[i], Item.Node, "[" + std::to_string(i) + "]", "", Item.Depth + 1);
	}

	void Finder::Drain()
	{
		while (!m_Queue.empty() && !m_Truncated)
		{
			const Pending item = m_Queue.front();	// Expand 가 큐에 더 넣으므로 사본으로 든다
			m_Queue.pop_front();
			Expand(item);
		}
		m_Queue.clear();
	}

	void Finder::FindGlobal(CInstance* GlobalInstance)
	{
		Begin("find_global");
		if (GlobalInstance && Wanted())
		{
			Visit(RValue(GlobalInstance), -1, "global", "", 0);
			Drain();
		}
		End("");
	}

	// ds_map 하나. 키가 너무 많아 건너뛰었으면 거짓.
	bool Finder::WalkMap(int Id, int Root, bool& SelfSeen)
	{
		const RValue ds(static_cast<double>(Id));
		if (NlGame::CallNumber("ds_map_size", { ds }, 0) > m_Limits.MaxDsKeys)
			return false;

		RValue keys;
		if (!NlGame::Call("ds_map_keys_to_array", { ds }, keys) || !keys.IsArray())
			return true;

		const int node = m_Paths.Add(Root, "_map[" + std::to_string(Id) + "]");
		for (const RValue& key : keys.ToVector())
		{
			RValue value;
			if (!NlGame::Call("ds_map_find_value", { ds, key }, value))
				continue;

			const std::string name = KeyName(key);
			if (name == k_SelfKey)
			{
				SelfSeen = true;
				continue;
			}

			const std::string segment = "." + name;
			Visit(value, node, segment, name, 1);
			if (NlGame::IsNumber(value) && MatchesName(name))
			{
				if (NlGame::CallNumber("ds_map_is_map", { ds, key }, 0) > 0)
					NestedHit(node, segment, true, value.ToDouble());
				else if (NlGame::CallNumber("ds_map_is_list", { ds, key }, 0) > 0)
					NestedHit(node, segment, false, value.ToDouble());
			}
		}
		return true;
	}

	// ds_list 하나. 너무 길어 건너뛰었으면 거짓.
	bool Finder::WalkList(int Id, int Root, bool& SelfSeen)
	{
		const RValue ds(static_cast<double>(Id));
		const double size = NlGame::CallNumber("ds_list_size", { ds }, 0);
		if (size > m_Limits.MaxArray)
			return false;

		const int node = m_Paths.Add(Root, "_list[" + std::to_string(Id) + "]");
		for (int i = 0; i < size; i++)
		{
			RValue value;
			if (!NlGame::Call("ds_list_find_value", { ds, RValue(static_cast<double>(i)) }, value))
				continue;
			if (NlGame::IsNumber(value) && value.ToDouble() == k_SelfListValue)
			{
				SelfSeen = true;
				continue;
			}
			Visit(value, node, "[" + std::to_string(i) + "]", "", 1);
		}
		return true;
	}

	void Finder::FindDataStructures()
	{
		Begin("find_ds");
		int maps = 0, lists = 0, skipped = 0;
		int highest_map = -1, highest_list = -1;
		bool self_made = false, self_map = false, self_list = false;

		if (Wanted())
		{
			// 자가 점검: 표식을 넣은 ds_map 과 ds_list 를 하나씩 만들었다가 지운다. 게임의 값은 건드리지 않는다.
			RValue made_map, made_list, ignored;
			const bool have_map = NlGame::Call("ds_map_create", {}, made_map);
			const bool have_list = NlGame::Call("ds_list_create", {}, made_list);
			if (have_map)
				NlGame::Call("ds_map_add", { made_map, RValue(k_SelfKey), RValue(k_SelfMapValue) }, ignored);
			if (have_list)
				NlGame::Call("ds_list_add", { made_list, RValue(k_SelfListValue) }, ignored);
			self_made = have_map && have_list;

			// 번호 0 부터 한도까지 모두 본다. 같은 번호가 map 과 list 에 따로 있을 수 있다(매뉴얼 ds_exists).
			// 형 상수: ds_type_map = 1, ds_type_list = 2 (YoYoGames/GameMaker-HTML5 Function_YoYo.js 36~41행).
			// 이 러너에서도 맞는지는 위의 표식을 찾았는가로 드러난다.
			const int root = m_Paths.Add(-1, "ds");
			for (int id = 0; id < m_Limits.MaxDsId && !m_Truncated; id++)
			{
				const RValue ds(static_cast<double>(id));
				if (NlGame::CallNumber("ds_exists", { ds, RValue(1.0) }, 0) > 0)
				{
					maps++;
					highest_map = id;
					if (!WalkMap(id, root, self_map))
						skipped++;
				}
				if (NlGame::CallNumber("ds_exists", { ds, RValue(2.0) }, 0) > 0)
				{
					lists++;
					highest_list = id;
					if (!WalkList(id, root, self_list))
						skipped++;
				}
			}
			Drain();

			if (have_map)
				NlGame::Call("ds_map_destroy", { made_map }, ignored);
			if (have_list)
				NlGame::Call("ds_list_destroy", { made_list }, ignored);
		}

		End(",\"maps\":" + std::to_string(maps) + ",\"lists\":" + std::to_string(lists)
			+ ",\"highest_map\":" + std::to_string(highest_map) + ",\"highest_list\":" + std::to_string(highest_list)
			+ ",\"skipped_large\":" + std::to_string(skipped)
			+ ",\"selfcheck\":{\"made\":" + Bool(self_made) + ",\"ds_map\":" + Bool(self_map) + ",\"ds_list\":" + Bool(self_list) + "}");
	}

	void Finder::FindInstances(const std::vector<InstanceRef>& Refs)
	{
		Begin("find_instances");
		size_t walked = 0;

		if (Wanted())
		{
			const int root = m_Paths.Add(-1, "instance");
			for (const InstanceRef& ref : Refs)
			{
				if (m_Truncated)
					break;
				if (ref.Number >= m_Limits.MaxInstances)
					continue;

				RValue names;
				if (!NlGame::Call("variable_instance_get_names", { ref.Id }, names) || !names.IsArray())
					continue;
				walked++;

				const int node = m_Paths.Add(root, ":" + ref.Object + "#" + std::to_string(ref.Number));
				for (const RValue& name : names.ToVector())
				{
					if (!name.IsString())
						continue;
					RValue value;
					if (!NlGame::Call("variable_instance_get", { ref.Id, name }, value))
						continue;
					const std::string text = name.ToString();
					Visit(value, node, "." + text, text, 1);
				}
			}
			Drain();
		}

		End(",\"instances\":" + std::to_string(walked));
	}
}
