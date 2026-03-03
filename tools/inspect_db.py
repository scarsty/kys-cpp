import sqlite3, sys
sys.stdout.reconfigure(encoding='utf-8')
db_path = sys.argv[1] if len(sys.argv) > 1 else 'd:/projects/kys-cpp/kys-cpp/work/game-dev/save/0.db'
conn = sqlite3.connect(db_path)
cur = conn.cursor()

# List tables
cur.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
tables = [r[0] for r in cur.fetchall()]
print("Tables:", tables)

# For each table, show columns and row count
for t in tables:
    cur.execute(f"PRAGMA table_info([{t}])")
    cols = [(r[1], r[2]) for r in cur.fetchall()]
    cur.execute(f"SELECT COUNT(*) FROM [{t}]")
    count = cur.fetchone()[0]
    print(f"\n=== {t} ({count} rows) ===")
    for name, typ in cols:
        print(f"  {name} ({typ})")

# Show a sample role row
if 'role' in tables:
    cur.execute("SELECT * FROM role LIMIT 1")
    row = cur.fetchone()
    cur.execute("PRAGMA table_info(role)")
    col_names = [r[1] for r in cur.fetchall()]
    print("\n=== Sample role row ===")
    for cn, val in zip(col_names, row):
        print(f"  {cn} = {val}")

if 'role_old' in tables:
    cur.execute("PRAGMA table_info(role_old)")
    cols = [(r[1], r[2]) for r in cur.fetchall()]
    print(f"\n=== role_old columns ({len(cols)}) ===")
    for name, typ in cols:
        print(f"  {name} ({typ})")
    cur.execute("SELECT * FROM role_old LIMIT 1")
    row = cur.fetchone()
    col_names = [r[1] for r in [c for c in cols]]
    # Too many, just show first 20
    print("\n=== Sample role_old row (first 30) ===")
    for cn_tuple, val in zip(cols[:30], row[:30]):
        print(f"  {cn_tuple[0]} = {val}")

if 'magic' in tables:
    cur.execute("SELECT * FROM magic LIMIT 1")
    row = cur.fetchone()
    cur.execute("PRAGMA table_info(magic)")
    col_names = [r[1] for r in cur.fetchall()]
    print(f"\n=== Sample magic row ===")
    for cn, val in zip(col_names, row):
        print(f"  {cn} = {val}")

if 'magic_old' in tables:
    cur.execute("PRAGMA table_info(magic_old)")
    cols = [r[1] for r in cur.fetchall()]
    print(f"\n=== magic_old columns ({len(cols)}) ===")
    for name in cols:
        print(f"  {name}")

conn.close()
