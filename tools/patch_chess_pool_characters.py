#!/usr/bin/env python3
"""Patch the requested chess-pool characters in game.db and emit equivalent SQL."""

from __future__ import annotations

import argparse
import sqlite3
import sys
from dataclasses import dataclass
from pathlib import Path


if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")


REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = REPO_ROOT / "work" / "game-dev" / "save" / "game.db"
DEFAULT_SQL_PATH = REPO_ROOT / "tools" / "chess_pool_character_patch.sql"

ROLE_COLUMNS = [
    "编号",
    "头像",
    "名字",
    "外号",
    "费用",
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
]

IDX = {name: idx for idx, name in enumerate(ROLE_COLUMNS)}
SKILL_SLOT_COLUMNS = [
    ("一星武功1", "一星威力1"),
    ("一星武功2", "一星威力2"),
    ("二星武功1", "二星威力1"),
    ("二星武功2", "二星威力2"),
    ("三星武功1", "三星威力1"),
    ("三星武功2", "三星威力2"),
]


@dataclass(frozen=True)
class RoleSpec:
    role_id: int
    name: str
    cost: int
    gender: int
    max_hp: int
    attack: int
    speed: int
    defence: int
    ultimate_magic_id: int
    ultimate_power: int
    normal_magic_id: int
    normal_power: int
    template_id: int | None = None
    nickname: str | None = None


ROLE_SPECS = [
    RoleSpec(58, "楊過", 4, 0, 840, 148, 80, 118, 25, 960, 45, 420),
    RoleSpec(56, "黃蓉", 2, 1, 620, 80, 60, 60, 80, 860, 12, 420),
    RoleSpec(76, "王語嫣", 3, 1, 690, 108, 78, 76, 72, 830, 37, 390),
    RoleSpec(38, "石破天", 4, 0, 900, 156, 72, 132, 102, 960, 81, 420),
    RoleSpec(75, "陳家洛", 2, 0, 618, 82, 62, 56, 40, 780, 37, 380),
    RoleSpec(3, "苗人鳳", 3, 0, 760, 120, 74, 94, 44, 860, 67, 420),
    RoleSpec(77, "蕭中慧", 2, 1, 610, 80, 58, 54, 62, 780, 55, 380),
    RoleSpec(35, "令狐沖", 4, 0, 850, 150, 92, 120, 47, 940, 39, 420, nickname="華山浪子"),
    RoleSpec(612, "韋小寶", 2, 0, 600, 72, 65, 48, 79, 780, 72, 380, template_id=115, nickname="鹿鼎奇俠"),
    RoleSpec(613, "袁冠南", 2, 0, 620, 82, 60, 55, 62, 800, 55, 380, template_id=54, nickname="鴛鴦刀客"),
    RoleSpec(614, "李文秀", 1, 1, 520, 58, 52, 42, 39, 750, 12, 350, template_id=63, nickname="大漠俠女"),
]

STALE_ROLE_IDS = [615]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("db_path", nargs="?", default=str(DEFAULT_DB_PATH), help="Path to game.db")
    parser.add_argument("--sql-out", default=str(DEFAULT_SQL_PATH), help="Where to write the generated SQL patch")
    parser.add_argument("--apply", action="store_true", help="Apply the patch to the database")
    return parser.parse_args()


def quote_sql(value: object) -> str:
    if value is None:
        return "NULL"
    if isinstance(value, str):
        return "'" + value.replace("'", "''") + "'"
    return str(value)


def fetch_row(cur: sqlite3.Cursor, role_id: int) -> list[object] | None:
    row = cur.execute("SELECT * FROM role WHERE 编号 = ?", (role_id,)).fetchone()
    return list(row) if row else None


def assign_skills(row: list[object], spec: RoleSpec) -> None:
    repeated = [
        (spec.ultimate_magic_id, spec.ultimate_power),
        (spec.normal_magic_id, spec.normal_power),
    ] * 3
    for (magic_col, power_col), (magic_id, power) in zip(SKILL_SLOT_COLUMNS, repeated):
        row[IDX[magic_col]] = magic_id
        row[IDX[power_col]] = power


def build_patched_row(base_row: list[object], spec: RoleSpec) -> list[object]:
    row = list(base_row)
    row[IDX["编号"]] = spec.role_id
    row[IDX["名字"]] = spec.name
    if spec.nickname is not None:
        row[IDX["外号"]] = spec.nickname
    row[IDX["费用"]] = spec.cost
    row[IDX["性别"]] = spec.gender
    row[IDX["生命最大值"]] = spec.max_hp
    row[IDX["内力最大值"]] = 100
    row[IDX["攻击力"]] = spec.attack
    row[IDX["轻功"]] = spec.speed
    row[IDX["防御力"]] = spec.defence
    assign_skills(row, spec)
    return row


def diff_columns(before: list[object], after: list[object]) -> list[tuple[str, object]]:
    changes: list[tuple[str, object]] = []
    for idx, column in enumerate(ROLE_COLUMNS):
        if column == "编号":
            continue
        if before[idx] != after[idx]:
            changes.append((column, after[idx]))
    return changes


def render_update_sql(role_id: int, changes: list[tuple[str, object]]) -> str:
    assignments = ", ".join(f"[{column}] = {quote_sql(value)}" for column, value in changes)
    return f"UPDATE role SET {assignments} WHERE [编号] = {role_id};"


def render_insert_sql(row: list[object]) -> str:
    columns = ", ".join(f"[{column}]" for column in ROLE_COLUMNS)
    values = ", ".join(quote_sql(value) for value in row)
    return f"INSERT INTO role ({columns}) VALUES ({values});"


def apply_update(cur: sqlite3.Cursor, role_id: int, changes: list[tuple[str, object]]) -> None:
    assignments = ", ".join(f"[{column}] = ?" for column, _ in changes)
    values = [value for _, value in changes]
    cur.execute(f"UPDATE role SET {assignments} WHERE [编号] = ?", [*values, role_id])


def apply_insert(cur: sqlite3.Cursor, row: list[object]) -> None:
    placeholders = ", ".join("?" for _ in ROLE_COLUMNS)
    columns = ", ".join(f"[{column}]" for column in ROLE_COLUMNS)
    cur.execute(f"INSERT INTO role ({columns}) VALUES ({placeholders})", row)


def render_delete_sql(role_id: int) -> str:
    return f"DELETE FROM role WHERE [编号] = {role_id};"


def apply_delete(cur: sqlite3.Cursor, role_id: int) -> None:
    cur.execute("DELETE FROM role WHERE [编号] = ?", (role_id,))


def main() -> int:
    args = parse_args()
    db_path = Path(args.db_path)
    sql_out = Path(args.sql_out)

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    sql_lines = ["BEGIN;"]
    summaries: list[str] = []

    for spec in ROLE_SPECS:
        current_row = fetch_row(cur, spec.role_id)
        if current_row is None:
            if spec.template_id is None:
                raise RuntimeError(f"Missing existing role {spec.role_id} {spec.name} and no template is configured")
            template_row = fetch_row(cur, spec.template_id)
            if template_row is None:
                raise RuntimeError(f"Missing template role {spec.template_id} for {spec.name}")
            new_row = build_patched_row(template_row, spec)
            sql_lines.append(render_insert_sql(new_row))
            summaries.append(f"INSERT {spec.role_id} {spec.name}")
            if args.apply:
                apply_insert(cur, new_row)
            continue

        new_row = build_patched_row(current_row, spec)
        changes = diff_columns(current_row, new_row)
        if not changes:
            summaries.append(f"SKIP   {spec.role_id} {spec.name}")
            continue

        sql_lines.append(render_update_sql(spec.role_id, changes))
        summaries.append(f"UPDATE {spec.role_id} {spec.name} ({len(changes)} fields)")
        if args.apply:
            apply_update(cur, spec.role_id, changes)

    for stale_role_id in STALE_ROLE_IDS:
        stale_row = fetch_row(cur, stale_role_id)
        if stale_row is None:
            continue
        sql_lines.append(render_delete_sql(stale_role_id))
        summaries.append(f"DELETE {stale_role_id}")
        if args.apply:
            apply_delete(cur, stale_role_id)

    sql_lines.append("COMMIT;")
    sql_text = "\n".join(sql_lines) + "\n"
    sql_out.parent.mkdir(parents=True, exist_ok=True)
    sql_out.write_text(sql_text, encoding="utf-8")

    if args.apply:
        conn.commit()
    else:
        conn.rollback()
    conn.close()

    print(f"SQL patch written to: {sql_out}")
    for summary in summaries:
        print(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
