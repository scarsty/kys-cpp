# Canonical Runtime Combo State Ingress Cleanup Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the creation-time `comboStates` side-map bridge and make runtime combo state enter battle through one canonical path before becoming record-owned runtime state.

**Architecture:** `BattleRuntimeUnitRecord::combo` is the persistent runtime owner after session creation. Before initialization, combo state should travel with the unit/spawn being initialized, not through a parallel `std::map<int, RoleComboState>` that must be copied into setup seeds and read again while building spawns. `BattleInitializationSystem` should mutate the spawn-owned combo state, then `BattleRuntimeSession` should append that initialized combo into `BattleRuntimeUnitRecord`.

**Tech Stack:** C++23, Catch2 tests through `kys_tests`, PowerShell, Visual Studio/MSBuild via `.github\\build-command.ps1`.

---

## Current Complexity

The current creation path has one logical combo seed represented in multiple places:

- `src/battle/BattleRuntimeSession.h`
  - `BattleRuntimeSessionCreationInput::comboStates` is a side map keyed by unit id.
- `src/battle/BattleRuntimeSession.cpp`
  - `applyCreationComboStatesToSetup()` copies the side map into `input.setup.units[*].baseCombo`.
  - `buildCanonicalSpawns()` independently reads the same side map again while creating `BattleRuntimeUnitSpawn::combo`.
- `src/battle/BattleInitialization.h`
  - `BattleInitializationUnitSeed::baseCombo` carries another copy into initialization.
- `src/battle/BattleInitialization.cpp`
  - `BattleInitializationSystem::initialize()` starts from `seed.baseCombo`, writes the final state back to `spawn.combo`, and returns `result.comboStates`.
- `src/BattleSceneHades.cpp`
  - Scene code still assigns `input.comboStates = ChessCombo::getMutableStates()` before creation, even though initialization now derives combo state from roster/setup definitions.

This creates two cleanup targets:

1. Remove `BattleRuntimeSessionCreationInput::comboStates` as an input bridge.
2. Stop duplicating base combo state between initialization seeds and runtime spawns.

Keep `BattleInitializationResult::comboStates` for now. It is an output/reporting bridge used by scene/UI state after initialization, and can be revisited separately.

---

## Target Shape

- Add `RoleComboState baseCombo` to `BattleSetupUnitInput`.
  - This is the one external/custom session input location for pre-initialization combo state.
- Create each `BattleRuntimeUnitSpawn` from `BattleSetupUnitInput::baseCombo`.
  - Prefer moving it inside `setupBattleRuntime()` because the creation input is passed by value.
- Remove `BattleRuntimeSessionCreationInput::comboStates`.
- Remove `BattleInitializationUnitSeed::baseCombo`.
- Change `BattleInitializationSystem::initialize()` to start from `spawn.combo`, not `seed.baseCombo`.
  - Initialization still appends resolved roster/teamwide/equipment/neigong effects and writes the initialized combo back to `spawn.combo`.
- Keep `BattleInitializationResult::comboStates = comboMapFromSpawns(spawns)` as the post-initialization reporting/output path.

Final ownership flow:

```text
BattleSetupUnitInput::baseCombo
    -> BattleRuntimeUnitSpawn::combo
        -> BattleInitializationSystem mutates spawn.combo
            -> BattleRuntimeUnitRecord::combo
                -> BattleRuntimeState::units.require(id).combo
```

No separate unit-id combo map is needed during creation.

---

## File Structure

- Modify `src/battle/BattleRuntimeSession.h`
  - Add `RoleComboState baseCombo` to `BattleSetupUnitInput`.
  - Remove `BattleRuntimeSessionCreationInput::comboStates`.
- Modify `src/battle/BattleRuntimeSession.cpp`
  - Delete `applyCreationComboStatesToSetup()`.
  - Change `buildCanonicalSpawns()` to read/move `BattleSetupUnitInput::baseCombo`.
  - Remove the call to `applyCreationComboStatesToSetup()`.
- Modify `src/battle/BattleInitialization.h`
  - Remove `BattleInitializationUnitSeed::baseCombo`.
- Modify `src/battle/BattleInitialization.cpp`
  - Initialize the local `RoleComboState combo` from `spawn.combo` instead of `seed.baseCombo`.
  - Keep final `spawn.combo = std::move(combo)`.
- Modify `src/BattleSceneSetupBuilder.cpp`
  - Update `makeInitializationUnitSeed()` after removing `baseCombo` from the seed.
- Modify `src/BattleSceneHades.cpp`
  - Remove `input.comboStates = ChessCombo::getMutableStates()`.
  - Leave the post-initialization `ChessCombo::getMutableStates() = initialization.comboStates` output bridge for this plan.
- Modify tests that write `.comboStates` or `.baseCombo` on initialization seeds.
  - Main files seen during review:
    - `tests/BattleInitializationUnitTests.cpp`
    - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
    - `tests/BattleRuntimeScenarioUnitTests.cpp`
    - `tests/BattleSceneUnitStoreUnitTests.cpp`
    - `tests/BattleSceneTestRuntimeFixture.h`

---

## Task 1: Add the Canonical Base Combo Input

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`

- [ ] **Step 1: Add `baseCombo` to `BattleSetupUnitInput`**

Add the member near the other initial unit facts in `BattleSetupUnitInput`:

```cpp
RoleComboState baseCombo;
```

Use default construction only; no temporary `= {}` assignment is needed if the surrounding style is direct member declaration.

- [ ] **Step 2: Remove the side-map from session creation input**

Delete this member from `BattleRuntimeSessionCreationInput`:

```cpp
std::map<int, RoleComboState> comboStates;
```

Remove now-unused includes if this makes `<map>` unnecessary in `BattleRuntimeSession.h`.

- [ ] **Step 3: Make spawn creation use the per-unit base combo**

Change `buildCanonicalSpawns()` so each spawn is created with the unit's own base combo instead of looking up `input.comboStates`.

Preferred direction:

```cpp
auto spawn = makeRuntimeUnitSpawn(
    makeRuntimeUnit(setup),
    std::move(setup.baseCombo),
    std::move(actionPlan));
```

Because this moves from `input.units`, `buildCanonicalSpawns()` should take a non-const `BattleRuntimeSessionCreationInput&` or otherwise receive/move the units it needs. The creation input is already passed by value into `setupBattleRuntime()`, so moving from it during setup is acceptable.

- [ ] **Step 4: Delete the bridge function**

Remove `applyCreationComboStatesToSetup()` and its call from `setupBattleRuntime()`.

---

## Task 2: Make Initialization Consume Spawn-Owned Combo State

**Files:**
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/BattleSceneSetupBuilder.cpp`

- [ ] **Step 1: Remove `baseCombo` from `BattleInitializationUnitSeed`**

Delete the member:

```cpp
RoleComboState baseCombo;
```

`BattleInitializationUnitSeed` should describe base stats/roster identity for initialization, not carry another copy of runtime combo state.

- [ ] **Step 2: Start initialization from `spawn.combo`**

In `BattleInitializationSystem::initialize()`, replace the current seed-copy start:

```cpp
RoleComboState combo = seed.baseCombo;
```

with spawn-owned state:

```cpp
RoleComboState combo = std::move(spawn.combo);
```

The function already writes the completed combo back to `spawn.combo` later, so the state remains owned by the spawn after initialization.

- [ ] **Step 3: Update setup-builder seed creation**

Update `makeInitializationUnitSeed()` in `BattleSceneSetupBuilder.cpp` after the field removal. This should mostly be deleting the trailing `{}` field from the aggregate initializer.

---

## Task 3: Remove Scene-Side Input Bridge

**Files:**
- Modify: `src/BattleSceneHades.cpp`

- [ ] **Step 1: Delete the pre-creation combo map assignment**

Remove:

```cpp
input.comboStates = KysChess::ChessCombo::getMutableStates();
```

The scene should no longer feed old global combo state into runtime creation.

- [ ] **Step 2: Keep the output bridge intentionally**

Keep this line for now:

```cpp
KysChess::ChessCombo::getMutableStates() = initialization.comboStates;
```

That is post-initialization reporting/UI state, not creation input. Removing it should be a separate plan if/when `ChessCombo::activeStates_` is no longer needed by scene/UI code.

---

## Task 4: Migrate Tests and Test Helpers

**Files:**
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify: `tests/BattleRuntimeScenarioUnitTests.cpp`
- Modify: `tests/BattleSceneUnitStoreUnitTests.cpp`
- Modify as needed: `tests/BattleSceneTestRuntimeFixture.h`

- [ ] **Step 1: Replace `.comboStates` setup**

For tests that currently do:

```cpp
input.comboStates.emplace(unitId, combo);
```

or:

```cpp
input.comboStates[unitId] = combo;
```

move that combo onto the matching setup unit:

```cpp
input.units[index].baseCombo = combo;
```

Prefer helper functions where the test already has `scenarioSetupUnit(...)` or a similar unit factory, so tests do not repeatedly scan by unit id.

- [ ] **Step 2: Replace `BattleInitializationUnitSeed::baseCombo` setup**

For direct initialization tests that currently assign:

```cpp
seed.baseCombo = combo;
```

put the combo on the corresponding `BattleRuntimeUnitSpawn` instead:

```cpp
auto spawn = makeRuntimeUnitSpawn(unit, combo);
```

or assign `spawn.combo` before calling `BattleInitializationSystem::initialize()`.

- [ ] **Step 3: Add one focused characterization test for the new ingress**

Add or adjust a test to prove a base combo on `BattleSetupUnitInput` reaches runtime after initialization.

Suggested assertion:

1. Create a `BattleRuntimeSessionCreationInput` with one unit.
2. Put a simple always-on combo effect on `input.units[0].baseCombo`.
3. Run `BattleRuntimeSession::createInitialized()`.
4. Assert `creation.session.runtime().units.require(0).combo` contains that effect or its expected derived runtime state.

Use an effect that does not depend on roster combo definitions, so the test specifically covers the base-combo ingress.

---

## Task 5: Verify and Clean Includes

**Files:**
- All modified files

- [ ] **Step 1: Remove stale includes**

After removing the side map, check for now-unused includes, especially `<map>` in `BattleRuntimeSession.h` and any test files that only needed map because of `.comboStates`.

- [ ] **Step 2: Search for deleted API usage**

Run a code search and ensure these no longer appear as creation inputs:

```text
.comboStates
BattleRuntimeSessionCreationInput::comboStates
applyCreationComboStatesToSetup
BattleInitializationUnitSeed::baseCombo
```

`BattleInitializationResult::comboStates` and `result.comboStates = comboMapFromSpawns(spawns)` may remain.

- [ ] **Step 3: Build and run focused tests**

Run the debug test build and focused tests.

If using CLI:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][initialization]"
.\x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
.\x64\Debug\kys_tests.exe "[battle][frame-runner]"
```

If integrated with VS Code, use the debug build and VS Code test adapter per project instructions.

Final game link failure is acceptable if the game executable is running; `kys_tests` should still build/run.

---

## Deferred Follow-Up Items

These were found during the same review but are intentionally not part of this plan:

- Consolidate duplicated `BattleCastInput` construction in `BattleCore.cpp`.
- Remove mirrored action timing/config fields from `BattleRuntimeActions`.
- Move combo trigger timer ownership/key helpers into one `BattleComboTriggerSystem` API.
- Factor repeated triggered-effect scanning loops.
- Centralize duplicated `triggeredEffect(...)` test helpers.
- Revisit old scene/render fallback duplication such as fight-style fallback and `BattleMap::copyLayerData` copying.

Keep this plan focused on the runtime combo-state creation bridge so the PR has one clear ownership story.
