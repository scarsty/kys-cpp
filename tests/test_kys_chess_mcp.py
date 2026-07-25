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

from kys_chess_mcp.server import AUTO_SAVE_SLOT, CliSession, dispatch_mcp_tool, mcp_tools


CLI = Path(os.environ.get("KYS_CHESS_CLI", ROOT / "x64" / "Debug" / "kys_chess_cli.exe"))


def write_deployment_manifest(
    path: Path,
    version: str,
    executable: Path,
    activation: str | None = None,
) -> None:
    temporary = path.with_suffix(".tmp")
    temporary.write_text(
        json.dumps({
            "version": version,
            "activation": activation or version,
            "executable": str(executable),
        }),
        encoding="utf-8",
    )
    temporary.replace(path)


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
                        "checkpoint": exported_save["result"]["checkpoint"],
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
        self.assertNotIn("def new_game(", source)
        self.assertNotIn("def observe_game(", source)
        self.assertIn("def dispatch_mcp_tool(", source)

        with tempfile.TemporaryDirectory() as save_dir, CliSession(
            CLI,
            save_dir=save_dir,
            autosave=False,
        ) as adapter:
            definitions = {tool["name"]: tool for tool in adapter.tool_catalog()}
            self.assertIn("new_game", definitions)
            self.assertIn("export_save_file", definitions)
            self.assertIn("import_save_file", definitions)
            self.assertNotIn("export_save", definitions)
            self.assertNotIn("import_save", definitions)
            new_schema = definitions["new_game"]["inputSchema"]["properties"]
            self.assertEqual(new_schema["difficulty"]["enum"], ["easy", "normal", "hard"])
            self.assertEqual(new_schema["detail"]["enum"], ["compact", "full"])
            self.assertEqual(new_schema["seed"]["pattern"], r"^0x[0-9a-fA-F]{16}$")
            self.assertIn("0x 前綴", new_schema["seed"]["description"])
            self.assertEqual(
                definitions["take_action"]["inputSchema"]["properties"]["detail"]["default"],
                "summary",
            )
            self.assertEqual(len(mcp_tools(adapter)), len(definitions))
            created = dispatch_mcp_tool(
                adapter,
                "new_game",
                {"difficulty": "normal", "seed": "0x0000000000000042"},
            )
            self.assertTrue(created["ok"])
            self.assertTrue(adapter.request("save_game", {"slot": "source"})["ok"])
            exported_path = Path(save_dir) / "portable.json"
            exported = dispatch_mcp_tool(
                adapter,
                "export_save_file",
                {"slot": "source", "path": str(exported_path)},
            )
            self.assertTrue(exported["ok"])
            checkpoint = json.loads(exported_path.read_text(encoding="utf-8"))
            self.assertGreater(checkpoint["random"]["streams"][0]["words"][0], 2**53)
            imported = dispatch_mcp_tool(
                adapter,
                "import_save_file",
                {"slot": "copy", "path": str(exported_path)},
            )
            self.assertTrue(imported["ok"])

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

    def test_generic_stdio_adapter_interoperates_with_the_official_client(self):
        import anyio
        from mcp import ClientSession, StdioServerParameters
        from mcp.client.stdio import stdio_client

        async def verify(manifest: Path, save_dir: Path):
            environment = os.environ.copy()
            environment["KYS_CHESS_MCP_CURRENT"] = str(manifest)
            environment["KYS_CHESS_MCP_SAVE_DIR"] = str(save_dir)
            environment["PYTHONPATH"] = os.pathsep.join(
                filter(None, [str(PACKAGE_ROOT), environment.get("PYTHONPATH")])
            )
            parameters = StdioServerParameters(
                command=sys.executable,
                args=["-m", "kys_chess_mcp.server"],
                env=environment,
            )
            async with stdio_client(parameters) as (read_stream, write_stream):
                async with ClientSession(read_stream, write_stream) as session:
                    await session.initialize()
                    tools = await session.list_tools()
                    created = await session.call_tool(
                        "new_game",
                        {"difficulty": "normal", "seed": "0x0000000000000042"},
                    )
                    names = {tool.name for tool in tools.tools}
                    self.assertIn("export_save_file", names)
                    self.assertNotIn("export_save", names)
                    self.assertFalse(created.isError)
                    self.assertTrue(created.structuredContent["ok"])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "current.json"
            write_deployment_manifest(manifest, "test", CLI)
            anyio.run(verify, manifest, root / "saves")

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

    def test_runtime_deployment_reload_restores_the_active_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "current.json"
            save_dir = root / "saves"
            write_deployment_manifest(manifest, "version-1", CLI)
            with CliSession(deployment_manifest=manifest, save_dir=save_dir) as adapter:
                self.assertTrue(adapter.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000049"},
                )["ok"])
                self.assertTrue(adapter.request(
                    "act",
                    {"action": {"type": "refresh_shop"}},
                )["result"]["accepted"])
                expected = adapter.request("observe", {"detail": "compact"})["result"]["game_state"]
                previous_pid = adapter._process.pid

                write_deployment_manifest(manifest, "version-2", CLI)
                restored = adapter.request("observe", {"detail": "compact"})

                self.assertTrue(restored["ok"])
                self.assertEqual(restored["runtime_reload"]["status"], "succeeded")
                self.assertTrue(restored["runtime_reload"]["session_restored"])
                self.assertEqual(restored["runtime_reload"]["previous"]["version"], "version-1")
                self.assertEqual(restored["runtime_reload"]["current"]["version"], "version-2")
                self.assertNotEqual(adapter._process.pid, previous_pid)
                self.assertEqual(restored["result"]["game_state"]["state_hash"], expected["state_hash"])
                self.assertEqual(restored["result"]["game_state"]["money"], expected["money"])

    def test_runtime_deployment_activation_retries_the_same_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "current.json"
            write_deployment_manifest(manifest, "same-package", CLI, "activation-1")
            with CliSession(deployment_manifest=manifest, save_dir=root / "saves") as adapter:
                previous_pid = adapter._process.pid

                write_deployment_manifest(manifest, "same-package", CLI, "activation-2")
                reloaded = adapter.request("list_saves")

                self.assertTrue(reloaded["ok"])
                self.assertEqual(reloaded["runtime_reload"]["status"], "succeeded")
                self.assertFalse(reloaded["runtime_reload"]["session_restored"])
                self.assertEqual(reloaded["runtime_reload"]["current"]["activation"], "activation-2")
                self.assertNotEqual(adapter._process.pid, previous_pid)

    def test_runtime_deployment_failure_rolls_back_the_active_state(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = root / "current.json"
            save_dir = root / "saves"
            write_deployment_manifest(manifest, "working", CLI)
            with CliSession(deployment_manifest=manifest, save_dir=save_dir) as adapter:
                self.assertTrue(adapter.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x000000000000004a"},
                )["ok"])
                self.assertTrue(adapter.request(
                    "act",
                    {"action": {"type": "refresh_shop"}},
                )["result"]["accepted"])
                expected = adapter.request("observe", {"detail": "compact"})["result"]["game_state"]

                write_deployment_manifest(manifest, "broken", Path(sys.executable))
                failure = adapter.request("observe", {"detail": "compact"})

                self.assertFalse(failure["ok"])
                self.assertEqual(failure["error_code"], "runtime_reload_failed")
                self.assertFalse(failure["session_lost"])
                self.assertEqual(failure["runtime_reload"]["status"], "rolled_back")
                self.assertEqual(failure["runtime_reload"]["requested"]["version"], "broken")
                continued = adapter.request("observe", {"detail": "compact"})
                self.assertTrue(continued["ok"])
                self.assertEqual(continued["result"]["game_state"]["state_hash"], expected["state_hash"])
                self.assertEqual(adapter.diagnostic_status()["runtime"]["version"], "working")

    def test_named_saves_survive_adapter_restart(self):
        with tempfile.TemporaryDirectory() as save_dir:
            with CliSession(CLI, save_dir=save_dir) as first:
                self.assertTrue(first.request(
                    "new",
                    {"difficulty": "normal", "seed": "0x0000000000000045"},
                )["ok"])
                self.assertTrue(first.request("save_game", {"slot": "長期", "label": "持久"})["ok"])

            persisted_files = list(Path(save_dir).glob("*.json"))
            self.assertEqual(len(persisted_files), 1)
            stored = json.loads(persisted_files[0].read_text(encoding="utf-8"))
            self.assertIn("checkpoint", stored)
            self.assertNotIn("payload", stored)
            self.assertIsInstance(stored["checkpoint"]["replay"]["decisions"], list)

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
