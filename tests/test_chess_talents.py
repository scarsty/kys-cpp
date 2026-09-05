import copy
import json
from pathlib import Path
import unittest

import jsonschema
import yaml

from test_kys_chess_cli import run_cli, run_jsonl, run_mcp


ROOT = Path(__file__).resolve().parents[1]
TALENTS = ("divine_arms", "late_bloomer", "gambler", "backbone")


class ChessTalentTests(unittest.TestCase):
    def test_config_schemas_accept_official_content_and_reject_invalid_talents(self):
        for name in ("talents", "balance"):
            schema = json.loads((ROOT / "schemas" / f"chess_{name}.schema.json").read_text(encoding="utf-8"))
            jsonschema.Draft202012Validator.check_schema(schema)
            validator = jsonschema.Draft202012Validator(schema)
            paths = [ROOT / "config/chess_talents.yaml"] if name == "talents" else sorted((ROOT / "config").glob("chess_balance_*.yaml"))
            for path in paths:
                document = yaml.safe_load(path.read_text(encoding="utf-8"))
                validator.validate(document)
                invalid = copy.deepcopy(document)
                if name == "talents":
                    invalid["棋手天賦"]["晚成"]["勝場成長受加成比例"] = 101
                else:
                    invalid["棋手天賦"]["可選"].append("神兵")
                with self.assertRaises(jsonschema.ValidationError):
                    validator.validate(invalid)
                invalid = copy.deepcopy(document)
                if name == "talents":
                    invalid["棋手天賦"]["賭徒"]["賭運"]["觸發機率上限"] = -1
                else:
                    invalid["玩家裝備獎勵"]["基本"][0].pop("最低層級")
                with self.assertRaises(jsonschema.ValidationError):
                    validator.validate(invalid)

    def test_jsonl_talent_identity_capability_and_difficulty_rejection(self):
        requests = [{"id": index, "method": "new", "params": {
            "difficulty": "hard", "seed": "0x0000000000000001", "talent": talent,
        }} for index, talent in enumerate(TALENTS)]
        requests += [{"id": 9, "method": "new", "params": {
            "difficulty": "easy", "seed": "0x0000000000000001", "talent": "gambler",
        }}]
        completed = run_jsonl(requests)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        responses = [json.loads(line) for line in completed.stdout.splitlines()]
        for talent, response in zip(TALENTS, responses):
            self.assertTrue(response["ok"], response)
            game = response["result"]["game_state"]
            self.assertEqual(game["talent"], talent)
            self.assertTrue(game["talent_description"])
            self.assertEqual(game["talent_has_legendary_shop"], talent == "divine_arms")
            self.assertEqual(game["shop_guarantees"], [])
            self.assertEqual(game["next_basic_equipment_reward"]["fight"], 3)
            if talent == "gambler":
                self.assertIsNotNone(game["pending_reward"])
        self.assertEqual(responses[-1]["error_code"], "invalid_params")

    def test_cli_explicit_talent_and_default_identity(self):
        for difficulty, args, expected in (("hard", ["--talent", "late_bloomer"], "late_bloomer"),
                                           ("normal", [], "divine_arms")):
            completed = run_cli(["new", "--json", "--difficulty", difficulty, *args])
            self.assertEqual(completed.returncode, 0, completed.stderr)
            self.assertEqual(json.loads(completed.stdout)["result"]["game_state"]["talent"], expected)

    def test_mcp_new_game_schema_exposes_talents(self):
        completed = run_mcp([{"jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}}])
        self.assertEqual(completed.returncode, 0, completed.stderr)
        response = json.loads(completed.stdout.splitlines()[-1])
        new_game = next(tool for tool in response["result"]["tools"] if tool["name"] == "new_game")
        self.assertEqual(new_game["inputSchema"]["properties"]["talent"]["enum"], list(TALENTS))


if __name__ == "__main__":
    unittest.main()
