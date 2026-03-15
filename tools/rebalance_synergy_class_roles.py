from __future__ import annotations

import argparse
import shutil
import sqlite3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = ROOT / "work" / "game-dev" / "save" / "0.db"

ROLE_UPDATES = {
    54: {
        "name": "袁承志",
        "masteries": {"拳掌": 36, "御剑": 48, "耍刀": 15, "特殊": 15},
        "skills": {"ultimate": 40, "ultimate_power": (820, 860, 900), "normal": 10, "normal_power": (420, 450, 480)},
        "note": "Mixed-tag role: keep the 剑客 identity primary, but add a real 拳师 side skill and meaningful 拳掌 mastery.",
    },
    103: {
        "name": "鳩摩智",
        "masteries": {"拳掌": 46, "御剑": 10, "耍刀": 62, "特殊": 25},
        "skills": {"ultimate": 66, "ultimate_power": (900, 940, 980), "normal": 103, "normal_power": (560, 600, 640)},
        "note": "刀客 tag now drives the primary mastery and primary active while still keeping 龍象般若功 as the off-class backup.",
    },
}

SKILL_COLUMN_PAIRS = (
    ("一星武功1", "一星威力1", "一星武功2", "一星威力2"),
    ("二星武功1", "二星威力1", "二星武功2", "二星威力2"),
    ("三星武功1", "三星威力1", "三星武功2", "三星威力2"),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Apply targeted class-synergy rebalances to current-schema chess roles.")
    parser.add_argument("db_path", nargs="?", default=str(DEFAULT_DB_PATH), help="SQLite DB path")
    parser.add_argument("--apply", action="store_true", help="Write the changes back to the database")
    return parser.parse_args()


def build_role_values(update: dict[str, object]) -> dict[str, int]:
    values = dict(update["masteries"])
    skills = update["skills"]
    for index, (ultimate_magic_col, ultimate_power_col, normal_magic_col, normal_power_col) in enumerate(SKILL_COLUMN_PAIRS):
        values[ultimate_magic_col] = skills["ultimate"]
        values[ultimate_power_col] = skills["ultimate_power"][index]
        values[normal_magic_col] = skills["normal"]
        values[normal_power_col] = skills["normal_power"][index]
    return values


def update_role(cur: sqlite3.Cursor, role_id: int, values: dict[str, int]) -> None:
    assignments = ", ".join(f"[{column}] = ?" for column in values)
    params = list(values.values()) + [role_id]
    cur.execute(f"UPDATE role SET {assignments} WHERE 编号 = ?", params)
    if not cur.rowcount:
        raise RuntimeError(f"missing role row: {role_id}")


def validate_role(cur: sqlite3.Cursor, role_id: int, expected_name: str) -> sqlite3.Row:
    cur.row_factory = sqlite3.Row
    row = cur.execute(
        "SELECT 编号, 名字, 拳掌, 御剑, 耍刀, 特殊, 一星武功1, 一星威力1, 一星武功2, 一星威力2, 二星武功1, 二星威力1, 二星武功2, 二星威力2, 三星武功1, 三星威力1, 三星武功2, 三星威力2 FROM role WHERE 编号 = ?",
        (role_id,),
    ).fetchone()
    if row is None:
        raise RuntimeError(f"missing role row: {role_id}")
    if row["名字"] != expected_name:
        raise RuntimeError(f"role {role_id} expected {expected_name} but found {row['名字']}")
    return row


def print_report(cur: sqlite3.Cursor) -> None:
    for role_id, update in ROLE_UPDATES.items():
        row = validate_role(cur, role_id, update["name"])
        values = build_role_values(update)
        print(f"[{role_id}] {update['name']}")
        print(f"  note     {update['note']}")
        print(
            "  mastery  "
            f"拳 {row['拳掌']}→{values['拳掌']}  "
            f"劍 {row['御剑']}→{values['御剑']}  "
            f"刀 {row['耍刀']}→{values['耍刀']}  "
            f"特 {row['特殊']}→{values['特殊']}"
        )
        print(
            "  skills   "
            f"1★ {row['一星武功1']}:{row['一星威力1']} / {row['一星武功2']}:{row['一星威力2']}"
            f"  ->  {values['一星武功1']}:{values['一星威力1']} / {values['一星武功2']}:{values['一星威力2']}"
        )
        print(
            "           "
            f"2★ {row['二星武功1']}:{row['二星威力1']} / {row['二星武功2']}:{row['二星威力2']}"
            f"  ->  {values['二星武功1']}:{values['二星威力1']} / {values['二星武功2']}:{values['二星威力2']}"
        )
        print(
            "           "
            f"3★ {row['三星武功1']}:{row['三星威力1']} / {row['三星武功2']}:{row['三星威力2']}"
            f"  ->  {values['三星武功1']}:{values['三星威力1']} / {values['三星武功2']}:{values['三星威力2']}"
        )


def main() -> int:
    args = parse_args()
    db_path = Path(args.db_path).resolve()
    if not db_path.exists():
        raise FileNotFoundError(db_path)

    backup_path = db_path.with_suffix(".db.bak-synergy-class")
    if args.apply:
        shutil.copy2(db_path, backup_path)

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    try:
        cur = conn.cursor()
        print(f"DB: {db_path}")
        print_report(cur)
        if not args.apply:
            print("\nAnalyze only; no database changes were written.")
            return 0

        cur.execute("BEGIN")
        for role_id, update in ROLE_UPDATES.items():
            update_role(cur, role_id, build_role_values(update))
            validate_role(cur, role_id, update["name"])
        conn.commit()
        print(f"\nApplied class-synergy rebalance to {len(ROLE_UPDATES)} roles.")
        print(f"Backup written to {backup_path}")
        return 0
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()


if __name__ == "__main__":
    raise SystemExit(main())