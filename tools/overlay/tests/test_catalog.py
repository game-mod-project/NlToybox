"""catalog.py 의 시험."""
import copy
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import catalog
import paths
from catalog import CatalogError


class CatalogTests(unittest.TestCase):
    def test_find_returns_the_first_group_that_matches_file_and_path(self):
        cat = support.make_catalog()
        self.assertEqual(cat.find("debug.json", ("budget_money",)).runtime, "VERIFIED")
        self.assertEqual(cat.find("debug.json", ("production_cost", "ale")).runtime, "SEEN")
        self.assertEqual(cat.find("books/a.json", ("upgrade_skill", 0, "value")).effect_by, "community")
        self.assertIsNone(cat.find("debug.json", ("name",)))
        self.assertIsNone(cat.find("books/a.json", ("budget_money",)))

    def test_unseen_keys_and_marked_keys_are_experimental(self):
        cat = support.make_catalog()
        self.assertFalse(cat.find("debug.json", ("budget_money",)).experimental)
        self.assertTrue(cat.find("debug.json", ("building_resources", "hut", 0, 1)).experimental)
        self.assertTrue(cat.find("debug.json", ("slave", "cost")).experimental)

    def test_file_patterns_match_within_one_path_segment(self):
        self.assertTrue(catalog.file_matches("books/*.json", "books/a.json"))
        self.assertFalse(catalog.file_matches("books/*.json", "books/sub/a.json"))
        self.assertFalse(catalog.file_matches("books/*.json", "books/a.jsonx"))
        self.assertFalse(catalog.file_matches("debug.json", "debugXjson"))
        self.assertEqual(support.make_catalog().files_matching("books/*.json"), ["books/a.json", "books/b.json"])

    def test_paths_outside_the_game_folder_are_refused(self):
        for bad in ("", "..", "../x.json", "a/../x.json", "/x.json", "C:/x.json", "a\\x.json", "a//x.json", "./x.json", None):
            with self.subTest(bad=bad):
                with self.assertRaises(CatalogError):
                    catalog.check_rel(bad)

    def test_a_mistyped_keys_file_is_an_error(self):
        def broken(change):
            groups = copy.deepcopy(support.GROUPS)
            change(groups)
            return groups

        cases = {
            "모르는 항목": lambda g: g[0].update(runtme="SEEN"),
            "runtime": lambda g: g[0].update(runtime="MAYBE"),
            "effect": lambda g: g[0].update(effect="PROVEN"),
            "effect_by": lambda g: g[4].pop("effect_by"),
            "paths": lambda g: g[0].update(paths=[]),
            "점이나 대괄호": lambda g: g[0].update(paths=["a b"]),
            "min 이 max": lambda g: g[0].update(min=5, max=1),
            "유한한 수": lambda g: g[0].update(min="0"),
            "맞는 파일이 없다": lambda g: g[0].update(file="nope.json"),
            "상대경로": lambda g: g[0].update(file="../debug.json"),
        }
        for message, change in cases.items():
            with self.subTest(message=message):
                with self.assertRaises(CatalogError) as caught:
                    support.make_catalog(broken(change))
                self.assertIn(message, str(caught.exception))

    def test_keys_and_files_must_be_for_the_same_game_version(self):
        files = support.files_data()
        files["game_version"] = "9.9.9.9"
        with self.assertRaises(CatalogError):
            catalog.build(support.keys_data(), files)

    def test_file_hashes_must_be_uppercase_sha256(self):
        files = support.files_data()
        files["files"]["debug.json"] = files["files"]["debug.json"].lower()
        with self.assertRaises(CatalogError):
            catalog.build(support.keys_data(), files)

    def test_load_reads_the_two_files_and_rejects_duplicate_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = pathlib.Path(tmp)
            (tmp / "keys.json").write_text(json.dumps(support.keys_data()), encoding="utf-8")
            with self.assertRaises(CatalogError) as caught:
                catalog.load(tmp)
            self.assertIn("files.json", str(caught.exception))
            (tmp / "files.json").write_text(json.dumps(support.files_data()), encoding="utf-8")
            self.assertEqual(len(catalog.load(tmp).files), 3)
            (tmp / "files.json").write_text('{"game_version": "1", "game_version": "2", "files": {}}', encoding="utf-8")
            with self.assertRaises(CatalogError) as caught:
                catalog.load(tmp)
            self.assertIn("두 번", str(caught.exception))

    def test_group_paths_are_parsed(self):
        group = support.make_catalog().groups[2]
        self.assertEqual(group.paths, (paths.parse("building_resources.*[*][1]"),))


if __name__ == "__main__":
    unittest.main()
