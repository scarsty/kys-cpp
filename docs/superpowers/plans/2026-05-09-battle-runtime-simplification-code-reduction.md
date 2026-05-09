# Battle Runtime Simplification Code Reduction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove stale migration surfaces and reduce duplicated battle-system code without changing the runtime/scene ownership design.

**Architecture:** Keep frame advancement, runtime maintenance, and combat decisions in `BattleRuntimeSession` / `BattleFrameRunner`. Keep `BattleSceneHades` and `BattleSceneBattleAdapter` as legacy presentation/setup bridges only. Favor deletion and local helper extraction over new broad abstractions.

**Tech Stack:** C++20, Visual Studio solution `kys.sln`, doctest-based `kys_tests`, PowerShell build wrapper `.github\build-command.ps1`.

---

## Files

- Modify: `src/battle/BattleCore.h`
  - Remove stale public frame directive DTOs and remove `BattleFrameResult::commands`.
  - Add only small helper declarations if they are needed by tests or other battle core files.
- Modify: `src/battle/BattleCore.cpp`
  - Remove directive-driven branches from `advanceActionFrameUnits`.
  - Convert frame command reduction to a local queue.
  - Add append helpers for damage transaction/presentation alignment.
  - Add local generic find-by-id helpers.
- Modify: `src/BattleSceneBattleAdapter.cpp`
  - Replace repeated `Role` projection mappings with a small projection helper class.
- Modify: `src/BattleSceneBattleAdapter.h`
  - Remove declarations that become private implementation details if the projection refactor makes them unnecessary outside the adapter.
- Modify: `tests/BattleCoreUnitTests.cpp`
  - Remove tests that exist only to exercise deleted directive APIs.
  - Rewrite remaining behavior tests through runtime plan seeds, pending commit inputs, or direct subsystem tests.
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - Remove assertions against `BattleFrameResult::commands`.
  - Keep assertions that runtime queues are fully reduced by the frame runner.
- Run: `git diff --check`
- Run: `.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal`
- Run: `x64\Debug\kys_tests.exe`
- Run: `.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal`

---

## Task 1: Remove Returned Frame Command Queue

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Find all `BattleFrameResult::commands` callers**

Run:

```powershell
rg -n "result\.commands|frameResult\.commands|BattleFrameResult.*commands|\.commands\.empty\(\)" src tests
```

Expected: production use should be inside `src/battle/BattleCore.cpp`; scene and adapter should not consume `BattleFrameResult::commands`.

- [ ] **Step 2: Remove the field**

In `src/battle/BattleCore.h`, delete the migration fallback field from `BattleFrameResult`:

```cpp
// Remove this field completely:
// std::vector<BattleGameplayCommand> commands;
```

- [ ] **Step 3: Convert `runFrame` to local command queues**

In `src/battle/BattleCore.cpp`, make `BattleFrameRunner::runFrame` own the queue:

```cpp
BattleFrameResult BattleFrameRunner::runFrame(BattleRuntimeState& state) const
{
    assertFrameMovementConfigConfigured(state.world.config);
    assertFrameAttackWorldConfigured(state.attacks);

    BattleFrameResult result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<BattleGameplayCommand> deferredCommands;

    clearBattleDamageFrameOutputs(state);
    state.status.events.clear();
    state.combo.events.clear();
    state.deathEffects.events.clear();
    state.teamEffects.committedEvents.clear();
    state.effects.committedCommands.clear();
    state.rescue.committedResults.clear();
    advanceStatus(state);
    advanceRuntimeUnits(state, frameCommands, result.runtimeResults);
    applyRuntimeComboEvents(state, result.runtimeResults, deferredCommands, logEvents);
    applyPendingTeamEffects(state, logEvents);
    reduceFrameGameplayCommands(state, frameCommands, result.applications, gameplayEvents, logEvents, visualEvents);
    if (hasCanonicalUnitStore(state))
    {
        refreshMovementWorldFromRuntimeUnits(state);
    }
    result.movementPhysicsResults = advanceMovementPhysics(state);
    advanceMovement(state, result);
    advanceActionFrameUnits(
        state,
        result.movement,
        result.actionResults,
        gameplayEvents,
        logEvents,
        visualEvents);
    advanceAttacksAndResolveHits(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
    syncRescueStateFromRuntimeUnits(state);
    applyDamageAndLifecycle(state, result, frameCommands, gameplayEvents, logEvents, visualEvents);
    frameCommands.insert(
        frameCommands.end(),
        std::make_move_iterator(deferredCommands.begin()),
        std::make_move_iterator(deferredCommands.end()));
    reduceFrameGameplayCommands(state, frameCommands, result.applications, gameplayEvents, logEvents, visualEvents);
    assert(frameCommands.empty());
    publishFrameApplyOutputs(state, result);
    emitPresentationFrame(state, result, gameplayEvents, logEvents, visualEvents);
    pruneFinishedRuntimeAttacks(state);
    return result;
}
```

Also update helper signatures that currently write to `result.commands`:

```cpp
void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents);

void applyDamageAndLifecycle(
    BattleRuntimeState& state,
    BattleFrameResult& result,
    std::vector<BattleGameplayCommand>& frameCommands,
    std::vector<BattleGameplayEvent>& gameplayEvents,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents);
```

Within those helpers, replace `result.commands` with `frameCommands`.

- [ ] **Step 4: Add an explicit pruning helper while editing the frame tail**

Extract the existing erase block into a named local helper in `BattleCore.cpp`:

```cpp
void pruneFinishedRuntimeAttacks(BattleRuntimeState& state)
{
    state.attacks.attacks.erase(
        std::remove_if(
            state.attacks.attacks.begin(),
            state.attacks.attacks.end(),
            [](const BattleAttackInstance& attack)
            {
                return attack.frame >= attack.state.totalFrame;
            }),
        state.attacks.attacks.end());
}
```

- [ ] **Step 5: Update tests**

Remove assertions that only check `result.commands.empty()`.

For tests that search `result.commands` to prove a command was reduced, replace that with state/result assertions. Example replacement for hit damage reduction:

```cpp
REQUIRE(result.damageTransactions.size() == 1);
CHECK(result.damageTransactions.front().defender.id == 2);
CHECK(result.damageTransactions.front().finalHpDamage > 0);
CHECK(state.damage.pendingTransactions.empty());
```

- [ ] **Step 6: Run focused verification**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: build succeeds and all tests pass.

---

## Task 2: Remove Dead Frame Directive API

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Confirm production no longer writes directives**

Run:

```powershell
rg -n "action\.directives|BattleFrameActionUnitInput|canPlanCast|hasSelectedCastInput|hasPendingActionInput|selectedActionInput|pendingActionInput|selectedCastInput" src tests
```

Expected: production references should be limited to `BattleCore.h`/`BattleCore.cpp`; non-test scene/adapter code should not construct directives.

- [ ] **Step 2: Delete stale public DTO**

Remove `BattleFrameActionUnitInput` from `src/battle/BattleCore.h` and remove this field from `BattleRuntimeState::ActionState`:

```cpp
std::vector<BattleFrameActionUnitInput> directives;
```

- [ ] **Step 3: Simplify `advanceActionFrameUnits` to runtime-owned inputs**

Remove:

```cpp
std::unordered_map<int, const BattleFrameActionUnitInput*> inputsByUnitId;
```

Remove branches that depend on:

```cpp
input->canPlanCast
input->hasSelectedCastInput
input->hasPendingActionInput
```

Keep only:

```cpp
auto pendingCommitIt = state.action.pendingCommitInputs.find(unit.id);
std::optional<BattleCastInput> runtimeCastPlan;
const BattleCastInput* castPlanInput = nullptr;
auto runtimePlanSeedIt = state.action.planSeeds.find(unit.id);
bool usingRuntimeCastPlan = false;

if (runtimePlanSeedIt != state.action.planSeeds.end()
    && !unit.haveAction
    && !actionMovementDashActive(state, unit.id))
{
    runtimeCastPlan = makeRuntimeCastInputFromSeed(
        state,
        unit,
        runtimePlanSeedIt->second,
        unit.cooldown == 0,
        false,
        false);
    castPlanInput = &*runtimeCastPlan;
    usingRuntimeCastPlan = true;
}
```

For pending commits, commit only from `state.action.pendingCommitInputs`.

At the end of the loop, push an action result only when:

```cpp
if (wasActionActive || result.castStarted || result.actionCommitted)
{
    actionResults.push_back(std::move(result));
}
```

Remove:

```cpp
state.action.directives.clear();
```

- [ ] **Step 4: Rewrite or delete directive-only tests**

Delete tests whose only purpose is selected/pending directive injection. Preserve coverage for runtime-owned behavior:

```cpp
state.action.planSeeds.emplace(1, BattleActionPlanSeed{
    .unitId = 1,
    .hasEquippedSkill = false,
    .normalSkill = BattleActionSkillSeed{
        .id = 101,
        .name = "測試招式",
        .attackAreaType = 0,
        .magicType = 0,
        .selectDistance = 1,
        .magicPower = 100,
    },
});
```

For pending commit behavior, seed:

```cpp
state.action.pendingCommitInputs.emplace(1, frameActionCommitInput());
state.units.requireUnit(1).haveAction = true;
state.units.requireUnit(1).operationType = BattleOperationType::Melee;
state.units.requireUnit(1).actFrame = actionCastFrameForTest;
```

Do not add back any test-only production API.

- [ ] **Step 5: Run focused verification**

Run:

```powershell
rg -n "BattleFrameActionUnitInput|action\.directives|hasSelectedCastInput|hasPendingActionInput|canPlanCast" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: search returns no matches; tests pass.

---

## Task 3: Consolidate Damage Transaction Append Logic

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp` only if assertions need naming updates.

- [ ] **Step 1: Add append helpers near `buildFrameDamageTransaction`**

Add:

```cpp
void appendDamagePendingPresentationPlaceholder(BattleRuntimeState& state)
{
    if (!state.damage.pendingPresentation.empty())
    {
        state.damage.pendingPresentation.push_back({});
    }
}

void appendFrameDamageTransaction(
    BattleRuntimeState& state,
    BattleDamageTransactionInput transaction,
    std::optional<BattleDamagePresentationInput> presentation)
{
    state.damage.pendingTransactions.push_back(std::move(transaction));
    if (presentation)
    {
        while (state.damage.pendingPresentation.size() + 1 < state.damage.pendingTransactions.size())
        {
            state.damage.pendingPresentation.push_back({});
        }
        state.damage.pendingPresentation.push_back(std::move(*presentation));
        return;
    }
    appendDamagePendingPresentationPlaceholder(state);
}
```

Add `#include <optional>` if it is not already available through the current includes.

- [ ] **Step 2: Extract HP damage presentation builder**

Add:

```cpp
std::optional<BattleDamagePresentationInput> makeFrameDamagePresentation(
    const BattleRuntimeState& state,
    const BattleHpDamageCommand& command)
{
    const auto styleIt = state.damage.presentationStylesByDefender.find(command.targetUnitId);
    if (styleIt == state.damage.presentationStylesByDefender.end())
    {
        return std::nullopt;
    }

    BattleDamagePresentationInput presentation;
    presentation.enabled = true;
    presentation.critical = command.critical;
    presentation.ultimate = command.ultimate;
    presentation.executed = command.executed;
    presentation.skillName = command.skillName;
    presentation.detailText = command.detailText;
    presentation.normalDamageColor = styleIt->second.normalDamageColor;
    presentation.emphasizedDamageColor = styleIt->second.emphasizedDamageColor;
    presentation.executeTextColor = styleIt->second.executeTextColor;
    presentation.normalDamageTextSize = styleIt->second.normalDamageTextSize;
    presentation.emphasizedDamageTextSize = styleIt->second.emphasizedDamageTextSize;
    presentation.executeTextSize = styleIt->second.executeTextSize;
    return presentation;
}
```

- [ ] **Step 3: Rewrite the three overloads**

For HP:

```cpp
appendFrameDamageTransaction(
    state,
    std::move(transaction),
    makeFrameDamagePresentation(state, command));
return true;
```

For MP and accepted side effects:

```cpp
appendFrameDamageTransaction(state, std::move(transaction), std::nullopt);
return true;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
x64\Debug\kys_tests.exe --test-case="*Damage*"
```

If the test runner does not accept the filter syntax, run the full executable:

```powershell
x64\Debug\kys_tests.exe
```

Expected: damage presentation ordering and transaction aggregation tests still pass.

---

## Task 4: Collapse Local Find-By-Id Boilerplate

**Files:**
- Modify: `src/battle/BattleCore.cpp`

- [ ] **Step 1: Add local helpers in the anonymous namespace**

Near other local helpers, add:

```cpp
template<typename Unit>
auto* findById(std::vector<Unit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const Unit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

template<typename Unit>
const auto* findById(const std::vector<Unit>& units, int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const Unit& unit)
        {
            return unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

BattleFrameRescueUnitSnapshot* findRescueSnapshotById(
    std::vector<BattleFrameRescueUnitSnapshot>& units,
    int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleFrameRescueUnitSnapshot& unit)
        {
            return unit.unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}

const BattleFrameRescueUnitSnapshot* findRescueSnapshotById(
    const std::vector<BattleFrameRescueUnitSnapshot>& units,
    int unitId)
{
    auto it = std::find_if(units.begin(), units.end(), [&](const BattleFrameRescueUnitSnapshot& unit)
        {
            return unit.unit.id == unitId;
        });
    return it != units.end() ? &*it : nullptr;
}
```

- [ ] **Step 2: Replace redundant helper implementations**

Replace bodies or call sites for:

```cpp
findStatusUnit(state.status.units, unitId)
findWorldUnit(state.world, unitId)
findDamageRuntimeUnit(state.damage.unitExtras, unitId)
findRescueUnit(state.rescue.units, unitId)
```

Use:

```cpp
findById(state.status.units, unitId)
findById(state.world.units, unitId)
findById(state.damage.unitExtras, unitId)
findRescueSnapshotById(state.rescue.units, unitId)
```

Delete the old repeated helper definitions after all call sites are migrated.

- [ ] **Step 3: Run compile check**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: build succeeds.

---

## Task 5: Centralize Adapter Role Projection

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h` only if public declarations become private.
- Modify: `tests/BattleInitializationUnitTests.cpp` only if setup assertions need updates.

- [ ] **Step 1: Replace free snapshot projection with a small class**

In `src/BattleSceneBattleAdapter.cpp`, replace `RoleBattleSnapshot makeRoleBattleSnapshot(Role*)` usage with:

```cpp
class RoleBattleProjection
{
public:
    explicit RoleBattleProjection(Role* role)
        : role_(role)
    {
        assert(role_);
    }

    int id() const { return role_->ID; }
    int realRoleId() const { return role_->RealID; }
    const std::string& name() const { return role_->Name; }
    int team() const { return role_->Team; }
    bool alive() const { return role_->Dead == 0; }
    int hp() const { return role_->HP; }
    int maxHp() const { return role_->MaxHP; }
    int mp() const { return role_->MP; }
    int maxMp() const { return role_->MaxMP; }
    int attack() const { return role_->Attack; }
    int defence() const { return role_->Defence; }
    int speed() const { return role_->Speed; }
    int star() const { return role_->Star; }
    int invincible() const { return role_->Invincible; }
    int hurtFrame() const { return role_->HurtFrame; }
    int frozen() const { return role_->Frozen; }
    int frozenMax() const { return role_->FrozenMax; }
    Pointf position() const { return role_->Pos; }
    Pointf velocity() const { return role_->Velocity; }
    Pointf acceleration() const { return role_->Acceleration; }
    Pointf facing() const { return role_->RealTowards; }

    Battle::BattleUnitState worldUnit() const;
    Battle::BattleRuntimeUnit runtimeUnit(
        const RoleComboState* state,
        const Battle::BattleGridTransform& gridTransform) const;
    Battle::BattleStatusUnitState statusUnit(const RoleComboState& state) const;
    Battle::BattleDamageUnitState damageUnit(const RoleComboState* state) const;

private:
    Role* role_ = nullptr;
};
```

- [ ] **Step 2: Move repeated mappings into projection methods**

Example `worldUnit`:

```cpp
Battle::BattleUnitState RoleBattleProjection::worldUnit() const
{
    Battle::BattleUnitState unit;
    unit.id = id();
    unit.realRoleId = realRoleId();
    unit.name = name();
    unit.team = team();
    unit.alive = alive();
    unit.position = position();
    unit.velocity = velocity();
    unit.speed = speed() / ROLE_MOVE_SPEED_DIVISOR;
    unit.star = star();
    unit.canAttack = role_->CoolDown == 0;
    return unit;
}
```

Apply the same pattern to `runtimeUnit`, `statusUnit`, and `damageUnit`, keeping current field values exactly the same.

- [ ] **Step 3: Rewrite wrapper functions or call sites**

Keep public adapter APIs stable only where `BattleSceneHades` still calls them. Internal builders can become:

```cpp
Battle::BattleUnitState makeBattleWorldUnit(Role* role)
{
    return RoleBattleProjection(role).worldUnit();
}
```

For runtime/status/damage:

```cpp
RoleBattleProjection(role).runtimeUnit(state, context.rules.gridTransform);
RoleBattleProjection(role).statusUnit(stateIt->second);
RoleBattleProjection(role).damageUnit(statePtr);
```

- [ ] **Step 4: Run projection regression tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: battle initialization and frame runner tests pass.

---

## Task 6: Final Boundary and Build Verification

**Files:**
- Verify only unless previous tasks require fixes.

- [ ] **Step 1: Run code hygiene checks**

Run:

```powershell
git diff --check
```

Expected: no whitespace errors.

- [ ] **Step 2: Run boundary searches**

Run:

```powershell
rg -n "BattleFrameActionUnitInput|action\.directives|hasSelectedCastInput|hasPendingActionInput|canPlanCast|result\.commands|frameResult\.commands|BattleFrameResult.*commands" src tests
rg -n "battleRuntime\(\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected:
- First search: no matches.
- Second search: no matches.

- [ ] **Step 3: Build and run tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected:
- `kys_tests` build succeeds.
- `kys_tests.exe` reports all tests passing.
- `kys` build succeeds. If final linking fails because the game is running, treat compile as successful per project instructions and report the linker blocker explicitly.

---

## Implementation Notes

- Do not reintroduce scene access to `BattleRuntimeState`.
- Do not add compatibility overloads for deleted APIs.
- Use `assert` for invariants; do not hide invariant violations behind silent early returns.
- Keep Traditional Chinese in source text and tests.
- Prefer deletion first. Only add helpers that remove repeated code in the same task.

## Self-Review

- Finding 1 is covered by Task 2.
- Finding 2 is covered by Task 1.
- Finding 3 is covered by Task 5.
- Finding 4 is covered by Task 3.
- Finding 5 is covered by Task 4.
- Verification and boundary searches are covered by Task 6.
