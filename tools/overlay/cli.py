"""데이터 오버레이의 명령줄. 보통은 tools/overlay.ps1 이 경로를 채워서 부른다.

  check          프리셋을 입히면 무엇이 바뀌는지 보여 준다. 쓰지 않는다
  apply          바닐라로 되돌린 뒤 프리셋을 입힌다
  restore        이 도구가 쓴 파일을 바닐라로 되돌린다
  status         카탈로그의 파일마다 지금의 상태
  keys           카탈로그의 파일에 든 수를 모두 적는다(경로, 값, 등급). 탭으로 나눈 표
  pin            keys.json 의 파일 패턴을 게임 폴더에서 찾아 지금의 해시로 files.json 을 만든다. 지금의 파일이
                 바닐라인지는 이 도구가 알 수 없다. Steam 무결성 검사 직후에만 돌린다(프리셋이 입혀져 있으면 거부한다)
  scan           게임 폴더의 모든 *.json 을 읽어 본다. 읽기 전용
  probe-request  프리셋이 쓰는 값을 런타임에서 찾는 요청 파일을 만든다
  verify         덤프에서 프리셋의 값을 찾아 판정한다
"""
import argparse
import json
import pathlib
import sys

import catalog
import jsonedit
import paths
import preset as presets
import store
import verify
from jsonedit import OverlayError


def _dirs(args):
    return store.Dirs(pathlib.Path(args.game_dir), pathlib.Path(args.snapshot_dir), pathlib.Path(args.state_dir))


def _show(edits):
    for edit in edits:
        mark = " 실험" if edit.group.experimental else ""
        print(f"  {edit.file}  {paths.show(edit.path)}  {edit.old_text} -> {edit.new_text}"
              f"  [{edit.group.runtime}/{edit.group.effect}{mark}]")
    unverified = sum(1 for edit in edits if edit.group.runtime != "VERIFIED")
    print(f"변경 {len(edits)}개, 파일 {len({edit.file for edit in edits})}개"
          + (f". 런타임 반영을 확인하지 않은 키 {unverified}개" if unverified else ""))


def cmd_check(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    _show(edits)
    print("check ok (쓰지 않았다)")


def cmd_apply(args):
    cat = catalog.load(args.catalog_dir)
    preset = presets.load(args.preset)
    edits, written, restored = store.apply(_dirs(args), cat, preset, args.game_version)
    _show(edits)
    for rel in restored:
        print(f"  바닐라로 되돌림: {rel}")
    print(f"apply ok: {preset.name} (쓴 파일 {len(written)}개). 게임을 다시 켜야 반영된다")


def cmd_restore(args):
    restored = store.restore(_dirs(args), catalog.load(args.catalog_dir), args.game_version)
    for rel in restored:
        print(f"  바닐라로 되돌림: {rel}")
    print(f"restore ok ({len(restored)})")


def cmd_status(args):
    cat = catalog.load(args.catalog_dir)
    dirs = _dirs(args)
    status = store.classify(dirs, cat)
    state = store.read_state(dirs)
    print(f"게임 버전 {args.game_version}, 카탈로그 {cat.game_version}, 파일 {len(status)}개")
    for name in ("vanilla", "applied", "unknown", "missing"):
        rels = sorted(rel for rel, value in status.items() if value == name)
        print(f"  {name}: {len(rels)}")
        if name != "vanilla":
            for rel in rels:
                print(f"    {rel}")
    recorded = sorted(state["files"])
    applied = sorted(rel for rel, value in status.items() if value == "applied")
    broken = any(value in ("unknown", "missing") for value in status.values())
    if applied and applied != recorded:
        # apply 가 도중에 실패했다: 상태에는 적혔는데 아직 바닐라인 파일이 있다
        print(f"프리셋: {state.get('preset')} — 부분 적용이다(상태에 적힌 {len(recorded)}개 가운데 {len(applied)}개만 입혀져 있다). "
              "다시 apply 하거나 restore 로 되돌린다")
        return 1
    if broken:
        print("프리셋: (알 수 없다. 모르는 파일이나 없는 파일이 있다)")
        return 1
    if applied:
        print(f"프리셋: {state.get('preset')}")
    else:
        print("프리셋: (없음. 바닐라)" + (f" — 상태 파일에는 {state.get('preset')} 이 남아 있다(밖에서 되돌린 것이다)" if recorded else ""))
    return 0


def cmd_keys(args):
    cat = catalog.load(args.catalog_dir)
    dirs = _dirs(args)
    status = store.classify(dirs, cat)
    lines = ["file\tpath\tvalue\truntime\teffect\texperimental"]
    for rel in sorted(cat.files):
        if args.file and not catalog.file_matches(args.file, rel):
            continue
        _, text = jsonedit.decode(store.vanilla_bytes(dirs, cat, rel, status))
        for slot in jsonedit.scan(text):
            if slot.kind != "number":
                continue
            group = cat.find(rel, slot.path)
            grades = [group.runtime, group.effect, "Y" if group.experimental else "N"] if group else ["-", "-", "-"]
            lines.append("\t".join([rel, paths.show(slot.path), text[slot.start:slot.end]] + grades))
    if args.out:
        out = pathlib.Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"keys ok: {len(lines) - 1}줄 -> {out}")
    else:
        print("\n".join(lines))


def cmd_pin(args):
    directory = pathlib.Path(args.catalog_dir)
    target = directory / "files.json"
    if target.exists():
        raise OverlayError(f"이미 있다: {target}. 다시 만들려면 게임 파일이 바닐라인 것을 확인하고 지운 뒤 실행한다")
    version, groups = catalog.parse_keys(catalog.read_json(directory / "keys.json"))
    if version != args.game_version:
        raise OverlayError(f"keys.json 은 {version} 의 것이고 게임은 {args.game_version} 이다")
    # 프리셋이 입혀진 채 핀하면 고친 파일이 바닐라로 적힌다. 이 버전이든 옛 버전이든 상태에 파일이 남아 있으면 하지 않는다.
    for state in sorted(pathlib.Path(args.state_dir).parent.glob(f"*/{store.STATE_NAME}")):
        try:
            leftover = json.loads(state.read_text(encoding="utf-8")).get("files")
        except (OSError, ValueError, AttributeError):
            leftover = True
        if leftover:
            raise OverlayError(f"프리셋이 입혀져 있다(게임 버전 {state.parent.name} 의 상태: {state}). 지금 핀하면 고친 파일이 바닐라로 적힌다. "
                               "먼저 restore 한다. 게임이 갱신돼 restore 할 수 없으면 Steam 무결성 검사로 되돌린 뒤 그 상태 파일을 지운다")
    game = pathlib.Path(args.game_dir)
    files = {}
    for group in groups:
        found = sorted(path for path in game.glob(group.file) if path.is_file())
        if not found:
            raise OverlayError(f"게임 폴더에 맞는 파일이 없다: {group.file}")
        for path in found:
            files[path.relative_to(game).as_posix()] = store.digest(path.read_bytes())
    data = {"game_version": version, "files": dict(sorted(files.items()))}
    store.write(target, (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    print(f"pin ok: {len(files)}개 -> {target}")


def _lookup(node, path):
    for part in path:
        node = node[part]
    return node


def cmd_scan(args):
    game = pathlib.Path(args.game_dir)
    files = numbers = lenient_only = 0
    failed = []
    for path in sorted(game.rglob("*.json")):
        rel = path.relative_to(game).as_posix()
        raw = path.read_bytes()
        files += 1
        try:
            bom, text = jsonedit.decode(raw)
            slots = jsonedit.scan(text)
        except jsonedit.JsonError as error:
            failed.append(f"{rel}: {error}")
            continue
        number_slots = [slot for slot in slots if slot.kind == "number"]
        numbers += len(number_slots)
        same = jsonedit.apply_edits(text, [(slot, text[slot.start:slot.end]) for slot in number_slots])
        if jsonedit.encode(bom, same) != raw:
            failed.append(f"{rel}: 아무것도 바꾸지 않았는데 바이트가 달라졌다")
            continue
        try:
            parsed = json.loads(text)
        except ValueError:
            lenient_only += 1           # 닫는 괄호 앞의 쉼표. 표준 파서와 견줄 수 없다
            continue
        for slot in slots:
            if slot.kind in ("object", "array"):
                continue
            if _lookup(parsed, slot.path) != jsonedit.value_of(text, slot):
                failed.append(f"{rel}: {paths.show(slot.path)} 의 값이 표준 파서와 다르다")
                break
    print(f"파일 {files}개, 수 {numbers}개, 표준 파서가 못 읽는 파일 {lenient_only}개, 실패 {len(failed)}개")
    for line in failed[:50]:
        print(f"  FAIL {line}")
    if failed:
        return 1
    print("scan ok")


def cmd_probe_request(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(verify.probe_request(edits), encoding="utf-8", newline="\n")
    print(f"probe-request ok: 값 {len(edits)}개 -> {out}")


def cmd_verify(args):
    cat = catalog.load(args.catalog_dir)
    _, _, edits = store.plan(_dirs(args), cat, presets.load(args.preset), args.game_version)
    try:
        with open(args.dump, encoding="utf-8") as f:
            dump = json.load(f)
        if not isinstance(dump, dict):
            raise ValueError("객체가 아니다")
    except (OSError, ValueError) as error:
        raise OverlayError(f"덤프를 읽을 수 없다: {args.dump}: {error}") from None
    doubts = verify.problems(dump)
    for doubt in doubts:
        print(f"주의: {doubt} — 이 덤프의 '없다'는 믿을 수 없다. 못 찾은 값은 unknown 으로 적는다")
    counts = {"anchored": 0, "value-only": 0, "absent": 0, "unknown": 0}
    for edit, verdict, where in verify.verdicts(edits, dump):
        if verdict == "absent" and doubts:
            verdict = "unknown"
        counts[verdict] += 1
        print(f"{verdict:10}  {edit.file}  {paths.show(edit.path)} = {edit.new_text}")
        for path in where[:5]:
            print(f"              {path}")
        if len(where) > 5:
            print(f"              … 모두 {len(where)}곳")
    print(", ".join(f"{name} {count}" for name, count in counts.items()))
    if edits and not counts["anchored"]:
        print("주의: anchored 가 하나도 없다. 이 덤프가 이 프리셋의 값을 찾은 것인지(요청 파일, 프리셋이 입혀진 채 켰는지) 확인한다")
    return 1 if doubts or counts["absent"] or counts["unknown"] or not counts["anchored"] else 0


COMMANDS = {"check": cmd_check, "apply": cmd_apply, "restore": cmd_restore, "status": cmd_status, "keys": cmd_keys,
            "pin": cmd_pin, "scan": cmd_scan, "probe-request": cmd_probe_request, "verify": cmd_verify}


def main(argv):
    parser = argparse.ArgumentParser(prog="cli.py", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("command", choices=sorted(COMMANDS))
    parser.add_argument("--game-dir", required=True)
    parser.add_argument("--game-version", required=True)
    parser.add_argument("--catalog-dir", required=True)
    parser.add_argument("--snapshot-dir", required=True)
    parser.add_argument("--state-dir", required=True)
    parser.add_argument("--preset")
    parser.add_argument("--out")
    parser.add_argument("--dump")
    parser.add_argument("--file")
    args = parser.parse_args(argv)
    needs = {"check": ["preset"], "apply": ["preset"], "probe-request": ["preset", "out"], "verify": ["preset", "dump"]}
    missing = [name for name in needs.get(args.command, []) if not getattr(args, name)]
    if missing:
        parser.error(f"{args.command} 에는 --{' --'.join(missing)} 가 필요하다")
    try:
        return COMMANDS[args.command](args) or 0
    except (OverlayError, OSError) as error:
        print(f"FAIL: {error}")
        return 1


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main(sys.argv[1:]))
