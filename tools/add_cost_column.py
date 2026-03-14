#!/usr/bin/env python3
"""Rebuild role tables with 费用 as the 5th column and populate it from chess_pool.yaml."""

from __future__ import annotations

import argparse
import re
import shutil
import sqlite3
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')

TIER_LINE_RE = re.compile(r'^\s*(?:#|-)\s*费用:\s*(\d+)\s*$')
ROLE_LINE_RE = re.compile(r'^\s*-\s*(\d+)\b')

ROLE_COLUMNS = [
    ('编号', 'INTEGER'),
    ('头像', 'INTEGER DEFAULT 0'),
    ('名字', 'TEXT'),
    ('外号', 'TEXT'),
    ('费用', 'INTEGER DEFAULT -1'),
    ('性别', 'INTEGER DEFAULT 0'),
    ('生命最大值', 'INTEGER DEFAULT 0'),
    ('内力性质', 'INTEGER DEFAULT 0'),
    ('内力最大值', 'INTEGER DEFAULT 0'),
    ('攻击力', 'INTEGER DEFAULT 0'),
    ('轻功', 'INTEGER DEFAULT 0'),
    ('防御力', 'INTEGER DEFAULT 0'),
    ('医疗', 'INTEGER DEFAULT 0'),
    ('用毒', 'INTEGER DEFAULT 0'),
    ('解毒', 'INTEGER DEFAULT 0'),
    ('抗毒', 'INTEGER DEFAULT 0'),
    ('拳掌', 'INTEGER DEFAULT 0'),
    ('御剑', 'INTEGER DEFAULT 0'),
    ('耍刀', 'INTEGER DEFAULT 0'),
    ('特殊', 'INTEGER DEFAULT 0'),
    ('暗器', 'INTEGER DEFAULT 0'),
    ('武学常识', 'INTEGER DEFAULT 0'),
    ('品德', 'INTEGER DEFAULT 0'),
    ('攻击带毒', 'INTEGER DEFAULT 0'),
    ('左右互搏', 'INTEGER DEFAULT 0'),
    ('声望', 'INTEGER DEFAULT 0'),
    ('资质', 'INTEGER DEFAULT 0'),
    ('一星武功1', 'INTEGER DEFAULT 0'),
    ('一星威力1', 'INTEGER DEFAULT 0'),
    ('一星武功2', 'INTEGER DEFAULT 0'),
    ('一星威力2', 'INTEGER DEFAULT 0'),
    ('二星武功1', 'INTEGER DEFAULT 0'),
    ('二星威力1', 'INTEGER DEFAULT 0'),
    ('二星武功2', 'INTEGER DEFAULT 0'),
    ('二星威力2', 'INTEGER DEFAULT 0'),
    ('三星武功1', 'INTEGER DEFAULT 0'),
    ('三星威力1', 'INTEGER DEFAULT 0'),
    ('三星武功2', 'INTEGER DEFAULT 0'),
    ('三星威力2', 'INTEGER DEFAULT 0'),
]


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description='Add/sync role cost in save DBs.')
    parser.add_argument(
        '--pool',
        type=Path,
        default=repo_root / 'config' / 'chess_pool.yaml',
        help='Pool config to source role costs from.',
    )
    parser.add_argument(
        'paths',
        nargs='*',
        type=Path,
        help='Database files or directories. Defaults to work/game-dev/save.',
    )
    parser.add_argument(
        '--no-backup',
        action='store_true',
        help='Skip creating .cost.bak backups before rewriting DBs.',
    )
    return parser.parse_args()


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


def iter_db_paths(paths: list[Path], repo_root: Path) -> list[Path]:
    if not paths:
        paths = [repo_root / 'work' / 'game-dev' / 'save']

    result: list[Path] = []
    for path in paths:
        if path.is_dir():
            result.extend(sorted(candidate for candidate in path.glob('*.db') if candidate.is_file()))
        elif path.is_file():
            result.append(path)
    return result


def rebuild_role_table(db_path: Path, cost_map: dict[int, int], create_backup: bool) -> None:
    if create_backup:
        backup_path = db_path.with_suffix(db_path.suffix + '.cost.bak')
        if not backup_path.exists():
            shutil.copy2(db_path, backup_path)

    conn = sqlite3.connect(str(db_path))
    conn.row_factory = sqlite3.Row
    try:
        cur = conn.cursor()
        existing_columns = [row[1] for row in cur.execute('PRAGMA table_info(role)').fetchall()]
        rows = cur.execute('SELECT * FROM role ORDER BY 编号').fetchall()

        cur.execute('DROP TABLE IF EXISTS role_old_cost')
        cur.execute('ALTER TABLE role RENAME TO role_old_cost')

        column_defs = ', '.join(f'{name} {column_type}' for name, column_type in ROLE_COLUMNS)
        cur.execute(f'CREATE TABLE role ({column_defs})')

        ordered_names = [name for name, _ in ROLE_COLUMNS]
        placeholders = ', '.join('?' for _ in ordered_names)
        insert_sql = f"INSERT INTO role ({', '.join(ordered_names)}) VALUES ({placeholders})"

        new_rows = []
        for row in rows:
            role_id = int(row['编号'])
            values = []
            for name in ordered_names:
                if name == '费用':
                    values.append(cost_map.get(role_id, row[name] if name in existing_columns else -1))
                elif name in existing_columns:
                    values.append(row[name])
                else:
                    values.append(0 if name not in ('名字', '外号') else '')
            new_rows.append(tuple(values))

        cur.executemany(insert_sql, new_rows)
        cur.execute('DROP TABLE role_old_cost')
        conn.commit()
    finally:
        conn.close()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[1]
    db_paths = iter_db_paths(args.paths, repo_root)
    if not db_paths:
        print('No database files found.')
        return 1

    cost_map = load_cost_mapping(args.pool)
    print(f'Loaded {len(cost_map)} role costs from {args.pool}')

    for db_path in db_paths:
        print(f'Processing {db_path}...')
        rebuild_role_table(db_path, cost_map, create_backup=not args.no_backup)
        print('  OK')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
