#!/usr/bin/env python3
"""建構《金群自走棋》宣傳單 + 玩法指南單頁（簡體 / 繁體切換）。

從 tools/promo/page_template.html 生成自包含的 HTML 頁面：
- %%SHOT:key%%      → tools/promo/shots/key.jpg 的 base64 內嵌（建構時可重新壓縮）
- %%ICON%%          → assets/app_icon.png 的遊戲圖示
- %%HEAD:id%%       → <game-dir>/resource/head/id.webp 角色頭像（壓縮至 120px webp）
- %%TIP:id%%        → game.db 角色四圍/武功 + chess_combos.yaml 羈絆的 hover 懸浮卡片
- %%Y:eb|nb|hb:路徑%% → config/chess_balance_{easy,normal,hard}.yaml 中的純量
- %%Y:ng:路徑%%      → config/chess_neigong.yaml 中的純量
- %%C:pool|pool_easy|combos|equip|neigong|challenge%% → 各設定的條目計數
- %%GEN:STATS%%     → 頭版資料速覽條（由上述計數生成）
- %%PLAY_URL%%      → 網頁版遊戲網址

繁體正文為來源，簡體版本由 OpenCC t2s 自動轉換；兩個版本同頁內嵌、前端切換。
用法：.venv/Scripts/python.exe tools/promo/build_page.py [--game-dir 路徑] [--config-dir 路徑] [--output 路徑] [--play-url 網址]
"""
import argparse
import base64
import io
import json
import re
import sqlite3
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
        name = convert_db_text(magics.get(m, ""))
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
        combo_names = [convert_yaml_text(name) for name in own]
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


def convert_preserving_placeholders(text: str, converter) -> str:
    parts = re.split(r"(%%[^%]+%%)", text)
    return "".join(part if part.startswith("%%") else converter(part) for part in parts)


def main() -> None:
    parser = argparse.ArgumentParser(description="建構《金群自走棋》宣傳與玩法指南頁面")
    parser.add_argument("--game-dir", type=Path, default=ROOT / "work/game-dev")
    parser.add_argument("--config-dir", type=Path, default=ROOT / "config")
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

    counts = {
        "pool": len(pool["角色"]),
        "pool_easy": len(pool_easy["角色"]),
        "combos": len(combos["羈絆"]),
        "equip": len(equipment["裝備列表"]),
        "neigong": sum(len(t["武功"]) for t in neigong["層級分配"]),
        "challenge": len(challenge["遠征挑戰"]),
    }

    stats_data = [
            (counts["pool"], "名群俠棋子"),
            (counts["combos"], "種羈絆"),
            (counts["neigong"], "種內功"),
            (counts["equip"], "件裝備"),
            (counts["challenge"], "關遠征挑戰"),
            (f'{balances["nb"]["进度"]["总关卡数"]}~{balances["hb"]["进度"]["总关卡数"]}', "關主線棋局"),
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
            data = neigong if scope == "ng" else balances[scope]
            return convert_yaml_text(dig(data, dotted))

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
        body = re.sub(r"%%Y:(eb|nb|hb|ng):([\w.]+)%%", sub_y, body)
        body = re.sub(r"%%C:(\w+)%%", lambda match: str(counts[match.group(1)]), body)
        return body.replace("%%GEN:STATS%%", stats)

    body_t = render_language(
        m.group(1),
        lambda text: text,
        lambda text: text,
        lambda text: to_traditional(text, traditional, PROMO_TRADITIONAL_REPLACEMENTS),
    )
    body_s = render_language(m.group(1), simplified.convert, simplified.convert, simplified.convert)

    final = (
        html[: m.start(1)]
        + '<div id="body-s">'
        + body_s
        + '</div>\n<div id="body-t" hidden>'
        + body_t
        + "</div>"
        + html[m.end(1) :]
    )
    final = final.replace("%%PLAY_URL%%", html_escape(args.play_url, quote=True))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(final, encoding="utf-8", newline="\n")
    print(f"OK {output}: {output.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
