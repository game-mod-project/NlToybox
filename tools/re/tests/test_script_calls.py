"""script_calls.py 의 시험. 사용: py -3.14 -m unittest discover -s tools/re/tests -v

게임의 exe 를 읽지 않는다. 구역 둘(.text, .rdata)짜리 PE 를 메모리에 지어 쓴다.
"""
import importlib.util
import pathlib
import struct
import unittest

_spec = importlib.util.spec_from_file_location("script_calls", pathlib.Path(__file__).resolve().parents[1] / "script_calls.py")
script_calls = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(script_calls)

BASE = 0x140000000
TEXT_RVA, TEXT_RAW, TEXT_SIZE = 0x1000, 0x400, 0x400
RDATA_RVA, RDATA_RAW, RDATA_SIZE = 0x2000, 0x800, 0x400


def _section(name, rva, raw, size):
    return name.ljust(8, b"\0") + struct.pack("<IIII", size, rva, size, raw) + b"\0" * 16


def build_exe(functions, calls):
    """functions: {이름: .text 안의 자리}. calls: [(명령 바이트 0xE8|0xE9, .text 안의 자리, 가는 함수 이름)]."""
    text = bytearray(b"\xCC" * TEXT_SIZE)
    for opcode, at, name in calls:
        dest = TEXT_RVA + functions[name]
        text[at] = opcode
        struct.pack_into("<i", text, at + 1, dest - (TEXT_RVA + at + 5))

    rdata = bytearray(RDATA_SIZE)
    cursor, table = 0x10, 0x200
    for name, at in functions.items():
        raw = name.encode("ascii") + b"\0"
        rdata[cursor:cursor + len(raw)] = raw
        struct.pack_into("<QQ", rdata, table, BASE + RDATA_RVA + cursor, BASE + TEXT_RVA + at)
        cursor += len(raw) + 3
        table += 24      # 이름, 함수, 그 뒤의 칸 하나

    header = bytearray(TEXT_RAW)
    header[0:2] = b"MZ"
    struct.pack_into("<I", header, 0x3C, 0x80)
    header[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", header, 0x84, 0x8664, 2)        # 기계, 구역 수
    struct.pack_into("<H", header, 0x84 + 16, 240)          # 선택 헤더의 크기
    struct.pack_into("<H", header, 0x98, 0x20B)             # PE32+
    struct.pack_into("<Q", header, 0x98 + 24, BASE)
    table_at = 0x98 + 240
    header[table_at:table_at + 40] = _section(b".text", TEXT_RVA, TEXT_RAW, TEXT_SIZE)
    header[table_at + 40:table_at + 80] = _section(b".rdata", RDATA_RVA, RDATA_RAW, RDATA_SIZE)
    return bytes(header) + bytes(text) + bytes(rdata)


class ScriptCallsTests(unittest.TestCase):
    def setUp(self):
        self.functions = {"gml_Script_budget_money_change": 0x100, "gml_Script_time_hour": 0x180, "xgml_Script_tail": 0x1C0}
        self.exe = build_exe(self.functions, [
            (0xE8, 0x10, "gml_Script_budget_money_change"),
            (0xE8, 0x20, "gml_Script_budget_money_change"),
            (0xE8, 0x30, "gml_Script_budget_money_change"),
            (0xE9, 0x40, "gml_Script_budget_money_change"),
        ])

    def test_counts_direct_calls_and_jumps_to_a_named_script(self):
        rows = script_calls.survey(self.exe, ["gml_Script_budget_money_change"])
        self.assertEqual(rows, [{"name": "gml_Script_budget_money_change", "va": BASE + TEXT_RVA + 0x100, "calls": 3, "jumps": 1}])

    def test_a_script_nobody_calls_directly_reports_zero(self):
        # 직접 호출이 0곳이면 훅을 걸어도 기록에 남지 않는다(research/07). 0 을 "없다"와 가릴 수 있어야 한다.
        rows = script_calls.survey(self.exe, ["gml_Script_time_hour"])
        self.assertEqual((rows[0]["va"], rows[0]["calls"], rows[0]["jumps"]), (BASE + TEXT_RVA + 0x180, 0, 0))

    def test_a_name_without_the_prefix_gets_it(self):
        rows = script_calls.survey(self.exe, ["budget_money_change"])
        self.assertEqual((rows[0]["name"], rows[0]["calls"]), ("gml_Script_budget_money_change", 3))

    def test_an_unknown_name_has_no_address(self):
        rows = script_calls.survey(self.exe, ["gml_Script_nowhere"])
        self.assertEqual(rows, [{"name": "gml_Script_nowhere", "va": None, "calls": 0, "jumps": 0}])

    def test_the_tail_of_a_longer_name_is_not_the_name(self):
        # "xgml_Script_tail" 안의 "gml_Script_tail" 은 그 이름이 아니다.
        self.assertIsNone(script_calls.survey(self.exe, ["gml_Script_tail"])[0]["va"])

    def test_report_says_when_a_script_cannot_be_recorded(self):
        text = script_calls.report(script_calls.survey(self.exe, ["gml_Script_budget_money_change", "gml_Script_time_hour", "gml_Script_nowhere"]))
        lines = text.splitlines()
        self.assertIn("call 3, jmp 1", lines[0])
        self.assertIn("call 0, jmp 0", lines[1])
        self.assertIn("no direct calls", lines[1])
        self.assertIn("not found", lines[2])

    def test_a_file_that_is_not_an_exe_is_an_error(self):
        with self.assertRaises(ValueError):
            script_calls.survey(b"not an exe at all", ["gml_Script_x"])


if __name__ == "__main__":
    unittest.main()
