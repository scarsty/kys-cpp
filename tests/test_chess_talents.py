import json
import unittest
from pathlib import Path

import jsonschema

from test_kys_chess_cli import run_cli, run_jsonl, run_mcp


TALENTS = ("divine_arms", "late_bloomer", "gambler", "backbone", "alliance")


class ChessTalentSchemaTests(unittest.TestCase):
    def test_alliance_opening_bonus_only_accepts_zero_or_one(self):
        path = Path(__file__).resolve().parents[1] / "schemas" / "chess_talents.schema.json"
        schema = json.loads(path.read_text(encoding="utf-8"))
        alliance_schema = schema["properties"]["棋手天賦"]["properties"]["合縱"]
        validator = jsonschema.Draft202012Validator(alliance_schema)
        fixture = {"說明": "測試合縱", "高階門檻": {"最低門檻": 6, "降低人數": 1}}
        for bonus in (0, 1):
            with self.subTest(bonus=bonus):
                validator.validate({**fixture, "開局羈絆加點": bonus})
        for bonus in (-1, 2, 100):
            with self.subTest(bonus=bonus):
                self.assertTrue(list(validator.iter_errors({**fixture, "開局羈絆加點": bonus})))


class ChessTalentTests(unittest.TestCase):
    def test_jsonl_explicit_talent_identity(self):
        requests = [{"id": index, "method": "new", "params": {
            "difficulty": "hard", "seed": "0x0000000000000001", "talent": talent,
            "detail": "full",
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
