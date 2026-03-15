import shutil
import sqlite3
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DB_PATH = ROOT / "work" / "game-dev" / "save" / "0.db"
BACKUP_PATH = DB_PATH.with_suffix(".db.bak-invalid-role-magics")
EFT_DIR = ROOT / "work" / "game-dev" / "resource" / "eft"
SOUND_DIR = ROOT / "work" / "game-dev" / "sound"

MAGIC_COLUMNS = [
    "编号",
    "名称",
    "出招音效",
    "武功类型",
    "武功动画",
    "伤害类型",
    "攻击范围类型",
    "消耗内力",
    "敌人中毒",
    "移动范围",
    "杀伤范围",
    "加内力",
    "杀伤内力",
]

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

MAGIC_UPDATES = {
    115: {
        "名称": "摧心掌",
        "出招音效": 2,
        "武功类型": 1,
        "武功动画": 95,
        "伤害类型": 0,
        "攻击范围类型": 0,
        "消耗内力": 4,
        "敌人中毒": 0,
        "移动范围": 3,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    117: {
        "名称": "大力金刚掌",
        "出招音效": 2,
        "武功类型": 1,
        "武功动画": 31,
        "伤害类型": 0,
        "攻击范围类型": 0,
        "消耗内力": 5,
        "敌人中毒": 0,
        "移动范围": 4,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    118: {
        "名称": "大伏魔拳",
        "出招音效": 1,
        "武功类型": 1,
        "武功动画": 34,
        "伤害类型": 0,
        "攻击范围类型": 1,
        "消耗内力": 6,
        "敌人中毒": 0,
        "移动范围": 5,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    119: {
        "名称": "御剑要术",
        "出招音效": 6,
        "武功类型": 2,
        "武功动画": 33,
        "伤害类型": 0,
        "攻击范围类型": 1,
        "消耗内力": 2,
        "敌人中毒": 0,
        "移动范围": 3,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    121: {
        "名称": "柴刀十八路",
        "出招音效": 5,
        "武功类型": 3,
        "武功动画": 42,
        "伤害类型": 0,
        "攻击范围类型": 2,
        "消耗内力": 2,
        "敌人中毒": 0,
        "移动范围": 3,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    135: {
        "名称": "五行拳",
        "出招音效": 3,
        "武功类型": 1,
        "武功动画": 21,
        "伤害类型": 0,
        "攻击范围类型": 0,
        "消耗内力": 2,
        "敌人中毒": 0,
        "移动范围": 2,
        "杀伤范围": 0,
        "加内力": 0,
        "杀伤内力": 0,
    },
    136: {
        "名称": "劈卦刀",
        "出招音效": 5,
        "武功类型": 3,
        "武功动画": 52,
        "伤害类型": 0,
        "攻击范围类型": 3,
        "消耗内力": 4,
        "敌人中毒": 0,
        "移动范围": 4,
        "杀伤范围": 1,
        "加内力": 0,
        "杀伤内力": 0,
    },
}

ROLE_UPDATES = {
    590: {
        "skill1": 118,
        "power1": (1100, 1100, 1100),
        "skill2": 115,
        "power2": (400, 400, 400),
    },
    591: {
        "skill1": 5,
        "power1": (820, 820, 820),
        "skill2": 135,
        "power2": (580, 580, 580),
    },
    594: {
        "skill1": 38,
        "power1": (900, 900, 900),
        "skill2": 119,
        "power2": (390, 390, 390),
    },
    595: {
        "skill1": 36,
        "power1": (860, 860, 860),
        "skill2": 119,
        "power2": (500, 500, 500),
    },
    596: {
        "skill1": 136,
        "power1": (900, 900, 900),
        "skill2": 135,
        "power2": (500, 500, 500),
    },
    599: {
        "skill1": 16,
        "power1": (900, 900, 900),
        "skill2": 121,
        "power2": (500, 500, 500),
    },
    602: {
        "skill1": 117,
        "power1": (780, 840, 900),
        "skill2": 1,
        "power2": (600, 660, 720),
    },
}


def upsert_magic(cur: sqlite3.Cursor, magic_id: int, fields: dict[str, int | str]) -> None:
    assignments = ", ".join(f"[{column}] = ?" for column in fields)
    values = list(fields.values()) + [magic_id]
    cur.execute(f"UPDATE magic SET {assignments} WHERE 编号 = ?", values)
    if cur.rowcount:
        return

    row = {"编号": magic_id, **fields}
    placeholders = ", ".join("?" for _ in MAGIC_COLUMNS)
    columns = ", ".join(f"[{column}]" for column in MAGIC_COLUMNS)
    cur.execute(
        f"INSERT INTO magic ({columns}) VALUES ({placeholders})",
        [row[column] for column in MAGIC_COLUMNS],
    )


def update_magic_rows(cur: sqlite3.Cursor) -> None:
    for magic_id, fields in MAGIC_UPDATES.items():
        upsert_magic(cur, magic_id, fields)


def update_roles(cur: sqlite3.Cursor) -> None:
    for role_id, update in ROLE_UPDATES.items():
        params = [
            update["skill1"],
            update["power1"][0],
            update["skill2"],
            update["power2"][0],
            update["skill1"],
            update["power1"][1],
            update["skill2"],
            update["power2"][1],
            update["skill1"],
            update["power1"][2],
            update["skill2"],
            update["power2"][2],
            role_id,
        ]
        cur.execute(
            """
            UPDATE role
            SET [一星武功1] = ?, [一星威力1] = ?, [一星武功2] = ?, [一星威力2] = ?,
                [二星武功1] = ?, [二星威力1] = ?, [二星武功2] = ?, [二星威力2] = ?,
                [三星武功1] = ?, [三星威力1] = ?, [三星武功2] = ?, [三星威力2] = ?
            WHERE 编号 = ?
            """,
            params,
        )
        if not cur.rowcount:
            raise RuntimeError(f"missing role row: {role_id}")


def validate_assets() -> None:
    missing = []
    for magic_id, fields in MAGIC_UPDATES.items():
        sound_path = SOUND_DIR / f"atk{fields['出招音效']:02d}.wav"
        eft_path = EFT_DIR / f"eft{fields['武功动画']:03d}.zip"
        if not sound_path.exists():
            missing.append(f"magic {magic_id} missing sound {sound_path.name}")
        if not eft_path.exists():
            missing.append(f"magic {magic_id} missing effect {eft_path.name}")
    if missing:
        raise RuntimeError("asset validation failed: " + "; ".join(missing))


def validate_magic_ids(cur: sqlite3.Cursor) -> None:
    max_magic = cur.execute("SELECT MAX(编号) FROM magic").fetchone()[0]
    conditions = " OR ".join(f"[{column}] > ?" for column in ROLE_SKILL_COLUMNS)
    params = [max_magic] * len(ROLE_SKILL_COLUMNS)
    cur.execute(f"SELECT 编号, 名字 FROM role WHERE {conditions}", params)
    invalid_roles = cur.fetchall()
    if invalid_roles:
        details = ", ".join(f"{row[0]}:{row[1]}" for row in invalid_roles)
        raise RuntimeError(f"roles still reference magic IDs above max {max_magic}: {details}")


def validate_target_roles(cur: sqlite3.Cursor) -> None:
    existing_magic_ids = {row[0] for row in cur.execute("SELECT 编号 FROM magic")}
    placeholders = ", ".join("?" for _ in ROLE_UPDATES)
    cur.execute(
        f"SELECT 编号, 名字, {', '.join(f'[{col}]' for col in ROLE_SKILL_COLUMNS)} FROM role WHERE 编号 IN ({placeholders})",
        list(ROLE_UPDATES),
    )
    invalid = []
    for row in cur.fetchall():
        role_id = row[0]
        role_name = row[1]
        for skill_id in row[2:]:
            if skill_id <= 0:
                continue
            if skill_id not in existing_magic_ids:
                invalid.append(f"{role_id}:{role_name}->{skill_id}")
                break
    if invalid:
        raise RuntimeError("updated roles reference missing magic IDs: " + ", ".join(invalid))


def main() -> None:
    if not DB_PATH.exists():
        raise FileNotFoundError(DB_PATH)

    validate_assets()
    shutil.copy2(DB_PATH, BACKUP_PATH)

    conn = sqlite3.connect(DB_PATH)
    try:
        cur = conn.cursor()
        cur.execute("BEGIN")
        update_magic_rows(cur)
        update_roles(cur)
        validate_target_roles(cur)
        validate_magic_ids(cur)
        conn.commit()
    except Exception:
        conn.rollback()
        raise
    finally:
        conn.close()

    print(f"Updated {DB_PATH}")
    print(f"Backup written to {BACKUP_PATH}")


if __name__ == "__main__":
    main()