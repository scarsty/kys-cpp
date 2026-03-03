import sqlite3, sys
sys.stdout.reconfigure(encoding='utf-8')
conn = sqlite3.connect('d:/projects/kys-cpp/kys-cpp/work/game-dev/save/0.db')
cur = conn.cursor()

# Show a few roles with their skills
cur.execute("""
    SELECT 编号, 名字, 
           所会武功1, 所会武功2, 所会武功3, 所会武功4, 所会武功5, 所会武功6,
           武功等级1, 武功等级2, 武功等级3, 武功等级4, 武功等级5, 武功等级6,
           武功威力1, 武功威力2, 武功威力3, 武功威力4, 武功威力5, 武功威力6
    FROM role_old
    WHERE 编号 IN (0, 1, 55, 100, 200)
""")
rows = cur.fetchall()
for row in rows:
    rid, name = row[0], row[1]
    skills = list(row[2:8])
    levels = list(row[8:14])
    powers = list(row[14:20])
    print(f"\nRole {rid} ({name}):")
    for i in range(6):
        if skills[i] and skills[i] > 0:
            print(f"  Slot {i}: 武功={skills[i]}, 等级={levels[i]}, 威力={powers[i]}")

# Check magic attack table for a few skills
print("\n--- Magic attacks for some skills ---")
cur.execute("""
    SELECT 编号, 名称, 威力1, 威力2, 威力3, 威力4, 威力5, 威力6, 威力7, 威力8, 威力9, 威力10
    FROM magic WHERE 编号 IN (1, 2, 3, 10, 20, 50)
""")
for row in cur.fetchall():
    mid, name = row[0], row[1]
    attacks = list(row[2:])
    print(f"  Magic {mid} ({name}): attacks={attacks}")

# Count how many roles have skills
cur.execute("SELECT COUNT(*) FROM role_old WHERE 所会武功1 > 0")
print(f"\nRoles with at least 1 skill: {cur.fetchone()[0]}")
cur.execute("SELECT COUNT(*) FROM role_old WHERE 所会武功3 > 0")
print(f"Roles with at least 3 skills: {cur.fetchone()[0]}")
cur.execute("SELECT COUNT(*) FROM role_old WHERE 武功威力1 > 0")
print(f"Roles with 武功威力1 > 0: {cur.fetchone()[0]}")

conn.close()
