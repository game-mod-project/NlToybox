"""dump_tool.py 의 시험. 사용: py -3.14 -m unittest discover -s tools/re/tests -v"""
import contextlib
import importlib.util
import io
import json
import pathlib
import unittest

_spec = importlib.util.spec_from_file_location("dump_tool", pathlib.Path(__file__).resolve().parents[1] / "dump_tool.py")
dump_tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(dump_tool)

NEW = {
    "module_dump": 2, "module_version": "0.2.0", "name": "late1", "seq": 4, "elapsed_seconds": 200.5,
    "room": "rm_game", "delay_seconds": 60, "repeat_seconds": 45, "limits": {"max_ds_id": 100000},
    "present": {"o_data": 1}, "watch": {"global.a.b": {"kind": "bool", "value": 1}},
    "global_status": "AURIE_SUCCESS",
    "globals": {"x": {"kind": "number", "value": 1},
                "s": {"kind": "struct", "members": {"m": {"kind": "number", "value": 745}}}},
    "find_global": {"hits": [{"path": "global.s.m", "why": "value", "kind": "number", "value": 745}],
                    "visited": 10, "truncated": False},
    "find_ds": {"hits": [{"path": "ds_map[19].budget_money", "why": "name", "kind": "number", "value": 2345}],
                "maps": 3, "lists": 1, "highest_map": 412, "highest_list": 97,
                "selfcheck": {"types": True, "made": True, "ds_map": True, "ds_list": True},
                "visited": 20, "truncated": False},
    "instances": {"o_data": {"count": 1, "own": 1, "members": {"state": {"kind": "number", "value": 3}}}},
    "find_instances": {"hits": [], "instances": 1, "visited": 25, "truncated": False},
    "scripts": [],
}

OLD = {
    "module_dump": 1, "elapsed_seconds": 60.0, "global_status": "AURIE_SUCCESS",
    "globals": {"x": {"kind": "number", "value": 1}},
    "find": {"hits": [{"path": "global.x", "why": "value", "kind": "number", "value": 1}], "visited": 1, "truncated": False},
    "scripts": [],
}


class DumpToolTests(unittest.TestCase):
    def test_hits_come_from_every_find_section(self):
        self.assertEqual([(section, hit["path"]) for section, hit in dump_tool.hits(NEW)],
                         [("find_global", "global.s.m"), ("find_ds", "ds_map[19].budget_money")])

    def test_hits_read_the_old_format(self):
        self.assertEqual([(section, hit["path"]) for section, hit in dump_tool.hits(OLD)], [("find", "global.x")])

    def test_flatten_includes_globals_and_instance_members(self):
        flat = dump_tool.flatten(NEW)
        self.assertEqual(flat["global.s.m"], 745)
        self.assertEqual(flat["instance:o_data.state"], 3)

    def test_summary_reports_name_room_selfcheck_and_hits(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            dump_tool.summary(NEW)
        text = out.getvalue()
        self.assertIn("name=late1", text)
        self.assertIn("room=rm_game", text)
        self.assertIn("selfcheck", text)
        self.assertIn("ds_map[19].budget_money", text)

    def test_controls_pass_on_a_trustworthy_dump(self):
        results = dump_tool.controls(NEW, [("global.s.m", 745.0)])
        self.assertTrue(all(ok for _, ok, _ in results), results)

    def test_controls_flag_what_makes_an_absence_untrustworthy(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_ds"]["selfcheck"]["ds_list"] = False          # ds_list 안을 보지 못했다
        bad["find_ds"]["highest_map"] = 99999                   # ds 번호가 본 범위의 끝에 닿았다
        bad["find_instances"]["truncated"] = True               # 한도에 걸렸다
        bad["instances"]["o_data"]["members"] = {}              # 인스턴스 변수를 하나도 읽지 못했다
        failed = {name for name, ok, _ in dump_tool.controls(bad, [("global.nope", 1.0)]) if not ok}
        self.assertEqual(failed, {"expect global.nope", "selfcheck", "ds_range", "truncated", "instances"})

    def test_controls_flag_cut_hits_and_cut_enumeration(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_global"]["hits_cut"] = True                  # 히트를 다 적지 못했다
        bad["find_ds"]["enum_short"] = 2                       # 구조체 둘의 멤버를 다 보지 못했다
        bad["find_instances"]["enum_failed"] = 1
        failed = {name for name, ok, _ in dump_tool.controls(bad, []) if not ok}
        self.assertEqual(failed, {"hits_cut", "enumeration"})

    def test_controls_require_the_type_constants_to_be_verified(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_ds"]["selfcheck"]["types"] = False            # ds 형 상수가 이 러너에서 확인되지 않았다
        failed = {name for name, ok, _ in dump_tool.controls(bad, []) if not ok}
        self.assertEqual(failed, {"selfcheck"})

    def test_controls_flag_a_skipped_section(self):
        bad = json.loads(json.dumps(NEW))
        bad["find_ds"] = {"hits": [], "skipped": True}
        failed = {name for name, ok, _ in dump_tool.controls(bad, []) if not ok}
        self.assertEqual(failed, {"skipped"})


if __name__ == "__main__":
    unittest.main()
