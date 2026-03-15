import shutil
import sqlite3
from pathlib import Path


DB_PATH = Path(__file__).resolve().parent.parent / "work" / "game-dev" / "save" / "0.db"
BACKUP_PATH = DB_PATH.with_suffix(".db.bak-new-magics")

ROLE_SKILL_COLUMNS = [
    "一星武功1",
    "一星武功2",
    "二星武功1",
    "二星武功2",
    "三星武功1",
    "三星武功2",
]

ROLE_POWER_COLUMNS = [
    "一星威力1",
    "一星威力2",
    "二星威力1",
    "二星威力2",
    "三星威力1",
    "三星威力2",
]

# Keep only the new magics that are actually used after reassignment.
# Keys are the current IDs before compaction.
MAGIC_FIXES = {
    126: {
        "名称": "五郎棍法",
        "出招音效": 7,
        "武功类型": 4,
        "武功动画": 52,
        "攻击范围类型": 2,
        "移动范围": 6,
        "杀伤范围": 0,
        "消耗内力": 2,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    131: {
        "名称": "青囊奇術",
        "出招音效": 1,
        "武功类型": 4,
        "武功动画": 31,
        "攻击范围类型": 2,
        "移动范围": 3,
        "杀伤范围": 0,
        "消耗内力": 6,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    132: {
        "名称": "藥王毒手",
        "出招音效": 2,
        "武功类型": 4,
        "武功动画": 34,
        "攻击范围类型": 0,
        "移动范围": 1,
        "杀伤范围": 0,
        "消耗内力": 6,
        "敌人中毒": 1,
        "加内力": 0,
        "杀伤内力": 0,
    },
    133: {
        "名称": "昊天掌",
        "出招音效": 12,
        "武功类型": 1,
        "武功动画": 95,
        "攻击范围类型": 3,
        "移动范围": 3,
        "杀伤范围": 1,
        "消耗内力": 2,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    136: {
        "名称": "寧式劍訣",
        "出招音效": 6,
        "武功类型": 2,
        "武功动画": 209,
        "攻击范围类型": 1,
        "移动范围": 2,
        "杀伤范围": 0,
        "消耗内力": 3,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    137: {
        "名称": "無極玄功拳",
        "出招音效": 12,
        "武功类型": 1,
        "武功动画": 4,
        "攻击范围类型": 2,
        "移动范围": 1,
        "杀伤范围": 1,
        "消耗内力": 3,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    138: {
        "名称": "兩頁刀法",
        "出招音效": 5,
        "武功类型": 3,
        "武功动画": 42,
        "攻击范围类型": 3,
        "移动范围": 6,
        "杀伤范围": 1,
        "消耗内力": 7,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    140: {
        "名称": "一炁化三清",
        "出招音效": 6,
        "武功类型": 2,
        "武功动画": 33,
        "攻击范围类型": 1,
        "移动范围": 8,
        "杀伤范围": 0,
        "消耗内力": 7,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    141: {
        "名称": "浣花流水劍",
        "出招音效": 6,
        "武功类型": 2,
        "武功动画": 50,
        "攻击范围类型": 1,
        "移动范围": 8,
        "杀伤范围": 0,
        "消耗内力": 5,
        "敌人中毒": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
}

# Use the pre-compaction magic IDs first, then remap them once the unused rows are deleted.
ROLE_UPDATES = {
    2: (131, 600, 132, 320),
    4: (138, 780, 52, 400),
    16: (131, 680, 132, 340),
    19: (136, 880, 48, 430),
    28: (131, 680, 132, 340),
    63: (141, 700, 27, 400),
    123: (133, 660, 137, 350),
    124: (140, 640, 39, 340),
    125: (140, 740, 39, 390),
    126: (133, 760, 137, 400),
    130: (126, 800, 77, 400),
}

KEPT_NEW_MAGIC_IDS = sorted(MAGIC_FIXES)
DELETE_MAGIC_IDS = [magic_id for magic_id in range(126, 142) if magic_id not in MAGIC_FIXES]
OLD_TO_NEW_MAGIC_IDS = {
    old_id: 126 + index for index, old_id in enumerate(KEPT_NEW_MAGIC_IDS)
}


def update_magic_rows(cur: sqlite3.Cursor) -> None:
    for magic_id, fields in MAGIC_FIXES.items():
        assignments = ", ".join(f"[{column}] = ?" for column in fields)
        values = list(fields.values()) + [magic_id]
        cur.execute(f"UPDATE magic SET {assignments} WHERE 编号 = ?", values)


def update_roles(cur: sqlite3.Cursor) -> None:
    for role_id, (magic1, power1, magic2, power2) in ROLE_UPDATES.items():
        params = []
        assignments = []
        for skill_col, power_col in zip(ROLE_SKILL_COLUMNS[0::2], ROLE_POWER_COLUMNS[0::2]):
            assignments.append(f"[{skill_col}] = ?")
            params.append(magic1)
            assignments.append(f"[{power_col}] = ?")
            params.append(power1)
        for skill_col, power_col in zip(ROLE_SKILL_COLUMNS[1::2], ROLE_POWER_COLUMNS[1::2]):
            assignments.append(f"[{skill_col}] = ?")
            params.append(magic2)
            assignments.append(f"[{power_col}] = ?")
            params.append(power2)
        params.append(role_id)
        cur.execute(f"UPDATE role SET {', '.join(assignments)} WHERE 编号 = ?", params)


def ensure_no_deleted_magic_refs(cur: sqlite3.Cursor) -> None:
    conditions = " OR ".join(f"[{column}] IN ({','.join('?' * len(DELETE_MAGIC_IDS))})" for column in ROLE_SKILL_COLUMNS)
    params = DELETE_MAGIC_IDS * len(ROLE_SKILL_COLUMNS)
    cur.execute(f"SELECT 编号, 名字 FROM role WHERE {conditions}", params)
    dangling = cur.fetchall()
    if dangling:
        details = ", ".join(f"{row[0]}:{row[1]}" for row in dangling)
        raise RuntimeError(f"deleted magic IDs still referenced by roles: {details}")


def remap_role_magic_ids(cur: sqlite3.Cursor) -> None:
    for old_id, new_id in OLD_TO_NEW_MAGIC_IDS.items():
        if old_id == new_id:
            continue
        for column in ROLE_SKILL_COLUMNS:
            cur.execute(f"UPDATE role SET [{column}] = ? WHERE [{column}] = ?", (new_id, old_id))


def compact_magic_ids(cur: sqlite3.Cursor) -> None:
    for old_id, new_id in OLD_TO_NEW_MAGIC_IDS.items():
        if old_id == new_id:
            continue
        cur.execute("UPDATE magic SET 编号 = ? WHERE 编号 = ?", (-new_id, old_id))

    if DELETE_MAGIC_IDS:
        cur.execute(
            f"DELETE FROM magic WHERE 编号 IN ({','.join('?' * len(DELETE_MAGIC_IDS))})",
            DELETE_MAGIC_IDS,
        )

    cur.execute("UPDATE magic SET 编号 = -编号 WHERE 编号 < 0")


def validate_final_state(cur: sqlite3.Cursor) -> None:
    cur.execute("SELECT 编号 FROM magic ORDER BY 编号")
    magic_ids = [row[0] for row in cur.fetchall()]
    expected = list(range(magic_ids[-1] + 1))
    if magic_ids != expected:
        raise RuntimeError(f"magic IDs are not contiguous after compaction: expected {expected[-10:]}, got {magic_ids[-10:]}")

    existing_magic_ids = set(magic_ids)
    role_placeholders = ",".join("?" * len(ROLE_UPDATES))
    role_columns = ", ".join(f"[{column}]" for column in ["编号", "名字", *ROLE_SKILL_COLUMNS])
    cur.execute(
        f"SELECT {role_columns} FROM role WHERE 编号 IN ({role_placeholders})",
        list(ROLE_UPDATES),
    )
    invalid_roles = []
    for row in cur.fetchall():
        role_id = row[0]
        role_name = row[1]
        for skill_id in row[2:]:
            if skill_id <= 0:
                continue
            if skill_id not in existing_magic_ids:
                invalid_roles.append((role_id, role_name, skill_id))
                break
    if invalid_roles:
        details = ", ".join(f"{role_id}:{role_name}->{skill_id}" for role_id, role_name, skill_id in invalid_roles)
        raise RuntimeError(f"updated roles reference missing magic IDs: {details}")


def main() -> None:
    if not DB_PATH.exists():
        raise FileNotFoundError(DB_PATH)

    shutil.copy2(DB_PATH, BACKUP_PATH)

    conn = sqlite3.connect(DB_PATH)
    try:
        cur = conn.cursor()
        cur.execute("BEGIN")
        update_magic_rows(cur)
        update_roles(cur)
        ensure_no_deleted_magic_refs(cur)
        remap_role_magic_ids(cur)
        compact_magic_ids(cur)
        validate_final_state(cur)
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

    print(f"Updated {DB_PATH}")
    print(f"Backup written to {BACKUP_PATH}")
    print(f"Kept new magic IDs: {OLD_TO_NEW_MAGIC_IDS}")
    print(f"Deleted new magic IDs: {DELETE_MAGIC_IDS}")


if __name__ == "__main__":
    main()