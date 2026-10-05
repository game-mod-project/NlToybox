"""store.py 의 시험. 임시 폴더의 가짜 게임으로 돈다."""
import json
import os
import pathlib
import sys
import tempfile
import unittest
import unittest.mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import catalog
import compose
import preset
import store
from store import StoreError


def make(*changes, name="T", version=support.VERSION, allow_experimental=False):
    return preset.parse({"name": name, "game_version": version, "allow_experimental": allow_experimental,
                         "changes": [{"file": "debug.json", **c} if "file" not in c else c for c in changes]})


GOLD = make({"path": "budget_money", "set": 5000}, name="Gold")
BOOKS = make({"file": "books/*.json", "path": "upgrade_skill[*].value", "mul": 2}, name="Books")


class StoreTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dirs = support.make_game(self.tmp.name)
        self.cat = support.make_catalog()
        self.vanilla = support.game_bytes(self.dirs)

    def apply(self, p):
        return store.apply(self.dirs, self.cat, p, support.VERSION)

    def test_apply_writes_the_value_keeps_a_snapshot_and_records_the_hash(self):
        edits, written, restored = self.apply(GOLD)
        self.assertEqual((len(edits), written, restored), (1, ["debug.json"], []))
        now = support.game_bytes(self.dirs)
        self.assertEqual(now["debug.json"], self.vanilla["debug.json"].replace(b'"budget_money": 2000', b'"budget_money": 5000'))
        self.assertEqual(now["books/a.json"], self.vanilla["books/a.json"])
        self.assertEqual((self.dirs.snapshot / "debug.json").read_bytes(), self.vanilla["debug.json"])
        self.assertFalse((self.dirs.snapshot / "books").exists())          # 건드리지 않은 파일은 스냅샷도 뜨지 않는다
        state = json.loads((self.dirs.state / "state.json").read_text(encoding="utf-8"))
        self.assertEqual(state["preset"], "Gold")
        self.assertEqual(state["files"], {"debug.json": store.digest(now["debug.json"])})
        self.assertEqual(store.classify(self.dirs, self.cat)["debug.json"], "applied")

    def test_restore_brings_back_every_byte_and_clears_the_state(self):
        self.apply(GOLD)
        self.assertEqual(store.restore(self.dirs, self.cat, support.VERSION), ["debug.json"])
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse((self.dirs.state / "state.json").exists())
        self.assertEqual(store.restore(self.dirs, self.cat, support.VERSION), [])

    def test_applying_another_preset_starts_from_vanilla(self):
        self.apply(GOLD)
        edits, written, restored = self.apply(BOOKS)
        self.assertEqual((written, restored), (["books/a.json"], ["debug.json"]))
        now = support.game_bytes(self.dirs)
        self.assertEqual(now["debug.json"], self.vanilla["debug.json"])     # 앞 프리셋의 변경이 남지 않는다
        self.assertIn(b'"value":32', now["books/a.json"])
        self.assertEqual(set(store.classify(self.dirs, self.cat).values()), {"vanilla", "applied"})

    def test_applying_the_same_preset_twice_gives_the_same_bytes(self):
        self.apply(GOLD)
        first = support.game_bytes(self.dirs)
        self.apply(GOLD)
        self.assertEqual(support.game_bytes(self.dirs), first)

    def test_a_preset_that_changes_nothing_leaves_every_byte_alone(self):
        edits, written, restored = self.apply(make({"path": "budget_money", "set": 2000}))
        self.assertEqual((edits, written, restored), ([], [], []))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_a_file_changed_outside_the_tool_stops_everything(self):
        self.apply(GOLD)
        applied = support.game_bytes(self.dirs)
        target = self.dirs.game / "books" / "b.json"
        target.write_bytes(b'{"upgrade_skill":[],"tag":1}')                # 게임 갱신이나 손으로 고친 것
        changed = support.game_bytes(self.dirs)
        for action in (lambda: self.apply(BOOKS), lambda: store.restore(self.dirs, self.cat, support.VERSION),
                       lambda: store.plan(self.dirs, self.cat, GOLD, support.VERSION)):
            with self.assertRaises(StoreError) as caught:
                action()
            self.assertIn("books/b.json", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), changed)           # 아무것도 쓰지 않았다
        self.assertNotEqual(changed, applied)

    def test_a_missing_game_file_stops_everything(self):
        (self.dirs.game / "books" / "b.json").unlink()
        with self.assertRaises(StoreError):
            self.apply(GOLD)
        self.assertFalse(self.dirs.snapshot.exists())

    def test_the_game_version_must_match_catalog_and_preset(self):
        with self.assertRaises(StoreError) as caught:
            store.apply(self.dirs, self.cat, GOLD, "9.9.9.9")
        self.assertIn("카탈로그", str(caught.exception))
        with self.assertRaises(StoreError) as caught:
            self.apply(make({"path": "budget_money", "set": 1}, version="9.9.9.9"))
        self.assertIn("프리셋", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_a_snapshot_that_is_not_vanilla_is_not_trusted(self):
        snap = self.dirs.snapshot / "debug.json"
        snap.parent.mkdir(parents=True)
        snap.write_bytes(b'{"budget_money": 1}')                           # 고친 파일에서 뜬 스냅샷
        with self.assertRaises(StoreError) as caught:
            self.apply(GOLD)
        self.assertIn("스냅샷", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)

    def test_an_applied_file_without_its_snapshot_cannot_be_restored(self):
        self.apply(GOLD)
        (self.dirs.snapshot / "debug.json").unlink()
        applied = support.game_bytes(self.dirs)
        for action in (lambda: store.restore(self.dirs, self.cat, support.VERSION), lambda: self.apply(BOOKS)):
            with self.assertRaises(StoreError) as caught:
                action()
            self.assertIn("스냅샷이 없", str(caught.exception))
        self.assertEqual(support.game_bytes(self.dirs), applied)

    def test_a_preset_that_cannot_be_applied_writes_nothing(self):
        with self.assertRaises(compose.ComposeError):
            self.apply(make({"path": "budget_money", "set": 5000}, {"path": "no_such_key", "set": 1}))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse(self.dirs.state.exists())
        self.assertFalse(self.dirs.snapshot.exists())                      # 스냅샷도 뜨지 않는다

    def test_only_files_that_change_get_a_snapshot(self):
        self.apply(BOOKS)                                                  # books/b.json 에는 바뀔 값이 없다
        self.assertTrue((self.dirs.snapshot / "books" / "a.json").is_file())
        self.assertFalse((self.dirs.snapshot / "books" / "b.json").exists())

    def test_a_game_file_that_changed_after_it_was_classified_is_not_taken_for_vanilla(self):
        status = store.classify(self.dirs, self.cat)                       # 이때는 바닐라였다
        (self.dirs.game / "debug.json").write_bytes(self.vanilla["debug.json"].replace(b"2000", b"2001"))
        for action in (lambda: store.vanilla_bytes(self.dirs, self.cat, "debug.json", status),
                       lambda: store.ensure_snapshot(self.dirs, self.cat, "debug.json", status)):
            with self.assertRaises(StoreError) as caught:
                action()
            self.assertIn("바뀌었다", str(caught.exception))
        self.assertFalse(self.dirs.snapshot.exists())

    def test_restore_also_brings_back_the_modification_time(self):
        target = self.dirs.game / "debug.json"
        steam = 1_700_000_000_000_000_000                                  # Steam 이 파일을 쓴 때
        os.utime(target, ns=(steam, steam))
        self.apply(GOLD)
        self.assertEqual((self.dirs.snapshot / "debug.json").stat().st_mtime_ns, steam)
        self.assertNotEqual(target.stat().st_mtime_ns, steam)              # 고친 파일은 지금 쓴 것이다
        store.restore(self.dirs, self.cat, support.VERSION)
        self.assertEqual(target.stat().st_mtime_ns, steam)

    def test_plan_writes_nothing(self):
        status, data, edits = store.plan(self.dirs, self.cat, GOLD, support.VERSION)
        self.assertEqual((sorted(data), len(edits)), (["debug.json"], 1))
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse(self.dirs.snapshot.exists())
        self.assertFalse(self.dirs.state.exists())

    def test_a_crash_at_any_write_leaves_files_the_tool_still_recognises(self):
        both = make({"path": "budget_money", "set": 7000},
                    {"file": "books/*.json", "path": "upgrade_skill[*].value", "mul": 2}, name="Both")
        self.apply(both)                                                   # 스냅샷을 미리 만들어 둔다
        store.restore(self.dirs, self.cat, support.VERSION)
        real = store.write
        # Gold 가 입혀진 위에 Both 를 입히면 쓰기는 넷이다: debug.json 되돌리기, 상태, books/a.json, debug.json
        for crash_at in (1, 2, 3, 4):
            self.apply(GOLD)
            calls = []

            def flaky(path, data, *rest):
                calls.append(path)
                if len(calls) == crash_at:
                    raise OSError("죽었다")
                real(path, data, *rest)

            with unittest.mock.patch.object(store, "write", flaky):
                with self.assertRaises(StoreError) as caught:
                    self.apply(both)
            self.assertIn("쓰는 도중에 실패했다", str(caught.exception))
            self.assertNotIn("unknown", store.classify(self.dirs, self.cat).values(), f"{crash_at}번째 쓰기")
            store.restore(self.dirs, self.cat, support.VERSION)
            self.assertEqual(support.game_bytes(self.dirs), self.vanilla, f"{crash_at}번째 쓰기")

    def test_no_temporary_files_are_left_behind(self):
        self.apply(GOLD)
        self.apply(BOOKS)
        store.restore(self.dirs, self.cat, support.VERSION)
        root = pathlib.Path(self.tmp.name)
        self.assertEqual([p for p in root.rglob("*") if p.name.endswith(store.TMP_SUFFIX)], [])

    def test_a_bom_and_crlf_survive_apply_and_restore(self):
        files = dict(support.FILES)
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        dirs = support.make_game(tmp.name, files)
        raw = b"\xef\xbb\xbf" + files["debug.json"].encode("utf-8")
        (dirs.game / "debug.json").write_bytes(raw)
        data = support.files_data(files)
        data["files"]["debug.json"] = store.digest(raw)
        cat = catalog.build(support.keys_data(), data)
        store.apply(dirs, cat, GOLD, support.VERSION)
        self.assertEqual((dirs.game / "debug.json").read_bytes(), raw.replace(b'"budget_money": 2000', b'"budget_money": 5000'))
        store.restore(dirs, cat, support.VERSION)
        self.assertEqual((dirs.game / "debug.json").read_bytes(), raw)


if __name__ == "__main__":
    unittest.main()
