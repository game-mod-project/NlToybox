"""jsonedit.py 의 시험. 사용: py -3.14 -m unittest discover -s tools/overlay/tests -v"""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import jsonedit
from jsonedit import JsonError

# 게임 파일의 모양을 줄인 것: 탭 들여쓰기, CRLF, 닫는 괄호 앞의 쉼표, 뒤 공백이 붙은 키, 긴 소수.
SAMPLE = ('{\r\n\t"production_cost": {\r\n\t\t"ale": 2,\r\n\t\t"coal": 0.90000000000000002,\r\n\t},\r\n'
          '\t"building_resources": {\r\n\t\t"hut": [["wood", 10], ["iron", 5.0]],\r\n\t},\r\n'
          '\t"paper": {"messenger_cost ": 1},\r\n\t"on": true, "off": false, "none": null, "neg": -6\r\n}')


def find(text, *path):
    return next(slot for slot in jsonedit.scan(text) if slot.path == path)


class ScanTests(unittest.TestCase):
    def test_scan_lists_every_value_in_document_order_with_its_path(self):
        slots = jsonedit.scan('{"a": {"b": [1, "x"]}, "c": null}')
        self.assertEqual([(slot.path, slot.kind) for slot in slots],
                         [((), "object"), (("a",), "object"), (("a", "b"), "array"), (("a", "b", 0), "number"),
                          (("a", "b", 1), "string"), (("c",), "null")])

    def test_slot_positions_cover_exactly_the_value_text(self):
        text = '{"a": -12.5 , "b": "q\\"x"}'
        self.assertEqual(text[find(text, "a").start:find(text, "a").end], "-12.5")
        self.assertEqual(text[find(text, "b").start:find(text, "b").end], '"q\\"x"')

    def test_scan_reads_the_shapes_found_in_game_files(self):
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "production_cost", "coal")), 0.9)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "building_resources", "hut", 1, 1)), 5.0)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "paper", "messenger_cost ")), 1)
        self.assertEqual(jsonedit.value_of(SAMPLE, find(SAMPLE, "neg")), -6)

    def test_trailing_comma_is_accepted_in_objects_and_arrays(self):
        self.assertEqual(len(jsonedit.scan('{"a": [1, 2, ], }')), 4)

    def test_what_game_files_never_contain_is_an_error_not_a_guess(self):
        for bad in ('{"a": 1} x', '{"a": 1, "a": 2}', "{'a': 1}", "{a: 1}", '{"a": NaN}', '{"a": 1 // c\n}',
                    '{"a": 01}', '{"a": }', '{"a": [1 2]}', '{"a": "x', '{"a": 1', '{,}', '{"a": 1,,}', ""):
            with self.subTest(bad=bad):
                with self.assertRaises(JsonError):
                    jsonedit.scan(bad)

    def test_errors_say_where(self):
        with self.assertRaises(JsonError) as caught:
            jsonedit.scan('{\n  "a": 1,\n  "a": 2\n}')
        self.assertIn("3행 3열", str(caught.exception))


class ValueTests(unittest.TestCase):
    def test_value_of_keeps_integers_integers(self):
        text = '{"i": 7, "f": 7.0, "e": 1e2, "s": "a\\nb", "t": true}'
        self.assertIs(type(jsonedit.value_of(text, find(text, "i"))), int)
        self.assertIs(type(jsonedit.value_of(text, find(text, "f"))), float)
        self.assertEqual(jsonedit.value_of(text, find(text, "e")), 100.0)
        self.assertEqual(jsonedit.value_of(text, find(text, "s")), "a\nb")
        self.assertIs(jsonedit.value_of(text, find(text, "t")), True)

    def test_format_number_writes_integers_and_shortest_floats(self):
        self.assertEqual(jsonedit.format_number(5000, "2000"), "5000")
        self.assertEqual(jsonedit.format_number(5000.0, "2000"), "5000")
        self.assertEqual(jsonedit.format_number(0.1 + 0.2, "1"), "0.30000000000000004")
        self.assertEqual(jsonedit.format_number(-2.5, "1"), "-2.5")

    def test_format_number_keeps_the_decimal_point_of_the_old_text(self):
        self.assertEqual(jsonedit.format_number(3, "2.0"), "3.0")
        self.assertEqual(jsonedit.format_number(3, "0.5"), "3.0")
        self.assertEqual(jsonedit.format_number(3.5, "2.0"), "3.5")

    def test_format_number_never_writes_an_exponent(self):
        self.assertEqual(jsonedit.format_number(0.00001, "1"), "0.00001")
        self.assertEqual(jsonedit.format_number(1.5e-7, "1"), "0.00000015")
        self.assertEqual(jsonedit.format_number(1e22, "1"), "10000000000000000000000")

    def test_format_number_refuses_what_is_not_a_finite_number(self):
        for bad in (float("inf"), float("nan"), True, "5", None):
            with self.subTest(bad=bad):
                with self.assertRaises(JsonError):
                    jsonedit.format_number(bad, "1")


class EditTests(unittest.TestCase):
    def test_apply_edits_changes_only_the_value_text(self):
        out = jsonedit.apply_edits(SAMPLE, [(find(SAMPLE, "production_cost", "ale"), "7"),
                                            (find(SAMPLE, "building_resources", "hut", 0, 1), "5")])
        self.assertEqual(out, SAMPLE.replace('"ale": 2,', '"ale": 7,').replace('["wood", 10]', '["wood", 5]'))

    def test_writing_the_same_text_back_leaves_every_byte_alone(self):
        numbers = [slot for slot in jsonedit.scan(SAMPLE) if slot.kind == "number"]
        self.assertEqual(jsonedit.apply_edits(SAMPLE, [(slot, SAMPLE[slot.start:slot.end]) for slot in numbers]), SAMPLE)
        self.assertEqual(jsonedit.apply_edits(SAMPLE, []), SAMPLE)

    def test_overlapping_edits_are_refused(self):
        text = '{"a": [1, 2]}'
        with self.assertRaises(JsonError):
            jsonedit.apply_edits(text, [(find(text, "a"), "[]"), (find(text, "a", 0), "9")])

    def test_decode_keeps_the_bom_and_the_line_endings(self):
        raw = jsonedit.BOM + SAMPLE.encode("utf-8")
        bom, text = jsonedit.decode(raw)
        self.assertEqual((bom, text), (jsonedit.BOM, SAMPLE))
        self.assertEqual(jsonedit.encode(bom, text), raw)
        self.assertEqual(jsonedit.decode(SAMPLE.encode("utf-8"))[0], b"")

    def test_decode_refuses_bytes_that_are_not_utf8(self):
        with self.assertRaises(JsonError):
            jsonedit.decode(b'{"a": "\xff"}')


if __name__ == "__main__":
    unittest.main()
