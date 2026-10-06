"""모듈 덤프(NlToyBox.dump.*.json)를 '경로 → 값' 표로 펴서 요약하고, 찾고, 비교하고, 믿을 만한지 점검한다.

사용:
  py -3.14 dump_tool.py summary <덤프>
  py -3.14 dump_tool.py find <덤프> <낱말 또는 수>...
  py -3.14 dump_tool.py hits <덤프> [<경로에 든 낱말>...]
  py -3.14 dump_tool.py diff <덤프 A> <덤프 B>
  py -3.14 dump_tool.py controls <덤프> [<경로>=<값>...]
"""
import collections
import json
import sys

FIND_SECTIONS = ("find", "find_global", "find_ds", "find_instances")  # "find" 는 옛 형식(module_dump 1)
FIND_STATS = ("visited", "containers", "depth_cut", "matches", "hits_cut", "truncated", "enum_failed", "enum_short",
              "maps", "lists", "highest_map", "highest_list", "instances", "selfcheck", "skipped")
TOO_LONG_SHOWN = 15  # 길어서 들어가지 않은 배열·ds 를 요약에 보여 주는 수


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def flatten(dump):
    """전역, 인스턴스 변수, 스크립트 결과를 {경로: 값} 으로 편다. 값이 없는 것은 '<형>' 으로 적는다."""
    flat = {}

    def put(path, node):
        kind = node.get("kind")
        if kind == "struct":
            flat[path] = "<struct>"
            for key, child in node.get("members", {}).items():
                put(f"{path}.{key}", child)
        elif kind == "array":
            flat[path] = f"<array {node.get('length')}>"
        elif "value" in node:
            flat[path] = node["value"]
        else:
            flat[path] = f"<{kind}>"

    for name, node in dump.get("globals", {}).items():
        put(f"global.{name}", node)
    for name, node in dump.get("instances", {}).items():
        flat[f"instance:{name}"] = f"<count {node.get('count')}>"
        for key, child in node.get("members", {}).items():
            put(f"instance:{name}.{key}", child)
    for call in dump.get("scripts", []):
        key = f"script:{call['name']}({','.join(str(a) for a in call.get('args', []))})"
        if call.get("status") == "AURIE_SUCCESS":
            put(key, call.get("result", {"kind": "none"}))
        else:
            flat[key] = f"<{call.get('status')}>"
    return flat


def hits(dump):
    """모든 찾기 구역의 맞은 것을 (구역, 항목) 으로 낸다."""
    for section in FIND_SECTIONS:
        for hit in dump.get(section, {}).get("hits", []):
            yield section, hit


def describe_hit(hit):
    members = hit.get("members")
    extra = f" members={list(members)[:40]}" if members is not None else ""
    value = hit.get("value", "<" + str(hit.get("kind")) + ">")
    return f"[{hit.get('why')}] {hit.get('path')} = {value}{extra}"


def summary(dump):
    print(f"name={dump.get('name', '-')} seq={dump.get('seq', '-')} room={dump.get('room', '-')} "
          f"elapsed_seconds={dump.get('elapsed_seconds')} module={dump.get('module_version', '-')} "
          f"global_status={dump.get('global_status')}")
    if "present" in dump:
        print("present: " + " ".join(f"{name}:{count}" for name, count in dump["present"].items()))
    for path, node in dump.get("watch", {}).items():
        print(f"watch {path} = {node.get('value', '<' + str(node.get('kind')) + '>')}")
    kinds = collections.Counter(node.get("kind") for node in dump.get("globals", {}).values())
    print(f"globals={len(dump.get('globals', {}))} " + " ".join(f"{k}={n}" for k, n in kinds.most_common()))
    if "instances" in dump:
        print("instances (수/적힌 변수): " + " ".join(
            f"{name}:{node.get('count')}/{len(node.get('members', {}))}" for name, node in dump["instances"].items()))
    for call in dump.get("scripts", []):
        print(f"script {call['name']}({call.get('args')}) -> {call.get('status')} {call.get('result')}")
    for section in FIND_SECTIONS:
        if section not in dump:
            continue
        found = dump[section]
        stats = " ".join(f"{key}={found[key]}" for key in FIND_STATS if key in found)
        print(f"{section}: hits={len(found.get('hits', []))} {stats}")
        for hit in found.get("hits", []):
            print("  " + describe_hit(hit))
        too_long = found.get("too_long", [])
        if too_long:
            print(f"  길어서 들어가지 않은 것 {len(too_long)}개 (cut={found.get('too_long_cut')}):")
            for item in too_long[:TOO_LONG_SHOWN]:
                print(f"    {item.get('path')} length={item.get('length')}")


def find(dump, terms):
    flat = flatten(dump)
    for term in terms:
        try:
            number = float(term)
        except ValueError:
            number = None
        print(f"--- {term} ---")
        shown = 0
        for path, value in flat.items():
            by_value = number is not None and isinstance(value, (int, float)) and not isinstance(value, bool) and value == number
            by_name = number is None and term.lower() in path.lower()
            if by_value or by_name:
                print(f"  {path} = {value}")
                shown += 1
                if shown >= 60:
                    print("  ... (60개에서 끊음)")
                    break
        if shown == 0:
            print("  (없음)")


def show_hits(dump, terms):
    shown = 0
    for section, hit in hits(dump):
        path = str(hit.get("path"))
        if terms and not any(term.lower() in path.lower() for term in terms):
            continue
        print(f"{section} {describe_hit(hit)}")
        for key, child in (hit.get("members") or {}).items():
            print(f"    {key} = {child.get('value', '<' + str(child.get('kind')) + '>')}")
        shown += 1
    print(f"hits shown={shown}")


def diff(a, b):
    fa, fb = flatten(a), flatten(b)
    changed = [(p, fa[p], fb[p]) for p in fa if p in fb and fa[p] != fb[p]]
    print(f"changed={len(changed)} only_a={len(fa.keys() - fb.keys())} only_b={len(fb.keys() - fa.keys())}")
    for path, va, vb in changed[:200]:
        print(f"  {path}: {va} -> {vb}")
    for path in sorted(fb.keys() - fa.keys())[:50]:
        print(f"  + {path} = {fb[path]}")
    for path in sorted(fa.keys() - fb.keys())[:50]:
        print(f"  - {path} = {fa[path]}")


def controls(dump, expectations):
    """덤프의 '없다'를 믿어도 되는지 본다. (이름, 통과, 설명) 의 목록을 돌려준다.

    expectations: [(경로, 값)]. 찾기가 실제로 찾아야 하는 것(양성 대조).
    그 덤프가 게임 안에서 뜬 것인지는 여기서 판정하지 않는다. 사용자가 알려 준 때와 덤프의 시각을 맞춰 본다.
    """
    results = []

    found = set()
    for _, hit in hits(dump):
        value = hit.get("value")
        if isinstance(value, (int, float)) and not isinstance(value, bool):
            value = float(value)
        found.add((hit.get("path"), value))
    for path, value in expectations:
        results.append((f"expect {path}", (path, value) in found, f"want {value}"))

    skipped = [s for s in FIND_SECTIONS if dump.get(s, {}).get("skipped")]
    if skipped:
        results.append(("skipped", False, f"sections={skipped}"))

    ds = dump.get("find_ds", {})
    selfcheck = ds.get("selfcheck")
    if selfcheck is not None:
        results.append(("selfcheck", all(selfcheck.get(key) for key in ("types", "made", "ds_map", "ds_list")), str(selfcheck)))
    limit = dump.get("limits", {}).get("max_ds_id")
    if limit is not None and "highest_map" in ds:
        highest = max(ds.get("highest_map", -1), ds.get("highest_list", -1))
        results.append(("ds_range", highest < limit - 1, f"highest id={highest} scanned below {limit}"))

    cut = [s for s in FIND_SECTIONS if dump.get(s, {}).get("truncated")]
    results.append(("truncated", not cut, f"sections={cut}"))

    # 히트를 다 적지 못했으면 찾는 값이 적히지 않은 쪽에 있을 수 있다.
    hits_cut = [s for s in FIND_SECTIONS if dump.get(s, {}).get("hits_cut")]
    results.append(("hits_cut", not hits_cut, f"sections={hits_cut}"))

    # 구조체의 멤버를 다 보지 못했으면 그 안의 값은 본 적이 없다.
    short = {s: (dump[s].get("enum_failed", 0), dump[s].get("enum_short", 0)) for s in FIND_SECTIONS
             if dump.get(s, {}).get("enum_failed") or dump.get(s, {}).get("enum_short")}
    results.append(("enumeration", not short, f"(failed, short) by section={short}"))

    if "instances" in dump:
        with_members = [name for name, node in dump["instances"].items() if node.get("members")]
        results.append(("instances", bool(with_members), f"objects with members={len(with_members)}"))
    return results


def parse_expectation(text):
    path, _, value = text.rpartition("=")
    try:
        return path, float(value)
    except ValueError:
        return path, value


def main(argv):
    if len(argv) >= 3 and argv[1] == "summary":
        summary(load(argv[2]))
    elif len(argv) >= 4 and argv[1] == "find":
        find(load(argv[2]), argv[3:])
    elif len(argv) >= 3 and argv[1] == "hits":
        show_hits(load(argv[2]), argv[3:])
    elif len(argv) == 4 and argv[1] == "diff":
        diff(load(argv[2]), load(argv[3]))
    elif len(argv) >= 3 and argv[1] == "controls":
        results = controls(load(argv[2]), [parse_expectation(text) for text in argv[3:]])
        for name, ok, detail in results:
            print(f"{'ok  ' if ok else 'FAIL'} {name}: {detail}")
        return 0 if all(ok for _, ok, _ in results) else 1
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
