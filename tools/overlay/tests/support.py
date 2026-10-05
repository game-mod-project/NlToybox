"""시험이 함께 쓰는 것: 작은 가짜 게임과 그 카탈로그."""
import hashlib
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import catalog

VERSION = "1.2.3.4"

# 게임 파일의 모양을 줄인 것. debug.json 에는 CRLF, 탭, 닫는 괄호 앞의 쉼표, 긴 소수, '.0' 꼴이 있다.
FILES = {
    "debug.json": '{\r\n\t"budget_money": 2000,\r\n\t"factor": 0.5,\r\n\t"production_cost": {\r\n\t\t"ale": 2,\r\n'
                  '\t\t"coal": 0.90000000000000002,\r\n\t},\r\n\t"building_resources": {\r\n'
                  '\t\t"hut": [["wood", 10], ["iron", 5]],\r\n\t\t"mine": [["wood", 15]],\r\n\t},\r\n'
                  '\t"name": "x",\r\n\t"slave": {"cost": 50.0}\r\n}',
    "books/a.json": '{"upgrade_skill":[{"value":16,"name":"combat"}],"tag":0}',
    "books/b.json": '{"upgrade_skill":[],"tag":0}',
}

GROUPS = [
    {"file": "debug.json", "paths": ["budget_money"], "runtime": "VERIFIED", "effect": "UNKNOWN", "min": 0, "max": 100000},
    {"file": "debug.json", "paths": ["factor", "production_cost.*"], "runtime": "SEEN", "effect": "UNKNOWN"},
    {"file": "debug.json", "paths": ["building_resources.*[*][1]"], "runtime": "UNSEEN", "effect": "UNKNOWN"},
    {"file": "debug.json", "paths": ["slave.*"], "runtime": "SEEN", "effect": "UNKNOWN", "experimental": True},
    {"file": "books/*.json", "paths": ["upgrade_skill[*].value"], "runtime": "SEEN", "effect": "TESTED", "effect_by": "community"},
]


def sha(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest().upper()


def keys_data(groups=None):
    return {"game_version": VERSION, "groups": GROUPS if groups is None else groups}


def files_data(files=None):
    return {"game_version": VERSION, "files": {rel: sha(text) for rel, text in (FILES if files is None else files).items()}}


def make_catalog(groups=None, files=None):
    return catalog.build(keys_data(groups), files_data(files))
