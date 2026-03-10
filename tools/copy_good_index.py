#!/usr/bin/env python3
"""Copy the good index.ka and extend it with zeros for missing entries."""
import struct, glob, os, re

good = r"D:\games\金群自走棋+0.1.11-beta\game\resource\head\index.ka"
bad = r"D:\projects\kys-cpp\kys-cpp\work\game-dev\resource\head\index.ka"
head_dir = r"D:\projects\kys-cpp\kys-cpp\work\game-dev\resource\head"

# Read good index.ka
with open(good, 'rb') as f:
    good_data = f.read()
good_pairs = len(good_data) // 4
print(f"Good index.ka has {good_pairs} pairs")

# Find max PNG index
pngs = glob.glob(os.path.join(head_dir, '*.png'))
nums = []
for p in pngs:
    m = re.match(r'(\d+)\.png$', os.path.basename(p), re.I)
    if m:
        nums.append(int(m.group(1)))
nums.sort()
max_idx = max(nums) if nums else 0
print(f"Found {len(nums)} numeric PNGs, max index: {max_idx}")

# Extend with zeros if needed
needed = max_idx + 1
if needed > good_pairs:
    print(f"Need to extend from {good_pairs} to {needed} pairs")
    new_data = good_data + b'\x00\x00\x00\x00' * (needed - good_pairs)
else:
    new_data = good_data

# Write
with open(bad, 'wb') as f:
    f.write(new_data)
print(f"Wrote {len(new_data)//4} pairs to {bad}")
