from __future__ import annotations

import copy
import json
import shutil
import sqlite3
import subprocess
import tempfile
import unittest
from contextlib import closing
from pathlib import Path

import jsonschema
import yaml

ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas" / "chess_effects"
SCHEMA_NAMES = ("combos", "equipment", "magic_effects", "neigong")


class NamedEffectSchemaTests(unittest.TestCase):
    def schema(self, name):
        return json.loads((SCHEMA_DIR / f"chess_{name}.schema.json").read_text(encoding="utf-8"))

    def test_all_formal_configs_match_the_public_schema(self):
        for name in SCHEMA_NAMES:
            with self.subTest(name=name):
                schema = self.schema(name)
                jsonschema.Draft202012Validator.check_schema(schema)
                config = yaml.safe_load((ROOT / "config" / f"chess_{name}.yaml").read_text(encoding="utf-8"))
                # JSON object keys are strings; the YAML parser preserves numeric identifiers.
                config = json.loads(json.dumps(config, ensure_ascii=False))
                jsonschema.Draft202012Validator(schema).validate(config)

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

    def test_no_authored_card_summaries_or_runtime_wiring_remain(self):
        def visit(value):
            if isinstance(value, dict):
                self.assertNotIn("卡片摘要", value)
                if "類型" in value and "效果" not in value:
                    self.assertNotIn("時機", value)
                    self.assertNotIn("狀態槽", value)
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)
        for name in SCHEMA_NAMES:
            visit(yaml.safe_load((ROOT / "config" / f"chess_{name}.yaml").read_text(encoding="utf-8")))

    def test_all_inner_skills_are_tiered_and_have_manuals(self):
        config = yaml.safe_load((ROOT / "config/chess_neigong.yaml").read_text(encoding="utf-8"))
        ids = [mid for tier in config["層級分配"] for mid in tier["武功"]]
        self.assertEqual(len(ids), len(set(ids)))
        self.assertEqual(set(ids), set(config["效果"]))
        self.assertEqual(len(ids), 19)
        self.assertIn(103, next(t["武功"] for t in config["層級分配"] if t["層級"] == 3))
        with closing(sqlite3.connect(f"file:{(ROOT / 'work/game-dev/save/game.db').as_posix()}?mode=ro", uri=True)) as db:
            manuals = {r[0] for r in db.execute("select 练出武功 from item where 物品类型=2")}
        self.assertTrue(set(ids) <= manuals)

    def test_shared_effects_use_the_same_global_definition(self):
        config = yaml.safe_load((ROOT / "config/chess_magic_effects.yaml").read_text(encoding="utf-8"))
        by_id = {e["武功"]: e["效果"] for e in config["絕招"]}
        for left, right in ((16, 46), (24, 22)):
            self.assertEqual(by_id[left], by_id[right])
        schema = self.schema("magic_effects")
        self.assertIn("承傷蓄力加傷", schema["$defs"])
        self.assertNotIn("太極反擊", schema["$defs"])


class ChessContentValidationCommandTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.executable = ROOT / "x64" / "Debug" / "kys_chess_cli.exe"
        if not cls.executable.exists():
            raise unittest.SkipTest("kys_chess_cli has not been built")

    def run_cli(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.executable), *arguments],
            cwd=ROOT,
            text=True,
            encoding="utf-8",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )

    def content_arguments(self, config_root: Path) -> list[str]:
        return [
            "--data-root",
            str(ROOT / "work" / "game-dev"),
            "--config-root",
            str(config_root),
        ]

    def test_help_lists_validate_content(self) -> None:
        result = self.run_cli("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("validate-content", result.stdout)

    def test_validate_content_accepts_one_or_all_difficulties(self) -> None:
        explicit = self.run_cli(
            "validate-content",
            "--difficulty",
            "normal",
            *self.content_arguments(ROOT / "config"),
        )
        self.assertEqual(explicit.returncode, 0, explicit.stderr)
        self.assertIn("內容驗證成功：normal", explicit.stdout)
        self.assertNotIn("內容驗證成功：easy", explicit.stdout)

        all_difficulties = self.run_cli(
            "validate-content", *self.content_arguments(ROOT / "config")
        )
        self.assertEqual(all_difficulties.returncode, 0, all_difficulties.stderr)
        for difficulty in ("easy", "normal", "hard"):
            self.assertIn(f"內容驗證成功：{difficulty}", all_difficulties.stdout)

    def test_validate_content_rejects_unknown_named_effect(self):
        with tempfile.TemporaryDirectory(prefix="kys-invalid-effect-") as temp:
            config_root = Path(temp) / "config"
            shutil.copytree(ROOT / "config", config_root)
            path = config_root / "chess_magic_effects.yaml"
            content = yaml.safe_load(path.read_text(encoding="utf-8"))
            content["絕招"][0]["效果"][0]["類型"] = "不存在的效果"
            path.write_text(yaml.safe_dump(content, allow_unicode=True), encoding="utf-8")
            result = self.run_cli("validate-content", "--difficulty", "normal", *self.content_arguments(config_root))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("未知效果", result.stderr)

    def test_validate_content_rejects_incompatible_effect_attachment(self):
        with tempfile.TemporaryDirectory(prefix="kys-invalid-effect-source-") as temp:
            config_root = Path(temp) / "config"
            shutil.copytree(ROOT / "config", config_root)
            path = config_root / "chess_magic_effects.yaml"
            content = yaml.safe_load(path.read_text(encoding="utf-8"))
            content["絕招"][0]["效果"] = [{"類型": "技能增傷", "百分比": 15}]
            path.write_text(yaml.safe_dump(content, allow_unicode=True), encoding="utf-8")
            result = self.run_cli("validate-content", "--difficulty", "normal", *self.content_arguments(config_root))
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("開場效果", result.stderr)


if __name__ == "__main__":
    unittest.main()
