"""모듈 덤프(NlToyBox.dump.json)를 '경로 → 값' 표로 펴서 요약하고, 찾고, 비교한다.

사용:
  py -3.14 dump_tool.py summary <덤프>
  py -3.14 dump_tool.py find <덤프> <낱말 또는 수>...
  py -3.14 dump_tool.py diff <덤프 A> <덤프 B>
"""
import collections
import json
import sys


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def flatten(dump):
    """전역과 스크립트 결과를 {경로: 값} 으로 편다. 값이 없는 것은 '<형>' 으로 적는다."""
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
    for call in dump.get("scripts", []):
        key = f"script:{call['name']}({','.join(str(a) for a in call.get('args', []))})"
        if call.get("status") == "AURIE_SUCCESS":
            put(key, call.get("result", {"kind": "none"}))
        else:
            flat[key] = f"<{call.get('status')}>"
    return flat


def summary(dump):
    kinds = collections.Counter(node.get("kind") for node in dump.get("globals", {}).values())
    print(f"elapsed_seconds={dump.get('elapsed_seconds')} global_status={dump.get('global_status')}")
    print(f"globals={len(dump.get('globals', {}))} " + " ".join(f"{k}={n}" for k, n in kinds.most_common()))
    for call in dump.get("scripts", []):
        print(f"script {call['name']}({call.get('args')}) -> {call.get('status')} {call.get('result')}")
    found = dump.get("find", {})
    print(f"find: visited={found.get('visited')} truncated={found.get('truncated')} hits={len(found.get('hits', []))}")
    for hit in found.get("hits", []):
        members = hit.get("members")
        extra = f" members={list(members)[:40]}" if members is not None else ""
        print(f"  [{hit.get('why')}] {hit.get('path')} = {hit.get('value', '<' + str(hit.get('kind')) + '>')}{extra}")


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


def main(argv):
    if len(argv) >= 3 and argv[1] == "summary":
        summary(load(argv[2]))
    elif len(argv) >= 4 and argv[1] == "find":
        find(load(argv[2]), argv[3:])
    elif len(argv) == 4 and argv[1] == "diff":
        diff(load(argv[2]), load(argv[3]))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
