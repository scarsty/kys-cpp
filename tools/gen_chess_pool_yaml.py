# -*- coding: utf-8 -*-
import sqlite3, sys
sys.stdout.reconfigure(encoding='utf-8')

conn = sqlite3.connect('d:/projects/kys-cpp/kys-cpp/work/game-dev/save/0.db')
cur = conn.cursor()

tiers = [
    [130,131,132,133,134,135,136,63,84,160,161,45,47,104,105],
    [54,37,97,56,44,48,99,100,102,115],
    [55,67,68,59,46,51,53,70,98,103,112,113],
    [57,60,64,65,69,58,62,49,50,117,118],
    [129,114,116],
]

lines = []
lines.append("角色:")
for i, ids in enumerate(tiers):
    lines.append(f"  # 费用: {i+1}")
    for rid in ids:
        cur.execute('SELECT 名字 FROM role WHERE 编号=?', (rid,))
        name = cur.fetchone()[0].strip()
        lines.append(f"  - {rid}  # {name}")

with open('d:/projects/kys-cpp/kys-cpp/config/chess_pool.yaml', 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines) + '\n')

print("Generated chess_pool.yaml")
conn.close()
