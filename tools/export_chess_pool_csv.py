#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import sqlite3
import sys
from contextlib import closing
from pathlib import Path
from typing import NamedTuple

from chess_pool_io import PoolEntry as PoolRow
from chess_pool_io import load_synergy_members
from chess_pool_io import parse_pool_entries


ROLE_ID_COLUMN = "\u7f16\u53f7"
ROLE_NAME_COLUMN = "\u540d\u5b57"
MAGIC_ID_COLUMN = "\u7f16\u53f7"
MAGIC_NAME_COLUMN = "\u540d\u79f0"
ROLE_MAGIC_COLUMNS = (
    ("\u4e00\u661f\u6b66\u529f1", "\u4e00\u661f\u5a01\u529b1"),
    ("\u4e00\u661f\u6b66\u529f2", "\u4e00\u661f\u5a01\u529b2"),
    ("\u4e8c\u661f\u6b66\u529f1", "\u4e8c\u661f\u5a01\u529b1"),
    ("\u4e8c\u661f\u6b66\u529f2", "\u4e8c\u661f\u5a01\u529b2"),
    ("\u4e09\u661f\u6b66\u529f1", "\u4e09\u661f\u5a01\u529b1"),
    ("\u4e09\u661f\u6b66\u529f2", "\u4e09\u661f\u5a01\u529b2"),
)


sys.stdout.reconfigure(encoding="utf-8")


class MagicExportData(NamedTuple):
    magic_id: int
    name: str
    power: int
    slot_order: int


class RoleExportData(NamedTuple):
    name: str
    magics: tuple[MagicExportData, ...]


def parse_pool_rows(pool_path: Path) -> list[PoolRow]:
    return parse_pool_entries(pool_path)


def ordered_role_magics(row: sqlite3.Row, magic_names: dict[int, str]) -> tuple[MagicExportData, ...]:
    by_id: dict[int, MagicExportData] = {}
    for slot_order, (magic_column, power_column) in enumerate(ROLE_MAGIC_COLUMNS):
        magic_id = int(row[magic_column] or 0)
        if magic_id <= 0:
            continue
        power = int(row[power_column] or 0)
        existing = by_id.get(magic_id)
        if existing is None or power > existing.power:
            by_id[magic_id] = MagicExportData(magic_id, magic_names[magic_id], power, slot_order)

    return tuple(sorted(by_id.values(), key=lambda magic: (-magic.power, magic.slot_order))[:2])


def load_magic_names(cursor: sqlite3.Cursor) -> dict[int, str]:
    rows = cursor.execute(f'SELECT "{MAGIC_ID_COLUMN}", "{MAGIC_NAME_COLUMN}" FROM magic').fetchall()
    return {int(magic_id): str(name).strip() for magic_id, name in rows}


def load_role_export_data(db_path: Path) -> dict[int, RoleExportData]:
    selected_columns = [ROLE_ID_COLUMN, ROLE_NAME_COLUMN]
    for magic_column, power_column in ROLE_MAGIC_COLUMNS:
        selected_columns.extend([magic_column, power_column])

    quoted_columns = ", ".join(f'"{column}"' for column in selected_columns)
    with closing(sqlite3.connect(db_path)) as connection:
        connection.row_factory = sqlite3.Row
        cursor = connection.cursor()
        magic_names = load_magic_names(cursor)
        rows = cursor.execute(f"SELECT {quoted_columns} FROM role").fetchall()

    result: dict[int, RoleExportData] = {}
    for row in rows:
        role_id = int(row[ROLE_ID_COLUMN])
        result[role_id] = RoleExportData(
            str(row[ROLE_NAME_COLUMN]).strip(),
            ordered_role_magics(row, magic_names),
        )
    return result


def load_role_names(db_path: Path) -> dict[int, str]:
    return {role_id: role.name for role_id, role in load_role_export_data(db_path).items()}


def write_csv(
    rows: list[PoolRow],
    output_path: Path,
    role_data: dict[int, RoleExportData],
    synergies: dict[int, list[str]],
) -> None:
    max_synergies = max((len(synergies.get(row.role_id, [])) for row in rows), default=0)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["id", "name", "star", "ult", "normal"] + [f"synergy_{index}" for index in range(1, max_synergies + 1)])
        for row in rows:
            data = role_data.get(row.role_id, RoleExportData(row.comment_name, ()))
            magic_names = [magic.name for magic in data.magics]
            role_synergies = synergies.get(row.role_id, [])
            writer.writerow(
                [
                    row.role_id,
                    data.name,
                    row.star,
                    magic_names[0] if len(magic_names) > 0 else "",
                    magic_names[1] if len(magic_names) > 1 else "",
                    *role_synergies,
                    *([""] * (max_synergies - len(role_synergies))),
                ]
            )


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    default_pool = repo_root / "config" / "chess_pool.yaml"

    parser = argparse.ArgumentParser(description="Export chess_pool.yaml to an id,name,star CSV.")
    parser.add_argument(
        "--pool",
        type=Path,
        default=default_pool,
        help="Pool YAML to export. Defaults to config/chess_pool.yaml.",
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=repo_root / "work" / "game-dev" / "save" / "game.db",
        help="SQLite game DB used for canonical role names.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="CSV output path. Defaults to the pool path with a .csv suffix.",
    )
    parser.add_argument(
        "--combos",
        type=Path,
        default=repo_root / "config" / "chess_combos.yaml",
        help="Combo YAML used to append synergy columns.",
    )
    parser.add_argument(
        "--allow-comment-names",
        action="store_true",
        help="Use YAML comment names when the database is missing or lacks a role.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_path = args.output if args.output is not None else args.pool.with_suffix(".csv")

    rows = parse_pool_rows(args.pool)
    if not rows:
        print(f"No role rows found in {args.pool}", file=sys.stderr)
        return 1

    role_data: dict[int, RoleExportData] = {}
    if args.db.exists():
        role_data = load_role_export_data(args.db)
    elif not args.allow_comment_names:
        print(f"Database not found: {args.db}", file=sys.stderr)
        return 1

    missing_names = [row.role_id for row in rows if row.role_id not in role_data and not row.comment_name]
    if missing_names:
        print(
            "Missing names for role IDs: " + ", ".join(str(role_id) for role_id in missing_names),
            file=sys.stderr,
        )
        return 1

    synergies = load_synergy_members(args.combos) if args.combos.exists() else {}
    write_csv(rows, output_path, role_data, synergies)
    print(f"Wrote {len(rows)} rows to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
