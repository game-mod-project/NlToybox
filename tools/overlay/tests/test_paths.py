"""paths.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import paths
from paths import ANY_INDEX, ANY_KEY, INDEX, KEY, PathError


class PathTests(unittest.TestCase):
    def test_parse_reads_keys_wildcards_and_indexes(self):
        self.assertEqual(paths.parse("budget_money"), ((KEY, "budget_money"),))
        self.assertEqual(paths.parse("building_resources.*[*][1]"),
                         ((KEY, "building_resources"), (ANY_KEY, None), (ANY_INDEX, None), (INDEX, 1)))
        self.assertEqual(paths.parse("available_parameters[0].population"),
                         ((KEY, "available_parameters"), (INDEX, 0), (KEY, "population")))

    def test_a_number_after_a_dot_is_a_key_not_an_index(self):
        self.assertEqual(paths.parse("unit_skill_stage.1"), ((KEY, "unit_skill_stage"), (KEY, "1")))

    def test_odd_keys_are_written_as_json_strings(self):
        self.assertEqual(paths.parse('paper["messenger_cost "]'), ((KEY, "paper"), (KEY, "messenger_cost ")))
        self.assertEqual(paths.parse('["a.b"]["*"]'), ((KEY, "a.b"), (KEY, "*")))

    def test_malformed_paths_are_errors(self):
        for bad in ("", ".a", "a.", "a..b", "a b", "a[", "a[1", "a[x]", "a[-1]", 'a["x]', "a*", "a.*b", "a.[0]", "a]", None, 5):
            with self.subTest(bad=bad):
                with self.assertRaises(PathError):
                    paths.parse(bad)

    def test_matches_distinguishes_keys_from_indexes(self):
        pattern = paths.parse("building_resources.*[*][1]")
        self.assertTrue(paths.matches(pattern, ("building_resources", "hut", 0, 1)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", 0, 0)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", "0", 1)))
        self.assertFalse(paths.matches(pattern, ("building_resources", "hut", 0)))
        self.assertTrue(paths.matches(paths.parse("unit_skill_stage.1"), ("unit_skill_stage", "1")))
        self.assertFalse(paths.matches(paths.parse("unit_skill_stage.1"), ("unit_skill_stage", 1)))
        self.assertFalse(paths.matches(paths.parse("a.*"), ("a", 0)))

    def test_show_writes_a_path_that_parses_back_to_the_same_place(self):
        for path in (("budget_money",), ("building_resources", "hut", 0, 1), ("paper", "messenger_cost "),
                     ("unit_skill_stage", "1"), ("x", "<unknown built-in variable>"), (0, "a"), ("a.b", "*", 'q"')):
            with self.subTest(path=path):
                self.assertTrue(paths.matches(paths.parse(paths.show(path)), path))
        self.assertEqual(paths.show(("building_resources", "hut", 0, 1)), "building_resources.hut[0][1]")
        self.assertEqual(paths.show(("paper", "messenger_cost ")), 'paper["messenger_cost "]')


if __name__ == "__main__":
    unittest.main()
