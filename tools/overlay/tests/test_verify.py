"""verify.py 의 시험."""
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import support
import compose
import preset
import verify


def edits_for(*changes):
    p = preset.parse({"name": "T", "game_version": support.VERSION, "allow_experimental": True, "changes": list(changes)})
    return compose.compose(p, support.make_catalog(), dict(support.FILES))[1]


EDITS = edits_for({"file": "debug.json", "path": "budget_money", "set": 2345},
                  {"file": "debug.json", "path": "production_cost.ale", "set": 2311.5},
                  {"file": "debug.json", "path": "building_resources.hut[0][1]", "set": 1517},
                  {"file": "debug.json", "path": "factor", "set": 0.5731})


class VerifyTests(unittest.TestCase):
    def test_probe_request_finds_every_new_value_and_names_what_sits_in_arrays(self):
        self.assertEqual(verify.probe_request(EDITS).splitlines()[1:],
                         ["delay_seconds=60", "max_hits=5000", "find=2345", "find=0.5731", "find=2311.5", "find=1517", "find_name=hut"])

    def test_probe_request_writes_each_value_once(self):
        edits = edits_for({"file": "debug.json", "path": "production_cost.*", "set": 77})
        self.assertEqual(verify.probe_request(edits).count("find=77"), 1)

    def test_verdicts_tell_anchored_from_value_only_from_absent(self):
        dump = {
            "find_ds": {"hits": [
                {"path": "ds_map[150].budget_money", "why": "name", "kind": "number", "value": 2345},
                {"path": "ds_map[150].budget_money", "why": "value", "kind": "number", "value": 2345},
                {"path": "ds_list[412][1]", "why": "value", "kind": "number", "value": 1517},
                {"path": "ds_map[150].other", "why": "value", "kind": "bool", "value": True}]},
            "find_instances": {"hits": [
                {"path": "instance:o_debug#0.debug_factor", "why": "value", "kind": "number", "value": 0.5731}]},
        }
        result = {edit.path: (verdict, where) for edit, verdict, where in verify.verdicts(EDITS, dump)}
        self.assertEqual(result[("budget_money",)], ("anchored", ["ds_map[150].budget_money"]))
        self.assertEqual(result[("factor",)], ("anchored", ["instance:o_debug#0.debug_factor"]))
        self.assertEqual(result[("building_resources", "hut", 0, 1)], ("value-only", ["ds_list[412][1]"]))
        self.assertEqual(result[("production_cost", "ale")], ("absent", []))

    def test_a_name_hit_alone_is_not_evidence(self):
        dump = {"find_ds": {"hits": [{"path": "ds_map[150].budget_money", "why": "name", "kind": "number", "value": 2345}]}}
        self.assertEqual([verdict for _, verdict, _ in verify.verdicts(EDITS[:1], dump)], ["absent"])

    def test_the_anchor_ignores_the_trailing_space_of_a_key(self):
        self.assertEqual(verify.anchor(("paper", "messenger_cost ")), "messenger_cost")
        self.assertEqual(verify.anchor(("a", "b", 0, 1)), "b")
        self.assertIsNone(verify.anchor((0, 1)))


if __name__ == "__main__":
    unittest.main()
