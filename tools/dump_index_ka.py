"""Dump index.ka offsets from a zip file. Usage: python dump_index_ka.py <zipfile> [count]"""
import zipfile, struct, sys

if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <zipfile> [count]")
    sys.exit(1)

zip_path = sys.argv[1]
count = int(sys.argv[2]) if len(sys.argv) > 2 else 20

with zipfile.ZipFile(zip_path, 'r') as z:
    data = z.read('index.ka')

total = len(data) // 4
print(f"File: {zip_path}")
print(f"Total entries: {total}")
print(f"Showing first {min(count, total)}:\n")
print(f"{'Index':>6}  {'dx':>6}  {'dy':>6}")
print("-" * 22)

for i in range(min(count, total)):
    dx, dy = struct.unpack_from('<hh', data, i * 4)
    print(f"{i:>6}  {dx:>6}  {dy:>6}")
