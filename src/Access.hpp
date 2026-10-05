#pragma once
// 주소(core/AskPath)를 따라가 게임의 값을 읽고, 쓰고, 자식을 늘어놓는다. 스펙: 치트 메뉴 §5.
// 모든 함수는 게임 스레드의 틱에서만 부른다. RValue 는 부른 쪽의 함수 안에서만 든다.

#include "core/AskPath.hpp"

#include <YYTK_Shared.hpp>

#include <functional>
#include <string>
#include <vector>

namespace NlAccess
{
	// 값을 어떤 그릇으로 여는가. 수는 그릇이 아니지만 ds 번호로 열 수 있다(Map, List).
	enum class Holder { None, Global, Instance, Struct, Array, Map, List };

	// 값 하나를 글로 적은 것. RValue 를 담지 않으므로 틱 너머로 들고 있어도 된다.
	struct Row
	{
		NlCore::PathStep Step;		// 부모에서 이 값으로 가는 단계
		std::string Name;			// 표에 보일 이름
		std::string Type;			// "number", "bool", "string", "struct", "method", "array", "ref", "undefined", …
		std::string Text;			// 값의 글. 그릇이면 크기
		std::string Raw;			// 문자열이면 그 글 그대로
		double Number = 0;			// 수·불리언이면 그 값
		bool IsNumber = false, IsBool = false, IsString = false, IsContainer = false;
	};

	struct RootObject
	{
		std::string Name;
		int Count = 0;				// instance_number. 자식 오브젝트의 인스턴스도 센다
	};

	// 주소가 가리키는 값. 없으면 거짓이고 Why 에 까닭.
	bool Read(const NlCore::AskPath& Path, YYTK::RValue& Out, std::string& Why);
	// 값과 그것을 여는 방식.
	bool Open(const NlCore::AskPath& Path, YYTK::RValue& Out, Holder& Kind, std::string& Why);

	// 있는 자리에만 쓴다. 쓴 뒤 뿌리부터 다시 읽어 그 값인지 본다. 아니면 거짓이고 Why 는 "did not stick".
	bool Write(const NlCore::AskPath& Path, const YYTK::RValue& Value, std::string& Why);

	// 수(불리언 포함)로 읽고 쓴다. 수가 아닌 값은 수로 덮어쓰지 않는다. 형은 원래 값의 것을 따른다.
	bool ReadNumber(const std::string& Path, double& Out);
	bool WriteNumber(const std::string& Path, double Number, std::string& Why);
	// 문자열인 자리에만 쓴다.
	bool WriteString(const std::string& Path, const std::string& Text, std::string& Why);

	// 값을 글로 적는다.
	Row Describe(const NlCore::PathStep& Step, const YYTK::RValue& Value);

	// 값을 보고 여는 방식을 정한다(구조체, 배열, 있는 인스턴스를 가리키는 ref). 그 밖은 None.
	Holder Classify(const YYTK::RValue& Value);

	// 그릇의 자식을 차례로 넘긴다. Visit 이 거짓을 돌려주면 그만둔다.
	// 돌려주는 값: 자식의 수(구조체와 전역은 도중에 그만두면 그때까지 본 수). 그릇이 아니거나 없는 ds 면 -1.
	double ForEachChild(const YYTK::RValue& Value, Holder Kind,
		const std::function<bool(const NlCore::PathStep&, const YYTK::RValue&)>& Visit);

	// 주소가 가리키는 그릇의 자식들. As: 그것이 수일 때 Map 이나 List 로 연다. Limit 개까지만 적고 Total 은 전체 수.
	bool List(const NlCore::AskPath& Path, Holder As, size_t Limit, std::vector<Row>& Rows, size_t& Total, std::string& Why);

	// 인스턴스가 하나라도 있는 오브젝트들.
	std::vector<RootObject> LiveObjects();
	int InstanceCount(const std::string& Object);

	// 게임 화면인가: o_main_menu 가 없고 o_character 가 있다(research/02. 새 게임에서 잰 것이다).
	bool InGame();
}
