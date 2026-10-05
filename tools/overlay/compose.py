"""프리셋과 카탈로그와 바닐라 글로 새 글을 계산한다. 파일을 읽거나 쓰지 않는다."""
import dataclasses
import math

import jsonedit
import paths
from jsonedit import OverlayError


class ComposeError(OverlayError):
    pass


@dataclasses.dataclass(frozen=True)
class Edit:
    file: str
    path: tuple
    old: object         # 바닐라 값 (int 또는 float)
    new: object
    old_text: str       # 바닐라 파일에 적힌 글
    new_text: str
    group: object       # catalog.Group
    slot: object        # jsonedit.Slot (바닐라 글에서의 자리)


def targets(preset, cat):
    """프리셋이 닿는 파일의 상대경로들."""
    out = set()
    for n, change in enumerate(preset.changes, 1):
        files = cat.files_matching(change.file)
        if not files:
            raise ComposeError(f"변경 #{n}: 카탈로그에 없는 파일이다: {change.file}")
        out.update(files)
    return sorted(out)


def multiply(old, factor, rounding):
    """곱한 값. 유효숫자 12자리로 다듬은 뒤(0.1×3 = 0.30000000000000004 를 0.3 으로) 올림·내림한다."""
    value = float(format(old * factor, ".12g"))
    if rounding == "ceil":
        return math.ceil(value)
    if rounding == "floor":
        return math.floor(value)
    if rounding == "nearest":
        return math.floor(value + 0.5)
    return value


def compose(preset, cat, vanilla):
    """vanilla: {상대경로: 바닐라 글}. targets() 가 말한 파일이 모두 들어 있어야 한다.
    ({상대경로: 새 글}, [Edit])를 돌려준다. 새 글에는 바뀐 파일만 있다. 바닐라와 같은 값은 건드리지 않는다."""
    slots = {}
    chosen = {}         # (파일, 자리의 시작) → Edit. 뒤의 변경이 앞의 것을 덮는다
    for n, change in enumerate(preset.changes, 1):
        where = f"변경 #{n} ({change.file} {change.path_text})"
        files = cat.files_matching(change.file)
        if not files:
            raise ComposeError(f"변경 #{n}: 카탈로그에 없는 파일이다: {change.file}")
        reached = 0
        for rel in files:
            text = vanilla[rel]
            if rel not in slots:
                try:
                    slots[rel] = jsonedit.scan(text)
                except jsonedit.JsonError as error:
                    raise ComposeError(f"{rel}: {error}") from None
            for slot in slots[rel]:
                if not paths.matches(change.path, slot.path):
                    continue
                reached += 1
                shown = f"{rel} {paths.show(slot.path)}"
                if slot.kind != "number":
                    raise ComposeError(f"{where}: 수가 아닌 값에 닿는다: {shown} ({slot.kind})")
                group = cat.find(rel, slot.path)
                if group is None:
                    raise ComposeError(f"{where}: 카탈로그에 없는 키다: {shown}")
                if group.experimental and not preset.allow_experimental:
                    raise ComposeError(f"{where}: 실험 키다(runtime {group.runtime}): {shown}. "
                                       "바꾸려면 프리셋에 \"allow_experimental\": true 를 적는다")
                old = jsonedit.value_of(text, slot)
                new = change.value if change.op == "set" else multiply(old, change.value, change.rounding)
                if (group.min is not None and new < group.min) or (group.max is not None and new > group.max):
                    raise ComposeError(f"{where}: 허용 범위 [{group.min}, {group.max}] 를 벗어난다: {shown} = {new}")
                key = (rel, slot.start)
                if new == old:
                    chosen.pop(key, None)
                    continue
                old_text = text[slot.start:slot.end]
                chosen[key] = Edit(rel, slot.path, old, new, old_text, jsonedit.format_number(new, old_text), group, slot)
        if reached == 0:
            raise ComposeError(f"{where}: 아무 값에도 닿지 않는다")
    edits = [chosen[key] for key in sorted(chosen)]
    texts = {}
    for rel in sorted({edit.file for edit in edits}):
        texts[rel] = jsonedit.apply_edits(vanilla[rel], [(edit.slot, edit.new_text) for edit in edits if edit.file == rel])
    return texts, edits
