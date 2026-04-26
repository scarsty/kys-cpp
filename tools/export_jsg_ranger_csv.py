#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.jsg_legacy import ROOT, dedupe_rows, iter_jsg_rows


DEFAULT_OUTPUT = ROOT / "output" / "jsg_ranger_roles.csv"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Export JSG ranger.grp role ids and names to CSV")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="CSV output path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    rows = sorted(dedupe_rows(iter_jsg_rows()), key=lambda row: int(row["record_id"]))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "record_id",
                "head_id",
                "name",
                "nickname",
                "attack_frame",
                "attack_sound_frame",
                "idle_frames",
                "move_frames",
                "attack_frames",
                "hurt_frames",
                "death_frames",
            ],
        )
        writer.writeheader()
        for row in rows:
            frame_num = list(row["frame_num"])
            writer.writerow(
                {
                    "record_id": row["record_id"],
                    "head_id": row["head_id"],
                    "name": row["name"],
                    "nickname": row["nickname"],
                    "attack_frame": row["attack_frame"],
                    "attack_sound_frame": row["attack_sound_frame"],
                    "idle_frames": frame_num[0],
                    "move_frames": frame_num[1],
                    "attack_frames": frame_num[2],
                    "hurt_frames": frame_num[3],
                    "death_frames": frame_num[4],
                }
            )

    print(args.output)
    print(f"exported {len(rows)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())