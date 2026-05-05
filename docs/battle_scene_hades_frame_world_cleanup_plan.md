# BattleSceneHades Frame World Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the battle frame build path data-owned and callback-free, so the scene captures one legacy snapshot, the adapter translates that snapshot, and the frame runner no longer depends on hidden scene lookups.

**Architecture:** Add one per-frame legacy snapshot that owns role, combo, grid, random, magic, item, and runtime lookup facts for the frame. Adapter population functions consume that snapshot data instead of `std::function` callbacks back into `BattleSceneHades`. Cleanup work must reduce coupling or delete dead code; splitting `buildCoreFrameBundle()` into cosmetic helpers is not sufficient.

**Tech Stack:** C++23, existing `src/battle` systems, Catch2 tests under `tests`, MSBuild through `.github\build-command.ps1`.

---

## Why This Plan Exists

The previous backend exit work achieved the one-runner-call boundary, but `buildCoreFrameBundle()` is still awkward because it assembles a frame through repeated scene lookups and callback contexts:

- `BattleActionFrameAdapterCallbacks` hides role, magic, grid, random, movement-runtime, and targeting lookups behind `std::function`.
- rescue and movement physics population still ask callback objects for grid and movement facts.
- the scene repeatedly linearly searches `battle_roles_` by battle id during frame build/apply.
- some bundle/test fields are transitional and unused by production.

The cleanup goal is not to make the current shape prettier. The cleanup goal is to make the data model more honest: frame population reads captured frame data, not live scene behavior.

## Non-Negotiable Rules

- Do not introduce new `std::function` callbacks in frame-build adapter contexts.
- Do not move `BattleSceneHades` gameplay logic into adapter functions unless the adapter consumes explicit snapshot data.
- Do not make a new orchestrator or second runner.
- Do not split `buildCoreFrameBundle()` into smaller methods unless the split also removes callbacks, duplicated lookups, or dead state.
- Keep presentation and legacy object mutation outside core, but pass them explicit committed result data.
- Use assertions for required invariants; do not add broad defensive no-op branches.
- Use Traditional Chinese for touched source comments and string literals.
- Drop backwards compatibility and remove transitional fields/tests in the same task that makes them obsolete.

## Target Shape

The frame path should move toward:

```cpp
auto snapshot = buildBattleFrameLegacySnapshot();
auto bundle = battle_adapter_.buildFrameBundle(snapshot);
auto frameResult = KysChess::Battle::BattleFrameRunner().advanceFrame(bundle.state);
battle_adapter_.applyFrameResult(snapshot, bundle, frameResult);
```

The exact helper names are not important. The important ownership rules are:

- snapshot construction may inspect `BattleSceneHades`, `Role`, `Magic`, `Item`, grid, random, and combo state once;
- adapter frame population consumes snapshot facts without calling back into the scene;
- core frame runner receives complete frame state;
- apply path uses `rolesByBattleId` and committed deltas, not fresh linear searches for every event.

## File Responsibilities

- `src/BattleSceneHades.h/.cpp`
  - Owns snapshot construction from live legacy scene state.
  - Owns legacy object application after the runner.
  - Does not expose callback bundles to adapter frame-build code.
- `src/BattleSceneBattleAdapter.h/.cpp`
  - Owns snapshot-to-core translation and committed-delta application helpers.
  - May read `Role*`, `Magic*`, and `Item*` from snapshot structs.
  - Must not hold `std::function` callback fields for frame build.
- `src/battle/BattleCore.h/.cpp`
  - Owns core frame state and runner behavior.
  - Should not gain legacy `Role*`, `Magic*`, `Item*`, scene, render, engine, or save dependencies.
- `tests/BattleCoreUnitTests.cpp`
  - Remove stale bundle tests that only prove transitional fields exist.
  - Add focused tests only where core/adapter pure functions can be tested without constructing the whole scene.

---

## Task 0: Delete Proven Transitional Dead Code

**Purpose:** Remove fields and helpers that only preserve the old migration shape.

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] Remove `BattleSceneFrameBundle::pendingPresentationEvents` from `src/BattleSceneBattleAdapter.h`.
- [ ] Delete `TEST_CASE("BattleSceneFrameBundle_CarriesCoreStateAndAdapterApplicationFacts", "[battle][core]")` from `tests/BattleCoreUnitTests.cpp`.
- [ ] Delete the unused declaration and definition of `BattleSceneHades::applyCoreStatusDamageState`.
- [ ] Delete the unused declaration and definition of `BattleSceneHades::makeNoOpCoreWorld`.
- [ ] Delete empty `BattleSceneHades::makeSpecialMagicEffect()` and its call from `BattleSceneHades::onEntrance()` if the call still targets only that empty function.
- [ ] Run:

```powershell
rg -n "pendingPresentationEvents|applyCoreStatusDamageState|makeNoOpCoreWorld|makeSpecialMagicEffect" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: the search returns no matches, the test target builds, and core tests pass.

- [ ] Commit:

```powershell
git add src\BattleSceneBattleAdapter.h src\BattleSceneHades.h src\BattleSceneHades.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: delete stale battle frame migration code"
```

## Task 1: Introduce One Legacy Frame Snapshot

**Purpose:** Capture repeated scene facts once per frame before removing callback contexts.

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] Add snapshot structs to `src/BattleSceneBattleAdapter.h`:

```cpp
struct BattleFrameLegacyRoleSnapshot
{
    Role* role = nullptr;
    int unitId = -1;
    Point grid;
    bool alive = false;
    int movementDashFrames = 0;
    int movementDashCooldown = 0;
    int movementDashSpreadFrames = 0;
};

struct BattleFrameLegacyGridSnapshot
{
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::vector<Battle::BattleMovementPhysicsCellSnapshot> movementCells;
};

struct BattleFrameLegacySnapshot
{
    int battleFrame = 0;
    std::vector<Role*> roles;
    std::vector<BattleFrameLegacyRoleSnapshot> roleSnapshots;
    std::unordered_map<int, Role*> rolesByBattleId;
    std::map<int, RoleComboState>* comboStates = nullptr;
    BattleFrameLegacyGridSnapshot grid;
};
```

- [ ] Add this declaration to `BattleSceneHades`:

```cpp
KysChess::BattleSceneBattleAdapter::BattleFrameLegacySnapshot buildBattleFrameLegacySnapshot();
```

- [ ] Implement `BattleSceneHades::buildBattleFrameLegacySnapshot()` by:
  - copying `battle_frame_`;
  - copying `battle_roles_` into `snapshot.roles`;
  - setting `snapshot.comboStates = &KysChess::ChessCombo::getMutableStates()`;
  - filling `rolesByBattleId` for every non-null role;
  - filling `roleSnapshots` with each role's battle id, grid from `pos90To45`, alive flag, and movement runtime values;
  - filling `grid.rescueCells` and `grid.movementCells` in one nested `BATTLE_COORD_COUNT` loop.

- [ ] Change `buildCoreFrameBundle()` to start with:

```cpp
auto snapshot = buildBattleFrameLegacySnapshot();
```

Then use `snapshot.rolesByBattleId` to initialize `bundle.rolesByBattleId`, and use `*snapshot.comboStates` instead of repeatedly calling `ChessCombo::getMutableStates()` inside that method.

- [ ] Run:

```powershell
rg -n "ChessCombo::getMutableStates\(\)" src\BattleSceneHades.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core]"
```

Expected: build/tests pass. The search may still find legitimate apply-side combo writes, but `buildCoreFrameBundle()` should no longer call `ChessCombo::getMutableStates()` more than once through snapshot construction.

- [ ] Commit:

```powershell
git add src\BattleSceneBattleAdapter.h src\BattleSceneHades.h src\BattleSceneHades.cpp
git commit -m "refactor: capture battle frame legacy snapshot"
```

## Task 2: Remove Rescue And Movement Build Callbacks

**Purpose:** Make rescue and movement physics population consume captured grid/runtime facts instead of calling scene lambdas.

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`

- [ ] Delete `BattleRescueFrameAdapterCallbacks` from `src/BattleSceneBattleAdapter.h`.
- [ ] Change `BattleRescueFrameAdapterContext` to hold explicit positions:

```cpp
std::map<std::pair<int, int>, Pointf> positionsByCell;
```

- [ ] Update `populateBattleFrameRescueState()` to use `context.positionsByCell` and each `BattleFrameLegacyRoleSnapshot::grid` value instead of `context.callbacks.toGrid` and `context.callbacks.toPosition`.
- [ ] Delete `toGrid` and `toPosition` callback assignment from the rescue section of `buildCoreFrameBundle()`.
- [ ] Delete `BattleMovementPhysicsFrameAdapterCallbacks` from `src/BattleSceneBattleAdapter.h`.
- [ ] Change `BattleMovementPhysicsFrameAdapterContext` to hold:

```cpp
const BattleFrameLegacySnapshot* snapshot = nullptr;
std::vector<Battle::BattleMovementPhysicsCellSnapshot> cells;
```

- [ ] Update `populateBattleMovementPhysicsFrame()` to read movement runtime values and cells from `context.snapshot`/`context.cells`, not callback functions.
- [ ] Move movement-dash runtime application out of `applyBattleMovementPhysicsFrameResults()` callback use and into `BattleSceneHades::applyCoreFrameBundle()` by directly writing `movement_runtime_[role]`.
- [ ] Run:

```powershell
rg -n "BattleRescueFrameAdapterCallbacks|BattleMovementPhysicsFrameAdapterCallbacks|callbacks\.toGrid|callbacks\.toPosition|callbacks\.canWalk|callbacks\.movementDashFrames|callbacks\.movementDashCooldown|callbacks\.movementDashSpreadFrames|callbacks\.setMovementDashRuntime" src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][movement],[battle][rescue_reposition][unit]"
```

Expected: callback search returns no rescue/movement build callback matches, and focused tests pass.

- [ ] Commit:

```powershell
git add src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
git commit -m "refactor: populate rescue and movement from frame snapshot"
```

## Task 3: Replace Action Build Callbacks With Snapshot Facts

**Purpose:** Remove the largest hidden coupling: action population should read selected per-unit facts, not call back into scene for target selection, magic selection, geometry, random, or movement runtime.

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`

- [ ] Add action snapshot structs to `src/BattleSceneBattleAdapter.h`:

```cpp
struct BattleFrameLegacyActionUnitSnapshot
{
    int unitId = -1;
    int nearestEnemyUnitId = -1;
    int farthestEnemyUnitId = -1;
    Magic* normalMagic = nullptr;
    Magic* ultimateMagic = nullptr;
    bool forceRangedMagic = false;
    int forcedRangedMinSelectDistance = 0;
    int projectileSpeedMultiplierPct = 100;
    int ultimateExtraProjectileCount = 0;
    double normalEffectiveReach = 0.0;
    double ultimateEffectiveReach = 0.0;
    bool normalRangedStyle = false;
    bool ultimateRangedStyle = false;
    double normalBlinkReach = 0.0;
    double ultimateBlinkReach = 0.0;
    int castFrameByOperation[4] = {};
    double randomUnitRolls[2] = {};
    int projectileBounceRoll = 0;
    int blinkRandomRoll = 0;
    int blinkCellRandomRoll = 0;
};
```

- [ ] Add this field to `BattleFrameLegacySnapshot`:

```cpp
std::unordered_map<int, BattleFrameLegacyActionUnitSnapshot> actionsByUnitId;
```

- [ ] In `buildBattleFrameLegacySnapshot()`, fill one `BattleFrameLegacyActionUnitSnapshot` per role using existing scene methods:
  - `findNearestEnemy`;
  - `findFarthestEnemy`;
  - `selectMagic`;
  - `roleForcesRangedMagic`;
  - `getForcedRangedMinSelectDistance`;
  - `getProjectileSpeedMultiplierPct`;
  - `getUltimateExtraProjectileCount`;
  - `effectiveBattleReach`;
  - `isRangedStyleMagic`;
  - `calcBlinkReach`;
  - `calCast`;
  - `rand_.rand()` and `rand_.rand_int(...)`.

- [ ] Delete `BattleActionFrameAdapterCallbacks` from `src/BattleSceneBattleAdapter.h`.
- [ ] Change `BattleActionFrameAdapterContext` to hold:

```cpp
const BattleFrameLegacySnapshot* snapshot = nullptr;
```

- [ ] Update these adapter functions to use `context.snapshot->actionsByUnitId.at(role->ID)` instead of callbacks:
  - `makeActionFrameSkillInput`;
  - `rollDashHitCount`;
  - `captureActionComboState`;
  - `populateActionCommitInputForRole`;
  - `populateActionCastInputForRole`;
  - `makeActionFrameUnitState`;
  - `makeBlinkGeometryInput`;
  - `applyBlinkTeleportDelta`;
  - `commitBattleSelectedSkillAction`.

- [ ] Keep legacy application effects explicit. For example, replace `context.callbacks.faceTowardsNearest(role)` by returning a committed facing delta or calling a scene-owned apply helper after adapter returns.
- [ ] Run:

```powershell
rg -n "BattleActionFrameAdapterCallbacks|callbacks\." src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][presentation]"
```

Expected: no action callback type or `callbacks.` usage remains in the frame-build adapter path. Focused tests pass.

- [ ] Commit:

```powershell
git add src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.h src\BattleSceneHades.cpp
git commit -m "refactor: populate battle actions from frame snapshot"
```

## Task 4: Consolidate Frame Apply Lookups

**Purpose:** Stop repeated linear role lookup during frame application.

**Files:**

- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] Add a local helper in `BattleSceneHades.cpp` near frame application:

```cpp
Role* requireFrameRole(
    const KysChess::BattleSceneBattleAdapter::BattleSceneFrameBundle& bundle,
    int unitId)
{
    auto it = bundle.rolesByBattleId.find(unitId);
    assert(it != bundle.rolesByBattleId.end());
    assert(it->second);
    return it->second;
}
```

- [ ] Replace `findRoleByBattleId(battle_roles_, ...)` inside `applyCoreFrameBundle()`, `applyCoreStatusState()`, `applyCoreDamageTransactions()`, `applyCoreTeamEffectState()`, `applyCoreFrameApplications()`, and `applyCoreMovementSnapshot()` with `requireFrameRole(...)` where a frame bundle is available.
- [ ] If a helper does not currently accept the bundle, change the signature to accept `const BattleSceneFrameBundle& bundle` instead of only `const BattleFrameState& frameState`.
- [ ] Do not change non-frame UI or setup code in this task.
- [ ] Run:

```powershell
rg -n "findRoleByBattleId\(battle_roles_," src\BattleSceneHades.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][presentation]"
```

Expected: any remaining search matches are outside frame build/apply helpers and are listed in the task verification note before commit.

- [ ] Commit:

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp
git commit -m "refactor: apply battle frame through captured role lookup"
```

## Task 5: Final Coupling Gate

**Purpose:** Prove the cleanup changed the model rather than just reorganizing code.

**Files:**

- Modify: `docs/battle_scene_hades_frame_world_cleanup_plan.md`

- [ ] Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

- [ ] Run boundary searches:

```powershell
rg -n "std::function|callbacks\." src\BattleSceneBattleAdapter.h src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
rg -n "BattleFrameRunner\(\)\.advanceFrame" src\BattleSceneHades.cpp
rg -n "BattleDamageApplicationSystem|BattleRescueRepositionSystem|BattleMovementPhysicsSystem|BattleHitResolver|BattleActionCommitSystem|BattleCastPlanner|BattleTeamEffectSystem|BattleComboTriggerSystem|BattleEffectDispatcher" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected:

- no `std::function` or `callbacks.` remain in frame-build adapter paths;
- `BattleFrameRunner().advanceFrame` still appears once in `BattleSceneHades.cpp`;
- scene/adapter still do not call backend gameplay systems directly;
- `src\battle` still has no legacy/render dependencies.

- [ ] Record exact verification output in `## Verification Log`.
- [ ] Commit:

```powershell
git add docs\battle_scene_hades_frame_world_cleanup_plan.md
git commit -m "docs: record battle frame world cleanup verification"
```

## Verification Log

- 2026-05-06 plan created after `76e7dc3 docs: record battle backend exit verification` on `codex/battle-backend-exit-breakthrough`. The plan intentionally prioritizes callback removal and snapshot ownership over cosmetic method extraction.
