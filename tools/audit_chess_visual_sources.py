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

from tools.add_chess_characters import TARGET_CHARACTERS  # noqa: E402
from tools.derive_chess_identity_mappings import (  # noqa: E402
    JSG_ROOT,
    WORK_DB,
    YRJH_FIGHT_DIR,
    YRJH_HEAD_DIR,
    load_jsg_rows,
    load_yrjh_rows,
    to_simplified,
)


WORK_RESOURCE_ROOT = ROOT / "work" / "game-dev" / "resource"
WORK_HEAD_DIR = WORK_RESOURCE_ROOT / "head"
WORK_FIGHT_DIR = WORK_RESOURCE_ROOT / "fight"
OUTPUT_JSON = ROOT / "output" / "chess_visual_audit.json"
OUTPUT_MD = ROOT / "output" / "chess_visual_audit.md"
JSG_HEAD_ZIP = JSG_ROOT / "data" / "head.zip"
POOL_CONFIGS = [ROOT / "config" / "chess_pool.yaml", ROOT / "config" / "chess_pool_easy.yaml"]


def load_pool_role_ids() -> list[int]:
    role_ids: set[int] = set()
    for config_path in POOL_CONFIGS:
        with config_path.open("r", encoding="utf-8") as handle:
            pool = yaml.safe_load(handle)
        for section in pool.values():
            if isinstance(section, list):
                role_ids.update(int(value) for value in section if isinstance(value, int))
    return sorted(role_ids)


def load_pool_roles() -> list[dict[str, Any]]:
    role_ids = load_pool_role_ids()
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


def find_fight_zip(base_dir: Path, fight_id: int) -> Path | None:
    for name in (f"fight{fight_id}.zip", f"fight{fight_id:03d}.zip", f"fight{fight_id:04d}.zip"):
        path = base_dir / name
        if path.exists():
            return path
    return None


def load_file_bytes(path: Path | None) -> bytes | None:
    if path is None or not path.exists():
        return None
    return path.read_bytes()


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


def current_asset(role: dict[str, Any]) -> dict[str, Any]:
    head_id = int(role["头像"])
    head_path = WORK_HEAD_DIR / f"{head_id}.png"
    fight_path = find_fight_zip(WORK_FIGHT_DIR, head_id)
    head_bytes = load_file_bytes(head_path)
    fight_bytes = load_file_bytes(fight_path)
    return {
        "head_id": head_id,
        "fight_id": head_id,
        "head_exists": head_bytes is not None,
        "fight_exists": fight_bytes is not None,
        "head_hash": image_hash_from_bytes(head_bytes) if head_bytes is not None else None,
        "fight_hash": zip_hash_from_bytes(fight_bytes) if fight_bytes is not None else None,
        "fight_png_count": zip_png_count_from_bytes(fight_bytes) if fight_bytes is not None else 0,
    }


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


def load_jsg_head_bytes(portrait_id: int) -> bytes | None:
    with zipfile.ZipFile(JSG_HEAD_ZIP) as zf:
        entry = f"{portrait_id}.png"
        if entry not in zf.namelist():
            return None
        return zf.read(entry)


def build_fallback_map() -> dict[str, dict[str, Any]]:
    fallback_map: dict[str, dict[str, Any]] = {}
    for spec in TARGET_CHARACTERS:
        payload = {
            "name": spec.name,
            "fallback_name": spec.fallback_name,
            "aliases": list(spec.aliases),
        }
        fallback_map[spec.name] = payload
        fallback_map[to_simplified(spec.name)] = payload
    return fallback_map


def collect_lookup_names(role_name: str, fallback_map: dict[str, dict[str, Any]]) -> list[dict[str, str]]:
    out: list[dict[str, str]] = []
    seen: set[tuple[str, str]] = set()

    def push(match_type: str, name: str) -> None:
        key = (match_type, name)
        if name and key not in seen:
            seen.add(key)
            out.append({"match_type": match_type, "lookup_name": name})

    push("exact", role_name)
    simplified = to_simplified(role_name)
    if simplified != role_name:
        push("simplified", simplified)

    spec = fallback_map.get(role_name) or fallback_map.get(simplified)
    if spec is not None:
        fallback_name = spec.get("fallback_name")
        if fallback_name:
            push("fallback", str(fallback_name))
            fallback_simplified = to_simplified(str(fallback_name))
            if fallback_simplified != fallback_name:
                push("fallback-simplified", fallback_simplified)
        for alias in spec.get("aliases", []):
            push("alias", str(alias))
            alias_simplified = to_simplified(str(alias))
            if alias_simplified != alias:
                push("alias-simplified", alias_simplified)
    return out


def collect_yrjh_candidates(role_name: str, fallback_map: dict[str, dict[str, Any]], yrjh_rows: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    seen: set[tuple[int, int]] = set()
    for lookup in collect_lookup_names(role_name, fallback_map):
        rows = dedupe_rows(yrjh_rows.get(lookup["lookup_name"], []), ("portrait_code", "fight_code"))
        for row in rows:
            key = (int(row["portrait_code"]), int(row["fight_code"]))
            if key in seen:
                continue
            seen.add(key)
            head_bytes = load_file_bytes(YRJH_HEAD_DIR / f"{row['portrait_code']}.png")
            fight_path = find_fight_zip(YRJH_FIGHT_DIR, int(row["fight_code"]))
            fight_bytes = load_file_bytes(fight_path)
            candidates.append(
                {
                    "source": "人在江湖",
                    "match_type": lookup["match_type"],
                    "lookup_name": lookup["lookup_name"],
                    "source_name": row["name"],
                    "head_id": int(row["portrait_code"]),
                    "fight_id": int(row["fight_code"]),
                    "complete": head_bytes is not None and fight_bytes is not None,
                    "head_hash": image_hash_from_bytes(head_bytes) if head_bytes is not None else None,
                    "fight_hash": zip_hash_from_bytes(fight_bytes) if fight_bytes is not None else None,
                    "fight_png_count": zip_png_count_from_bytes(fight_bytes) if fight_bytes is not None else 0,
                }
            )
    return candidates


def collect_jsg_candidates(role_name: str, fallback_map: dict[str, dict[str, Any]], jsg_rows: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    seen: set[tuple[int, int]] = set()
    for lookup in collect_lookup_names(role_name, fallback_map):
        rows = dedupe_rows(jsg_rows.get(lookup["lookup_name"], []), ("record_id", "head_id"))
        for row in rows:
            key = (int(row["record_id"]), int(row["head_id"]))
            if key in seen:
                continue
            seen.add(key)
            head_bytes = load_jsg_head_bytes(int(row["record_id"]))
            fight_path = find_fight_zip(JSG_ROOT / "data" / "fight", int(row["head_id"]))
            fight_bytes = load_file_bytes(fight_path)
            candidates.append(
                {
                    "source": "金书",
                    "match_type": lookup["match_type"],
                    "lookup_name": lookup["lookup_name"],
                    "source_name": row["name"],
                    "head_id": int(row["record_id"]),
                    "fight_id": int(row["head_id"]),
                    "complete": head_bytes is not None and fight_bytes is not None,
                    "head_hash": image_hash_from_bytes(head_bytes) if head_bytes is not None else None,
                    "fight_hash": zip_hash_from_bytes(fight_bytes) if fight_bytes is not None else None,
                    "fight_png_count": zip_png_count_from_bytes(fight_bytes) if fight_bytes is not None else 0,
                }
            )
    return candidates


def build_role_audit(role: dict[str, Any], fallback_map: dict[str, dict[str, Any]], yrjh_rows: dict[str, list[dict[str, Any]]], jsg_rows: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    current = current_asset(role)
    yrjh_candidates = collect_yrjh_candidates(str(role["名字"]), fallback_map, yrjh_rows)
    jsg_candidates = collect_jsg_candidates(str(role["名字"]), fallback_map, jsg_rows)
    candidate_matches: list[dict[str, Any]] = []
    for candidate in yrjh_candidates + jsg_candidates:
        candidate_matches.append(
            {
                **candidate,
                "head_match": candidate["head_hash"] is not None and candidate["head_hash"] == current["head_hash"],
                "fight_match": candidate["fight_hash"] is not None and candidate["fight_hash"] == current["fight_hash"],
                "full_match": candidate["head_hash"] is not None and candidate["fight_hash"] is not None and candidate["head_hash"] == current["head_hash"] and candidate["fight_hash"] == current["fight_hash"],
            }
        )
    return {
        "role_id": int(role["编号"]),
        "role_name": str(role["名字"]),
        "current": current,
        "matching_candidates": [item for item in candidate_matches if item["head_match"] or item["fight_match"]],
        "yrjh_candidates": yrjh_candidates,
        "jsg_candidates": jsg_candidates,
    }


def build_duplicate_groups(role_reports: list[dict[str, Any]], field: str) -> list[dict[str, Any]]:
    groups: dict[str, list[dict[str, Any]]] = {}
    for report in role_reports:
        value = report["current"][field]
        if value is None:
            continue
        groups.setdefault(str(value), []).append(
            {
                "role_id": report["role_id"],
                "role_name": report["role_name"],
                "head_id": report["current"]["head_id"],
                "fight_id": report["current"]["fight_id"],
            }
        )
    return [
        {"hash": key, "roles": value}
        for key, value in sorted(groups.items(), key=lambda item: (len(item[1]) * -1, item[1][0]["role_id"]))
        if len(value) > 1
    ]


def write_markdown(report: dict[str, Any]) -> None:
    OUTPUT_MD.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Chess Visual Audit",
        "",
        f"- 角色数: {report['summary']['role_count']}",
        f"- 重复头像组: {report['summary']['duplicate_portrait_group_count']}",
        f"- 重复战斗资源组: {report['summary']['duplicate_fight_group_count']}",
        "",
        "## 重复头像",
        "",
        "| 共享角色 | 当前头ID | 匹配来源 |",
        "| --- | --- | --- |",
    ]
    for group in report["duplicate_portraits"]:
        roles = " / ".join(f"{item['role_name']}({item['role_id']})" for item in group["roles"])
        head_ids = "/".join(str(item["head_id"]) for item in group["roles"])
        matched_sources: list[str] = []
        for item in group["roles"]:
            role_report = report["by_role_id"][str(item["role_id"])]
            for match in role_report["matching_candidates"]:
                if match["head_match"]:
                    matched_sources.append(f"{item['role_name']}->{match['source']}:{match['source_name']}({match['head_id']}/{match['fight_id']})")
        lines.append(f"| {roles} | {head_ids} | {'；'.join(matched_sources) if matched_sources else '-'} |")

    lines.extend([
        "",
        "## 重复战斗资源",
        "",
        "| 共享角色 | 当前头ID | 匹配来源 |",
        "| --- | --- | --- |",
    ])
    for group in report["duplicate_fights"]:
        roles = " / ".join(f"{item['role_name']}({item['role_id']})" for item in group["roles"])
        head_ids = "/".join(str(item['head_id']) for item in group["roles"])
        matched_sources: list[str] = []
        for item in group["roles"]:
            role_report = report["by_role_id"][str(item["role_id"])]
            for match in role_report["matching_candidates"]:
                if match["fight_match"]:
                    matched_sources.append(f"{item['role_name']}->{match['source']}:{match['source_name']}({match['head_id']}/{match['fight_id']})")
        lines.append(f"| {roles} | {head_ids} | {'；'.join(matched_sources) if matched_sources else '-'} |")

    lines.extend([
        "",
        "## 重点角色",
        "",
        "| 角色 | 当前头ID | 当前匹配来源 |",
        "| --- | ---: | --- |",
    ])
    for report_item in report["roles"]:
        matches = [
            f"{match['source']}:{match['source_name']} 头{match['head_id']} 战{match['fight_id']}"
            for match in report_item["matching_candidates"]
            if match["full_match"] or match["head_match"] or match["fight_match"]
        ]
        if not matches:
            continue
        lines.append(f"| {report_item['role_name']}({report_item['role_id']}) | {report_item['current']['head_id']} | {'；'.join(matches)} |")

    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    fallback_map = build_fallback_map()
    yrjh_rows = load_yrjh_rows()
    jsg_rows = load_jsg_rows()
    roles = [build_role_audit(role, fallback_map, yrjh_rows, jsg_rows) for role in load_pool_roles()]
    duplicate_portraits = build_duplicate_groups(roles, "head_hash")
    duplicate_fights = build_duplicate_groups(roles, "fight_hash")
    report = {
        "summary": {
            "role_count": len(roles),
            "duplicate_portrait_group_count": len(duplicate_portraits),
            "duplicate_fight_group_count": len(duplicate_fights),
        },
        "duplicate_portraits": duplicate_portraits,
        "duplicate_fights": duplicate_fights,
        "roles": roles,
    }
    report["by_role_id"] = {str(item["role_id"]): item for item in roles}

    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    write_markdown(report)
    print(OUTPUT_JSON)
    print(OUTPUT_MD)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())