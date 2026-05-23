# Battle Runtime Phase 3 Reducer Breakdown Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse remaining frame-time mirror worlds into visible `BattleFrameRunner` runtime reducers.

**Architecture:** Keep pure calculation kernels small and value-based. Keep frame-time gameplay mutation in frame-runner-owned reducers that mutate the authoritative `BattleRuntimeState&` in visible order. Do not replace hidden writeback with subsystem-owned copies of `BattleRuntimeState`, `BattleUnitStore`, runtime unit vectors, or state-shaped output that must be synchronized later.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Correction

The previous 3A direction removed hidden mutation from `BattleDamageApplicationSystem::apply()` by adding a one-frame working copy of `BattleUnitStore`, `BattleDamageRuntimeUnit`, and `BattleStatusRuntimeUnit`.

That direction is rejected.

It improved the call boundary but preserved the core problem: a subsystem-owned copy of authoritative runtime state, followed by state-shaped results that must be interpreted back into runtime. It also increases copy cost and makes missing-field bugs likely on both input and output.

The corrected rule is:

```text
pure calculation kernels below
frame-runner-owned runtime reducers above
no broad frame-time mirror state in the middle
```

Events are not automatically clean. A returned value is acceptable only when it is one of these:

- a pure calculation result that is committed immediately by the calling reducer
- a presentation/log/report fact after runtime has already been mutated
- a narrow command that the frame runner reduces immediately in the same visible frame order

A returned value is not acceptable when it is a broad unit/status/world snapshot whose purpose is to synchronize authoritative runtime later.

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

Current result from the completed inventory: 123 matches across these files:

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
| `BattleDamageSystem` local helpers: `makeBattleUnitDelta`, `makeBattleResourceUnit`, `writeBattleResourceUnit`, `makeBattleDamageModifierState` | Pure rule/value helpers | Keep. They operate inside a single damage calculation and do not own runtime stores. |
| `BattleHitResolver` local helpers: `makeDamageUnit`, `makeResourceUnit`, `makeCooldownState` | Pure rule/value helpers | Keep for now, but avoid expanding them into full runtime mirrors. |
| `BattleDamageApplicationSystem::applyResolvedTransactionToLiveState`, `makeFrameDamageApplicationInput`, `BattleDamageApplicationInput`, `BattleDamageApplicationResult` | Frame-time damage mirror/reducer split | Collapse into `BattleCore.cpp::applyDamageAndLifecycle()`. The frame runner should build narrow damage calculation input from current runtime, call `BattleDamageSystem`, commit immediately, then emit presentation/log facts. |
| `pendingDefenderHpDamage` plus `BattleDamageApplicationSystem::previewDefenderPendingHpDamage` | Frame-time preview mirror | Remove the full-store preview copy. Either move execute prediction into the damage reducer where pending damage order is authoritative, or make a narrow projection that only carries fields needed for HP projection. |
| `makeMovementPlannerInputFromRuntime`, `commitMovementPlannerStateToRuntime`, `makeBattleWorldUnitState` | Persistent movement planner mirror | Dedicated movement reducer slice. Preserve movement decisions, but stop using `BattleMovementFrameInput` as both a copied world and writeback carrier. |
| `makeActionRuntimeState`, `BattleFrameActionUnitResult::state`, `commitActionFrameStateToRuntime` | Persistent action state mirror | Dedicated action reducer slice. Mutate runtime action fields directly in `advanceActionFrameUnits()` after using any local stack state needed for calculation. |
| `makeBattleDamageUnitStateFromRuntime`, `writeBattleDamageRuntimeUnitImpl`, public `makeBattleDamageUnitState`, public `writeBattleDamageRuntimeUnit` | Damage value conversion/writeback | Keep only where used to build a single `BattleDamageTransactionInput` or commit immediately in the frame reducer. Remove public frame-time writeback usage after 3A. |
| `makeBattleStatusUnitState`, `writeBattleStatusRuntimeUnit` | Status value conversion/writeback | Keep only where used to build a single calculation input or commit immediately in the frame reducer. Do not use as a broad subsystem sync API. |
| `makeHitResolutionInput`, `updateCommittedHitCombos` | Hit calculation input plus explicit combo commit | Keep for now. After damage/action cleanup, re-check whether pending damage preview can be removed from hit input. |
| `commitRescueResultToRuntime` | Direct runtime commit from frame runner | Keep for now. It is named and frame-runner-owned. |

---

## Implementation Order

### Slice 3A: Collapse Damage Application Into A Frame Reducer

**Status:** Ready to plan/implement. The previous local-working-copy implementation was unwound and must not be restored.

**Intent:** `BattleFrameRunner::applyDamageAndLifecycle()` owns pending damage ordering, runtime mutation, death lifecycle, rescue, battle-end publication, and damage presentation emission. `BattleDamageSystem` remains the pure damage calculation kernel. `BattleDamageApplicationSystem::apply()` should stop being the frame-time orchestrator.

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify/delete: `src/battle/BattleDamageApplicationSystem.h`
- Modify/delete: `src/battle/BattleDamageApplicationSystem.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Modify/delete: `tests/BattleDamageApplicationSystemUnitTests.cpp`
- Project files if the `.cpp` is deleted: `src/CMakeLists.txt`, `src/kys.vcxproj`, `src/kys.vcxproj.filters`, `tests/kys_tests.vcxproj`

**Reducer shape:**

In `BattleCore.cpp`, `applyDamageAndLifecycle()` should conceptually do this:

```cpp
for (const auto pendingIndex : orderedFramePendingDamageIndexes(state.damage.pendingDamage, state.damage.sortPendingDamageByDefenderMagnitude))
{
    const auto& intent = state.damage.pendingDamage[pendingIndex];
    if (!state.unitStore.requireUnit(intent.request.defenderUnitId).alive)
    {
        continue;
    }

    auto transaction = BattleDamageSystem().resolveTransaction(
        makeFrameDamageTransactionInput(state, intent.request));
    applyFrameDamageTakenMpGain(transaction);

    applyDamageResultToFrameState(state, transaction, frame.frameStartMotion);
    appendFrameDamagePresentation(frame, intent.presentation, transaction);
    appendFrameDamageLifecycle(state, frame, transaction);
    appendFrameDamageKillRewardLogs(frame, transaction);
    applyRescueRepositionForDamage(state, frame.result, transaction, frame.logEvents, frame.visualEvents);

    frame.result.damageRenderApplications.push_back(makeBattleFrameDamageRenderApplication(transaction, critical));
    frame.result.damageTransactions.push_back(transaction);
}
```

The exact helper names can differ, but these constraints cannot:

- `makeFrameDamageTransactionInput()` reads current authoritative runtime state only for one transaction.
- `BattleDamageSystem().resolveTransaction()` remains a pure calculation.
- `applyDamageResultToFrameState()` commits the transaction immediately before later lifecycle logic reads runtime.
- Death-effect, shield-break, battle-end, and follow-up command logic runs as frame-owned reducer logic, not through a subsystem-owned copied store.
- `BattleDamageApplicationWorkingState` or equivalent full-store local copies must not exist.

**Steps:**

- [ ] **Step 1: Add/confirm frame-level behavior coverage**

Use existing frame tests first. If a behavior below is not covered, add a `BattleRuntimeSession::runFrame()` or `runBattleFrame(state)` test before refactoring:

```text
same-frame multi-hit accumulation
death prevention
block-first-hit consumption
poison/bleed/status application
damage-taken MP gain
death AOE command
ally-death command emission
battle end emitted once
rescue after damage
damage number/log ordering
```

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][frame_runner][runtime][unit]"
```

- [ ] **Step 2: Create frame reducer helpers in `BattleCore.cpp`**

Move or recreate the damage-application helper logic inside the `BattleCore.cpp` anonymous namespace. Do not duplicate code between `BattleDamageApplicationSystem.cpp` and `BattleCore.cpp`; after the reducer is compiling, delete the old frame orchestrator helpers from `BattleDamageApplicationSystem.cpp`.

Required helper responsibilities:

```text
orderedFramePendingDamageIndexes(...)
makeFrameDamageTransactionInput(BattleRuntimeState&, const BattleDamageRequest&)
applyFrameDamageTakenMpGain(BattleDamageTransactionResult&)
appendFrameDamageOutputEvents(...)
appendFrameDamagePreDeathLogEvents(...)
appendFrameDamageLifecycle(...)
appendFrameShieldBreakCommands(...)
appendFrameDamageKillRewardLogEvents(...)
updateFrameBattleResultAfterDamage(...)
```

- [ ] **Step 3: Change `applyDamageAndLifecycle()` to reduce pending damage directly**

Replace:

```cpp
auto application = BattleDamageApplicationSystem().apply(makeFrameDamageApplicationInput(state));
```

with direct iteration over `state.damage.pendingDamage`, immediate calls to `applyDamageResultToFrameState()`, and immediate event/log/visual/command emission into `BattleFrameContext`.

- [ ] **Step 4: Remove the frame-time damage application API**

Delete `BattleDamageApplicationInput`, `BattleDamageApplicationResult`, `BattleDamageLifecycleEvent`, and `BattleDamageApplicationSystem::apply()` once no production code uses them. Keep `BattlePendingDamageIntent`, `BattleDamagePresentationInput`, `BattleDamagePresentationStyle`, and `BattleDamageApplicationUnitEffects` only if they still describe runtime queues/config; otherwise move them to `BattleCore.h` or a narrower damage queue header.

Verification:

```powershell
rg -n "BattleDamageApplicationSystem\\(\\)\\.apply|makeFrameDamageApplicationInput|BattleDamageApplicationInput|BattleDamageApplicationResult|BattleDamageApplicationWorkingState" src tests
```

Expected: no frame-time production usage. If type definitions remain temporarily for queue/config types, the grep output must be reviewed and documented in the commit message.

- [ ] **Step 5: Update tests away from subsystem orchestration confidence**

Delete or migrate `tests/BattleDamageApplicationSystemUnitTests.cpp` cases that only verify frame orchestration. Keep/move pure damage math tests under `BattleDamageSystem` tests. Frame composition cases belong in `tests/BattleCoreUnitTests.cpp` or `tests/BattleFrameRunnerRuntimeUnitTests.cpp`.

- [ ] **Step 6: Verify**

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe "BattleDamageApplication_*,BattleFrameRunner_*,BattleRuntimeSession_*"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected:

- `git diff --check` exits 0
- all Catch2 tests pass
- MSBuild exits 0, except final linking may be treated as successful only if the executable is locked by a running game process

### Slice 3B: Remove Pending-Damage Preview Mirror

**Depends on:** Slice 3A.

**Intent:** `pendingDefenderHpDamage()` currently copies runtime units, combo units, status units, and damage extras to preview queued damage. That repeats the mirror problem. Remove this full-store preview.

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: hit resolver tests if present

**Acceptable outcomes:**

- Move execute decision to the damage reducer so it uses authoritative pending-damage order at application time.
- Or replace `pendingDefenderHpDamage()` with a narrow projection that carries only the scalar state required for execute prediction. If that projection starts needing full defender/status/damage-extra state, reject it and move the decision to the reducer.

**Verification focus:**

- execute threshold
- projectile cancel damage
- same-frame queued damage before a hit
- death prevention
- battle end once

### Slice 3C: Collapse Movement Planner Mirror

**Depends on:** Slice 3A only for reduced risk; no direct code dependency.

**Intent:** Stop using `BattleMovementFrameInput` as both a copied world and a writeback carrier. `BattleMovement` may remain a decision/planning kernel, but runtime agent and motion mutation must be owned by `advanceMotionFrame()` / `commitFrameMovement()`.

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify movement planner headers/source as needed
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

**Reducer direction:**

- Keep physics advancement as direct runtime reducer logic.
- Build narrow planner inputs per living unit or per tactical query.
- Return movement decisions, not runtime-shaped unit state.
- Mutate `state.movement.agents`, `state.unitStore`, and physics state directly in `commitFrameMovement()`.

**Verification focus:**

- post-movement cast origin
- moving corpse physics
- death kick presentation
- frozen physics timer
- movement reservation cleanup

### Slice 3D: Collapse Action State Writeback

**Depends on:** Slice 3C if movement decisions are still read by action commit input.

**Intent:** Remove `BattleFrameActionUnitResult::state` as the state writeback carrier. `advanceActionFrameUnits()` may use a local stack `BattleUnitFrameTickState` for calculation, but it should mutate the authoritative `BattleRuntimeUnit` action fields directly at the visible commit point.

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

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
