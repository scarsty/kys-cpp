# Battle Frame Context Low-Hanging Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove remaining low-value battle frame command/scratch channels without broad file shuffling or type churn.

**Architecture:** Keep `BattleFrameRunner::runFrame()` as the ordered frame transaction and keep persistent gameplay state in `BattleRuntimeState`. Prefer deleting write-only ledgers and no-op commands over moving code between files. Preserve current frame ordering: runtime combo effects before pending team effects, movement before actions, hit commands before damage, late auto-ultimate commands after damage/lifecycle logs.

**Tech Stack:** C++20, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Current Shape

- `BattleCore.cpp` owns a private `BattleFrameContext` with fewer scratch fields than before, but some remaining fields are still write-only or single-phase handoffs.
- `BattleProjectileCancelDamageCommand` is still part of `BattleGameplayCommand`, but the reducer treats it as a no-op. Projectile cancel damage and logs already flow through `BattleAttackEvent`.
- `BattleFrameContext::effectCommands` and `BattleFrameContext::teamEffectEvents` are appended for historical ledger purposes, but frame logs and visuals are emitted immediately at the application sites.
- `BattleFrameContext::runtimeCommits` is filled by `advanceRuntimeUnits(...)` and consumed immediately by `applyRuntimeComboEvents(...)`.
- `BattleFrameContext::deferredCommands` only stores late auto-ultimate commands until the late reducer drain.
- `BattleFrameContext::movement` is a real phase handoff from motion to action, but it can be a local `runFrame()` value.

## End State

- `BattleProjectileCancelDamageCommand` is gone from `BattleHitResolver.h`, and projectile cancel resolution no longer emits no-op gameplay commands.
- `BattleFrameContext` no longer contains:
  - `effectCommands`
  - `teamEffectEvents`
  - `runtimeCommits`
  - `deferredCommands`
  - `movement`
- Runtime combo commits and late auto-ultimate commands are local variables in `runFrame()`.
- Movement results are returned from `advanceMotionFrame(...)` and passed explicitly into `advanceActionFrameUnits(...)`.
- `BattleFrameContext` remains private to `BattleCore.cpp` and contains only frame output/event accumulators that are still consumed later in the frame.

---

## Task 1: Delete No-Op Projectile-Cancel Command

**Files:**
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm command is no-op traffic**

Run:

```powershell
rg -n -C 4 "BattleProjectileCancelDamageCommand|applyProjectileCancelDamageResults|appendProjectileCancellationLogEvents" src\battle tests
```

Expected before editing:
- `BattleProjectileCancelDamageCommand` is declared in `src/battle/BattleHitResolver.h`.
- It is included in the `BattleGameplayCommand` variant.
- `applyProjectileCancelDamageResults(...)` creates and pushes the command.
- `reduceFrameGameplayCommand(...)` returns `true` for the command without applying behavior.
- Projectile cancel logs are emitted from `BattleAttackEvent` by `appendProjectileCancellationLogEvents(...)`.

- [ ] **Step 2: Remove the command type**

In `src/battle/BattleHitResolver.h`, delete:

```cpp
struct BattleProjectileCancelDamageCommand
{
    int attackId{};
    int otherAttackId{};
    int sourceUnitId{};
    int otherSourceUnitId{};
    int damage{};
    int otherDamage{};
};
```

In the `BattleGameplayCommand` variant, delete:

```cpp
    BattleProjectileCancelDamageCommand,
```

- [ ] **Step 3: Stop emitting the no-op command**

In `src/battle/BattleCore.cpp`, inside `applyProjectileCancelDamageResults(...)`, replace:

```cpp
        BattleProjectileCancelDamageCommand command;
        command.attackId = event.attackId;
        command.otherAttackId = event.otherAttackId;
        command.sourceUnitId = event.sourceUnitId;
        command.otherSourceUnitId = event.otherSourceUnitId;
        command.damage = event.projectileCancelDamage;
        command.otherDamage = event.otherProjectileCancelDamage;
        commands.push_back(command);
        attackSystem.applyProjectileCancelDamage(state.attacks, event);
```

with:

```cpp
        attackSystem.applyProjectileCancelDamage(state.attacks, event);
```

Then change the function signature from:

```cpp
void applyProjectileCancelDamageResults(
    BattleRuntimeState& state,
    std::vector<BattleAttackEvent>& events,
    std::vector<BattleGameplayCommand>& commands)
```

to:

```cpp
void applyProjectileCancelDamageResults(
    BattleRuntimeState& state,
    std::vector<BattleAttackEvent>& events)
```

In `advanceAttacksAndResolveHits(...)`, replace:

```cpp
    applyProjectileCancelDamageResults(
        state,
        attackEvents,
        frameCommands);
```

with:

```cpp
    applyProjectileCancelDamageResults(state, attackEvents);
```

- [ ] **Step 4: Remove reducer no-op branch**

In `reduceFrameGameplayCommand(...)`, delete:

```cpp
    if (std::holds_alternative<BattleProjectileCancelDamageCommand>(command))
    {
        return true;
    }
```

- [ ] **Step 5: Verify slice 1**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][hit_resolver],[battle][presentation]"
rg -n "BattleProjectileCancelDamageCommand" src tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 6: Commit slice 1**

Run:

```powershell
git add src\battle\BattleHitResolver.h src\battle\BattleCore.cpp
git commit -m "refactor: remove battle projectile cancel no-op command"
```

---

## Task 2: Remove Write-Only Effect Command Ledger

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm `effectCommands` is write-only**

Run:

```powershell
rg -n "effectCommands|frame\.effectCommands" src\battle\BattleCore.cpp tests
```

Expected before editing:
- `BattleFrameContext` has `std::vector<BattleEffectCommand> effectCommands`.
- `reduceFrameGameplayCommand(...)`, `reduceFrameGameplayCommandsImpl(...)`, and `tryCommitAutoUltimate(...)` pass the vector through.
- `applyCastPostSkillInvincibility(...)` appends to it after already appending log events.
- There is no later consumer of `frame.effectCommands`.

- [ ] **Step 2: Remove the context field**

In private `BattleFrameContext`, delete:

```cpp
    std::vector<BattleEffectCommand> effectCommands;
```

- [ ] **Step 3: Stop passing `effectCommands` through auto ultimate**

Change `tryCommitAutoUltimate(...)` signature from:

```cpp
bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::vector<int>& attackSoundIds,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleGameplayCommand>& pendingCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
bool tryCommitAutoUltimate(
    BattleRuntimeState& state,
    int unitId,
    bool consumeMp,
    bool announceAutoUltimate,
    std::vector<int>& attackSoundIds,
    std::vector<BattleGameplayCommand>& pendingCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

Inside `tryCommitAutoUltimate(...)`, replace:

```cpp
    applyCastPostSkillInvincibility(
        state,
        unitId,
        actionResult.combo.postSkillInvincFrames,
        effectCommands,
        logEvents);
```

with:

```cpp
    applyCastPostSkillInvincibility(
        state,
        unitId,
        actionResult.combo.postSkillInvincFrames,
        logEvents);
```

- [ ] **Step 4: Stop passing `effectCommands` through command reduction**

Change `reduceFrameGameplayCommand(...)` signature by deleting:

```cpp
    std::vector<BattleEffectCommand>& effectCommands,
```

In the `BattleAutoUltimateCommand` branch, replace:

```cpp
            attackSoundIds,
            effectCommands,
            pending,
            gameplayEvents,
            logEvents,
            visualEvents);
```

with:

```cpp
            attackSoundIds,
            pending,
            gameplayEvents,
            logEvents,
            visualEvents);
```

Change `reduceFrameGameplayCommandsImpl(...)` signature by deleting:

```cpp
    std::vector<BattleEffectCommand>& effectCommands,
```

In its call to `reduceFrameGameplayCommand(...)`, delete:

```cpp
            effectCommands,
```

In `reduceFrameGameplayCommands(...)`, replace:

```cpp
        frame.attackSoundIds,
        frame.rumbles,
        frame.effectCommands,
        frame.teamEffectEvents,
        frame.gameplayEvents,
```

with:

```cpp
        frame.attackSoundIds,
        frame.rumbles,
        frame.teamEffectEvents,
        frame.gameplayEvents,
```

- [ ] **Step 5: Make post-skill invincibility log-only**

Change `applyCastPostSkillInvincibility(...)` declaration and definition from:

```cpp
void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleEffectCommand>& effectCommands,
    std::vector<BattleLogEvent>& logEvents)
```

to:

```cpp
void applyCastPostSkillInvincibility(
    BattleRuntimeState& state,
    int sourceUnitId,
    int frames,
    std::vector<BattleLogEvent>& logEvents)
```

Inside the function, delete:

```cpp
    effectCommands.push_back(std::move(command));
```

Keep:

```cpp
    const std::vector<BattleEffectCommand> logCommands{ command };
    appendEffectCommandLogEvents(logEvents, logCommands);
```

- [ ] **Step 6: Remove action-phase local alias and call argument**

In `advanceActionFrameUnits(...)`, delete:

```cpp
    auto& effectCommands = frame.effectCommands;
```

Replace:

```cpp
                    applyCastPostSkillInvincibility(
                        state,
                        unit.id,
                        result.actionResult.combo.postSkillInvincFrames,
                        effectCommands,
                        logEvents);
```

with:

```cpp
                    applyCastPostSkillInvincibility(
                        state,
                        unit.id,
                        result.actionResult.combo.postSkillInvincFrames,
                        logEvents);
```

- [ ] **Step 7: Verify slice 2**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][presentation]"
rg -n "effectCommands|frame\.effectCommands" src\battle\BattleCore.cpp tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 8: Commit slice 2**

Run:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: remove battle effect command frame ledger"
```

---

## Task 3: Remove Write-Only Team Effect Event Ledger

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm `teamEffectEvents` is write-only**

Run:

```powershell
rg -n "teamEffectEvents|frame\.teamEffectEvents" src\battle\BattleCore.cpp tests
```

Expected before editing:
- `BattleFrameContext` has `std::vector<BattleTeamEffectEvent> teamEffectEvents`.
- Team effect application functions append logs and visuals immediately.
- `frame.teamEffectEvents` is never consumed after insertion.

- [ ] **Step 2: Remove the context field**

In private `BattleFrameContext`, delete:

```cpp
    std::vector<BattleTeamEffectEvent> teamEffectEvents;
```

- [ ] **Step 3: Simplify frame team effect command application**

Change `applyFrameTeamEffectCommand(...)` signature from:

```cpp
bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
bool applyFrameTeamEffectCommand(
    BattleRuntimeState& state,
    const BattleGameplayCommand& command,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

Inside the function, delete:

```cpp
    teamEffectEvents.insert(
        teamEffectEvents.end(),
        application.events.begin(),
        application.events.end());
```

Keep the log and visual inserts:

```cpp
    logEvents.insert(
        logEvents.end(),
        application.logEvents.begin(),
        application.logEvents.end());
    appendTeamEffectVisualEvents(visualEvents, application.events);
```

- [ ] **Step 4: Stop passing team effect events through command reduction**

Change `reduceFrameGameplayCommand(...)` signature by deleting:

```cpp
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
```

In the team heal/mp/shield branch, replace:

```cpp
        return applyFrameTeamEffectCommand(state, command, teamEffectEvents, logEvents, visualEvents);
```

with:

```cpp
        return applyFrameTeamEffectCommand(state, command, logEvents, visualEvents);
```

Change `reduceFrameGameplayCommandsImpl(...)` signature by deleting:

```cpp
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
```

In its call to `reduceFrameGameplayCommand(...)`, delete:

```cpp
            teamEffectEvents,
```

In `reduceFrameGameplayCommands(...)`, delete:

```cpp
        frame.teamEffectEvents,
```

- [ ] **Step 5: Simplify runtime team event application**

Change `applyRuntimeTeamEvents(...)` signature from:

```cpp
void applyRuntimeTeamEvents(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattleTeamEffectEvent>& teamEffectEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

to:

```cpp
void applyRuntimeTeamEvents(
    BattleRuntimeState& state,
    int sourceUnitId,
    const BattleComboFrameRuntimeEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
```

Inside the function, delete:

```cpp
    teamEffectEvents.insert(
        teamEffectEvents.end(),
        events.begin(),
        events.end());
```

In `applyRuntimeComboEvents(...)`, replace:

```cpp
                applyRuntimeTeamEvents(
                    state,
                    result.unitId,
                    event,
                    frame.teamEffectEvents,
                    frame.logEvents,
                    frame.visualEvents);
```

with:

```cpp
                applyRuntimeTeamEvents(
                    state,
                    result.unitId,
                    event,
                    frame.logEvents,
                    frame.visualEvents);
```

- [ ] **Step 6: Simplify pending team effects**

In `applyPendingTeamEffects(...)`, delete:

```cpp
            frame.teamEffectEvents.insert(
                frame.teamEffectEvents.end(),
                application.events.begin(),
                application.events.end());
```

Keep:

```cpp
            frame.logEvents.insert(
                frame.logEvents.end(),
                application.logEvents.begin(),
                application.logEvents.end());
            appendTeamEffectVisualEvents(frame.visualEvents, application.events);
```

- [ ] **Step 7: Verify slice 3**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][scenario][runtime],[battle][presentation]"
rg -n "teamEffectEvents|frame\.teamEffectEvents" src\battle\BattleCore.cpp tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 8: Commit slice 3**

Run:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: remove battle team effect frame ledger"
```

---

## Task 4: Localize Runtime Combo Commits

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm `runtimeCommits` is a single-phase handoff**

Run:

```powershell
rg -n -C 4 "BattleRuntimeUnitFrameCommit|runtimeCommits|frame\.runtimeCommits|advanceRuntimeUnits|applyRuntimeComboEvents" src\battle\BattleCore.cpp
```

Expected before editing:
- `BattleFrameContext` owns `runtimeCommits`.
- `advanceRuntimeUnits(...)` fills it.
- `applyRuntimeComboEvents(...)` immediately reads it.
- `BattleRuntimeUnitFrameCommit::tick` is only used locally inside `advanceRuntimeUnits(...)`.

- [ ] **Step 2: Remove unused `tick` from `BattleRuntimeUnitFrameCommit`**

Change:

```cpp
struct BattleRuntimeUnitFrameCommit
{
    int unitId = -1;
    BattleUnitFrameTickResult tick;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
    RoleComboState comboState;
};
```

to:

```cpp
struct BattleRuntimeUnitFrameCommit
{
    int unitId = -1;
    std::vector<BattleComboFrameRuntimeEvent> comboEvents;
    RoleComboState comboState;
};
```

- [ ] **Step 3: Make `advanceRuntimeUnits(...)` return commits**

Change the signature from:

```cpp
void advanceRuntimeUnits(
    BattleRuntimeState& state,
    std::vector<BattleGameplayCommand>& commands,
    std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits)
```

to:

```cpp
std::vector<BattleRuntimeUnitFrameCommit> advanceRuntimeUnits(BattleRuntimeState& state)
```

At the start of the function, replace:

```cpp
    BattleComboTriggerSystem comboSystem;
    runtimeCommits.clear();
```

with:

```cpp
    BattleComboTriggerSystem comboSystem;
    std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits;
```

Inside the loop, replace:

```cpp
        committed.tick = BattleUnitFrameTickSystem().advance(input);

        unit.animation.cooldown = committed.tick.state.cooldown;
        unit.animation.actFrame = committed.tick.state.actFrame;
        unit.haveAction = committed.tick.state.haveAction;
        unit.operationType = committed.tick.state.operationType;
        unit.animation.actType = committed.tick.state.actType;
        unit.physicalPower = committed.tick.state.physicalPower;
        applyRuntimeUnitMpDelta(unit, committed.tick.mpDelta);
        if (committed.tick.resetDashVelocity)
```

with:

```cpp
        auto tick = BattleUnitFrameTickSystem().advance(input);

        unit.animation.cooldown = tick.state.cooldown;
        unit.animation.actFrame = tick.state.actFrame;
        unit.haveAction = tick.state.haveAction;
        unit.operationType = tick.state.operationType;
        unit.animation.actType = tick.state.actType;
        unit.physicalPower = tick.state.physicalPower;
        applyRuntimeUnitMpDelta(unit, tick.mpDelta);
        if (tick.resetDashVelocity)
```

Replace:

```cpp
        if (committed.tick.skillFinished && comboIt != state.combo.units.end())
```

with:

```cpp
        if (tick.skillFinished && comboIt != state.combo.units.end())
```

At the end of the function, after the loop, add:

```cpp
    return runtimeCommits;
```

- [ ] **Step 4: Remove runtime commits from frame context**

In private `BattleFrameContext`, delete:

```cpp
    std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits;
```

Change `applyRuntimeComboEvents(...)` signature from:

```cpp
void applyRuntimeComboEvents(BattleRuntimeState& state, BattleFrameContext& frame)
```

to:

```cpp
void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits)
```

Inside the function, replace:

```cpp
    for (const auto& result : frame.runtimeCommits)
```

with:

```cpp
    for (const auto& result : runtimeCommits)
```

In `BattleFrameRunner::runFrame(...)`, replace:

```cpp
    advanceRuntimeUnits(state, frame.frameCommands, frame.runtimeCommits);
    // Apply combo timer events to runtime state, deferring auto-ultimate commands until late frame.
    applyRuntimeComboEvents(state, frame);
```

with:

```cpp
    auto runtimeCommits = advanceRuntimeUnits(state);
    // Apply combo timer events to runtime state, deferring auto-ultimate commands until late frame.
    applyRuntimeComboEvents(state, frame, runtimeCommits);
```

- [ ] **Step 5: Verify slice 4**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][scenario][runtime]"
rg -n "frame\.runtimeCommits|std::vector<BattleRuntimeUnitFrameCommit>&|runtimeCommits\.clear\(|committed\.tick" src\battle\BattleCore.cpp tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 6: Commit slice 4**

Run:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: localize battle runtime combo commits"
```

---

## Task 5: Localize Late Deferred Commands

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm deferred commands only preserve late ordering**

Run:

```powershell
rg -n -C 4 "deferredCommands|frame\.deferredCommands|BattleAutoUltimateCommand" src\battle\BattleCore.cpp tests
```

Expected before editing:
- `BattleFrameContext` owns `deferredCommands`.
- `applyRuntimeComboEvents(...)` pushes only `BattleAutoUltimateCommand{ result.unitId, false, true }`.
- `runFrame(...)` appends deferred commands to `frame.frameCommands` immediately before the late reducer.

- [ ] **Step 2: Make runtime combo event application return deferred commands**

Change `applyRuntimeComboEvents(...)` signature from:

```cpp
void applyRuntimeComboEvents(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits)
```

to:

```cpp
std::vector<BattleGameplayCommand> applyRuntimeComboEvents(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<BattleRuntimeUnitFrameCommit>& runtimeCommits)
```

At the start of the function, add:

```cpp
    std::vector<BattleGameplayCommand> deferredCommands;
```

Replace:

```cpp
            frame.deferredCommands.push_back(BattleAutoUltimateCommand{ result.unitId, false, true });
```

with:

```cpp
            deferredCommands.push_back(BattleAutoUltimateCommand{ result.unitId, false, true });
```

At the end of the function, add:

```cpp
    return deferredCommands;
```

- [ ] **Step 3: Remove deferred commands from frame context**

In private `BattleFrameContext`, delete:

```cpp
    std::vector<BattleGameplayCommand> deferredCommands;
```

In `BattleFrameRunner::runFrame(...)`, replace:

```cpp
    applyRuntimeComboEvents(state, frame, runtimeCommits);
```

with:

```cpp
    auto deferredCommands = applyRuntimeComboEvents(state, frame, runtimeCommits);
```

Keep the late insertion ordering, but replace:

```cpp
    frame.frameCommands.insert(
        frame.frameCommands.end(),
        std::make_move_iterator(frame.deferredCommands.begin()),
        std::make_move_iterator(frame.deferredCommands.end()));
```

with:

```cpp
    frame.frameCommands.insert(
        frame.frameCommands.end(),
        std::make_move_iterator(deferredCommands.begin()),
        std::make_move_iterator(deferredCommands.end()));
```

- [ ] **Step 4: Verify slice 5**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][scenario][runtime]"
rg -n "frame\.deferredCommands" src\battle\BattleCore.cpp tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 5: Commit slice 5**

Run:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: localize battle late deferred commands"
```

---

## Task 6: Pass Movement Result Explicitly

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Confirm movement is a motion-to-action phase handoff**

Run:

```powershell
rg -n -C 4 "frame\.movement|BattleTickResult movement|advanceMotionFrame|commitFrameMovement|advanceActionFrameUnits" src\battle\BattleCore.cpp tests
```

Expected before editing:
- `commitFrameMovement(...)` stores `movement` in `frame.movement`.
- `advanceActionFrameUnits(...)` reads `frame.movement`.
- No later phase needs `frame.movement`.

- [ ] **Step 2: Remove movement from frame context**

In private `BattleFrameContext`, delete:

```cpp
    BattleTickResult movement;
```

- [ ] **Step 3: Make movement commit return the movement result**

Change `commitFrameMovement(...)` signature from:

```cpp
void commitFrameMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults,
    BattleTickResult movement)
```

to:

```cpp
BattleTickResult commitFrameMovement(
    BattleRuntimeState& state,
    const std::vector<BattleFrameMovementPhysicsUnitResult>& physicsResults,
    BattleTickResult movement)
```

Inside the function, replace:

```cpp
    frame.movement = std::move(movement);
    state.movement.frame = frame.movement.frame;
    state.movement.movementReservations = frame.movement.movementReservations;
    for (const auto& [unitId, decision] : frame.movement.decisions)
```

with:

```cpp
    state.movement.frame = movement.frame;
    state.movement.movementReservations = movement.movementReservations;
    for (const auto& [unitId, decision] : movement.decisions)
```

Replace the second movement decision loop:

```cpp
    for (const auto& [unitId, decision] : frame.movement.decisions)
```

with:

```cpp
    for (const auto& [unitId, decision] : movement.decisions)
```

At the end of the function, add:

```cpp
    return movement;
```

- [ ] **Step 4: Make motion frame return movement**

Change `advanceMotionFrame(...)` signature from:

```cpp
void advanceMotionFrame(BattleRuntimeState& state, BattleFrameContext& frame)
```

to:

```cpp
BattleTickResult advanceMotionFrame(BattleRuntimeState& state)
```

Replace the end of the function:

```cpp
    commitFrameMovement(state, frame, physicsResults, std::move(movement));
```

with:

```cpp
    return commitFrameMovement(state, physicsResults, std::move(movement));
```

- [ ] **Step 5: Pass movement into action advancement**

Change `advanceActionFrameUnits(...)` signature from:

```cpp
void advanceActionFrameUnits(BattleRuntimeState& state, BattleFrameContext& frame)
```

to:

```cpp
void advanceActionFrameUnits(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleTickResult& movement)
```

Inside the function, delete:

```cpp
    const auto& movement = frame.movement;
```

In `BattleFrameRunner::runFrame(...)`, replace:

```cpp
    advanceMotionFrame(state, frame);
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(state, frame);
```

with:

```cpp
    auto movement = advanceMotionFrame(state);
    // Start or commit unit actions, e.g. cast startup, attack spawn requests, blink teleports, action sounds.
    advanceActionFrameUnits(state, frame, movement);
```

- [ ] **Step 6: Update frame context comment**

Replace the comment above `BattleFrameContext`:

```cpp
// One runFrame() transaction ledger. Persistent gameplay lives in BattleRuntimeState.
// The only public frame output is result; every other member is frame-local scratch.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
```

with:

```cpp
// Private runFrame() output accumulator. Persistent gameplay lives in BattleRuntimeState.
// Keep this type private to BattleCore.cpp and do not pass it to subsystem classes.
```

- [ ] **Step 7: Verify slice 6**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][movement],[battle][scenario][runtime]"
rg -n "frame\.movement|BattleTickResult movement;" src\battle\BattleCore.cpp tests
```

Expected: diff check passes, build succeeds, focused tests pass, and `rg` returns no matches.

- [ ] **Step 8: Commit slice 6**

Run:

```powershell
git add src\battle\BattleCore.cpp
git commit -m "refactor: pass battle movement frame explicitly"
```

---

## Task 7: Final Verification and Residual Complexity Check

**Files:**
- Verify all files changed by Tasks 1-6

- [ ] **Step 1: Confirm removed channels stay removed**

Run:

```powershell
rg -n "BattleProjectileCancelDamageCommand|frame\.effectCommands|effectCommands|frame\.teamEffectEvents|teamEffectEvents|frame\.runtimeCommits|frame\.deferredCommands|frame\.movement" src\battle\BattleCore.cpp src\battle\BattleHitResolver.h tests
```

Expected: no matches.

- [ ] **Step 2: Confirm `BattleFrameContext` is smaller and still private**

Run:

```powershell
rg -n -C 18 "struct BattleFrameContext" src\battle\BattleCore.cpp
rg -n "BattleFrameContext" src\battle\BattleCore.h tests
```

Expected:
- `BattleFrameContext` contains `result`, `frameCommands`, presentation/log/event accumulators, `attackEvents`, and `frameStartMotion`.
- `BattleFrameContext` has no matches in `src\battle\BattleCore.h` or tests.

- [ ] **Step 3: Run full verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Target kys_tests
x64\Debug\kys_tests.exe "[battle][scene_frame_delta],[battle][scene_impact],[battle][presentation]"
x64\Debug\kys_tests.exe "[battle][scenario][runtime]"
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][hit_resolver],[battle][movement]"
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: diff check exits 0, focused tests pass, full tests pass, and the debug build completes. A final link failure is acceptable only if caused by the game executable already running.

- [ ] **Step 4: Record the next complexity candidates**

After this plan passes, inspect but do not implement these in the same branch:

```powershell
rg -n "BattleFrameActionUnitResult|BattleTeamEffectCommandApplication|collectFrameCastScopedComboCommands|applyBattleTeamEffectCommand|makeBattleDamageRuntimeUnit|pendingAttackSpawns" src\battle tests
```

Expected follow-up candidates:
- Replace local `BattleFrameActionUnitResult` staging inside `advanceActionFrameUnits(...)` with direct locals where it deletes branches instead of just renaming fields.
- Move tests off direct `BattleCore.h` helper calls so `applyBattleTeamEffectCommand(...)`, `collectFrameCastScopedComboCommands(...)`, and `makeBattleDamageRuntimeUnit(...)` can stop being public battle-core declarations.
- Revisit `state.pendingAttackSpawns` after command/frame staging is thinner; it is still a cross-phase queue from action/death/rescue to attack spawning.

