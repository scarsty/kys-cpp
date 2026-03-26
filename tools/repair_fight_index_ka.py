#!/usr/bin/env python3
"""Audit and repair fight archive index.ka files.

Each fight zip in work/game-dev/resource/fight contains numeric PNG frames and a
binary index.ka made of little-endian int16 dx/dy pairs. This script validates
the archive structure, discovers the dominant offset pair from the valid
archives, and can rewrite malformed archives with a configurable guessed pair.
"""
from __future__ import annotations

import argparse
import collections
import os
import re
import struct
import tempfile
import zipfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parents[1]
FIGHT_DIR = ROOT / "work" / "game-dev" / "resource" / "fight"
DEFAULT_GUESS_OFFSET = (51, 59)
NUMERIC_PNG_RE = re.compile(r"^(?P<index>\d+)\.png$", re.IGNORECASE)


@dataclass(frozen=True)
class ArchiveState:
    path: Path
    png_entries: list[tuple[int, str]]
    index_pairs: list[tuple[int, int]] | None
    status: str
    reason: str


def is_fight_zip(path: Path) -> bool:
    return path.is_file() and path.suffix.lower() == ".zip" and path.name.lower().startswith("fight") and path.stem[5:].isdigit()


def iter_fight_zips(fight_dir: Path) -> list[Path]:
    return [path for path in sorted(fight_dir.iterdir()) if is_fight_zip(path)]


def find_entry_by_basename(zip_file: zipfile.ZipFile, basename: str) -> zipfile.ZipInfo | None:
    basename = basename.lower()
    for info in zip_file.infolist():
        if info.is_dir():
            continue
        normalized_name = info.filename.replace("\\", "/")
        if normalized_name.lower() == basename:
            return info
    return None


def read_numeric_png_entries(zip_file: zipfile.ZipFile) -> list[tuple[int, str]]:
    entries: list[tuple[int, str]] = []
    for info in zip_file.infolist():
        if info.is_dir():
            continue
        normalized_name = info.filename.replace("\\", "/")
        match = NUMERIC_PNG_RE.fullmatch(normalized_name)
        if match is None:
            continue
        entries.append((int(match.group("index")), info.filename))
    entries.sort(key=lambda item: item[0])
    return entries


def read_index_pairs(zip_file: zipfile.ZipFile) -> list[tuple[int, int]] | None:
    index_info = find_entry_by_basename(zip_file, "index.ka")
    if index_info is None:
        return None

    data = zip_file.read(index_info)
    if len(data) % 4 != 0:
        raise ValueError(f"index.ka size {len(data)} is not divisible by 4")

    return [struct.unpack_from("<hh", data, offset) for offset in range(0, len(data), 4)]


def validate_archive(zip_path: Path) -> ArchiveState:
    with zipfile.ZipFile(zip_path, "r") as zip_file:
        png_entries = read_numeric_png_entries(zip_file)
        if not png_entries:
            return ArchiveState(zip_path, png_entries, None, "skip", "no numeric PNG frames")

        index_pairs = read_index_pairs(zip_file)
        if index_pairs is None:
            return ArchiveState(zip_path, png_entries, None, "missing", "missing root index.ka")

        expected_indices = list(range(len(png_entries)))
        actual_indices = [index for index, _ in png_entries]
        if actual_indices != expected_indices:
            return ArchiveState(
                zip_path,
                png_entries,
                index_pairs,
                "malformed",
                f"PNG indices are {actual_indices[:8]}... instead of 0..{len(png_entries) - 1}",
            )

        if len(index_pairs) != len(png_entries):
            return ArchiveState(
                zip_path,
                png_entries,
                index_pairs,
                "malformed",
                f"index pair count {len(index_pairs)} does not match PNG count {len(png_entries)}",
            )

        return ArchiveState(zip_path, png_entries, index_pairs, "ok", "structure matches PNG frames")


def discover_guess_offset(states: Iterable[ArchiveState]) -> tuple[tuple[int, int], int]:
    counter: collections.Counter[tuple[int, int]] = collections.Counter()
    for state in states:
        if state.status != "ok" or state.index_pairs is None:
            continue
        counter.update(state.index_pairs)

    if not counter:
        return DEFAULT_GUESS_OFFSET, 0

    return counter.most_common(1)[0]


def build_index_bytes(pair_count: int, guess_offset: tuple[int, int]) -> bytes:
    dx, dy = guess_offset
    return struct.pack("<hh", dx, dy) * pair_count


def clone_zip_info(info: zipfile.ZipInfo) -> zipfile.ZipInfo:
    clone = zipfile.ZipInfo(info.filename, info.date_time)
    clone.comment = info.comment
    clone.extra = info.extra
    clone.create_system = info.create_system
    clone.create_version = info.create_version
    clone.extract_version = info.extract_version
    clone.reserved = info.reserved
    clone.flag_bits = info.flag_bits
    clone.volume = info.volume
    clone.internal_attr = info.internal_attr
    clone.external_attr = info.external_attr
    clone.compress_type = info.compress_type
    return clone


def rewrite_archive(zip_path: Path, guess_offset: tuple[int, int], png_count: int) -> None:
    replacement = build_index_bytes(png_count, guess_offset)
    fd, temp_name = tempfile.mkstemp(prefix=f"{zip_path.stem}.", suffix=".repair.zip", dir=zip_path.parent)
    os.close(fd)
    temp_path = Path(temp_name)

    try:
        with zipfile.ZipFile(zip_path, "r") as source_zip, zipfile.ZipFile(temp_path, "w") as target_zip:
            target_zip.comment = source_zip.comment
            index_written = False

            for info in source_zip.infolist():
                if info.is_dir():
                    target_zip.writestr(clone_zip_info(info), b"")
                    continue

                normalized_name = Path(info.filename.replace("\\", "/")).name.lower()
                if normalized_name == "index.ka":
                    target_zip.writestr(clone_zip_info(info), replacement)
                    index_written = True
                else:
                    target_zip.writestr(clone_zip_info(info), source_zip.read(info.filename))

            if not index_written:
                target_zip.writestr("index.ka", replacement)

        os.replace(temp_path, zip_path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def resolve_targets(fight_dir: Path, requested: list[str]) -> set[Path]:
    if not requested:
        return set()

    resolved: set[Path] = set()
    for item in requested:
        candidate = Path(item)
        if candidate.is_file():
            resolved.add(candidate)
            continue

        if not candidate.is_absolute():
            relative_candidate = fight_dir / candidate
            if relative_candidate.is_file():
                resolved.add(relative_candidate)
                continue
            if candidate.suffix.lower() != ".zip":
                relative_candidate_with_suffix = fight_dir / candidate.with_suffix(".zip")
                if relative_candidate_with_suffix.is_file():
                    resolved.add(relative_candidate_with_suffix)
                    continue

        if candidate.suffix.lower() != ".zip":
            candidate = candidate.with_suffix(".zip")
        if candidate.is_file():
            resolved.add(candidate)
            continue

        raise FileNotFoundError(f"Target fight archive not found: {item}")
    return resolved


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Audit and repair fight archive index.ka files")
    parser.add_argument("--fight-dir", type=Path, default=FIGHT_DIR, help="directory containing fight*.zip archives")
    parser.add_argument("targets", nargs="*", help="optional fight zip names or paths to force rewrite")
    parser.add_argument("--apply", action="store_true", help="rewrite malformed archives instead of only reporting them")
    parser.add_argument("--guess-offset", nargs=2, type=int, metavar=("DX", "DY"), help="override the guessed offset pair used for repairs")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    fight_dir = args.fight_dir.resolve()

    if not fight_dir.exists():
        raise FileNotFoundError(f"Fight directory does not exist: {fight_dir}")

    states = [validate_archive(path) for path in iter_fight_zips(fight_dir)]
    discovered_guess, discovered_count = discover_guess_offset(states)
    guess_offset = tuple(args.guess_offset) if args.guess_offset else discovered_guess
    forced_targets = resolve_targets(fight_dir, list(args.targets))

    repairable = [state for state in states if state.status in {"missing", "malformed"} or state.path in forced_targets]

    print(f"fight_dir: {fight_dir}")
    print(f"guess_offset: {guess_offset[0]}, {guess_offset[1]} ({'cli' if args.guess_offset else 'discovered' if discovered_count else 'default'})")
    print(f"validated_archives: {len(states)}")
    print(f"repairable_archives: {len(repairable)}")
    print(f"forced_targets: {len(forced_targets)}")

    for state in states:
        if state.status == "skip":
            continue
        needs_repair = state.status in {"missing", "malformed"} or state.path in forced_targets
        if not needs_repair:
            continue
        prefix = "[repair]" if needs_repair else "[ok]"
        print(f"{prefix} {state.path.name}: {state.status} - {state.reason}")

    if not args.apply:
        print("Run again with --apply to rewrite the repairable archives.")
        return 0

    repaired = 0
    for state in repairable:
        rewrite_archive(state.path, guess_offset, len(state.png_entries))
        repaired += 1
        print(f"rewrote {state.path.name} with {len(state.png_entries)} guessed offsets")

    print(f"repaired_archives: {repaired}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())