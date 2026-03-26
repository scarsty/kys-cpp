#!/usr/bin/env python3
"""Compare, repair, and repack 2x-upscaled EFT directories.

The script operates on two unzipped EFT roots:

1. Compare the original tree against the 2x-upscaled tree and print missing PNGs.
2. Copy each group's index.ka from the original tree into the scaled tree after
   doubling every dx/dy pair.
3. Zip each group directory from the scaled tree into a target output directory.

Use ``--steps 1`` to only report missing PNGs, or keep the default ``1 2 3`` to
run the full workflow.
"""
from __future__ import annotations

import argparse
import os
import sys
import tempfile
import zipfile
from pathlib import Path

try:
    from scale_index_ka import scale_index_bytes
except ImportError:
    ROOT = Path(__file__).resolve().parents[1]
    if str(ROOT) not in sys.path:
        sys.path.insert(0, str(ROOT))
    from tools.scale_index_ka import scale_index_bytes


INDEX_NAME = "index.ka"
PNG_SUFFIX = ".png"


def looks_like_group_dir(directory: Path) -> bool:
    if not directory.is_dir():
        return False
    if (directory / INDEX_NAME).is_file():
        return True
    for path in directory.rglob("*"):
        if path.is_file() and path.suffix.lower() == PNG_SUFFIX:
            return True
    return False


def iter_group_dirs(root: Path) -> list[Path]:
    return [child for child in sorted(root.iterdir()) if looks_like_group_dir(child)]


def iter_png_relative_paths(directory: Path) -> list[str]:
    paths: list[str] = []
    for path in sorted(directory.rglob("*")):
        if path.is_file() and path.suffix.lower() == PNG_SUFFIX:
            paths.append(path.relative_to(directory).as_posix())
    return paths


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temp_name = tempfile.mkstemp(prefix=f"{path.stem}.", suffix=".tmp", dir=path.parent)
    os.close(fd)
    temp_path = Path(temp_name)
    try:
        temp_path.write_bytes(data)
        os.replace(temp_path, path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def compare_pngs(original_root: Path, scaled_root: Path) -> list[str]:
    missing: list[str] = []
    for original_group in iter_group_dirs(original_root):
        scaled_group = scaled_root / original_group.name
        scaled_pngs = {relative_path.lower() for relative_path in iter_png_relative_paths(scaled_group)} if scaled_group.is_dir() else set()

        for relative_path in iter_png_relative_paths(original_group):
            if relative_path.lower() not in scaled_pngs:
                missing.append(f"{original_group.name}/{relative_path}")
    return missing


def sync_index_files(original_root: Path, scaled_root: Path, factor: int) -> tuple[int, int]:
    updated = 0
    failed = 0

    for original_group in iter_group_dirs(original_root):
        source_index = original_group / INDEX_NAME
        target_group = scaled_root / original_group.name
        target_index = target_group / INDEX_NAME

        if not source_index.is_file():
            print(f"failed {source_index}: missing source index.ka", file=sys.stderr)
            failed += 1
            continue

        if not target_group.is_dir():
            print(f"failed {target_group}: missing target group directory", file=sys.stderr)
            failed += 1
            continue

        try:
            scaled = scale_index_bytes(source_index.read_bytes(), factor, source_index)
            atomic_write(target_index, scaled)
            print(f"synced {target_index}")
            updated += 1
        except Exception as exc:
            print(f"failed {target_index}: {exc}", file=sys.stderr)
            failed += 1

    return updated, failed


def zip_group_directory(source_dir: Path, output_zip: Path) -> None:
    fd, temp_name = tempfile.mkstemp(prefix=f"{output_zip.stem}.", suffix=".tmp.zip", dir=output_zip.parent)
    os.close(fd)
    temp_path = Path(temp_name)

    try:
        with zipfile.ZipFile(temp_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for entry in sorted(source_dir.rglob("*")):
                archive.write(entry, entry.relative_to(source_dir).as_posix())

        os.replace(temp_path, output_zip)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise


def zip_scaled_groups(original_root: Path, scaled_root: Path, output_dir: Path) -> tuple[int, int]:
    output_dir.mkdir(parents=True, exist_ok=True)

    updated = 0
    failed = 0
    for original_group in iter_group_dirs(original_root):
        source_group = scaled_root / original_group.name
        if not source_group.is_dir():
            print(f"failed {source_group}: missing target group directory", file=sys.stderr)
            failed += 1
            continue

        output_zip = output_dir / f"{original_group.name}.zip"
        try:
            zip_group_directory(source_group, output_zip)
            print(f"zipped {output_zip}")
            updated += 1
        except Exception as exc:
            print(f"failed {output_zip}: {exc}", file=sys.stderr)
            failed += 1

    return updated, failed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare, repair, and repack 2x-upscaled EFT directories")
    parser.add_argument("original_root", type=Path, help="original unzipped EFT root")
    parser.add_argument("scaled_root", type=Path, help="2x-upscaled unzipped EFT root")
    parser.add_argument("--zip-output", type=Path, help="directory where rebuilt .zip files are written")
    parser.add_argument(
        "--steps",
        nargs="+",
        type=int,
        choices=(1, 2, 3),
        default=[1, 2, 3],
        help="workflow steps to run: 1 compare PNGs, 2 sync index.ka, 3 zip groups",
    )
    parser.add_argument("--factor", type=int, default=2, help="scale factor used when copying index.ka")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    original_root = args.original_root.resolve()
    scaled_root = args.scaled_root.resolve()
    selected_steps = set(args.steps)

    if not original_root.is_dir():
        print(f"failed {original_root}: original root does not exist or is not a directory", file=sys.stderr)
        return 1

    if not scaled_root.is_dir():
        print(f"failed {scaled_root}: scaled root does not exist or is not a directory", file=sys.stderr)
        return 1

    if 3 in selected_steps and args.zip_output is None:
        print("failed: --zip-output is required when step 3 is selected", file=sys.stderr)
        return 1

    exit_code = 0

    if 1 in selected_steps:
        missing_pngs = compare_pngs(original_root, scaled_root)
        if missing_pngs:
            print("missing pngs:")
            for missing_png in missing_pngs:
                print(missing_png)
        else:
            print("missing pngs: none")
        print(f"missing png count: {len(missing_pngs)}")

    if 2 in selected_steps:
        _, failed = sync_index_files(original_root, scaled_root, args.factor)
        exit_code = max(exit_code, 1 if failed else 0)

    if 3 in selected_steps:
        assert args.zip_output is not None
        _, failed = zip_scaled_groups(original_root, scaled_root, args.zip_output.resolve())
        exit_code = max(exit_code, 1 if failed else 0)

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())