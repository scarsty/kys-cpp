#!/usr/bin/env python3
"""Scan a head resource directory and append zero offsets to index.ka for any
numeric PNG filenames that lack index entries.
Usage: python fix_index_ka.py <head_dir> [--apply] [--old <old_head_dir>]
If --apply is given, the script will modify index.ka; otherwise it's a dry run.
"""
import os, glob, re, sys

def scan_and_fix(head_dir, apply=False):
    pngs = glob.glob(os.path.join(head_dir, '*.png')) + glob.glob(os.path.join(head_dir, '*.PNG'))
    nums = sorted({int(re.match(r'(\\d+)\\.png$', os.path.basename(p), re.I).group(1))
                   for p in pngs if re.match(r'(\\d+)\\.png$', os.path.basename(p), re.I)})
    idx_file = os.path.join(head_dir, 'index.ka')
    data = b''
    if os.path.exists(idx_file):
        with open(idx_file, 'rb') as f:
            data = f.read()
    pairs = len(data) // 4
    print(f"Scanned: {head_dir}")
    print(f"  PNG count (numeric names): {len(nums)}; max index: {nums[-1] if nums else 'N/A'}")
    print(f"  index.ka pairs: {pairs}")
    if not nums:
        print('  No numeric pngs found; nothing to do')
        return pairs
    max_i = nums[-1]
    missing = [n for n in nums if n >= pairs]
    print(f"  Missing indices (>=pairs): {len(missing)} -> {missing[:10]}{'...' if len(missing)>10 else ''}")
    if missing:
        add_pairs = max_i - pairs + 1
        print(f"  Need to add {add_pairs} zero pairs to cover up to index {max_i}")
        if apply:
            with open(idx_file, 'ab') as f:
                f.write(b'\\x00\\x00\\x00\\x00' * add_pairs)
            with open(idx_file, 'rb') as f:
                new_pairs = len(f.read())//4
            print(f"  Wrote new index.ka; new pairs: {new_pairs}")
            return new_pairs
        else:
            print('  (dry run)')
    else:
        print('  No missing entries; index.ka covers all png indices')
    return pairs

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python fix_index_ka.py <head_dir> [--apply] [--old <old_head_dir>]')
        sys.exit(2)
    head = sys.argv[1]
    apply_flag = '--apply' in sys.argv
    old_dir = None
    if '--old' in sys.argv:
        try:
            old_dir = sys.argv[sys.argv.index('--old') + 1]
        except Exception:
            old_dir = None
    # Dry run new head
    new_pairs = scan_and_fix(head, apply=False)
    # Inspect old head if given
    if old_dir and os.path.exists(old_dir):
        scan_and_fix(old_dir, apply=False)
    # Apply to new head if requested
    if apply_flag:
        scan_and_fix(head, apply=True)
    else:
        print('\nRun with --apply to modify index.ka (will append zero offsets).')
