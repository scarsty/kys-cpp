# Battle Committed Frame Output Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove per-frame committed output buffers from persistent `BattleRuntimeState` so frame outputs are local to `BattleFrameRunner::runFrame()` and published through `BattleFrameResult`.

**Architecture:** Persistent runtime state should hold only authoritative battle state and durable queues. Per-frame outputs such as committed damage transactions, rescue results, team-effect events, and effect commands should flow through `BattleFrameResult` or local frame accumulators, not through state members that must be cleared at the start of the next frame.

**Tech Stack:** C++20, MSVC, Catch2 tests, existing `BattleFrameRunner`, `BattleRuntimeState`, `BattleFrameResult`, and battle subsystem result structs.

---

## File Structure

- Modify `src/battle/BattleCore.h`
  - Remove per-frame output fields from nested runtime state structs:
    - `DamageState::committedTransactions`
    - `DamageState::lifecycleEvents`
    - `DamageState::logEvents`
    - `DamageState::visualEvents`
    - `RescueState::committedResults`
    - `TeamEffectState::committedEvents`
    - `EffectState::committedCommands`
  - Keep durable queues/configuration:
    - `DamageState::pendingTransactions`
    - `DamageState::pendingPresentation`
    - `TeamEffectState::pendingCommands`
    - rescue cells/configuration/counters
    - effect activation counts

- Modify `src/battle/BattleCore.cpp`
  - Convert helpers that currently write committed frame outputs into functions that append to `BattleFrameResult` or local vectors.
  - Delete `clearBattleDamageFrameOutputs(...)`.
  - Remove the start-of-frame clears for committed output fields.
  - Change `publishFrameApplyOutputs(...)` so it no longer copies committed outputs from `state`.

- Modify `tests/BattleCoreUnitTests.cpp`
  - Replace assertions against `state.damage.committedTransactions`, `state.damage.logEvents`, `state.damage.visualEvents`, `state.rescue.committedResults`, `state.teamEffects.committedEvents`, and `state.effects.committedCommands` with assertions against `BattleFrameResult`.

---

## Task 1: Move Damage Committed Outputs Out Of Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Update tests to assert frame result damage output**

In `tests/BattleCoreUnitTests.cpp`, replace each assertion like:

```cpp
REQUIRE(state.damage.committedTransactions.size() == 1);
CHECK(state.damage.committedTransactions.front().defender.id == 2);
```

with:

```cpp
REQUIRE(result.damageTransactions.size() == 1);
CHECK(result.damageTransactions.front().defender.id == 2);
```

For tests currently checking damage visual/log output through `state.damage.visualEvents` or `state.damage.logEvents`, assert against `result.frame.visualEvents` and `result.frame.logEvents` instead. Example:

```cpp
REQUIRE(std::any_of(
    result.frame.visualEvents.begin(),
    result.frame.visualEvents.end(),
    [](const BattleVisualEvent& event)
    {
        return event.type == BattleVisualEventType::DamageNumber
            && event.amount == 7
            && event.textSize == 33
            && event.color.r == 40;
    }));

REQUIRE(std::any_of(
    result.frame.logEvents.begin(),
    result.frame.logEvents.end(),
    [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Damage
            && event.skillName == "終段"
            && event.detailText == "第二段";
    }));
```

- [ ] **Step 2: Run focused test build to verify red state**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: tests still compile before production cleanup, but some edited tests may fail until `BattleFrameResult` is populated directly.

- [ ] **Step 3: Remove damage committed output fields**

In `src/battle/BattleCore.h`, remove these fields from `BattleRuntimeState::DamageState`:

```cpp
std::vector<BattleDamageTransactionResult> committedTransactions;
std::vector<BattleDamageLifecycleEvent> lifecycleEvents;
std::vector<BattleLogEvent> logEvents;
std::vector<BattleVisualEvent> visualEvents;
```

- [ ] **Step 4: Change `applyDamageAndLifecycle(...)` to append directly to frame outputs**

In `src/battle/BattleCore.cpp`, replace:

```cpp
state.damage.lifecycleEvents = application.lifecycleEvents;
state.damage.logEvents = application.logEvents;
state.damage.visualEvents = application.visualEvents;
logEvents.insert(logEvents.end(), state.damage.logEvents.begin(), state.damage.logEvents.end());
visualEvents.insert(visualEvents.end(), state.damage.visualEvents.begin(), state.damage.visualEvents.end());
```

with:

```cpp
logEvents.insert(
    logEvents.end(),
    application.logEvents.begin(),
    application.logEvents.end());
visualEvents.insert(
    visualEvents.end(),
    application.visualEvents.begin(),
    application.visualEvents.end());
```

Replace:

```cpp
state.damage.committedTransactions.push_back(transaction);
```

with:

```cpp
result.damageTransactions.push_back(transaction);
```

- [ ] **Step 5: Remove damage clearing and publishing**

Delete `clearBattleDamageFrameOutputs(...)`.

In `BattleFrameRunner::runFrame(...)`, remove:

```cpp
clearBattleDamageFrameOutputs(state);
```

In `publishFrameApplyOutputs(...)`, remove:

```cpp
result.damageTransactions = state.damage.committedTransactions;
```

- [ ] **Step 6: Verify damage output tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core]"
```

Expected: all matching core battle tests pass.

---

## Task 2: Move Rescue Committed Results Out Of Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Update rescue tests to assert `BattleFrameResult::rescueResults`**

Replace assertions like:

```cpp
CHECK(state.rescue.committedResults.empty());
```

with:

```cpp
CHECK(result.rescueResults.empty());
```

When a rescue result is expected, use:

```cpp
REQUIRE(result.rescueResults.size() == 1);
const auto& rescue = result.rescueResults.front();
CHECK(rescue.teleport->unitId == 2);
```

- [ ] **Step 2: Change rescue application helpers to accept frame output**

Find every write:

```cpp
state.rescue.committedResults.push_back(std::move(rescue));
```

Change the owning helper signature from:

```cpp
void applyRescueResultToFrameState(
    BattleRuntimeState& state,
    BattleRescueRepositionResult rescue,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void applyRescueResultToFrameState(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    BattleRescueRepositionResult rescue,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

Inside it, replace the state write with:

```cpp
result.rescueResults.push_back(std::move(rescue));
```

Thread `BattleFrameResult& result` through callers such as `applyRescueRepositionForDamage(...)`.

- [ ] **Step 3: Remove rescue committed state field**

In `src/battle/BattleCore.h`, remove:

```cpp
std::vector<BattleRescueRepositionResult> committedResults;
```

In `BattleFrameRunner::runFrame(...)`, remove:

```cpp
state.rescue.committedResults.clear();
```

In `publishFrameApplyOutputs(...)`, remove:

```cpp
result.rescueResults = state.rescue.committedResults;
```

- [ ] **Step 4: Verify rescue tests**

Run:

```powershell
x64\Debug\kys_tests.exe "*Rescue*"
```

Expected: all matching rescue tests pass.

---

## Task 3: Move Team Effect Committed Events Out Of Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Update team-effect tests to assert `BattleFrameResult::teamEffectEvents`**

Replace:

```cpp
REQUIRE(state.teamEffects.committedEvents.size() == 2);
```

with:

```cpp
REQUIRE(result.teamEffectEvents.size() == 2);
```

- [ ] **Step 2: Thread a frame team-effect output vector through reducers**

Change helper signatures that currently write to `state.teamEffects.committedEvents` so they accept:

```cpp
std::vector<BattleTeamEffectEvent>& teamEffectEvents
```

For example, update `applyFrameTeamEffectCommand(...)` from:

```cpp
bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleLogEvent>& logEvents)
```

to:

```cpp
bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents)
```

Replace every insert into `state.teamEffects.committedEvents` with:

```cpp
teamEffectEvents.insert(
    teamEffectEvents.end(),
    application.events.begin(),
    application.events.end());
```

Also update `applyPendingTeamEffects(...)` to accept the same vector and append there.

- [ ] **Step 3: Pass `result.teamEffectEvents` from `runFrame(...)`**

Update calls:

```cpp
applyPendingTeamEffects(state, logEvents);
reduceFrameGameplayCommands(state, frameCommands, result.applications, gameplayEvents, logEvents, visualEvents);
```

to:

```cpp
applyPendingTeamEffects(state, result.teamEffectEvents, logEvents);
reduceFrameGameplayCommands(
    state,
    frameCommands,
    result.applications,
    result.teamEffectEvents,
    gameplayEvents,
    logEvents,
    visualEvents);
```

Propagate the new `teamEffectEvents` parameter through `reduceFrameGameplayCommands(...)` and `reduceFrameGameplayCommand(...)`.

- [ ] **Step 4: Remove team-effect committed state field**

In `src/battle/BattleCore.h`, remove:

```cpp
std::vector<BattleTeamEffectEvent> committedEvents;
```

In `BattleFrameRunner::runFrame(...)`, remove:

```cpp
state.teamEffects.committedEvents.clear();
```

In `publishFrameApplyOutputs(...)`, remove:

```cpp
result.teamEffectEvents = state.teamEffects.committedEvents;
```

- [ ] **Step 5: Verify team-effect tests**

Run:

```powershell
x64\Debug\kys_tests.exe "*TeamEffect*"
```

Expected: all matching team-effect tests pass.

---

## Task 4: Move Effect Committed Commands Out Of Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Update effect tests to assert `BattleFrameResult::effectCommands`**

Replace:

```cpp
REQUIRE(state.effects.committedCommands.size() == 1);
CHECK(state.effects.committedCommands[0].type == BattleEffectCommandType::AddInvincibility);
```

with:

```cpp
REQUIRE(result.effectCommands.size() == 1);
CHECK(result.effectCommands[0].type == BattleEffectCommandType::AddInvincibility);
```

- [ ] **Step 2: Change effect dispatch to append to frame result**

Find the function that currently does:

```cpp
state.effects.committedCommands.insert(
    state.effects.committedCommands.end(),
    commands.begin(),
    commands.end());
```

Change its signature to accept:

```cpp
std::vector<BattleEffectCommand>& effectCommands
```

Then replace the insert with:

```cpp
effectCommands.insert(
    effectCommands.end(),
    commands.begin(),
    commands.end());
```

Pass `result.effectCommands` from the caller.

- [ ] **Step 3: Remove effect committed state field**

In `src/battle/BattleCore.h`, remove:

```cpp
std::vector<BattleEffectCommand> committedCommands;
```

In `BattleFrameRunner::runFrame(...)`, remove:

```cpp
state.effects.committedCommands.clear();
```

In `publishFrameApplyOutputs(...)`, remove:

```cpp
result.effectCommands = state.effects.committedCommands;
```

- [ ] **Step 4: Verify effect tests**

Run:

```powershell
x64\Debug\kys_tests.exe "*Effect*"
```

Expected: all matching effect tests pass.

---

## Task 5: Simplify `publishFrameApplyOutputs(...)` And Remove Start-Of-Frame Committed Clears

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Collapse `publishFrameApplyOutputs(...)` to authoritative snapshots only**

After Tasks 1-4, `publishFrameApplyOutputs(...)` should only populate:

```cpp
result.stateApplications.comboStates = state.combo.units;
result.stateApplications.statusUnits = ...;
result.deathEffectTrackers = ...;
```

It must not copy damage, rescue, team-effect, or effect-command outputs from `state`.

- [ ] **Step 2: Remove committed clear block from `runFrame(...)`**

In `BattleFrameRunner::runFrame(...)`, the top of frame should no longer contain:

```cpp
clearBattleDamageFrameOutputs(state);
state.teamEffects.committedEvents.clear();
state.effects.committedCommands.clear();
state.rescue.committedResults.clear();
```

Keep these existing clears for now because they are separate event-buffer cleanup:

```cpp
state.status.events.clear();
state.combo.events.clear();
state.deathEffects.events.clear();
```

- [ ] **Step 3: Add a boundary search test step**

Run:

```powershell
rg -n "committedTransactions|committedEvents|committedCommands|committedResults|clearBattleDamageFrameOutputs|damage\\.logEvents|damage\\.visualEvents|damage\\.lifecycleEvents" src\battle tests
```

Expected remaining matches:

```text
tests\BattleDamageApplicationSystemUnitTests.cpp:<line>: result.lifecycleEvents
src\battle\BattleDamageApplicationSystem.h:<line>: std::vector<BattleDamageLifecycleEvent> lifecycleEvents;
src\battle\BattleDamageApplicationSystem.cpp:<line>: result.lifecycleEvents.push_back(...)
```

There should be no remaining `BattleRuntimeState` committed output fields or tests reading committed frame output from `state`.

---

## Task 6: Optional Follow-Up - Remove Non-Committed Event Buffers From Runtime State

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

This task is intentionally separate from the committed-output cleanup. These fields are also per-frame outputs, but they are not named `committed`:

```cpp
StatusState::events
ComboTriggerState::events
DeathEffectState::events
```

- [ ] **Step 1: Search current usage**

Run:

```powershell
rg -n "status\\.events|combo\\.events|deathEffects\\.events" src\battle tests
```

Expected: usage is limited to frame production and tests. If any production caller outside `BattleFrameRunner` depends on these buffers after `runFrame()`, stop and split that dependency first.

- [ ] **Step 2: Move status events into local frame output**

Change `advanceStatus(...)` from mutating `state.status.events` to returning:

```cpp
std::vector<BattleStatusEvent> advanceStatus(BattleRuntimeState& state)
```

Use a local:

```cpp
auto statusEvents = advanceStatus(state);
```

Only add status events to presentation/log outputs where needed. Remove `StatusState::events` after all uses are local.

- [ ] **Step 3: Move combo frame events into `BattleFrameRuntimeUnitResult` only**

`advanceRuntimeUnits(...)` already writes combo events into `BattleFrameRuntimeUnitResult::comboEvents`. Remove `ComboTriggerState::events` if boundary search shows no meaningful persistent use.

- [ ] **Step 4: Move death-effect events into local lifecycle output**

If `DeathEffectState::events` is only used as a frame output, change the relevant death-effect application helper to append directly to `BattleFrameResult` or local `visualEvents` / `logEvents`.

- [ ] **Step 5: Remove remaining event clears**

After the event buffers are gone, remove:

```cpp
state.status.events.clear();
state.combo.events.clear();
state.deathEffects.events.clear();
```

Run:

```powershell
rg -n "status\\.events|combo\\.events|deathEffects\\.events" src\battle tests
```

Expected: no matches.

---

## Task 7: Full Verification

**Files:**
- Verify all changed files.

- [ ] **Step 1: Run whitespace check**

Run:

```powershell
git diff --check
```

Expected: no output and exit code `0`.

- [ ] **Step 2: Build test target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exits with code `0`.

- [ ] **Step 3: Run all tests**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all test cases pass.

- [ ] **Step 4: Build game target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: MSBuild exits with code `0`. Existing warning noise is acceptable.

- [ ] **Step 5: Run final boundary searches**

Run:

```powershell
rg -n "committedTransactions|committedEvents|committedCommands|committedResults|clearBattleDamageFrameOutputs|damage\\.logEvents|damage\\.visualEvents|damage\\.lifecycleEvents" src\battle tests
rg -n "state\\.damage\\.committed|state\\.teamEffects\\.committed|state\\.effects\\.committed|state\\.rescue\\.committed" tests src
```

Expected:

- No runtime-state committed output fields.
- No tests asserting frame output through `state`.
- Damage application system may still have its own local `BattleDamageApplicationResult::lifecycleEvents`.

---

## Self-Review

- Spec coverage: The plan covers all currently identified committed frame output fields and the clear calls they require.
- Placeholder scan: No open implementation placeholders remain; each task names the files, fields, function signatures, replacement code shape, and verification commands.
- Type consistency: The plan consistently uses existing `BattleFrameResult` fields for public frame outputs and local/vector parameters for intermediate outputs.
