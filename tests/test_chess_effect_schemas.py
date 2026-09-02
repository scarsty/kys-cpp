from __future__ import annotations

import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import jsonschema
import yaml


ROOT = Path(__file__).resolve().parents[1]
SCHEMA_DIR = ROOT / "schemas" / "chess_effects"
SCHEMA_NAMES = ("combos", "equipment", "magic_effects", "neigong")


class ChessEffectSchemasTests(unittest.TestCase):
    def schema(self, name: str) -> dict:
        return json.loads(
            (SCHEMA_DIR / f"chess_{name}.schema.json").read_text(encoding="utf-8")
        )

    def test_effect_schemas_are_valid_and_accept_all_formal_configs(self) -> None:
        for name in SCHEMA_NAMES:
            with self.subTest(name=name):
                schema = self.schema(name)
                jsonschema.Draft202012Validator.check_schema(schema)
                config = yaml.safe_load(
                    (ROOT / "config" / f"chess_{name}.yaml").read_text(
                        encoding="utf-8"
                    )
                )
                jsonschema.Draft202012Validator(schema).validate(config)

    def test_magic_schema_rejects_removed_author_shapes_and_bad_periods(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertTrue(errors({"事件": "傷害結算後", "獲得護盾": 1}))
        self.assertTrue(
            errors(
                {
                    "時機": "傷害後",
                    "動作": [{"類型": "資源變更", "資源": "護盾"}],
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "傷害後",
                    "條件": [{"類型": "已接受命中"}],
                    "獲得護盾": 1,
                }
            )
        )
        self.assertTrue(
            errors({"時機": "開場", "屬性加成": {"固定": {"攻擊": 10}}})
        )
        self.assertTrue(errors({"時機": "每幀", "間隔幀數": 1, "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "每隔", "獲得護盾": 1}))
        self.assertTrue(errors({"時機": "每隔", "間隔幀數": 0, "獲得護盾": 1}))
        self.assertFalse(errors({"時機": "每幀", "獲得護盾": 1}))
        self.assertFalse(errors({"時機": "每隔", "間隔幀數": 1, "獲得護盾": 1}))

    def test_schema_rejects_promoted_action_conflicts_and_empty_macros(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertTrue(errors({"時機": "開場", "套用狀態": {"狀態": "中毒"}}))
        self.assertTrue(errors({"時機": "開場", "套用狀態": {"狀態": "未知狀態"}}))
        self.assertTrue(
            errors(
                {
                    "時機": "開場",
                    "屬性修正": {"屬性": "攻擊", "方式": "未知運算", "數值": 1},
                }
            )
        )
        self.assertTrue(
            errors({"時機": "開場", "獲得護盾": 1, "回復內力": 1})
        )
        self.assertTrue(
            errors({"時機": "開場", "獲得護盾": 1, "動作": [{"獲得護盾": 1}]})
        )
        self.assertTrue(errors({"時機": "開場", "屬性加成": {}}))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {"持續幀數": 30}}))
        self.assertTrue(errors({"時機": "開場", "屬性加成": {"百分比": {}}}))
        for macro in ("回復內力", "獲得護盾", "忽略防禦", "單次承傷上限"):
            with self.subTest(macro=macro):
                self.assertTrue(errors({"時機": "開場", macro: {}}))

    def test_named_mechanisms_are_exact_and_event_aware_at_every_depth(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertFalse(errors({"時機": "開場", "生成分身": {"數量": 1}}))
        self.assertTrue(
            errors(
                {
                    "時機": "開場",
                    "狀態機": {"機制": "生成分身", "數量": 1},
                }
            )
        )
        self.assertTrue(errors({"時機": "開場", "生成分身": {"無敵幀數": 30}}))

        self.assertTrue(errors({"時機": "絕招施放", "生成分身": {"數量": 1}}))
        self.assertTrue(
            errors(
                {
                    "時機": "施放規劃",
                    "複製攻擊定義": {
                        "目標": {
                            "類型": "所有存活單位",
                            "排除效果擁有者": True,
                        },
                        "可選武功條件": [
                            "有絕招攻擊定義",
                            "排除複製與借用遞迴",
                        ],
                    },
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "絕招施放",
                    "借用效果規則": {
                        "目標": "友軍",
                        "來源數量": 1,
                        "允許動作類別": ["傷害修正"],
                    },
                }
            )
        )
        self.assertTrue(errors({"時機": "開場", "目標": "命中目標", "獲得護盾": 1}))
        self.assertTrue(
            errors(
                {
                    "時機": "施放規劃",
                    "條件": [{"傷害種類符合": ["招式"]}],
                    "獲得護盾": 1,
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "絕招施放",
                    "條件": [{"攻擊序號": 1}],
                    "獲得護盾": 1,
                }
            )
        )
        self.assertFalse(
            errors(
                {
                    "時機": "命中",
                    "目標": "交易目標",
                    "獲得護盾": 1,
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "造成傷害後",
                    "目標": "原攻擊目標",
                    "獲得護盾": 1,
                }
            )
        )
        self.assertFalse(
            errors(
                {
                    "時機": "施放結算完成",
                    "條件": ["受益者施放前滿內"],
                    "獲得護盾": 1,
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "開場",
                    "記錄最大招式生命傷害": {
                        "狀態槽": "最大招式生命傷害"
                    },
                }
            )
        )
        self.assertFalse(
            errors(
                {
                    "時機": "造成傷害後",
                    "記錄最大招式生命傷害": {
                        "狀態槽": "最大招式生命傷害"
                    },
                }
            )
        )
        force_move = {
            "方向": "遠離來源",
            "距離格數": 1,
            "碰撞": "阻擋前停止",
            "受阻結果": "縮短",
        }
        self.assertTrue(errors({"時機": "開場", "強制移動": force_move}))
        self.assertFalse(errors({"時機": "命中", "強制移動": force_move}))
        self.assertTrue(errors({"時機": "治療嘗試", "獲得護盾": 1}))
        self.assertTrue(
            errors(
                {
                    "時機": "施放規劃",
                    "獲得護盾": {"實際生命傷害百分比": 50},
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "絕招施放",
                    "條件分支": {
                        "條件": ["僅限絕招"],
                        "成立": [{"生成分身": {"數量": 1}}],
                    },
                }
            )
        )

    def test_poison_action_is_closed_and_old_forms_are_rejected(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        self.assertFalse(
            errors(
                {
                    "時機": "命中",
                    "施加中毒": {
                        "可觸發次數": 3,
                        "持續幀數": 90,
                        "重複套用": "保留較高傷害",
                        "同事件合併": "合計傷害百分比",
                        "效果": {
                            "每次觸發": {"目前生命傷害百分比": 7}
                        },
                    },
                }
            )
        )
        self.assertFalse(
            errors(
                {
                    "時機": "絕招施放",
                    "施加中毒": {
                        "可觸發次數": 5,
                        "持續幀數": 150,
                        "重複套用": "取代並重設",
                        "效果": {
                            "每次觸發": {"目前生命傷害百分比": 10}
                        },
                    },
                }
            )
        )
        self.assertTrue(
            errors(
                {
                    "時機": "命中",
                    "施毒": {"持續幀數": 90, "層數": 3, "強度": 7},
                }
            )
        )

        keep_higher = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "保留較高傷害",
            "效果": {"每次觸發": {"目前生命傷害百分比": 7}},
        }
        self.assertTrue(errors({"時機": "命中", "施加中毒": keep_higher}))
        replace_and_merge = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "取代並重設",
            "同事件合併": "合計傷害百分比",
            "效果": {"每次觸發": {"目前生命傷害百分比": 7}},
        }
        self.assertTrue(
            errors({"時機": "命中", "施加中毒": replace_and_merge})
        )
        removed_same_event_potency = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "保留較高傷害",
            "同事件合併": "合計傷害百分比",
            "同事件合計強度": True,
            "效果": {"每次觸發": {"目前生命傷害百分比": 7}},
        }
        self.assertTrue(
            errors({"時機": "命中", "施加中毒": removed_same_event_potency})
        )
        wrong_poison_scope = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "取代並重設",
            "效果": {"持續生效": {"目前生命傷害百分比": 7}},
        }
        self.assertTrue(
            errors({"時機": "命中", "施加中毒": wrong_poison_scope})
        )

    def test_status_application_schema_is_closed_per_status(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(status: dict) -> list[jsonschema.ValidationError]:
            document = {
                "絕招": [
                    {
                        "武功": 1,
                        "名稱": "測試",
                        "效果": [{"時機": "命中", "套用狀態": status}],
                    }
                ]
            }
            return list(validator.iter_errors(document))

        self.assertFalse(
            errors(
                {
                    "狀態": "眩暈",
                    "持續幀數": 30,
                    "重複套用": "保留較長持續時間",
                }
            )
        )
        self.assertFalse(
            errors(
                {
                    "狀態": "戰意",
                    "增加層數": 1,
                    "層數上限": 10,
                    "效果": {
                        "每層生效": {
                            "招式傷害增加百分比": 5,
                            "傷害減免百分比": 1,
                        }
                    },
                }
            )
        )

        valid_statuses = (
            {
                "狀態": "流血",
                "增加層數": 1,
                "層數上限": 5,
                "效果": {"每層生效": {"最大生命傷害百分比": 1}},
            },
            {"狀態": "眩暈", "持續幀數": 30, "重複套用": "延長持續時間"},
            {"狀態": "封內", "持續幀數": 30, "重複套用": "保留較長持續時間"},
            {
                "狀態": "寒毒",
                "持續幀數": 90,
                "重複套用": "刷新持續時間",
                "效果": {
                    "持續生效": {"禁止受到治療": True, "速度降低百分比": 25}
                },
            },
            {
                "狀態": "枯骨",
                "持續幀數": 90,
                "重複套用": "刷新持續時間",
                "效果": {
                    "持續生效": {
                        "受到傷害增加百分比": 25,
                        "受到治療減少百分比": 75,
                    }
                },
            },
            {"狀態": "七星", "持續幀數": 150, "設定印記層數": 7},
            {
                "狀態": "化勁",
                "可觸發次數": 1,
                "效果": {
                    "每次觸發": {"阻止本次施放": True, "原攻擊目標獲得護盾": 100}
                },
            },
            {
                "狀態": "刺目",
                "可觸發次數": 1,
                "效果": {"每次觸發": {"阻止本次施放": True}},
            },
            {
                "狀態": "下一次受到攻擊必定落空",
                "持續幀數": 30,
                "可觸發次數": 1,
                "效果": {"每次觸發": {"使本次受到攻擊落空": True}},
            },
            {
                "狀態": "傷害抵擋",
                "增加可抵擋次數": 1,
                "可抵擋次數上限": 3,
                "效果": {"每次觸發": {"抵擋非處決正傷害": True}},
            },
            {
                "狀態": "下次承傷上限",
                "可觸發次數": 1,
                "效果": {"每次觸發": {"傷害上限": 100}},
            },
            {
                "狀態": "戰意",
                "增加層數": 1,
                "層數上限": 10,
                "效果": {
                    "每層生效": {"招式傷害增加百分比": 5, "傷害減免百分比": 1}
                },
            },
            {
                "狀態": "真氣",
                "增加層數": 1,
                "層數上限": 10,
                "效果": {"每層生效": {"命中附加純粹傷害": 9}},
            },
            {
                "狀態": "毒爆",
                "增加層數": 1,
                "層數上限": 5,
                "效果": {"每層提供數值": {"死亡爆炸純粹傷害": 60}},
            },
            {"狀態": "無影", "持續幀數": 30, "重複套用": "刷新持續時間"},
        )
        for status in valid_statuses:
            with self.subTest(status=status):
                self.assertFalse(errors(status))

        invalid_statuses = (
            {"狀態": "中毒"},
            {"狀態": "下一次攻擊必定暴擊", "可觸發次數": 1},
            {"狀態": "眩暈"},
            {
                "狀態": "眩暈",
                "持續幀數": 30,
                "重複套用": "保留較長持續時間",
                "增加層數": 1,
                "層數上限": 10,
                "效果": {
                    "每層生效": {
                        "招式傷害增加百分比": 5,
                        "傷害減免百分比": 1,
                    }
                },
            },
            {
                "狀態": "戰意",
                "增加層數": 1,
                "層數上限": 10,
                "持續幀數": 30,
                "效果": {
                    "每層生效": {
                        "招式傷害增加百分比": 5,
                        "傷害減免百分比": 1,
                    }
                },
            },
            {
                "狀態": "戰意",
                "增加層數": 1,
                "層數上限": 10,
                "重複套用": "刷新持續時間",
                "效果": {
                    "每層生效": {
                        "招式傷害增加百分比": 5,
                        "傷害減免百分比": 1,
                    }
                },
            },
            {
                "狀態": "戰意",
                "增加層數": 1,
                "層數上限": 10,
                "效果": {
                    "持續生效": {
                        "招式傷害增加百分比": 5,
                        "傷害減免百分比": 1,
                    }
                },
            },
            {
                "狀態": "戰意",
                "增加層數": 1,
                "層數上限": 10,
                "效果": {
                    "每層生效": {
                        "速度降低百分比": 5,
                    }
                },
            },
            {
                "狀態": "七星",
                "增加層數": 7,
                "層數上限": 7,
                "持續幀數": 150,
            },
            {
                "狀態": "傷害抵擋",
                "增加可抵擋次數": 1,
                "效果": {"每次觸發": {"抵擋非處決正傷害": True}},
            },
            {
                "狀態": "傷害抵擋",
                "設定可抵擋次數": 1,
                "增加可抵擋次數": 1,
                "可抵擋次數上限": 3,
                "效果": {"每次觸發": {"抵擋非處決正傷害": True}},
            },
            {
                "狀態": "傷害抵擋",
                "設定可抵擋次數": 1,
                "效果": {"每次觸發": {"抵擋非處決正傷害": False}},
            },
        )
        for status in invalid_statuses:
            with self.subTest(status=status):
                self.assertTrue(errors(status))

    def test_status_numeric_reference_schema_matches_the_status_catalog(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        def quantity_rule(status: str) -> dict:
            return {
                "時機": "單位死亡",
                "重複次數": {"來源狀態數量": status, "最小": 1},
                "造成傷害": {
                    "數值": 1,
                    "傷害種類": "純粹",
                    "範圍": "單體",
                },
            }

        self.assertFalse(errors(quantity_rule("毒爆")))
        self.assertTrue(errors(quantity_rule("眩暈")))
        self.assertTrue(errors(quantity_rule("下一次攻擊必定暴擊")))

        def value_rule(status: str, name: str) -> dict:
            return {
                "時機": "單位死亡",
                "造成傷害": {
                    "數值": {
                        "來源狀態效果值": {"狀態": status, "名稱": name}
                    },
                    "傷害種類": "純粹",
                    "範圍": "單體",
                },
            }

        self.assertFalse(errors(value_rule("毒爆", "死亡爆炸純粹傷害")))
        self.assertTrue(errors(value_rule("眩暈", "死亡爆炸純粹傷害")))
        self.assertTrue(errors(value_rule("真氣", "死亡爆炸純粹傷害")))

    def test_structural_keys_use_traditional_chinese_without_aliases(self) -> None:
        combos = yaml.safe_load(
            (ROOT / "config" / "chess_combos.yaml").read_text(encoding="utf-8")
        )
        equipment = yaml.safe_load(
            (ROOT / "config" / "chess_equipment.yaml").read_text(encoding="utf-8")
        )
        neigong = yaml.safe_load(
            (ROOT / "config" / "chess_neigong.yaml").read_text(encoding="utf-8")
        )
        self.assertGreater(len(combos["羈絆"]), 0)
        self.assertGreater(len(equipment["裝備列表"]), 0)
        self.assertGreater(len(neigong["層級分配"]), 0)

        invalid_documents = {
            "combos": {
                "羁绊": [
                    {
                        "名称": "舊式",
                        "成员": [1],
                        "阈值": [{"人数": 1, "名称": "舊式", "效果": []}],
                    }
                ]
            },
            "equipment": {
                "装备列表": [{"装备ID": 1, "层级": 1, "装备类型": 0}]
            },
            "neigong": {
                "选择数量": 1,
                "层级分配": [{"层级": 1, "武功": [1]}],
                "效果": {"1": []},
            },
        }
        for name, document in invalid_documents.items():
            with self.subTest(name=name):
                validator = jsonschema.Draft202012Validator(self.schema(name))
                self.assertTrue(list(validator.iter_errors(document)))

        verifier = subprocess.run(
            ["python", str(ROOT / "tools" / "verify_easy_pool.py")],
            cwd=ROOT,
            text=True,
            encoding="utf-8",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(verifier.returncode, 0, verifier.stderr)
        self.assertIn(f"Total synergies: {len(combos['羈絆'])}", verifier.stdout)


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

    def test_validate_content_reports_invalid_effect_content(self) -> None:
        with tempfile.TemporaryDirectory(prefix="kys-invalid-config-") as temp:
            config_root = Path(temp) / "config"
            shutil.copytree(ROOT / "config", config_root)
            effects_path = config_root / "chess_magic_effects.yaml"
            content = effects_path.read_text(encoding="utf-8")
            content = content.replace("時機: 絕招施放", "時機: 不存在", 1)
            effects_path.write_text(content, encoding="utf-8")

            result = self.run_cli(
                "validate-content",
                "--difficulty",
                "normal",
                *self.content_arguments(config_root),
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("內容驗證失敗：normal", result.stderr)
            self.assertIn("未知時機", result.stderr)

    def test_validate_content_enforces_named_action_event_constraints(self) -> None:
        with tempfile.TemporaryDirectory(prefix="kys-invalid-action-event-") as temp:
            config_root = Path(temp) / "config"
            shutil.copytree(ROOT / "config", config_root)
            effects_path = config_root / "chess_magic_effects.yaml"
            content = effects_path.read_text(encoding="utf-8")
            invalid_rule = (
                "    效果:\n"
                "      - 時機: 開場\n"
                "        記錄最大招式生命傷害:\n"
                "          狀態槽: 最大招式生命傷害\n"
            )
            content = content.replace("    效果:\n", invalid_rule, 1)
            effects_path.write_text(content, encoding="utf-8")

            result = self.run_cli(
                "validate-content",
                "--difficulty",
                "normal",
                *self.content_arguments(config_root),
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("內容驗證失敗：normal", result.stderr)
            self.assertIn("只允許用於傷害結算事件", result.stderr)


if __name__ == "__main__":
    unittest.main()
