"""프리셋이 쓴 값이 런타임에 올라왔는지 모듈의 덤프로 확인한다. 카탈로그의 runtime 등급을 올리는 근거다."""
import jsonedit

FIND_SECTIONS = ("find_global", "find_ds", "find_instances")


def anchor(path):
    """경로에서 마지막 키 이름. 런타임 경로에 이 이름이 보이면 우연한 일치가 아니라고 본다."""
    return next((part.strip() for part in reversed(path) if isinstance(part, str)), None)


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


def verdicts(edits, dump):
    """edit 마다 (edit, 판정, 경로들)을 돌려준다.
    anchored: 그 값이 있고 경로에 키 이름도 있다.  value-only: 값은 있으나 이름이 없다(사람이 판단한다).  absent: 없다."""
    hits = []
    for section in FIND_SECTIONS:
        for hit in dump.get(section, {}).get("hits", []):
            value = hit.get("value")
            if hit.get("why") == "value" and isinstance(value, (int, float)) and not isinstance(value, bool):
                hits.append((str(hit.get("path")), value))
    out = []
    for edit in edits:
        found = [path for path, value in hits if value == edit.new]
        name = anchor(edit.path)
        anchored = [path for path in found if name and name in path]
        out.append((edit, "anchored" if anchored else "value-only" if found else "absent", anchored or found))
    return out
