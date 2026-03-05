#!/usr/bin/env python3
"""Fix invalid magic ID 109 in role 304"""
import sqlite3
import sys
import shutil
from pathlib import Path

def fix_database(db_path):
    db_file = Path(db_path)
    if not db_file.exists():
        print(f"Error: {db_path} not found")
        return False

    # Backup
    backup = db_file.with_suffix('.db.bak')
    shutil.copy2(db_file, backup)
    print(f"Created backup: {backup}")

    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Fix role 304 - replace magic 109 with 0 (safe default)
    cur.execute('''
        UPDATE role
        SET 一星武功1 = 0, 二星武功1 = 0, 三星武功1 = 0
        WHERE 编号 = 304
    ''')

    conn.commit()
    print(f"Fixed role 304: replaced magic ID 109 with 0")

    # Verify
    cur.execute('SELECT 编号, 名字, 一星武功1, 二星武功1, 三星武功1 FROM role WHERE 编号 = 304')
    print("After fix:", cur.fetchone())

    conn.close()
    return True

if __name__ == '__main__':
    save_dir = Path('d:/projects/kys-cpp/kys-cpp/work/game-dev/save')
    for db_file in save_dir.glob('*.db'):
        print(f"\n=== Processing {db_file.name} ===")
        fix_database(db_file)
