#!/usr/bin/env python3
import sqlite3
from pathlib import Path

from chess_pool_io import load_cost_mapping


def update_database(db_path, cost_map):
    """Update role cost/tiering in database"""
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    columns = [row[1] for row in cur.execute('PRAGMA table_info(role)')]
    if '费用' not in columns:
        raise RuntimeError('role table is missing 费用 column; run tools/add_cost_column.py first')

    for role_id, cost in cost_map.items():
        cur.execute('UPDATE role SET 费用 = ? WHERE 编号 = ?', (cost, role_id))

    conn.commit()
    updated = cur.rowcount
    conn.close()
    return updated

if __name__ == '__main__':
    project_root = Path(__file__).parent.parent
    yaml_path = project_root / 'config' / 'chess_pool.yaml'
    save_dir = project_root / 'work' / 'game-dev' / 'save'

    cost_map = load_cost_mapping(yaml_path)
    print(f"Loaded {len(cost_map)} role costs from chess_pool.yaml")

    for db_file in save_dir.glob('*.db'):
        try:
            update_database(db_file, cost_map)
            print(f"✓ Updated {db_file.name}")
        except Exception as e:
            print(f"✗ Failed to update {db_file.name}: {e}")
