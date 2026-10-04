#!/usr/bin/env python3
"""建構《金群自走棋》宣傳單 + 玩法指南單頁（簡體 / 繁體切換）。

從 tools/promo/page_template.html 生成自包含的 HTML 頁面：
- %%SHOT:key%%      → tools/promo/shots/key.jpg 的 base64 內嵌（建構時可重新壓縮）
- %%ICON%%          → assets/app_icon.png 的遊戲圖示
- %%HEAD:id%%       → <game-dir>/resource/head/id.webp 角色頭像（壓縮至 120px webp）
- %%TIP:id%%        → game.db 角色四圍/武功 + chess_combos.yaml 羈絆的 hover 懸浮卡片
- %%Y:eb|nb|hb:路徑%% → config/chess_balance_{easy,normal,hard}.yaml 中的純量
- %%Y:ng:路徑%%      → config/chess_neigong.yaml 中的純量
- %%Y:talents:路徑%% → config/chess_talents.yaml 中的純量
- %%C:pool|pool_easy|combos|equip|neigong|challenge%% → 各設定的條目計數
- %%GEN:STATS%%     → 頭版資料速覽條（由上述計數生成）
- %%GEN:TALENTS|TALENT_CHOICES|EQUIPMENT_REWARDS|ULTIMATE_CATALOG%% → 配置與遊戲目錄
- %%ULTIMATE:id%%   → 與遊戲共用的絕招效果描述
- %%PLAY_URL%%      → 網頁版遊戲網址

繁體正文為來源，簡體版本由 OpenCC t2s 自動轉換；兩個版本同頁內嵌、前端切換。
先以 .github/build-command.ps1 建構 kys_chess_cli，再執行本工具。
用法：.venv/Scripts/python.exe tools/promo/build_page.py [--game-dir 路徑] [--config-dir 路徑] [--cli 路徑] [--output 路徑] [--play-url 網址]
"""
import argparse
import base64
import io
import json
import re
import sqlite3
import subprocess
import sys
from html import escape as html_escape
from pathlib import Path

import yaml
from opencc import OpenCC
from PIL import Image
from traditional_text import PROMO_TRADITIONAL_REPLACEMENTS, to_traditional

ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = ROOT / "tools/promo/page_template.html"
SHOTS_DIR = ROOT / "tools/promo/shots"
APP_ICON = ROOT / "assets/app_icon.png"

SHOT_WIDTH, SHOT_QUALITY = 960, 72
ICON_SIZE, ICON_QUALITY = 180, 88
HEAD_SIZE, HEAD_QUALITY = 120, 82

BALANCE_FILES = {
    "eb": "chess_balance_easy.yaml",
    "nb": "chess_balance_normal.yaml",
    "hb": "chess_balance_hard.yaml",
}

DIFFICULTIES = {"eb": ("easy", "簡單"), "nb": ("normal", "標準"), "hb": ("hard", "困難")}

def data_uri(mime: str, raw: bytes) -> str:
    return "data:" + mime + ";base64," + base64.b64encode(raw).decode()


def render_shot(key: str) -> str:
    path = SHOTS_DIR / f"{key}.jpg"
    im = Image.open(path).convert("RGB")
    if im.width > SHOT_WIDTH:
        im = im.resize((SHOT_WIDTH, im.height * SHOT_WIDTH // im.width), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=SHOT_QUALITY, optimize=True)
    return f'<img src="{data_uri("image/jpeg", buf.getvalue())}" alt="{key}" loading="lazy">'


def render_icon(convert_ui_text) -> str:
    im = Image.open(APP_ICON).convert("RGBA")
    im.thumbnail((ICON_SIZE, ICON_SIZE), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "WEBP", quality=ICON_QUALITY)
    return f'<img src="{data_uri("image/webp", buf.getvalue())}" alt="{convert_ui_text("遊戲圖示")}">'


def render_head(role_id: str, heads_dir: Path, convert_ui_text) -> str:
    path = heads_dir / f"{role_id}.webp"
    im = Image.open(path).convert("RGBA")
    im.thumbnail((HEAD_SIZE, HEAD_SIZE), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "WEBP", quality=HEAD_QUALITY)
    return (
        f'<img src="{data_uri("image/webp", buf.getvalue())}" '
        f'alt="{convert_ui_text("角色頭像")}{role_id}" loading="lazy">'
    )


def load_role_table(game_db: Path):
    """讀取 UTF-8 game.db：武功 id → 名稱，角色 id → 四圍與武功 id 列表。"""
    con = sqlite3.connect(game_db)
    cur = con.cursor()
    magics = dict(cur.execute("SELECT 编号,名称 FROM magic"))
    roles = {}
    rows = cur.execute(
        "SELECT 编号,生命最大值,攻击力,防御力,轻功,"
        "一星武功1,一星武功2,二星武功1,二星武功2,三星武功1,三星武功2 FROM role"
    )
    for row in rows:
        roles[row[0]] = {
            "hp": row[1], "atk": row[2], "def": row[3], "spd": row[4],
            "skills": [row[5], row[6], row[7], row[8], row[9], row[10]],
        }
    con.close()
    return roles, magics


def render_tip(role_id: str, roles, magics, id2combo, convert_ui_text, convert_db_text, convert_yaml_text) -> str:
    r = roles[int(role_id)]
    skills = []
    for m in r["skills"]:
        if m <= 0:
            continue
        name = html_escape(convert_db_text(magics[m]))
        if name and name not in skills:
            skills.append(name)
    parts = [
        f'<span><span class="lb">{convert_ui_text("四圍")}</span>　'
        f'{convert_ui_text("生命")} {r["hp"]} · {convert_ui_text("攻擊")} {r["atk"]} · '
        f'{convert_ui_text("防禦")} {r["def"]} · {convert_ui_text("輕功")} {r["spd"]}</span>'
    ]
    if skills:
        parts.append(f'<span><span class="lb">{convert_ui_text("武功")}</span>　{"、".join(skills)}</span>')
    own = id2combo.get(int(role_id))
    if own:
        combo_names = [html_escape(convert_yaml_text(name)) for name in own]
        parts.append(f'<span><span class="lb">{convert_ui_text("羈絆")}</span>　{"、".join(combo_names)}</span>')
    return '<div class="tip">' + "".join(parts) + "</div>"


def load_yaml(config_dir: Path, name: str):
    return yaml.safe_load((config_dir / name).read_text(encoding="utf-8"))


def dig(data, dotted: str):
    cur = data
    for part in dotted.split("."):
        cur = cur[part]
    if isinstance(cur, list):
        return "、".join(str(x) for x in cur)
    return str(cur)


def load_catalog(cli: Path, game_dir: Path, config_dir: Path, role_ids):
    """完整目錄用於判定各星級絕招；精簡目錄提供遊戲共用的短描述。"""
    requests = [{"id": "new", "method": "new", "params": {
        "difficulty": "hard", "seed": "0x0000000000000001",
    }}]
    requests.extend({"id": detail, "method": "inspect_catalog", "params": {
        "role_ids": list(role_ids), "detail": detail,
    }} for detail in ("full", "compact"))
    completed = subprocess.run(
        [str(cli), "--jsonl", "--data-root", str(game_dir), "--config-root", str(config_dir)],
        input="".join(json.dumps(request, ensure_ascii=False) + "\n" for request in requests),
        text=True, encoding="utf-8", capture_output=True, check=True, timeout=60,
    )
    responses = [json.loads(line) for line in completed.stdout.splitlines()]
    if len(responses) != len(requests):
        raise RuntimeError("遊戲目錄回覆不完整，請重新建構 kys_chess_cli")
    catalogs = {}
    for request, response in zip(requests, responses):
        if response["id"] != request["id"] or not response["ok"]:
            raise RuntimeError(f'遊戲目錄查詢失敗：{response}')
        if request["method"] == "inspect_catalog":
            catalogs[request["id"]] = response["result"]
    return catalogs


def ultimate_owners(roles):
    """與 chessRoleMagicsForStar / BattleSetupFactory 同按（威力，武功 ID）取最大值。"""
    owners = {}
    for role in roles:
        selected_stars = {}
        for star in (1, 2, 3):
            candidates = [(power["power"], ability["magic_id"])
                          for ability in role["abilities"] for power in ability["power_by_star"]
                          if power["star"] == star]
            if candidates:
                magic_id = max(candidates)[1]
                selected_stars.setdefault(magic_id, []).append(star)
        for magic_id, stars in selected_stars.items():
            owners.setdefault(magic_id, []).append({"name": role["name"], "stars": stars})
    return owners


def highlighted_text(text, convert_text):
    parts = re.split(r"([+-]?\d+(?:\.\d+)?(?:%|幀|格|層|次)?)", convert_text(text))
    return "".join(f'<strong>{html_escape(part)}</strong>' if index % 2 else html_escape(part)
                   for index, part in enumerate(parts))


def effect_paragraphs(effects, convert_text):
    return "".join(
        f'<p>{highlighted_text(row["text"], convert_text)}</p>'
        for section in effects["sections"] for block in section["blocks"] for row in block["rows"]
    )


def render_ultimate(definition, abilities, owners, convert_text):
    magic_id = definition["武功"]
    ability = abilities[magic_id]
    name = html_escape(convert_text(definition["名稱"]))
    caster_chips = []
    for owner in owners.get(magic_id, []):
        stars = "" if owner["stars"] == [1, 2, 3] else '<small>' + "／".join(f'{star}★' for star in owner["stars"]) + '</small>'
        caster_chips.append(f'<span>{html_escape(convert_text(owner["name"]))}{stars}</span>')
    casters = (
        f'<div class="ultimate-owners"><span class="owner-label">{html_escape(convert_text("絕招使用者"))}</span>'
        + "".join(caster_chips) + '</div>'
        if caster_chips else f'<p class="ultimate-owners">{html_escape(convert_text("目前棋池無絕招使用者"))}</p>'
    )
    return (
        f'<article class="card ultimate-card"><h3>{name}</h3>'
        + casters
        + effect_paragraphs(ability["effects"], convert_text)
        + '</article>'
    )


def render_metrics(metrics, text):
    return '<div class="talent-metrics">' + "".join(
        f'<div><b>{text(value)}</b><span>{text(label)}</span></div>' for value, label in metrics
    ) + '</div>'


def render_chips(values, text, class_name="talent-chips"):
    return f'<div class="{class_name}">' + "".join(f'<span>{text(value)}</span>' for value in values) + '</div>'


def render_flow(values, text):
    return '<ol class="talent-flow">' + "".join(f'<li>{text(value)}</li>' for value in values) + '</ol>'


def render_talents(talents, balances, convert_text):
    def text(value):
        return html_escape(convert_text(str(value)))

    cards = []
    for name, definition in talents.items():
        scope = next(scope for scope in reversed(balances) if name in balances[scope]["棋手天賦"]["可選"])
        balance = balances[scope]
        if name == "神兵":
            route = "裝備優先"
            rewards = balance["玩家裝備獎勵"]["天賦額外"][name]
            shop = balance["神兵商店"]
            shop_unlock = f'{shop["通關後"]}關後' if shop["通關後"] > 0 else "未開放"
            content = render_metrics([(f'+{len(rewards)}次', "額外裝備獎勵"),
                                      (shop_unlock, "神兵商店"),
                                      (f'{shop["價格"]}金', "每件神兵")], text)
            content += render_flow(["推進主線", "多選裝備", "終局買神兵"], text)
            content += f'<div class="talent-caption">{text("額外裝備獎勵關卡")}</div>'
            content += render_chips([f'{reward["關卡"]}關' for reward in rewards], text, "reward-stages")
        elif name == "晚成":
            route = "勝場養成"
            proportion = definition["勝場成長受加成比例"]
            factors = [1 + balance["星級加成"]["生命倍率"] * proportion / 100 * extra for extra in (0, 1, 2)]
            content = render_metrics([(f'{proportion}%', "成長參與加成"), ("2★", "開始星級放大"), ("3★", "放大兩次")], text)
            content += f'<div class="talent-caption">{text("同勝場 · 生命成長倍率")}</div><div class="growth-chart">'
            for star, factor in enumerate(factors, 1):
                factor_label = f'{factor:.3f}'.rstrip("0").rstrip(".")
                content += (f'<div><span>{star}★</span><div class="growth-track"><i style="width:{factor / max(factors) * 100:.2f}%"></i></div>'
                            f'<b>×{factor_label}</b></div>')
            content += '</div>' + render_chips(["保留高勝場主力", "升星放大成長"], text)
            content += f'<p class="talent-note">{text("示意未計其他加成與取整")}</p>'
        elif name == "賭徒":
            route = "低費免死"
            bans, luck = definition["開局額外禁棋"], definition["賭運"]
            content = render_metrics([(f'+{bans["次數"]}', "開局禁棋"),
                                      (f'{luck["目標最低費用"]}～{luck["目標最高費用"]}費', "賭運棋子"),
                                      (f'{luck["累積截止關卡"]}關', "累積截止")], text)
            content += render_flow(["付費刷新", f'+{luck["每次增加層數"]}層賭運', "致命時免死判定"], text)
            content += f'<div class="talent-caption">{text("層數 → 免死機率")}</div><div class="luck-ladder">'
            for stack in range(1, luck["層數上限"] + 1):
                chance = min(100, stack * luck["每層觸發機率百分點"])
                content += f'<div><b>{chance}%</b><span>{text(f"{stack}層")}</span></div>'
            content += '</div>' + render_chips([f'{luck["觸發後生命"]}生命', f'{luck["無敵幀數"]}幀無敵', "嘗試絕招"], text)
            ban_note = f'禁棋限{bans["最低費用"]}～{bans["最高費用"]}費；賭運優先上場棋子'
            content += f'<p class="talent-note">{text(ban_note)}</p>'
        elif name == "中堅":
            route = "三費核心"
            tier = definition["目標費用"]
            bonus, guarantee = definition["額外星級加成"], definition["刷新保證"]
            content = render_metrics([(f'{tier}費', "加成對象"),
                                      (f'+{bonus["每顆開場內力"] * bonus["計算上限"]}', "開場內力上限"),
                                      (f'{bonus["每顆強化次數"] * bonus["計算上限"]}次', "強化上限")], text)
            content += render_flow([f'非{tier}費友軍的額外星級', f'{tier}費核心開場加成'], text)
            content += render_chips([f'增傷 +{bonus["強化傷害百分比"]}%', f'減傷 {bonus["強化傷害百分比"]}%', "攻防共用次數"], text)
            per_star = f'+{bonus["每顆開場內力"]}內力 · +{bonus["每顆強化次數"]}次'
            counting = f'已上陣 · 最多{bonus["計算上限"]}顆'
            content += f'<div class="talent-rule"><span>{text("每顆額外星")}</span><b>{text(per_star)}</b></div>'
            content += f'<div class="talent-rule"><span>{text("計算範圍")}</span><b>{text(counting)}</b></div>'
            content += render_flow([f'{tier}費升至{guarantee["觸發星級"]}★', f'下次刷新保證{guarantee["每次數量"]}枚同名'], text)
            content += f'<p class="talent-note">{text("升至3★取消未使用保證")}</p>'
        else:
            raise ValueError(f"未知天賦：{name}")
        cards.append(f'<article class="card talent-card"><h3>{text(name)}<span class="tag">{text(route)}</span></h3>'
                     + content + f'<span class="talent-scope">{text(DIFFICULTIES[scope][1])}</span></article>')
    return "".join(cards)


def render_talent_choices(balances, convert_text):
    rows = []
    for scope, balance in balances.items():
        selection = balance["棋手天賦"]
        cells = [DIFFICULTIES[scope][1], "、".join(selection["可選"]), selection["預設"]]
        rows.append('<tr>' + "".join(f'<td>{html_escape(convert_text(cell))}</td>' for cell in cells) + '</tr>')
    return "".join(rows)


def render_equipment_rewards(balances, convert_text):
    def text(value):
        return html_escape(convert_text(str(value)))

    schedules = []
    for scope, balance in balances.items():
        rewards = balance["玩家裝備獎勵"]
        entries = [("全體天賦", entry) for entry in rewards["基本"]]
        for talent, extras in rewards["天賦額外"].items():
            entries.extend((talent, entry) for entry in extras)
        rows = []
        for source, entry in sorted(entries, key=lambda pair: pair[1]["關卡"]):
            cells = [entry["關卡"], source, f'{entry["最低層級"]}～{entry["最高層級"]}',
                     entry["選項數量"], entry["追加選項費用"]]
            rows.append('<tr>' + "".join(f'<td>{text(cell)}</td>' for cell in cells) + '</tr>')
        headers = ["通關後", "適用天賦", "裝備層級", "免費候選", "追加候選價格（金）"]
        schedules.append(
            f'<details class="reward-schedule"><summary>{text(DIFFICULTIES[scope][1])} · {text("裝備獎勵時程")}</summary>'
            '<div class="table-scroll"><table><thead><tr>'
            + "".join(f'<th>{text(header)}</th>' for header in headers)
            + '</tr></thead><tbody>' + "".join(rows) + '</tbody></table></div></details>'
        )
    return "".join(schedules)


def assemble_page(template, body_s, body_t, play_url):
    marker = re.search(r"<!--SBODY-->(.*)<!--EBODY-->", template, re.S)
    assert marker, "模板缺少 SBODY/EBODY 標記"
    final = (template[:marker.start(1)] + '<div id="body-s">' + body_s
             + '</div>\n<div id="body-t" hidden>' + body_t + '</div>' + template[marker.end(1):])
    final = final.replace("%%PLAY_URL%%", html_escape(play_url, quote=True))
    unresolved = re.findall(r"%%[^%]+%%", final)
    if unresolved:
        raise ValueError(f"模板存在未解析欄位：{', '.join(unresolved)}")
    return final


def convert_preserving_placeholders(text: str, converter) -> str:
    parts = re.split(r"(%%[^%]+%%)", text)
    return "".join(part if part.startswith("%%") else converter(part) for part in parts)


def main() -> None:
    parser = argparse.ArgumentParser(description="建構《金群自走棋》宣傳與玩法指南頁面")
    parser.add_argument("--game-dir", type=Path, default=ROOT / "work/game-dev")
    parser.add_argument("--config-dir", type=Path, default=ROOT / "config")
    parser.add_argument("--cli", type=Path, default=ROOT / "x64/Debug/kys_chess_cli.exe")
    parser.add_argument("--output", type=Path, default=ROOT / "金群自走棋.html")
    parser.add_argument("--play-url", default="https://tiexuedanxin.net/kys/kyschess.html")
    args = parser.parse_args()

    game_dir = args.game_dir.resolve()
    config_dir = args.config_dir.resolve()
    output = args.output.resolve()
    heads_dir = game_dir / "resource/head"
    game_db = game_dir / "save/game.db"

    balances = {k: load_yaml(config_dir, v) for k, v in BALANCE_FILES.items()}
    neigong = load_yaml(config_dir, "chess_neigong.yaml")
    pool = load_yaml(config_dir, "chess_pool.yaml")
    pool_easy = load_yaml(config_dir, "chess_pool_easy.yaml")
    combos = load_yaml(config_dir, "chess_combos.yaml")
    equipment = load_yaml(config_dir, "chess_equipment.yaml")
    challenge = load_yaml(config_dir, "chess_challenge.yaml")
    talents = load_yaml(config_dir, "chess_talents.yaml")["棋手天賦"]
    ultimates = load_yaml(config_dir, "chess_magic_effects.yaml")["絕招"]
    catalogs = load_catalog(args.cli.resolve(), game_dir, config_dir,
                            dict.fromkeys(pool["角色"] + pool_easy["角色"]))
    abilities = {ability["magic_id"]: ability for role in catalogs["compact"]["roles"] for ability in role["abilities"]}
    owners = ultimate_owners(catalogs["full"]["roles"])
    ultimate_by_id = {entry["武功"]: entry for entry in ultimates}

    counts = {
        "pool": len(pool["角色"]),
        "pool_easy": len(pool_easy["角色"]),
        "combos": len(combos["羈絆"]),
        "equip": len(equipment["裝備列表"]),
        "neigong": sum(len(t["武功"]) for t in neigong["層級分配"]),
        "challenge": len(challenge["遠征挑戰"]),
        "talents": len(talents),
        "ultimates": len(ultimates),
    }

    stats_data = [
            (counts["pool"], "名群俠棋子"),
            (counts["talents"], "種棋手天賦"),
            (counts["ultimates"], "門特色絕招"),
            (counts["combos"], "種羈絆"),
            (counts["neigong"], "種內功"),
            (counts["equip"], "件裝備"),
            (counts["challenge"], "關遠征挑戰"),
            (f'{balances["nb"]["進度"]["總關卡數"]}~{balances["hb"]["進度"]["總關卡數"]}', "關主線棋局"),
    ]

    html = TEMPLATE.read_text(encoding="utf-8")
    roles, magics = load_role_table(game_db)
    simplified = OpenCC("t2s")
    traditional = OpenCC("s2t")
    id2combo = {}
    for c in combos["羈絆"]:
        for member in c["成員"]:
            id2combo.setdefault(member, []).append(c["名稱"])

    m = re.search(r"<!--SBODY-->(.*)<!--EBODY-->", html, re.S)
    assert m, "模板缺少 SBODY/EBODY 標記"

    def render_language(source, convert_ui_text, convert_db_text, convert_yaml_text):
        def sub_y(match):
            scope, dotted = match.group(1), match.group(2)
            data = {"ng": neigong, "talents": talents, **balances}[scope]
            return html_escape(convert_yaml_text(dig(data, dotted)))

        stats = "".join(
            f'<div class="stat"><b>{num}</b><i>{convert_ui_text(label)}</i></div>'
            for num, label in stats_data
        )
        body = convert_preserving_placeholders(source, convert_ui_text)
        body = body.replace("%%ICON%%", render_icon(convert_ui_text))
        body = re.sub(r"%%SHOT:([\w-]+)%%", lambda match: render_shot(match.group(1)), body)
        body = re.sub(
            r"%%HEAD:(\d+)%%",
            lambda match: render_head(match.group(1), heads_dir, convert_ui_text),
            body,
        )
        body = re.sub(r"%%TIP:(\d+)%%", lambda match: render_tip(
            match.group(1), roles, magics, id2combo, convert_ui_text, convert_db_text, convert_yaml_text
        ), body)
        body = re.sub(r"%%Y:(eb|nb|hb|ng|talents):([\w.]+)%%", sub_y, body)
        body = re.sub(r"%%C:(\w+)%%", lambda match: str(counts[match.group(1)]), body)
        body = re.sub(r"%%ULTIMATE:(\d+)%%", lambda match: render_ultimate(
            ultimate_by_id[int(match.group(1))], abilities, owners, convert_yaml_text), body)
        generated = {
            "STATS": stats,
            "TALENTS": render_talents(talents, balances, convert_yaml_text),
            "TALENT_CHOICES": render_talent_choices(balances, convert_yaml_text),
            "EQUIPMENT_REWARDS": render_equipment_rewards(balances, convert_yaml_text),
            "ULTIMATE_CATALOG": "".join(render_ultimate(entry, abilities, owners, convert_yaml_text) for entry in ultimates),
        }
        return re.sub(r"%%GEN:(\w+)%%", lambda match: generated[match.group(1)], body)

    body_t = render_language(
        m.group(1),
        lambda text: text,
        lambda text: to_traditional(text, traditional, PROMO_TRADITIONAL_REPLACEMENTS),
        lambda text: to_traditional(text, traditional, PROMO_TRADITIONAL_REPLACEMENTS),
    )
    body_s = render_language(m.group(1), simplified.convert, simplified.convert, simplified.convert)

    final = assemble_page(html, body_s, body_t, args.play_url)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(final, encoding="utf-8", newline="\n")
    print(f"OK {output}: {output.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
