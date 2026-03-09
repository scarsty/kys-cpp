# 03-tasks-00-READBEFORE — Context for All Tasks

## Project Overview

This is a C++ auto-chess mod (`kys-cpp`) set in the Jin Yong wuxia universe. The project compiles with `/source-charset:utf-8 /execution-charset:utf-8` on Windows (MSVC) and `-std=c++23` on Linux/macOS.

## Key Architecture

- **SQLite3 databases**: `work/game-dev/save/0.db` (and 1.db, 4.db) — tables: `role` (38 columns), `magic` (14 columns), `item`
- **YAML configs**: `config/chess_pool.yaml`, `config/chess_combos.yaml`, `config/chess_balance_normal.yaml`, `config/chess_balance_easy.yaml`, `config/chess_equipment.yaml`, `config/chess_neigong.yaml`
- **Effect system**: `src/ChessBattleEffects.h/cpp` — `EffectType` enum + Chinese→enum map + `RoleComboState` struct + `applyEffect()` switch
- **Combo/synergy system**: `src/ChessCombo.h/cpp` — `ComboId` enum, `ComboDef` parsed from YAML, `detectCombos()` / `buildComboStates()`
- **Battle engine**: `src/BattleSceneHades.cpp` — main combat loop, damage formula: `damage = attack² / (attack + defence) / 4.0`
- **Shop system**: `src/ChessPool.h/cpp` (pool selection), `src/ChessShopFlow.cpp` (UI), `src/ChessBalance.h/cpp` (config)
- **Persistence**: `src/GameDataStore.h/cpp` — JSON serialization via glaze (`glz`), stored in SQLite
- **Battle maps**: `src/DynamicChessMap.h/cpp` — 10 predefined maps, ally positions (6 existing + 4 extended)
- **Build**: `src/CMakeLists.txt` globs all `*.cpp` in `src/`. On Windows, build via `kys.sln`.
- **Reference game**: `D:\games\人在江湖cpp版\` with CSV data and fight/head resources

## Specification

Full spec: [01-specification.md](01-specification.md)

## Plan

Full plan: [02-plan.md](02-plan.md)

## Key Patterns

### Adding a new EffectType
1. Add enum value to `EffectType` in `src/ChessBattleEffects.h`
2. Add corresponding field(s) to `RoleComboState` in same file
3. Add Chinese→enum mapping in `effectTypeMap` in `src/ChessBattleEffects.cpp`
4. Add `case` in `applyEffect()` switch in same file
5. Add runtime behavior in `src/BattleSceneHades.cpp`

### Adding a new ComboId
1. Add enum value to `ComboId` in `src/ChessCombo.h` (before `COUNT`)
2. Add synergy definition in `config/chess_combos.yaml`
3. No C++ code needed — combos are data-driven via YAML

### Adding characters to DB
- Use Python scripts in `tools/` that connect to `work/game-dev/save/0.db`
- Reference game CSV at `D:\games\人在江湖cpp版\人在江湖cpp版\game\1_renwu.csv` (columns: 编号, 头像/战斗代号, 姓名, 战斗图编号)
- Fight frames at `resource/fight/fightXXXX.zip`, head images at `resource/head/`

## Conventions

- All C++ code uses `namespace KysChess { ... }`
- Chinese strings in C++ source are UTF-8 encoded literals
- YAML config keys use Chinese names (e.g., `名称`, `成员`, `阈值`, `效果`, `类型`, `数值`)
- Config YAML comments use `#` with character names for readability (e.g., `- 48  # 游坦之`)
- New `*.cpp` files in `src/` are auto-included in CMake build (glob pattern)
- No `just preflight` command exists in this project; instead verify by building the solution

## Commit Convention

Use conventional commits: `feat:`, `fix:`, `refactor:`, `chore:`, etc.
