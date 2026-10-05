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


def write(path, data):
    """임시 파일에 쓰고 바꿔치기한다. 도중에 죽어도 반쯤 쓰인 파일이 남지 않는다."""
    tmp = path.with_name(path.name + TMP_SUFFIX)
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_bytes(data)
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


def vanilla_bytes(dirs, cat, rel, status, create):
    """그 파일의 바닐라 바이트. 스냅샷이 없으면 바닐라인 게임 파일을 쓰고, create 면 스냅샷을 만든다."""
    snap = at(dirs.snapshot, rel)
    if snap.is_file():
        data = snap.read_bytes()
        if digest(data) != cat.files[rel]:
            raise StoreError(f"스냅샷이 카탈로그의 바닐라와 다르다: {snap}. 지우고 Steam 무결성 검사 뒤에 다시 만든다")
        return data
    if status[rel] != "vanilla":
        raise StoreError(f"스냅샷이 없는데 게임 파일이 바닐라가 아니다: {rel}. Steam 무결성 검사로 되돌린다")
    data = at(dirs.game, rel).read_bytes()
    if create:
        write(snap, data)
    return data


def plan(dirs, cat, preset, game_version, create_snapshots=False):
    """프리셋을 입히면 무엇이 바뀌는지 계산한다. (상태, {상대경로: 새 바이트}, [Edit])"""
    status = _check(dirs, cat, game_version, preset)
    decoded = {}
    for rel in compose.targets(preset, cat):
        try:
            decoded[rel] = jsonedit.decode(vanilla_bytes(dirs, cat, rel, status, create_snapshots))
        except jsonedit.JsonError as error:
            raise StoreError(f"{rel}: {error}") from None
    texts, edits = compose.compose(preset, cat, {rel: text for rel, (_, text) in decoded.items()})
    return status, {rel: jsonedit.encode(decoded[rel][0], text) for rel, text in texts.items()}, edits


def _originals(dirs, cat, status):
    return {rel: vanilla_bytes(dirs, cat, rel, status, False) for rel, state in sorted(status.items()) if state == "applied"}


def apply(dirs, cat, preset, game_version):
    """프리셋을 입힌다. ([Edit], 쓴 파일들, 바닐라로 되돌린 파일들)"""
    status, data, edits = plan(dirs, cat, preset, game_version, create_snapshots=True)
    originals = _originals(dirs, cat, status)       # 쓰기 전에 모두 읽어 둔다. 스냅샷이 없으면 여기서 멈춘다
    for rel, original in originals.items():
        write(at(dirs.game, rel), original)
    state = {"preset": preset.name, "game_version": game_version,
             "applied_at": datetime.datetime.now().isoformat(timespec="seconds"),
             "files": {rel: digest(new) for rel, new in sorted(data.items())}}
    write(dirs.state / STATE_NAME, json.dumps(state, ensure_ascii=False, indent=2).encode("utf-8"))
    for rel, new in sorted(data.items()):
        write(at(dirs.game, rel), new)
    return edits, sorted(data), sorted(rel for rel in originals if rel not in data)


def restore(dirs, cat, game_version):
    """이 도구가 쓴 파일을 바닐라로 되돌리고 상태를 지운다. 되돌린 파일들을 돌려준다."""
    status = _check(dirs, cat, game_version)
    originals = _originals(dirs, cat, status)
    for rel, original in originals.items():
        write(at(dirs.game, rel), original)
    state = dirs.state / STATE_NAME
    if state.exists():
        state.unlink()
    return sorted(originals)
