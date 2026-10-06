"""실행 파일에서 출력 가능한 ASCII 문자열(길이 5 이상)을 뽑아 중복 없이 저장한다. 읽기 전용.

사용: py -3.14 exe_strings.py <exe> <출력 파일>
"""
import os
import re
import sys

src, out = sys.argv[1], sys.argv[2]
with open(src, "rb") as f:
    data = f.read()

seen = set()
os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
with open(out, "w", encoding="utf-8", newline="\n") as w:
    for m in re.finditer(rb"[\x20-\x7e]{5,400}", data):
        s = m.group().decode("ascii")
        if s not in seen:
            seen.add(s)
            w.write(s + "\n")
print(f"{src}: {len(data):,} bytes, unique strings={len(seen):,} -> {out}")
