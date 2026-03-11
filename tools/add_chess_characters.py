#!/usr/bin/env python3
"""Add the new chess characters defined in the add-more-chess-piece spec."""

from __future__ import annotations

import argparse
import csv
import filecmp
import shutil
import sqlite3
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = REPO_ROOT / "work" / "game-dev" / "save" / "0.db"
DEFAULT_REFERENCE_ROOT = Path(r"D:\games\人在江湖cpp版\人在江湖cpp版\game")
DEFAULT_TARGET_FIGHT_DIR = REPO_ROOT / "work" / "game-dev" / "resource" / "fight"
DEFAULT_TARGET_HEAD_DIR = REPO_ROOT / "work" / "game-dev" / "resource" / "head"

TIER_RANGES = {
    1: {"hp": (200, 400), "atk": (40, 70), "def": (30, 50), "spd": (30, 50)},
    2: {"hp": (350, 550), "atk": (60, 90), "def": (40, 65), "spd": (40, 60)},
    3: {"hp": (500, 750), "atk": (80, 120), "def": (55, 85), "spd": (50, 70)},
    4: {"hp": (650, 950), "atk": (100, 150), "def": (70, 110), "spd": (60, 85)},
    5: {"hp": (900, 1200), "atk": (140, 200), "def": (90, 130), "spd": (75, 100)},
}

ROLE_TYPE_BIAS = {
    "Melee": {"hp": 0.08, "atk": 0.05, "def": 0.06, "spd": -0.04},
    "Ranged": {"hp": -0.05, "atk": 0.03, "def": -0.04, "spd": 0.06},
}

# Character names that already exist in the database will be updated in place.
# New characters will be assigned IDs starting from the current max ID + 1.

GENERIC_SKILLS = {
    "Melee": [(1, 250), (17, 250)],
    "Ranged": [(14, 250), (18, 250)],
}


@dataclass(frozen=True)
class CharacterSpec:
    name: str
    tier: int
    role_type: str
    fallback_name: str | None = None
    aliases: tuple[str, ...] = ()


def make_character_spec(
    name: str,
    tier: int,
    role_type: str,
    fallback_name: str | None = None,
    aliases: tuple[str, ...] = (),
) -> CharacterSpec:
    return CharacterSpec(
        name=name,
        tier=tier,
        role_type=role_type,
        fallback_name=fallback_name,
        aliases=aliases,
    )


TARGET_CHARACTERS: list[CharacterSpec] = [
    make_character_spec("胡一刀", 4, "Melee", fallback_name="胡斐"),
    make_character_spec("田歸農", 1, "Ranged"),
    make_character_spec("閻基", 1, "Ranged"),
    make_character_spec("王元霸", 1, "Melee"),
    make_character_spec("田伯光", 2, "Melee"),
    make_character_spec("東方不敗", 5, "Ranged"),
    make_character_spec("黃裳", 5, "Melee"),
    make_character_spec("張三丰", 5, "Melee"),
    make_character_spec("張無忌", 4, "Melee"),
    make_character_spec("韋一笑", 3, "Melee"),
    make_character_spec("謝遜", 4, "Melee"),
    make_character_spec("周芷若", 3, "Melee"),
    make_character_spec("楊逍", 3, "Ranged"),
    make_character_spec("殷天正", 4, "Melee"),
    make_character_spec("滅絕", 3, "Ranged", aliases=("滅絕師太",)),
    make_character_spec("趙敏", 1, "Ranged"),
    make_character_spec("宋青書", 2, "Melee"),
    make_character_spec("張翠山", 2, "Ranged"),
    make_character_spec("黛綺絲", 2, "Ranged"),
    make_character_spec("范遙", 3, "Ranged"),
    make_character_spec("宋遠橋", 4, "Melee"),
    make_character_spec("張松溪", 3, "Melee"),
    make_character_spec("俞蓮舟", 3, "Ranged"),
    make_character_spec("俞岱巖", 1, "Melee"),
    make_character_spec("殷梨亭", 2, "Melee"),
    make_character_spec("莫聲谷", 2, "Melee"),
    make_character_spec("無色禪師", 4, "Melee", fallback_name="玄慈", aliases=("無色",)),
    make_character_spec("郭襄", 4, "Ranged", fallback_name="程英"),
    make_character_spec("胡斐", 3, "Melee"),
    make_character_spec("程靈素", 1, "Ranged"),
    make_character_spec("平一指", 2, "Melee", fallback_name="胡青牛"),
    make_character_spec("胡青牛", 2, "Ranged"),
    make_character_spec("成崑", 3, "Melee"),
    make_character_spec("岳不群", 3, "Melee"),
    make_character_spec("馬鈺", 1, "Melee"),
    make_character_spec("王處一", 2, "Melee"),
    make_character_spec("譚處端", 1, "Ranged"),
    make_character_spec("劉處玄", 2, "Ranged"),
]


@dataclass
class CharacterPlan:
    spec: CharacterSpec
    reference_row: dict[str, str]
    reference_match: str
    head_id: int
    fight_source_id: int
    role_id: int = 0
    role_action: str = ""
    stats: dict[str, object] | None = None
    skills: list[tuple[int, int]] | None = None
    source_head_file: Path | None = None
    source_fight_file: Path | None = None
    fight_action: str = ""
    head_action: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "db_path",
        nargs="?",
        default=str(DEFAULT_DB_PATH),
        help="SQLite DB path (default: work/game-dev/save/0.db)",
    )
    parser.add_argument(
        "--reference-root",
        default=str(DEFAULT_REFERENCE_ROOT),
        help="Reference game root directory",
    )
    parser.add_argument(
        "--target-fight-dir",
        default=str(DEFAULT_TARGET_FIGHT_DIR),
        help="Target fight resource directory",
    )
    parser.add_argument(
        "--target-head-dir",
        default=str(DEFAULT_TARGET_HEAD_DIR),
        help="Target head resource directory",
    )
    return parser.parse_args()


def int_or_default(value: object, default: int = 0) -> int:
    if value is None:
        return default
    text = str(value).strip()
    if not text:
        return default
    if text.startswith('"') and text.endswith('"'):
        text = text[1:-1]
    try:
        return int(text)
    except ValueError:
        try:
            return int(float(text))
        except ValueError:
            return default


def clamp(value: int, bounds: tuple[int, int]) -> int:
    return max(bounds[0], min(bounds[1], value))


def linear_map(value: int, source_bounds: tuple[int, int], target_bounds: tuple[int, int]) -> int:
    src_min, src_max = source_bounds
    dst_min, dst_max = target_bounds
    if src_max <= src_min:
        return (dst_min + dst_max) // 2
    ratio = (value - src_min) / float(src_max - src_min)
    mapped = dst_min + ratio * (dst_max - dst_min)
    return int(round(mapped))


def pick_existing_path(candidates: Iterable[Path]) -> Path:
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise FileNotFoundError("None of the candidate paths exist:\n  " + "\n  ".join(str(p) for p in candidates))


def get_reference_paths(reference_root: Path) -> tuple[Path, Path, Path, Path]:
    renwu_csv = pick_existing_path(
        [
            reference_root / "1_renwu.csv",
            reference_root / "save" / "CSV" / "1_renwu.csv",
        ]
    )
    wugong_csv = pick_existing_path(
        [
            reference_root / "1_wugong.csv",
            reference_root / "save" / "CSV" / "1_wugong.csv",
        ]
    )
    fight_dir = pick_existing_path([reference_root / "resource" / "fight"])
    head_dir = pick_existing_path([reference_root / "resource" / "head"])
    return renwu_csv, wugong_csv, fight_dir, head_dir


def read_csv_rows(csv_path: Path) -> list[dict[str, str]]:
    with csv_path.open("r", encoding="utf-8-sig", newline="") as handle:
        return list(csv.DictReader(handle))


def build_role_indexes(rows: list[dict[str, str]]) -> tuple[dict[str, list[dict[str, str]]], dict[str, list[dict[str, str]]]]:
    by_name: dict[str, list[dict[str, str]]] = defaultdict(list)
    by_alias: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        name = row.get("姓名", "").strip()
        alias = row.get("外号", "").strip()
        if name:
            by_name[name].append(row)
        if alias:
            by_alias[alias].append(row)
    return by_name, by_alias


def select_best_reference_row(rows: list[dict[str, str]]) -> dict[str, str]:
    if not rows:
        raise ValueError("No rows provided")
    return max(
        rows,
        key=lambda row: (
            int_or_default(row.get("生命最大值")),
            int_or_default(row.get("等级")),
            int_or_default(row.get("编号")),
        ),
    )


def to_simplified(text: str) -> str:
    """Convert traditional Chinese to simplified for CSV lookup."""
    t2s = str.maketrans({
        '張': '张', '無': '无', '忌': '忌', '楊': '杨', '逍': '逍', '謝': '谢', '遜': '逊',
        '韋': '韦', '東': '东', '敗': '败', '黃': '黄', '裳': '裳', '豐': '丰',
        '滅': '灭', '絕': '绝', '師': '师', '趙': '赵', '敏': '敏', '書': '书',
        '翠': '翠', '黛': '黛', '綺': '绮', '絲': '丝', '瑤': '瑶', '遠': '远',
        '橋': '桥', '蓮': '莲', '舟': '舟', '岱': '岱', '巖': '岩', '聲': '声',
        '無': '无', '禪': '禅', '靈': '灵', '馬': '马', '鈺': '钰', '處': '处',
        '譚': '谭', '劉': '刘', '閻': '阎', '歸': '归', '農': '农',
    })
    return text.translate(t2s)


def resolve_reference_role(
    spec: CharacterSpec,
    by_name: dict[str, list[dict[str, str]]],
    by_alias: dict[str, list[dict[str, str]]],
) -> tuple[dict[str, str], str]:
    search_terms = [to_simplified(spec.name), *[to_simplified(a) for a in spec.aliases]]
    for term in search_terms:
        matches = by_name.get(term, [])
        if matches:
            return select_best_reference_row(matches), f"exact:{term}"
        alias_matches = by_alias.get(term, [])
        if alias_matches:
            return select_best_reference_row(alias_matches), f"alias:{term}"
    if spec.fallback_name:
        fallback_terms = [to_simplified(spec.fallback_name)]
        for term in fallback_terms:
            matches = by_name.get(term, [])
            if matches:
                return select_best_reference_row(matches), f"fallback:{term}"
            alias_matches = by_alias.get(term, [])
            if alias_matches:
                return select_best_reference_row(alias_matches), f"fallback-alias:{term}"
    raise KeyError(f"Could not resolve reference role for {spec.name}")


def load_reference_magic(csv_path: Path) -> dict[int, dict[str, str]]:
    rows = read_csv_rows(csv_path)
    return {int_or_default(row.get("编号"), -1): row for row in rows if int_or_default(row.get("编号"), -1) >= 0}


CANONICAL_CUSTOM_MAGIC_NAMES = {
    "药王丹术": "藥王丹術",
    "青囊奇术": "青囊奇術",
    "药王毒手": "藥王毒手",
    "宁式剑诀": "寧式劍訣",
    "无极玄功拳": "無極玄功拳",
}


def canonicalize_magic_name(name: str) -> str:
    stripped = name.strip()
    return CANONICAL_CUSTOM_MAGIC_NAMES.get(stripped, stripped)


def list_table_columns(cur: sqlite3.Cursor, table_name: str) -> list[str]:
    cur.execute(f"PRAGMA table_info([{table_name}])")
    return [row[1] for row in cur.fetchall()]


def ensure_cost_column(cur: sqlite3.Cursor) -> list[str]:
    role_columns = list_table_columns(cur, "role")
    if "费用" not in role_columns:
        cur.execute("ALTER TABLE role ADD COLUMN 费用 INTEGER DEFAULT -1")
        role_columns = list_table_columns(cur, "role")
        print("Added missing 费用 column to role table.")
    return role_columns


def extract_numeric_head_ids(directory: Path) -> set[int]:
    head_ids = set()
    for path in directory.iterdir():
        if path.suffix.lower() != ".png":
            continue
        if path.stem.isdigit():
            head_ids.add(int(path.stem))
    return head_ids


def sync_head_index_if_needed(source_head_dir: Path, target_head_dir: Path, used_head_ids: Iterable[int]) -> str:
    used_head_ids = list(used_head_ids)
    if not used_head_ids:
        return "not-needed"
    current_ids = extract_numeric_head_ids(target_head_dir)
    current_max = max(current_ids) if current_ids else -1
    needed_max = max(used_head_ids)
    source_index = source_head_dir / "index.ka"
    target_index = target_head_dir / "index.ka"
    if needed_max <= current_max:
        return "not-needed"
    if not source_index.exists():
        raise FileNotFoundError(f"Missing source head index: {source_index}")
    return copy_if_changed(source_index, target_index)


def copy_if_changed(source: Path, target: Path) -> str:
    if not source.exists():
        raise FileNotFoundError(f"Missing source file: {source}")
    target.parent.mkdir(parents=True, exist_ok=True)
    existed = target.exists()
    if existed and filecmp.cmp(source, target, shallow=False):
        return "unchanged"
    shutil.copy2(source, target)
    return "updated" if existed else "copied"


def find_source_head_file(source_head_dir: Path, head_id: int) -> Path:
    candidate = source_head_dir / f"{head_id}.png"
    if candidate.exists():
        return candidate
    raise FileNotFoundError(f"Missing source head image for ID {head_id}: {candidate}")


def find_source_fight_file(source_fight_dir: Path, fight_id: int) -> Path:
    exact_candidates = [
        source_fight_dir / f"fight{fight_id:04d}.zip",
        source_fight_dir / f"fight{fight_id:03d}.zip",
        source_fight_dir / f"fight{fight_id}.zip",
    ]
    for candidate in exact_candidates:
        if candidate.exists():
            return candidate
    glob_matches = sorted(source_fight_dir.glob(f"fight{fight_id:04d}*.zip"))
    if glob_matches:
        return glob_matches[0]
    raise FileNotFoundError(f"Missing source fight archive for ID {fight_id}")


def target_fight_filename(head_id: int) -> str:
    return f"fight{head_id:03d}.zip"


def choose_top_skills(
    spec: CharacterSpec,
    reference_row: dict[str, str],
    reference_magic: dict[int, dict[str, str]],
) -> list[tuple[int, int]]:
    seen: dict[int, tuple[int, int, int]] = {}
    for slot in range(1, 40):
        skill_id = int_or_default(reference_row.get(f"所会武功{slot}"))
        if skill_id <= 0:
            continue
        level = int_or_default(reference_row.get(f"所会武功等级{slot}"), 0)
        previous = seen.get(skill_id)
        candidate = (skill_id, level, slot)
        if previous is None or level > previous[1]:
            seen[skill_id] = candidate

    def skill_sort_key(candidate: tuple[int, int, int]) -> tuple[int, int, int]:
        skill_id, level, slot = candidate
        magic_row = reference_magic.get(skill_id, {})
        skill_type = int_or_default(magic_row.get("武功类型"), 1)
        is_active = 0 if skill_type in {1, 2, 3, 4} else 1
        return (is_active, -level, slot)

    ordered = sorted(seen.values(), key=skill_sort_key)
    selected = [
        (skill_id, clamp(level if level > 0 else 250, (150, 900)))
        for skill_id, level, _ in ordered[:2]
    ]
    if len(selected) < 2:
        for fallback_skill in GENERIC_SKILLS[spec.role_type]:
            if fallback_skill[0] not in {skill_id for skill_id, _ in selected}:
                selected.append(fallback_skill)
            if len(selected) == 2:
                break
    return selected


def collect_source_stat_ranges(plans: list[CharacterPlan]) -> dict[int, dict[str, tuple[int, int]]]:
    stat_ranges: dict[int, dict[str, tuple[int, int]]] = {}
    source_key = {
        "hp": "生命最大值",
        "atk": "攻击力",
        "def": "防御力",
        "spd": "轻功",
    }
    for tier in TIER_RANGES:
        tier_ranges: dict[str, tuple[int, int]] = {}
        tier_plans = [plan for plan in plans if plan.spec.tier == tier]
        for stat_name, column_name in source_key.items():
            values = [int_or_default(plan.reference_row.get(column_name)) for plan in tier_plans]
            if values:
                tier_ranges[stat_name] = (min(values), max(values))
            else:
                tier_ranges[stat_name] = (0, 0)
        stat_ranges[tier] = tier_ranges
    return stat_ranges


def build_role_stats(plan: CharacterPlan, source_stat_ranges: dict[int, dict[str, tuple[int, int]]]) -> dict[str, object]:
    tier = plan.spec.tier
    source_row = plan.reference_row
    tier_range = TIER_RANGES[tier]
    source_range = source_stat_ranges[tier]

    stats = {}
    for stat_name, source_column in {
        "hp": "生命最大值",
        "atk": "攻击力",
        "def": "防御力",
        "spd": "轻功",
    }.items():
        mapped = linear_map(
            int_or_default(source_row.get(source_column)),
            source_range[stat_name],
            tier_range[stat_name],
        )
        stats[stat_name] = mapped

    bias = ROLE_TYPE_BIAS[plan.spec.role_type]
    for stat_name, delta_ratio in bias.items():
        stat_min, stat_max = tier_range[stat_name]
        spread = stat_max - stat_min
        biased = stats[stat_name] + int(round(spread * delta_ratio))
        stats[stat_name] = clamp(biased, tier_range[stat_name])

    use_poison = int_or_default(source_row.get("毒术"))
    medicine = int_or_default(source_row.get("医疗"))
    anti_poison = int_or_default(source_row.get("抗毒"))
    unusual = int_or_default(source_row.get("特殊兵器"))
    hidden = int_or_default(source_row.get("暗器技巧"))

    nickname = source_row.get("外号", "").strip()
    if plan.reference_match.startswith("fallback"):
        nickname = plan.spec.name

    return {
        "头像": plan.head_id,
        "名字": plan.spec.name,
        "外号": nickname,
        "性别": int_or_default(source_row.get("性别")),
        "生命最大值": stats["hp"],
        "内力性质": int_or_default(source_row.get("内力性质")),
        "内力最大值": 100,
        "攻击力": stats["atk"],
        "轻功": stats["spd"],
        "防御力": stats["def"],
        "医疗": medicine,
        "用毒": use_poison,
        "解毒": max(medicine, anti_poison // 2),
        "抗毒": anti_poison,
        "拳掌": int_or_default(source_row.get("拳掌")),
        "御剑": int_or_default(source_row.get("御剑")),
        "耍刀": int_or_default(source_row.get("耍刀")),
        "特殊": unusual,
        "暗器": hidden,
        "武学常识": int_or_default(source_row.get("武学常识")),
        "品德": int_or_default(source_row.get("品德")),
        "攻击带毒": 0 if use_poison < 50 else min(50, use_poison // 4),
        "左右互搏": int_or_default(source_row.get("左右互搏")),
        "声望": int_or_default(source_row.get("声望")),
        "资质": int_or_default(source_row.get("资质")),
        "费用": tier,
    }


def build_magic_values(skill_id: int, reference_row: dict[str, str]) -> dict[str, int | str]:
    movement_values = [int_or_default(reference_row.get("移动范围"))]
    movement_values.extend(int_or_default(reference_row.get(f"移动范围{i}")) for i in range(1, 10))
    kill_values = [int_or_default(reference_row.get("杀伤范围"))]
    kill_values.extend(int_or_default(reference_row.get(f"杀伤范围{i}")) for i in range(1, 10))
    add_mp_values = [int_or_default(reference_row.get("加内力"))]
    add_mp_values.extend(int_or_default(reference_row.get(f"加内力{i}")) for i in range(1, 3))
    magic_name = canonicalize_magic_name(reference_row.get("名称", f"引用武功{skill_id}"))
    return {
        "编号": skill_id,
        "名称": magic_name,
        "出招音效": int_or_default(reference_row.get("出招音效")),
        "武功类型": int_or_default(reference_row.get("武功类型")),
        "武功动画": int_or_default(reference_row.get("武功动画&音效")),
        "伤害类型": int_or_default(reference_row.get("内力类型")),
        "攻击范围类型": int_or_default(reference_row.get("攻击范围")),
        "消耗内力": int_or_default(reference_row.get("消耗内力")),
        "敌人中毒": int_or_default(reference_row.get("敌人中毒点数")),
        "移动范围": max(movement_values) if movement_values else 0,
        "杀伤范围": max(kill_values) if kill_values else 0,
        "加内力": max(add_mp_values) if add_mp_values else 0,
        "杀伤内力": 0,
    }


def update_or_insert_row(
    cur: sqlite3.Cursor,
    table_name: str,
    key_column: str,
    key_value: int,
    values: dict[str, object],
) -> str:
    cur.execute(f"SELECT COUNT(*) FROM [{table_name}] WHERE [{key_column}] = ?", (key_value,))
    exists = cur.fetchone()[0] > 0
    filtered_values = {column: value for column, value in values.items() if column != key_column}
    if exists:
        assignments = ", ".join(f"[{column}] = ?" for column in filtered_values)
        cur.execute(
            f"UPDATE [{table_name}] SET {assignments} WHERE [{key_column}] = ?",
            [filtered_values[column] for column in filtered_values] + [key_value],
        )
        return "updated"

    insert_values = {key_column: key_value, **filtered_values}
    columns = ", ".join(f"[{column}]" for column in insert_values)
    placeholders = ", ".join("?" for _ in insert_values)
    cur.execute(
        f"INSERT INTO [{table_name}] ({columns}) VALUES ({placeholders})",
        [insert_values[column] for column in insert_values],
    )
    return "inserted"


def assign_role_ids(cur: sqlite3.Cursor, plans: list[CharacterPlan]) -> None:
    if not plans:
        return

    placeholders = ", ".join("?" for _ in plans)
    cur.execute(
        f"SELECT 编号, 名字 FROM role WHERE 名字 IN ({placeholders})",
        [plan.spec.name for plan in plans],
    )

    existing_by_name: dict[str, int] = {}
    for role_id, name in cur.fetchall():
        normalized_name = (name or "").strip()
        if normalized_name:
            existing_by_name[normalized_name] = int_or_default(role_id)

    cur.execute("SELECT MAX(编号) FROM role")
    next_id = (cur.fetchone()[0] or 0) + 1

    for plan in plans:
        if plan.spec.name in existing_by_name:
            plan.role_id = existing_by_name[plan.spec.name]
            plan.role_action = "update"
        else:
            plan.role_id = next_id
            plan.role_action = "insert"
            next_id += 1


def ensure_magic_rows(
    cur: sqlite3.Cursor,
    magic_columns: list[str],
    reference_magic: dict[int, dict[str, str]],
    skill_ids: Iterable[int],
) -> dict[int, str]:
    cur.execute("SELECT 编号 FROM magic")
    existing_magic_ids = {int_or_default(row[0]) for row in cur.fetchall()}
    actions: dict[int, str] = {}
    for skill_id in sorted(set(skill_ids)):
        if skill_id in existing_magic_ids:
            actions[skill_id] = "existing"
            continue
        reference_row = reference_magic.get(skill_id)
        if reference_row is None:
            values = {
                "编号": skill_id,
                "名称": f"引用武功{skill_id}",
                "出招音效": 0,
                "武功类型": 1,
                "武功动画": 0,
                "伤害类型": 0,
                "攻击范围类型": 0,
                "消耗内力": 0,
                "敌人中毒": 0,
                "移动范围": 0,
                "杀伤范围": 0,
                "加内力": 0,
                "杀伤内力": 0,
            }
        else:
            values = build_magic_values(skill_id, reference_row)
        filtered_values = {column: values[column] for column in values if column in magic_columns}
        actions[skill_id] = update_or_insert_row(cur, "magic", "编号", skill_id, filtered_values)
        existing_magic_ids.add(skill_id)
    return actions


def copy_role_resources(
    plan: CharacterPlan,
    target_fight_dir: Path,
    target_head_dir: Path,
) -> None:
    if plan.source_fight_file is None or plan.source_head_file is None:
        raise ValueError(f"Resource paths not prepared for {plan.spec.name}")

    source_fight = plan.source_fight_file
    target_fight = target_fight_dir / target_fight_filename(plan.head_id)
    plan.fight_action = copy_if_changed(source_fight, target_fight)

    source_head = plan.source_head_file
    target_head = target_head_dir / f"{plan.head_id}.png"
    plan.head_action = copy_if_changed(source_head, target_head)


def build_role_values(plan: CharacterPlan, role_columns: list[str]) -> dict[str, object]:
    if plan.stats is None or plan.skills is None:
        raise ValueError(f"Plan for {plan.spec.name} is incomplete")

    role_values: dict[str, object] = {
        "编号": plan.role_id,
        **plan.stats,
        "一星武功1": plan.skills[0][0] if len(plan.skills) > 0 else 0,
        "一星威力1": plan.skills[0][1] if len(plan.skills) > 0 else 0,
        "一星武功2": plan.skills[1][0] if len(plan.skills) > 1 else 0,
        "一星威力2": plan.skills[1][1] if len(plan.skills) > 1 else 0,
        "二星武功1": plan.skills[0][0] if len(plan.skills) > 0 else 0,
        "二星威力1": clamp((plan.skills[0][1] if len(plan.skills) > 0 else 0) + 75, (0, 900)),
        "二星武功2": plan.skills[1][0] if len(plan.skills) > 1 else 0,
        "二星威力2": clamp((plan.skills[1][1] if len(plan.skills) > 1 else 0) + 75, (0, 900)),
        "三星武功1": plan.skills[0][0] if len(plan.skills) > 0 else 0,
        "三星威力1": clamp((plan.skills[0][1] if len(plan.skills) > 0 else 0) + 150, (0, 900)),
        "三星武功2": plan.skills[1][0] if len(plan.skills) > 1 else 0,
        "三星威力2": clamp((plan.skills[1][1] if len(plan.skills) > 1 else 0) + 150, (0, 900)),
    }

    return {column: value for column, value in role_values.items() if column in role_columns}


def print_summary(
    plans: list[CharacterPlan],
    magic_actions: dict[int, str],
    head_index_action: str,
    renwu_csv: Path,
    wugong_csv: Path,
) -> None:
    print()
    print("Reference role CSV :", renwu_csv)
    print("Reference skill CSV:", wugong_csv)
    print("Head index sync    :", head_index_action)
    print()
    print(f"Processed {len(plans)} chess characters:")
    for plan in plans:
        skill_desc = ", ".join(f"{skill_id}@{power}" for skill_id, power in (plan.skills or []))
        stats = plan.stats or {}
        print(
            f"  [{plan.role_action:7}] ID {plan.role_id:4d} {plan.spec.name:<6} "
            f"tier={plan.spec.tier} head={plan.head_id} fight={plan.fight_source_id} "
            f"ref={plan.reference_match} skills=[{skill_desc}] "
            f"hp={stats.get('生命最大值', 0)} atk={stats.get('攻击力', 0)} "
            f"def={stats.get('防御力', 0)} spd={stats.get('轻功', 0)} "
            f"fight_file={plan.fight_action} head_file={plan.head_action}"
        )

    inserted_magic = sorted(skill_id for skill_id, action in magic_actions.items() if action != "existing")
    print()
    print(f"Magic rows inserted/updated: {len(inserted_magic)}")
    if inserted_magic:
        print("  " + ", ".join(str(skill_id) for skill_id in inserted_magic))


def main() -> int:
    args = parse_args()
    db_path = Path(args.db_path).resolve()
    reference_root = Path(args.reference_root)
    target_fight_dir = Path(args.target_fight_dir)
    target_head_dir = Path(args.target_head_dir)

    if not db_path.exists():
        raise FileNotFoundError(f"DB not found: {db_path}")

    renwu_csv, wugong_csv, source_fight_dir, source_head_dir = get_reference_paths(reference_root)
    reference_role_rows = read_csv_rows(renwu_csv)
    reference_magic = load_reference_magic(wugong_csv)
    by_name, by_alias = build_role_indexes(reference_role_rows)

    plans: list[CharacterPlan] = []
    for spec in TARGET_CHARACTERS:
        reference_row, reference_match = resolve_reference_role(spec, by_name, by_alias)
        head_id = int_or_default(reference_row.get("头像/战斗代号"))
        fight_source_id = int_or_default(reference_row.get("战斗图编号"), head_id)
        if head_id <= 0:
            raise ValueError(f"Invalid head ID for {spec.name}: {head_id}")
        if fight_source_id <= 0:
            fight_source_id = head_id
        plans.append(
            CharacterPlan(
                spec=spec,
                reference_row=reference_row,
                reference_match=reference_match,
                head_id=head_id,
                fight_source_id=fight_source_id,
            )
        )

    source_stat_ranges = collect_source_stat_ranges(plans)
    for plan in plans:
        plan.source_fight_file = find_source_fight_file(source_fight_dir, plan.fight_source_id)
        plan.source_head_file = find_source_head_file(source_head_dir, plan.head_id)
        plan.stats = build_role_stats(plan, source_stat_ranges)
        plan.skills = choose_top_skills(plan.spec, plan.reference_row, reference_magic)

    target_fight_dir.mkdir(parents=True, exist_ok=True)
    target_head_dir.mkdir(parents=True, exist_ok=True)
    head_index_action = sync_head_index_if_needed(source_head_dir, target_head_dir, (plan.head_id for plan in plans))

    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        role_columns = ensure_cost_column(cur)
        magic_columns = list_table_columns(cur, "magic")

        required_role_columns = {
            "编号",
            "头像",
            "名字",
            "外号",
            "性别",
            "生命最大值",
            "内力性质",
            "内力最大值",
            "攻击力",
            "轻功",
            "防御力",
            "医疗",
            "用毒",
            "解毒",
            "抗毒",
            "拳掌",
            "御剑",
            "耍刀",
            "特殊",
            "暗器",
            "武学常识",
            "品德",
            "攻击带毒",
            "左右互搏",
            "声望",
            "资质",
            "一星武功1",
            "一星威力1",
            "一星武功2",
            "一星威力2",
            "二星武功1",
            "二星威力1",
            "二星武功2",
            "二星威力2",
            "三星武功1",
            "三星威力1",
            "三星武功2",
            "三星威力2",
        }
        missing_role_columns = sorted(required_role_columns - set(role_columns))
        if missing_role_columns:
            raise RuntimeError(f"role table missing required columns: {', '.join(missing_role_columns)}")

        assign_role_ids(cur, plans)

        all_skill_ids = [skill_id for plan in plans for skill_id, _ in (plan.skills or []) if skill_id > 0]
        magic_actions = ensure_magic_rows(cur, magic_columns, reference_magic, all_skill_ids)

        for plan in plans:
            copy_role_resources(plan, target_fight_dir, target_head_dir)
            role_values = build_role_values(plan, role_columns)
            action = update_or_insert_row(cur, "role", "编号", plan.role_id, role_values)
            if plan.role_action != "update":
                plan.role_action = action

        conn.commit()
    finally:
        conn.close()

    print_summary(plans, magic_actions, head_index_action, renwu_csv, wugong_csv)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
