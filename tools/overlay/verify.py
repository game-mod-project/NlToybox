"""프리셋이 쓴 값이 런타임에 올라왔는지 모듈의 덤프로 확인한다. 카탈로그의 runtime 등급을 올리는 근거다."""
import re

import jsonedit

FIND_SECTIONS = ("find_global", "find_ds", "find_instances")
_NAME = re.compile(r"[^.\[\]#:]+")      # 런타임 경로를 이름 조각으로 나눈다: ds_map[150].x, instance:o_data#0.__y


def anchor(path):
    """경로에서 마지막 키 이름(양끝 공백을 뗀다). 배열 안의 값은 가장 가까운 키다."""
    return next((part.strip() for part in reversed(path) if isinstance(part, str)), None)


def anchors(path):
    """이 값이 런타임에서 가질 수 있다고 본 이름들(실측한 꼴만. research/01, 02): 마지막 키 그대로
    (ds_map 의 키, 앞에 밑줄이 붙은 구조체 멤버 __population), 그리고 키 경로를 _ 로 이은 이름
    (global.__gameplay_vars.global_map_ai_economy_initial_budget). 앞의 밑줄은 떼고 견준다."""
    keys = [part.strip() for part in path if isinstance(part, str)]
    return {keys[-1].lstrip("_"), "_".join(keys).lstrip("_")} if keys else set()


def probe_request(edits, delay_seconds=60):
    """edits 의 새 값을 값으로 찾는 요청 파일의 글(tools/probe.ps1 -Request 로 넘긴다).
    배열 안의 값은 런타임 경로에 이름이 남지 않으므로 가장 가까운 키 이름도 찾는다."""
    values, names = [], []
    for edit in edits:
        value = jsonedit.format_number(edit.new, "")
        if value not in values:
            values.append(value)
        if isinstance(edit.path[-1], int) and anchor(edit.path) and anchor(edit.path) not in names:
            names.append(anchor(edit.path))
    lines = ["# tools/overlay/cli.py probe-request 가 만들었다. 프리셋이 쓴 값을 메인 메뉴의 런타임에서 찾는다.",
             f"delay_seconds={delay_seconds}", "max_hits=5000"]
    return "\n".join(lines + [f"find={value}" for value in values] + [f"find_name={name}" for name in names]) + "\n"


def problems(dump):
    """이 덤프의 '없다'를 믿을 수 없게 만드는 것들. 비어 있어야 absent 를 말할 수 있다.
    (덤프의 자가 점검과 ds 범위는 tools/re/dump_tool.py controls 가 본다. 둘 다 본다.)"""
    found = []
    for section in FIND_SECTIONS:
        part = dump.get(section)
        if not isinstance(part, dict) or not isinstance(part.get("hits"), list):
            found.append(f"{section}: 구역이 없다")
            continue
        found += [f"{section}: {flag}" for flag in ("skipped", "truncated", "hits_cut") if part.get(flag)]
    return found


def verdicts(edits, dump):
    """edit 마다 (edit, 판정, 경로들)을 돌려준다.
    anchored: 그 값이 있고, 경로의 이름 조각 하나가 키 이름과 같다(더 긴 이름의 일부인 것은 치지 않는다).
    value-only: 값은 있으나 그런 이름이 없다(사람이 판단한다).  absent: 그 값이 없다."""
    hits = []
    for section in FIND_SECTIONS:
        for hit in (dump.get(section) or {}).get("hits") or []:
            value = hit.get("value")
            if hit.get("why") == "value" and isinstance(value, (int, float)) and not isinstance(value, bool):
                hits.append((str(hit.get("path")), value))
    out = []
    for edit in edits:
        found = [path for path, value in hits if value == edit.new]
        wanted = anchors(edit.path)
        anchored = [path for path in found if wanted & {name.lstrip("_") for name in _NAME.findall(path)}]
        out.append((edit, "anchored" if anchored else "value-only" if found else "absent", anchored or found))
    return out
