"""Migrate chess_combos.yaml from character names to numeric IDs."""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "config"

# Build name->id map from chess_pool.yaml (format: "    - 130  # 柯鎮惡")
name_to_id = {}
for line in (ROOT / "chess_pool.yaml").read_text(encoding="utf-8").splitlines():
    m = re.match(r"\s*-\s*(\d+)\s*#\s*(.+)", line)
    if m:
        name_to_id[m.group(2).strip()] = int(m.group(1))

# Transform chess_combos.yaml
lines = (ROOT / "chess_combos.yaml").read_text(encoding="utf-8").splitlines()
out = []
for line in lines:
    # Match inline member arrays like "    成员: [楊過, 小龍女]"
    m = re.match(r"^(\s*)成员:\s*\[(.+)\]", line)
    if m:
        indent = m.group(1)
        names = [n.strip() for n in m.group(2).split(",")]
        out.append(f"{indent}成员:")
        for name in names:
            if name in name_to_id:
                out.append(f"{indent}  - {name_to_id[name]}  # {name}")
            else:
                print(f"WARNING: '{name}' not found in chess_pool.yaml")
                out.append(f"{indent}  - {name}  # NOT FOUND")
    else:
        out.append(line)

(ROOT / "chess_combos.yaml").write_text("\n".join(out) + "\n", encoding="utf-8")
print(f"Done. Mapped {len(name_to_id)} names. Output: {ROOT / 'chess_combos.yaml'}")
