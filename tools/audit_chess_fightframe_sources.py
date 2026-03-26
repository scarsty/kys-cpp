from __future__ import annotations

import argparse
import hashlib
import io
import json
import re
import sqlite3
import sys
import zipfile
from pathlib import Path
from typing import Any

import yaml


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.add_chess_characters import TARGET_CHARACTERS  # noqa: E402
from tools.derive_chess_identity_mappings import (  # noqa: E402
    JSG_ROOT,
    WORK_DB,
    WORK_FIGHT_DIR,
    fightframe_hash_from_values,
    fightframe_text_from_values,
    load_jsg_rows,
    to_simplified,
)


CHESS_POOL = ROOT / "config" / "chess_pool.yaml"
OUTPUT_JSON = ROOT / "output" / "chess_fightframe_audit.json"
OUTPUT_MD = ROOT / "output" / "chess_fightframe_audit.md"

MATCH_PRIORITY = {
    "exact": 0,
    "simplified": 1,
    "fallback": 2,
    "fallback-simplified": 3,
    "alias": 4,
    "alias-simplified": 5,
}


def image_hash_from_bytes(data: bytes) -> str:
    from PIL import Image

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
            if normalized_name.endswith("fightframe.txt"):
                continue
            h.update(normalized_name.encode("utf-8"))
            if normalized_name.endswith(".png"):
                h.update(image_hash_from_bytes(payload).encode("ascii"))
            else:
                h.update(payload)
    return h.hexdigest()


def load_pool_role_ids() -> list[int]:
    role_ids: set[int] = set()
    for config_path in (ROOT / "config" / "chess_pool.yaml", ROOT / "config" / "chess_pool_easy.yaml"):
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


def parse_fightframe_text(text: str) -> list[int]:
    values = [0, 0, 0, 0, 0]
    numbers = [int(value) for value in re.findall(r"-?\d+", text)]
    for index in range(0, len(numbers) - 1, 2):
        style = numbers[index]
        frame_count = numbers[index + 1]
        if 0 <= style < 5 and frame_count > 0:
            values[style] = frame_count
    return values


def read_local_fightframe(fight_path: Path | None) -> dict[str, Any]:
    if fight_path is None or not fight_path.exists():
        return {
            "fight_zip_exists": False,
            "fightframe_file_exists": False,
            "resource_hash": None,
            "fightframe_values": [0, 0, 0, 0, 0],
            "fightframe_text": "",
            "fightframe_hash": None,
        }

    data = fight_path.read_bytes()
    resource_hash = zip_hash_from_bytes(data)

    with zipfile.ZipFile(io.BytesIO(data)) as zf:
        names = {info.filename.replace("\\", "/").lower() for info in zf.infolist() if not info.is_dir()}
        if "fightframe.txt" not in names:
            return {
                "fight_zip_exists": True,
                "fightframe_file_exists": False,
                "resource_hash": resource_hash,
                "fightframe_values": [0, 0, 0, 0, 0],
                "fightframe_text": "",
                "fightframe_hash": None,
            }
        raw_text = zf.read("fightframe.txt").decode("utf-8", "ignore").replace("\r\n", "\n").strip()

    values = parse_fightframe_text(raw_text)
    canonical_text = fightframe_text_from_values(values)
    return {
        "fight_zip_exists": True,
        "fightframe_file_exists": True,
        "resource_hash": resource_hash,
        "fightframe_values": values,
        "fightframe_text": canonical_text,
        "fightframe_hash": fightframe_hash_from_values(values),
    }


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
            values = [int(value) for value in row.get("frame_num", [])]
            candidates.append(
                {
                    "source": "金书",
                    "match_type": lookup["match_type"],
                    "lookup_name": lookup["lookup_name"],
                    "source_name": row["name"],
                    "source_nickname": row.get("nickname"),
                    "record_id": int(row["record_id"]),
                    "head_id": int(row["head_id"]),
                    "fightframe_values": values,
                    "fightframe_text": row.get("fightframe_text") or fightframe_text_from_values(values),
                    "fightframe_hash": row.get("fightframe_hash") or fightframe_hash_from_values(values),
                }
            )
    return candidates


def select_source_candidate(local_resource_hash: str | None, local_fightframe_hash: str | None, candidates: list[dict[str, Any]]) -> dict[str, Any] | None:
    if not candidates:
        return None

    def priority(candidate: dict[str, Any]) -> tuple[int, int, int]:
        return (
            MATCH_PRIORITY.get(str(candidate["match_type"]), 99),
            int(candidate["record_id"]),
            int(candidate["head_id"]),
        )

    if local_resource_hash is not None:
        matching = [candidate for candidate in candidates if candidate["resource_hash"] == local_resource_hash]
        if matching:
            return sorted(matching, key=priority)[0]

    if local_fightframe_hash is not None:
        matching = [candidate for candidate in candidates if candidate["fightframe_hash"] == local_fightframe_hash]
        if matching:
            return sorted(matching, key=priority)[0]

    resource_hashes = {candidate["resource_hash"] for candidate in candidates if candidate["resource_hash"]}
    if len(resource_hashes) == 1:
        return sorted(candidates, key=priority)[0]

    unique_hashes = {candidate["fightframe_hash"] for candidate in candidates if candidate["fightframe_hash"]}
    if len(unique_hashes) == 1:
        return sorted(candidates, key=priority)[0]

    return None


def write_fightframe_txt(zip_path: Path, fightframe_text: str) -> None:
    with zipfile.ZipFile(zip_path, "a") as zf:
        zf.writestr("fightframe.txt", fightframe_text)


def build_role_report(role: dict[str, Any], fallback_map: dict[str, dict[str, Any]], jsg_rows: dict[str, list[dict[str, Any]]]) -> dict[str, Any]:
    role_id = int(role["编号"])
    role_name = str(role["名字"])
    head_id = int(role["头像"])
    fight_path = find_fight_zip(WORK_FIGHT_DIR, head_id)

    current = {
        "fight_id": head_id,
        "fight_zip_path": str(fight_path.relative_to(ROOT)) if fight_path is not None else None,
        **read_local_fightframe(fight_path),
    }

    source_candidates = collect_jsg_candidates(role_name, fallback_map, jsg_rows)
    source_match = select_source_candidate(current["fightframe_hash"], source_candidates)

    if current["fightframe_file_exists"] and source_match is not None and current["fightframe_hash"] == source_match["fightframe_hash"]:
        status = "match"
    elif not current["fightframe_file_exists"] and source_match is not None:
        status = "missing"
    elif current["fightframe_file_exists"] and source_match is not None:
        status = "mismatch"
    else:
        status = "unresolved"

    auto_fixable = (
        status == "missing"
        and source_match is not None
        and bool(source_match["fightframe_text"])
        and fight_path is not None
        and fight_path.exists()
    )

    return {
        "role_id": role_id,
        "role_name": role_name,
        "current": current,
        "source_candidates": source_candidates,
        "source_match": source_match,
        "status": status,
        "needs_update": status in {"missing", "mismatch"},
        "auto_fixable": auto_fixable,
    }


def build_hash_groups(items: list[dict[str, Any]], hash_getter, item_getter) -> list[dict[str, Any]]:
    groups: dict[str, list[dict[str, Any]]] = {}
    for item in items:
        hash_value = hash_getter(item)
        if hash_value is None:
            continue
        groups.setdefault(str(hash_value), []).append(item_getter(item))
    return [
        {"hash": hash_value, "items": values}
        for hash_value, values in sorted(groups.items(), key=lambda entry: (-len(entry[1]), entry[1][0]["role_id"]))
        if len(values) > 1
    ]


def load_source_rows(jsg_rows: dict[str, list[dict[str, Any]]]) -> list[dict[str, Any]]:
    rows = [row for bucket in jsg_rows.values() for row in bucket]
    return dedupe_rows(rows, ("record_id", "head_id"))


def extract_fight_id(zip_path: Path) -> int | None:
    match = re.fullmatch(r"fight(\d+)\.zip", zip_path.name, re.IGNORECASE)
    if match is None:
        return None
    return int(match.group(1))


def iter_fight_zip_paths() -> list[Path]:
    def sort_key(path: Path) -> tuple[int, str]:
        fight_id = extract_fight_id(path)
        return (999999 if fight_id is None else fight_id, path.name.lower())

    return sorted((path for path in WORK_FIGHT_DIR.glob("fight*.zip") if path.is_file()), key=sort_key)


def build_source_index(source_rows: list[dict[str, Any]]) -> dict[int, list[dict[str, Any]]]:
    index: dict[int, list[dict[str, Any]]] = {}
    for row in source_rows:
        index.setdefault(int(row["head_id"]), []).append(row)
    return index


def build_fight_report(zip_path: Path, source_index: dict[int, list[dict[str, Any]]]) -> dict[str, Any]:
    fight_id = extract_fight_id(zip_path)
    if fight_id is None:
        raise ValueError(f"unexpected fight zip name: {zip_path.name}")

    current = {
        "fight_id": fight_id,
        "fight_zip_path": str(zip_path.relative_to(ROOT)),
        **read_local_fightframe(zip_path),
    }

    source_fight_path = find_fight_zip(JSG_ROOT / "data" / "fight", fight_id)
    source_resource_hash = zip_hash_from_bytes(source_fight_path.read_bytes()) if source_fight_path is not None else None

    source_candidates = [
        {
            "source": "金书",
            "match_type": "fight-id",
            "lookup_name": str(fight_id),
            "source_name": row["name"],
            "source_nickname": row.get("nickname"),
            "record_id": int(row["record_id"]),
            "head_id": int(row["head_id"]),
            "resource_hash": source_resource_hash,
            "fightframe_values": list(row.get("frame_num", [])),
            "fightframe_text": row.get("fightframe_text") or fightframe_text_from_values(list(row.get("frame_num", []))),
            "fightframe_hash": row.get("fightframe_hash") or fightframe_hash_from_values(list(row.get("frame_num", []))),
        }
        for row in dedupe_rows(source_index.get(fight_id, []), ("record_id", "head_id"))
    ]
    source_match = select_source_candidate(current["resource_hash"], current["fightframe_hash"], source_candidates)

    if current["fightframe_file_exists"] and source_match is not None and current["fightframe_hash"] == source_match["fightframe_hash"]:
        status = "match"
    elif not current["fightframe_file_exists"] and source_match is not None:
        status = "missing"
    elif current["fightframe_file_exists"] and source_match is not None:
        status = "mismatch"
    else:
        status = "unresolved"

    auto_fixable = (
        status == "missing"
        and source_match is not None
        and bool(source_match["fightframe_text"])
        and zip_path.exists()
    )

    return {
        "role_id": fight_id,
        "role_name": zip_path.name,
        "fight_id": fight_id,
        "current": current,
        "source_candidates": source_candidates,
        "source_match": source_match,
        "status": status,
        "needs_update": status in {"missing", "mismatch"},
        "auto_fixable": auto_fixable,
    }


def fix_missing_fightframes(role_reports: list[dict[str, Any]]) -> int:
    fixed = 0
    for report in role_reports:
        if not report["auto_fixable"]:
            continue
        fight_path = Path(str(report["current"]["fight_zip_path"]))
        zip_path = ROOT / fight_path
        source_match = report["source_match"]
        assert source_match is not None
        write_fightframe_txt(zip_path, source_match["fightframe_text"])
        report["current"]["fightframe_file_exists"] = True
        report["current"]["fightframe_values"] = list(source_match["fightframe_values"])
        report["current"]["fightframe_text"] = source_match["fightframe_text"]
        report["current"]["fightframe_hash"] = source_match["fightframe_hash"]
        report["status"] = "fixed"
        report["needs_update"] = False
        fixed += 1
    return fixed


def write_markdown(report: dict[str, Any]) -> None:
    OUTPUT_MD.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Chess Fightframe Audit",
        "",
        f"- Fight zip 数: {report['summary']['fight_zip_count']}",
        f"- 本地 fightframe.txt 缺失: {report['summary']['missing_count']}",
        f"- 本地 fightframe.txt 匹配: {report['summary']['match_count']}",
        f"- 本地 fightframe.txt 不匹配: {report['summary']['mismatch_count']}",
        f"- 无法自动定位: {report['summary']['unresolved_count']}",
        f"- 已修复缺失文件: {report['summary']['fixed_count']}",
        "",
        "## 需要更新的角色",
        "",
        "| 角色 | 状态 | 本地哈希 | 金书哈希 | 来源 |",
        "| --- | --- | --- | --- | --- |",
    ]

    for item in report["roles"]:
        if item["status"] not in {"missing", "mismatch", "unresolved", "fixed"}:
            continue
        current_hash = item["current"]["fightframe_hash"] or "-"
        source_match = item["source_match"]
        source_hash = source_match["fightframe_hash"] if source_match is not None else "-"
        source_name = "-"
        if source_match is not None:
            source_name = f"{source_match['source_name']}({source_match['record_id']}/{source_match['head_id']})"
        lines.append(f"| {item['role_name']}({item['role_id']}) | {item['status']} | {current_hash} | {source_hash} | {source_name} |")

    lines.extend([
        "",
        "## 本地哈希重复组",
        "",
        "| 哈希 | 角色 |",
        "| --- | --- |",
    ])
    for group in report["local_hash_groups"]:
        names = " / ".join(f"{item['role_name']}({item['role_id']})" for item in group["items"])
        lines.append(f"| {group['hash']} | {names} |")

    lines.extend([
        "",
        "## 金书哈希重复组",
        "",
        "| 哈希 | 角色 |",
        "| --- | --- |",
    ])
    for group in report["source_hash_groups"]:
        names = " / ".join(f"{item['source_name']}({item['record_id']})" for item in group["items"])
        lines.append(f"| {group['hash']} | {names} |")

    OUTPUT_MD.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_report(fix_missing: bool) -> dict[str, Any]:
    jsg_rows = load_jsg_rows()
    source_rows = load_source_rows(jsg_rows)
    source_index = build_source_index(source_rows)
    roles = [build_fight_report(zip_path, source_index) for zip_path in iter_fight_zip_paths()]
    fixed_count = fix_missing_fightframes(roles) if fix_missing else 0

    local_hash_groups = build_hash_groups(
        roles,
        lambda item: item["current"]["fightframe_hash"],
        lambda item: {
            "role_id": item["role_id"],
            "role_name": item["role_name"],
            "fight_id": item["current"]["fight_id"],
        },
    )

    source_hash_groups = build_hash_groups(
        source_rows,
        lambda item: item["fightframe_hash"],
        lambda item: {
            "role_id": int(item["record_id"]),
            "role_name": str(item["name"]),
            "source_name": str(item["name"]),
            "record_id": int(item["record_id"]),
            "head_id": int(item["head_id"]),
        },
    )

    summary = {
        "fight_zip_count": len(roles),
        "missing_count": sum(1 for item in roles if item["status"] == "missing"),
        "match_count": sum(1 for item in roles if item["status"] == "match"),
        "mismatch_count": sum(1 for item in roles if item["status"] == "mismatch"),
        "unresolved_count": sum(1 for item in roles if item["status"] == "unresolved"),
        "fixed_count": fixed_count,
        "local_hash_group_count": len(local_hash_groups),
        "source_hash_group_count": len(source_hash_groups),
    }

    report = {
        "summary": summary,
        "roles": roles,
        "local_hash_groups": local_hash_groups,
        "source_hash_groups": source_hash_groups,
    }
    report["by_role_id"] = {str(item["role_id"]): item for item in roles}
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit and repair fightframe.txt files using 金书 role blobs")
    parser.add_argument("--fix-missing", action="store_true", help="write fightframe.txt into missing local fight zips when the JSG source is unambiguous")
    args = parser.parse_args()

    report = build_report(args.fix_missing)

    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    write_markdown(report)

    print(OUTPUT_JSON)
    print(OUTPUT_MD)
    print(json.dumps(report["summary"], ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())