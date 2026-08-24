from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path

import jsonschema
import yaml


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas" / "chess_effects"


class GenerateChessEffectSchemasTests(unittest.TestCase):
    def test_generated_effect_schemas_have_no_drift(self) -> None:
        subprocess.run(
            [sys.executable, str(ROOT / "tools" / "generate_chess_effect_schemas.py"), "--check"],
            cwd=ROOT,
            check=True,
        )

    def test_effect_schemas_accept_all_formal_configs(self) -> None:
        for name in ("combos", "equipment", "magic_effects", "neigong"):
            schema = json.loads(
                (SCHEMA_DIR / f"chess_{name}.schema.json").read_text(encoding="utf-8")
            )
            config = yaml.safe_load(
                (ROOT / "config" / f"chess_{name}.yaml").read_text(encoding="utf-8")
            )
            jsonschema.Draft202012Validator(schema).validate(config)

    def test_magic_schema_rejects_removed_author_shapes_and_bad_periods(self) -> None:
        schema = json.loads(
            (SCHEMA_DIR / "chess_magic_effects.schema.json").read_text(encoding="utf-8")
        )
        validator = jsonschema.Draft202012Validator(schema)

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertTrue(errors({"事件": "傷害結算後", "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "傷害後", "動作": [{"類型": "資源變更", "資源": "護盾"}]}))
        self.assertTrue(errors({"時機": "傷害後", "條件": [{"類型": "已接受命中"}], "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {"固定": {"攻擊": 10}}}))
        self.assertTrue(errors({"時機": "每幀", "間隔幀數": 1, "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "每隔", "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "每隔", "間隔幀數": 0, "獲得護盾": 1}))
        self.assertFalse(errors({"時機": "每幀", "獲得護盾": 1}))
        self.assertFalse(errors({"時機": "每隔", "間隔幀數": 1, "獲得護盾": 1}))

    def test_schema_uses_descriptor_enums_and_rejects_empty_macros(self) -> None:
        schema = json.loads(
            (SCHEMA_DIR / "chess_magic_effects.schema.json").read_text(encoding="utf-8")
        )
        validator = jsonschema.Draft202012Validator(schema)

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertFalse(errors({"時機": "開場", "套用狀態": {"狀態": "中毒"}}))
        self.assertTrue(errors({"時機": "開場", "套用狀態": {"狀態": "未知狀態"}}))
        self.assertTrue(errors({
            "時機": "開場",
            "屬性修正": {"屬性": "攻擊", "方式": "未知運算", "數值": 1},
        }))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {}}))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {"持續幀數": 30}}))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {"百分比": {}}}))
        for macro in ("回復內力", "獲得護盾", "忽略防禦", "單次承傷上限"):
            with self.subTest(macro=macro):
                self.assertTrue(errors({"時機": "開場", macro: {}}))


if __name__ == "__main__":
    unittest.main()
