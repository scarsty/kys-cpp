# Task 7: Easy Mode Pool — Curated Subset

**Depends on**: Task 2 (pool and combo configs), Task 6 (difficulty-aware pool loading)
**Estimated complexity**: Low
**Type**: Feature

## Objective

Create a curated Easy mode pool (`chess_pool_easy.yaml`) with ~50-55 characters where every synergy either has 0 members or enough for its maximum threshold. Wire up Easy mode to load this pool instead of the full pool.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Read the specification: [01-specification.md](01-specification.md) — "Easy Mode Curated Pool" section.

## Files to Modify
- `src/ChessBalance.cpp` (or wherever pool YAML is loaded, to use difficulty-specific file)

## Files to Create
- `config/chess_pool_easy.yaml`

## Reference Files to Read
- `config/chess_pool.yaml` — Full pool format
- `config/chess_combos.yaml` — All synergy member lists (after Task 2 updates)
- `src/ChessBalance.cpp` — How pool config is loaded

## Detailed Steps

### 1. Determine the pool loading mechanism

Read `src/ChessBalance.cpp` to understand how `chess_pool.yaml` is loaded into `BalanceConfig::chessPool`. Check if it's loaded in `loadConfig()` or separately. Determine how to make it difficulty-aware.

### 2. Wire up difficulty-specific pool file

Modify the pool loading to:
- If difficulty == Easy: load `config/chess_pool_easy.yaml`
- If difficulty == Normal: load `config/chess_pool.yaml`

This might be as simple as changing the filename in the load call based on `getDifficulty()`.

### 3. Create `config/chess_pool_easy.yaml`

Select ~50-55 characters from the full 88-character pool. For each synergy in `chess_combos.yaml`, decide:
- **Include ALL members** (synergy is fully available), or
- **Include ZERO members** (synergy doesn't appear at all)

**Strategy**: Keep the most fun/impactful synergies complete. Drop niche synergies entirely. Ensure tier distribution remains balanced.

Suggested approach:
- Keep: 天龙三兄弟 (3), 大理段氏 (4), 少林寺, 拳师 subset (9), 剑客 subset (9), 红颜 subset (9), 全真教 (7), 独行 subset, 真武七截 (partially or fully), etc.
- Drop: Very complex mechanics like 乾坤挪移 (2 members), 龙套 (death AOE), 先天神照 (2 members)
- The goal is a simpler, more forgiving experience.

Use the same YAML format as `chess_pool.yaml` with name comments.

### 4. Validate synergy completeness

Write a quick check (mental or script) verifying:
- For each synergy in `chess_combos.yaml`, count members present in the Easy pool.
- If count > 0 but less than the synergy's maximum threshold member count: fix by adding/removing members.
- All-or-nothing is the rule.

### 5. Verify
Build and run in Easy mode. Verify the shop only shows characters from the Easy pool.

### 6. Commit: `feat: add curated Easy mode chess pool`

## Acceptance Criteria
- [ ] `chess_pool_easy.yaml` exists with ~50-55 characters
- [ ] Every synergy has either 0 or enough members for max threshold
- [ ] Easy mode loads `chess_pool_easy.yaml`
- [ ] Normal mode still loads `chess_pool.yaml`
- [ ] Tier distribution is balanced (not all tier-1 or all tier-5)
- [ ] Project compiles and runs in Easy mode without errors

## Notes
- This task is relatively simple once the pool loading mechanism is understood.
- The completeness check is the most important part — incomplete synergies confuse players.
- Easy mode also has `shopSlotCount = 5` and `maxBanCount = 0` (from Task 6).
