#!/usr/bin/env python3
"""Fix duplicate role IDs by reassigning them sequentially."""

import sqlite3
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB_PATH = REPO_ROOT / "work" / "game-dev" / "save" / "0.db"


def fix_duplicates(db_path: Path) -> None:
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()

        # Get all roles ordered by current ID, then by rowid for stable ordering
        cur.execute("SELECT rowid, 编号, 名字 FROM role ORDER BY 编号, rowid")
        roles = cur.fetchall()

        # Assign new sequential IDs starting from 0
        next_id = 0
        updates = []
        for rowid, old_id, name in roles:
            if old_id != next_id:
                updates.append((next_id, rowid))
            next_id += 1

        # Apply updates
        for new_id, rowid in updates:
            cur.execute("UPDATE role SET 编号 = ? WHERE rowid = ?", (new_id, rowid))

        conn.commit()
        print(f"Fixed {len(updates)} role IDs in {db_path.name}")

        # Show new max ID
        cur.execute("SELECT MAX(编号) FROM role")
        print(f"New max ID: {cur.fetchone()[0]}")

    finally:
        conn.close()


if __name__ == "__main__":
    db_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_DB_PATH
    fix_duplicates(db_path)
