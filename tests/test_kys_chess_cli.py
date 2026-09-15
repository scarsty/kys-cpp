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


def run_mcp(requests, cwd=None, extra_args=None):
    payload = "".join(json.dumps(request, ensure_ascii=False) + "\n" for request in requests)
    return subprocess.run(
        [str(CLI), "--mcp", *(extra_args or [])],
        input=payload,
        text=True,
        encoding="utf-8",
        capture_output=True,
        cwd=cwd,
        check=False,
    )


def run_cli(args, input_text=""):
    return subprocess.run(
        [str(CLI), *args],
        input=input_text,
        text=True,
        encoding="utf-8",
        capture_output=True,
        check=False,
    )


class ChessCliTests(unittest.TestCase):
    def test_help_exits_successfully(self):
        completed = run_cli(["--help"])

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(completed.stderr, "")

    def test_unknown_command_and_option_are_rejected(self):
        unknown_command = run_cli(["frobnicate"])
        unknown_option = run_cli(["--trace"])

        self.assertEqual(unknown_command.returncode, 2)
        self.assertEqual(unknown_option.returncode, 2)

    def test_invalid_startup_values_are_rejected(self):
        difficulty = run_cli(["new", "--difficulty", "impossible"])
        seed = run_cli(["new", "--seed", "not-a-seed"])

        self.assertEqual(difficulty.returncode, 2)
        self.assertEqual(seed.returncode, 2)

    def test_interactive_json_is_rejected_in_favor_of_jsonl(self):
        completed = run_cli(["--json"], "quit\n")

        self.assertEqual(completed.returncode, 2)
        self.assertEqual(completed.stdout, "")

    def test_one_shot_json_is_one_complete_line(self):
        completed = run_cli(
            [
                "new",
                "--json",
                "--data-root",
                str(ROOT / "work" / "game-dev"),
                "--config-root",
                str(ROOT / "config"),
            ]
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(len(completed.stdout.splitlines()), 1)
        self.assertTrue(json.loads(completed.stdout)["ok"])
        self.assertTrue(completed.stdout.endswith("\n"))

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

    def test_standalone_stdio_mcp_initializes_lists_tools_and_dispatches(self):
        completed = run_mcp(
            [
                {
                    "jsonrpc": "2.0",
                    "id": 1,
                    "method": "initialize",
                    "params": {
                        "protocolVersion": "2025-11-25",
                        "capabilities": {},
                        "clientInfo": {"name": "test", "version": "1"},
                    },
                },
                {
                    "jsonrpc": "2.0",
                    "method": "notifications/initialized",
                    "params": {},
                },
                {"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}},
                {
                    "jsonrpc": "2.0",
                    "id": 3,
                    "method": "tools/call",
                    "params": {
                        "name": "new_game",
                        "arguments": {
                            "difficulty": "normal",
                            "seed": "0x0000000000000042",
                        },
                    },
                },
            ]
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual(len(responses), 3)
        self.assertEqual(responses[0]["result"]["protocolVersion"], "2025-11-25")
        names = {tool["name"] for tool in responses[1]["result"]["tools"]}
        self.assertIn("export_save_file", names)
        self.assertNotIn("export_save", names)
        self.assertTrue(responses[2]["result"]["structuredContent"]["ok"])

    def test_standalone_stdio_mcp_interoperates_with_the_official_client(self):
        import anyio
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client

        async def verify():
            parameters = StdioServerParameters(command=str(CLI), args=["--mcp"])
            async with stdio_client(parameters) as (read_stream, write_stream):
                async with ClientSession(read_stream, write_stream) as session:
                    initialized = await session.initialize()
                    tools = await session.list_tools()
                    created = await session.call_tool(
                        "new_game",
                        {"difficulty": "normal", "seed": "0x0000000000000042"},
                    )
                    self.assertEqual(initialized.protocolVersion, "2025-11-25")
                    self.assertIn("export_save_file", {tool.name for tool in tools.tools})
                    self.assertFalse(created.isError)
                    self.assertTrue(created.structuredContent["ok"])

        anyio.run(verify)

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

    def test_native_durable_autosave_can_resume_in_a_new_process(self):
        with tempfile.TemporaryDirectory() as save_dir:
            autosave_file = Path(save_dir) / "autosave.json"
            first = run_jsonl(
                [
                    {
                        "id": 1,
                        "method": "new",
                        "params": {
                            "difficulty": "normal",
                            "seed": "0x0000000000000047",
                            "detail": "compact",
                        },
                    },
                    {
                        "id": 2,
                        "method": "act",
                        "params": {
                            "action": {"type": "refresh_shop"},
                            "detail": "summary",
                        },
                    },
                    {"id": 3, "method": "observe", "params": {"detail": "compact"}},
                ],
                extra_args=["--autosave-file", str(autosave_file)],
            )
            first_responses = [json.loads(line) for line in first.stdout.splitlines()]
            expected = first_responses[-1]["result"]["game_state"]
            self.assertTrue(first_responses[1]["result"]["accepted"])
            self.assertTrue(autosave_file.is_file())

            second = run_jsonl(
                [
                    {"id": 4, "method": "list_saves", "params": {}},
                    {
                        "id": 5,
                        "method": "resume_game",
                        "params": {"detail": "compact"},
                    },
                ],
                extra_args=["--autosave-file", str(autosave_file)],
            )
            second_responses = [json.loads(line) for line in second.stdout.splitlines()]
            autosave = next(
                slot for slot in second_responses[0]["result"] if slot["slot"] == "autosave"
            )
            for redundant_field in (
                "compatibility",
                "difficulty",
                "game_version",
                "occupied",
            ):
                self.assertNotIn(redundant_field, autosave)
            restored = second_responses[1]["result"]["game_state"]
            self.assertEqual(restored["state_hash"], expected["state_hash"])
            self.assertEqual(restored["money"], expected["money"])

    def test_headless_battle_keeps_jsonl_stdout_protocol_clean(self):
        completed = run_jsonl(
            [
                {
                    "id": 1,
                    "method": "new",
                    "params": {
                        "difficulty": "normal",
                        "seed": "0x00000000000000b3",
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
                {"id": 8, "method": "inspect_last_battle", "params": {"detail": "full"}},
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
        self.assertTrue(battle["summary"])
        self.assertIn(
            battle["outcome"],
            ("player_victory", "player_defeat", "timeout"),
        )
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
                {
                    "id": 2,
                    "method": "legal_actions",
                    "params": {"action_type": "set_deployment"},
                },
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
                {
                    "id": 7,
                    "method": "inspect_combo",
                    "params": {"combo_name": "刀客", "detail": "full"},
                },
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        created, legal, invalid, locked, compact, equipment, combo = [
            json.loads(line) for line in completed.stdout.splitlines()
        ]
        game = created["result"]["game_state"]
        self.assertTrue(game["projected_victory_income_excludes_conditional_bonuses"])
        self.assertTrue(game["relevant_roles"])
        role = game["relevant_roles"][0]
        self.assertTrue(role["name"])
        self.assertIn("base_stats", role)
        self.assertIn("abilities", role)
        self.assertIn("combos", role)
        self.assertIn("geometry", role["abilities"][0])
        def assert_structured_description(description):
            self.assertEqual(set(description), {"sections"})
            self.assertTrue(description["sections"])
            rows = []
            for section in description["sections"]:
                self.assertIn("blocks", section)
                for block in section["blocks"]:
                    self.assertIn("rows", block)
                    rows.extend(block["rows"])
            self.assertTrue(rows)
            for row in rows:
                self.assertEqual(
                    set(row),
                    {"kind", "text", "indent", "break_before"},
                )
                self.assertIn(
                    row["kind"],
                    {"field", "list_item", "heading", "prose", "summary"},
                )
                self.assertIsInstance(row["indent"], int)
                self.assertIn(
                    row["break_before"],
                    {"none", "block", "branch", "sequence", "action_group", "qualifier"},
                )
                self.assertIsInstance(row["text"], str)
                self.assertTrue(row["text"].strip())

        assert_structured_description(role["abilities"][0]["effects"])
        self.assertNotIn("effect_note", role["abilities"][0])
        self.assertIn("power_by_star", role["abilities"][0])
        deployment = legal["result"]
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
        next_game = locked["result"]["next_observation"]
        self.assertNotIn("role_metadata_scope", next_game)
        self.assertNotIn("equipment_metadata_scope", next_game)
        self.assertNotIn("relevant_roles", next_game)
        self.assertEqual(compact["result"]["game_state"]["detail"], "compact")
        self.assertNotIn("relevant_roles", compact["result"]["game_state"])
        equipment_info = equipment["result"]
        self.assertNotIn("base_stat_effects", equipment_info)
        self.assertIn("special_effects", equipment_info)
        assert_structured_description(equipment_info["special_effects"])
        self.assertIn("character_bonuses", equipment_info)
        for bonus in equipment_info["character_bonuses"]:
            if "effects" in bonus:
                assert_structured_description(bonus["effects"])
        self.assertTrue(combo["result"]["thresholds"])
        for threshold in combo["result"]["thresholds"]:
            assert_structured_description(threshold["effects"])


    def test_battle_report_counters_match_emitted_effect_events(self):
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
                {"id": 7, "method": "inspect_last_battle", "params": {"detail": "full"}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertIn("battle", responses[-2]["result"])
        self.assertNotIn("unit_stats", responses[-2]["result"]["battle"])

        full = responses[-1]["result"]
        # 正式配置只驗證報告契約；中毒與奪內的行為案例由獨立 C++ fixture 驗證。
        for unit in full["unit_stats"]:
            events = [event for event in full["effect_activations"]
                      if event["source_unit_id"] == unit["unit_id"]]
            self.assertEqual(unit["magic_points_drained"], sum(
                event["value"] for event in events if event["type"] == "magic_points_drained"))
            for counter, event_type in (
                ("magic_points_drain_events", "magic_points_drained"),
                ("poison_payload_events", "poison_payload"),
                ("poison_application_events", "poison_applied"),
            ):
                self.assertEqual(unit[counter], sum(event["type"] == event_type for event in events))


if __name__ == "__main__":
    unittest.main()
