from __future__ import annotations

import hashlib
import csv
import json
import sqlite3
import sys
from pathlib import Path
from typing import Any

import yaml

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.jsg_legacy import JSG_ROOT, JSG_RANGER_GRP, JSG_RANGER_IDX, fightframe_hash_from_values, fightframe_text_from_values, load_jsg_rows, read_i16_values, to_simplified


CHESS_POOL = ROOT / "config" / "chess_pool.yaml"
WORK_DB = ROOT / "work" / "game-dev" / "save" / "0.db"
WORK_HEAD_DIR = ROOT / "work" / "game-dev" / "resource" / "head"
WORK_FIGHT_DIR = ROOT / "work" / "game-dev" / "resource" / "fight"

YRJH_ROOT = Path(r"D:\games\人在江湖cpp版\人在江湖cpp版")
YRJH_ROLE_CSV_DIR = YRJH_ROOT / "game" / "save" / "CSV"
YRJH_HEAD_DIR = YRJH_ROOT / "game" / "resource" / "head"
YRJH_FIGHT_DIR = YRJH_ROOT / "game" / "resource" / "fight"

JSG_ROOT = Path(r"D:\games\金书群侠传1.07V25")


def fight_exists(fight_dir: Path, head_id: int | None) -> bool:
    if head_id is None:
        return False
    for name in (f"fight{head_id}.zip", f"fight{head_id:03}.zip", f"fight{head_id:04}.zip"):
        if (fight_dir / name).exists():
            return True
    return False


def head_exists(head_dir: Path, head_id: int | None) -> bool:
    return head_id is not None and (head_dir / f"{head_id}.png").exists()


def load_chess_roles() -> list[dict[str, Any]]:
    with CHESS_POOL.open("r", encoding="utf-8") as f:
        pool = yaml.safe_load(f)
    role_ids = sorted({int(v) for section in pool.values() if isinstance(section, list) for v in section if isinstance(v, int)})

    conn = sqlite3.connect(WORK_DB)
    conn.row_factory = sqlite3.Row
    try:
        rows = conn.execute(
            "select 编号, 名字, 头像 from role where 编号 in ({}) order by 编号".format(",".join("?" for _ in role_ids)),
            role_ids,
        ).fetchall()
        return [dict(row) for row in rows]
    finally:
        conn.close()


def load_workspace_head_users() -> dict[int, list[dict[str, Any]]]:
    conn = sqlite3.connect(WORK_DB)
    conn.row_factory = sqlite3.Row
    try:
        users: dict[int, list[dict[str, Any]]] = {}
        for row in conn.execute("select 编号, 名字, 头像 from role order by 编号"):
            users.setdefault(int(row["头像"]), []).append({"role_id": int(row["编号"]), "name": row["名字"]})
        return users
    finally:
        conn.close()


def load_yrjh_rows() -> dict[str, list[dict[str, Any]]]:
    by_name: dict[str, list[dict[str, Any]]] = {}
    for csv_path in sorted(YRJH_ROLE_CSV_DIR.glob("*_renwu.csv")):
        with csv_path.open("r", encoding="utf-8-sig", newline="") as f:
            for row in csv.DictReader(f):
                name = row["姓名"].strip()
                payload = {
                    "source_csv": csv_path.name,
                    "record_id": int(row["编号"]),
                    "portrait_code": int(row["头像/战斗代号"]),
                    "fight_code": int(row["战斗图编号"]),
                    "name": name,
                    "nickname": row["外号"].strip(),
                }
                for key in {name, to_simplified(name)}:
                    bucket = by_name.setdefault(key, [])
                    if payload not in bucket:
                        bucket.append(payload)
    return by_name
def build_role_report(role: dict[str, Any], yrjh_rows: dict[str, list[dict[str, Any]]], jsg_rows: dict[str, list[dict[str, Any]]], workspace_head_users: dict[int, list[dict[str, Any]]]) -> dict[str, Any]:
    role_id = int(role["编号"])
    name = str(role["名字"])
    current_head = int(role["头像"])
    simp_name = to_simplified(name)

    yrjh = yrjh_rows.get(name, []) or yrjh_rows.get(simp_name, [])
    jsg = jsg_rows.get(name, [])

    candidate_codes = set()
    for row in yrjh:
        candidate_codes.add(row["portrait_code"])
        candidate_codes.add(row["fight_code"])
    for row in jsg:
        candidate_codes.add(row["record_id"])
        candidate_codes.add(row["head_id"])

    facts: list[str] = []
    if yrjh:
        row = yrjh[0]
        if row["portrait_code"] == row["fight_code"]:
            facts.append(f"人在江湖单键ID={row['portrait_code']}")
        else:
            facts.append(f"人在江湖拆分: 头像={row['portrait_code']} 战斗={row['fight_code']}")
    if jsg:
        portrait_ids = sorted({row["record_id"] for row in jsg})
        fight_ids = sorted({row["head_id"] for row in jsg})
        if portrait_ids == fight_ids:
            facts.append("金书单键ID=" + "/".join(str(v) for v in portrait_ids))
        else:
            facts.append("金书拆分: 头像=" + "/".join(str(v) for v in portrait_ids) + " 战斗=" + "/".join(str(v) for v in fight_ids))
    if current_head in candidate_codes:
        facts.append("当前HeadID命中外部来源")
    else:
        facts.append("当前HeadID未命中外部来源")

    collisions = []
    for row in yrjh:
        portrait_code = row["portrait_code"]
        users = [u for u in workspace_head_users.get(portrait_code, []) if u["role_id"] != role_id]
        if users:
            collisions.append({"code": portrait_code, "users": users})

    return {
        "role_id": role_id,
        "name": name,
        "simplified_name": simp_name,
        "current_head_id": current_head,
        "current_head_exists": head_exists(WORK_HEAD_DIR, current_head),
        "current_fight_exists": fight_exists(WORK_FIGHT_DIR, current_head),
        "yrjh": [
            {
                **row,
                "portrait_head_exists": head_exists(YRJH_HEAD_DIR, row["portrait_code"]),
                "portrait_fight_exists": fight_exists(YRJH_FIGHT_DIR, row["portrait_code"]),
                "fight_head_exists": head_exists(YRJH_HEAD_DIR, row["fight_code"]),
                "fight_fight_exists": fight_exists(YRJH_FIGHT_DIR, row["fight_code"]),
            }
            for row in yrjh
        ],
        "jsg": jsg,
        "workspace_head_collisions": collisions,
        "facts": facts,
    }


def main() -> None:
    roles = load_chess_roles()
    workspace_head_users = load_workspace_head_users()
    yrjh_rows = load_yrjh_rows()
    jsg_rows = load_jsg_rows()
    report = {
        "roles": [build_role_report(role, yrjh_rows, jsg_rows, workspace_head_users) for role in roles],
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()