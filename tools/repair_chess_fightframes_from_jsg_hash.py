from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import zipfile
from collections import defaultdict
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.derive_chess_identity_mappings import JSG_ROOT, fightframe_text_from_values, load_jsg_rows  # noqa: E402


LOCAL_FIGHT_DIR = ROOT / "work" / "game-dev" / "resource" / "fight"
JSG_FIGHT_DIR = JSG_ROOT / "data" / "fight"
OUTPUT_JSON = ROOT / "output" / "jsg_fightframe_archive_audit.json"
OUTPUT_MD = ROOT / "output" / "jsg_fightframe_archive_audit.md"


def is_numbered_fight_zip(path: Path) -> bool:
    return path.name.lower().startswith("fight") and path.name.lower().endswith(".zip") and path.stem[5:].isdigit()


def archive_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def zip_contains_fightframe(zip_path: Path) -> bool:
    with zipfile.ZipFile(zip_path) as zf:
        return any(info.filename.replace("\\", "/").lower() == "fightframe.txt" for info in zf.infolist() if not info.is_dir())


def read_fightframe_text(zip_path: Path) -> str | None:
    with zipfile.ZipFile(zip_path) as zf:
        for info in zf.infolist():
            if not info.is_dir() and info.filename.replace("\\", "/").lower() == "fightframe.txt":
                return zf.read(info).decode("utf-8", "ignore").replace("\r\n", "\n").strip()
    return None


def write_fightframe_text(zip_path: Path, text: str) -> None:
    with zipfile.ZipFile(zip_path, mode="a", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("fightframe.txt", text)


def load_jsg_archive_index() -> dict[str, list[int]]:
    index: dict[str, list[int]] = defaultdict(list)
    for path in sorted(JSG_FIGHT_DIR.iterdir()):
        if not path.is_file() or not is_numbered_fight_zip(path):
            continue
        fight_id = int(path.stem[5:])
        index[archive_hash(path)].append(fight_id)
    return dict(index)


def load_jsg_head_index() -> tuple[dict[int, dict[str, Any]], dict[int, dict[str, Any]]]:
    by_head: dict[int, list[dict[str, Any]]] = defaultdict(list)
    seen: set[tuple[int, int]] = set()

    for bucket in load_jsg_rows().values():
        for row in bucket:
            key = (int(row["record_id"]), int(row["head_id"]))
            if key in seen:
                continue
            seen.add(key)
            by_head[int(row["head_id"])].append(row)

    resolved: dict[int, dict[str, Any]] = {}
    ambiguous: dict[int, dict[str, Any]] = {}
    for head_id, rows in by_head.items():
        texts = sorted({str(row.get("fightframe_text", "")) for row in rows})
        if len(texts) == 1:
            text = texts[0]
            representative = next((row for row in rows if str(row.get("fightframe_text", "")) == text), rows[0])
            resolved[head_id] = {
                "head_id": head_id,
                "fightframe_text": text,
                "record_id": int(representative["record_id"]),
                "name": str(representative["name"]),
                "nickname": str(representative.get("nickname", "")),
                "row_count": len(rows),
            }
        else:
            ambiguous[head_id] = {
                "head_id": head_id,
                "texts": texts,
                "rows": [
                    {
                        "record_id": int(row["record_id"]),
                        "name": str(row["name"]),
                        "nickname": str(row.get("nickname", "")),
                        "fightframe_text": str(row.get("fightframe_text", "")),
                    }
                    for row in rows
                ],
            }
    return resolved, ambiguous


def select_source_fight_id(local_fight_id: int, candidate_fight_ids: list[int], head_index: dict[int, dict[str, Any]]) -> int | None:
    if not candidate_fight_ids:
        return None
    if len(candidate_fight_ids) == 1:
        return candidate_fight_ids[0]

    if local_fight_id in candidate_fight_ids and local_fight_id in head_index:
        return local_fight_id

    available = [fight_id for fight_id in candidate_fight_ids if fight_id in head_index]
    if len(available) == 1:
        return available[0]

    texts = {head_index[fight_id]["fightframe_text"] for fight_id in available if fight_id in head_index}
    if len(texts) == 1 and available:
        return available[0]

    return None


def build_report(fix_missing: bool) -> dict[str, Any]:
    local_paths = [path for path in sorted(LOCAL_FIGHT_DIR.iterdir()) if path.is_file() and is_numbered_fight_zip(path)]
    jsg_archive_index = load_jsg_archive_index()
    jsg_head_index, ambiguous_heads = load_jsg_head_index()

    entries: list[dict[str, Any]] = []
    repaired = 0

    for local_path in local_paths:
        local_hash = archive_hash(local_path)
        local_fight_id = int(local_path.stem[5:])
        candidate_fight_ids = jsg_archive_index.get(local_hash, [])
        selected_fight_id = select_source_fight_id(local_fight_id, candidate_fight_ids, jsg_head_index)
        selected_source = jsg_head_index.get(selected_fight_id) if selected_fight_id is not None else None
        has_fightframe = zip_contains_fightframe(local_path)

        status = "unmatched"
        reason = "no matching JSG archive hash"
        if candidate_fight_ids:
            if selected_source is None:
                status = "ambiguous"
                reason = f"matching JSG archive hash points to fight ids {candidate_fight_ids} but the fight-frame text cannot be resolved unambiguously"
            elif has_fightframe:
                current_text = read_fightframe_text(local_path)
                if current_text == selected_source["fightframe_text"]:
                    status = "already-correct"
                    reason = f"matches JSG fight {selected_fight_id}"
                else:
                    status = "different"
                    reason = f"matches JSG fight {selected_fight_id}, but local fightframe.txt differs"
            else:
                status = "repairable"
                reason = f"matches JSG fight {selected_fight_id}"
                if fix_missing:
                    write_fightframe_text(local_path, selected_source["fightframe_text"])
                    repaired += 1
                    has_fightframe = True
                    status = "repaired"

        entries.append(
            {
                "local_fight_id": local_fight_id,
                "local_name": local_path.name,
                "local_hash": local_hash,
                "has_fightframe": has_fightframe,
                "candidate_fight_ids": candidate_fight_ids,
                "selected_fight_id": selected_fight_id,
                "selected_source": selected_source,
                "status": status,
                "reason": reason,
            }
        )

    summary = {
        "local_fight_count": len(local_paths),
        "exact_archive_matches": sum(1 for item in entries if item["candidate_fight_ids"]),
        "repairable_missing": sum(1 for item in entries if item["status"] == "repairable"),
        "repaired": repaired,
        "already_correct": sum(1 for item in entries if item["status"] == "already-correct"),
        "different": sum(1 for item in entries if item["status"] == "different"),
        "ambiguous": sum(1 for item in entries if item["status"] == "ambiguous"),
        "unmatched": sum(1 for item in entries if item["status"] == "unmatched"),
        "ambiguous_head_ids": len(ambiguous_heads),
        "unique_head_ids": len(jsg_head_index),
        "numbered_jsg_archives": sum(len(v) for v in jsg_archive_index.values()),
    }

    return {
        "summary": summary,
        "entries": entries,
        "ambiguous_heads": ambiguous_heads,
    }


def write_report(report: dict[str, Any]) -> None:
    OUTPUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_JSON.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# JSG Fightframe Archive Audit",
        "",
        f"- Local fight zips: {report['summary']['local_fight_count']}",
        f"- Numbered JSG fight archives: {report['summary']['numbered_jsg_archives']}",
        f"- Unique JSG fight ids with a resolved fightframe: {report['summary']['unique_head_ids']}",
        f"- Ambiguous JSG head ids: {report['summary']['ambiguous_head_ids']}",
        f"- Exact archive matches: {report['summary']['exact_archive_matches']}",
        f"- Repairable missing fightframe.txt: {report['summary']['repairable_missing']}",
        f"- Repaired: {report['summary']['repaired']}",
        f"- Already correct: {report['summary']['already_correct']}",
        f"- Different text: {report['summary']['different']}",
        f"- Ambiguous archive hash matches: {report['summary']['ambiguous']}",
        f"- Unmatched: {report['summary']['unmatched']}",
        "",
        "## Repairable Files",
        "",
        "| Local fight | JSG fight id | Source character | Status |",
        "| --- | --- | --- | --- |",
    ]

    for item in report["entries"]:
        if item["status"] not in {"repairable", "repaired", "already-correct", "different"}:
            continue
        source = item["selected_source"] or {}
        source_name = source.get("name", "-")
        lines.append(
            f"| {item['local_name']} | {item['selected_fight_id'] if item['selected_fight_id'] is not None else '-'} | {source_name} | {item['status']} |"
        )

    OUTPUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Match local fight zip archives to JSG archives by whole-zip hash and generate fightframe.txt from JSG role blobs")
    parser.add_argument("--fix-missing", action="store_true", help="write fightframe.txt into local zips whose whole archive hash matches JSG and whose frame text is unambiguous")
    args = parser.parse_args()

    report = build_report(args.fix_missing)
    write_report(report)

    print(OUTPUT_JSON)
    print(OUTPUT_MD)
    print(json.dumps(report["summary"], ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())