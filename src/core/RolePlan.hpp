#pragma once
// 인물의 역할 프리셋(사용자 요청 2026-10-06. research/22): 역할마다 올릴 능력치, 붙일 재능·자질, 뗄 특성.
// 능력치와 특성의 뜻은 게임의 설명 글을 읽어 골랐다(재능마다의 효과를 게임에서 재지는 않았다). 사용자가 표를 보고 정했다:
// 능력치는 올리기만 하고(이미 더 높은 것은 그대로), 특성은 붙이고, 역할에 해로운 특성은 뗀다.
// 붙이고 떼고 쓰는 길은 인물 패널의 것이다(능력치: __soul.__skills.__level 에 쓰기, 특성: Traits.trait_attach·trait_detach. research/11).
// 한 사람에게 재능을 30개까지 붙여도 게임이 거부하지 않았다(71번 붙여 71번. research/21).

#include <string>
#include <utility>
#include <vector>

namespace NlCore
{
	struct RoleSkill
	{
		const char* Skill;		// SkillNames 의 열쇠(combat, command, education, knowledge, management, manners, negotiation, oratory)
		int Level;				// 이 수까지 올린다(1 ~ 20)
	};

	struct RolePreset
	{
		const char* Id;			// 원격 명령의 이름(person <uuid> role name=<Id>)
		const char* Label;		// 창에 보일 이름
		std::vector<RoleSkill> Skills;
		std::vector<const char*> Add;		// 붙일 특성(게임의 이름)
		std::vector<const char*> Remove;	// 뗄 특성(그 역할에 해로운 것. 가진 것만 뗀다)
	};

	// 프리셋 열셋(창에 보이는 차례대로).
	const std::vector<RolePreset>& RolePresets();
	const RolePreset* FindRole(const std::string& Id);
	// 표가 말이 되는가: Id 가 겹치지 않고, 능력치의 열쇠가 SkillNames 에 있고 수가 1 ~ 20 이고 한 프리셋 안에서 겹치지 않고,
	// 특성의 이름이 이름의 꼴이고 보호된 특성(종, 죽음)이 아니고, 붙일 것과 뗄 것이 겹치지 않는다. 아니면 거짓이고 Why 에 까닭.
	bool CheckRoles(std::string& Why);

	// 한 사람에게 그 프리셋을 입힐 때 할 일.
	struct RoleTodo
	{
		std::vector<std::pair<int, int>> Skills;	// (SkillNames 의 자리, 써 넣을 수). 지금 값이 그보다 낮은 것만
		std::vector<std::string> Add;				// 아직 가지지 않은 것만
		std::vector<std::string> Remove;			// 지금 가진 것만
		bool Empty() const { return Skills.empty() && Add.empty() && Remove.empty(); }
	};
	// Skills: SkillNames 의 차례대로의 지금 값. 읽지 못한 능력치(수가 아닌 값, 음수, 모자란 칸)는 건드리지 않는다(주민에게는 전투만 있다).
	// Traits: 지금 가진 특성.
	RoleTodo PlanRole(const RolePreset& Role, const std::vector<double>& Skills, const std::vector<std::string>& Traits);

	// 할 일을 걸음의 차례로: 능력치 → 떼기 → 붙이기. 해로운 특성을 먼저 뗀다
	// (그것을 가진 사람에게 맞서는 재능을 게임이 붙여 주는지는 재지 않았다. 거부하든 스스로 떼든 먼저 떼는 쪽이 낫다).
	struct RoleStep
	{
		char Kind = 's';			// 's' 능력치를 쓴다, 'r' 특성을 뗀다, 'a' 특성을 붙인다
		int Index = -1, Level = 0;	// 's': SkillNames 의 자리와 써 넣을 수
		std::string Name;			// 'r', 'a': 특성의 이름
	};
	std::vector<RoleStep> RoleSteps(const RoleTodo& Todo);
	// 그 걸음을 지금 해야 하는가. TraitsNow: 걸음 바로 앞에 다시 읽은 특성(앞의 걸음이나 게임이 목록을 바꿨을 수 있다).
	// 뗄 것이 이미 없거나 붙일 것이 이미 있으면 거짓: 게임의 함수를 부르지 않는다(그런 상태에서 불러 본 적이 없다). 능력치의 걸음은 언제나 참.
	bool RoleStepNeeded(const RoleStep& Step, const std::vector<std::string>& TraitsNow);

	// 누르기 전에 보일 요약(걸음의 차례대로): "능력치 2개를 올리고, 특성 1개를 떼고, 3개를 붙입니다". 할 일이 없으면 그렇다고 말한다.
	std::string RolePreview(const RoleTodo& Todo);
	// 그 특성을 떼기 전에 알릴 것. **잰 것만 적는다**(재지 않은 특성에는 빈 글).
	const char* RemoveNote(const std::string& Trait);

	// 결과의 글. Skills·Added·Removed: 한 것의 수. Failed: 하지 못한 것의 수(Why 에 첫 까닭).
	std::string RoleReport(const std::string& Name, const RolePreset& Role, int Skills, int Added, int Removed, int Failed, const std::string& Why);
}
