#!/usr/bin/env python3
"""
Database migration script for Chess Role Mod.

Fully restructures save DB to match the new chess-mod schema:

1. Role table:
   - Drops deprecated columns (等级, 经验, 生命成长, etc.)
   - Picks top 2 skills by power and assigns them to ALL star tiers
   - Interleaved column order: 一星武功1, 一星威力1, 一星武功2, 一星威力2, ...
   - Creates star-gated magic slots from old data

2. Magic table:
   - Drops 未知1-5, all level array columns (威力2-10 etc.)
   - Renames 威力1 → 威力, 移动范围1 → 移动范围, etc.

Usage:
    python migrate_chess_save.py <db_path>
    python migrate_chess_save.py <db_path> --dry-run
    python migrate_chess_save.py work/game-dev/save/0.db
"""

import sqlite3
import sys
import os
import re
import shutil
from pathlib import Path


TIER_LINE_RE = re.compile(r'^\s*(?:#|-)\s*费用:\s*(\d+)\s*$')
ROLE_LINE_RE = re.compile(r'^\s*-\s*(\d+)\b')


def load_cost_mapping(pool_path: Path) -> dict[int, int]:
    cost_map: dict[int, int] = {}
    current_tier: int | None = None
    for raw_line in pool_path.read_text(encoding='utf-8').splitlines():
        line = raw_line.rstrip()
        tier_match = TIER_LINE_RE.match(line)
        if tier_match:
            current_tier = int(tier_match.group(1))
            continue
        role_match = ROLE_LINE_RE.match(line)
        if role_match and current_tier is not None:
            cost_map[int(role_match.group(1))] = current_tier
    return cost_map


def get_columns(cur, table):
    """Get set of column names for a table."""
    cur.execute(f"PRAGMA table_info({table})")
    return {row[1] for row in cur.fetchall()}


def get_column_info(cur, table):
    """Get full column info list."""
    cur.execute(f"PRAGMA table_info({table})")
    return cur.fetchall()


def migrate_role_table(conn, dry_run):
    """Rebuild the role table with interleaved star columns."""
    cur = conn.cursor()
    repo_root = Path(__file__).resolve().parents[1]
    cost_map = load_cost_mapping(repo_root / 'config' / 'chess_pool.yaml')

    # Determine source table
    all_tables = {r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}
    if 'role_old' in all_tables:
        source = 'role_old'
        print(f"  Using role_old as source")
    elif 'role' in all_tables:
        source = 'role'
    else:
        print("  ERROR: No role or role_old table found!")
        return

    cols = get_columns(cur, source)

    # Check if already using new schema and has data
    if source == 'role' and '一星武功1' in cols:
        cur.execute("SELECT COUNT(*) FROM role")
        count = cur.fetchone()[0]
        if count > 0:
            print(f"  Role table already migrated with {count} rows, skipping.")
            return
        # Has schema but no data -> need to rebuild from role_old
        if 'role_old' not in all_tables:
            print("  ERROR: role has new schema but 0 rows and no role_old backup!")
            return
        source = 'role_old'
        print(f"  role has 0 rows, falling back to role_old")

    # Get source column info
    col_info = get_column_info(cur, source)
    old_col_names = [c[1] for c in col_info]
    old_col_map = {name: idx for idx, name in enumerate(old_col_names)}

    def get_val(row, col, default=0):
        if col in old_col_map:
            v = row[old_col_map[col]]
            return v if v is not None else default
        return default

    # Read all rows from source
    cur.execute(f"SELECT * FROM {source}")
    all_rows = cur.fetchall()
    print(f"  Read {len(all_rows)} rows from {source}")

    # New schema with INTERLEAVED columns
    new_columns = [
        ("编号", "INTEGER"),
        ("头像", "INTEGER DEFAULT 0"),
        ("名字", "TEXT"),
        ("外号", "TEXT"),
        ("费用", "INTEGER DEFAULT -1"),
        ("性别", "INTEGER DEFAULT 0"),
        ("生命最大值", "INTEGER DEFAULT 0"),
        ("内力性质", "INTEGER DEFAULT 0"),
        ("内力最大值", "INTEGER DEFAULT 0"),
        ("攻击力", "INTEGER DEFAULT 0"),
        ("轻功", "INTEGER DEFAULT 0"),
        ("防御力", "INTEGER DEFAULT 0"),
        ("医疗", "INTEGER DEFAULT 0"),
        ("用毒", "INTEGER DEFAULT 0"),
        ("解毒", "INTEGER DEFAULT 0"),
        ("抗毒", "INTEGER DEFAULT 0"),
        ("拳掌", "INTEGER DEFAULT 0"),
        ("御剑", "INTEGER DEFAULT 0"),
        ("耍刀", "INTEGER DEFAULT 0"),
        ("特殊", "INTEGER DEFAULT 0"),
        ("暗器", "INTEGER DEFAULT 0"),
        ("武学常识", "INTEGER DEFAULT 0"),
        ("品德", "INTEGER DEFAULT 0"),
        ("攻击带毒", "INTEGER DEFAULT 0"),
        ("左右互搏", "INTEGER DEFAULT 0"),
        ("声望", "INTEGER DEFAULT 0"),
        ("资质", "INTEGER DEFAULT 0"),
        # Interleaved: skill then power, grouped by star
        ("一星武功1", "INTEGER DEFAULT 0"),
        ("一星威力1", "INTEGER DEFAULT 0"),
        ("一星武功2", "INTEGER DEFAULT 0"),
        ("一星威力2", "INTEGER DEFAULT 0"),
        ("二星武功1", "INTEGER DEFAULT 0"),
        ("二星威力1", "INTEGER DEFAULT 0"),
        ("二星武功2", "INTEGER DEFAULT 0"),
        ("二星威力2", "INTEGER DEFAULT 0"),
        ("三星武功1", "INTEGER DEFAULT 0"),
        ("三星威力1", "INTEGER DEFAULT 0"),
        ("三星武功2", "INTEGER DEFAULT 0"),
        ("三星威力2", "INTEGER DEFAULT 0"),
    ]

    new_col_names = [c[0] for c in new_columns]

    # Build new rows
    new_rows = []
    for row in all_rows:
        new_row = {}

        # Direct stat columns
        for col in ["编号", "头像", "性别", "生命最大值", "内力性质", "内力最大值",
                     "攻击力", "轻功", "防御力", "医疗", "用毒", "解毒", "抗毒",
                     "拳掌", "御剑", "耍刀", "特殊", "暗器", "武学常识", "品德",
                     "攻击带毒", "左右互搏", "声望", "资质"]:
            new_row[col] = get_val(row, col, 0)

        new_row["名字"] = get_val(row, "名字", "")
        new_row["外号"] = get_val(row, "外号", "")
        new_row["费用"] = cost_map.get(new_row["编号"], get_val(row, "费用", -1))

        # Gather all skills with their power values, pick top 2
        skills = []
        for i in range(1, 11):
            mid_col = f"所会武功{i}"
            if mid_col not in old_col_map:
                continue
            mid = get_val(row, mid_col, 0)
            if mid and mid > 0:
                # Try 武功威力 first (pre-computed), else 0
                power_col = f"武功威力{i}"
                if power_col in old_col_map:
                    power = get_val(row, power_col, 0)
                else:
                    power = 0
                skills.append((mid, power))

        # Sort by power descending, take top 2
        skills.sort(key=lambda x: x[1], reverse=True)
        top2 = skills[:2]

        # Fill ALL 3 star tiers with the same top 2 skills
        star_prefixes = ["一星", "二星", "三星"]
        for prefix in star_prefixes:
            for slot in range(2):
                if slot < len(top2):
                    new_row[f"{prefix}武功{slot+1}"] = top2[slot][0]
                    new_row[f"{prefix}威力{slot+1}"] = top2[slot][1]
                else:
                    new_row[f"{prefix}武功{slot+1}"] = 0
                    new_row[f"{prefix}威力{slot+1}"] = 0

        new_rows.append(tuple(new_row.get(c, 0) for c in new_col_names))

    # Drop old role table, create new
    if source == 'role_old':
        cur.execute("DROP TABLE IF EXISTS role")
    else:
        cur.execute("DROP TABLE IF EXISTS role_old")
        cur.execute("ALTER TABLE role RENAME TO role_old")

    col_defs = ", ".join(f"{name} {typ}" for name, typ in new_columns)
    cur.execute(f"CREATE TABLE role ({col_defs})")

    placeholders = ", ".join("?" * len(new_columns))
    cur.executemany(f"INSERT INTO role ({', '.join(new_col_names)}) VALUES ({placeholders})", new_rows)

    # Keep role_old for safety

    print(f"  Rebuilt role table: {len(new_rows)} rows, {len(new_columns)} columns")

    # Show sample
    cur.execute("""
        SELECT 编号, 名字, 一星武功1, 一星威力1, 一星武功2, 一星威力2
        FROM role WHERE 一星武功1 > 0
        ORDER BY 一星威力1 DESC LIMIT 5
    """)
    print(f"\n  Top 5 roles by power:")
    print(f"  {'ID':>4} {'Name':<12} {'1★S1':>6} {'1★P1':>7} {'1★S2':>6} {'1★P2':>7}")
    for r in cur.fetchall():
        print(f"  {r[0]:>4} {r[1]:<12} {r[2]:>6} {r[3]:>7} {r[4]:>6} {r[5]:>7}")


def migrate_magic_table(conn, dry_run):
    """Rebuild the magic table with cleaned schema."""
    cur = conn.cursor()

    all_tables = {r[0] for r in cur.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()}
    if 'magic_old' in all_tables:
        source = 'magic_old'
    elif 'magic' in all_tables:
        source = 'magic'
    else:
        print("  ERROR: No magic table found!")
        return

    cols = get_columns(cur, source)

    # Check if already migrated (no 威力 columns at all)
    if source == 'magic' and '威力1' not in cols and '威力' not in cols:
        print("  Magic table already migrated")
        return

    col_info = get_column_info(cur, source)
    old_col_names = [c[1] for c in col_info]
    old_col_map = {name: idx for idx, name in enumerate(old_col_names)}

    def get_val(row, col, default=0):
        if col in old_col_map:
            v = row[old_col_map[col]]
            return v if v is not None else default
        return default

    cur.execute(f"SELECT * FROM {source}")
    all_rows = cur.fetchall()
    print(f"  Read {len(all_rows)} rows from {source}")

    # New schema (no 威力 - power is per-role)
    new_columns = [
        ("编号", "INTEGER"),
        ("名称", "TEXT"),
        ("出招音效", "INTEGER DEFAULT 0"),
        ("武功类型", "INTEGER DEFAULT 0"),
        ("武功动画", "INTEGER DEFAULT 0"),
        ("伤害类型", "INTEGER DEFAULT 0"),
        ("攻击范围类型", "INTEGER DEFAULT 0"),
        ("消耗内力", "INTEGER DEFAULT 0"),
        ("敌人中毒", "INTEGER DEFAULT 0"),
        ("移动范围", "INTEGER DEFAULT 0"),
        ("杀伤范围", "INTEGER DEFAULT 0"),
        ("加内力", "INTEGER DEFAULT 0"),
        ("杀伤内力", "INTEGER DEFAULT 0"),
    ]

    # Flatten arrays by taking MAX (no 威力 - power is per-role now)
    array_flatten = {
        "移动范围": [f"移动范围{i}" for i in range(1, 11)],
        "杀伤范围": [f"杀伤范围{i}" for i in range(1, 11)],
        "加内力": [f"加内力{i}" for i in range(1, 11)],
        "杀伤内力": [f"杀伤内力{i}" for i in range(1, 11)],
    }

    new_col_names = [c[0] for c in new_columns]
    new_rows = []

    for row in all_rows:
        new_row = {}

        # Direct columns
        for col in ["编号", "出招音效", "武功类型", "武功动画", "伤害类型",
                     "攻击范围类型", "消耗内力", "敌人中毒"]:
            new_row[col] = get_val(row, col, 0)

        new_row["名称"] = get_val(row, "名称", "")

        # Flatten arrays
        for new_col, old_variants in array_flatten.items():
            existing = [c for c in old_variants if c in old_col_map]
            if existing:
                values = [get_val(row, c, 0) for c in existing]
                new_row[new_col] = max(values) if values else 0
            else:
                new_row[new_col] = 0

        new_rows.append(tuple(new_row.get(c, 0) for c in new_col_names))

    # Rebuild
    if source == 'magic_old':
        cur.execute("DROP TABLE IF EXISTS magic")
    else:
        cur.execute("DROP TABLE IF EXISTS magic_old")
        cur.execute("ALTER TABLE magic RENAME TO magic_old")

    col_defs = ", ".join(f"{name} {typ}" for name, typ in new_columns)
    cur.execute(f"CREATE TABLE magic ({col_defs})")

    placeholders = ", ".join("?" * len(new_columns))
    cur.executemany(f"INSERT INTO magic ({', '.join(new_col_names)}) VALUES ({placeholders})", new_rows)

    # Keep magic_old for safety

    print(f"  Rebuilt magic table: {len(new_rows)} rows, {len(new_columns)} columns")

    # Show sample
    cur.execute("SELECT 编号, 名称, 移动范围, 杀伤范围, 消耗内力 FROM magic ORDER BY 编号 LIMIT 5")
    print(f"\n  Sample magic rows:")
    print(f"  {'ID':>4} {'Name':<16} {'Sel':>4} {'AtkD':>5} {'MP':>4}")
    for r in cur.fetchall():
        print(f"  {r[0]:>4} {r[1]:<16} {r[2]:>4} {r[3]:>5} {r[4]:>4}")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <db_path> [--dry-run]")
        sys.exit(1)

    db_path = sys.argv[1]
    dry_run = "--dry-run" in sys.argv

    if not os.path.exists(db_path):
        print(f"ERROR: File not found: {db_path}")
        sys.exit(1)

    sys.stdout.reconfigure(encoding='utf-8')

    # Backup
    backup_path = db_path + ".bak"
    if not os.path.exists(backup_path):
        shutil.copy2(db_path, backup_path)
        print(f"Backup created: {backup_path}")
    else:
        print(f"Backup already exists: {backup_path}")

    conn = sqlite3.connect(db_path)

    print(f"\nMigrating: {db_path}")
    print("=" * 50)

    print("\n[1/2] Rebuilding role table...")
    migrate_role_table(conn, dry_run)

    print("\n[2/2] Rebuilding magic table...")
    migrate_magic_table(conn, dry_run)

    if dry_run:
        print("\n[DRY RUN] Rolling back changes...")
        conn.rollback()
    else:
        conn.commit()
        print("\nMigration complete!")

    conn.close()


if __name__ == "__main__":
    main()
