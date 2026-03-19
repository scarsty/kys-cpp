from __future__ import annotations

import hashlib
import io
import json
import sqlite3
import sys
import zipfile
from pathlib import Path
from typing import Any

import yaml
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.derive_chess_identity_mappings import (  # noqa: E402
    JSG_ROOT,
    WORK_DB,
    YRJH_FIGHT_DIR,
    YRJH_HEAD_DIR,
    load_jsg_rows,
    load_yrjh_rows,
)


KEYWORDS = ("僧", "禅", "禪", "佛", "少林", "方丈", "高僧", "大师", "大師", "神僧", "掌門")


def image_hash_from_bytes(data: bytes) -> str:
    with Image.open(io.BytesIO(data)) as image:
        rgba = image.convert("RGBA")
        h = hashlib.sha256()
        h.update(str(rgba.size).encode("ascii"))
        h.update(rgba.tobytes())
        return h.hexdigest()


def zip_hash_from_bytes(data: bytes) -> str:
    h = hashlib.sha256()
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        for name in sorted(info.filename for info in zf.infolist() if not info.is_dir()):
            payload = zf.read(name)
            normalized_name = name.replace("\\", "/").lower()
            h.update(normalized_name.encode("utf-8"))
            if normalized_name.endswith(".png"):
                h.update(image_hash_from_bytes(payload).encode("ascii"))
            elif normalized_name.endswith("fightframe.txt"):
                text = payload.decode("utf-8", "ignore").replace("\r\n", "\n").strip()
                h.update(text.encode("utf-8"))
            else:
                h.update(payload)
    return h.hexdigest()


def find_fight_zip(base_dir: Path, fight_id: int) -> Path | None:
    for name in (f"fight{fight_id}.zip", f"fight{fight_id:03d}.zip", f"fight{fight_id:04d}.zip"):
        path = base_dir / name
        if path.exists():
            return path
    return None


def load_pool_roles() -> list[dict[str, Any]]:
    pool = yaml.safe_load((ROOT / "config" / "chess_pool.yaml").read_text(encoding="utf-8"))
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


def load_pool_hashes(pool_roles: list[dict[str, Any]]) -> tuple[set[str], set[str], set[int], set[str]]:
    head_hashes: set[str] = set()
    fight_hashes: set[str] = set()
    head_ids: set[int] = set()
    names: set[str] = set()
    for row in pool_roles:
        head_id = int(row["头像"])
        names.add(str(row["名字"]))
        head_ids.add(head_id)
        head_path = ROOT / "work" / "game-dev" / "resource" / "head" / f"{head_id}.png"
        if head_path.exists():
            head_hashes.add(image_hash_from_bytes(head_path.read_bytes()))
        fight_path = find_fight_zip(ROOT / "work" / "game-dev" / "resource" / "fight", head_id)
        if fight_path is not None:
            fight_hashes.add(zip_hash_from_bytes(fight_path.read_bytes()))
    return head_hashes, fight_hashes, head_ids, names


def is_monk_like(name: str, nickname: str) -> bool:
    text = f"{name} {nickname}"
    return any(keyword in text for keyword in KEYWORDS)


def add_yrjh_candidates(out: list[dict[str, Any]], used_names: set[str], used_head_ids: set[int], pool_head_hashes: set[str], pool_fight_hashes: set[str]) -> None:
    seen: set[tuple[str, int, int]] = set()
    for rows in load_yrjh_rows().values():
        for row in rows:
            name = str(row["name"])
            nickname = str(row["nickname"])
            key = (name, int(row["portrait_code"]), int(row["fight_code"]))
            if key in seen or not is_monk_like(name, nickname):
                continue
            seen.add(key)
            head_id = int(row["portrait_code"])
            fight_id = int(row["fight_code"])
            head_path = YRJH_HEAD_DIR / f"{head_id}.png"
            fight_path = find_fight_zip(YRJH_FIGHT_DIR, fight_id)
            if not head_path.exists() or fight_path is None:
                continue
            head_hash = image_hash_from_bytes(head_path.read_bytes())
            fight_hash = zip_hash_from_bytes(fight_path.read_bytes())
            out.append(
                {
                    "source": "人在江湖",
                    "name": name,
                    "nickname": nickname,
                    "head_id": head_id,
                    "fight_id": fight_id,
                    "name_in_pool": name in used_names,
                    "head_id_in_pool": head_id in used_head_ids,
                    "fight_id_in_pool": fight_id in used_head_ids,
                    "head_hash_in_pool": head_hash in pool_head_hashes,
                    "fight_hash_in_pool": fight_hash in pool_fight_hashes,
                }
            )


def add_jsg_candidates(out: list[dict[str, Any]], used_names: set[str], used_head_ids: set[int], pool_head_hashes: set[str], pool_fight_hashes: set[str]) -> None:
    with zipfile.ZipFile(JSG_ROOT / "data" / "head.zip") as head_zip:
        seen: set[tuple[str, int, int]] = set()
        for rows in load_jsg_rows().values():
            for row in rows:
                name = str(row["name"])
                nickname = str(row["nickname"])
                key = (name, int(row["record_id"]), int(row["head_id"]))
                if key in seen or not is_monk_like(name, nickname):
                    continue
                seen.add(key)
                portrait_id = int(row["record_id"])
                fight_id = int(row["head_id"])
                head_name = f"{portrait_id}.png"
                if head_name not in head_zip.namelist():
                    continue
                fight_path = find_fight_zip(JSG_ROOT / "data" / "fight", fight_id)
                if fight_path is None:
                    continue
                head_bytes = head_zip.read(head_name)
                head_hash = image_hash_from_bytes(head_bytes)
                fight_hash = zip_hash_from_bytes(fight_path.read_bytes())
                out.append(
                    {
                        "source": "金书",
                        "name": name,
                        "nickname": nickname,
                        "head_id": portrait_id,
                        "fight_id": fight_id,
                        "name_in_pool": name in used_names,
                        "head_id_in_pool": portrait_id in used_head_ids,
                        "fight_id_in_pool": fight_id in used_head_ids,
                        "head_hash_in_pool": head_hash in pool_head_hashes,
                        "fight_hash_in_pool": fight_hash in pool_fight_hashes,
                    }
                )


def main() -> None:
    pool_roles = load_pool_roles()
    pool_head_hashes, pool_fight_hashes, used_head_ids, used_names = load_pool_hashes(pool_roles)
    candidates: list[dict[str, Any]] = []
    add_yrjh_candidates(candidates, used_names, used_head_ids, pool_head_hashes, pool_fight_hashes)
    add_jsg_candidates(candidates, used_names, used_head_ids, pool_head_hashes, pool_fight_hashes)
    candidates.sort(
        key=lambda item: (
            item["name_in_pool"],
            item["head_hash_in_pool"],
            item["fight_hash_in_pool"],
            item["head_id_in_pool"],
            item["fight_id_in_pool"],
            item["source"],
            item["name"],
        )
    )
    print(json.dumps(candidates, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()