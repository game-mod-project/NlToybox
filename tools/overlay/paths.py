"""경로 식: 프리셋과 카탈로그가 JSON 안의 값을 가리키는 방법.

  budget_money                      키
  fair_trade.fair_trade_sale.ale    점으로 잇는다
  production_cost.*                 * 는 객체의 모든 키
  building_resources.*[*][1]        [n] 은 배열의 첨자, [*] 는 모든 첨자
  paper["messenger_cost "]          점·공백·괄호·별표·따옴표가 든 키는 JSON 문자열로 적는다

점 뒤의 수는 키다(unit_skill_stage.1 의 "1"). 배열의 첨자는 대괄호로만 적는다.
"""
import json
import re

from jsonedit import OverlayError

KEY, ANY_KEY, INDEX, ANY_INDEX = "key", "any_key", "index", "any_index"
_BARE = re.compile(r'[^.\[\]*"\s]+')
_DIGITS = re.compile(r"[0-9]+")
_DECODER = json.JSONDecoder()


class PathError(OverlayError):
    pass


def parse(expr):
    """경로 식을 (종류, 값)의 튜플로 바꾼다."""
    if not isinstance(expr, str) or not expr:
        raise PathError(f"경로는 비어 있지 않은 글이어야 한다: {expr!r}")
    out = []
    i = 0
    while i < len(expr):
        if expr[i] == "[":
            i = _bracket(expr, i, out)
            continue
        if out:
            if expr[i] != ".":
                raise PathError(f"{i + 1}번째 글자에 점이나 대괄호가 와야 한다: {expr!r}")
            i += 1
        if expr.startswith("*", i):
            out.append((ANY_KEY, None))
            i += 1
            continue
        match = _BARE.match(expr, i)
        if not match:
            raise PathError(f"{i + 1}번째 글자에서 키를 읽을 수 없다: {expr!r}")
        out.append((KEY, match.group(0)))
        i = match.end()
    return tuple(out)


def _bracket(expr, i, out):
    j = i + 1
    if expr.startswith("*", j):
        out.append((ANY_INDEX, None))
        j += 1
    elif expr.startswith('"', j):
        try:
            key, j = _DECODER.raw_decode(expr, j)
        except ValueError:
            raise PathError(f"대괄호 안의 문자열을 읽을 수 없다: {expr!r}") from None
        out.append((KEY, key))
    else:
        match = _DIGITS.match(expr, j)
        if not match:
            raise PathError(f"대괄호 안에는 수, *, 문자열이 와야 한다: {expr!r}")
        out.append((INDEX, int(match.group(0))))
        j = match.end()
    if not expr.startswith("]", j):
        raise PathError(f"대괄호가 닫히지 않았다: {expr!r}")
    return j + 1


def matches(pattern, path):
    """parse 한 식이 구체 경로(키는 str, 첨자는 int 인 튜플)에 맞는가."""
    if len(pattern) != len(path):
        return False
    for (kind, value), part in zip(pattern, path):
        if kind in (KEY, ANY_KEY):
            if not isinstance(part, str) or (kind == KEY and part != value):
                return False
        elif not isinstance(part, int) or (kind == INDEX and part != value):
            return False
    return True


def show(path):
    """구체 경로를 식으로 적는다. parse(show(p)) 는 p 에만 맞는다."""
    out = []
    for part in path:
        if isinstance(part, int):
            out.append(f"[{part}]")
        elif _BARE.fullmatch(part):
            out.append(("." if out else "") + part)
        else:
            out.append("[" + json.dumps(part, ensure_ascii=False) + "]")
    return "".join(out)
