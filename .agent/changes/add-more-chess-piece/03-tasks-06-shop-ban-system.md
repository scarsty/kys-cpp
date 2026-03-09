# Task 6: Shop System — Configurable Size, Ban System, Seen Tracking

**Depends on**: Task 3 (compiles), Task 2 (balance config has new YAML fields)
**Estimated complexity**: Medium
**Type**: Feature

## Objective

Make shop slot count configurable (5 or 6), implement chess banning system (seen tracking + ban UI + pool filtering), and add persistence for both via GameDataStore.

## ⚠️ Important information

Before coding, Read FIRST -> Load [03-tasks-00-READBEFORE.md](03-tasks-00-READBEFORE.md)

Then read the full specification: [01-specification.md](01-specification.md) — specifically "Shop Size Configuration" and "Chess Banning System" sections.

Before implementing, read:
- `src/ChessBalance.h/cpp` — how config fields are declared and parsed
- `src/ChessPool.h/cpp` — the `getChessFromPool()` loop and `selectFromPool()` rejection logic
- `src/ChessShopFlow.cpp` — the full shop UI flow
- `src/GameDataStore.h/cpp` — serialization structure

## Files to Modify
- `src/ChessBalance.h`
- `src/ChessBalance.cpp`
- `src/ChessPool.h`
- `src/ChessPool.cpp`
- `src/ChessShopFlow.cpp`
- `src/GameDataStore.h`
- `src/GameDataStore.cpp`

## Detailed Steps

### 1. Add config fields to `ChessBalance.h`

In `BalanceConfig` struct, add:
```cpp
    int shopSlotCount = 5;
    int maxBanCount = 15;
```

### 2. Parse new fields in `ChessBalance.cpp`

Find where YAML config is parsed. Add parsing for `商店数量` and `最大禁棋数`:
```cpp
    if (node["商店数量"]) config.shopSlotCount = node["商店数量"].as<int>();
    if (node["最大禁棋数"]) config.maxBanCount = node["最大禁棋数"].as<int>();
```

### 3. Add persistence fields to `GameDataStore.h`

Add to `GameDataStore` struct:
```cpp
    std::set<int> seenRoleIds;
    std::set<int> bannedRoleIds;
```

### 4. Update `GameDataStore.cpp` serialization

Add `seenRoleIds` and `bannedRoleIds` to the glaze JSON serialization. Check how other `set<int>` fields (like `completedChallenges`) are serialized — follow the same pattern.

Also update `reset()` to clear these fields.

### 5. Parameterize shop slot count in `ChessPool.cpp`

In `getChessFromPool()`, change:
```cpp
    for (int i = 0; i < 5; ++i)
```
to:
```cpp
    for (int i = 0; i < ChessBalance::config().shopSlotCount; ++i)
```

### 6. Add ban filtering to `ChessPool`

In `ChessPool.h`, add:
```cpp
    void setBannedRoleIds(const std::set<int>& banned);
private:
    std::set<int> banned_;
```

In `ChessPool.cpp`, `selectFromPool()`, after the `rejected_` check:
```cpp
    if (banned_.contains(idx))
        continue;
```

Add an infinite-loop guard (max 100 iterations). If exhausted, return a random unbanned role from the tier.

Implement `setBannedRoleIds`:
```cpp
void ChessPool::setBannedRoleIds(const std::set<int>& banned) {
    banned_ = banned;
}
```

### 7. Update `ChessShopFlow.cpp` — Seen tracking

After the shop roll in `getChess()`, track which roles appeared:
```cpp
    for (auto [role, tier] : rollOfChess) {
        // Add to seen set for ban eligibility
        gameDataStore.seenRoleIds.insert(role->ID);
    }
```

Find where `GameDataStore` is accessible from the shop flow (likely through `services_`).

### 8. Add ban UI flow

Create a new function (e.g., `showBanMenu()`) callable from the main chess menu. This function:
1. Gets the list of `seenRoleIds` from GameDataStore.
2. Gets current `bannedRoleIds`.
3. Uses `makeIndexedMenu` to create a scrollable list of all seen characters.
4. Each entry shows: character name, tier color, and ban status (已禁 / 未禁).
5. Clicking toggles ban status (if under max ban count).
6. Title shows: `禁棋管理 已禁{count}/{max}`.
7. If `maxBanCount == 0` (Easy mode), hide or disable the ban button.

**Where to add the ban button**: In the main chess menu (not the shop menu). Search for where the main menu is built — likely in a file like `ChessFlow.cpp` or similar. Add a "禁棋" menu option that calls `showBanMenu()`.

### 9. Pass banned set to ChessPool

Where `ChessPool` is created or the shop is initialized each round, call:
```cpp
    pool.setBannedRoleIds(gameDataStore.bannedRoleIds);
```

### 10. Adapt shop UI for 6 slots

The shop UI in `ChessShopFlow.cpp` already dynamically sizes based on `menuData.labels.size()` and sets `menuConfig.perPage`. With 6 chess + 2 buttons = 8 items, this should work automatically. Verify that the layout doesn't overflow the screen. If it does, adjust `menuAnchor` or `fontSize`.

### 11. Verify and commit

Build, test with 6-slot shop, test banning a character and verifying it doesn't appear in subsequent shops.

Commit: `feat: configurable shop size, chess banning system with persistence`

## Acceptance Criteria
- [ ] Shop displays 6 chess slots in Normal mode, 5 in Easy mode
- [ ] `seenRoleIds` is populated after each shop roll and persists across saves
- [ ] `bannedRoleIds` is populated via ban UI and persists
- [ ] Banned characters never appear in shop rolls
- [ ] Ban UI shows seen characters with toggle, respects max ban count
- [ ] Ban UI is hidden/disabled when `maxBanCount == 0`
- [ ] `selectFromPool()` has infinite-loop protection against all-banned tiers
- [ ] GameDataStore backward compatibility: old saves load without crash (new fields default to empty)
- [ ] Project compiles without errors

## Notes
- The existing `rejected_` set in ChessPool prevents the same role appearing twice in one shop roll. The `banned_` set is a separate concept — permanently excluded from the pool for the run.
- Glaze JSON serialization: check how `std::set<int> completedChallenges` is already serialized in `GameDataStore.cpp` — use the exact same pattern for the new sets.
- The ban UI doesn't need to be fancy — a simple scrollable list with toggle is sufficient. The existing `makeIndexedMenu` pattern handles scrolling.
