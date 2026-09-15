import json
import unittest

from test_kys_chess_cli import run_cli, run_jsonl, run_mcp


TALENTS = ("divine_arms", "late_bloomer", "gambler", "backbone")


class ChessTalentTests(unittest.TestCase):
    def test_jsonl_explicit_talent_identity(self):
        requests = [{"id": index, "method": "new", "params": {
            "difficulty": "hard", "seed": "0x0000000000000001", "talent": talent,
        }} for index, talent in enumerate(TALENTS)]
        completed = run_jsonl(requests)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        for talent, response in zip(TALENTS, responses):
            self.assertTrue(response["ok"], response)
            game = response["result"]["game_state"]
            self.assertEqual(game["talent"], talent)
            self.assertTrue(game["talent_description"])

    def test_cli_explicit_talent_identity(self):
        completed = run_cli(["new", "--json", "--difficulty", "hard", "--talent", "late_bloomer"])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertEqual(json.loads(completed.stdout)["result"]["game_state"]["talent"], "late_bloomer")

    def test_mcp_new_game_schema_exposes_talents(self):
        completed = run_mcp([{"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        response = json.loads(completed.stdout.splitlines()[-1])
        new_game = next(tool for tool in response["result"]["tools"] if tool["name"] == "new_game")
        self.assertEqual(new_game["inputSchema"]["properties"]["talent"]["enum"], list(TALENTS))


if __name__ == "__main__":
    unittest.main()
