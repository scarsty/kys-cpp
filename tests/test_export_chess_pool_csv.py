from __future__ import annotations

import csv
import importlib.util
import sqlite3
import sys
import tempfile
import unittest
from contextlib import closing
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_PATH = REPO_ROOT / "tools" / "export_chess_pool_csv.py"
ROLE_ID_COLUMN = "\u7f16\u53f7"
ROLE_NAME_COLUMN = "\u540d\u5b57"
MAGIC_ID_COLUMN = "\u7f16\u53f7"
MAGIC_NAME_COLUMN = "\u540d\u79f0"


def load_export_module():
    tools_path = str(SCRIPT_PATH.parent)
    if tools_path not in sys.path:
        sys.path.insert(0, tools_path)
    spec = importlib.util.spec_from_file_location("export_chess_pool_csv", SCRIPT_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


def create_role_table(con: sqlite3.Connection) -> None:
    con.execute(
        f"""
        CREATE TABLE role (
            "{ROLE_ID_COLUMN}" INTEGER PRIMARY KEY,
            "{ROLE_NAME_COLUMN}" TEXT,
            "\u4e00\u661f\u6b66\u529f1" INTEGER,
            "\u4e00\u661f\u5a01\u529b1" INTEGER,
            "\u4e00\u661f\u6b66\u529f2" INTEGER,
            "\u4e00\u661f\u5a01\u529b2" INTEGER,
            "\u4e8c\u661f\u6b66\u529f1" INTEGER,
            "\u4e8c\u661f\u5a01\u529b1" INTEGER,
            "\u4e8c\u661f\u6b66\u529f2" INTEGER,
            "\u4e8c\u661f\u5a01\u529b2" INTEGER,
            "\u4e09\u661f\u6b66\u529f1" INTEGER,
            "\u4e09\u661f\u5a01\u529b1" INTEGER,
            "\u4e09\u661f\u6b66\u529f2" INTEGER,
            "\u4e09\u661f\u5a01\u529b2" INTEGER
        )
        """
    )


class ExportChessPoolCsvTests(unittest.TestCase):
    def test_parse_pool_rows_keeps_yaml_order_and_star_from_tier_comments(self) -> None:
        module = load_export_module()
        with tempfile.TemporaryDirectory() as tmp:
            pool_path = Path(tmp) / "chess_pool.yaml"
            pool_path.write_text(
                "\n".join(
                    [
                        "\u89d2\u8272:",
                        "  # \u8d39\u7528: 1",
                        "  - 130  # \u67ef\u93ae\u60e1",
                        "  - 131  # \u6731\u8070",
                        "  # \u8d39\u7528: 2",
                        "  - 54  # \u8881\u627f\u5fd7",
                    ]
                ),
                encoding="utf-8",
            )

            rows = module.parse_pool_rows(pool_path)

        self.assertEqual(
            rows,
            [
                module.PoolRow(130, "\u67ef\u93ae\u60e1", 1),
                module.PoolRow(131, "\u6731\u8070", 1),
                module.PoolRow(54, "\u8881\u627f\u5fd7", 2),
            ],
        )

    def test_write_csv_orders_magic_by_power_and_appends_synergy_columns(self) -> None:
        module = load_export_module()
        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            db_path = tmp_path / "game.db"
            output_path = tmp_path / "pool.csv"

            with closing(sqlite3.connect(db_path)) as con:
                create_role_table(con)
                con.execute(
                    f'CREATE TABLE magic ("{MAGIC_ID_COLUMN}" INTEGER PRIMARY KEY, "{MAGIC_NAME_COLUMN}" TEXT)'
                )
                con.executemany(
                    f"""
                    INSERT INTO role VALUES (
                        ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
                    )
                    """,
                    [
                        (
                            130,
                            "\u67ef\u93ae\u60e1",
                            1,
                            300,
                            2,
                            800,
                            1,
                            300,
                            2,
                            800,
                            1,
                            300,
                            2,
                            800,
                        ),
                        (
                            131,
                            "\u6731\u8070",
                            3,
                            450,
                            0,
                            0,
                            3,
                            450,
                            0,
                            0,
                            3,
                            450,
                            0,
                            0,
                        ),
                    ],
                )
                con.executemany(
                    f'INSERT INTO magic ("{MAGIC_ID_COLUMN}", "{MAGIC_NAME_COLUMN}") VALUES (?, ?)',
                    [
                        (1, "\u964d\u9f8d\u638c"),
                        (2, "\u4f0f\u9b54\u6756"),
                        (3, "\u7a7a\u660e\u62f3"),
                    ],
                )
                con.commit()

            rows = [
                module.PoolRow(130, "\u820a\u540d", 1),
                module.PoolRow(131, "\u6731\u8070", 1),
            ]
            role_data = module.load_role_export_data(db_path)
            synergies = {
                130: ["\u6c5f\u5357\u4e03\u602a", "\u8a66\u4f5c\u7d44"],
                131: ["\u6c5f\u5357\u4e03\u602a"],
            }

            module.write_csv(rows, output_path, role_data, synergies)

            with output_path.open("r", encoding="utf-8", newline="") as handle:
                csv_rows = list(csv.reader(handle))

        self.assertEqual(
            csv_rows,
            [
                ["id", "name", "star", "ult", "normal", "synergy_1", "synergy_2"],
                ["130", "\u67ef\u93ae\u60e1", "1", "\u4f0f\u9b54\u6756", "\u964d\u9f8d\u638c", "\u6c5f\u5357\u4e03\u602a", "\u8a66\u4f5c\u7d44"],
                ["131", "\u6731\u8070", "1", "\u7a7a\u660e\u62f3", "", "\u6c5f\u5357\u4e03\u602a", ""],
            ],
        )


if __name__ == "__main__":
    unittest.main()
