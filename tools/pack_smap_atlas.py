#!/usr/bin/env python3
"""Pack an smap directory into a single atlas blob plus a manifest.

The atlas format is intentionally simple:
- <base>.atlas: concatenated raw file bytes
- <base>.atlas.json: JSON manifest with one record per file

Each record stores: path, offset, size

The runtime keeps backward compatibility by probing `smap.zip`, then the atlas,
then the flat directory.
"""
from __future__ import annotations

import argparse
import json
import os
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path


MANIFEST_HEADER = "KYS_ATLAS_V1"
DEFAULT_SOURCE = Path("work/game-dev/resource/smap")
ATLAS_EXTENSION = ".atlas"
MANIFEST_EXTENSION = ".atlas.json"


@dataclass(slots=True)
class AtlasRecord:
    path: str
    offset: int
    size: int


@dataclass(slots=True)
class AtlasManifest:
    version: str
    records: list[AtlasRecord]


def normalize_relative_path(path: Path) -> str:
    return path.as_posix()


def iter_source_files(source_dir: Path) -> list[Path]:
    files = [path for path in source_dir.rglob("*") if path.is_file()]
    return sorted(files, key=lambda path: normalize_relative_path(path.relative_to(source_dir)).lower())


def atomic_replace(path: Path, data: bytes) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".tmp", dir=path.parent)
    os.close(fd)
    temp_path = Path(temp_name)
    try:
        temp_path.write_bytes(data)
        os.replace(temp_path, path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def build_atlas(source_dir: Path, output_base: Path) -> tuple[int, int]:
    files = iter_source_files(source_dir)
    if not files:
        raise FileNotFoundError(f"No files found under {source_dir}")

    blob_parts: list[bytes] = []
    records: list[AtlasRecord] = []
    offset = 0
    for file_path in files:
        relative_path = normalize_relative_path(file_path.relative_to(source_dir))
        data = file_path.read_bytes()
        blob_parts.append(data)
        records.append(AtlasRecord(path=relative_path, offset=offset, size=len(data)))
        offset += len(data)

    blob_path = output_base.with_suffix(ATLAS_EXTENSION)
    manifest_path = output_base.with_suffix(MANIFEST_EXTENSION)
    manifest = AtlasManifest(version=MANIFEST_HEADER, records=records)
    atomic_replace(blob_path, b"".join(blob_parts))
    atomic_replace(manifest_path, (json.dumps(asdict(manifest), ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
    return len(files), offset


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Pack work/game-dev/resource/smap into an atlas blob")
    parser.add_argument(
        "source",
        nargs="?",
        type=Path,
        default=DEFAULT_SOURCE,
        help="smap directory to pack (default: work/game-dev/resource/smap)",
    )
    parser.add_argument(
        "--output-base",
        type=Path,
        help="output base path without extension (default: sibling path matching the source name)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    source_dir = args.source.resolve()
    if not source_dir.is_dir():
        raise SystemExit(f"Source directory not found: {source_dir}")

    output_base = args.output_base.resolve() if args.output_base else source_dir.parent / source_dir.name
    output_base.parent.mkdir(parents=True, exist_ok=True)

    file_count, total_bytes = build_atlas(source_dir, output_base)
    print(f"packed {file_count} file(s) into {output_base.with_suffix(ATLAS_EXTENSION)}")
    print(f"wrote manifest {output_base.with_suffix(MANIFEST_EXTENSION)}")
    print(f"total payload bytes: {total_bytes}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())