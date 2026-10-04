import json
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "promo"))
import build_page as promo


def identity(text):
    return text


class PromoPageTests(unittest.TestCase):
    def test_config_paths_use_canonical_keys_and_render_lists(self):
        config = {"棋手天賦": {"可選": ["神兵", "晚成"]}, "經濟": {"刷新費用": 7}}
        self.assertEqual(promo.dig(config, "棋手天賦.可選"), "神兵、晚成")
        self.assertEqual(promo.dig(config, "經濟.刷新費用"), "7")
        with self.assertRaises(KeyError):
            promo.dig(config, "经济.刷新费用")

    def test_language_conversion_preserves_config_and_asset_placeholders(self):
        source = '天賦 %%Y:hb:棋手天賦.可選%% %%HEAD:123%% %%ULTIMATE:7%%'
        converted = promo.convert_preserving_placeholders(source, lambda text: text.replace("天賦", "天赋"))
        self.assertEqual(converted, '天赋 %%Y:hb:棋手天賦.可選%% %%HEAD:123%% %%ULTIMATE:7%%')

    def test_catalog_queries_use_explicit_roots_and_both_description_styles(self):
        def run(command, **kwargs):
            self.assertEqual(command, ["fixture-cli", "--jsonl", "--data-root", "fixture-game", "--config-root", "fixture-config"])
            requests = [json.loads(line) for line in kwargs["input"].splitlines()]
            self.assertEqual(requests[0]["params"]["difficulty"], "hard")
            self.assertEqual(requests[1]["params"], {"role_ids": [123], "detail": "full"})
            self.assertEqual(requests[2]["params"], {"role_ids": [123], "detail": "compact"})
            responses = [{"id": request["id"], "ok": True, "result": {"fixture": request["id"]}} for request in requests]
            return subprocess.CompletedProcess(command, 0, "\n".join(json.dumps(response) for response in responses), "")

        with patch.object(promo.subprocess, "run", side_effect=run):
            result = promo.load_catalog(Path("fixture-cli"), Path("fixture-game"), Path("fixture-config"), [123])
        self.assertEqual(result, {"full": {"fixture": "full"}, "compact": {"fixture": "compact"}})

    def test_catalog_failure_stops_generation(self):
        responses = [{"id": "new", "ok": False, "error_message": "無效配置"},
                     {"id": "full", "ok": False}, {"id": "compact", "ok": False}]
        completed = subprocess.CompletedProcess([], 0, "\n".join(json.dumps(response) for response in responses), "")
        with patch.object(promo.subprocess, "run", return_value=completed), self.assertRaisesRegex(RuntimeError, "無效配置"):
            promo.load_catalog(Path("cli"), Path("game"), Path("config"), [])

    def test_ultimate_owners_select_highest_power_per_star_and_match_tie_break(self):
        def ability(magic_id, powers):
            return {"magic_id": magic_id, "power_by_star": [
                {"star": star, "power": power} for star, power in powers]}

        roles = [
            {"name": "甲", "abilities": [ability(7, [(1, 40), (2, 90), (3, 50)]),
                                         ability(8, [(1, 80), (2, 20), (3, 60)])]},
            {"name": "乙", "abilities": [ability(7, [(1, 20), (2, 20), (3, 20)])]},
            {"name": "丙", "abilities": [ability(7, [(1, 60)]), ability(9, [(1, 60)])]},
            {"name": "無武功", "abilities": []},
        ]
        self.assertEqual(promo.ultimate_owners(roles), {
            7: [{"name": "甲", "stars": [2]}, {"name": "乙", "stars": [1, 2, 3]}],
            8: [{"name": "甲", "stars": [1, 3]}],
            9: [{"name": "丙", "stars": [1]}],
        })

    def test_ultimate_uses_shared_effect_text_and_escapes_content(self):
        effects = {"sections": [{"blocks": [{"rows": [{"text": "護盾 <120> & 70幀"}]}]}]}
        result = promo.render_ultimate({"武功": 7, "名稱": "測試<劍>"},
                                       {7: {"effects": effects}}, {7: [{"name": "甲&乙", "stars": [2, 3]}]}, identity)
        self.assertIn("測試&lt;劍&gt;", result)
        self.assertIn("甲&amp;乙", result)
        self.assertIn("絕招使用者", result)
        self.assertIn("2★／3★", result)
        self.assertIn("護盾 &lt;<strong>120</strong>&gt; &amp; <strong>70幀</strong>", result)
        self.assertNotIn("<120>", result)

    def test_ultimate_without_selected_casters_is_explicit(self):
        result = promo.render_ultimate({"武功": 7, "名稱": "測試"},
                                       {7: {"effects": {"sections": []}}}, {}, identity)
        self.assertIn("目前棋池無絕招使用者", result)

    def test_talent_visuals_use_fixture_values_instead_of_long_descriptions(self):
        talents = {
            "神兵": {"說明": "長篇說明", "可使用神兵商店": True},
            "晚成": {"說明": "長篇說明", "勝場成長受加成比例": 50},
            "賭徒": {"說明": "長篇說明", "開局額外禁棋": {"次數": 9, "最低費用": 1, "最高費用": 2},
                     "賭運": {"目標最低費用": 2, "目標最高費用": 4, "累積截止關卡": 17,
                              "每次增加層數": 2, "層數上限": 3, "每層觸發機率百分點": 25,
                              "觸發後生命": 2, "無敵幀數": 73}},
            "中堅": {"說明": "長篇說明", "目標費用": 3,
                     "額外星級加成": {"每顆開場內力": 13, "計算上限": 4, "每顆強化次數": 2, "強化傷害百分比": 37},
                     "刷新保證": {"觸發星級": 2, "每次數量": 2}},
        }
        balances = {"hb": {"棋手天賦": {"可選": list(talents)}, "星級加成": {"生命倍率": 0.6},
                           "神兵商店": {"通關後": 22, "價格": 71},
                           "玩家裝備獎勵": {"天賦額外": {"神兵": [{"關卡": 7}, {"關卡": 13}]}}}}
        result = promo.render_talents(talents, balances, identity)
        for value in ("+2次", "22關後", "71金", "×1.3", "×1.6", "25%", "75%", "73幀無敵", "+52", "8次", "增傷 +37%", "保證2枚同名"):
            with self.subTest(value=value):
                self.assertIn(value, result)
        self.assertIn('class="growth-chart"', result)
        self.assertIn('class="luck-ladder"', result)
        self.assertNotIn("長篇說明", result)

    def test_talent_availability_and_defaults_follow_fixture(self):
        result = promo.render_talent_choices({"eb": {"棋手天賦": {"可選": ["晚成", "中堅"], "預設": "中堅"}}}, identity)
        self.assertEqual(result, '<tr><td>簡單</td><td>晚成、中堅</td><td>中堅</td></tr>')

    def test_reward_schedule_merges_sources_and_keeps_tier_bounds(self):
        def reward(fight, minimum, maximum, choices, cost):
            return {"關卡": fight, "最低層級": minimum, "最高層級": maximum,
                    "選項數量": choices, "追加選項費用": cost}
        balances = {"hb": {"玩家裝備獎勵": {
            "基本": [reward(9, 2, 4, 6, 13)], "天賦額外": {"神兵": [reward(5, 1, 3, 8, 17)]}}}}
        result = promo.render_equipment_rewards(balances, identity)
        self.assertIn('<td>5</td><td>神兵</td><td>1～3</td><td>8</td><td>17</td>', result)
        self.assertIn('<td>9</td><td>全體天賦</td><td>2～4</td><td>6</td><td>13</td>', result)
        self.assertLess(result.index('<td>5</td>'), result.index('<td>9</td>'))

    def test_assembly_embeds_both_languages_and_escapes_play_url(self):
        template = '<html><!--SBODY-->source<!--EBODY--><script>script</script></html>'
        result = promo.assemble_page(template, '<a href="%%PLAY_URL%%">天赋</a>', '天賦', 'https://example.test/?a=1&b="2"')
        self.assertIn('<div id="body-s">', result)
        self.assertIn('<div id="body-t" hidden>天賦</div>', result)
        self.assertIn('https://example.test/?a=1&amp;b=&quot;2&quot;', result)
        self.assertIn('<script>script</script>', result)

    def test_unresolved_placeholder_stops_generation(self):
        with self.assertRaisesRegex(ValueError, "%%UNKNOWN%%"):
            promo.assemble_page('<!--SBODY-->source<!--EBODY-->', '%%UNKNOWN%%', '', 'https://example.test/')


if __name__ == "__main__":
    unittest.main()
