#!/usr/bin/env python3
"""Analyze and rebalance the added chess-pool roles in the migrated save DB."""

from __future__ import annotations

import argparse
import re
import sqlite3
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = REPO_ROOT / "work" / "game-dev" / "save" / "0.db"
DEFAULT_POOL_PATH = REPO_ROOT / "config" / "chess_pool.yaml"

STAT_COLUMNS = {
    "hp": "生命最大值",
    "atk": "攻击力",
    "def": "防御力",
    "spd": "轻功",
}

MASTERY_COLUMNS = {
    "Fist": "拳掌",
    "Sword": "御剑",
    "Knife": "耍刀",
    "Unusual": "特殊",
}

SKILL_COLUMNS = [
    ("一星武功1", "一星威力1"),
    ("一星武功2", "一星威力2"),
    ("二星武功1", "二星威力1"),
    ("二星武功2", "二星威力2"),
    ("三星武功1", "三星威力1"),
    ("三星武功2", "三星威力2"),
]

ROLE_TYPE_ADJUSTMENTS = {
    "melee": {"hp": 25, "atk": 5, "def": 5, "spd": -4},
    "ranged": {"hp": -20, "atk": 2, "def": -6, "spd": 5},
    "support": {"hp": -30, "atk": -6, "def": -2, "spd": 3},
}

TAG_ADJUSTMENTS = {
    "tank": {"hp": 60, "atk": 0, "def": 12, "spd": -6},
    "burst": {"hp": -15, "atk": 10, "def": -4, "spd": 2},
    "fast": {"hp": -20, "atk": 3, "def": -4, "spd": 8},
    "support": {"hp": 0, "atk": -4, "def": 4, "spd": 4},
    "healer": {"hp": -10, "atk": -4, "def": -2, "spd": 2},
}

CLAMP_MARGIN = {"hp": 40, "atk": 8, "def": 8, "spd": 8}
PRIMARY_MASTERY = {1: 42, 2: 50, 3: 60, 4: 70, 5: 82}
LOW_MASTERY = {1: 16, 2: 18, 3: 20, 4: 22, 5: 24}
STAR_POWER_SCALES = (0.70, 0.85, 1.00)

CANONICAL_MAGIC_NAME_ALIASES = {
    "药王丹术": "藥王丹術",
    "青囊奇术": "青囊奇術",
    "药王毒手": "藥王毒手",
    "宁式剑诀": "寧式劍訣",
    "无极玄功拳": "無極玄功拳",
}

MAGIC_FIELD_UPDATES = {
    107: {"武功类型": 4},
}


@dataclass(frozen=True)
class RolePlan:
    name: str
    aliases: tuple[str, ...]
    role_type: str
    primary: str
    secondary: str | None
    tags: tuple[str, ...]
    rank: int
    normal_skill: str
    ultimate_skill: str
    normal_power: int
    ultimate_power: int
    note: str = ""


@dataclass
class PoolEntry:
    config_id: int
    yaml_name: str
    tier: int
    plan: RolePlan


def canonicalize_magic_name(name: str) -> str:
    return CANONICAL_MAGIC_NAME_ALIASES.get(name, name)


def plan(
    name: str,
    *,
    aliases: tuple[str, ...] = (),
    role_type: str,
    primary: str,
    secondary: str | None,
    tags: tuple[str, ...],
    rank: int,
    normal_skill: str,
    ultimate_skill: str,
    normal_power: int,
    ultimate_power: int,
    note: str = "",
) -> RolePlan:
    return RolePlan(
        name=name,
        aliases=aliases,
        role_type=role_type,
        primary=primary,
        secondary=secondary,
        tags=tags,
        rank=rank,
        normal_skill=normal_skill,
        ultimate_skill=ultimate_skill,
        normal_power=normal_power,
        ultimate_power=ultimate_power,
        note=note,
    )


TARGET_PLANS = [
    plan("田歸農", role_type="ranged", primary="Sword", secondary="Unusual", tags=("support",), rank=-1, normal_skill="雷震劍法", ultimate_skill="三分劍術", normal_power=380, ultimate_power=760),
    plan("閻基", role_type="ranged", primary="Knife", secondary="Unusual", tags=("support",), rank=-1, normal_skill="鬼頭刀法", ultimate_skill="胡家刀法", normal_power=400, ultimate_power=780),
    plan("王元霸", role_type="melee", primary="Knife", secondary=None, tags=("burst",), rank=0, normal_skill="柴刀十八路", ultimate_skill="五虎斷門刀", normal_power=360, ultimate_power=620),
    plan("趙敏", role_type="ranged", primary="Sword", secondary="Unusual", tags=("support", "fast"), rank=-1, normal_skill="松風劍法", ultimate_skill="滅劍絕劍", normal_power=320, ultimate_power=580, note="Using 松風劍法 as the closest existing substitute for missing 峨眉劍法."),
    plan("俞岱巖", aliases=("俞岱岩",), role_type="melee", primary="Fist", secondary=None, tags=("tank",), rank=-1, normal_skill="綿掌", ultimate_skill="太極拳", normal_power=340, ultimate_power=680),
    plan("程靈素", role_type="support", primary="Unusual", secondary="Fist", tags=("healer", "support"), rank=-1, normal_skill="藥王毒手", ultimate_skill="青囊奇術", normal_power=320, ultimate_power=600),
    plan("馬鈺", role_type="melee", primary="Fist", secondary="Sword", tags=("support",), rank=-1, normal_skill="無極玄功拳", ultimate_skill="一炁化三清", normal_power=350, ultimate_power=660),
    plan("譚處端", role_type="ranged", primary="Sword", secondary=None, tags=("support",), rank=-1, normal_skill="七星劍法", ultimate_skill="一炁化三清", normal_power=340, ultimate_power=640),
    plan("田伯光", role_type="melee", primary="Knife", secondary="Fist", tags=("burst", "fast"), rank=1, normal_skill="狂風刀法", ultimate_skill="玄虛刀法", normal_power=430, ultimate_power=820),
    plan("宋青書", role_type="melee", primary="Fist", secondary="Sword", tags=("burst",), rank=0, normal_skill="綿掌", ultimate_skill="降龍十八掌", normal_power=400, ultimate_power=760),
    plan("張翠山", role_type="ranged", primary="Unusual", secondary="Fist", tags=("burst",), rank=0, normal_skill="綿掌", ultimate_skill="倚天屠龍功", normal_power=410, ultimate_power=760),
    plan("黛綺絲", role_type="ranged", primary="Unusual", secondary="Fist", tags=("support", "fast"), rank=0, normal_skill="鶴蛇八打", ultimate_skill="乾坤大挪移", normal_power=390, ultimate_power=760),
    plan("殷梨亭", role_type="melee", primary="Sword", secondary=None, tags=(), rank=-1, normal_skill="松風劍法", ultimate_skill="柔云劍術", normal_power=360, ultimate_power=620),
    plan("莫聲谷", role_type="melee", primary="Fist", secondary=None, tags=(), rank=-1, normal_skill="綿掌", ultimate_skill="太極拳", normal_power=370, ultimate_power=650),
    plan("平一指", role_type="support", primary="Unusual", secondary="Fist", tags=("healer", "support"), rank=-1, normal_skill="藥王毒手", ultimate_skill="青囊奇術", normal_power=340, ultimate_power=680),
    plan("胡青牛", role_type="support", primary="Unusual", secondary="Fist", tags=("healer", "support"), rank=-1, normal_skill="藥王毒手", ultimate_skill="青囊奇術", normal_power=340, ultimate_power=680),
    plan("王處一", role_type="melee", primary="Fist", secondary="Sword", tags=("tank",), rank=0, normal_skill="無極玄功拳", ultimate_skill="一炁化三清", normal_power=400, ultimate_power=760),
    plan("劉處玄", role_type="ranged", primary="Unusual", secondary="Sword", tags=("support",), rank=0, normal_skill="中平槍法", ultimate_skill="伏魔杖法", normal_power=390, ultimate_power=740, note="Shifted to a 奇门 loadout to match the 奇门 synergy tag."),
    plan("韋一笑", role_type="melee", primary="Fist", secondary=None, tags=("burst", "fast"), rank=1, normal_skill="逍遙游", ultimate_skill="寒冰綿掌", normal_power=460, ultimate_power=900),
    plan("周芷若", role_type="melee", primary="Unusual", secondary="Fist", tags=("burst",), rank=0, normal_skill="毒龍鞭法", ultimate_skill="九陰白骨爪", normal_power=420, ultimate_power=840),
    plan("楊逍", role_type="ranged", primary="Fist", secondary="Unusual", tags=("burst",), rank=1, normal_skill="彈指神通", ultimate_skill="乾坤大挪移", normal_power=450, ultimate_power=880),
    plan("滅絕", aliases=("滅絕師太",), role_type="ranged", primary="Sword", secondary=None, tags=("burst",), rank=0, normal_skill="松風劍法", ultimate_skill="滅劍絕劍", normal_power=420, ultimate_power=840, note="Using 松風劍法 as the closest existing substitute for missing 峨眉劍法."),
    plan("范遙", aliases=("范瑤",), role_type="ranged", primary="Knife", secondary="Sword", tags=("burst",), rank=0, normal_skill="反兩儀刀法", ultimate_skill="火焰刀法", normal_power=450, ultimate_power=900, note="Shifted off sword skills so the 刀客 tag now matches both weapon type and mastery."),
    plan("張松溪", role_type="melee", primary="Sword", secondary="Fist", tags=("support",), rank=0, normal_skill="綿掌", ultimate_skill="太極劍法", normal_power=400, ultimate_power=820),
    plan("俞蓮舟", role_type="melee", primary="Fist", secondary="Sword", tags=("support",), rank=0, normal_skill="綿掌", ultimate_skill="太極拳", normal_power=410, ultimate_power=840, note="Shifted to拳掌 skills to match the 拳师 synergy tag."),
    plan("胡斐", role_type="melee", primary="Knife", secondary="Sword", tags=("burst", "fast"), rank=1, normal_skill="狂風刀法", ultimate_skill="胡家刀法", normal_power=450, ultimate_power=900),
    plan("成崑", aliases=("成昆",), role_type="melee", primary="Fist", secondary=None, tags=("burst",), rank=1, normal_skill="幻陰指", ultimate_skill="混元功", normal_power=430, ultimate_power=860),
    plan("岳不群", role_type="melee", primary="Sword", secondary="Fist", tags=("burst", "support"), rank=1, normal_skill="寧式劍訣", ultimate_skill="辟邪劍法", normal_power=430, ultimate_power=880),
    plan("胡一刀", role_type="melee", primary="Knife", secondary=None, tags=("burst", "tank"), rank=1, normal_skill="狂風刀法", ultimate_skill="胡家刀法", normal_power=500, ultimate_power=980),
    plan("張無忌", role_type="melee", primary="Fist", secondary="Unusual", tags=("tank",), rank=1, normal_skill="七傷拳", ultimate_skill="九陽神功", normal_power=500, ultimate_power=960),
    plan("謝遜", role_type="melee", primary="Fist", secondary="Knife", tags=("burst", "tank"), rank=1, normal_skill="七傷拳", ultimate_skill="獅子吼", normal_power=480, ultimate_power=1000, note="Canonicalized 狮吼功 to 獅子吼."),
    plan("殷天正", role_type="melee", primary="Fist", secondary="Unusual", tags=("tank",), rank=0, normal_skill="鷹爪功", ultimate_skill="羅漢伏魔功", normal_power=450, ultimate_power=860),
    plan("宋遠橋", role_type="melee", primary="Fist", secondary=None, tags=("tank", "support"), rank=0, normal_skill="綿掌", ultimate_skill="太極拳", normal_power=430, ultimate_power=840),
    plan("無色禪師", aliases=("無色",), role_type="melee", primary="Fist", secondary=None, tags=("burst", "tank"), rank=1, normal_skill="大金剛掌", ultimate_skill="須彌山神掌", normal_power=500, ultimate_power=980),
    plan("郭襄", role_type="ranged", primary="Sword", secondary="Unusual", tags=("burst", "fast"), rank=0, normal_skill="玉簫劍法", ultimate_skill="滅劍絕劍", normal_power=460, ultimate_power=940, note="Using 玉簫劍法 as the closest existing substitute for missing 峨眉劍法."),
    plan("東方不敗", role_type="ranged", primary="Sword", secondary="Unusual", tags=("burst", "fast"), rank=1, normal_skill="辟邪劍法", ultimate_skill="葵花神功", normal_power=540, ultimate_power=1100),
    plan("黃裳", role_type="melee", primary="Unusual", secondary="Fist", tags=("burst",), rank=1, normal_skill="裴將軍帖", ultimate_skill="九陰神功", normal_power=540, ultimate_power=1080, note="Retags 九陰神功 to 奇门 for its sole user and pairs it with a 奇门 side skill to match the 奇门 synergy tag."),
    plan("張三丰", aliases=("張三豐",), role_type="melee", primary="Fist", secondary="Sword", tags=("tank", "support"), rank=1, normal_skill="太極拳", ultimate_skill="太極劍法", normal_power=520, ultimate_power=1060, note="Canonicalized 太極劍 to 太極劍法."),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("db_path", nargs="?", default=str(DEFAULT_DB_PATH), help="SQLite DB path")
    parser.add_argument("--pool-path", default=str(DEFAULT_POOL_PATH), help="Path to chess_pool.yaml")
    parser.add_argument("--apply", action="store_true", help="Write the rebalance changes back to the database")
    return parser.parse_args()


def build_plan_lookup() -> dict[str, RolePlan]:
    lookup: dict[str, RolePlan] = {}
    for role_plan in TARGET_PLANS:
        for name in (role_plan.name, *role_plan.aliases):
            lookup[name] = role_plan
    return lookup


def parse_pool(path: Path) -> tuple[list[PoolEntry], dict[int, list[int]]]:
    text = path.read_text(encoding="utf-8")
    lookup = build_plan_lookup()
    current_tier: int | None = None
    selected: list[PoolEntry] = []
    tier_to_ids: dict[int, list[int]] = defaultdict(list)
    seen: set[str] = set()
    for line in text.splitlines():
        tier_match = re.match(r"\s*(?:#|-)\s*费用:\s*(\d+)", line)
        if tier_match:
            current_tier = int(tier_match.group(1))
            continue
        role_match = re.match(r"\s*-\s*(\d+)\s*#\s*(.+?)\s*$", line)
        if not role_match or current_tier is None:
            continue
        role_id = int(role_match.group(1))
        yaml_name = role_match.group(2).strip()
        tier_to_ids[current_tier].append(role_id)
        role_plan = lookup.get(yaml_name)
        if role_plan and role_plan.name not in seen:
            selected.append(PoolEntry(role_id, yaml_name, current_tier, role_plan))
            seen.add(role_plan.name)
    missing = [role_plan.name for role_plan in TARGET_PLANS if role_plan.name not in seen]
    if missing:
        raise ValueError(f"Missing target roles in pool config: {', '.join(missing)}")
    return selected, tier_to_ids


def load_magic_index(cur: sqlite3.Cursor) -> tuple[dict[int, sqlite3.Row], dict[str, list[sqlite3.Row]]]:
    by_id: dict[int, sqlite3.Row] = {}
    by_name: dict[str, list[sqlite3.Row]] = defaultdict(list)
    for row in cur.execute("SELECT 编号, 名称, 武功类型 FROM magic ORDER BY 编号"):
        by_id[row["编号"]] = row
        raw_name = row["名称"]
        by_name[raw_name].append(row)
        canonical_name = canonicalize_magic_name(raw_name)
        if canonical_name != raw_name:
            by_name[canonical_name].append(row)
    return by_id, by_name


def resolve_magic_id(name: str, magic_by_name: dict[str, list[sqlite3.Row]]) -> int:
    rows = magic_by_name.get(canonicalize_magic_name(name), [])
    if not rows:
        raise KeyError(f"Magic '{name}' does not exist in the current DB")
    return min(row["编号"] for row in rows)


def resolve_role_row(cur: sqlite3.Cursor, entry: PoolEntry) -> tuple[sqlite3.Row, list[str]]:
    warnings: list[str] = []
    expected_names = (entry.plan.name, *entry.plan.aliases)
    row = cur.execute("SELECT * FROM role WHERE 编号 = ?", (entry.config_id,)).fetchone()
    if row and row["名字"] in expected_names:
        return row, warnings

    candidates: list[sqlite3.Row] = []
    seen_ids: set[int] = set()
    for name in expected_names:
        for candidate in cur.execute("SELECT * FROM role WHERE 名字 = ? ORDER BY 编号", (name,)).fetchall():
            if candidate["编号"] not in seen_ids:
                candidates.append(candidate)
                seen_ids.add(candidate["编号"])
    if not candidates:
        raise KeyError(f"Unable to resolve role '{entry.plan.name}' from pool ID {entry.config_id}")

    chosen = next((candidate for candidate in candidates if candidate["名字"] == entry.plan.name), None)
    if chosen is None:
        chosen = max(candidates, key=lambda candidate: candidate["编号"])

    if chosen["编号"] != entry.config_id:
        warnings.append(
            f"Pool ID {entry.config_id} ({entry.yaml_name}) is missing in role; using DB row {chosen['编号']} ({chosen['名字']}) instead."
        )
    return chosen, warnings


def average(values: list[int]) -> float:
    return sum(values) / float(len(values))


def build_peer_stats(cur: sqlite3.Cursor, tier_to_ids: dict[int, list[int]], target_config_ids: set[int]) -> dict[int, dict[str, object]]:
    peer_stats: dict[int, dict[str, object]] = {}
    for tier, role_ids in tier_to_ids.items():
        peer_ids = [role_id for role_id in role_ids if role_id not in target_config_ids]
        rows = [
            cur.execute(
                "SELECT 生命最大值, 攻击力, 防御力, 轻功 FROM role WHERE 编号 = ?",
                (role_id,),
            ).fetchone()
            for role_id in peer_ids
        ]
        rows = [row for row in rows if row is not None]
        if not rows:
            raise ValueError(f"No peer rows available for tier {tier}")
        tier_info: dict[str, object] = {"avg": {}, "range": {}}
        for key, column in STAT_COLUMNS.items():
            values = [int(row[column]) for row in rows]
            tier_info["avg"][key] = average(values)
            tier_info["range"][key] = (min(values), max(values))
        peer_stats[tier] = tier_info
    return peer_stats


def clamp(value: int, lower: int, upper: int) -> int:
    return max(lower, min(upper, value))


def round_to(value: float, step: int) -> int:
    return int(round(value / step) * step)


def apply_adjustments(values: dict[str, float], adjustments: dict[str, int]) -> None:
    for key, delta in adjustments.items():
        values[key] += delta


def derive_stats(role_plan: RolePlan, tier: int, peer_stats: dict[int, dict[str, object]]) -> dict[str, int]:
    tier_info = peer_stats[tier]
    stats = {key: float(tier_info["avg"][key]) for key in STAT_COLUMNS}
    apply_adjustments(stats, ROLE_TYPE_ADJUSTMENTS[role_plan.role_type])
    for tag in role_plan.tags:
        apply_adjustments(stats, TAG_ADJUSTMENTS[tag])
    step = {
        "hp": 20 + tier * 5,
        "atk": 3 + tier,
        "def": 3 + tier,
        "spd": 2 + tier,
    }
    for key in stats:
        stats[key] += step[key] * role_plan.rank
    derived: dict[str, int] = {}
    for key in stats:
        lower, upper = tier_info["range"][key]
        value = int(round(stats[key]))
        value = clamp(value, lower - CLAMP_MARGIN[key], upper + CLAMP_MARGIN[key])
        derived[key] = round_to(value, 5) if key == "hp" else value
    return derived


def derive_masteries(role_plan: RolePlan, tier: int) -> dict[str, int]:
    primary_value = PRIMARY_MASTERY[tier] + role_plan.rank * 3
    if "burst" in role_plan.tags:
        primary_value += 2
    if "tank" in role_plan.tags:
        primary_value += 2
    if "healer" in role_plan.tags:
        primary_value -= 2
    primary_value = clamp(primary_value, 30, 95)

    low_value = LOW_MASTERY[tier] + (2 if role_plan.role_type == "support" else 0)
    values = {name: low_value for name in MASTERY_COLUMNS}
    if role_plan.secondary:
        values[role_plan.secondary] = max(values[role_plan.secondary], primary_value - 12)
    values[role_plan.primary] = primary_value
    return values


def build_star_powers(final_power: int) -> list[int]:
    values = [round_to(final_power * scale, 5) for scale in STAR_POWER_SCALES]
    for index in range(1, len(values)):
        if values[index] <= values[index - 1]:
            values[index] = values[index - 1] + 5
    return values


def extract_skill_pairs(role_row: sqlite3.Row, magic_by_id: dict[int, sqlite3.Row]) -> list[tuple[str, int, str, int]]:
    pairs: list[tuple[str, int, str, int]] = []
    for magic_col_1, power_col_1, magic_col_2, power_col_2 in (
        ("一星武功1", "一星威力1", "一星武功2", "一星威力2"),
        ("二星武功1", "二星威力1", "二星武功2", "二星威力2"),
        ("三星武功1", "三星威力1", "三星武功2", "三星威力2"),
    ):
        id1 = int(role_row[magic_col_1])
        id2 = int(role_row[magic_col_2])
        name1 = magic_by_id[id1]["名称"] if id1 in magic_by_id else f"missing:{id1}"
        name2 = magic_by_id[id2]["名称"] if id2 in magic_by_id else f"missing:{id2}"
        pairs.append((name1, int(role_row[power_col_1]), name2, int(role_row[power_col_2])))
    return pairs


def format_skill_pairs(pairs: list[tuple[str, int, str, int]]) -> str:
    parts = []
    for index, (name1, power1, name2, power2) in enumerate(pairs, start=1):
        parts.append(f"{index}★[{name1} {power1} / {name2} {power2}]")
    return " ".join(parts)


def build_after_snapshot(role_plan: RolePlan, tier: int, role_row: sqlite3.Row, magic_by_name: dict[str, list[sqlite3.Row]], peer_stats: dict[int, dict[str, object]]) -> dict[str, object]:
    stats = derive_stats(role_plan, tier, peer_stats)
    masteries = derive_masteries(role_plan, tier)
    normal_magic_id = resolve_magic_id(role_plan.normal_skill, magic_by_name)
    ultimate_magic_id = resolve_magic_id(role_plan.ultimate_skill, magic_by_name)
    normal_powers = [role_plan.normal_power] * 3
    ultimate_powers = [role_plan.ultimate_power] * 3
    update_values = {
        STAT_COLUMNS["hp"]: stats["hp"],
        STAT_COLUMNS["atk"]: stats["atk"],
        STAT_COLUMNS["def"]: stats["def"],
        STAT_COLUMNS["spd"]: stats["spd"],
        MASTERY_COLUMNS["Fist"]: masteries["Fist"],
        MASTERY_COLUMNS["Sword"]: masteries["Sword"],
        MASTERY_COLUMNS["Knife"]: masteries["Knife"],
        MASTERY_COLUMNS["Unusual"]: masteries["Unusual"],
        "一星武功1": ultimate_magic_id,
        "一星威力1": ultimate_powers[0],
        "一星武功2": normal_magic_id,
        "一星威力2": normal_powers[0],
        "二星武功1": ultimate_magic_id,
        "二星威力1": ultimate_powers[1],
        "二星武功2": normal_magic_id,
        "二星威力2": normal_powers[1],
        "三星武功1": ultimate_magic_id,
        "三星威力1": ultimate_powers[2],
        "三星武功2": normal_magic_id,
        "三星威力2": normal_powers[2],
    }
    after_pairs = [
        (role_plan.ultimate_skill, ultimate_powers[0], role_plan.normal_skill, normal_powers[0]),
        (role_plan.ultimate_skill, ultimate_powers[1], role_plan.normal_skill, normal_powers[1]),
        (role_plan.ultimate_skill, ultimate_powers[2], role_plan.normal_skill, normal_powers[2]),
    ]
    changed_fields = sum(1 for column, value in update_values.items() if int(role_row[column]) != int(value))
    return {
        "values": update_values,
        "after_pairs": after_pairs,
        "changed_fields": changed_fields,
    }


def update_role(conn: sqlite3.Connection, role_id: int, values: dict[str, int]) -> None:
    columns = list(values.keys())
    assignments = ", ".join(f"{column} = ?" for column in columns)
    conn.execute(
        f"UPDATE role SET {assignments} WHERE 编号 = ?",
        [values[column] for column in columns] + [role_id],
    )


def update_magic_fields(conn: sqlite3.Connection) -> None:
    for magic_id, fields in MAGIC_FIELD_UPDATES.items():
        assignments = ", ".join(f"[{column}] = ?" for column in fields)
        result = conn.execute(
            f"UPDATE magic SET {assignments} WHERE 编号 = ?",
            list(fields.values()) + [magic_id],
        )
        if not result.rowcount:
            raise RuntimeError(f"Missing magic row for override: {magic_id}")


def main() -> int:
    args = parse_args()
    db_path = Path(args.db_path).resolve()
    pool_path = Path(args.pool_path).resolve()
    entries, tier_to_ids = parse_pool(pool_path)

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()
    magic_by_id, magic_by_name = load_magic_index(cur)
    peer_stats = build_peer_stats(cur, tier_to_ids, {entry.config_id for entry in entries})

    warnings: list[str] = []
    reports: list[tuple[PoolEntry, sqlite3.Row, dict[str, object]]] = []
    total_changed_fields = 0
    for entry in entries:
        role_row, role_warnings = resolve_role_row(cur, entry)
        warnings.extend(role_warnings)
        after_snapshot = build_after_snapshot(entry.plan, entry.tier, role_row, magic_by_name, peer_stats)
        total_changed_fields += int(after_snapshot["changed_fields"])
        reports.append((entry, role_row, after_snapshot))

    print(f"DB: {db_path}")
    print(f"Pool: {pool_path}")
    print(f"Targets resolved: {len(reports)} roles")
    if warnings:
        print("Warnings:")
        for warning in warnings:
            print(f"  - {warning}")
    print(f"Total changed fields if applied: {total_changed_fields}")
    print()

    for entry, role_row, after_snapshot in reports:
        after_values = after_snapshot["values"]
        db_label = f"cfg {entry.config_id}"
        if int(role_row["编号"]) != entry.config_id:
            db_label += f" -> db {role_row['编号']}"
        print(f"\n[{entry.tier}费] {db_label} {entry.plan.name}")
        if entry.plan.note:
            print(f"  note     {entry.plan.note}")
        print(
            "  stats    "
            f"HP {role_row['生命最大值']}→{after_values['生命最大值']}  "
            f"ATK {role_row['攻击力']}→{after_values['攻击力']}  "
            f"DEF {role_row['防御力']}→{after_values['防御力']}  "
            f"SPD {role_row['轻功']}→{after_values['轻功']}"
        )
        print(
            "  mastery  "
            f"拳 {role_row['拳掌']}→{after_values['拳掌']}  "
            f"劍 {role_row['御剑']}→{after_values['御剑']}  "
            f"刀 {role_row['耍刀']}→{after_values['耍刀']}  "
            f"特 {role_row['特殊']}→{after_values['特殊']}"
        )
        before_pairs = extract_skill_pairs(role_row, magic_by_id)
        print(f"  before   {format_skill_pairs(before_pairs)}")
        print(f"  after    {format_skill_pairs(after_snapshot['after_pairs'])}")

    if args.apply:
        update_magic_fields(conn)
        for _, role_row, after_snapshot in reports:
            update_role(conn, int(role_row["编号"]), after_snapshot["values"])
        conn.commit()
        print(f"\nApplied rebalance to {len(reports)} roles.")
    else:
        print("\nAnalyze only; no database changes were written.")

    conn.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
