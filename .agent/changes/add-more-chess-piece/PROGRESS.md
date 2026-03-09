# Implementation Progress: Chess Pool Expansion

## Task Status

| # | Task | Status | Depends On |
|---|------|--------|------------|
| 1 | Python DB Script (add_chess_characters.py) | ✅ Done | None |
| 2 | Config YAML Updates (pool, combos, balance) | ✅ Done | Task 1 |
| 3 | C++ Effect Type Registration (enum, map, state) | ✅ Done | None |
| 4 | Simple Battle Effects (15 effects) | ✅ Done | Task 3 |
| 5 | Complex Battle Effects (4 spatial effects) | ✅ Done | Tasks 3, 4 |
| 6 | Shop Size & Ban System | ✅ Done | Tasks 2, 3 |
| 7 | Easy Mode Curated Pool | ✅ Done | Tasks 2, 6 |
| 8 | Final Validation & Commit Message | ⛔ Blocked (manual smoke pending) | All |

## Parallelization

Tasks **1** and **3** can start in parallel (no dependencies).

```
Task 1 (DB) ──→ Task 2 (YAML) ──→ Task 6 (Shop/Ban) ──→ Task 7 (Easy) ──→ Task 8
Task 3 (Enum) ──→ Task 4 (Simple FX) ──→ Task 5 (Complex FX) ──↗      ──↗
```

## Notes
- Update Status column to 🔄 when starting, ✅ when done
- Automated validation, commit artifacts, and feature commits are complete; the only remaining
  validation is manual in-game smoke testing outside the CLI.
- Exact synergy-complete Easy pools cannot fit the requested 50-55 size under the current combo
  graph; the checked minimum valid subset is 76 roles.
- Existing saves in `work/game-dev/save/1.db` and `work/game-dev/save/4.db` still carry
  `schema_version = 1` payloads without `seenRoleIds` / `bannedRoleIds`, which matches the
  intended backward-compatible defaulting path in `GameDataStore::load()`.
