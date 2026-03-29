#!/usr/bin/env python3
"""Convert PNG assets to WebP or AVIF — works on directories and inside zip archives.

Usage:
    python convert_png_to_webp.py <path> [--format webp|avif] [--quality Q] [--lossy]
    python convert_png_to_webp.py work/game-dev/resource
    python convert_png_to_webp.py work/game-dev/resource --format avif --lossy --quality 90

By default uses lossless compression.  Pass --lossy to use lossy mode
(default quality 90, adjustable with --quality). WebP remains the default
output format for backward compatibility.

Handles:
  - Directories of loose .png files (battle-earth, head, smap, …)
  - Zip archives containing .png files (fight*.zip, eft*.zip, mmap.zip, …)
  - Recursively walks subdirectories
"""
from __future__ import annotations

import argparse
import io
import os
import shutil
import sys
import tempfile
import zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from PIL import Image


def convert_png_bytes(data: bytes, *, output_format: str, lossless: bool, quality: int) -> bytes:
    """Convert raw PNG bytes to encoded image bytes."""
    with Image.open(io.BytesIO(data)) as img:
        buf = io.BytesIO()
        img.save(buf, format=output_format.upper(), lossless=lossless, quality=quality)
        return buf.getvalue()


def convert_directory(dirpath: Path, *, output_format: str, lossless: bool, quality: int) -> dict:
    """Convert all .png files in *dirpath* to the target format, removing the originals."""
    stats = {"converted": 0, "skipped": 0, "saved_bytes": 0}
    output_suffix = f".{output_format}"
    pngs = sorted(dirpath.glob("*.png"))
    if not pngs:
        return stats
    for png in pngs:
        encoded = png.with_suffix(output_suffix)
        if encoded.exists():
            stats["skipped"] += 1
            continue
        try:
            data = png.read_bytes()
            out = convert_png_bytes(data, output_format=output_format, lossless=lossless, quality=quality)
            encoded.write_bytes(out)
            saved = len(data) - len(out)
            stats["saved_bytes"] += saved
            stats["converted"] += 1
            png.unlink()
        except Exception as e:
            print(f"  WARN: {png}: {e}", file=sys.stderr)
            if encoded.exists():
                encoded.unlink()
    return stats


def convert_zip(zippath: Path, *, output_format: str, lossless: bool, quality: int) -> dict:
    """Re-pack a zip, converting every .png inside to the target format."""
    stats = {"converted": 0, "skipped": 0, "saved_bytes": 0}
    output_suffix = f".{output_format}"

    if not zipfile.is_zipfile(zippath):
        print(f"  WARN: {zippath} is not a valid zip", file=sys.stderr)
        return stats

    tmpfd, tmppath = tempfile.mkstemp(suffix=".zip", dir=zippath.parent)
    os.close(tmpfd)

    try:
        with zipfile.ZipFile(zippath, "r") as zin, \
             zipfile.ZipFile(tmppath, "w", compression=zipfile.ZIP_STORED) as zout:
            for info in zin.infolist():
                data = zin.read(info.filename)
                name = info.filename
                if name.lower().endswith(".png"):
                    encoded_name = name[:-4] + output_suffix
                    try:
                        out = convert_png_bytes(data, output_format=output_format, lossless=lossless, quality=quality)
                        stats["saved_bytes"] += len(data) - len(out)
                        stats["converted"] += 1
                        zout.writestr(encoded_name, out)
                        continue
                    except Exception as e:
                        print(f"  WARN: {zippath}!{name}: {e}", file=sys.stderr)
                # Keep non-png (or failed) entries as-is
                zout.writestr(info, data)

        # Atomic replace
        shutil.move(tmppath, zippath)
    except Exception:
        if os.path.exists(tmppath):
            os.unlink(tmppath)
        raise

    return stats


def process_target(target: Path, *, output_format: str, lossless: bool, quality: int) -> None:
    """Process a single directory or zip file."""
    if target.is_file() and target.suffix.lower() == ".zip":
        label = f"ZIP  {target}"
        st = convert_zip(target, output_format=output_format, lossless=lossless, quality=quality)
    elif target.is_dir():
        label = f"DIR  {target}"
        st = convert_directory(target, output_format=output_format, lossless=lossless, quality=quality)
    else:
        return

    if st["converted"] or st["skipped"]:
        saved_mb = st["saved_bytes"] / (1024 * 1024)
        print(f"  {label}: {st['converted']} converted, {st['skipped']} skipped, {saved_mb:+.2f} MB")


def gather_targets(root: Path) -> list[Path]:
    """Walk root and collect every directory containing .png files and every .zip."""
    targets: list[Path] = []
    for dirpath, dirnames, filenames in os.walk(root):
        dp = Path(dirpath)
        has_png = any(f.lower().endswith(".png") for f in filenames)
        if has_png:
            targets.append(dp)
        for f in filenames:
            if f.lower().endswith(".zip"):
                targets.append(dp / f)
    return sorted(targets)


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert PNG assets to WebP or AVIF")
    parser.add_argument("path", type=Path, help="Directory or zip file to convert")
    parser.add_argument("--format", choices=("webp", "avif"), default="webp", help="Output format (default: webp)")
    parser.add_argument("--lossy", action="store_true", help="Use lossy compression (default: lossless)")
    parser.add_argument("--quality", type=int, default=90, help="Lossy quality 0-100 (default 90, ignored for lossless)")
    parser.add_argument("-j", "--jobs", type=int, default=1, help="Parallel jobs for zip files (default 1)")
    args = parser.parse_args()

    lossless = not args.lossy
    quality = args.quality
    output_format = args.format
    mode = "lossless" if lossless else f"lossy q={quality}"
    print(f"Converting PNG -> {output_format.upper()} ({mode})")

    root = args.path.resolve()
    if root.is_file():
        targets = [root]
    elif root.is_dir():
        targets = gather_targets(root)
    else:
        print(f"ERROR: {root} not found", file=sys.stderr)
        sys.exit(1)

    dirs = [t for t in targets if t.is_dir()]
    zips = [t for t in targets if t.is_file()]

    print(f"Found {len(dirs)} directories, {len(zips)} zip files")

    # Directories first (fast, in-place)
    for d in dirs:
        process_target(d, output_format=output_format, lossless=lossless, quality=quality)

    # Zips (parallel if requested)
    if args.jobs > 1 and len(zips) > 1:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futs = {
                pool.submit(process_target, z, output_format=output_format, lossless=lossless, quality=quality): z
                for z in zips
            }
            for fut in as_completed(futs):
                try:
                    fut.result()
                except Exception as e:
                    print(f"  ERROR: {futs[fut]}: {e}", file=sys.stderr)
    else:
        for z in zips:
            process_target(z, output_format=output_format, lossless=lossless, quality=quality)

    print("Done.")


if __name__ == "__main__":
    main()
