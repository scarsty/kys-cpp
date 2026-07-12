import ast
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
PACKAGE_ROOT = ROOT / "tools" / "kys_chess_mcp"
sys.path.insert(0, str(PACKAGE_ROOT))

from kys_chess_mcp.server import CliSession


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
            with CliSession(CLI) as adapter:
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
                exported_replay = request_pair("export_replay")
                request_pair(
                    "verify_replay",
                    {"replay_jsonl": exported_replay["result"]["replay_jsonl"]},
                )
        finally:
            direct.close()

    def test_mcp_tool_surface_matches_the_protocol_contract(self):
        source = (PACKAGE_ROOT / "kys_chess_mcp" / "server.py").read_text(encoding="utf-8")
        for tool in (
            "new_game",
            "observe_game",
            "list_legal_actions",
            "take_action",
            "list_saves",
            "inspect_save",
            "save_game",
            "load_game",
            "export_save",
            "import_save",
            "export_replay",
            "verify_replay",
        ):
            self.assertIn(f"def {tool}(", source)

        tree = ast.parse(source)
        docstrings = {
            node.name: ast.get_docstring(node) or ""
            for node in ast.walk(tree)
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
        }
        expected_rollback_descriptions = {
            "observe_game": ("替換時間線", "捨棄目前後綴"),
            "list_saves": ("前綴", "捨棄目前較新的行動"),
            "load_game": ("重播前綴", "discarded_active_actions", "存檔目錄仍保留"),
            "import_save": ("不會靜默載入", "替換目前時間線"),
        }
        for tool, fragments in expected_rollback_descriptions.items():
            self.assertIn(tool, docstrings)
            for fragment in fragments:
                self.assertIn(fragment, docstrings[tool])

    def test_adapter_owns_and_closes_one_cli_process(self):
        adapter = CliSession(CLI)
        response = adapter.request(
            "new",
            {"difficulty": "normal", "seed": "0x0000000000000043"},
        )
        self.assertTrue(response["ok"])
        adapter.close()
        self.assertIsNotNone(adapter._process.poll())


if __name__ == "__main__":
    unittest.main()
