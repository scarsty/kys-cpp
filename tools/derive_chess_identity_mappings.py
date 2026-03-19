from __future__ import annotations

import csv
import json
import sqlite3
import struct
from pathlib import Path
from typing import Any

import yaml


ROOT = Path(__file__).resolve().parents[1]
CHESS_POOL = ROOT / "config" / "chess_pool.yaml"
WORK_DB = ROOT / "work" / "game-dev" / "save" / "0.db"
WORK_HEAD_DIR = ROOT / "work" / "game-dev" / "resource" / "head"
WORK_FIGHT_DIR = ROOT / "work" / "game-dev" / "resource" / "fight"

YRJH_ROOT = Path(r"D:\games\人在江湖cpp版\人在江湖cpp版")
YRJH_ROLE_CSV_DIR = YRJH_ROOT / "game" / "save" / "CSV"
YRJH_HEAD_DIR = YRJH_ROOT / "game" / "resource" / "head"
YRJH_FIGHT_DIR = YRJH_ROOT / "game" / "resource" / "fight"

JSG_ROOT = Path(r"D:\games\金书群侠传1.07V25")
JSG_RANGER_IDX = JSG_ROOT / "data" / "ranger.idx"
JSG_RANGER_GRP = JSG_ROOT / "data" / "ranger.grp"

JSG_ROLE_RECORD_SIZE = 1278
JSG_ROLE_ID_OFFSET = 0
JSG_ROLE_HEAD_OFFSET = 2
JSG_ROLE_NAME_OFFSET = 8
JSG_ROLE_NAME_SIZE = 10
JSG_ROLE_NICK_OFFSET = 18
JSG_ROLE_NICK_SIZE = 10


def to_simplified(text: str) -> str:
    t2s = str.maketrans({
        "張": "张", "無": "无", "忌": "忌", "楊": "杨", "逍": "逍", "謝": "谢", "遜": "逊",
        "韋": "韦", "東": "东", "敗": "败", "黃": "黄", "裳": "裳", "豐": "丰", "絕": "绝",
        "師": "师", "趙": "赵", "書": "书", "靈": "灵", "瑤": "瑶", "遠": "远", "橋": "桥",
        "蓮": "莲", "舟": "舟", "岱": "岱", "巖": "岩", "聲": "声", "禪": "禅", "馬": "马",
        "鈺": "钰", "處": "处", "譚": "谭", "劉": "刘", "閻": "阎", "歸": "归", "農": "农",
        "慕": "慕", "復": "复", "歐": "欧", "陽": "阳", "輪": "轮", "榮": "荣", "鳩": "鸠",
        "掃": "扫", "蘇": "苏", "遙": "遥", "臺": "台", "俠": "侠", "麗": "丽", "綺": "绮",
        "絲": "丝", "黛": "黛", "碧": "碧", "蓉": "蓉", "藥": "药", "燈": "灯", "聰": "聪",
        "韓": "韩", "瑩": "莹", "達": "达", "爾": "尔", "壩": "坝", "虛": "虚", "喬": "乔",
        "蕭": "萧", "譽": "誉", "龍": "龙", "滅": "灭", "綠": "绿", "風": "风", "雲": "云",
        "嶽": "岳", "葉": "叶", "後": "后", "國": "国", "實": "实", "兒": "儿", "雙": "双",
    })
    return text.translate(t2s)


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


def decode_cp950(raw: bytes) -> str:
    return raw.split(b"\x00", 1)[0].decode("cp950", errors="ignore").strip()


def load_jsg_rows() -> dict[str, list[dict[str, Any]]]:
    idx = JSG_RANGER_IDX.read_bytes()
    offsets = [int.from_bytes(idx[i:i + 4], "little") for i in range(0, len(idx), 4)]
    role_blob = JSG_RANGER_GRP.read_bytes()[offsets[0]:offsets[1]]
    assert len(role_blob) % JSG_ROLE_RECORD_SIZE == 0

    by_name: dict[str, list[dict[str, Any]]] = {}
    for start in range(0, len(role_blob), JSG_ROLE_RECORD_SIZE):
        record_id = int.from_bytes(role_blob[start + JSG_ROLE_ID_OFFSET:start + JSG_ROLE_ID_OFFSET + 2], "little", signed=True)
        head_id = int.from_bytes(role_blob[start + JSG_ROLE_HEAD_OFFSET:start + JSG_ROLE_HEAD_OFFSET + 2], "little", signed=True)
        name = decode_cp950(role_blob[start + JSG_ROLE_NAME_OFFSET:start + JSG_ROLE_NAME_OFFSET + JSG_ROLE_NAME_SIZE])
        nick = decode_cp950(role_blob[start + JSG_ROLE_NICK_OFFSET:start + JSG_ROLE_NICK_OFFSET + JSG_ROLE_NICK_SIZE])
        if not name:
            continue
        payload = {"record_id": record_id, "head_id": head_id, "name": name, "nickname": nick}
        by_name.setdefault(name, []).append(payload)
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