# Battle Movement Agent Lifetime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep one stable `BattleMovementAgentState` row per runtime unit and replace dead-unit movement-agent erasure with an explicit active flag.

**Architecture:** `BattleRuntimeUnit` membership is already stable after clone initialization; movement should follow that model. `BattleMovementState::agents` remains a horizontal map for now, but every runtime unit has a row for the unit lifetime. Death cleanup flips `BattleMovementAgentState::active` off when a dead unit no longer needs corpse physics, while reservations remain transient and may still be erased.

**Tech Stack:** C++20/C++23, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Direction

This plan addresses the movement-specific blocker called out in `2026-05-25-battle-runtime-owner-types.md`: `unit.movement()` is currently phase-sensitive because dead-unit cleanup can erase the movement agent. After this plan:

- `state.movement.agents` has one row for every `state.unitStore.units` entry.
- Dead units do not lose their movement row.
- Dead units stop participating in movement planning and physics when `agent.active == false`.
- Dead units with corpse physics keep `agent.active == true` until `needsCorpsePhysics(unit)` becomes false.
- `state.movement.movementReservations` remains transient and continues to remove dead-unit reservations.
- `BattleRuntimeUnitHandle::movement()` can require a movement row instead of tolerating absence.

Do not introduce a broad `BattleRuntimeMovement` owner in this plan. The goal is lifetime stability first. A richer movement owner or movement subview can follow after the row invariant is real.

## File Structure

- Modify `src/battle/BattleTypes.h`
  - Add `active` to `BattleMovementAgentState`.

- Modify `src/battle/BattleRuntimeUnitSpawn.cpp`
  - Initialize the agent row for every spawn and set the starting active flag from `unit.alive`.

- Modify `src/battle/BattleCore.cpp`
  - Stop erasing movement agents in `prepareMovementAgents`.
  - Use `agent.active` to decide whether a dead unit participates in corpse physics.
  - Require movement rows in `BattleRuntimeUnits::makeHandle`.
  - Remove optional movement-agent lookup patterns that were only needed because rows could be erased.

- Modify `src/battle/BattleRuntimeUnits.h`
  - Update the handle invariant comment.
  - Keep `movement()` as a required row accessor.

- Modify tests:
  - `tests/BattleCoreUnitTests.cpp`
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - `tests/BattleRuntimeScenarioTestHelpers.h`
  - `tests/BattleInitializationUnitTests.cpp`

## Task 1: Add Failing Lifetime Regressions

**Files:**
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Replace the inert-dead-unit erasure expectation**

In `tests/BattleCoreUnitTests.cpp`, replace the current test named:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_RemovesInertDeadUnitsFromMovementAgents", "[battle][core][movement]")
```

with this test:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents", "[battle][core][movement][ownership]")
{
    BattleRuntimeState state;
    state.unitStore.gridTransform = { SceneTileWidth, 64 };
    auto dead = unit(1, 1, { 200, 100, 0 });
    dead.alive = false;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        dead,
    }));
    state.attacks = attackWorld();
    state.unitStore.requireUnit(0).motion.velocity = { 5, 0, 0 };
    state.movementPhysics.config.gravity = 0.0f;
    state.movementPhysics.config.friction = 0.0f;
    state.movementPhysics.config.postDashSpreadFrames = 6;
    state.movementPhysics.terrain.tileWidth = 100.0;
    state.movementPhysics.terrain.coordCount = 2;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth;
    state.movementPhysics.terrain.walkableByCell.assign(2 * 2, 1);

    runBattleFrame(state);

    REQUIRE(state.movement.agents.contains(1));
    CHECK_FALSE(state.movement.agents.at(1).active);
    CHECK(state.unitStore.requireUnit(1).motion.position.x == Catch::Approx(200.0f));
}
```

- [ ] **Step 2: Strengthen the corpse physics test**

In `tests/BattleCoreUnitTests.cpp`, in `BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics`, keep the existing `contains(1)` assertion and add:

```cpp
CHECK(state.movement.agents.at(1).active);
```

right after:

```cpp
CHECK(state.movement.agents.contains(1));
```

- [ ] **Step 3: Add a runtime-session dead-unit row regression**

In `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, add this test near `BattleRuntimeSession_CreateInitializedBuildsOwnedRuntimeStores`:

```cpp
TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsMovementAgentRowsForDeadUnits", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);

    BattleSetupUnitInput live;
    live.unitId = 0;
    live.realRoleId = 1000;
    live.name = "測試";
    live.team = 0;
    live.alive = true;
    live.vitals = { 100, 100, 0, 100 };
    live.stats = { 10, 10, 10 };
    live.motion.position = { 128, 256, 0 };
    input.units.push_back(live);
    input.comboStates.emplace(0, KysChess::RoleComboState{});

    BattleSetupUnitInput dead = live;
    dead.unitId = 1;
    dead.realRoleId = 1001;
    dead.name = "死亡測試";
    dead.team = 1;
    dead.alive = false;
    dead.vitals.hp = 0;
    dead.motion.position = { 256, 256, 0 };
    input.units.push_back(dead);
    input.comboStates.emplace(1, KysChess::RoleComboState{});

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    REQUIRE(session.runtime().movement.agents.size() == 2);
    CHECK(session.runtime().movement.agents.contains(0));
    CHECK(session.runtime().movement.agents.contains(1));
    CHECK(session.runtime().movement.agents.at(0).active);
    CHECK_FALSE(session.runtime().movement.agents.at(1).active);
}
```

- [ ] **Step 4: Run the focused tests and verify failure**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents"
.\x64\Debug\kys_tests.exe "BattleRuntimeSession_CreateInitializedBuildsMovementAgentRowsForDeadUnits"
```

Expected before implementation:

- Build fails because `BattleMovementAgentState::active` does not exist, or
- The tests fail because inert dead agents are still erased.

- [ ] **Step 5: Commit the failing tests**

```powershell
git add tests/BattleCoreUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp
git commit -m "test: define stable movement agent lifetime"
```

## Task 2: Add Movement Agent Active State

**Files:**
- Modify: `src/battle/BattleTypes.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`

- [ ] **Step 1: Add the active flag**

In `src/battle/BattleTypes.h`, change `BattleMovementAgentState` to:

```cpp
struct BattleMovementAgentState
{
    bool active = true;
    int targetId = -1;
    int assignedSlot = 0;
    int slotSwitchCooldownRemaining = 0;
    BattleMovementPhysicsState physics;
};
```

The default is `true` because most tests and runtime setup construct live units. Explicit spawn initialization will set the correct value from `BattleRuntimeUnit::alive`.

- [ ] **Step 2: Initialize active from the runtime unit**

In `src/battle/BattleRuntimeUnitSpawn.cpp`, replace `makeInitialMovementAgent(...)` with:

```cpp
BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit)
{
    BattleMovementAgentState agent;
    agent.active = unit.alive;
    agent.physics.position = unit.motion.position;
    agent.physics.velocity = unit.motion.velocity;
    agent.physics.acceleration = unit.motion.acceleration;
    return agent;
}
```

- [ ] **Step 3: Run the focused tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleRuntimeSession_CreateInitializedBuildsMovementAgentRowsForDeadUnits"
```

Expected after this task:

- The runtime-session initialization test passes.
- The inert-dead frame test still fails because `prepareMovementAgents` still erases the row.

- [ ] **Step 4: Commit**

```powershell
git add src/battle/BattleTypes.h src/battle/BattleRuntimeUnitSpawn.cpp
git commit -m "refactor: add active movement agent state"
```

## Task 3: Stop Erasing Movement Agents On Death

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Replace `prepareMovementAgents`**

In `src/battle/BattleCore.cpp`, replace the whole `prepareMovementAgents(...)` function with:

```cpp
void prepareMovementAgents(BattleRuntimeState& state)
{
    assert(state.movement.agents.size() == state.unitStore.units.size());
    for (const auto& unit : state.unitStore.units)
    {
        auto& agent = requireMappedById(state.movement.agents, unit.id);
        agent.active = unit.alive || needsCorpsePhysics(unit);
        if (!unit.alive)
        {
            state.movement.movementReservations.erase(unit.id);
        }
    }

    for (auto it = state.movement.movementReservations.begin(); it != state.movement.movementReservations.end();)
    {
        const auto& unit = state.unitStore.requireUnit(it->first);
        if (!unit.alive)
        {
            it = state.movement.movementReservations.erase(it);
            continue;
        }
        ++it;
    }
}
```

This keeps reservation cleanup as transient frame state, but makes movement-agent rows persistent.

- [ ] **Step 2: Update movement physics participation**

In `computeMovementPhysics(...)`, replace:

```cpp
if (!unit.alive && !needsCorpsePhysics(unit))
{
    continue;
}
auto& physics = requireMappedById(state.movement.agents, unit.id).physics;
```

with:

```cpp
auto& agent = requireMappedById(state.movement.agents, unit.id);
if (!agent.active)
{
    continue;
}
auto& physics = agent.physics;
```

- [ ] **Step 3: Run focused movement tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_InactivatesInertDeadMovementAgents"
.\x64\Debug\kys_tests.exe "BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics"
```

Expected: both tests pass.

- [ ] **Step 4: Run movement grep**

Run:

```powershell
rg -n "movement\.agents\.erase|if \(!unit\.alive && !needsCorpsePhysics|movement\.agents\.find" src\battle tests
```

Expected:

- No `movement.agents.erase` matches.
- No old dead-unit physics predicate remains in `computeMovementPhysics`.
- `movement.agents.find` remains only before Task 4 removes `actionMovementDashActive`'s optional lookup.

- [ ] **Step 5: Commit**

```powershell
git add src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp
git commit -m "refactor: keep dead movement agent rows stable"
```

## Task 4: Make Unit Movement Access Required

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleRuntimeUnits.h`

- [ ] **Step 1: Require movement rows in handle construction**

In `src/battle/BattleCore.cpp`, replace the movement lookup in `BattleRuntimeUnits::makeHandle(...)`:

```cpp
auto movementIt = runtime.movement.agents.find(unitId);
requireRescueRuntime(runtime, unitId);
return {
    core,
    &requireById(runtime.status.units, unitId),
    &requireMappedById(runtime.combo.units, unitId),
    &requireById(runtime.damage.unitExtras, unitId),
    movementIt == runtime.movement.agents.end() ? nullptr : &movementIt->second,
    &requireById(runtime.deathEffects.store.units, unitId),
    &runtime.action,
    &runtime,
};
```

with:

```cpp
auto& movement = requireMappedById(runtime.movement.agents, unitId);
requireRescueRuntime(runtime, unitId);
return {
    core,
    &requireById(runtime.status.units, unitId),
    &requireMappedById(runtime.combo.units, unitId),
    &requireById(runtime.damage.unitExtras, unitId),
    &movement,
    &requireById(runtime.deathEffects.store.units, unitId),
    &runtime.action,
    &runtime,
};
```

- [ ] **Step 2: Update the handle invariant comment**

In `src/battle/BattleRuntimeUnits.h`, replace:

```cpp
// Runtime unit membership is fixed after clone initialization. Handles and unit
// ranges are frame-local views over stable unit/status/damage/combo/death rows.
// Movement agent access is phase-sensitive because dead-unit cleanup may erase
// movement agents for non-corpse units.
```

with:

```cpp
// Runtime unit membership is fixed after clone initialization. Handles and unit
// ranges are frame-local views over stable unit/status/damage/combo/death/rescue/
// movement rows. Movement rows remain present for dead units; agent.active tells
// movement phases whether the unit currently participates in planning/physics.
```

- [ ] **Step 3: Keep the invariant assert in `movement()`**

Leave `BattleRuntimeUnitHandle::movement()` as:

```cpp
BattleMovementAgentState& movement() const
{
    assert(movement_ != nullptr);
    return *movement_;
}
```

This is still useful for catching malformed handles, but runtime construction now makes the pointer required.

- [ ] **Step 4: Replace optional dash lookup**

In `src/battle/BattleCore.cpp`, replace `actionMovementDashActive(...)` with:

```cpp
bool actionMovementDashActive(const BattleRuntimeState& state, int unitId)
{
    const auto& movement = requireMappedById(state.movement.agents, unitId);
    return movement.active && movement.physics.movementDashFrames > 0;
}
```

- [ ] **Step 5: Run handle and battle tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session][ownership]"
.\x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: all pass.

- [ ] **Step 6: Commit**

```powershell
git add src/battle/BattleCore.cpp src/battle/BattleRuntimeUnits.h
git commit -m "refactor: require movement rows in runtime unit handles"
```

## Task 5: Update Test And Scenario Seeders

**Files:**
- Modify: `tests/BattleRuntimeScenarioTestHelpers.h`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Make scenario seeders create all movement rows**

In `tests/BattleRuntimeScenarioTestHelpers.h`, replace:

```cpp
if (unit.alive)
{
    BattleMovementAgentState agent;
    agent.physics.position = unit.motion.position;
    agent.physics.velocity = unit.motion.velocity;
    agent.physics.acceleration = unit.motion.acceleration;
    state.movement.agents.emplace(unit.id, agent);
}
```

with:

```cpp
BattleMovementAgentState agent;
agent.active = unit.alive;
agent.physics.position = unit.motion.position;
agent.physics.velocity = unit.motion.velocity;
agent.physics.acceleration = unit.motion.acceleration;
state.movement.agents.emplace(unit.id, agent);
```

- [ ] **Step 2: Make canonical runtime test seeding set active**

In `tests/BattleFrameRunnerRuntimeUnitTests.cpp`, inside `seedCanonicalUnitsFromMovementUnits(...)`, after:

```cpp
BattleMovementAgentState agent;
```

add:

```cpp
agent.active = worldUnit.alive;
```

- [ ] **Step 3: Make runtime-unit seeding preserve the spawn path**

Confirm `tests/BattleFrameRunnerRuntimeUnitTests.cpp::seedRuntimeUnits(...)` and `tests/BattleCoreUnitTests.cpp::seedRuntimeUnits(...)` continue to call:

```cpp
appendRuntimeUnit(state, std::move(spawn));
```

Do not hand-build movement rows there. The spawn path must remain authoritative.

- [ ] **Step 4: Run scenario/runtime tests**

Run:

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
.\x64\Debug\kys_tests.exe "[battle][runtime_session]"
.\x64\Debug\kys_tests.exe "[battle][scenario]"
```

Expected: both pass. If `[battle][scenario]` matches no tests in the current binary, run the full test binary in Task 7.

- [ ] **Step 5: Commit**

```powershell
git add tests/BattleRuntimeScenarioTestHelpers.h tests/BattleFrameRunnerRuntimeUnitTests.cpp
git commit -m "test: seed stable movement agent rows"
```

## Task 6: Remove Erasure Artifacts From Expectations

**Files:**
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Update any remaining dead-agent absence assertions**

Run:

```powershell
rg -n "CHECK_FALSE\\(.*movement\\.agents\\.contains|movement\\.agents\\.size\\(\\) ==|movement\\.agents\\.find" tests
```

For each test that encodes the old lifetime rule, change it to assert row presence and active state. The common replacement shape is:

```cpp
REQUIRE(state.movement.agents.contains(unitId));
CHECK_FALSE(state.movement.agents.at(unitId).active);
```

For live units, use:

```cpp
REQUIRE(state.movement.agents.contains(unitId));
CHECK(state.movement.agents.at(unitId).active);
```

- [ ] **Step 2: Preserve clone initialization assertions**

In `tests/BattleInitializationUnitTests.cpp`, keep the existing clone movement row checks and add:

```cpp
CHECK(cloneAgentIt->second.active == cloneUnit.alive);
```

near the existing clone agent assertions.

- [ ] **Step 3: Run ownership grep**

Run:

```powershell
rg -n "movement agent access is phase-sensitive|movement\.agents\.erase|movementIt ==|movement\.agents\.find" src\battle tests
```

Expected:

- No stale comment saying movement access is phase-sensitive because rows are erased.
- No `movement.agents.erase` in battle runtime code.
- No optional movement-agent lookup in `BattleRuntimeUnits::makeHandle`.
- Remaining `movement.agents.find` in tests is acceptable only when the test is explicitly verifying map membership.

- [ ] **Step 4: Commit**

```powershell
git add tests/BattleCoreUnitTests.cpp tests/BattleInitializationUnitTests.cpp
git commit -m "test: migrate movement agent lifetime expectations"
```

## Task 7: Full Verification And Ownership Scan

**Files:**
- No source edits expected unless verification finds a concrete issue.

- [ ] **Step 1: Run build**

```powershell
.\.github\build-command.ps1 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits with code 0. If final linking fails because the game executable is running, treat only the test target result as relevant.

- [ ] **Step 2: Run movement/core/runtime tests**

```powershell
.\x64\Debug\kys_tests.exe "[battle][core][movement]"
.\x64\Debug\kys_tests.exe "[battle][core][ownership]"
.\x64\Debug\kys_tests.exe "[battle][runtime_session]"
```

Expected: all selected tests pass.

- [ ] **Step 3: Run the full test binary**

```powershell
.\x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 4: Run final movement lifetime scans**

```powershell
rg -n "movement\.agents\.erase|movementIt ==|movement agent access is phase-sensitive" src\battle tests
rg -n "requireMappedById\\(state\\.movement\\.agents|state\\.movement\\.agents\\.find|movementReservations\\.erase" src\battle\BattleCore.cpp
```

Expected:

- First scan returns no matches.
- Second scan shows direct movement-agent access only in movement/action bridge code that still lacks a real owner type.
- `movementReservations.erase` remains in `prepareMovementAgents` and movement-system internals because reservations are transient, not per-unit lifetime rows.

- [ ] **Step 5: Commit final cleanup**

```powershell
git add src/battle tests
git commit -m "refactor: stabilize battle movement agent lifetime"
```

## Self-Review

- Spec coverage: The plan directly addresses the requested change: death flips `BattleMovementAgentState::active` instead of removing the agent row, preserving stable per-unit movement access.
- Migration completeness: The plan covers runtime spawn, clone/session initialization, frame cleanup, corpse physics, test seeders, and ownership comments.
- Boundary discipline: The plan does not introduce a shallow `BattleRuntimeMovement` wrapper. It creates the invariant needed before a real movement owner can expose meaningful verbs.
- Type consistency: The flag is consistently named `active` and lives on `BattleMovementAgentState`.
- Verification: The plan includes focused movement tests, ownership scans, and the full test binary.
