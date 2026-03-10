#!/usr/bin/env python3
"""Fix chess config files - properly map 2000-based IDs to new IDs."""

import sqlite3
import sys
import re
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parents[1]
DB_PATH = REPO_ROOT / "work" / "game-dev" / "save" / "0.db"

conn = sqlite3.connect(DB_PATH)
cur = conn.cursor()

trad_names = [
    '胡一刀', '田歸農', '閻基', '王元霸', '田伯光', '東方不敗', '黃裳', '張三丰',
    '張無忌', '韋一笑', '謝遜', '周芷若', '楊逍', '殷天正', '滅絕', '趙敏',
    '宋青書', '張翠山', '黛綺絲', '范遙', '宋遠橋', '張松溪', '俞蓮舟', '俞岱巖',
    '殷梨亭', '莫聲谷', '無色禪師', '郭襄', '胡斐', '程靈素', '平一指', '胡青牛',
    '成崑', '岳不群', '馬鈺', '王處一', '譚處端', '劉處玄'
]

placeholders = ','.join('?' * len(trad_names))
cur.execute(f'SELECT 编号, 名字 FROM role WHERE 名字 IN ({placeholders})', trad_names)
name_to_new_id = {name: id for id, name in cur.fetchall()}

old_to_name = {
    2000: '胡一刀', 2001: '田歸農', 2002: '閻基', 2003: '王元霸', 2004: '田伯光',
    2005: '東方不敗', 2006: '黃裳', 2007: '張三豐', 2008: '張無忌', 2009: '韋一笑',
    2010: '謝遜', 2011: '周芷若', 2012: '楊逍', 2013: '殷天正', 2014: '滅絕師太',
    2015: '趙敏', 2016: '宋青書', 2017: '張翠山', 2018: '黛綺絲', 2019: '范瑤',
    2020: '宋遠橋', 2021: '張松溪', 2022: '俞蓮舟', 2023: '俞岱巖', 2024: '殷梨亭',
    2025: '莫聲谷', 2026: '無色禪師', 2027: '郭襄', 2028: '胡斐', 2029: '程靈素',
    2030: '平一指', 2031: '胡青牛', 2032: '成昆', 2033: '岳不群', 2034: '馬鈺',
    2035: '王處一', 2036: '譚處端', 2037: '劉處玄'
}

id_mapping = {}
for old_id, name in old_to_name.items():
    new_id = name_to_new_id.get(name)
    if new_id is not None:
        id_mapping[old_id] = new_id

print(f"Mapping {len(id_mapping)} characters")
for old_id, new_id in sorted(id_mapping.items())[:5]:
    print(f"  {old_id} -> {new_id}")

for pattern in ['**/chess_pool*.yaml', '**/chess_combos.yaml']:
    for file_path in REPO_ROOT.glob(pattern):
        content = file_path.read_text(encoding='utf-8')
        original = content

        # Step 1: Replace with temp placeholders
        for old_id in sorted(id_mapping.keys(), reverse=True):
            content = re.sub(rf'^(\s+)-\s+{old_id}\s', rf'\1- TEMP{old_id} ', content, flags=re.MULTILINE)

        # Step 2: Replace temp placeholders with new IDs
        for old_id, new_id in id_mapping.items():
            content = re.sub(rf'^(\s+)-\s+TEMP{old_id}\s', rf'\1- {new_id} ', content, flags=re.MULTILINE)

        if content != original:
            file_path.write_text(content, encoding='utf-8')
            print(f"Updated: {file_path.relative_to(REPO_ROOT)}")

print("Done!")
