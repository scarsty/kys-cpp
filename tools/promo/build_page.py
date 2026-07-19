#!/usr/bin/env python3
"""构建《金群自走棋》宣传单 + 玩法指南单页（简体 / 繁體切换）。

从 tools/promo/page_template.html 生成仓库根目录的 金群自走棋.html：
- %%SHOT:key%%      → tools/promo/shots/key.jpg 的 base64 内嵌（构建时可重新压缩）
- %%HEAD:id%%       → work/game-dev/resource/head/id.webp 角色头像（压缩至 120px webp）
- %%Y:eb|nb|hb:路径%% → config/chess_balance_{easy,normal,hard}.yaml 中的标量
- %%Y:ng:路径%%      → config/chess_neigong.yaml 中的标量
- %%C:pool|pool_easy|combos|equip|neigong|challenge%% → 各配置的条目计数
- %%GEN:STATS%%     → 头版数据速览条（由上述计数生成）

简体正文由 OpenCC s2t 自动转换生成繁體版本，两个版本同页内嵌、前端切换。
用法:  .venv/Scripts/python.exe tools/promo/build_page.py
"""
import base64
import io
import json
import re
import sys
from pathlib import Path

import yaml
from opencc import OpenCC
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
TEMPLATE = ROOT / "tools/promo/page_template.html"
SHOTS_DIR = ROOT / "tools/promo/shots"
HEADS_DIR = ROOT / "work/game-dev/resource/head"
CONFIG = ROOT / "config"
OUT = ROOT / "金群自走棋.html"

SHOT_WIDTH, SHOT_QUALITY = 960, 72
HEAD_SIZE, HEAD_QUALITY = 120, 82

BALANCE_FILES = {
    "eb": "chess_balance_easy.yaml",
    "nb": "chess_balance_normal.yaml",
    "hb": "chess_balance_hard.yaml",
}

# OpenCC s2t 后的统一修正（与游戏界面用字保持一致，修正姓氏误转）
POSTFIX = [("羣", "群"), ("範遙", "范遙"), ("二孃", "二娘"), ("峯", "峰")]


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


def render_head(role_id: str) -> str:
    path = HEADS_DIR / f"{role_id}.webp"
    im = Image.open(path).convert("RGBA")
    im.thumbnail((HEAD_SIZE, HEAD_SIZE), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "WEBP", quality=HEAD_QUALITY)
    return f'<img src="{data_uri("image/webp", buf.getvalue())}" alt="角色头像{role_id}" loading="lazy">'


def load_yaml(name: str):
    return yaml.safe_load((CONFIG / name).read_text(encoding="utf-8"))


def dig(data, dotted: str):
    cur = data
    for part in dotted.split("."):
        cur = cur[part]
    if isinstance(cur, list):
        return "、".join(str(x) for x in cur)
    return str(cur)


def main() -> None:
    balances = {k: load_yaml(v) for k, v in BALANCE_FILES.items()}
    neigong = load_yaml("chess_neigong.yaml")
    pool = load_yaml("chess_pool.yaml")
    pool_easy = load_yaml("chess_pool_easy.yaml")
    combos = load_yaml("chess_combos.yaml")
    equipment = load_yaml("chess_equipment.yaml")
    challenge = load_yaml("chess_challenge.yaml")

    counts = {
        "pool": len(pool["角色"]),
        "pool_easy": len(pool_easy["角色"]),
        "combos": len(combos["羁绊"]),
        "equip": len(equipment["装备列表"]),
        "neigong": sum(len(t["武功"]) for t in neigong["层级分配"]),
        "challenge": len(challenge["遠征挑戰"]),
    }

    stats = "".join(
        f'<div class="stat"><b>{num}</b><i>{label}</i></div>'
        for num, label in [
            (counts["pool"], "名群侠棋子"),
            (counts["combos"], "种羁绊"),
            (counts["neigong"], "种内功"),
            (counts["equip"], "件装备"),
            (counts["challenge"], "关远征挑战"),
            (f'{balances["nb"]["进度"]["总关卡数"]}~{balances["hb"]["进度"]["总关卡数"]}', "关主线棋局"),
        ]
    )

    html = TEMPLATE.read_text(encoding="utf-8")

    def sub_shot(m):
        return render_shot(m.group(1))

    def sub_head(m):
        return render_head(m.group(1))

    def sub_y(m):
        scope, dotted = m.group(1), m.group(2)
        data = neigong if scope == "ng" else balances[scope]
        return dig(data, dotted)

    def sub_c(m):
        return str(counts[m.group(1)])

    body = re.sub(r"%%SHOT:([\w-]+)%%", sub_shot, html)
    body = re.sub(r"%%HEAD:(\d+)%%", sub_head, body)
    body = re.sub(r"%%Y:(eb|nb|hb|ng):([\w.]+)%%", sub_y, body)
    body = re.sub(r"%%C:(\w+)%%", sub_c, body)
    body = body.replace("%%GEN:STATS%%", stats)

    m = re.search(r"<!--SBODY-->(.*)<!--EBODY-->", body, re.S)
    assert m, "模板缺少 SBODY/EBODY 标记"
    body_s = m.group(1)

    cc = OpenCC("s2t")
    body_t = cc.convert(body_s)
    for old, new in POSTFIX:
        body_t = body_t.replace(old, new)

    final = (
        body[: m.start(1)]
        + '<div id="body-s">'
        + body_s
        + '</div>\n<div id="body-t" hidden>'
        + body_t
        + "</div>"
        + body[m.end(1) :]
    )
    OUT.write_text(final, encoding="utf-8", newline="\n")
    print(f"OK {OUT.name}: {OUT.stat().st_size // 1024} KB")


if __name__ == "__main__":
    sys.exit(main())
