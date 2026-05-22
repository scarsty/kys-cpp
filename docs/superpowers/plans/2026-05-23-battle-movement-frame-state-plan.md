# Battle Movement Frame State Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Narrow movement frame state so movement planning and physics no longer publish implementation scratch through `BattleFrameResult`.

**Architecture:** `BattleRuntimeState` owns persistent unit motion, movement agents, reservations, and frame counters. `BattleFrameContext` may hold one-frame movement planner/physics scratch; `BattleFrameResult` should expose only scene/report contracts.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Scope

Run this after `2026-05-23-battle-frame-result-movement-physics-channel-plan.md` is complete. This plan removes the next movement internals from public result shape; it does not rewrite the movement planner algorithm in `BattleMovement.cpp`.

## Required Behavior Gates

These tests must stay green throughout this plan:

```text
BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition
BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore
BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics
BattleFrameRunner_AdvanceFrame_AppliesDeathKickVelocity
BattleFrameRunner_AdvanceFrame_MovingCorpsePhysicsPersistsIntoRuntime
```

---

## Task 1: Move `BattleTickResult movement` Out Of Public Result

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [x] **Step 1: Confirm remaining movement result consumers**

Run:

```powershell
rg -n "result\\.movement|frame\\.result\\.movement|\\.movement\\.decisions|\\.movement\\.events" src tests
```

Expected: `BattleCore.cpp` uses movement decisions for action/cast and presentation; tests directly assert movement events/decisions.

Result: direct `BattleFrameResult::movement` consumers were limited to `BattleCore.cpp` and two tests asserting movement events/decisions.

- [x] **Step 2: Rewrite public-result tests**

In `tests/BattleCoreUnitTests.cpp`, replace direct assertions on `result.movement.events` and `result.movement.decisions` with canonical runtime or presentation checks.

For `BattleFrameRunner_AdvanceFrame_CommitsMovementBeforeProjectileEvents`, keep the public ordering assertion on `result.frame.visualEvents` and remove direct movement-event assertions.

For `BattleFrameRunner_PostDashRetreatYieldsMovementPlannerToPhysics`, replace:

```cpp
REQUIRE(result.movement.decisions.contains(0));
CHECK(result.movement.decisions.at(0).action == MovementAction::Hold);
```

with:

```cpp
CHECK(state.movement.agents.at(0).physics.postDashRetreatFrames > 0);
CHECK(state.unitStore.requireUnit(0).motion.velocity.x == 0.0f);
```

- [x] **Step 3: Add private context storage**

In `src/battle/BattleCore.cpp`, add this member to `BattleFrameContext`:

```cpp
BattleTickResult movement;
```

- [x] **Step 4: Remove the public movement field**

In `src/battle/BattleCore.h`, remove this line from `BattleFrameResult`:

```cpp
BattleTickResult movement;
```

- [x] **Step 5: Route movement through context**

Change `commitFrameMovement` to write `frame.movement` instead of `result.movement`. Use this signature:

```cpp
void commitFrameMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    BattleTickResult movement,
    const BattleMovementFrameInput& movementInput)
```

Inside the helper:

```cpp
frame.movement = std::move(movement);
applyMovementFrameState(state, movementInput);
```

Replace later `result.movement` reads in this helper with `frame.movement`.

- [x] **Step 6: Update movement consumers**

Change `advanceActionFrameUnits` from:

```cpp
const auto& movement = frame.result.movement;
```

to:

```cpp
const auto& movement = frame.movement;
```

Change `publishMovementPresentationResults` to accept `const BattleTickResult& movement` and replace `result.movement` reads with that parameter.

- [x] **Step 7: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement],[battle][core]"
```

Expected: selected tests pass.

Result: `x64\Debug\kys_tests.exe "[battle][core][movement],[battle][core]"` passed 754 assertions in 102 test cases.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src/battle/BattleCore.h src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp docs/superpowers/plans/2026-05-23-battle-movement-frame-state-plan.md
git commit -m "refactor: keep movement tick result private to frame"
```

Expected: one commit that removes public `BattleFrameResult::movement`.

---

## Task 2: Name The Remaining Movement Mirror Boundary

**Files:**

- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Rename movement writeback helper**

Rename `applyMovementFrameState` to:

```cpp
commitMovementPlannerStateToRuntime
```

Keep the implementation unchanged in this task. The name should make the remaining mirror boundary searchable.

- [ ] **Step 2: Rename movement construction helper**

Rename `makeMovementFrameInput` to:

```cpp
makeMovementPlannerInputFromRuntime
```

Keep the implementation unchanged in this task.

- [ ] **Step 3: Verify the boundary names**

Run:

```powershell
rg -n "makeMovementFrameInput|applyMovementFrameState|makeMovementPlannerInputFromRuntime|commitMovementPlannerStateToRuntime" src/battle/BattleCore.cpp
```

Expected: only the new names remain.

- [ ] **Step 4: Run tests and commit**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][movement],[battle][core]"
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-23-battle-movement-frame-state-plan.md
git commit -m "refactor: name movement planner runtime boundary"
```

Expected: selected tests pass, then one naming-only commit.

---

## Completion Gate

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, all Catch2 tests pass, and MSBuild exits 0.
