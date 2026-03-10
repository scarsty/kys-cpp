#!/usr/bin/env python3
"""Rebuild database IDs from 0 based on name order."""

import sqlite3
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parents[1]

def rebuild_db(db_path: Path) -> None:
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()

        # Get all roles in their CURRENT order (by rowid)
        cur.execute("SELECT rowid, 编号, 名字 FROM role ORDER BY rowid")
        roles = cur.fetchall()

        # Assign new IDs from 0 in the same order
        updates = []
        for new_id, (rowid, old_id, name) in enumerate(roles):
            if old_id != new_id:
                updates.append((new_id, rowid))

        # Apply updates
        for new_id, rowid in updates:
            cur.execute("UPDATE role SET 编号 = ? WHERE rowid = ?", (new_id, rowid))

        conn.commit()
        print(f"Updated {len(updates)} IDs in {db_path.name}")

    finally:
        conn.close()

if __name__ == "__main__":
    for db in (REPO_ROOT / "work" / "game-dev" / "save").glob("*.db"):
        rebuild_db(db)
