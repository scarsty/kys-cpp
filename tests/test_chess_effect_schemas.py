from __future__ import annotations

import json
import unittest
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas" / "chess_effects"


class NamedEffectSchemaTests(unittest.TestCase):
    def schema(self, name):
        return json.loads((SCHEMA_DIR / f"chess_{name}.schema.json").read_text(encoding="utf-8"))

    def test_public_effects_reject_wiring_unknown_parameters_and_missing_values(self):
        schema = self.schema("magic_effects")
        validator = jsonschema.Draft202012Validator(schema)
        for effect in (
            {"類型": "不存在"},
            {"類型": "命中眩暈"},
            {"類型": "命中眩暈", "持續幀數": "abc"},
            {"類型": "命中眩暈", "持續幀數": 30, "時機": "開場"},
            {"類型": "機率命中眩暈", "持續幀數": 30, "機率百分比": 101},
            {"時機": "主彈命中", "套用狀態": {"狀態": "眩暈", "持續幀數": 30}},
        ):
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [effect]}]}
            with self.subTest(effect=effect):
                self.assertTrue(list(validator.iter_errors(document)))

    def test_composable_effect_choices_and_optional_amount_terms_match_the_parser(self):
        schema = self.schema("magic_effects")
        validator = jsonschema.Draft202012Validator(schema)
        for effect in (
            {"類型": "出招護盾", "每星護盾": 70},
            {"類型": "出招護盾", "固定護盾": 20, "每星護盾": 70, "生命護盾百分比": 5},
            {"類型": "出招治療自身", "每星治療": 30, "生命治療百分比": 6},
            {"類型": "出招臨時屬性加成", "屬性": "格擋率", "百分比": 25, "持續幀數": 60},
            {"類型": "出招全隊攻擊加成", "每星攻擊": 66, "持續幀數": 100},
            {"類型": "出招全隊防禦加成", "每星防禦": 66, "持續幀數": 100},
        ):
            validator.validate({"絕招": [{"武功": 108, "名稱": "測試", "效果": [effect]}]})
        for attribute in (3, "不存在"):
            effect = {"類型": "出招臨時屬性加成", "屬性": attribute, "百分比": 25, "持續幀數": 60}
            self.assertTrue(list(validator.iter_errors({"絕招": [{"武功": 108, "名稱": "測試", "效果": [effect]}]})))
        self.assertNotIn("出招格擋護盾", schema["$defs"])
        self.assertNotIn("破盾反擊與陣亡補盾", schema["$defs"])
        self.assertEqual(schema["$defs"]["出招護盾"]["required"], ["類型"])
        self.assertEqual(schema["$defs"]["出招護盾"]["properties"]["固定護盾"]["default"], 0)


if __name__ == "__main__":
    unittest.main()
