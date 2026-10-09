#pragma once
// 생산의 자료를 걷는 함수와 그 일의 등록(치트 표의 Custom·CustomScale). 엔진은 src/Jobs 에 있다(2026-10-07 리팩토링 C). 자리와 잰 것은 research/10.
//   storage_capacity   창고 종류마다의 갈래별 용량에 배율을 쓴다
//   production_amount  조리법의 "만들어지는 수"에 배율을 쓴다
//   production_free    조리법의 재료를 0 으로 쓴다

#include <functional>
#include <string>

namespace NlProduction
{
	using LogFn = std::function<void(const std::string&)>;

	// ModuleInitialize 에서 한 번(NlJobs::Init 뒤에). 일들을 NlJobs 에 등록한다.
	void Init(LogFn Log);
}
