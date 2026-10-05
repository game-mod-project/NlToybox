"""exe 에서 게임 스크립트 함수의 주소를 이름으로 찾고, 그리로 가는 직접 호출(call/jmp rel32)을 센다.

사용: py -3.14 tools/re/script_calls.py <Norland.exe> <이름> [<이름> ...]
이름에 gml_Script_ 가 없으면 붙인다.

왜 세는가(research/07): 모듈의 호출 기록기는 함수의 들머리에 훅을 건다. 직접 호출이 0곳인 함수는 참조로만 불리거나
부르는 곳마다 본문이 들어가 있다. 뒤의 경우 게임의 호출은 훅에 오지 않는다(pub_sub_event_perform 이 그랬다).
메서드(gml_Script_anon_…)는 참조로 불리므로 0곳이어도 훅에 온다. 0곳이면 기록이 0번이어도 "안 불렸다"고 읽지 않는다.

찾는 법: 이름 문자열의 주소를 가리키는 8바이트 바로 뒤에 .text 안의 주소가 오는 자리(러너의 함수 표)를 찾는다.
세는 법: .text 의 모든 0xE8/0xE9 바이트를 명령의 시작으로 보고 목적지를 견준다. 명령의 시작이 아닌 바이트가
우연히 맞을 수 있다(.text 가 50MB 일 때 함수 하나에 약 0.02번꼴). 수가 1이면 주소 근처를 디스어셈블해 확인한다.
인자의 수는 이 도구가 읽지 않는다: `dumpbin /disasm /range:<va>,<끝>`으로 본문이 argc 와 argv[i] 를 보는 곳을 읽는다.
"""
import re
import struct
import sys

PREFIX = "gml_Script_"
_BRANCH = re.compile(rb"[\xE8\xE9]")


def routine_name(given):
    """정식 이름. 접두 없는 이름은 러너에서 다른 루틴을 가리킨다."""
    name = given.strip()
    return name if name.startswith(PREFIX) else PREFIX + name


def sections(data):
    """(이미지 기준 주소, [(이름, rva, 가상 크기, 파일 자리, 파일 크기)])."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("not a PE file")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0" or struct.unpack_from("<H", data, pe + 24)[0] != 0x20B:
        raise ValueError("not a 64-bit PE file")
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
    table = pe + 24 + optional_size
    found = []
    for i in range(count):
        at = table + i * 40
        name = data[at:at + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", data, at + 8)
        found.append((name, rva, virtual_size, raw, raw_size))
    return base, found


def _va_of(offset, base, secs):
    for _, rva, _, raw, raw_size in secs:
        if raw <= offset < raw + raw_size:
            return base + rva + (offset - raw)
    return None


def find_function(data, base, secs, name, text_range):
    """이름이 가리키는 함수의 주소. 없으면 None."""
    needle = name.encode("ascii") + b"\0"
    at = data.find(needle)
    while at >= 0:
        string_va = _va_of(at, base, secs) if at > 0 and data[at - 1] == 0 else None
        if string_va is not None:
            pointer = struct.pack("<Q", string_va)
            ref = data.find(pointer)
            while ref >= 0:
                if ref + 16 <= len(data):
                    function = struct.unpack_from("<Q", data, ref + 8)[0]
                    if text_range[0] <= function < text_range[1]:
                        return function
                ref = data.find(pointer, ref + 1)
        at = data.find(needle, at + 1)
    return None


def survey(data, names):
    """이름마다 {"name", "va", "calls", "jumps"}. 못 찾은 이름은 va 가 None."""
    base, secs = sections(data)
    text = next((s for s in secs if s[0] == ".text"), None)
    if text is None:
        raise ValueError("no .text section")
    _, rva, _, raw, raw_size = text
    text_va = base + rva
    code = data[raw:raw + raw_size]

    rows = []
    for given in names:
        name = routine_name(given)
        rows.append({"name": name, "va": find_function(data, base, secs, name, (text_va, text_va + len(code))), "calls": 0, "jumps": 0})

    wanted = {}
    for row in rows:
        if row["va"] is not None:
            wanted.setdefault(row["va"], []).append(row)
    if wanted:
        last = len(code) - 5
        for match in _BRANCH.finditer(code):
            at = match.start()
            if at > last:
                break
            dest = text_va + at + 5 + struct.unpack_from("<i", code, at + 1)[0]
            for row in wanted.get(dest, ()):
                row["calls" if code[at] == 0xE8 else "jumps"] += 1
    return rows


def report(rows):
    lines = []
    for row in rows:
        if row["va"] is None:
            lines.append(f"{row['name']}: not found")
            continue
        note = "" if row["calls"] or row["jumps"] else "  (no direct calls: reached only through a reference, or inlined into its callers)"
        lines.append(f"{row['name']}: va {row['va']:#x}, call {row['calls']}, jmp {row['jumps']}{note}")
    return "\n".join(lines)


def main(argv):
    if len(argv) < 3:
        print(__doc__.strip().splitlines()[2], file=sys.stderr)
        return 2
    with open(argv[1], "rb") as handle:
        data = handle.read()
    print(report(survey(data, argv[2:])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
