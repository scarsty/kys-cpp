from __future__ import annotations

import json
import re
import sqlite3
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

try:
    from PIL import Image
except Exception:
    Image = None


ROOT = Path(__file__).resolve().parents[1]
CHESS_POOL = ROOT / "config" / "chess_pool.yaml"
WORK_DB = ROOT / "work" / "game-dev" / "save" / "0.db"
WORK_FIGHT_DIR = ROOT / "work" / "game-dev" / "resource" / "fight"
WORK_HEAD_DIR = ROOT / "work" / "game-dev" / "resource" / "head"
OLD_MOD_DIR = Path(r"D:\games\金书群侠传1.07V25")
OLD_MOD_DB = OLD_MOD_DIR / "save" / "0.db"
OLD_MOD_FIGHT_DIR = OLD_MOD_DIR / "data" / "fight"


ROLE_LINE_RE = re.compile(r"^\s*-\s*(\d+)\s*#\s*(.+?)\s*$")


@dataclass
class RoleAudit:
    role_id: int
    pool_name: str
    db_name: str | None
    head_id: int | None
    sexual: int | None
    cost: int | None
    magic_types: list[int]
    fight_zip_exists: bool
    fightframe: dict[int, int]
    missing_styles: list[int]
    empty: bool


def parse_pool_ids(path: Path) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        match = ROLE_LINE_RE.match(line)
        if match:
            result.append((int(match.group(1)), match.group(2).strip()))
    return result


def load_role_rows(db_path: Path) -> dict[int, dict[str, Any]]:
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    try:
        rows = conn.execute("select * from Role").fetchall()
        return {int(row[0]): dict(row) for row in rows}
    finally:
        conn.close()


def normalize_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except Exception:
        return None


def collect_magic_types(row: dict[str, Any]) -> list[int]:
    magic_ids: list[int] = []
    for key, value in row.items():
        if "武功" in key and "威力" not in key:
            ivalue = normalize_int(value)
            if ivalue and ivalue > 0:
                magic_ids.append(ivalue)
    if not magic_ids:
        return []

    conn = sqlite3.connect(WORK_DB)
    conn.row_factory = sqlite3.Row
    try:
        placeholder = ",".join("?" for _ in magic_ids)
        query = f"select 编号, 武功类型 from Magic where 编号 in ({placeholder})"
        rows = conn.execute(query, magic_ids).fetchall()
        return sorted({int(r[1]) for r in rows if normalize_int(r[1]) is not None})
    finally:
        conn.close()


def read_fightframe(fight_dir: Path, head_id: int | None) -> tuple[bool, dict[int, int]]:
    if head_id is None:
        return False, {}
    zip_path = None
    for name in (f"fight{head_id}.zip", f"fight{head_id:03}.zip", f"fight{head_id:04}.zip"):
        candidate = fight_dir / name
        if candidate.exists():
            zip_path = candidate
            break
    if zip_path is None:
        return False, {}
    with zipfile.ZipFile(zip_path) as zf:
        if "fightframe.txt" not in zf.namelist():
            return True, {}
        content = zf.read("fightframe.txt").decode("utf-8", "ignore")
    nums = [int(n) for n in re.findall(r"-?\d+", content)]
    frame_map: dict[int, int] = {}
    for idx in range(0, len(nums) - 1, 2):
        frame_map[nums[idx]] = nums[idx + 1]
    return True, frame_map


def average_hash(path: Path, size: int = 8) -> list[int] | None:
    if Image is None or not path.exists():
        return None
    with Image.open(path) as image:
        image = image.convert("L").resize((size, size))
        pixels = list(image.getdata())
    avg = sum(pixels) / len(pixels)
    return [1 if px >= avg else 0 for px in pixels]


def hash_distance(lhs: list[int] | None, rhs: list[int] | None) -> int | None:
    if lhs is None or rhs is None or len(lhs) != len(rhs):
        return None
    return sum(1 for a, b in zip(lhs, rhs) if a != b)


def audit_roles() -> dict[str, Any]:
    pool_roles = parse_pool_ids(CHESS_POOL)
    work_roles = load_role_rows(WORK_DB)
    old_roles = load_role_rows(OLD_MOD_DB) if OLD_MOD_DB.exists() else {}

    all_head_hashes: dict[int, list[int] | None] = {}
    for row in work_roles.values():
        head_id = normalize_int(row.get("头像"))
        if head_id is None or head_id in all_head_hashes:
            continue
        all_head_hashes[head_id] = average_hash(WORK_HEAD_DIR / f"{head_id}.png")

    audits: list[RoleAudit] = []
    by_head: dict[int, list[int]] = {}

    for role_id, pool_name in pool_roles:
        row = work_roles.get(role_id, {})
        head_id = normalize_int(row.get("头像"))
        magic_types = collect_magic_types(row) if row else []
        fight_zip_exists, fightframe = read_fightframe(WORK_FIGHT_DIR, head_id)
        missing_styles = [style for style in range(5) if fightframe.get(style, 0) <= 0]
        audit = RoleAudit(
            role_id=role_id,
            pool_name=pool_name,
            db_name=str(row.get("名字")) if row else None,
            head_id=head_id,
            sexual=normalize_int(row.get("性别")) if row else None,
            cost=normalize_int(row.get("费用")) if row else None,
            magic_types=magic_types,
            fight_zip_exists=fight_zip_exists,
            fightframe=fightframe,
            missing_styles=missing_styles,
            empty=not fightframe or all(v <= 0 for v in fightframe.values()),
        )
        audits.append(audit)
        if head_id is not None:
            by_head.setdefault(head_id, []).append(role_id)

    old_role_index = {}
    for role_id, row in old_roles.items():
        old_role_index[role_id] = {
            "name": row.get("名字"),
            "head_id": normalize_int(row.get("头像")),
            "sexual": normalize_int(row.get("性别")),
            "cost": normalize_int(row.get("费用")),
        }

    candidate_rows: list[dict[str, Any]] = []
    for row in work_roles.values():
        head_id = normalize_int(row.get("头像"))
        if head_id is None:
            continue
        fight_zip_exists, fightframe = read_fightframe(WORK_FIGHT_DIR, head_id)
        candidate_rows.append(
            {
                "role_id": normalize_int(row.get("编号")),
                "name": row.get("名字"),
                "head_id": head_id,
                "sexual": normalize_int(row.get("性别")),
                "fight_zip_exists": fight_zip_exists,
                "fightframe": fightframe,
            }
        )

    suggestions = []
    for a in audits:
        missing_magic_styles = [style for style in a.magic_types if a.fightframe.get(style, 0) <= 0]
        if not missing_magic_styles:
            continue
        src_hash = all_head_hashes.get(a.head_id or -1)
        candidates = []
        for cand in candidate_rows:
            if cand["head_id"] == a.head_id or not cand["fight_zip_exists"]:
                continue
            cand_frame = cand["fightframe"]
            covered = sum(1 for style in a.magic_types if cand_frame.get(style, 0) > 0)
            if covered <= 0:
                continue
            if a.sexual is not None and cand["sexual"] is not None and a.sexual != cand["sexual"]:
                continue
            distance = hash_distance(src_hash, all_head_hashes.get(cand["head_id"]))
            candidates.append(
                {
                    "candidate_role_id": cand["role_id"],
                    "candidate_name": cand["name"],
                    "candidate_head_id": cand["head_id"],
                    "covered_styles": [style for style in a.magic_types if cand_frame.get(style, 0) > 0],
                    "fightframe": cand_frame,
                    "hash_distance": distance,
                }
            )
        candidates.sort(
            key=lambda item: (
                -len(item["covered_styles"]),
                999 if item["hash_distance"] is None else item["hash_distance"],
                item["candidate_head_id"],
            )
        )
        suggestions.append(
            {
                "role_id": a.role_id,
                "name": a.pool_name,
                "head_id": a.head_id,
                "missing_magic_styles": missing_magic_styles,
                "candidates": candidates[:8],
            }
        )

    summary = {
        "pool_count": len(audits),
        "missing_fight_zip": [a.role_id for a in audits if not a.fight_zip_exists],
        "empty_fightframe": [a.role_id for a in audits if a.empty],
        "missing_required_style": [
            {
                "role_id": a.role_id,
                "name": a.pool_name,
                "head_id": a.head_id,
                "magic_types": a.magic_types,
                "missing_magic_styles": [style for style in a.magic_types if a.fightframe.get(style, 0) <= 0],
                "fightframe": a.fightframe,
            }
            for a in audits
            if any(a.fightframe.get(style, 0) <= 0 for style in a.magic_types)
        ],
        "special_role_ids": [a.role_id for a in audits if a.role_id >= 500],
        "head_reuse": {str(head): ids for head, ids in sorted(by_head.items()) if len(ids) > 1},
        "candidate_suggestions": suggestions,
        "roles": [
            {
                "role_id": a.role_id,
                "pool_name": a.pool_name,
                "db_name": a.db_name,
                "head_id": a.head_id,
                "sexual": a.sexual,
                "cost": a.cost,
                "magic_types": a.magic_types,
                "fight_zip_exists": a.fight_zip_exists,
                "fightframe": a.fightframe,
                "missing_styles": a.missing_styles,
                "empty": a.empty,
                "old_mod": old_role_index.get(a.role_id),
            }
            for a in audits
        ],
    }
    return summary


def main() -> None:
    result = audit_roles()
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()