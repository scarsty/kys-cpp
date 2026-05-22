# Battle Frame Context Routing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route eligible top-level `BattleFrameRunner` helper signatures through the one-frame `BattleFrameContext` introduced in phase 2A without turning the context into a subsystem-owned mini-world.

**Architecture:** Keep `BattleFrameContext` local to `src/battle/BattleCore.cpp` as a frame transaction ledger owned by `BattleFrameRunner`, not as a subsystem API. Convert only named frame-transaction step helpers; lower-level rule/reducer helpers should keep narrow explicit parameters. Do not delete `BattleFrameResult` fields and do not change subsystem ownership in this plan.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Scope

This plan is phase 2B. It depends on:

```text
ed98256 refactor: introduce battle frame context
```

This plan does not:

- remove or rename `BattleFrameResult` fields
- delete movement, damage, action, attack, or status mirror-world APIs
- change scene presentation behavior
- add new gameplay behavior
- pass `BattleFrameContext` to subsystem classes or headers
- convert lower-level calculation helpers just to reduce parameter count

Existing frame-level tests are the guard because this is a mechanical refactor. If any step requires a behavior change, stop and write a failing frame-level test before changing production code.

---

## Frame Context Ownership Contract

`BattleFrameContext` is not authoritative gameplay state. Its ownership rule is:

```text
BattleRuntimeState = persistent gameplay owner
BattleFrameContext = one-runFrame transaction scratch and output ledger
BattleFrameResult = public scene/report output after the transaction
```

Use these lanes consistently:

| Lane | Members | Rule |
| --- | --- | --- |
| Public frame output | `frame.result` | May be returned to scene/tests. Writes should stay visible at publish/commit points. |
| Internal command queues | `frame.frameCommands`, `frame.deferredCommands` | Must be reduced, merged, or drained before the frame returns. |
| Internal event staging | `frame.gameplayEvents`, `frame.logEvents`, `frame.visualEvents` | Must be consumed into `frame.result.frame` by `emitPresentationFrame()`. |
| Internal unit tick scratch | `frame.runtimeCommits` | Exists only to bridge unit tick output into same-frame combo handling. |
| Internal motion snapshot | `frame.frameStartMotion` | Exists only so same-frame damage/death logic can compare against pre-damage motion. |

Eligibility rule for `BattleFrameContext&` parameters:

- Allowed: anonymous-namespace functions in `src/battle/BattleCore.cpp` that are named frame-transaction steps and already coordinate several frame lanes.
- Not allowed: subsystem classes, public headers, pure rule helpers, math helpers, conversion helpers, or functions that only need one lane.
- Prefer explicit `frame.result.*` at publication points over hiding public output writes behind generic helper names.

Phase 2B helps ownership only by making the transaction ledger explicit and auditable. Actual deletion of mirror worlds and result channels happens in later plans after consumer audits.

---

## Files And Responsibilities

| File | Responsibility |
| --- | --- |
| `src/battle/BattleCore.cpp` | Convert helper signatures and `runFrame()` call sites to use `BattleFrameContext&`. |
| `docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md` | Track this phase 2B implementation. |
| `tests/BattleCoreUnitTests.cpp` | Existing frame-runner behavior coverage. |
| `tests/BattleFrameRunnerRuntimeUnitTests.cpp` | Existing runtime-owned frame-state coverage. |

---

## Task 0: Codify Context Boundary In Code

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [x] **Step 1: Add an ownership comment above `BattleFrameContext`**

In `src/battle/BattleCore.cpp`, add this comment immediately above `struct BattleFrameContext`:

```cpp
// One runFrame() transaction ledger. Persistent gameplay lives in BattleRuntimeState.
// The only public frame output is result; every other member is frame-local scratch.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
```

- [x] **Step 2: Add a result-consumption helper**

Add this helper after `makeBattleFrameContext()`:

```cpp
BattleFrameResult consumeBattleFrameContext(BattleFrameContext&& frame)
{
    assert(frame.frameCommands.empty());
    return std::move(frame.result);
}
```

This is an exit-boundary check, not defensive programming. `frameCommands` must be empty because unreduced gameplay commands mean the frame transaction did not finish.

- [x] **Step 3: Return through the consumption helper**

Change the end of `BattleFrameRunner::runFrame()` from:

```cpp
return std::move(frame.result);
```

to:

```cpp
return consumeBattleFrameContext(std::move(frame));
```

- [x] **Step 4: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
```

Expected: all selected tests pass.

Result: `x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"` passed 893 assertions in 124 test cases.

- [x] **Step 5: Commit Task 0**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: codify battle frame context boundary"
```

Expected: one commit that only documents/enforces the frame-context exit boundary.

Result: committed as `refactor: codify battle frame context boundary`.

---

## Task 1: Route Gameplay Command Reduction Through Context

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [x] **Step 1: Run the focused baseline**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
```

Expected: all selected tests pass before editing.

Result: `x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"` passed 893 assertions in 124 test cases.

- [x] **Step 2: Split narrow implementation from context wrapper**

In `src/battle/BattleCore.cpp`, keep the narrow reducer shape for lower-level callers by renaming the existing function from:

```cpp
void reduceFrameGameplayCommands(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    BattleFrameApplications& applications,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    BattleFrameApplications& applications,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

Keep the existing implementation body under `reduceFrameGameplayCommandsImpl`.

- [x] **Step 3: Add the context wrapper for top-level transaction steps**

Add this wrapper immediately after `reduceFrameGameplayCommandsImpl`:

```cpp
void reduceFrameGameplayCommands(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame.frameCommands,
        frame.result.applications,
        frame.result.effectCommands,
        frame.result.teamEffectEvents,
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents);
}
```

- [x] **Step 4: Update direct `runFrame()` call sites**

Replace each call in `BattleFrameRunner::runFrame()` with:

```cpp
reduceFrameGameplayCommands(state, frame);
```

Inside `advanceAttacksAndResolveHits`, update the nested call to the narrow implementation:

```cpp
reduceFrameGameplayCommandsImpl(
    state,
    frameCommands,
    result.applications,
    effectCommands,
    result.teamEffectEvents,
    gameplayEvents,
    logEvents,
    visualEvents);
```

- [x] **Step 5: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
```

Expected: all selected tests pass.

Result: `x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"` passed 893 assertions in 124 test cases.

- [x] **Step 6: Commit Task 1**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: route frame command reduction through context"
```

Expected: one commit that adds a context wrapper for `reduceFrameGameplayCommands`, keeps a narrow implementation for lower-level callers, and updates direct `runFrame()` call sites.

Result: committed as `refactor: route frame command reduction through context`.

---

## Task 2: Route Combo And Pending Team Effects Through Context

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [x] **Step 1: Change `applyRuntimeComboEvents` signature**

Change:

```cpp
void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits,
    std::vector<BattleGameplayCommand>& deferredCommands,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void applyRuntimeComboEvents(BattleRuntimeState& state, BattleFrameContext& frame)
```

Inside the body:

```cpp
for (const auto& result : frame.runtimeCommits)
```

Replace output references:

```cpp
frame.result.teamEffectEvents
frame.result.effectCommands
frame.logEvents
frame.visualEvents
frame.deferredCommands
```

- [x] **Step 2: Change `applyPendingTeamEffects` signature**

Change:

```cpp
void applyPendingTeamEffects(
    BattleRuntimeState& state,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void applyPendingTeamEffects(BattleRuntimeState& state, BattleFrameContext& frame)
```

Inside the body, replace output references:

```cpp
frame.result.teamEffectEvents
frame.logEvents
frame.visualEvents
```

- [x] **Step 3: Update `runFrame()` call sites**

Replace:

```cpp
applyRuntimeComboEvents(state, ...);
applyPendingTeamEffects(state, ...);
```

with:

```cpp
applyRuntimeComboEvents(state, frame);
applyPendingTeamEffects(state, frame);
```

- [x] **Step 4: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
```

Expected: all selected tests pass.

Result: `x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"` passed 893 assertions in 124 test cases.

- [x] **Step 5: Commit Task 2**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: route combo frame effects through context"
```

Expected: one commit that changes only combo/team-effect frame routing.

Result: committed as `refactor: route combo frame effects through context`.

---

## Task 3: Route Action Frame Advancement Through Context

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Change `advanceActionFrameUnits` signature**

Change:

```cpp
void advanceActionFrameUnits(
    BattleRuntimeState& state,
    const BattleTickResult& movement,
    BattleFrameApplications& applications,
    std::vector<BattleFrameActionUnitResult>& actionResults,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void advanceActionFrameUnits(BattleRuntimeState& state, BattleFrameContext& frame)
```

Use these replacements inside the function:

```cpp
movement -> frame.result.movement
applications -> frame.result.applications
actionResults -> frame.result.actionResults
effectCommands -> frame.result.effectCommands
frameCommands -> frame.frameCommands
gameplayEvents -> frame.gameplayEvents
logEvents -> frame.logEvents
visualEvents -> frame.visualEvents
```

- [ ] **Step 2: Update `runFrame()` call site**

Replace the multi-argument call with:

```cpp
advanceActionFrameUnits(state, frame);
```

- [ ] **Step 3: Run action/cast focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner]"
```

Expected: all selected tests pass.

- [ ] **Step 4: Commit Task 3**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: route action frame through context"
```

Expected: one commit that changes only action frame routing.

---

## Task 4: Route Attack And Hit Advancement Through Context

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Change `advanceAttacksAndResolveHits` signature**

Change:

```cpp
void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void advanceAttacksAndResolveHits(BattleRuntimeState& state, BattleFrameContext& frame)
```

Use these replacements inside the function:

```cpp
result -> frame.result
frameCommands -> frame.frameCommands
effectCommands -> frame.result.effectCommands
gameplayEvents -> frame.gameplayEvents
logEvents -> frame.logEvents
visualEvents -> frame.visualEvents
```

- [ ] **Step 2: Update the nested command reduction**

Inside `advanceAttacksAndResolveHits`, replace the nested multi-argument `reduceFrameGameplayCommands(...)` call with:

```cpp
reduceFrameGameplayCommands(state, frame);
```

- [ ] **Step 3: Update `runFrame()` call site**

Replace the multi-argument call with:

```cpp
advanceAttacksAndResolveHits(state, frame);
```

- [ ] **Step 4: Run projectile/hit focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: all selected tests pass.

- [ ] **Step 5: Commit Task 4**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: route attack hit frame through context"
```

Expected: one commit that changes only attack/hit frame routing.

---

## Task 5: Route Damage Lifecycle And Presentation Emission Through Context

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Change `applyDamageAndLifecycle` signature**

Change:

```cpp
void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    const UnitMotionSnapshotMap& frameStartMotion,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void applyDamageAndLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
```

Use these replacements inside the function:

```cpp
result -> frame.result
frameStartMotion -> frame.frameStartMotion
frameCommands -> frame.frameCommands
gameplayEvents -> frame.gameplayEvents
logEvents -> frame.logEvents
visualEvents -> frame.visualEvents
```

- [ ] **Step 2: Change `emitPresentationFrame` signature**

Change:

```cpp
void emitPresentationFrame(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void emitPresentationFrame(BattleRuntimeState& state, BattleFrameContext& frame)
```

Use these replacements inside the function:

```cpp
result -> frame.result
gameplayEvents -> frame.gameplayEvents
logEvents -> frame.logEvents
visualEvents -> frame.visualEvents
```

- [ ] **Step 3: Update final `runFrame()` context uses**

Replace:

```cpp
applyDamageAndLifecycle(state, ...);
emitPresentationFrame(state, ...);
```

with:

```cpp
applyDamageAndLifecycle(state, frame);
emitPresentationFrame(state, frame);
```

Keep these explicit context-field calls in `runFrame()`:

```cpp
appendProjectileCancellationLogEvents(state.attacks, frame.result.attackEvents, frame.logEvents, true);
publishMovementPresentationResults(state, frame.result);
publishFrameApplyOutputs(state, frame.result);
```

- [ ] **Step 4: Run focused and full tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
x64\Debug\kys_tests.exe
```

Expected: focused tests and full tests pass.

- [ ] **Step 5: Run build**

Run:

```powershell
.github\build-command.ps1
```

Expected: MSBuild exits 0. The existing `BattleCore.cpp(1264)` C4244 warning may still appear.

- [ ] **Step 6: Commit Task 5**

Run:

```powershell
git add src/battle/BattleCore.cpp docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
git commit -m "refactor: route damage frame output through context"
```

Expected: one commit that finishes phase 2B helper routing without changing `BattleFrameResult` fields.

---

## Final Verification

- [ ] **Step 1: Confirm top-level `runFrame()` no longer passes parallel frame vectors**

Run:

```powershell
rg -n "frameCommands,|gameplayEvents,|logEvents,|visualEvents|deferredCommands|runtimeCommits" src/battle/BattleCore.cpp
```

Expected: `BattleFrameRunner::runFrame()` may access `frame.*` fields, but direct helper calls from `runFrame()` no longer pass long parallel vector argument lists.

- [ ] **Step 2: Confirm `BattleFrameContext` remains private and narrowly used**

Run:

```powershell
rg -n "BattleFrameContext" src
```

Expected: matches only in `src/battle/BattleCore.cpp`.

Run:

```powershell
rg -n "BattleFrameContext&" src/battle/BattleCore.cpp
```

Expected: matches only for named frame-transaction step helpers from this plan and the command-reduction wrapper.

- [ ] **Step 3: Run completion gate**

Run:

```powershell
git diff --check
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, full tests pass, and MSBuild exits 0.
