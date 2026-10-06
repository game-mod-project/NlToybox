"""compose.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import compose
import preset
from compose import ComposeError

DEBUG = support.FILES["debug.json"]


def run(*changes, allow_experimental=False, cat=None):
    p = preset.parse({"name": "T", "game_version": support.VERSION, "allow_experimental": allow_experimental,
                      "changes": list(changes)})
    return compose.compose(p, cat or support.make_catalog(), dict(support.FILES))


def change(path, file="debug.json", **op):
    return {"file": file, "path": path, **op}


class ComposeTests(unittest.TestCase):
    def test_set_changes_only_the_text_of_that_value(self):
        texts, edits = run(change("budget_money", set=5000))
        self.assertEqual(texts, {"debug.json": DEBUG.replace('"budget_money": 2000', '"budget_money": 5000')})
        self.assertEqual([(e.file, e.path, e.old, e.new, e.old_text, e.new_text) for e in edits],
                         [("debug.json", ("budget_money",), 2000, 5000, "2000", "5000")])
        self.assertEqual(edits[0].group.runtime, "VERIFIED")

    def test_mul_multiplies_the_vanilla_value_and_rounds_as_asked(self):
        texts, _ = run(change("building_resources.*[*][1]", mul=0.5, round="ceil"), allow_experimental=True)
        self.assertEqual(texts["debug.json"], DEBUG.replace('["wood", 10], ["iron", 5]', '["wood", 5], ["iron", 3]')
                         .replace('["wood", 15]', '["wood", 8]'))

    def test_rounding_modes(self):
        self.assertEqual(compose.multiply(15, 0.5, "ceil"), 8)
        self.assertEqual(compose.multiply(15, 0.5, "floor"), 7)
        self.assertEqual(compose.multiply(15, 0.5, "nearest"), 8)      # 반은 올린다
        self.assertEqual(compose.multiply(5, 0.5, "nearest"), 3)
        self.assertEqual(compose.multiply(15, 0.5, ""), 7.5)

    def test_mul_trims_float_noise_before_rounding(self):
        self.assertEqual(compose.multiply(0.1, 3, ""), 0.3)             # 0.30000000000000004 가 아니다
        self.assertEqual(compose.multiply(0.1, 30, "ceil"), 3)          # 3.0000000000000004 를 4 로 올리지 않는다

    def test_multiplying_by_one_gives_back_the_same_number(self):
        self.assertEqual(compose.multiply(0.20000000298023224, 1, ""), 0.20000000298023224)   # 유효숫자가 12자리를 넘는 값
        self.assertEqual(compose.multiply(1234567890123, 1, ""), 1234567890123)
        long_value = DEBUG.replace('"factor": 0.5', '"factor": 0.20000000298023224')
        p = preset.parse({"name": "T", "game_version": support.VERSION, "changes": [change("factor", mul=1)]})
        self.assertEqual(compose.compose(p, support.make_catalog(), {"debug.json": long_value}), ({}, []))

    def test_mul_keeps_integers_exact_and_floats_to_fifteen_digits(self):
        self.assertEqual(compose.multiply(1234567890123, 2, ""), 2469135780246)
        self.assertEqual(compose.multiply(0.20000000298023224, 2, ""), 0.400000005960464)
        self.assertEqual(compose.multiply(0.7, 3, ""), 2.1)                                    # 2.0999999999999996 이 아니다

    def test_a_long_float_is_rewritten_only_when_its_value_changes(self):
        texts, edits = run(change("production_cost.coal", set=0.9))     # 파일에는 0.90000000000000002 로 적혀 있다
        self.assertEqual((texts, edits), ({}, []))
        texts, _ = run(change("production_cost.coal", mul=2))
        self.assertIn('"coal": 1.8,', texts["debug.json"])

    def test_the_decimal_point_of_the_old_text_is_kept(self):
        texts, _ = run(change("slave.cost", set=60), allow_experimental=True)
        self.assertIn('"slave": {"cost": 60.0}', texts["debug.json"])

    def test_a_later_change_wins_and_both_start_from_vanilla(self):
        texts, edits = run(change("production_cost.*", mul=10), change("production_cost.ale", mul=3))
        self.assertIn('"ale": 6,', texts["debug.json"])                 # 2×3 이다. 2×10×3 이 아니다
        self.assertIn('"coal": 9.0,', texts["debug.json"])             # 소수점으로 적혀 있던 자리다
        self.assertEqual(len(edits), 2)

    def test_a_later_change_back_to_vanilla_cancels_the_earlier_one(self):
        self.assertEqual(run(change("budget_money", set=5000), change("budget_money", set=2000)), ({}, []))

    def test_a_preset_with_no_changes_changes_nothing(self):
        self.assertEqual(run(), ({}, []))

    def test_a_file_pattern_reaches_every_file_that_has_the_path(self):
        texts, edits = run(change("upgrade_skill[*].value", file="books/*.json", mul=2))
        self.assertEqual(texts, {"books/a.json": '{"upgrade_skill":[{"value":32,"name":"combat"}],"tag":0}'})
        self.assertEqual(edits[0].file, "books/a.json")

    def test_targets_lists_the_files_a_preset_touches(self):
        p = preset.parse({"name": "T", "game_version": support.VERSION, "changes": [
            change("upgrade_skill[*].value", file="books/*.json", mul=2), change("budget_money", set=1)]})
        self.assertEqual(compose.targets(p, support.make_catalog()), ["books/a.json", "books/b.json", "debug.json"])

    def test_what_cannot_be_applied_is_an_error(self):
        cases = {
            "아무 값에도 닿지 않는다": [change("budget_mony", set=1)],
            "카탈로그에 없는 파일": [change("budget_money", file="other.json", set=1)],
            "카탈로그에 없는 키": [change("tag", file="books/a.json", set=1)],
            "수가 아닌 값": [change("name", set=1)],
            "수가 아닌 값 ": [change("production_cost", set=1)],
            "실험 키": [change("building_resources.hut[0][1]", set=1)],
            "실험 키 ": [change("slave.cost", set=1)],
            "허용 범위": [change("budget_money", set=-1)],
            "허용 범위 ": [change("budget_money", mul=1000)],
        }
        for message, changes in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(ComposeError) as caught:
                    run(*changes)
                self.assertIn(message.strip(), str(caught.exception))
                self.assertIn("변경 #1", str(caught.exception))

    def test_an_unreadable_game_file_is_reported_with_its_name(self):
        cat = support.make_catalog()
        p = preset.parse({"name": "T", "game_version": support.VERSION, "changes": [change("budget_money", set=1)]})
        with self.assertRaises(ComposeError) as caught:
            compose.compose(p, cat, {"debug.json": '{"budget_money": 2000 // 주석\n}'})
        self.assertIn("debug.json", str(caught.exception))


if __name__ == "__main__":
    unittest.main()
