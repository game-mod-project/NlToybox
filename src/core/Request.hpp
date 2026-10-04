#pragma once
// 요청 파일(NlToyBox.probe.txt)의 형식. 스펙: 데이터 오버레이 §3.2, §3.7.

#include <istream>
#include <string>
#include <vector>

namespace NlCore
{
	struct ScriptCall
	{
		std::string Name;
		std::vector<std::string> Args;
	};

	// 찾기의 한도. 요청 파일로 바꿀 수 있다(예비 실행에서 모듈을 고치지 않고 조정하려고).
	struct Limits
	{
		int MaxDepth = 6;				// 찾기가 내려가는 깊이 (max_depth)
		double MaxVisited = 2000000;	// 구역마다 찾기가 방문하는 값의 수 (max_visited)
		double MaxArray = 64;			// 이보다 긴 배열과 ds_list 는 들어가지 않는다 (max_array)
		int MaxHits = 2000;				// 구역마다 적는 히트의 수 (max_hits)
		double MaxDsKeys = 20000;		// 키가 이보다 많은 ds_map 은 들어가지 않는다 (max_ds_keys)
		int MaxDsId = 100000;			// ds 번호를 0 부터 여기까지 모두 본다 (max_ds_id)
		int MaxInstances = 16;			// 오브젝트마다 찾기가 들어가는 인스턴스 수 (max_instances)
	};

	struct Request
	{
		Limits Bounds;
		int DelaySeconds = 60;			// 첫 덤프(menu)까지
		double RepeatSeconds = 0;		// 0 이면 첫 덤프만 한다. 아니면 앞 덤프가 끝난 뒤 이만큼 지나 다시 덤프한다
		int KeepLast = 3;				// 되풀이 덤프는 마지막 이만큼만 남긴다
		bool TraceEvents = false;
		std::vector<double> FindValues;
		std::vector<std::string> FindNames;
		std::vector<ScriptCall> Scripts;
		std::vector<std::string> Watches;	// "global.a.b"
		std::vector<std::string> Skip;		// "ds", "instances", "room"
		std::vector<std::string> Errors;	// 비어 있지 않으면 이 요청으로 아무것도 하지 않는다
	};

	// 줄 단위 키=값. '#' 으로 시작하는 줄과 빈 줄은 건너뛴다. 모르는 키와 잘못된 값은 Errors 에 쌓는다.
	Request ParseRequest(std::istream& In);
}
