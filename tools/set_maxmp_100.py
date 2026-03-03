#!/usr/bin/env python3
"""Set all role 内力最大值 (MaxMP) to 100 in all save files."""
import sqlite3
import sys
from pathlib import Path

def set_maxmp_100(db_path):
    """Set all roles' MaxMP to 100."""
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Update all roles
    cur.execute('UPDATE role SET 内力最大值 = 100')
    conn.commit()

    # Verify
    cur.execute('SELECT COUNT(*) FROM role WHERE 内力最大值 != 100')
    count = cur.fetchone()[0]

    conn.close()
    return count == 0

if __name__ == '__main__':
    save_dir = Path('work/game-dev/save')

    for db_file in save_dir.glob('*.db'):
        print(f'Processing {db_file}...')
        if set_maxmp_100(db_file):
            print(f'  OK: All roles set to MaxMP=100')
        else:
            print(f'  ERROR: Failed to update all roles')
