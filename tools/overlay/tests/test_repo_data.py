"""레포에 든 카탈로그와 프리셋이 읽히는지 본다. 게임 폴더는 보지 않는다."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import catalog
import compose
import preset

REPO = pathlib.Path(__file__).resolve().parents[3]


class RepoDataTests(unittest.TestCase):
    def test_every_catalog_loads_and_is_named_after_its_game_version(self):
        directories = sorted(path for path in (REPO / "catalog").iterdir() if path.is_dir())
        self.assertTrue(directories)
        for directory in directories:
            with self.subTest(catalog=directory.name):
                self.assertEqual(catalog.load(directory).game_version, directory.name)

    def test_every_preset_loads_and_names_files_its_catalog_knows(self):
        files = sorted((REPO / "presets").glob("*.json"))
        self.assertTrue(files)
        for path in files:
            with self.subTest(preset=path.name):
                loaded = preset.load(path)
                cat = catalog.load(REPO / "catalog" / loaded.game_version)
                self.assertTrue(compose.targets(loaded, cat))


if __name__ == "__main__":
    unittest.main()
