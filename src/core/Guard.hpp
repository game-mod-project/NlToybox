#pragma once
// 재진입 가드. 틱이 부른 게임의 함수가 오브젝트 이벤트를 일으켜 같은 틱을 다시 부르면 안쪽은 아무것도 하지 않는다(CLAUDE.md "모듈을 쓸 때").
// 쓰는 꼴:  if (g_Busy) return;  const NlCore::ScopedFlag busy(g_Busy);
// 사는 동안 깃발을 세우고 죽으면 내린다. 겹쳐 쓰지 않는다(안쪽이 죽으면 내려간다). 러너에 기대지 않는다.

namespace NlCore
{
	class ScopedFlag
	{
	public:
		explicit ScopedFlag(bool& Flag) : m_Flag(Flag) { m_Flag = true; }
		~ScopedFlag() { m_Flag = false; }
		ScopedFlag(const ScopedFlag&) = delete;
		ScopedFlag& operator=(const ScopedFlag&) = delete;

	private:
		bool& m_Flag;
	};
}
