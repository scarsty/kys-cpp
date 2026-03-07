#!/usr/bin/env python3
import sqlite3
import sys
import re

sys.stdout.reconfigure(encoding='utf-8')

# Parse YAML manually (simple key-value extraction)
equipment = []
with open('config/chess_equipment.yaml', 'r', encoding='utf-8') as f:
    current = {}
    for line in f:
        if '装备ID:' in line:
            if current:
                equipment.append(current)
            match = re.search(r'装备ID:\s*(\d+)\s*#\s*(.+)', line)
            current = {'id': int(match.group(1)), 'name': match.group(2).strip()} if match else {}
        elif '装备类型:' in line and current:
            match = re.search(r'装备类型:\s*(\d+)', line)
            if match:
                current['type'] = int(match.group(1))
    if current:
        equipment.append(current)

# Connect to database
conn = sqlite3.connect('work/game-dev/save/0.db')
cur = conn.cursor()

errors = []
for eq in equipment:
    item_id = eq['id']
    yaml_name = eq.get('name', '?')
    yaml_type = eq.get('type', -1)

    cur.execute('SELECT 物品名, 装备类型, 物品类型 FROM item WHERE 编号 = ?', (item_id,))
    row = cur.fetchone()

    if not row:
        errors.append(f"❌ ID {item_id}: Not found in database")
        continue

    db_name, db_equip_type, db_item_type = row

    if db_item_type != 1:
        errors.append(f"❌ ID {item_id} ({db_name}): Not equipment (物品类型={db_item_type})")
    if db_equip_type != yaml_type:
        errors.append(f"❌ ID {item_id} ({db_name}): Type mismatch - YAML={yaml_type}, DB={db_equip_type}")
    if yaml_name != db_name:
        errors.append(f"❌ ID {item_id}: Name mismatch - YAML='{yaml_name}', DB='{db_name}'")

    if not errors or not errors[-1].startswith('❌ ID ' + str(item_id)):
        print(f"✓ ID {item_id}: {db_name} (类型{db_equip_type})")

conn.close()

if errors:
    print("\n=== ERRORS ===")
    for err in errors:
        print(err)
    sys.exit(1)
else:
    print(f"\n✓ All {len(equipment)} equipment validated successfully!")
