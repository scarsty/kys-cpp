#!/usr/bin/env python3
"""將 game.db 的名稱與稱號統一為繁體中文。"""

import argparse
import sqlite3
import sys
from pathlib import Path

from opencc import OpenCC
from promo.traditional_text import to_traditional


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_DB = ROOT / "work/game-dev/save/game.db"

TEXT_COLUMNS = (
    ("role", "名字"),
    ("role", "外号"),
    ("magic", "名称"),
    ("item", "物品名"),
    ("submap", "名称"),
)

def require_text_columns(connection: sqlite3.Connection) -> None:
    for table, column in TEXT_COLUMNS:
        columns = {row[1]: row[2].upper() for row in connection.execute(f'PRAGMA table_info("{table}")')}
        assert column in columns
        assert "TEXT" in columns[column]


def collect_changes(connection: sqlite3.Connection) -> list[tuple[str, str, int, str, str]]:
    converter = OpenCC("s2t")
    changes = []
    for table, column in TEXT_COLUMNS:
        for rowid, original in connection.execute(
            f'SELECT rowid, "{column}" FROM "{table}" WHERE "{column}" IS NOT NULL'
        ):
            converted = to_traditional(original, converter)
            if converted != original:
                changes.append((table, column, rowid, original, converted))
    return changes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("db", nargs="?", type=Path, default=DEFAULT_DB)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    sys.stdout.reconfigure(encoding="utf-8")
    db = args.db.resolve()
    assert db == DEFAULT_DB.resolve(), f"只能修改來源資料庫：{DEFAULT_DB}"

    with sqlite3.connect(db) as connection:
        require_text_columns(connection)
        changes = collect_changes(connection)
        if not args.dry_run:
            for table, column, rowid, _, converted in changes:
                connection.execute(
                    f'UPDATE "{table}" SET "{column}" = ? WHERE rowid = ?',
                    (converted, rowid),
                )

    mode = "預覽" if args.dry_run else "已更新"
    print(f"{mode} {db}: {len(changes)} 筆")
    for table, column, rowid, original, converted in changes:
        print(f"{table}.{column} rowid={rowid}: {original} -> {converted}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
