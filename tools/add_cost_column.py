#!/usr/bin/env python3
"""Add cost column to role table and populate from chess_pool.yaml."""
import sqlite3
from pathlib import Path

# Hardcoded cost map from chess_pool.yaml
COST_MAP = {
    # 费用: 1
    130: 1, 131: 1, 132: 1, 133: 1, 134: 1, 135: 1, 136: 1,
    63: 1, 84: 1, 160: 1, 161: 1, 45: 1, 47: 1, 104: 1, 105: 1,
    # 费用: 2
    54: 2, 37: 2, 97: 2, 56: 2, 44: 2, 48: 2, 99: 2, 100: 2, 102: 2, 115: 2,
    # 费用: 3
    55: 3, 67: 3, 68: 3, 59: 3, 46: 3, 51: 3, 53: 3, 70: 3, 98: 3, 103: 3, 112: 3, 113: 3,
    # 费用: 4
    57: 4, 60: 4, 64: 4, 65: 4, 69: 4, 58: 4, 62: 4, 49: 4, 50: 4, 117: 4, 118: 4,
    # 费用: 5
    129: 5, 114: 5, 116: 5,
}

def add_cost_column(db_path):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Check if column exists
    cur.execute("PRAGMA table_info(role)")
    columns = [row[1] for row in cur.fetchall()]

    if '费用' not in columns:
        cur.execute("ALTER TABLE role ADD COLUMN 费用 INTEGER DEFAULT -1")
        print(f"  Added 费用 column")

    # Set all to -1 first
    cur.execute('UPDATE role SET 费用 = -1')

    # Update costs
    for role_id, cost in COST_MAP.items():
        cur.execute('UPDATE role SET 费用 = ? WHERE 编号 = ?', (cost, role_id))

    conn.commit()
    conn.close()

if __name__ == '__main__':
    save_dir = Path('work/game-dev/save')
    for db_file in save_dir.glob('*.db'):
        print(f'Processing {db_file}...')
        add_cost_column(db_file)
        print(f'  OK: Cost column added and populated')
