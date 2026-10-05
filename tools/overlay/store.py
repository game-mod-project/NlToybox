"""게임 폴더에 프리셋을 입히고 되돌린다. 게임의 데이터 파일을 쓰는 곳은 이 모듈뿐이다.

- 바닐라인지는 카탈로그의 SHA256 으로 판정한다. 스냅샷(backups/data/<버전>/)은 그 바이트의 사본이다.
- 적용은 항상 바닐라에서 출발한다: 앞서 입힌 것을 모두 되돌린 뒤 새 프리셋을 입힌다.
- 상태(backups/overlay/<버전>/state.json)는 새 내용을 쓰기 전에 먼저 적는다. 도중에 죽어도 게임 파일은
  '바닐라' 아니면 '상태에 적힌 해시' 가운데 하나다.
"""
import dataclasses
import datetime
import hashlib
import json
import os
import pathlib

import compose
import jsonedit
from jsonedit import OverlayError

TMP_SUFFIX = ".nltoybox-tmp"
STATE_NAME = "state.json"


class StoreError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Dirs:
    game: pathlib.Path          # 게임 폴더
    snapshot: pathlib.Path      # backups/data/<게임 버전>
    state: pathlib.Path         # backups/overlay/<게임 버전>


def digest(data):
    return hashlib.sha256(data).hexdigest().upper()


def at(root, rel):
    return pathlib.Path(root).joinpath(*rel.split("/"))


def write(path, data, mtime_ns=None):
    """임시 파일에 쓰고 바꿔치기한다. 쓰다가 실패해도 대상 파일은 온전하다(예전 것이거나 새것이다).
    mtime_ns 를 주면 수정 시각을 그것으로 맞춘다(스냅샷과 복원이 원본의 시각을 옮길 때 쓴다)."""
    tmp = path.with_name(path.name + TMP_SUFFIX)
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_bytes(data)
        if mtime_ns is not None:
            os.utime(tmp, ns=(mtime_ns, mtime_ns))
        os.replace(tmp, path)
    finally:
        if tmp.exists():
            tmp.unlink()
    if path.read_bytes() != data:
        raise StoreError(f"쓴 뒤에 읽은 내용이 다르다: {path}")


def read_state(dirs):
    path = dirs.state / STATE_NAME
    if not path.exists():
        return {"preset": None, "files": {}}
    try:
        with open(path, encoding="utf-8") as f:
            state = json.load(f)
    except (OSError, ValueError) as error:
        raise StoreError(f"상태 파일을 읽을 수 없다: {path}: {error}") from None
    if not isinstance(state, dict) or not isinstance(state.get("files"), dict):
        raise StoreError(f"상태 파일의 꼴이 다르다: {path}")
    return state


def classify(dirs, cat):
    """카탈로그의 파일마다 지금의 상태: vanilla, applied(이 도구가 마지막에 쓴 것), unknown, missing."""
    applied = read_state(dirs)["files"]
    status = {}
    for rel, vanilla in cat.files.items():
        path = at(dirs.game, rel)
        if not path.is_file():
            status[rel] = "missing"
            continue
        sha = digest(path.read_bytes())
        status[rel] = "vanilla" if sha == vanilla else "applied" if sha == applied.get(rel) else "unknown"
    return status


def _check(dirs, cat, game_version, preset=None):
    if cat.game_version != game_version:
        raise StoreError(f"카탈로그는 {cat.game_version} 의 것이고 게임은 {game_version} 이다. 이 버전의 카탈로그가 필요하다")
    if preset is not None and preset.game_version != game_version:
        raise StoreError(f"프리셋 {preset.name!r} 은 {preset.game_version} 의 것이고 게임은 {game_version} 이다")
    status = classify(dirs, cat)
    bad = sorted(rel for rel, state in status.items() if state in ("unknown", "missing"))
    if bad:
        raise StoreError("게임 파일이 바닐라도 아니고 이 도구가 마지막에 쓴 것도 아니다(없거나 밖에서 바뀌었다): "
                         + ", ".join(bad) + ". 게임이 갱신됐으면 새 버전의 카탈로그가 필요하다. "
                         "직접 고친 파일이면 tools\\data-restore.ps1 이나 Steam 무결성 검사로 되돌린다")
    return status


def vanilla_bytes(dirs, cat, rel, status):
    """그 파일의 바닐라 바이트. 스냅샷이 있으면 스냅샷, 없으면 바닐라인 게임 파일에서 읽는다. 쓰지 않는다."""
    snap = at(dirs.snapshot, rel)
    if snap.is_file():
        data = snap.read_bytes()
        if digest(data) != cat.files[rel]:
            raise StoreError(f"스냅샷이 카탈로그의 바닐라와 다르다: {snap}. 지우고 Steam 무결성 검사 뒤에 다시 만든다")
        return data
    if status[rel] != "vanilla":
        raise StoreError(f"스냅샷이 없는데 게임 파일이 바닐라가 아니다: {rel}. Steam 무결성 검사로 되돌린다")
    data = at(dirs.game, rel).read_bytes()
    if digest(data) != cat.files[rel]:      # 상태를 본 뒤에 파일이 바뀌었다. 바닐라가 아닌 것을 바닐라로 쓰지 않는다
        raise StoreError(f"상태를 본 뒤에 게임 파일이 바뀌었다: {rel}. 다시 실행한다")
    return data


def ensure_snapshot(dirs, cat, rel, status):
    """스냅샷이 없으면 바닐라인 게임 파일에서 만든다. 원본의 수정 시각도 옮긴다."""
    snap = at(dirs.snapshot, rel)
    if not snap.is_file():
        write(snap, vanilla_bytes(dirs, cat, rel, status), at(dirs.game, rel).stat().st_mtime_ns)


def plan(dirs, cat, preset, game_version):
    """프리셋을 입히면 무엇이 바뀌는지 계산한다. 쓰지 않는다. (상태, {상대경로: 새 바이트}, [Edit])"""
    status = _check(dirs, cat, game_version, preset)
    decoded = {}
    for rel in compose.targets(preset, cat):
        try:
            decoded[rel] = jsonedit.decode(vanilla_bytes(dirs, cat, rel, status))
        except jsonedit.JsonError as error:
            raise StoreError(f"{rel}: {error}") from None
    texts, edits = compose.compose(preset, cat, {rel: text for rel, (_, text) in decoded.items()})
    return status, {rel: jsonedit.encode(decoded[rel][0], text) for rel, text in texts.items()}, edits


def _originals(dirs, cat, status):
    """입혀진 파일마다 (바닐라 바이트, 원본의 수정 시각). 스냅샷이 없으면 여기서 멈춘다."""
    return {rel: (vanilla_bytes(dirs, cat, rel, status), at(dirs.snapshot, rel).stat().st_mtime_ns)
            for rel, state in sorted(status.items()) if state == "applied"}


_HALFWAY = ("쓰는 도중에 실패했다: {error}. 게임 파일은 하나하나가 바닐라이거나 방금 쓴 것이다(반쯤 쓰인 파일은 없다). "
            "원인을 없앤 뒤 다시 apply 하거나 restore 로 되돌린다. 지금 상태는 status 로 본다")


def apply(dirs, cat, preset, game_version):
    """프리셋을 입힌다. ([Edit], 쓴 파일들, 바닐라로 되돌린 파일들)"""
    status, data, edits = plan(dirs, cat, preset, game_version)     # 계산이 끝나기 전에는 아무것도 쓰지 않는다
    originals = _originals(dirs, cat, status)
    try:
        for rel in sorted(data):
            ensure_snapshot(dirs, cat, rel, status)
        for rel, (original, mtime_ns) in originals.items():
            write(at(dirs.game, rel), original, mtime_ns)
        state = {"preset": preset.name, "game_version": game_version,
                 "applied_at": datetime.datetime.now().isoformat(timespec="seconds"),
                 "files": {rel: digest(new) for rel, new in sorted(data.items())}}
        write(dirs.state / STATE_NAME, json.dumps(state, ensure_ascii=False, indent=2).encode("utf-8"))
        for rel, new in sorted(data.items()):
            write(at(dirs.game, rel), new)
    except OSError as error:
        raise StoreError(_HALFWAY.format(error=error)) from None
    return edits, sorted(data), sorted(rel for rel in originals if rel not in data)


def restore(dirs, cat, game_version):
    """이 도구가 쓴 파일을 바닐라로 되돌리고(수정 시각도) 상태를 지운다. 되돌린 파일들을 돌려준다."""
    status = _check(dirs, cat, game_version)
    originals = _originals(dirs, cat, status)
    try:
        for rel, (original, mtime_ns) in originals.items():
            write(at(dirs.game, rel), original, mtime_ns)
        state = dirs.state / STATE_NAME
        if state.exists():
            state.unlink()
    except OSError as error:
        raise StoreError(_HALFWAY.format(error=error)) from None
    return sorted(originals)
