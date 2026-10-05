"""카탈로그: 이 도구가 바꿀 수 있는 키와, 키마다 무엇을 실측했는가.

  catalog/<게임 버전>/keys.json    손으로 쓴다. 키의 묶음과 등급
  catalog/<게임 버전>/files.json   `cli.py pin` 이 만든다. 파일마다 바닐라의 SHA256

runtime — 이 레포가 이 빌드에서 잰 것:
  VERIFIED  파일의 값을 바꿔서 런타임에서 그 값을 봤다
  SEEN      바닐라 값이 같은 이름으로 런타임에 있는 것을 봤다
  UNSEEN    런타임에서 본 적이 없다 (찾지 않았거나, 찾았는데 없었다)
effect — 값이 게임을 바꾸는가:
  OFFICIAL  개발진이나 공식 문서가 말했다
  TESTED    게임에서 바꿔 보고 달라진 것을 봤다 (effect_by 에 누가 봤는지 적는다)
  INFERRED  이름이나 구조로 미루어 본 것
  UNKNOWN   모른다
"""
import dataclasses
import json
import math
import pathlib
import re

import paths
from jsonedit import OverlayError

RUNTIME = ("VERIFIED", "SEEN", "UNSEEN")
EFFECT = ("OFFICIAL", "TESTED", "INFERRED", "UNKNOWN")
_GROUP_FIELDS = {"file", "paths", "runtime", "effect", "effect_by", "experimental", "min", "max", "note"}
_SHA256 = re.compile(r"[0-9A-F]{64}")


class CatalogError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Group:
    file: str               # 게임 폴더 기준 상대경로의 패턴
    paths: tuple            # paths.parse 한 식들
    runtime: str
    effect: str
    effect_by: str
    experimental: bool      # 적어 두었거나 runtime 이 UNSEEN 이면 참
    min: object             # 수 또는 None
    max: object
    note: str


@dataclasses.dataclass(frozen=True)
class Catalog:
    game_version: str
    files: dict             # 상대경로('/' 로 나눈다) → 바닐라의 SHA256(대문자)
    groups: tuple

    def files_matching(self, pattern):
        return sorted(rel for rel in self.files if file_matches(pattern, rel))

    def find(self, file, path):
        """그 파일의 그 값에 맞는 첫 묶음. 없으면 None."""
        for group in self.groups:
            if file_matches(group.file, file) and any(paths.matches(pattern, path) for pattern in group.paths):
                return group
        return None


def read_json(path):
    """이 도구의 파일(카탈로그, 프리셋)을 읽는다. 엄격한 JSON 이고, 한 객체에 같은 키가 두 번 나오면 오류다."""
    def pairs(items):
        keys = [key for key, _ in items]
        twice = sorted({key for key in keys if keys.count(key) > 1})
        if twice:
            raise CatalogError(f"같은 키가 두 번 나온다: {twice} ({path})")
        return dict(items)

    try:
        with open(path, encoding="utf-8-sig") as f:
            return json.load(f, object_pairs_hook=pairs)
    except FileNotFoundError:
        raise CatalogError(f"파일이 없다: {path}") from None
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CatalogError(f"읽을 수 없다: {path}: {error}") from None


def check_rel(rel):
    """게임 폴더 안의 상대경로(또는 그 패턴)인지 본다. 조각은 '/' 로 나눈다."""
    if not isinstance(rel, str) or not rel or "\\" in rel or ":" in rel or rel.startswith("/"):
        raise CatalogError(f"게임 폴더 안의 상대경로여야 한다('/' 로 나눈다): {rel!r}")
    if any(part in ("", ".", "..") for part in rel.split("/")):
        raise CatalogError(f"게임 폴더 안의 상대경로여야 한다: {rel!r}")
    return rel


def file_matches(pattern, rel):
    """패턴의 '*' 는 한 조각 안의 아무 글자다. 대소문자를 가린다."""
    regex = "/".join("[^/]*".join(re.escape(piece) for piece in part.split("*")) for part in pattern.split("/"))
    return re.fullmatch(regex, rel) is not None


def number(value, what):
    """유한한 수인지 본다. 불리언은 수로 치지 않는다."""
    if isinstance(value, bool) or not isinstance(value, (int, float)) or (isinstance(value, float) and not math.isfinite(value)):
        raise CatalogError(f"{what} 는 유한한 수여야 한다: {value!r}")
    return value


def parse_keys(data):
    """keys.json 의 내용을 (게임 버전, 묶음들)로 바꾼다."""
    if not isinstance(data, dict) or set(data) != {"game_version", "groups"}:
        raise CatalogError("keys.json 은 game_version 과 groups 만 가진 객체여야 한다")
    if not isinstance(data["game_version"], str) or not isinstance(data["groups"], list):
        raise CatalogError("keys.json: game_version 은 글, groups 는 배열이어야 한다")
    groups = []
    for n, raw in enumerate(data["groups"], 1):
        where = f"keys.json 의 묶음 #{n}"
        if not isinstance(raw, dict):
            raise CatalogError(f"{where}: 객체여야 한다")
        unknown = sorted(set(raw) - _GROUP_FIELDS)
        if unknown:
            raise CatalogError(f"{where}: 모르는 항목 {unknown}")
        for field in ("file", "paths", "runtime", "effect"):
            if field not in raw:
                raise CatalogError(f"{where}: {field} 가 없다")
        if not isinstance(raw["paths"], list) or not raw["paths"]:
            raise CatalogError(f"{where}: paths 는 비어 있지 않은 배열이어야 한다")
        if raw["runtime"] not in RUNTIME:
            raise CatalogError(f"{where}: runtime 은 {RUNTIME} 가운데 하나여야 한다: {raw['runtime']!r}")
        if raw["effect"] not in EFFECT:
            raise CatalogError(f"{where}: effect 는 {EFFECT} 가운데 하나여야 한다: {raw['effect']!r}")
        effect_by = raw.get("effect_by", "")
        if not isinstance(effect_by, str) or (raw["effect"] == "TESTED" and not effect_by):
            raise CatalogError(f"{where}: effect 가 TESTED 면 effect_by 에 누가 봤는지 적는다")
        if not isinstance(raw.get("experimental", False), bool) or not isinstance(raw.get("note", ""), str):
            raise CatalogError(f"{where}: experimental 은 불리언, note 는 글이어야 한다")
        low = None if raw.get("min") is None else number(raw["min"], f"{where}: min")
        high = None if raw.get("max") is None else number(raw["max"], f"{where}: max")
        if low is not None and high is not None and low > high:
            raise CatalogError(f"{where}: min 이 max 보다 크다")
        try:
            parsed = tuple(paths.parse(expr) for expr in raw["paths"])
        except paths.PathError as error:
            raise CatalogError(f"{where}: {error}") from None
        groups.append(Group(check_rel(raw["file"]), parsed, raw["runtime"], raw["effect"], effect_by,
                            raw.get("experimental", False) or raw["runtime"] == "UNSEEN", low, high, raw.get("note", "")))
    return data["game_version"], tuple(groups)


def parse_files(data):
    """files.json 의 내용을 (게임 버전, {상대경로: SHA256})로 바꾼다."""
    if not isinstance(data, dict) or set(data) != {"game_version", "files"} or not isinstance(data["files"], dict):
        raise CatalogError("files.json 은 game_version 과 files(객체)만 가진 객체여야 한다")
    for rel, sha in data["files"].items():
        check_rel(rel)
        if "*" in rel or not isinstance(sha, str) or not _SHA256.fullmatch(sha):
            raise CatalogError(f"files.json: {rel!r} 의 값은 대문자 16진 SHA256 이어야 하고, 이름에 * 를 쓸 수 없다")
    return data["game_version"], dict(data["files"])


def build(keys_data, files_data):
    version, groups = parse_keys(keys_data)
    files_version, files = parse_files(files_data)
    if version != files_version:
        raise CatalogError(f"keys.json({version})과 files.json({files_version})의 게임 버전이 다르다")
    for n, group in enumerate(groups, 1):
        if not any(file_matches(group.file, rel) for rel in files):
            raise CatalogError(f"keys.json 의 묶음 #{n}: files.json 에 {group.file!r} 에 맞는 파일이 없다 (cli.py pin 으로 다시 만든다)")
    return Catalog(version, files, groups)


def load(directory):
    directory = pathlib.Path(directory)
    return build(read_json(directory / "keys.json"), read_json(directory / "files.json"))
