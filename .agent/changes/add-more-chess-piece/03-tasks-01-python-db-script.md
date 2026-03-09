# Task 1: Python DB Script — Add ~37 Characters

**Depends on**: None
**Estimated complexity**: High
**Type**: Feature

## Objective

Create a Python script that adds ~37 new characters (roles + skills) to the game database, importing fight frames and head images from the reference game.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically the "Character Additions" tables and "Database Changes" section.

## Files to Create
- `tools/add_chess_characters.py`

## Reference Files to Read
- `tools/check_roles.py` — Example of how to query the DB
- `tools/set_maxmp_100.py` — Example of DB modification script
- `config/chess_pool.yaml` — Current pool with existing role IDs (to avoid ID conflicts)
- The reference game CSV: `D:\games\人在江湖cpp版\人在江湖cpp版\game\1_renwu.csv`

## Detailed Steps

1. Create `tools/add_chess_characters.py` that:
   a. Connects to `work/game-dev/save/0.db`
   b. Reads `D:\games\人在江湖cpp版\人在江湖cpp版\game\1_renwu.csv` to build a name→{role_id, fight_id, head_id} mapping
   c. For each of the ~37 new characters from the spec:
      - Look up the character in the reference CSV by name (姓名 column)
      - If found: use that role's 编号, 战斗图编号 for fight frame, 头像/战斗代号 for head
      - If not found: assign a new role ID (scan for max ID in DB + offset), use a fallback fight frame
      - INSERT OR REPLACE into `role` table with tier-appropriate stats (see stat ranges in spec)
      - Set 所会武功1 through 所会武功6 based on the reference game's existing skills, or create new magic entries
   d. Copy fight frame zips from `D:\games\人在江湖cpp版\人在江湖cpp版\game\resource\fight\fightXXXX.zip` to `work/game-dev/resource/fight/`
   e. Copy head images from `D:\games\人在江湖cpp版\人在江湖cpp版\game\resource\head\` to `work/game-dev/resource/head/`
   f. Print a summary of all operations performed
   g. Be idempotent (INSERT OR REPLACE, copy only if not exists)

2. The character list with tiers and types is in the spec tables. Key stat guidelines:

   | Tier | MaxHP | Attack | Defense | Speed |
   |------|-------|--------|---------|-------|
   | 1费 | 200-400 | 40-70 | 30-50 | 30-50 |
   | 2费 | 350-550 | 60-90 | 40-65 | 40-60 |
   | 3费 | 500-750 | 80-120 | 55-85 | 50-70 |
   | 4费 | 650-950 | 100-150 | 70-110 | 60-85 |
   | 5费 | 900-1200 | 140-200 | 90-130 | 75-100 |

3. Run the script and verify it succeeds without errors
4. Commit: `feat: add Python script to insert 37 new chess characters into DB`

## Acceptance Criteria
- [ ] Script runs without errors on `work/game-dev/save/0.db`
- [ ] All ~37 characters appear in the `role` table with correct stats
- [ ] Fight frames and head images are copied to the correct directories
- [ ] Script is idempotent (running twice produces same result)
- [ ] Script prints audit log of all changes

## Notes
- The `role` table columns can be inspected with `PRAGMA table_info(role)` or by reading `tools/check_roles.py`
- Some characters already exist in the reference game (灭绝师太 as ID 20, 俞莲舟 as ID 16) — use their existing data
- For characters not in the reference CSV (e.g., 胡青牛, 王元霸), try alternate name searches or use closest-match fight frames
- MaxMP is always 100 (set by `tools/set_maxmp_100.py` convention)
