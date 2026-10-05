"""cli.py 의 시험. 명령을 끝에서 끝까지 임시 폴더의 가짜 게임으로 돌린다."""
import contextlib
import io
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import cli


class CliTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = pathlib.Path(tmp.name)
        self.dirs = support.make_game(self.root)
        self.catalog_dir = self.root / "catalog"
        self.catalog_dir.mkdir()
        (self.catalog_dir / "keys.json").write_text(json.dumps(support.keys_data()), encoding="utf-8")
        (self.catalog_dir / "files.json").write_text(json.dumps(support.files_data()), encoding="utf-8")
        self.preset = self.root / "gold.json"
        self.preset.write_text(json.dumps({"name": "Gold", "game_version": support.VERSION, "changes": [
            {"file": "debug.json", "path": "budget_money", "set": 5000},
            {"file": "debug.json", "path": "production_cost.ale", "mul": 2}]}), encoding="utf-8")
        self.vanilla = support.game_bytes(self.dirs)

    def run_cli(self, command, *extra, version=support.VERSION):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main([command, "--game-dir", str(self.dirs.game), "--game-version", version,
                             "--catalog-dir", str(self.catalog_dir), "--snapshot-dir", str(self.dirs.snapshot),
                             "--state-dir", str(self.dirs.state), *extra])
        return code, out.getvalue()

    def test_check_shows_the_changes_and_writes_nothing(self):
        code, out = self.run_cli("check", "--preset", str(self.preset))
        self.assertEqual(code, 0, out)
        self.assertIn("debug.json  budget_money  2000 -> 5000  [VERIFIED/UNKNOWN]", out)
        self.assertIn("debug.json  production_cost.ale  2 -> 4  [SEEN/UNKNOWN]", out)
        self.assertIn("변경 2개, 파일 1개. 런타임 반영을 확인하지 않은 키 1개", out)
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        self.assertFalse(self.dirs.snapshot.exists())

    def test_apply_status_restore_round_trip(self):
        code, out = self.run_cli("apply", "--preset", str(self.preset))
        self.assertEqual(code, 0, out)
        self.assertIn("apply ok: Gold", out)
        self.assertIn(b'"budget_money": 5000', (self.dirs.game / "debug.json").read_bytes())
        code, out = self.run_cli("status")
        self.assertEqual(code, 0, out)
        self.assertIn("applied: 1", out)
        self.assertIn("프리셋: Gold", out)
        code, out = self.run_cli("restore")
        self.assertEqual(code, 0, out)
        self.assertIn("restore ok (1)", out)
        self.assertEqual(support.game_bytes(self.dirs), self.vanilla)
        code, out = self.run_cli("status")
        self.assertIn("프리셋: (없음. 바닐라)", out)

    def test_a_refusal_prints_fail_and_exits_1(self):
        code, out = self.run_cli("apply", "--preset", str(self.preset), version="9.9.9.9")
        self.assertEqual(code, 1)
        self.assertIn("FAIL: 카탈로그", out)
        code, out = self.run_cli("apply", "--preset", str(self.root / "none.json"))
        self.assertEqual(code, 1)
        self.assertIn("FAIL: 파일이 없다", out)

    def test_status_exits_1_when_a_file_is_unknown(self):
        (self.dirs.game / "books" / "a.json").write_bytes(b"{}")
        code, out = self.run_cli("status")
        self.assertEqual(code, 1)
        self.assertIn("unknown: 1", out)
        self.assertIn("books/a.json", out)

    def test_keys_lists_every_number_with_its_grades(self):
        target = self.root / "out" / "keys.tsv"
        code, out = self.run_cli("keys", "--out", str(target))
        self.assertEqual(code, 0, out)
        lines = target.read_text(encoding="utf-8").splitlines()
        self.assertEqual(lines[0], "file\tpath\tvalue\truntime\teffect\texperimental")
        self.assertIn("debug.json\tbudget_money\t2000\tVERIFIED\tUNKNOWN\tN", lines)
        self.assertIn("debug.json\tbuilding_resources.hut[0][1]\t10\tUNSEEN\tUNKNOWN\tY", lines)
        self.assertIn("books/a.json\ttag\t0\t-\t-\t-", lines)              # 카탈로그에 없는 키도 보인다
        self.assertEqual(len(lines), 1 + 11)

    def test_keys_reads_vanilla_even_while_a_preset_is_applied(self):
        self.run_cli("apply", "--preset", str(self.preset))
        code, out = self.run_cli("keys", "--file", "debug.json")
        self.assertEqual(code, 0, out)
        self.assertIn("debug.json\tbudget_money\t2000\t", out)

    def test_pin_writes_the_hashes_of_the_files_the_keys_name(self):
        (self.catalog_dir / "files.json").unlink()
        code, out = self.run_cli("pin")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.loads((self.catalog_dir / "files.json").read_text(encoding="utf-8")), support.files_data())
        code, out = self.run_cli("pin")
        self.assertEqual(code, 1)
        self.assertIn("이미 있다", out)

    def test_scan_checks_every_json_in_the_game_folder(self):
        code, out = self.run_cli("scan")
        self.assertEqual(code, 0, out)
        self.assertIn("파일 3개, 수 11개, 표준 파서가 못 읽는 파일 1개, 실패 0개", out)
        (self.dirs.game / "bad.json").write_bytes(b'{"a": 1 // x\n}')
        code, out = self.run_cli("scan")
        self.assertEqual(code, 1)
        self.assertIn("FAIL bad.json", out)

    def test_probe_request_and_verify(self):
        request = self.root / "req.txt"
        code, out = self.run_cli("probe-request", "--preset", str(self.preset), "--out", str(request))
        self.assertEqual(code, 0, out)
        self.assertIn("find=5000\nfind=4\n", request.read_text(encoding="utf-8"))
        dump = self.root / "dump.json"
        dump.write_text(json.dumps({"find_ds": {"hits": [
            {"path": "ds_map[1].budget_money", "why": "value", "kind": "number", "value": 5000}]}}), encoding="utf-8")
        code, out = self.run_cli("verify", "--preset", str(self.preset), "--dump", str(dump))
        self.assertEqual(code, 1, out)                                     # 하나가 absent 다
        self.assertIn("anchored    debug.json  budget_money = 5000", out)
        self.assertIn("absent      debug.json  production_cost.ale = 4", out)
        self.assertIn("anchored 1, value-only 0, absent 1", out)

    def test_a_command_without_its_argument_is_a_usage_error(self):
        with self.assertRaises(SystemExit) as caught, contextlib.redirect_stderr(io.StringIO()):
            self.run_cli("apply")
        self.assertEqual(caught.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
