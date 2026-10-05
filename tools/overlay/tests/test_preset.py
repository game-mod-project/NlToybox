"""preset.py 의 시험."""
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import paths
import preset
from preset import PresetError


def data(*changes, **extra):
    return {"name": "T", "game_version": support.VERSION, "changes": list(changes), **extra}


class PresetTests(unittest.TestCase):
    def test_parse_reads_set_and_mul(self):
        p = preset.parse(data({"file": "debug.json", "path": "budget_money", "set": 5000},
                              {"file": "debug.json", "path": "building_resources.*[*][1]", "mul": 0.5, "round": "ceil"}))
        self.assertEqual((p.name, p.game_version, p.allow_experimental), ("T", support.VERSION, False))
        self.assertEqual((p.changes[0].op, p.changes[0].value, p.changes[0].rounding), ("set", 5000, ""))
        self.assertEqual((p.changes[1].op, p.changes[1].value, p.changes[1].rounding), ("mul", 0.5, "ceil"))
        self.assertEqual(p.changes[1].path, paths.parse("building_resources.*[*][1]"))
        self.assertEqual(p.changes[1].path_text, "building_resources.*[*][1]")

    def test_a_preset_may_have_no_changes(self):
        self.assertEqual(preset.parse(data()).changes, ())

    def test_allow_experimental_is_read(self):
        self.assertTrue(preset.parse(data(allow_experimental=True)).allow_experimental)

    def test_a_mistyped_preset_is_an_error(self):
        ok = {"file": "debug.json", "path": "budget_money", "set": 1}
        cases = {
            "모르는 항목": data(dict(ok, sett=2)),
            "하나만": data({"file": "debug.json", "path": "budget_money"}),
            "하나만 ": data(dict(ok, mul=2)),
            "round": data(dict(ok, round="ceil")),
            "round ": data({"file": "debug.json", "path": "budget_money", "mul": 2, "round": "up"}),
            "유한한 수": data(dict(ok, set="5")),
            "유한한 수 ": data(dict(ok, set=True)),
            "경로": data(dict(ok, path="")),
            "상대경로": data(dict(ok, file="../debug.json")),
            "name": {"game_version": "1", "changes": []},
            "changes": {"name": "T", "game_version": "1"},
            "allow_experimental": data(allow_experimental="yes"),
            "모르는 항목 ": data(note="x"),
        }
        for message, bad in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(PresetError) as caught:
                    preset.parse(bad)
                self.assertIn(message.strip(), str(caught.exception))

    def test_errors_name_the_change(self):
        with self.assertRaises(PresetError) as caught:
            preset.parse(data({"file": "debug.json", "path": "budget_money", "set": 1}, {"file": "debug.json", "path": "a..b", "set": 1}))
        self.assertIn("변경 #2", str(caught.exception))

    def test_load_reads_a_file_and_reports_bad_json_as_a_preset_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / "p.json"
            path.write_text(json.dumps(data({"file": "debug.json", "path": "budget_money", "set": 1})), encoding="utf-8")
            self.assertEqual(preset.load(path).changes[0].value, 1)
            path.write_text('{"name": "T", }', encoding="utf-8")
            with self.assertRaises(PresetError):
                preset.load(path)
            with self.assertRaises(PresetError):
                preset.load(pathlib.Path(tmp) / "none.json")


if __name__ == "__main__":
    unittest.main()
