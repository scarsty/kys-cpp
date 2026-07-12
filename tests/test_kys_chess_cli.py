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
                {"id": 6, "method": "act", "params": {"action": {"type": "start_battle"}}},
            ]
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        self.assertEqual([response["id"] for response in responses], [1, 2, 3, 4, 5, 6])
        self.assertTrue(all(response["ok"] for response in responses))
        battle = responses[-1]["result"]["battle"]
        self.assertGreaterEqual(len(battle["initial_board"]["units"]), 2)
        self.assertTrue(battle["events"])
        self.assertIn(
            battle["outcome"],
            ("player_victory", "player_defeat", "timeout"),
        )
        self.assertEqual(len(battle["digest"]), 64)


if __name__ == "__main__":
    unittest.main()
