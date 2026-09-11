from __future__ import annotations

import copy
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

    def test_sanqing_cannot_replenish_quanzhen_shields(self) -> None:
        config = yaml.safe_load(
            (ROOT / "config" / "chess_magic_effects.yaml").read_text(encoding="utf-8")
        )
        sanqing = next(magic for magic in config["絕招"] if magic["武功"] == 133)
        shield_actions = {"獲得護盾", "消耗記錄為護盾", "原攻擊目標獲得護盾"}

        def visit(value: object) -> None:
            if isinstance(value, dict):
                self.assertFalse(shield_actions.intersection(value))
                self.assertNotEqual(value.get("資源"), "護盾")
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)

        visit(sanqing)

    def test_shipped_clone_tiers_remain_the_reviewed_one_two_three_sites(self) -> None:
        config = yaml.safe_load(
            (ROOT / "config" / "chess_combos.yaml").read_text(encoding="utf-8")
        )

        clone_counts: list[int] = []

        def visit(value: object) -> None:
            if isinstance(value, dict):
                clone = value.get("生成分身")
                if isinstance(clone, dict):
                    clone_counts.append(clone["數量"])
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)

        visit(config)
        self.assertEqual(clone_counts, [1, 2, 3])

    def test_seven_star_schema_exposes_explicit_holder_group_replacement(self) -> None:
        config = yaml.safe_load(
            (ROOT / "config" / "chess_magic_effects.yaml").read_text(
                encoding="utf-8"
            )
        )
        document = copy.deepcopy(config)
        applications: list[dict] = []

        def visit(value: object) -> None:
            if isinstance(value, dict):
                application = value.get("套用狀態")
                if isinstance(application, dict) and application.get("狀態") == "七星":
                    applications.append(application)
                for child in value.values():
                    visit(child)
            elif isinstance(value, list):
                for child in value:
                    visit(child)

        visit(document)
        self.assertEqual(len(applications), 1)
        applications[0]["重複套用"] = "取代整組"
        with self.assertRaises(jsonschema.ValidationError):
            jsonschema.Draft202012Validator(
                self.schema("magic_effects")
            ).validate(document)

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

        def status_behavior_errors(behavior_rule: dict) -> list[jsonschema.ValidationError]:
            return errors(
                {
                    "時機": "攻擊提交",
                    "目標": "自身",
                    "套用狀態": {
                        "狀態": "真氣",
                        "增加層數": 1,
                        "層數上限": 10,
                        "效果": [behavior_rule],
                    },
                }
            )

        self.assertFalse(
            status_behavior_errors(
                {
                    "時機": "命中",
                    "目標": "狀態持有者",
                    "使本次受到攻擊落空": {},
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "治療套用",
                    "目標": "狀態持有者",
                    "使本次受到攻擊落空": {},
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "命中",
                    "目標": "狀態持有者",
                    "強制移動": {
                        "方向": "遠離來源",
                        "距離像素": 40,
                        "碰撞": "阻擋前停止",
                        "受阻結果": "縮短",
                    },
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "攻擊生成",
                    "目標": "狀態持有者",
                    "修改攻擊": {
                        "執行行為": {
                            "類型": "範圍追蹤",
                            "範圍像素": 120,
                            "傷害倍率": 100,
                        }
                    },
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "施放規劃",
                    "目標": "狀態持有者",
                    "修改施放": {"射程模式": "遠程"},
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "造成傷害後",
                    "目標": "狀態持有者",
                    "條件分支": {
                        "條件": ["目標非無敵"],
                        "成立": [{"使本次施放攻擊落空": {}}],
                    },
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "命中",
                    "目標": "狀態持有者",
                    "動作": [
                        {"使本次施放攻擊落空": {}},
                        {"消耗此狀態": {"消耗數量": 1}},
                    ],
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "命中",
                    "目標": "狀態持有者",
                    "條件分支": {
                        "條件": ["目標非無敵"],
                        "成立": [{"使本次受到攻擊落空": {}}],
                    },
                }
            )
        )
        self.assertFalse(
            status_behavior_errors(
                {
                    "時機": "持續",
                    "目標": "狀態持有者",
                    "抵擋非處決正傷害": {},
                }
            )
        )
        self.assertTrue(
            status_behavior_errors(
                {
                    "時機": "命中",
                    "目標": "狀態持有者",
                    "抵擋非處決正傷害": {},
                }
            )
        )

    def test_poison_action_uses_generic_status_behavior_rules(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        behavior = [
            {
                "時機": "每隔",
                "間隔幀數": 30,
                "目標": "狀態持有者",
                "動作": [
                    {
                        "造成傷害": {
                            "數值": {"目標目前生命百分比": 7, "最小": 1},
                            "傷害種類": "中毒",
                        }
                    },
                    {"消耗此狀態": {"消耗數量": 1}},
                ],
            }
        ]
        self.assertFalse(errors({
            "時機": "命中",
            "施加中毒": {
                "可觸發次數": 3,
                "持續幀數": 90,
                "重複套用": "保留較高傷害",
                "同事件合併": "合計傷害百分比",
                "效果": behavior,
            },
        }))
        aggregate_poison = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "保留較高傷害",
            "同事件合併": "合計傷害百分比",
            "效果": behavior,
        }
        self.assertTrue(errors({
            "時機": "主彈命中",
            "施加中毒": aggregate_poison,
        }))
        self.assertTrue(errors({
            "時機": "命中",
            "目標": "自身",
            "施加中毒": aggregate_poison,
        }))
        for field, value in (
            ("條件", ["已接受命中"]),
            ("機率", 50),
            ("次數", 1),
            ("同來源冷卻幀數", 30),
            ("間隔幀數", 30),
            ("每N次事件", 2),
            ("觸發限制", {"範圍": "每次施放每個目標", "次數": 1}),
            ("重複次數", 2),
        ):
            invalid = {
                "時機": "命中",
                "施加中毒": aggregate_poison,
                field: value,
            }
            self.assertTrue(errors(invalid), field)
        self.assertTrue(errors({
            "時機": "命中",
            "施加中毒": aggregate_poison,
            "獲得護盾": 1,
        }))
        self.assertTrue(errors({
            "時機": "命中",
            "動作": [{"施加中毒": aggregate_poison}],
        }))
        self.assertTrue(errors({
            "時機": "命中",
            "條件分支": {
                "條件": ["已接受命中"],
                "成立": [{"施加中毒": aggregate_poison}],
            },
        }))
        nested_merge_behavior = behavior + [{
            "時機": "命中",
            "目標": "命中目標",
            "施加中毒": aggregate_poison,
        }]
        self.assertTrue(errors({
            "時機": "絕招施放",
            "施加中毒": {
                "可觸發次數": 3,
                "持續幀數": 90,
                "重複套用": "取代現有中毒",
                "效果": nested_merge_behavior,
            },
        }))
        self.assertFalse(errors({
            "時機": "絕招施放",
            "施加中毒": {
                "可觸發次數": 5,
                "持續幀數": 150,
                "重複套用": "取代現有中毒",
                "效果": behavior,
            },
        }))
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
            "效果": behavior,
        }
        self.assertTrue(errors({"時機": "命中", "施加中毒": keep_higher}))
        replace_and_merge = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "取代現有中毒",
            "同事件合併": "合計傷害百分比",
            "效果": behavior,
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
            "效果": behavior,
        }
        self.assertTrue(
            errors({"時機": "命中", "施加中毒": removed_same_event_potency})
        )
        wrong_poison_scope = {
            "可觸發次數": 3,
            "持續幀數": 90,
            "重複套用": "取代現有中毒",
            "效果": [
                {
                    "時機": "開場",
                    "觀察範圍": "效果擁有者",
                    "獲得護盾": 1,
                }
            ],
        }
        self.assertTrue(
            errors({"時機": "命中", "施加中毒": wrong_poison_scope})
        )

    def test_status_behavior_schema_matches_parser_contexts(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        cold_poison = {
            "狀態": "寒毒",
            "持續幀數": 90,
        }
        self.assertFalse(errors({"時機": "命中", "套用狀態": cold_poison}))
        cold_poison_with_authored_behavior = dict(cold_poison)
        cold_poison_with_authored_behavior["效果"] = [
            {"時機": "持續", "目標": "狀態持有者", "獲得護盾": 1}
        ]
        self.assertTrue(errors({
            "時機": "命中",
            "套用狀態": cold_poison_with_authored_behavior,
        }))

        shadowless = {
            "狀態": "無影",
            "持續幀數": 90,
            "重複套用": "刷新持續時間",
        }
        self.assertTrue(errors({"時機": "命中", "套用狀態": shadowless}))
        shadowless["效果"] = [
            {
                "時機": "命中",
                "目標": "狀態持有者",
                "消耗此狀態": {"消耗數量": 1},
            }
        ]
        self.assertFalse(errors({"時機": "命中", "套用狀態": shadowless}))

        stun_with_behavior = {
            "狀態": "眩暈",
            "持續幀數": 30,
            "重複套用": "延長持續時間",
            "效果": [
                {"時機": "命中", "目標": "狀態持有者", "獲得護盾": 1}
            ],
        }
        self.assertTrue(errors({"時機": "命中", "套用狀態": stun_with_behavior}))

        def persistent_errors(rule: dict) -> list[jsonschema.ValidationError]:
            status = dict(shadowless)
            status["效果"] = [rule]
            return errors({"時機": "命中", "套用狀態": status})

        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "獲得護盾": 1,
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "機率": 1,
            "治療交易修正": {
                "方式": "阻止",
                "治療種類": ["直接"],
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "條件": ["目標非無敵"],
            "治療交易修正": {
                "方式": "阻止",
                "治療種類": ["直接"],
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "自身",
            "治療交易修正": {
                "方式": "阻止",
                "治療種類": ["直接"],
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "屬性修正": {
                "屬性": "速度",
                "方式": "百分比加算",
                "數值": {"目標目前生命百分比": 10},
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "屬性修正": {
                "屬性": "攻擊",
                "方式": "百分比加算",
                "數值": 10,
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "屬性修正": {
                "屬性": "速度",
                "方式": "百分比加算",
                "數值": {"基準": "目標最大生命", "百分比": 1},
            },
        }))
        self.assertTrue(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "屬性修正": {
                "屬性": "速度",
                "方式": "百分比加算",
                "數值": {
                    "基準": "套用目標最大生命",
                    "乘數基準": "目標目前生命",
                    "百分比": 1,
                },
            },
        }))
        self.assertFalse(persistent_errors({
            "時機": "持續",
            "目標": "狀態持有者",
            "屬性修正": {
                "屬性": "速度",
                "方式": "百分比加算",
                "數值": {
                    "基準": "套用目標最大生命",
                    "百分比": 1,
                },
            },
        }))

        self.assertTrue(errors({"時機": "持續", "獲得護盾": 1}))
        self.assertTrue(errors({
            "時機": "命中", "目標": "狀態持有者", "獲得護盾": 1,
        }))
        self.assertTrue(errors({
            "時機": "命中", "觀察範圍": "狀態持有者事件來源", "獲得護盾": 1,
        }))
        self.assertTrue(errors({"時機": "命中", "消耗此狀態": {"消耗數量": 1}}))

        invalid_nested = dict(cold_poison)
        invalid_nested["效果"] = [
            {
                "時機": "持續",
                "觀察範圍": "效果擁有者",
                "目標": "狀態持有者",
                "獲得護盾": 1,
            }
        ]
        self.assertTrue(errors({"時機": "命中", "套用狀態": invalid_nested}))

        missing_quantity_limit = {
            "狀態": "戰意",
            "增加層數": 1,
            "效果": [{"時機": "持續", "目標": "狀態持有者", "獲得護盾": 1}],
        }
        self.assertTrue(errors({"時機": "命中", "套用狀態": missing_quantity_limit}))

    def test_scalable_numbers_are_closed_by_authoring_and_status_quantity_context(self) -> None:
        validator = jsonschema.Draft202012Validator(self.schema("magic_effects"))

        def errors(rule: dict) -> list[jsonschema.ValidationError]:
            document = {"絕招": [{"武功": 1, "名稱": "測試", "效果": [rule]}]}
            return list(validator.iter_errors(document))

        def pure_damage(value_fields: dict) -> dict:
            return {
                "時機": "命中",
                "目標": "命中目標",
                "造成傷害": {**value_fields, "傷害種類": "純粹"},
            }

        self.assertTrue(errors(pure_damage({})))
        self.assertTrue(errors(pure_damage({"每層數值": 9})))
        self.assertTrue(errors(pure_damage({"數值": 9, "每層數值": 10})))
        self.assertTrue(errors(pure_damage({
            "數值": {"基準": "此狀態貢獻數量", "百分比": 100}
        })))
        self.assertTrue(errors(pure_damage({
            "數值": {"基準": "套用目標最大生命", "百分比": 3}
        })))

        layered = {
            "狀態": "真氣",
            "增加層數": 1,
            "層數上限": 10,
            "效果": [{
                "時機": "命中",
                "目標": "命中目標",
                "造成傷害": {"每層數值": 9, "傷害種類": "純粹"},
            }],
        }
        self.assertFalse(errors({"時機": "命中", "套用狀態": layered}))

        charges = {
            "狀態": "刺目",
            "可觸發次數": 1,
            "效果": [{
                "時機": "命中",
                "目標": "狀態持有者",
                "使本次施放攻擊落空": {},
            }, {
                "時機": "單位死亡",
                "目標": "狀態持有者",
                "造成傷害": {"每層數值": 9, "傷害種類": "純粹"},
            }],
        }
        self.assertTrue(errors({"時機": "命中", "套用狀態": charges}))

    def test_status_quantity_reference_schema_matches_the_status_catalog(self) -> None:
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
                },
            }

        self.assertFalse(errors(quantity_rule("毒爆")))
        self.assertTrue(errors(quantity_rule("眩暈")))
        self.assertTrue(errors(quantity_rule("下一次攻擊必定暴擊")))

        for source in ("不限", "效果擁有者", "效果綁定"):
            rule = quantity_rule("毒爆")
            rule["重複次數"]["狀態來源"] = source
            self.assertFalse(errors(rule), source)
        current_contribution = quantity_rule("毒爆")
        current_contribution["重複次數"]["狀態來源"] = "此狀態貢獻"
        self.assertTrue(errors(current_contribution))

        self.assertTrue(errors({
            "時機": "命中",
            "目標": "命中目標",
            "消耗狀態": {
                "狀態": "毒爆",
                "狀態來源": "此狀態貢獻",
            },
        }))
        self.assertTrue(errors({
            "時機": "命中",
            "目標": "命中目標",
            "移除狀態": {
                "狀態": ["毒爆"],
                "狀態來源": "此狀態貢獻",
            },
        }))
        self.assertFalse(errors({
            "時機": "命中",
            "目標": "命中目標",
            "移除狀態": {
                "僅負面": True,
                "狀態來源": "效果擁有者",
                "數量": 1,
                "順序": "最舊",
            },
        }))

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
