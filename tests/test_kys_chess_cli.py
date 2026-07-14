import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
CLI = Path(os.environ.get("KYS_CHESS_CLI", ROOT / "x64" / "Debug" / "kys_chess_cli.exe"))


def run_jsonl(requests, cwd=None, extra_args=None):
    payload = "".join(json.dumps(request, ensure_ascii=False) + "\n" for request in requests)
    return subprocess.run(
        [str(CLI), "--jsonl", *(extra_args or [])],
        input=payload,
        text=True,
        encoding="utf-8",
        capture_output=True,
        cwd=cwd,
        check=False,
    )


class ChessCliTests(unittest.TestCase):
    def test_protocol_stdout_is_clean_and_ids_are_preserved(self):
        completed = run_jsonl(
            [
                {
                    "id": "caller-1",
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000003039",
                    },
                },
                {"id": 42, "method": "observe", "params": {}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual([response["id"] for response in responses], ["caller-1", 42])
        self.assertTrue(all(response["ok"] for response in responses))
        self.assertNotIn("載入成功", completed.stdout)
        self.assertIn("載入成功", completed.stderr)

    def test_default_data_root_is_executable_relative(self):
        with tempfile.TemporaryDirectory() as directory:
            default_root = run_jsonl(
                [
                    {
                        "id": 1,
                        "method": "new",
                        "params": {
                            "difficulty": "normal",
                            "seed": "0x0000000000000001",
                        },
                    }
                ],
                cwd=directory,
            )
            explicit_root = run_jsonl(
                [
                    {
                        "id": 1,
                        "method": "new",
                        "params": {
                            "difficulty": "normal",
                            "seed": "0x0000000000000001",
                        },
                    }
                ],
                cwd=directory,
                extra_args=[
                    "--data-root",
                    str(ROOT / "work" / "game-dev"),
                    "--config-root",
                    str(ROOT / "config"),
                ],
            )
        self.assertEqual(default_root.returncode, 0, default_root.stderr)
        self.assertEqual(explicit_root.returncode, 0, explicit_root.stderr)
        default_response = json.loads(default_root.stdout)
        explicit_response = json.loads(explicit_root.stdout)
        self.assertTrue(default_response["ok"])
        self.assertEqual(
            default_response["result"]["game_state"]["state_hash"],
            explicit_response["result"]["game_state"]["state_hash"],
        )

    def test_malformed_config_diagnostics_stay_off_stdout(self):
        with tempfile.TemporaryDirectory() as directory:
            completed = run_jsonl(
                [
                    {
                        "id": "bad-config",
                        "method": "new",
                        "params": {"difficulty": "normal", "seed": "0x0000000000000001"},
                    }
                ],
                extra_args=["--config-root", directory],
            )
        response = json.loads(completed.stdout)
        self.assertEqual(response["id"], "bad-config")
        self.assertFalse(response["ok"])
        self.assertNotEqual(completed.stderr, "")

    def test_save_load_reports_discarded_suffix_and_exits_cleanly(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000000002",
                    },
                },
                {
                    "id": 2,
                    "method": "act",
                    "params": {"action": {"type": "set_shop_locked", "locked": True}},
                },
                {"id": 3, "method": "save_game", "params": {"slot": "1", "label": "鎖定"}},
                {
                    "id": 4,
                    "method": "act",
                    "params": {"action": {"type": "set_shop_locked", "locked": False}},
                },
                {"id": 5, "method": "load_game", "params": {"slot": "1"}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(responses[-1]["result"]["discarded_active_actions"], 1)
        self.assertEqual(responses[-1]["result"]["restored_sequence"], 1)

    def test_headless_battle_keeps_jsonl_stdout_protocol_clean(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000000014",
                    },
                },
                {"id": 2, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 0}}},
                {"id": 3, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 5}}},
                {
                    "id": 4,
                    "method": "act",
                    "params": {"action": {"type": "set_deployment", "chess_instance_ids": [1, 2]}},
                },
                {"id": 5, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {"id": 6, "method": "inspect_prepared_battle", "params": {}},
                {
                    "id": 7,
                    "method": "act",
                    "params": {"detail": "compact", "action": {"type": "start_battle"}},
                },
                {"id": 8, "method": "inspect_last_battle", "params": {}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual([response["id"] for response in responses], [1, 2, 3, 4, 5, 6, 7, 8])
        self.assertTrue(all(response["ok"] for response in responses))
        self.assertIn("board", responses[-3]["result"])
        self.assertEqual(responses[-3]["result"]["units"][0]["team"], "我方")
        compact_action = responses[-2]["result"]
        self.assertNotIn("battle", compact_action)
        self.assertEqual(compact_action["next_observation"]["detail"], "compact")
        self.assertNotIn("relevant_roles", compact_action["next_observation"])
        self.assertTrue(all(
            "current_stats" not in unit
            for unit in compact_action["next_observation"]["roster"]
        ))

        battle = responses[-1]["result"]
        self.assertEqual(battle["detail"], "full")
        self.assertGreaterEqual(len(battle["initial_board"]["units"]), 2)
        self.assertTrue(all(unit["name"] for unit in battle["initial_board"]["units"]))
        self.assertTrue(battle["initial_board"]["chosen_map_name"])
        self.assertIn("圖例", battle["initial_board"]["board"])
        enemy = next(
            unit for unit in battle["initial_board"]["units"] if unit["team"] == "敵方"
        )
        self.assertIn("preview_stats", enemy)
        self.assertIn("abilities", enemy)
        self.assertIn("enemy_synergies", battle["initial_board"])
        self.assertTrue(battle["unit_stats"])
        self.assertIn("effect_activations", battle)
        ability_cast = next(
            effect
            for effect in battle["effect_activations"]
            if effect["type"] == "ability_cast"
        )
        self.assertIn("ability_id", ability_cast)
        self.assertTrue(ability_cast["ability_name"])
        debuff_change = next(
            effect
            for effect in battle["effect_activations"]
            if effect["type"] == "enemy_top_debuff_changed"
        )
        self.assertEqual(debuff_change["previous_value"], 0)
        self.assertLess(debuff_change["new_value"], 0)
        self.assertEqual(debuff_change["delta"], debuff_change["new_value"])
        self.assertEqual(debuff_change["source_kind"], "combo")
        self.assertEqual(debuff_change["source_name"], "陰險")
        self.assertNotEqual(debuff_change["source_team"], debuff_change["target_team"])
        weakened_debuff = next(
            effect
            for effect in battle["effect_activations"]
            if effect["type"] == "enemy_top_debuff_changed"
            and effect.get("previous_value") == -44
            and effect.get("new_value") == -22
        )
        self.assertEqual(weakened_debuff["delta"], 22)
        projectile_cancel = next(
            effect
            for effect in battle["effect_activations"]
            if effect["type"] == "projectile_cancelled"
        )
        self.assertEqual(
            projectile_cancel["cancelled_potential_damage"],
            min(
                projectile_cancel["source_value_before"],
                projectile_cancel["opposing_value_before"],
            ),
        )
        self.assertEqual(
            projectile_cancel["source_value_after"],
            max(
                0,
                projectile_cancel["source_value_before"]
                - projectile_cancel["opposing_value_before"],
            ),
        )
        for unit in battle["unit_stats"]:
            breakdown = unit["damage_breakdown"]
            self.assertEqual(
                sum(breakdown.values()),
                unit["damage_dealt"],
            )
            self.assertIn("healing_done", unit)
            self.assertIn("initial_combat_stats", unit)
            self.assertIn("initial_stat_delta_from_special_effects", unit)
            self.assertIn("enemy_attack_debuff", unit)
            self.assertIn("projectile_potential_damage_cancelled", unit)
            self.assertIn("projectile_cancellations", unit)
            self.assertIn("hitstun_applications", unit)
            self.assertIn("stun_applications", unit)
        self.assertGreater(
            sum(unit["hitstun_applications"] for unit in battle["unit_stats"]),
            0,
        )
        self.assertEqual(
            sum(unit["stun_applications"] for unit in battle["unit_stats"]),
            0,
        )
        debuffed_enemy = next(
            unit
            for unit in battle["unit_stats"]
            if unit["team"] == "敵方" and unit["enemy_attack_debuff"] < 0
        )
        self.assertLess(debuffed_enemy["enemy_defence_debuff"], 0)
        self.assertLess(
            debuffed_enemy["initial_stat_delta_from_special_effects"]["attack"],
            0,
        )
        self.assertLess(
            debuffed_enemy["initial_stat_delta_from_special_effects"]["defence"],
            0,
        )
        self.assertTrue(battle["summary"])
        self.assertIn(
            battle["outcome_description"],
            ("我方勝利", "我方戰敗", "超過戰鬥時間上限"),
        )
        self.assertIn(
            battle["outcome"],
            ("player_victory", "player_defeat", "timeout"),
        )
        if battle["key_events"]:
            self.assertIn("[", battle["key_events"][0]["description"])
            self.assertIn("敵方", battle["key_events"][0]["description"])
        self.assertEqual(len(battle["digest"]), 64)

    def test_protocol_exposes_semantics_schemas_and_actionable_parse_errors(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000005eed",
                    },
                },
                {"id": 2, "method": "legal_actions", "params": {}},
                {
                    "id": 3,
                    "method": "act",
                    "params": {"action": {"type": "set_deployment", "ids": [1]}},
                },
                {
                    "id": 4,
                    "method": "act",
                    "params": {
                        "detail": "compact",
                        "action": {"type": "set_shop_locked", "locked": True},
                    },
                },
                {"id": 5, "method": "observe", "params": {"detail": "compact"}},
                {"id": 6, "method": "inspect_equipment", "params": {"item_id": 61}},
                {"id": 7, "method": "inspect_combo", "params": {"combo_name": "刀客"}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        created, legal, invalid, locked, compact, equipment, combo = [
            json.loads(line) for line in completed.stdout.splitlines()
        ]
        game = created["result"]["game_state"]
        self.assertEqual(game["interest_gold"], 1)
        self.assertEqual(game["next_interest_threshold"], 20)
        self.assertEqual(game["maximum_interest_gold"], 3)
        self.assertEqual(game["projected_base_victory_gold"], 5)
        self.assertEqual(game["projected_victory_income"], 6)
        self.assertTrue(game["projected_victory_income_excludes_conditional_bonuses"])
        self.assertTrue(game["relevant_roles"])
        role = game["relevant_roles"][0]
        self.assertTrue(role["name"])
        self.assertIn("base_stats", role)
        self.assertIn("abilities", role)
        self.assertIn("combos", role)
        self.assertIn("geometry", role["abilities"][0])
        self.assertTrue(role["abilities"][0]["effect_note"])
        self.assertIn("power_by_star", role["abilities"][0])
        self.assertEqual(game["combos"], [])
        deployment = next(
            action for action in legal["result"] if action["type"] == "set_deployment"
        )
        self.assertEqual(
            deployment["action_schema"]["chess_instance_ids"],
            "整數陣列",
        )
        self.assertEqual(
            deployment["example"],
            {"type": "set_deployment", "chess_instance_ids": []},
        )
        deployment_fields = {
            entry["field"]: entry for entry in deployment["candidates_by_field"]
        }
        self.assertIn("chess_instance_ids", deployment_fields)
        self.assertTrue(deployment_fields["chess_instance_ids"]["multiple"])
        self.assertFalse(invalid["ok"])
        self.assertEqual(invalid["error_code"], "invalid_action")
        self.assertIn("chess_instance_ids", invalid["error_message"])
        self.assertIn("範例", invalid["error_message"])
        next_game = locked["result"]["next_observation"]
        self.assertNotIn("role_metadata_scope", next_game)
        self.assertNotIn("equipment_metadata_scope", next_game)
        self.assertNotIn("relevant_roles", next_game)
        self.assertEqual(compact["result"]["game_state"]["detail"], "compact")
        self.assertNotIn("relevant_roles", compact["result"]["game_state"])
        equipment_info = equipment["result"]
        self.assertIn("base_stat_effects", equipment_info)
        self.assertIn("special_effects", equipment_info)
        self.assertIn("character_bonuses", equipment_info)
        self.assertEqual(combo["result"]["name"], "刀客")
        self.assertTrue(combo["result"]["thresholds"])

    def test_defeat_uses_summary_then_targeted_full_observation_for_recovery(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000005eed",
                    },
                },
                {"id": 2, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 0}}},
                {"id": 3, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 2}}},
                {
                    "id": 4,
                    "method": "act",
                    "params": {"action": {"type": "set_deployment", "chess_instance_ids": [1, 2]}},
                },
                {"id": 5, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {
                    "id": 6,
                    "method": "act",
                    "params": {"detail": "full", "action": {"type": "start_battle"}},
                },
                {"id": 7, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {"id": 8, "method": "act", "params": {"action": {"type": "start_battle"}}},
                {"id": 9, "method": "observe", "params": {"detail": "full"}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        first_victory = responses[5]
        gold = next(
            event
            for event in first_victory["result"]["events"]
            if event["type"] == "gold_awarded"
        )
        self.assertEqual(
            gold["total_gold"],
            gold["base_gold"] + gold["interest_gold"] + gold["other_gold"],
        )
        self.assertNotIn("primary_id", gold)
        self.assertNotIn("secondary_id", gold)
        self.assertNotIn("value", gold)

        response = responses[-2]
        self.assertEqual(response["result"]["battle"]["outcome"], "player_defeat")
        self.assertNotIn("next_observation", response["result"])
        game = responses[-1]["result"]["game_state"]
        self.assertEqual(game["detail"], "full")
        self.assertEqual(game["role_metadata_scope"], "complete")
        self.assertTrue(game["relevant_roles"])

    def test_poison_reports_drain_payload_application_and_ticks_separately(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000000001",
                    },
                },
                {"id": 2, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 0}}},
                {"id": 3, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 1}}},
                {
                    "id": 4,
                    "method": "act",
                    "params": {"action": {"type": "set_deployment", "chess_instance_ids": [1, 2]}},
                },
                {"id": 5, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {"id": 6, "method": "act", "params": {"action": {"type": "start_battle"}}},
                {"id": 7, "method": "inspect_last_battle", "params": {}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertIn("battle", responses[-2]["result"])
        self.assertNotIn("unit_stats", responses[-2]["result"]["battle"])

        full = responses[-1]["result"]
        poison_units = [
            unit for unit in full["unit_stats"] if unit["poison_payload_events"] > 0
        ]
        self.assertEqual(len(poison_units), 2)
        self.assertTrue(all(unit["magic_points_drained"] > 0 for unit in poison_units))
        self.assertTrue(
            all(
                unit["poison_application_events"] <= unit["poison_payload_events"]
                for unit in poison_units
            )
        )
        self.assertTrue(all(unit["poison_ticks"] > 0 for unit in poison_units))
        self.assertTrue(all(unit["poison_damage"] > 0 for unit in poison_units))

        drain = next(
            event
            for event in full["effect_activations"]
            if event["type"] == "magic_points_drained"
        )
        self.assertGreater(drain["value"], 0)
        self.assertNotEqual(drain["source_unit_id"], drain["target_unit_id"])
        self.assertTrue(
            any(
                event["type"] == "magic_points_restored"
                for event in full["effect_activations"]
            )
        )
        payload = next(
            event
            for event in full["effect_activations"]
            if event["type"] == "poison_payload"
        )
        self.assertEqual(payload["poison_percent"], 7)
        self.assertEqual(payload["scheduled_ticks"], 3)
        self.assertTrue(
            any(
                event["type"] == "poison_applied"
                for event in full["effect_activations"]
            )
        )

    def test_equipment_reward_description_separates_character_effects(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x0000000000005eed",
                    },
                },
                {"id": 2, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 0}}},
                {"id": 3, "method": "act", "params": {"action": {"type": "buy_shop_slot", "slot": 1}}},
                {
                    "id": 4,
                    "method": "act",
                    "params": {"action": {"type": "set_deployment", "chess_instance_ids": [1, 2]}},
                },
                {"id": 5, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {"id": 6, "method": "act", "params": {"action": {"type": "start_battle"}}},
                {"id": 7, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {"id": 8, "method": "act", "params": {"action": {"type": "start_battle"}}},
                {"id": 9, "method": "act", "params": {"action": {"type": "prepare_battle"}}},
                {
                    "id": 10,
                    "method": "act",
                    "params": {"detail": "compact", "action": {"type": "start_battle"}},
                },
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        response = json.loads(completed.stdout.splitlines()[-1])
        reward = response["result"]["next_observation"]["pending_reward"]
        sword = next(option for option in reward["options"] if option["label"] == "越女劍")
        self.assertIn(
            "角色加成(韓小瑩)：25%擊退120距離並鎖定7幀；18%閃避",
            sword["description"],
        )
        self.assertNotIn("7幀：18%閃避", sword["description"])


if __name__ == "__main__":
    unittest.main()
