# Battle Frame Result Movement Physics Channel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove `BattleFrameResult::movementPhysicsResults` from the public frame result while preserving movement physics behavior and test coverage.

**Architecture:** Keep movement physics output as private one-frame `BattleFrameContext` scratch. `BattleRuntimeState` remains the persistent owner of unit motion and movement agent physics; `BattleFrameResult` keeps only externally meaningful frame output.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Scope

This is phase 2C's first field-removal slice. It does not change movement physics rules, movement planner rules, scene presentation, or runtime ownership. It only moves `movementPhysicsResults` out of `BattleFrameResult`.

## Files And Responsibilities

| File | Responsibility |
| --- | --- |
| `src/battle/BattleCore.h` | Remove the public `movementPhysicsResults` field from `BattleFrameResult`. |
| `src/battle/BattleCore.cpp` | Store movement physics results in `BattleFrameContext` and pass them explicitly to movement commit/presentation helpers. |
| `tests/BattleCoreUnitTests.cpp` | Rewrite frame-runner movement tests so they assert canonical runtime state and presentation output, not the removed public field. |
| `docs/superpowers/plans/2026-05-23-battle-frame-result-movement-physics-channel-plan.md` | Track this implementation. |

---

## Task 0: Baseline And Consumer Check

**Files:**

- Read: `src/battle/BattleCore.h`
- Read: `src/battle/BattleCore.cpp`
- Read: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Confirm direct consumers**

Run:

```powershell
rg -n "movementPhysicsResults" src tests
```

Expected: matches only in `BattleCore.h`, `BattleCore.cpp`, and `tests/BattleCoreUnitTests.cpp`.

Result: matches were limited to `src/battle/BattleCore.h`, `src/battle/BattleCore.cpp`, and `tests/BattleCoreUnitTests.cpp`.

- [x] **Step 2: Run the focused baseline**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: all selected movement tests pass before editing.

Result: `x64\Debug\kys_tests.exe "[battle][core][movement]"` passed 64 assertions in 7 test cases.

---

## Task 1: Rewrite Tests Away From The Public Physics Field

**Files:**

- Modify: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Update `BattleFrameRunner_RoutesMovementPhysicsThroughCanonicalUnitStore`**

Remove assertions that compare `result.movementPresentationResults` to `result.movementPhysicsResults`. Keep assertions against `result.movementPresentationResults`, `state.unitStore`, and `state.movement.agents`.

The retained checks should include this shape:

```cpp
REQUIRE(result.movementPresentationResults.size() == 1);
CHECK(result.movementPresentationResults[0].unitId == 0);
CHECK(result.movementPresentationResults[0].position.x == 105.0f);
CHECK(result.movementPresentationResults[0].velocity.x == 5.0f);
CHECK(result.movementPresentationResults[0].acceleration.x == 0.0f);
CHECK(result.movementPresentationResults[0].frozenFrames == 0);
const auto& unit = state.unitStore.requireUnit(0);
CHECK(unit.motion.position.x == 105.0f);
CHECK(unit.motion.velocity.x == 5.0f);
CHECK(state.movement.agents.at(0).physics.movementDashFrames == 0);
CHECK(state.movement.agents.at(0).physics.movementDashSpreadFrames == 6);
```

- [x] **Step 2: Update `BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore`**

Replace `moved` and `stopped` physics-result assertions with runtime store and agent assertions:

```cpp
const auto& movedUnit = state.unitStore.requireUnit(0);
CHECK(movedUnit.motion.position.x == 105.0f);
CHECK(state.movement.agents.at(0).physics.movementDashFrames == 0);
CHECK(state.movement.agents.at(0).physics.movementDashSpreadFrames == 6);

const auto& stoppedUnit = state.unitStore.requireUnit(1);
CHECK(state.status.units[1].effects.frozenTimer == 1);
CHECK(stoppedUnit.motion.position.x == 200.0f);
CHECK(stoppedUnit.motion.velocity.x == 0.0f);
```

- [x] **Step 3: Update corpse movement physics tests**

For `BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics`, assert the dead unit's canonical motion and agent persistence:

```cpp
const auto& corpse = state.unitStore.requireUnit(1);
CHECK(corpse.motion.position.x == 206.0f);
CHECK(corpse.motion.position.z == 8.0f);
CHECK(state.movement.agents.contains(1));
CHECK(state.movement.agents.at(1).physics.position.x == 206.0f);
```

For `BattleFrameRunner_AdvanceFrame_RemovesInertDeadUnitsFromMovementAgents`, keep:

```cpp
CHECK_FALSE(state.movement.agents.contains(1));
```

For `BattleFrameRunner_AdvanceFrame_MovingCorpsePhysicsPersistsIntoRuntime`, replace `first.movementPhysicsResults` and `second.movementPhysicsResults` lookup checks with `state.unitStore.requireUnit(1).motion` checks after each frame.

- [x] **Step 4: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement]"
```

Expected: movement tests pass before production code removal. This proves the tests no longer depend on the public physics field.

Result: `x64\Debug\kys_tests.exe "[battle][core][movement]"` passed 64 assertions in 7 test cases after removing public-field assertions.

- [x] **Step 5: Commit test migration**

Run:

```powershell
git add tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-frame-result-movement-physics-channel-plan.md
git commit -m "test: stop asserting public movement physics results"
```

Expected: one commit with test-only changes plus plan progress.

Result: committed as `803b4ac test: stop asserting public movement physics results`.

---

## Task 2: Move Movement Physics Results Into Frame Context

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Add private context storage**

In `src/battle/BattleCore.cpp`, add this member to `BattleFrameContext`:

```cpp
std::vector<BattleFrameMovementPhysicsUnitResult> movementPhysicsResults;
```

- [x] **Step 2: Remove the public result field**

In `src/battle/BattleCore.h`, remove this line from `BattleFrameResult`:

```cpp
std::vector<BattleFrameMovementPhysicsUnitResult> movementPhysicsResults;
```

- [x] **Step 3: Change movement commit helper to consume private physics results**

Change `commitFrameMovement` so it takes physics results by const reference and no longer assigns to `result.movementPhysicsResults`:

```cpp
void commitFrameMovement(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults,
    BattleTickResult movement,
    const BattleMovementFrameInput& movementInput)
{
    result.movement = std::move(movement);
    applyMovementFrameState(state, movementInput);

    for (const auto& physicsResult : physicsResults)
    {
        auto& unit = state.unitStore.requireUnit(physicsResult.unitId);
        auto& physics = requireMappedById(state.movement.agents, physicsResult.unitId).physics;

        unit.motion.position = physicsResult.state.position;
        unit.motion.velocity = physicsResult.state.velocity;
        unit.motion.acceleration = physicsResult.state.acceleration;
        physics = physicsResult.state;
        if (auto* status = tryFindById(state.status.units, physicsResult.unitId))
        {
            status->effects.frozenTimer = physicsResult.frozenFrames;
        }
    }

    // Keep the existing movement-unit synchronization block below this point,
    // replacing references to result.movementPhysicsResults with physicsResults.
}
```

- [x] **Step 4: Pass private physics results to presentation publishing**

Change `publishMovementPresentationResults` to accept the physics vector explicitly:

```cpp
void publishMovementPresentationResults(
    const BattleRuntimeState& state,
    BattleFrameResult& result,
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults)
```

Inside the helper, replace every `result.movementPhysicsResults` read with `physicsResults`.

- [x] **Step 5: Route `advanceMotionFrame` through context**

Change `advanceMotionFrame` to take `BattleFrameContext&` and write the private vector:

```cpp
void advanceMotionFrame(BattleRuntimeState& state, BattleFrameContext& frame)
{
    prepareMovementAgents(state);
    frame.movementPhysicsResults = computeMovementPhysics(state);
    auto movementInput = makeMovementFrameInput(state, makePostPhysicsMotionMap(frame.movementPhysicsResults));
    auto movement = BattleCore(movementInput).tickMovement();
    commitFrameMovement(state, frame.result, frame.movementPhysicsResults, std::move(movement), movementInput);
}
```

Update `BattleFrameRunner::runFrame()` call sites:

```cpp
advanceMotionFrame(state, frame);
publishMovementPresentationResults(state, frame.result, frame.movementPhysicsResults);
```

- [x] **Step 6: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement],[battle][core],[battle][frame_runner]"
```

Expected: selected tests pass.

Result: `x64\Debug\kys_tests.exe "[battle][core][movement],[battle][core],[battle][frame_runner]"` passed 861 assertions in 116 test cases.

- [x] **Step 7: Commit field removal**

Run:

```powershell
git add src/battle/BattleCore.h src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-frame-result-movement-physics-channel-plan.md
git commit -m "refactor: keep movement physics results private to frame"
```

Expected: one commit that removes the public field and keeps behavior unchanged.

Result: committed as `60676b7 refactor: keep movement physics results private to frame`.

---

## Task 3: Completion Verification

- [x] **Step 1: Confirm the field is gone from public result**

Run:

```powershell
rg -n "movementPhysicsResults" src tests
```

Expected: no matches in `src/battle/BattleCore.h` or tests. Matches in `src/battle/BattleCore.cpp` are allowed only for `BattleFrameContext` and movement helper internals.

Result: matches remain only in `src/battle/BattleCore.cpp` for `BattleFrameContext` and movement helper internals.

- [x] **Step 2: Run completion gate**

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, all Catch2 tests pass, and MSBuild exits 0.

Result: `git diff --check` exited 0. Full `x64\Debug\kys_tests.exe` passed 5529 assertions in 419 test cases. `.github\build-command.ps1` exited 0 and produced `x64\Debug\kys.exe`.
