from __future__ import annotations

import argparse
import hashlib
import io
import json
import sqlite3
import sys
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.derive_chess_identity_mappings import (
    JSG_ROOT,
    WORK_DB,
    YRJH_FIGHT_DIR,
    YRJH_HEAD_DIR,
    load_jsg_rows,
    load_yrjh_rows,
    to_simplified,
)


WORK_RESOURCE_ROOT = ROOT / "work" / "game-dev" / "resource"
RESOURCE_ROOTS = [
    WORK_RESOURCE_ROOT,
    ROOT / "wasm" / "dist" / "game" / "resource",
    ROOT / "wasm" / "dist" / "kys" / "game" / "resource",
]
DB_PATHS = [
    WORK_DB,
    ROOT / "wasm" / "dist" / "game" / "save" / "0.db",
    ROOT / "wasm" / "dist" / "kys" / "game" / "save" / "0.db",
]
JSG_HEAD_ZIP = JSG_ROOT / "data" / "head.zip"
JSG_FIGHT_DIR = JSG_ROOT / "data" / "fight"
OUTPUT_JSON = ROOT / "output" / "chess_visual_choices.json"
OUTPUT_MD = ROOT / "output" / "chess_visual_choices.md"

TARGET_CONFIG: dict[int, dict[str, Any]] = {
    132: {},
}


@dataclass
class SourceAsset:
    source: str
    display_name: str
    head_id: int
    fight_id: int
    head_exists: bool
    fight_exists: bool
    head_hash: str | None
    fight_hash: str | None
    fight_png_count: int
    head_bytes: bytes | None
    fight_bytes: bytes | None
    note: str = ""

    @property
    def complete(self) -> bool:
        return self.head_exists and self.fight_exists and self.head_bytes is not None and self.fight_bytes is not None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Resolve chess role portrait/fight source choices and optionally apply them.")
    parser.add_argument("--apply", action="store_true", help="Copy chosen resources into the workspace and update DB rows when target ids change.")
    return parser.parse_args()


def load_target_roles() -> list[dict[str, Any]]:
    role_ids = sorted(TARGET_CONFIG)
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


def dedupe_rows(rows: list[dict[str, Any]], keys: tuple[str, ...]) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    seen: set[tuple[Any, ...]] = set()
    for row in rows:
        key = tuple(row[k] for k in keys)
        if key in seen:
            continue
        seen.add(key)
        out.append(row)
    return out


def find_fight_zip(base_dir: Path, fight_id: int) -> Path | None:
    candidates = [
        base_dir / f"fight{fight_id}.zip",
        base_dir / f"fight{fight_id:03d}.zip",
        base_dir / f"fight{fight_id:04d}.zip",
    ]
    for path in candidates:
        if path.exists():
            return path
    return None


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
        names = sorted(info.filename for info in zf.infolist() if not info.is_dir())
        for name in names:
            payload = zf.read(name)
            normalized_name = name.replace("\\", "/").lower()
            h.update(normalized_name.encode("utf-8"))
            if normalized_name.endswith(".png"):
                h.update(image_hash_from_bytes(payload).encode("ascii"))
            elif normalized_name.endswith("fightframe.txt"):
                normalized = payload.decode("utf-8", "ignore").replace("\r\n", "\n").strip()
                h.update(normalized.encode("utf-8"))
            else:
                h.update(payload)
    return h.hexdigest()


def zip_png_count_from_bytes(data: bytes) -> int:
    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        return sum(1 for info in zf.infolist() if not info.is_dir() and info.filename.lower().endswith(".png"))


def load_file_bytes(path: Path | None) -> bytes | None:
    if path is None or not path.exists():
        return None
    return path.read_bytes()


def load_jsg_head_bytes(head_id: int) -> bytes | None:
    with zipfile.ZipFile(JSG_HEAD_ZIP) as zf:
        entry = f"{head_id}.png"
        if entry not in zf.namelist():
            return None
        return zf.read(entry)


def make_yrjh_asset(display_name: str, head_id: int, fight_id: int, note: str = "") -> SourceAsset:
    head_path = YRJH_HEAD_DIR / f"{head_id}.png"
    fight_path = find_fight_zip(YRJH_FIGHT_DIR, fight_id)
    head_bytes = load_file_bytes(head_path)
    fight_bytes = load_file_bytes(fight_path)
    return SourceAsset(
        source="人在江湖",
        display_name=display_name,
        head_id=head_id,
        fight_id=fight_id,
        head_exists=head_bytes is not None,
        fight_exists=fight_bytes is not None,
        head_hash=image_hash_from_bytes(head_bytes) if head_bytes is not None else None,
        fight_hash=zip_hash_from_bytes(fight_bytes) if fight_bytes is not None else None,
        fight_png_count=zip_png_count_from_bytes(fight_bytes) if fight_bytes is not None else 0,
        head_bytes=head_bytes,
        fight_bytes=fight_bytes,
        note=note,
    )


def make_jsg_asset(display_name: str, head_id: int, note: str = "") -> SourceAsset:
    fight_path = find_fight_zip(JSG_FIGHT_DIR, head_id)
    head_bytes = load_jsg_head_bytes(head_id)
    fight_bytes = load_file_bytes(fight_path)
    return SourceAsset(
        source="金书",
        display_name=display_name,
        head_id=head_id,
        fight_id=head_id,
        head_exists=head_bytes is not None,
        fight_exists=fight_bytes is not None,
        head_hash=image_hash_from_bytes(head_bytes) if head_bytes is not None else None,
        fight_hash=zip_hash_from_bytes(fight_bytes) if fight_bytes is not None else None,
        fight_png_count=zip_png_count_from_bytes(fight_bytes) if fight_bytes is not None else 0,
        head_bytes=head_bytes,
        fight_bytes=fight_bytes,
        note=note,
    )


def pick_yrjh_row(name: str, yrjh_rows: dict[str, list[dict[str, Any]]]) -> dict[str, Any] | None:
    rows = yrjh_rows.get(name, []) or yrjh_rows.get(to_simplified(name), [])
    unique = dedupe_rows(rows, ("portrait_code", "fight_code"))
    return unique[0] if unique else None


def pick_jsg_row(name: str, jsg_rows: dict[str, list[dict[str, Any]]]) -> dict[str, Any] | None:
    rows = dedupe_rows(jsg_rows.get(name, []), ("head_id",))
    return rows[0] if rows else None


def choose_asset(yrjh_asset: SourceAsset | None, jsg_asset: SourceAsset | None) -> tuple[SourceAsset, str, bool | None, bool | None]:
    same_head = None
    same_fight = None
    if yrjh_asset is not None and jsg_asset is not None and yrjh_asset.complete and jsg_asset.complete:
        same_head = yrjh_asset.head_hash == jsg_asset.head_hash
        same_fight = yrjh_asset.fight_hash == jsg_asset.fight_hash
        if not same_head or not same_fight:
            if yrjh_asset.fight_png_count > jsg_asset.fight_png_count:
                return yrjh_asset, f"两边资源都完整且不一致，人在江湖战斗包 PNG 更多（{yrjh_asset.fight_png_count}>{jsg_asset.fight_png_count}）", same_head, same_fight
            if jsg_asset.fight_png_count > yrjh_asset.fight_png_count:
                return jsg_asset, f"两边资源都完整且不一致，金书战斗包 PNG 更多（{jsg_asset.fight_png_count}>{yrjh_asset.fight_png_count}）", same_head, same_fight
            return yrjh_asset, f"两边资源都完整且不一致，战斗包 PNG 数量相同（{yrjh_asset.fight_png_count}），保留人在江湖", same_head, same_fight
        return yrjh_asset, "两边资源完整且一致，保留人在江湖作为首选来源", same_head, same_fight
    if yrjh_asset is not None and yrjh_asset.complete:
        return yrjh_asset, "仅人在江湖来源完整", same_head, same_fight
    if jsg_asset is not None and jsg_asset.complete:
        return jsg_asset, "仅金书来源完整", same_head, same_fight
    missing: list[str] = []
    if yrjh_asset is not None and not yrjh_asset.complete:
        missing.append("人在江湖不完整")
    if jsg_asset is not None and not jsg_asset.complete:
        missing.append("金书不完整")
    raise RuntimeError("; ".join(missing) or "未找到可用外部资源")


def target_fight_filename(head_id: int) -> str:
    return f"fight{head_id:03d}.zip"


def write_if_changed(path: Path, data: bytes) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return "unchanged"
    path.write_bytes(data)
    return "written"


def update_role_head_id(db_path: Path, role_id: int, head_id: int) -> str:
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        current = cur.execute("select 头像 from role where 编号=?", (role_id,)).fetchone()
        if current is None:
            return "missing-role"
        if int(current[0]) == head_id:
            return "unchanged"
        cur.execute("update role set 头像=? where 编号=?", (head_id, role_id))
        conn.commit()
        return "updated"
    finally:
        conn.close()


def build_choice(role: dict[str, Any], yrjh_rows: dict[str, list[dict[str, Any]]], jsg_rows: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    role_id = int(role["编号"])
    name = str(role["名字"])
    current_head_id = int(role["头像"])
    cfg = TARGET_CONFIG[role_id]
    target_head_id = int(cfg.get("target_head_id", current_head_id))

    source_name = name
    substitute_note = ""
    if cfg.get("substitute_name"):
        source_name = str(cfg["substitute_name"])
        substitute_note = str(cfg.get("substitute_note", ""))

    yrjh_row = pick_yrjh_row(source_name, yrjh_rows)
    jsg_row = pick_jsg_row(source_name if not cfg.get("substitute_name") else source_name, jsg_rows)

    yrjh_asset = None
    if yrjh_row is not None:
        extra_note = substitute_note if substitute_note else ""
        yrjh_asset = make_yrjh_asset(source_name, int(yrjh_row["portrait_code"]), int(yrjh_row["fight_code"]), extra_note)

    jsg_asset = None
    if jsg_row is not None:
        extra_note = substitute_note if substitute_note else ""
        jsg_asset = make_jsg_asset(source_name, int(jsg_row["head_id"]), extra_note)

    chosen, reason, same_head, same_fight = choose_asset(yrjh_asset, jsg_asset)

    notes: list[str] = []
    if substitute_note:
        notes.append(substitute_note)
    if yrjh_asset is not None and yrjh_asset.head_id != yrjh_asset.fight_id:
        notes.append(f"人在江湖拆头身: 头像 {yrjh_asset.head_id}, 战斗 {yrjh_asset.fight_id}")
    if target_head_id != current_head_id:
        notes.append(f"当前头ID冲突，改用独占目标ID {target_head_id}")

    return {
        "role_id": role_id,
        "role_name": name,
        "source_lookup_name": source_name,
        "current_head_id": current_head_id,
        "target_head_id": target_head_id,
        "yrjh": None if yrjh_asset is None else {
            "head_id": yrjh_asset.head_id,
            "fight_id": yrjh_asset.fight_id,
            "complete": yrjh_asset.complete,
            "head_hash": yrjh_asset.head_hash,
            "fight_hash": yrjh_asset.fight_hash,
            "fight_png_count": yrjh_asset.fight_png_count,
            "note": yrjh_asset.note,
        },
        "jsg": None if jsg_asset is None else {
            "head_id": jsg_asset.head_id,
            "fight_id": jsg_asset.fight_id,
            "complete": jsg_asset.complete,
            "head_hash": jsg_asset.head_hash,
            "fight_hash": jsg_asset.fight_hash,
            "fight_png_count": jsg_asset.fight_png_count,
            "note": jsg_asset.note,
        },
        "comparison": {
            "same_head": same_head,
            "same_fight": same_fight,
        },
        "choice": {
            "source": chosen.source,
            "source_name": chosen.display_name,
            "head_id": chosen.head_id,
            "fight_id": chosen.fight_id,
            "fight_png_count": chosen.fight_png_count,
            "reason": reason,
        },
        "notes": notes,
        "_chosen_asset": chosen,
    }


def apply_choice(choice: dict[str, Any]) -> None:
    chosen: SourceAsset = choice["_chosen_asset"]
    target_head_id = int(choice["target_head_id"])

    resource_actions: list[dict[str, str]] = []
    for resource_root in RESOURCE_ROOTS:
        head_action = write_if_changed(resource_root / "head" / f"{target_head_id}.png", chosen.head_bytes or b"")
        fight_action = write_if_changed(resource_root / "fight" / target_fight_filename(target_head_id), chosen.fight_bytes or b"")
        resource_actions.append({
            "root": str(resource_root),
            "head_action": head_action,
            "fight_action": fight_action,
        })

    db_actions: list[dict[str, str]] = []
    for db_path in DB_PATHS:
        db_action = update_role_head_id(db_path, int(choice["role_id"]), target_head_id)
        db_actions.append({"db": str(db_path), "action": db_action})

    choice["apply"] = {
        "resource_actions": resource_actions,
        "db_actions": db_actions,
    }


def json_ready(choices: list[dict[str, Any]]) -> list[dict[str, Any]]:
    ready: list[dict[str, Any]] = []
    for choice in choices:
        data = dict(choice)
        data.pop("_chosen_asset", None)
        ready.append(data)
    return ready


def write_markdown(choices: list[dict[str, Any]]) -> None:
    lines = [
        "# Chess Visual Choices",
        "",
        "| 角色 | 目标ID | 选择 | 来源头像/战斗 | 头图是否一致 | 战斗是否一致 | 说明 |",
        "| --- | ---: | --- | --- | --- | --- | --- |",
    ]
    for choice in choices:
        comparison = choice["comparison"]
        lines.append(
            "| {name} | {target} | {source} | {head}/{fight} ({png_count}) | {same_head} | {same_fight} | {reason}{extra} |".format(
                name=choice["role_name"],
                target=choice["target_head_id"],
                source=choice["choice"]["source"],
                head=choice["choice"]["head_id"],
                fight=choice["choice"]["fight_id"],
                png_count=choice["choice"]["fight_png_count"],
                same_head="-" if comparison["same_head"] is None else ("是" if comparison["same_head"] else "否"),
                same_fight="-" if comparison["same_fight"] is None else ("是" if comparison["same_fight"] else "否"),
                reason=choice["choice"]["reason"],
                extra=("；" + "；".join(choice["notes"])) if choice["notes"] else "",
            )
        )
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    args = parse_args()
    yrjh_rows = load_yrjh_rows()
    jsg_rows = load_jsg_rows()
    roles = load_target_roles()

    choices = [build_choice(role, yrjh_rows, jsg_rows) for role in roles]
    if args.apply:
        for choice in choices:
            apply_choice(choice)

    OUTPUT_JSON.write_text(json.dumps({"choices": json_ready(choices)}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    write_markdown(json_ready(choices))
    print(OUTPUT_JSON)
    print(OUTPUT_MD)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())