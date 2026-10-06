"""게임의 JSON 에서 값의 자리를 찾고 그 자리의 글자만 바꾼다. 파싱해서 다시 쓰지 않는다.

받는 문법은 게임 폴더의 JSON 4,701개에서 실제로 본 것이다(research/03-data-files.md): 표준 JSON 과,
닫는 괄호 앞의 쉼표. 주석, 작은따옴표, 따옴표 없는 키, NaN, 한 객체 안의 같은 키는 한 번도 없었다.
그런 것을 만나면 게임이 어떻게 읽는지 모르므로 오류를 낸다.
"""
import dataclasses
import decimal
import json
import math
import re

BOM = b"\xef\xbb\xbf"


class OverlayError(Exception):
    """이 도구가 사용자에게 그대로 보여 주는 오류."""


class JsonError(OverlayError):
    """읽을 수 없는 JSON, 또는 쓸 수 없는 값."""


@dataclasses.dataclass(frozen=True)
class Slot:
    path: tuple     # 뿌리에서 이 값까지. 객체의 키는 str, 배열의 첨자는 int
    start: int      # 값이 시작하는 글자 위치
    end: int        # 값이 끝난 다음 글자 위치
    kind: str       # number, string, true, false, null, object, array


_WS = " \t\r\n"
_NUMBER = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?")
_LITERALS = ("true", "false", "null")


def decode(raw):
    """바이트를 (BOM, 글)로 나눈다. UTF-8 로 읽히지 않으면 오류다(글자가 조용히 바뀌는 것을 막는다)."""
    bom = BOM if raw.startswith(BOM) else b""
    try:
        return bom, raw[len(bom):].decode("utf-8")
    except UnicodeDecodeError as error:
        raise JsonError(f"UTF-8 로 읽을 수 없다: {error}") from None


def encode(bom, text):
    return bom + text.encode("utf-8")


def _fail(text, pos, what):
    line = text.count("\n", 0, pos) + 1
    column = pos - (text.rfind("\n", 0, pos) + 1) + 1
    raise JsonError(f"{what} ({line}행 {column}열)")


def _skip(text, pos):
    while pos < len(text) and text[pos] in _WS:
        pos += 1
    return pos


def _string_end(text, pos):
    """pos 는 여는 따옴표의 위치. 닫는 따옴표의 다음 위치를 돌려준다."""
    i = pos + 1
    while i < len(text):
        if text[i] == "\\":
            i += 2
        elif text[i] == '"':
            return i + 1
        else:
            i += 1
    _fail(text, pos, "문자열이 닫히지 않았다")


def _value(text, pos, path, slots):
    if pos >= len(text):
        _fail(text, pos, "값이 와야 할 자리에서 글이 끝났다")
    c = text[pos]
    if c == "{":
        return _container(text, pos, path, slots, "object", "}")
    if c == "[":
        return _container(text, pos, path, slots, "array", "]")
    if c == '"':
        end = _string_end(text, pos)
        slots.append(Slot(path, pos, end, "string"))
        return end
    match = _NUMBER.match(text, pos)
    if match:
        slots.append(Slot(path, pos, match.end(), "number"))
        return match.end()
    for word in _LITERALS:
        if text.startswith(word, pos):
            slots.append(Slot(path, pos, pos + len(word), word))
            return pos + len(word)
    _fail(text, pos, "읽을 수 없는 값")


def _container(text, pos, path, slots, kind, close):
    index = len(slots)
    slots.append(None)          # 컨테이너의 자리는 끝을 안 뒤에 채운다. 순서는 안의 값보다 앞이다
    seen = set()
    count = 0
    i = _skip(text, pos + 1)
    while True:
        if i >= len(text):
            _fail(text, pos, "괄호가 닫히지 않았다")
        if text[i] == close:
            break
        if kind == "object":
            if text[i] != '"':
                _fail(text, i, "키는 큰따옴표로 시작해야 한다")
            key_end = _string_end(text, i)
            try:
                key = json.loads(text[i:key_end], strict=False)
            except ValueError:
                _fail(text, i, "키를 읽을 수 없다")
            if key in seen:
                _fail(text, i, f"한 객체 안에 같은 키가 두 번 있다: {key!r}")
            seen.add(key)
            i = _skip(text, key_end)
            if i >= len(text) or text[i] != ":":
                _fail(text, i, "키 뒤에 쌍점이 없다")
            i = _value(text, _skip(text, i + 1), path + (key,), slots)
        else:
            i = _value(text, i, path + (count,), slots)
        count += 1
        i = _skip(text, i)
        if i < len(text) and text[i] == ",":
            i = _skip(text, i + 1)          # 닫는 괄호 앞의 쉼표도 받는다(게임 파일 셋에 있다)
        elif i < len(text) and text[i] != close:
            _fail(text, i, f"쉼표나 {close} 가 와야 한다")
    slots[index] = Slot(path, pos, i + 1, kind)
    return i + 1


def scan(text):
    """글 안의 모든 값의 자리를 문서 순서로 돌려준다. 컨테이너가 그 안의 값보다 먼저 나온다."""
    slots = []
    pos = _skip(text, _value(text, _skip(text, 0), (), slots))
    if pos != len(text):
        _fail(text, pos, "값이 끝난 뒤에 글이 더 있다")
    return slots


def value_of(text, slot):
    """스칼라 자리의 값. 수는 소수점이나 지수가 없으면 int, 있으면 float 다."""
    token = text[slot.start:slot.end]
    if slot.kind == "number":
        return float(token) if any(c in token for c in ".eE") else int(token)
    if slot.kind == "string":
        return json.loads(token, strict=False)
    if slot.kind in _LITERALS:
        return {"true": True, "false": False, "null": None}[slot.kind]
    raise JsonError(f"스칼라가 아니다: {slot.kind}")


def format_number(value, like):
    """새 수를 글로 쓴다. 정수는 정수로, 실수는 가장 짧은 왕복 표기로. 지수 표기는 쓰지 않는다
    (게임 파일의 수 254,251개에 지수 표기가 없다. 게임이 읽는지 모른다).
    like 는 그 자리에 있던 글이다. 거기 소수점이 있으면 정수에도 '.0' 을 붙인다."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise JsonError(f"수가 아니다: {value!r}")
    if isinstance(value, float):
        if not math.isfinite(value):
            raise JsonError(f"유한하지 않은 수는 쓰지 않는다: {value!r}")
        if not value.is_integer():
            text = repr(value)
            return format(decimal.Decimal(text), "f") if "e" in text else text
        value = int(value)
    return f"{value}.0" if "." in like else str(value)


def apply_edits(text, edits):
    """edits: (Slot, 새 글)의 목록. 그 자리의 글자만 바뀐 글을 돌려준다. 자리가 겹치면 오류다."""
    ordered = sorted(edits, key=lambda edit: edit[0].start)
    for (a, _), (b, _) in zip(ordered, ordered[1:]):
        if b.start < a.end:
            raise JsonError(f"바꿀 자리가 겹친다: {a.path} 와 {b.path}")
    parts, cursor = [], 0
    for slot, new in ordered:
        parts += [text[cursor:slot.start], new]
        cursor = slot.end
    parts.append(text[cursor:])
    return "".join(parts)
