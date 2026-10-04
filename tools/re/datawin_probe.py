"""data.win(GameMaker IFF)을 읽기 전용으로 훑어 청크 표, GEN8 정보, STRG 문자열을 뽑는다.

사용: py -3.14 datawin_probe.py <data.win> <문자열 출력 파일>
"""
import datetime
import os
import struct
import sys

src, out_strings = sys.argv[1], sys.argv[2]

with open(src, "rb") as f:
    data = f.read()

assert data[:4] == b"FORM", data[:4]
total = struct.unpack_from("<I", data, 4)[0]
print(f"FORM size={total:,} file={len(data):,}")

chunks = {}
pos = 8
while pos < 8 + total:
    name = data[pos:pos + 4].decode("ascii", "replace")
    size = struct.unpack_from("<I", data, pos + 4)[0]
    chunks[name] = (pos + 8, size)
    print(f"  {name}  off=0x{pos + 8:08X}  size={size:>12,}")
    pos += 8 + size


def cstr(off):
    if off == 0:
        return ""
    end = data.index(b"\0", off)
    return data[off:end].decode("utf-8", "replace")


if "GEN8" in chunks:
    o, _ = chunks["GEN8"]
    debugger_disabled, bytecode = data[o], data[o + 1]
    fn_off, cfg_off = struct.unpack_from("<II", data, o + 4)
    name_off = struct.unpack_from("<I", data, o + 40)[0]
    major, minor, release, build = struct.unpack_from("<IIII", data, o + 44)
    ts = struct.unpack_from("<Q", data, o + 92)[0]
    disp_off = struct.unpack_from("<I", data, o + 100)[0]
    print("--- GEN8 ---")
    print(f"  debuggerDisabled={debugger_disabled} bytecodeVersion={bytecode}")
    print(f"  filename={cstr(fn_off)!r} config={cstr(cfg_off)!r} name={cstr(name_off)!r} display={cstr(disp_off)!r}")
    print(f"  version={major}.{minor}.{release}.{build}")
    print(f"  timestamp={datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).isoformat()}")

# CODE/VARI/FUNC 가 없으면 YYC 다.
for key in ("CODE", "VARI", "FUNC", "SCPT", "OBJT", "SPRT", "TXTR", "AUDO", "EXTN"):
    if key in chunks:
        o, size = chunks[key]
        count = struct.unpack_from("<I", data, o)[0] if size >= 4 else None
        print(f"  {key}: size={size:,} count={count}")
    else:
        print(f"  {key}: (청크 없음)")

if "STRG" in chunks:
    o, size = chunks["STRG"]
    n = struct.unpack_from("<I", data, o)[0]
    offs = struct.unpack_from(f"<{n}I", data, o + 4)
    os.makedirs(os.path.dirname(os.path.abspath(out_strings)), exist_ok=True)
    with open(out_strings, "w", encoding="utf-8", newline="\n") as w:
        for so in offs:
            ln = struct.unpack_from("<I", data, so)[0]
            s = data[so + 4:so + 4 + ln].decode("utf-8", "replace")
            w.write(s.replace("\r", "\\r").replace("\n", "\\n") + "\n")
    print(f"--- STRG --- count={n:,} -> {out_strings}")
