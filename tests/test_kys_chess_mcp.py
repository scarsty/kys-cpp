import ast
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
PACKAGE_ROOT = ROOT / "tools" / "kys_chess_mcp"
sys.path.insert(0, str(PACKAGE_ROOT))

from kys_chess_mcp.server import AUTO_SAVE_SLOT, CliSession, create_server


CLI = Path(os.environ.get("KYS_CHESS_CLI", ROOT / "x64" / "Debug" / "kys_chess_cli.exe"))


class DirectJsonl:
    def __init__(self):
        self.process = subprocess.Popen(
            [str(CLI), "--jsonl"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            bufsize=1,
        )
        self.next_id = 1

    def request(self, method, params=None):
        request = {"id": self.next_id, "method": method, "params": params or {}}
        self.next_id += 1
        self.process.stdin.write(json.dumps(request, ensure_ascii=False) + "\n")
        self.process.stdin.flush()
        return json.loads(self.process.stdout.readline())

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=5)
        self.process.stdout.close()


class McpAdapterTests(unittest.TestCase):
    def test_adapter_responses_equal_direct_jsonl(self):
        direct = DirectJsonl()
        try:
            with tempfile.TemporaryDirectory() as save_dir, CliSession(
                CLI,
                save_dir=save_dir,
                autosave=False,
            ) as adapter:
                def request_pair(method, params=None):
                    adapter_response = adapter.request(method, params)
                    direct_response = direct.request(method, params)
                    self.assertEqual(adapter_response, direct_response)
                    return adapter_response

                request_pair(
                    "new",
                    {
                        "difficulty": "normal",
                        "seed": "0x0000000000000042",
                        "position_swap_enabled": False,
                    },
                )
                request_pair("observe")
                request_pair("legal_actions")
                request_pair("inspect_role", {"role_id": 10})
                request_pair("inspect_shop_slot", {"slot": 0})
                request_pair("inspect_shop")
                request_pair("get_shop_odds")
                request_pair("inspect_bans")
                request_pair(
                    "act",
                    {"action": {"type": "buy_shop_slot", "slot": 0}},
                )
                request_pair("inspect_chess_instance", {"chess_instance_id": 1})
                request_pair(
                    "act",
                    {"action": {"type": "set_shop_locked", "locked": True}},
                )
                request_pair("save_game", {"slot": "1", "label": "鎖定"})
                request_pair("list_saves")
                request_pair("inspect_save", {"slot": "1"})
                exported_save = request_pair("export_save", {"slot": "1"})
                request_pair(
                    "import_save",
                    {
                        "slot": "copy",
                        "payload": exported_save["result"]["payload"],
                    },
                )
                request_pair(
                    "act",
                    {"action": {"type": "set_shop_locked", "locked": False}},
                )
                request_pair("load_game", {"slot": "1"})
                request_pair("export_replay")
        finally:
            direct.close()

    def test_mcp_tool_surface_matches_the_protocol_contract(self):
        source = (PACKAGE_ROOT / "kys_chess_mcp" / "server.py").read_text(encoding="utf-8")
        for tool in (
            "new_game",
            "observe_game",
            "get_diagnostics",
            "list_legal_actions",
            "take_action",
            "inspect_shop_slot",
            "inspect_shop",
            "get_shop_odds",
            "inspect_chess_instance",
            "inspect_bans",
            "inspect_role",
            "inspect_combo",
            "inspect_equipment",
            "inspect_challenge",
            "inspect_prepared_battle",
            "inspect_last_battle",
            "list_saves",
            "inspect_save",
            "save_game",
            "load_game",
            "export_save",
            "import_save",
            "export_replay",
        ):
            self.assertIn(f"def {tool}(", source)
        self.assertNotIn("def verify_replay(", source)

        tree = ast.parse(source)
        docstrings = {
            node.name: ast.get_docstring(node) or ""
            for node in ast.walk(tree)
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
        }
        expected_rollback_descriptions = {
            "observe_game": ("替換時間線", "捨棄目前後綴"),
            "list_saves": ("不需先建立棋局", "判定相容性"),
            "load_game": ("重播前綴", "discarded_active_actions", "存檔目錄仍保留"),
            "import_save": ("不會靜默載入", "替換目前時間線"),
        }
        for tool, fragments in expected_rollback_descriptions.items():
            self.assertIn(tool, docstrings)
            for fragment in fragments:
                self.assertIn(fragment, docstrings[tool])

        class StubSession:
            def request(self, method, params=None):
                return {"method": method, "params": params or {}}

        tools = create_server(StubSession())._tool_manager._tools
        new_schema = tools["new_game"].parameters["properties"]
        self.assertEqual(new_schema["difficulty"]["enum"], ["easy", "normal", "hard"])
        self.assertEqual(new_schema["detail"]["enum"], ["compact", "full"])
        self.assertEqual(new_schema["seed"]["pattern"], r"^0x[0-9a-fA-F]{16}$")
        self.assertIn("0x 前綴", new_schema["seed"]["description"])
        self.assertEqual(
            tools["take_action"].parameters["properties"]["detail"]["enum"],
            ["summary", "compact", "full"],
        )
        self.assertEqual(
            tools["take_action"].parameters["properties"]["detail"]["default"],
            "summary",
        )
        self.assertEqual(
            tools["inspect_prepared_battle"].parameters["properties"]["detail"]["enum"],
            ["summary", "compact", "full"],
        )
        self.assertEqual(
            tools["inspect_prepared_battle"].parameters["properties"]["detail"]["default"],
            "summary",
        )

    def test_adapter_owns_and_closes_one_cli_process(self):
        with tempfile.TemporaryDirectory() as save_dir:
            adapter = CliSession(CLI, save_dir=save_dir)
            response = adapter.request(
                "new",
                {"difficulty": "normal", "seed": "0x0000000000000043"},
            )
            self.assertTrue(response["ok"])
            adapter.close()
            self.assertIsNotNone(adapter._process.poll())

    def test_adapter_surfaces_diagnostics_and_recovers_after_cli_exit(self):
        with tempfile.TemporaryDirectory() as save_dir, CliSession(CLI, save_dir=save_dir) as adapter:
            self.assertTrue(adapter.request(
                "new",
                {"difficulty": "normal", "seed": "0x0000000000000044"},
            )["ok"])
            adapter._diagnostics.append("native assertion: 測試崩潰")
            adapter._process.terminate()
            adapter._process.wait(timeout=5)

            failure = adapter.request("observe")

            self.assertFalse(failure["ok"])
            self.assertEqual(failure["error_code"], "cli_process_exited")
            self.assertTrue(failure["restarted"])
            self.assertTrue(failure["session_lost"])
            self.assertIn("native assertion: 測試崩潰", failure["error_message"])
            self.assertEqual(adapter.request("observe")["error_code"], "no_session")

    def test_named_saves_survive_adapter_restart(self):
        with tempfile.TemporaryDirectory() as save_dir:
            with CliSession(CLI, save_dir=save_dir) as first:
                self.assertTrue(first.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000045"},
                )["ok"])
                self.assertTrue(first.request("save_game", {"slot": "長期", "label": "持久"})["ok"])

            with CliSession(CLI, save_dir=save_dir) as second:
                discovered = second.request("list_saves")
                self.assertTrue(discovered["ok"])
                self.assertEqual([slot["slot"] for slot in discovered["result"]], ["長期"])
                self.assertIsNone(discovered["result"][0]["compatible"])
                self.assertEqual(discovered["result"][0]["difficulty"], "normal")
                self.assertTrue(discovered["result"][0]["persisted"])
                self.assertTrue(second.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000046"},
                )["ok"])
                listed = second.request("list_saves")
                self.assertTrue(listed["ok"])
                self.assertEqual([slot["slot"] for slot in listed["result"]], ["長期"])
                loaded = second.request("load_game", {"slot": "長期"})
                self.assertTrue(loaded["ok"])
                self.assertEqual(loaded["result"]["loaded_slot"], "長期")

    def test_accepted_actions_update_a_durable_autosave(self):
        with tempfile.TemporaryDirectory() as save_dir:
            with CliSession(CLI, save_dir=save_dir) as first:
                self.assertTrue(first.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000047"},
                )["ok"])
                accepted = first.request(
                    "act",
                    {"action": {"type": "refresh_shop"}},
                )
                self.assertTrue(accepted["ok"])
                self.assertTrue(accepted["result"]["accepted"])
                expected = first.request("observe", {"detail": "compact"})["result"]["game_state"]
                saves = first.request("list_saves")["result"]
                autosave = next(slot for slot in saves if slot["slot"] == AUTO_SAVE_SLOT)
                self.assertEqual(autosave["label"], "自動存檔")
                revision = autosave["revision"]

                rejected = first.request(
                    "act",
                    {"action": {"type": "buy_shop_slot", "slot": 99}},
                )
                self.assertTrue(rejected["ok"])
                self.assertFalse(rejected["result"]["accepted"])
                unchanged = next(
                    slot
                    for slot in first.request("list_saves")["result"]
                    if slot["slot"] == AUTO_SAVE_SLOT
                )
                self.assertEqual(unchanged["revision"], revision)

            with CliSession(CLI, save_dir=save_dir) as second:
                discovered = second.request("list_saves")
                self.assertTrue(discovered["ok"])
                persisted = next(
                    slot for slot in discovered["result"] if slot["slot"] == AUTO_SAVE_SLOT
                )
                self.assertEqual(persisted["label"], "自動存檔")
                self.assertTrue(persisted["persisted"])
                self.assertTrue(second.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000048"},
                )["ok"])
                loaded = second.request("load_game", {"slot": AUTO_SAVE_SLOT})
                self.assertTrue(loaded["ok"])
                restored = second.request("observe", {"detail": "compact"})["result"]["game_state"]
                self.assertEqual(restored["state_hash"], expected["state_hash"])
                self.assertEqual(restored["money"], expected["money"])


if __name__ == "__main__":
    unittest.main()
