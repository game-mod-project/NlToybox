"""프리셋: 바닐라 위에 입힐 변경의 목록.

  { "name": "Easy", "game_version": "0.5588.9777.0", "allow_experimental": false,
    "changes": [
      { "file": "debug_params.json", "path": "budget_money", "set": 5000 },
      { "file": "debug_params.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil" } ] }

- 연산은 set(수를 지정)과 mul(바닐라 값에 곱함) 둘이다. 수만 바꾼다.
- 한 값에 변경이 둘 닿으면 뒤의 것이 이긴다. 둘 다 바닐라 값에서 계산한다(쌓이지 않는다).
"""
import dataclasses

import catalog
import paths
from jsonedit import OverlayError

ROUNDINGS = ("ceil", "floor", "nearest")
_FIELDS = {"name", "game_version", "allow_experimental", "changes", "note"}
_CHANGE_FIELDS = {"file", "path", "set", "mul", "round", "note"}      # note 는 사람이 읽는 설명이다. 도구는 쓰지 않는다


class PresetError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Change:
    file: str           # 게임 폴더 기준 상대경로의 패턴
    path: tuple         # paths.parse 한 식
    path_text: str
    op: str             # set 또는 mul
    value: object       # 수
    rounding: str       # mul 일 때만: ceil, floor, nearest, 또는 빈 글


@dataclasses.dataclass(frozen=True)
class Preset:
    name: str
    game_version: str
    allow_experimental: bool
    changes: tuple


def parse(data):
    if not isinstance(data, dict):
        raise PresetError("프리셋은 객체여야 한다")
    unknown = sorted(set(data) - _FIELDS)
    if unknown:
        raise PresetError(f"프리셋에 모르는 항목이 있다: {unknown}")
    for field in ("name", "game_version"):
        if not isinstance(data.get(field), str) or not data[field]:
            raise PresetError(f"프리셋의 {field} 는 비어 있지 않은 글이어야 한다")
    if not isinstance(data.get("allow_experimental", False), bool):
        raise PresetError("프리셋의 allow_experimental 은 불리언이어야 한다")
    if not isinstance(data.get("note", ""), str):
        raise PresetError("프리셋의 note 는 글이어야 한다")
    if not isinstance(data.get("changes"), list):
        raise PresetError("프리셋의 changes 는 배열이어야 한다")
    changes = []
    for n, raw in enumerate(data["changes"], 1):
        where = f"변경 #{n}"
        if not isinstance(raw, dict):
            raise PresetError(f"{where}: 객체여야 한다")
        unknown = sorted(set(raw) - _CHANGE_FIELDS)
        if unknown:
            raise PresetError(f"{where}: 모르는 항목 {unknown}")
        if not isinstance(raw.get("note", ""), str):
            raise PresetError(f"{where}: note 는 글이어야 한다")
        ops = [op for op in ("set", "mul") if op in raw]
        if len(ops) != 1:
            raise PresetError(f"{where}: set 과 mul 가운데 하나만 있어야 한다")
        rounding = raw.get("round", "")
        if rounding and (ops[0] != "mul" or rounding not in ROUNDINGS):
            raise PresetError(f"{where}: round 는 mul 과 함께만 쓰고 {ROUNDINGS} 가운데 하나여야 한다")
        try:
            file = catalog.check_rel(raw.get("file"))
            value = catalog.number(raw[ops[0]], ops[0])
            path = paths.parse(raw.get("path"))
        except OverlayError as error:
            raise PresetError(f"{where}: {error}") from None
        changes.append(Change(file, path, raw["path"], ops[0], value, rounding))
    return Preset(data["name"], data["game_version"], data.get("allow_experimental", False), tuple(changes))


def load(path):
    try:
        return parse(catalog.read_json(path))
    except catalog.CatalogError as error:
        raise PresetError(str(error)) from None
