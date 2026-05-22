# Battle Runtime Phase 3 Mirror Sync Breakdown Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete remaining persistent mirror-world synchronization pairs without hiding battle frame ordering or introducing new bridge layers.

**Architecture:** Keep `BattleFrameRunner::runFrame()` as the visible transaction owner. A subsystem may receive pure rule input, return a value/result, or mutate `BattleRuntimeState` only through an explicit frame-runner-owned boundary. One-frame scratch is allowed only when it cannot outlive `runFrame()`.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Decision

Do not execute all of phase 3 in one implementation pass.

The current inventory still has multiple risk classes:

- hidden runtime mutation inside a subsystem
- movement planner world input/output synchronization
- action state copied into a temporary result and written back
- damage/status value conversions used by both setup and frame-time code
- scene presentation synchronization that is not gameplay mutation

The first executable slice should be damage application hidden writeback because it is the clearest ownership violation: `BattleDamageApplicationSystem::apply()` mutates runtime stores through pointers while `BattleFrameRunner` also applies each returned transaction for frame-order side effects such as death kick, rescue, and battle-end publication.

---

## Inventory Command

The high-level plan's glob command is shell-dependent. In PowerShell, use this equivalent:

```powershell
$paths = rg --files src | Where-Object {
    $_ -match '^src[\\/]battle[\\/]'
    -or $_ -match '^src[\\/]BattleScene.*\.(cpp|h)$'
}
rg -n 'make.*World|write.*World|make.*State|write.*State|make.*Input|apply.*Snapshot|apply.*Frame' @paths
```

Current result: 123 matches across these files:

```text
src\battle\BattleCore.cpp
src\battle\BattleCore.h
src\battle\BattleDamageApplicationSystem.cpp
src\battle\BattleDamageSystem.cpp
src\battle\BattleDamageSystem.h
src\battle\BattleHitResolver.cpp
src\battle\BattleInitialization.cpp
src\battle\BattleRuntimeSession.cpp
src\battle\BattleStatusSystem.cpp
src\battle\BattleStatusSystem.h
src\battle\BattleUnitStore.h
src\BattleSceneHades.cpp
src\BattleSceneHades.h
src\BattleSceneUnitStore.cpp
```

---

## Classification

| Match group | Classification | Phase 3 action |
| --- | --- | --- |
| `BattleSceneHades::makeBattleRuntimeSessionCreationInput`, `BattleRuntimeSession::createInitialized`, `makeInitialDamageRuntimeUnit`, `makeBattleStatusUnitState(runtimeUnit, combo)` | Setup-time conversion | Keep narrow. Not active during frame execution. |
| `BattleSceneUnitStore::makeInitializedScenePresentationState` | Scene presentation setup | Keep for scene ownership; not gameplay sync. |
| `BattleSceneHades::applyCoreFrameResult`, `applySceneFrameDelta`, `applyLegacyBattleFrameResult` | Scene presentation/report synchronization | Keep for phase 3; simplify in phase 5 after runtime result channels shrink. |
| `makeRuntimeUnitTickInput`, `makeRuntimeCastSkillState`, `makeCommittedCastActionInput`, `makeRuntimeBlinkGeometry`, `makeRescueInput`, `makeMovementPhysicsCollisionWorld` | Pure calculation input or one-frame scratch | Keep unless a narrower direct mutator becomes obvious inside a domain slice. |
| `BattleDamageSystem` local helpers: `makeBattleUnitDelta`, `makeBattleResourceUnit`, `writeBattleResourceUnit`, `makeBattleDamageModifierState` | Pure rule/value helpers | Keep. They do not write persistent runtime stores. |
| `BattleHitResolver` local helpers: `makeDamageUnit`, `makeResourceUnit`, `makeCooldownState` | Pure rule/value helpers | Keep. They are resolver-internal snapshots. |
| `BattleDamageApplicationSystem::applyResolvedTransactionToLiveState` plus `makeDamageTransactionInputFromIntent` | Persistent hidden runtime mutation | First deletion slice. The application system should return transactions/events; the frame runner should apply runtime mutations in order. |
| `makeMovementPlannerInputFromRuntime`, `commitMovementPlannerStateToRuntime`, `makeBattleWorldUnitState` | Persistent movement planner mirror | Dedicated movement slice. Preserve movement planner behavior while replacing world writeback with explicit output deltas or direct runtime commits. |
| `makeActionRuntimeState`, `BattleFrameActionUnitResult::state`, `commitActionFrameStateToRuntime` | Persistent action state mirror | Dedicated action slice. Update runtime action fields directly or return a narrow action state delta. |
| `makeBattleDamageUnitStateFromRuntime`, `writeBattleDamageRuntimeUnitImpl`, public `makeBattleDamageUnitState`, public `writeBattleDamageRuntimeUnit` | Damage value conversion/writeback | Clean after the damage application hidden writeback slice; split setup/test helpers from frame-time runtime mutation. |
| `makeBattleStatusUnitState`, `writeBattleStatusRuntimeUnit` | Status value conversion/writeback | Clean with damage/status ownership once damage application no longer writes runtime stores internally. |
| `makeHitResolutionInput`, `updateCommittedHitCombos` | Hit one-frame snapshot plus explicit combo commit | Keep for now; the combo commit is explicit in `BattleCore.cpp`, but input shape can be narrowed after damage/action cleanup. |
| `commitRescueResultToRuntime` | Direct runtime commit from frame runner | Keep for now. It is named and frame-runner-owned; remove only if rescue events become direct runtime mutations without a result object. |

---

## Implementation Order

### Slice 3A: Remove Hidden Damage Application Writeback

**Files:**

- Modify: `src/battle/BattleDamageApplicationSystem.h`
- Modify: `src/battle/BattleDamageApplicationSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

**Intent:** `BattleDamageApplicationSystem::apply()` should no longer mutate `BattleUnitStore`, `BattleDamageRuntimeUnit`, or `BattleStatusRuntimeUnit` through input pointers. It should compute transactions/events from local one-frame working state. `BattleFrameRunner::applyDamageAndLifecycle()` remains the ordered runtime mutation point.

**Required red test:**

Add a unit test in `tests/BattleDamageApplicationSystemUnitTests.cpp` that builds an input with pending lethal or damaging intent, calls `BattleDamageApplicationSystem().apply(input)`, and asserts the input stores are unchanged after the call:

```cpp
CHECK(input.units->requireUnit(defenderId).vitals.hp == originalHp);
CHECK(requireById(*input.damageUnitExtras, defenderId).deathPreventionUsed == originalDeathPreventionUsed);
CHECK(requireById(*input.statusUnits, defenderId).effects.frozenTimer == originalFrozenTimer);
```

Expected before production changes: FAIL because `applyResolvedTransactionToLiveState()` currently writes into the input stores.

**Implementation direction:**

- Replace `applyResolvedTransactionToLiveState()` with local working-state updates inside `BattleDamageApplicationSystem.cpp`.
- Keep sequential damage semantics by resolving each pending damage against local copies of unit, damage-extra, and status state.
- Return the same ordered `BattleDamageTransactionResult` vector.
- Let `BattleCore.cpp::applyDamageAndLifecycle()` remain responsible for committing each transaction to `BattleRuntimeState`.

**Verification:**

```powershell
x64\Debug\kys_tests.exe "BattleDamageApplicationSystem_*"
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][frame_runner][runtime][unit]"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

### Slice 3B: Narrow Damage/Status Conversion APIs

**Depends on:** Slice 3A.

**Intent:** Separate setup/test value conversion from frame-time runtime mutation. Public `makeBattleDamageUnitState()` and `writeBattleDamageRuntimeUnit()` should either become setup/test helpers with clear names or move private if only frame internals use them.

**Verification focus:**

- death prevention
- poison/bleed transactions
- damage-taken MP gain
- impact freeze
- battle end once

### Slice 3C: Replace Movement Planner World Writeback

**Depends on:** Slice 3A only for reduced risk; no direct code dependency.

**Intent:** Stop using `BattleMovementFrameInput` as both planner input and output. Preserve `BattleMovement` as a rule/planner module, but return explicit movement decisions and planner state deltas instead of writing runtime-shaped movement unit fields back through `commitMovementPlannerStateToRuntime()`.

**Verification focus:**

- post-movement cast origin
- moving corpse physics
- death kick presentation
- frozen physics timer
- movement reservation cleanup

### Slice 3D: Remove Action State Result Writeback

**Depends on:** Slice 3C if movement decisions are still read by action commit input.

**Intent:** Remove `BattleFrameActionUnitResult::state` as the state writeback carrier. Either mutate `BattleRuntimeUnit` action fields directly inside `advanceActionFrameUnits()` or use a narrow action state delta that cannot be confused with authoritative state.

**Verification focus:**

- cast start without attack spawn
- pending cast refresh
- target death cancel/retarget
- MP gain/spend
- combo post-skill effects

### Slice 3E: Re-check Scene Synchronization

**Depends on:** Slices 3A-3D.

**Intent:** Confirm remaining `apply*Frame` scene calls are presentation/report only. Do not delete scene helpers here unless the same change removes an obsolete bridge.

**Verification focus:**

- `BattleSceneFrameDeltaBuilder` unit tests
- runtime-session death combo scene consumption
- report/log playback for projectile cancel damage

---

## Ready Criteria For Slice 3A

Slice 3A is ready when:

- working tree is clean
- the red test proves `BattleDamageApplicationSystem::apply()` currently mutates input stores
- the implementation keeps same-frame multi-hit accumulation behavior
- `BattleFrameRunner::applyDamageAndLifecycle()` remains the only damage runtime commit point after application result creation

---

## Completion Gate For Each Slice

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected:

- `git diff --check` exits 0
- all Catch2 tests pass
- MSBuild exits 0, except final linking may be treated as successful only if the executable is locked by a running game process
