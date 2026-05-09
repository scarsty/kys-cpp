# Battle Migration Artifact Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the remaining migration-shaped battle boundaries: mutable setup runtime access, split action sound output, and duplicated movement presentation application.

**Architecture:** Keep `BattleRuntimeSession` as the runtime owner. The scene and adapter may build pure setup/application inputs from legacy `Role` and `Magic`, but they must not receive a mutable `BattleRuntimeState&`; frame presentation must consume explicit runtime output.

**Tech Stack:** C++20, MSVC, Catch2 tests, existing `BattleRuntimeSession`, `BattleFrameRunner`, `BattleSceneBattleAdapter`, and `BattleSceneHades`.

---

## File Structure

- Modify `src/battle/BattleRuntimeSession.h`
  - Add a narrow one-shot setup configuration command.
  - Remove `runtimeForSetupConfiguration()`.
  - Track setup configuration as an invariant before `runFrame()`.

- Modify `src/battle/BattleRuntimeSession.cpp`
  - Implement `commitSetupConfiguration(...)`.
  - Assert on repeated setup configuration and configuration after `runFrame()`.

- Modify `src/BattleSceneBattleAdapter.h`
  - Replace public mutable-runtime configuration function shape with a function that builds and submits pure setup configuration.
  - Remove `BattleActionFrameApplyResult::attackSoundIds`.
  - Replace two movement apply helpers with one presentation movement apply helper.

- Modify `src/BattleSceneBattleAdapter.cpp`
  - Build setup configuration from const runtime state plus legacy role projections, then call `BattleRuntimeSession::commitSetupConfiguration(...)`.
  - Stop deriving normal attack sound ids from legacy `Magic` during frame apply.
  - Apply movement presentation through one helper.

- Modify `src/BattleSceneHades.cpp`
  - Call the renamed setup configuration function.
  - Remove the adapter action sound loop.
  - Replace the two movement apply calls with one call.

- Modify `src/battle/BattleCastSystem.h` and `src/battle/BattleCastSystem.cpp`
  - Carry the selected skill `soundId` in runtime cast output.

- Modify `src/battle/BattleCore.h` and `src/battle/BattleCore.cpp`
  - Emit normal action sound ids through `BattleFrameApplications::attackSoundIds`.
  - Optionally add a single movement presentation result payload if replacing the two existing frame movement payloads in one batch.

- Modify tests:
  - `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
  - `tests/BattleCoreUnitTests.cpp`

---

## Task 1: Replace Mutable Setup Runtime Escape With One-Shot Session Command

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Add failing tests for setup configuration ownership**

Add tests that prove setup configuration is a one-shot session command and cannot run after frames start.

```cpp
TEST_CASE("BattleRuntimeSession_CommitSetupConfigurationAppliesOwnedRuntimeSetup", "[battle][runtime_session][ownership]")
{
    BattleRuntimeInit init;
    init.runtime = ownedRuntimeState();
    BattleRuntimeSession session(std::move(init));

    BattleRuntimeSetupConfiguration config;
    config.world.frame = 42;
    config.world.config = BattleMovementConfig{};
    config.world.units = {
        runtimeUnit(1, 0, { 128, 256, 0 }),
    };
    config.attacks.nextAttackId = 90;
    config.action.castFrames = { 1, 2, 3, 4 };
    config.damage.aggregatePendingTransactionsByDefender = true;
    config.result.pendingAliveByTeam.emplace(1, 2);

    session.commitSetupConfiguration(std::move(config));

    CHECK(session.runtime().world.frame == 42);
    REQUIRE(session.runtime().world.units.size() == 1);
    CHECK(session.runtime().world.units[0].id == 1);
    CHECK(session.runtime().attacks.nextAttackId == 90);
    CHECK(session.runtime().action.castFrames.size() == 4);
    CHECK(session.runtime().damage.aggregatePendingTransactionsByDefender);
    CHECK(session.runtime().result.pendingAliveByTeam.at(1) == 2);
}

TEST_CASE("BattleRuntimeSession_CommitSetupConfigurationAssertsAfterFrameStart", "[battle][runtime_session][ownership]")
{
    BattleRuntimeInit init;
    init.runtime = ownedRuntimeState();
    BattleRuntimeSession session(std::move(init));

    session.runFrame();

    BattleRuntimeSetupConfiguration config;
    CHECK_THROWS_AS(session.commitSetupConfiguration(std::move(config)), std::exception);
}
```

If this project does not convert `assert` into Catch exceptions in the active test build, replace the second test with the project’s existing debug-assert helper pattern used for setup placement.

- [ ] **Step 2: Run the focused test build and verify the compile failure**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile failure because `BattleRuntimeSetupConfiguration` and `commitSetupConfiguration(...)` do not exist.

- [ ] **Step 3: Add the pure setup configuration type and session command**

In `src/battle/BattleRuntimeSession.h`, add a runtime-owned setup configuration payload. Keep this type in core, not in the adapter.

```cpp
struct BattleRuntimeSetupConfiguration
{
    BattleWorldState world;
    BattleAttackWorld attacks;
    BattleDamageApplicationState damage;
    BattleRuntimeState::BattleResultState result;
    BattleRuntimeState::BattleTeamEffectState teamEffects;
    BattleRuntimeState::BattleRescueState rescue;
    BattleRuntimeState::BattleMovementPhysicsRuntime movementPhysics;
    BattleRuntimeState::BattleActionRuntime action;
    BattleProjectileFollowUpContext projectileFollowUps;
    BattleDeathEffectStore deathEffects;
};
```

In `BattleRuntimeSession`, add:

```cpp
void commitSetupConfiguration(BattleRuntimeSetupConfiguration config);
```

Add a private member:

```cpp
bool setupConfigured_ = false;
```

- [ ] **Step 4: Implement the session command and remove the mutable runtime accessor**

In `src/battle/BattleRuntimeSession.cpp`, implement:

```cpp
void BattleRuntimeSession::commitSetupConfiguration(BattleRuntimeSetupConfiguration config)
{
    assert(!setupConfigured_);
    assert(!frameStarted_);
    setupConfigured_ = true;

    runtime_.world = std::move(config.world);
    runtime_.attacks = std::move(config.attacks);
    runtime_.damage = std::move(config.damage);
    runtime_.result = std::move(config.result);
    runtime_.teamEffects = std::move(config.teamEffects);
    runtime_.rescue = std::move(config.rescue);
    runtime_.movementPhysics = std::move(config.movementPhysics);
    runtime_.action = std::move(config.action);
    runtime_.projectileFollowUps = std::move(config.projectileFollowUps);
    runtime_.deathEffects.store = std::move(config.deathEffects);
}
```

Delete:

```cpp
BattleRuntimeState& runtimeForSetupConfiguration();
```

- [ ] **Step 5: Change the adapter to build configuration without mutable runtime access**

Rename the adapter function to make ownership explicit:

```cpp
void commitInitializedBattleRuntimeConfiguration(
    Battle::BattleRuntimeSession& session,
    const BattleRuntimeBuildContext& context,
    const std::unordered_map<int, Role*>& rolesByBattleId);
```

In the implementation, replace:

```cpp
auto& runtime = session.runtimeForSetupConfiguration();
```

with:

```cpp
const auto& runtime = session.runtime();
Battle::BattleRuntimeSetupConfiguration config;
```

Build `config` using the existing projection logic, then call:

```cpp
session.commitSetupConfiguration(std::move(config));
```

Use `runtime.units`, `runtime.combo`, and `runtime.movementRuntime` as const source data only. Do not call any mutable accessor on the session.

- [ ] **Step 6: Update `BattleSceneHades` call site**

Replace:

```cpp
KysChess::BattleSceneBattleAdapter::configureInitializedBattleRuntimeState(
    *battle_session_,
    buildContext,
    core_role_bindings_.rolesByBattleId);
```

with:

```cpp
KysChess::BattleSceneBattleAdapter::commitInitializedBattleRuntimeConfiguration(
    *battle_session_,
    buildContext,
    core_role_bindings_.rolesByBattleId);
```

- [ ] **Step 7: Run focused boundary search**

Run:

```powershell
rg -n "runtimeForSetupConfiguration|commitInitializedBattleRuntimeConfiguration|configureInitializedBattleRuntimeState" src\battle src\BattleSceneBattleAdapter.* src\BattleSceneHades.cpp
```

Expected:

```text
src\BattleSceneBattleAdapter.cpp:<line>:void commitInitializedBattleRuntimeConfiguration(...)
src\BattleSceneBattleAdapter.cpp:<line>:    session.commitSetupConfiguration(std::move(config));
src\BattleSceneBattleAdapter.h:<line>:void commitInitializedBattleRuntimeConfiguration(...)
src\BattleSceneHades.cpp:<line>:    KysChess::BattleSceneBattleAdapter::commitInitializedBattleRuntimeConfiguration(
```

There must be no `runtimeForSetupConfiguration` match.

---

## Task 2: Move Normal Action Sounds Into Runtime Frame Applications

**Files:**
- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a failing runtime test for normal action sound output**

Add a test near the existing action commit tests in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFrameRunner_ActionCommitPublishesNormalAttackSound", "[battle][core][action]")
{
    auto state = frameRunnerActionRuntimeState();
    state.action.castFrames = { 1, 1, 1, 1 };
    state.action.planSeeds[1].normalSkill.id = 301;
    state.action.planSeeds[1].normalSkill.soundId = 55;
    state.action.planSeeds[1].normalSkill.attackAreaType = 1;
    state.action.planSeeds[1].normalSkill.selectDistance = 5;
    state.action.planSeeds[1].normalSkill.visualEffectId = 44;

    auto input = makeCommittedActionInput(1, 2);
    input.hasCast = true;
    input.cast.decision.canCast = true;
    input.cast.decision.skillId = 301;
    input.cast.decision.soundId = 55;
    state.action.pendingCommitInputs.emplace(1, input);
    state.units.requireUnit(1).haveAction = true;
    state.units.requireUnit(1).actFrame = 1;
    state.units.requireUnit(1).operationType = BattleOperationType::RangedProjectile;

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(result.applications.attackSoundIds.size() == 1);
    CHECK(result.applications.attackSoundIds[0] == 55);
}
```

- [ ] **Step 2: Run focused tests and verify failure**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_ActionCommitPublishesNormalAttackSound"
```

Expected: compile failure or test failure because runtime cast decisions do not yet carry sound ids into frame applications.

- [ ] **Step 3: Add `soundId` to runtime cast decisions**

In `src/battle/BattleCastSystem.h`, extend `BattleCastDecision`:

```cpp
int soundId = -1;
```

In `BattleCastPlanner::plan(...)` and `BattleCastPlanner::commitSelectedCast(...)`, set:

```cpp
result.decision.soundId = selectedSkill.soundId;
```

Use the already selected `BattleCastSkillState`; do not look up `Magic` from runtime.

- [ ] **Step 4: Emit normal action sounds from `BattleFrameRunner`**

In `src/battle/BattleCore.cpp`, inside the action commit path after:

```cpp
result.actionResult = BattleActionCommitSystem().commit(result.actionInput);
```

add:

```cpp
if (result.actionInput.hasCast && result.actionInput.cast.decision.soundId >= 0)
{
    applications.attackSoundIds.push_back(result.actionInput.cast.decision.soundId);
}
```

Do not add this to the auto-ultimate path if that path already appends `castInput.ultimateSkill.soundId`, or the same sound can play twice.

- [ ] **Step 5: Remove adapter-owned action sound output**

In `src/BattleSceneBattleAdapter.h`, remove:

```cpp
std::vector<int> attackSoundIds;
```

from `BattleActionFrameApplyResult`.

In `src/BattleSceneBattleAdapter.cpp`, remove:

```cpp
result.attackSoundIds.push_back(magic->SoundID);
```

In `src/BattleSceneHades.cpp`, remove:

```cpp
for (int soundId : actionApply.attackSoundIds)
{
    Audio::getInstance()->playASound(soundId);
}
```

Keep the existing `applyCoreFrameApplications(...)` loop over `frameResult.applications.attackSoundIds`; that becomes the only attack sound playback path.

- [ ] **Step 6: Run sound boundary search**

Run:

```powershell
rg -n "attackSoundIds|decision\\.soundId|Magic->SoundID|magic->SoundID" src\battle src\BattleSceneBattleAdapter.* src\BattleSceneHades.cpp tests
```

Expected:

- `BattleFrameApplications::attackSoundIds` remains.
- `BattleCastDecision::soundId` remains.
- Runtime appends attack sound ids.
- `BattleSceneBattleAdapter` no longer appends `magic->SoundID` to frame apply results.

---

## Task 3: Collapse Movement Presentation Into One Frame Apply Surface

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add a failing test for combined movement presentation output**

Add a test in `tests/BattleCoreUnitTests.cpp`:

```cpp
TEST_CASE("BattleFrameRunner_PublishesCombinedMovementPresentationState", "[battle][core][movement]")
{
    auto state = basicFrameRunnerState();
    state.world.units = {
        runtimeUnit(1, 0, { 10, 20, 0 }),
    };
    state.movementPhysics.results = {
        BattleFrameMovementPhysicsUnitResult{
            .unitId = 1,
            .state = {
                .position = { 30, 40, 0 },
                .velocity = { 2, 0, 0 },
                .acceleration = { 0, 0, -1 },
            },
            .frozenFrames = 3,
        },
    };

    auto result = BattleFrameRunner().runFrame(state);

    REQUIRE(result.movementPresentationResults.size() == 1);
    CHECK(result.movementPresentationResults[0].unitId == 1);
    CHECK(result.movementPresentationResults[0].position.x == 30.0f);
    CHECK(result.movementPresentationResults[0].velocity.x == 2.0f);
    CHECK(result.movementPresentationResults[0].acceleration.z == -1.0f);
    CHECK(result.movementPresentationResults[0].frozenFrames == 3);
}
```

- [ ] **Step 2: Run focused tests and verify failure**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleFrameRunner_PublishesCombinedMovementPresentationState"
```

Expected: compile failure because `movementPresentationResults` does not exist.

- [ ] **Step 3: Add a single movement presentation payload**

In `src/battle/BattleCore.h`, add:

```cpp
struct BattleFrameMovementPresentationUnitResult
{
    int unitId = -1;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    int frozenFrames = 0;
};
```

In `BattleFrameResult`, add:

```cpp
std::vector<BattleFrameMovementPresentationUnitResult> movementPresentationResults;
```

- [ ] **Step 4: Populate the combined payload after movement and physics systems run**

In `src/battle/BattleCore.cpp`, add a helper:

```cpp
void publishMovementPresentationResults(BattleFrameResult& result)
{
    result.movementPresentationResults.clear();
    result.movementPresentationResults.reserve(result.movement.snapshot.units.size());

    for (const auto& unit : result.movement.snapshot.units)
    {
        BattleFrameMovementPresentationUnitResult presentation;
        presentation.unitId = unit.id;
        presentation.position = unit.position;
        presentation.velocity = unit.velocity;
        presentation.facing = unit.velocity;
        if (presentation.facing.norm() > 0.01)
        {
            presentation.facing.normTo(1);
        }
        result.movementPresentationResults.push_back(presentation);
    }

    for (const auto& physics : result.movementPhysicsResults)
    {
        auto& presentation = requireById(result.movementPresentationResults, physics.unitId);
        presentation.position = physics.state.position;
        presentation.velocity = physics.state.velocity;
        presentation.acceleration = physics.state.acceleration;
        presentation.frozenFrames = physics.frozenFrames;
        if (presentation.velocity.norm() > 0.01)
        {
            presentation.facing = presentation.velocity;
            presentation.facing.normTo(1);
        }
    }
}
```

Call it in `BattleFrameRunner::runFrame(...)` after both `result.movement` and `result.movementPhysicsResults` have been produced and before returning `result`.

- [ ] **Step 5: Replace two adapter apply helpers with one**

In `src/BattleSceneBattleAdapter.h`, remove:

```cpp
void applyBattleMovementPhysicsFrameResults(...);
void applyBattleMovementFrameResults(...);
```

Add:

```cpp
void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::vector<Role*>& roles);
```

In `src/BattleSceneBattleAdapter.cpp`, implement:

```cpp
void applyBattleMovementPresentationResults(
    const std::vector<Battle::BattleFrameMovementPresentationUnitResult>& movementResults,
    const std::vector<Role*>& roles)
{
    for (const auto& result : movementResults)
    {
        auto* role = findRoleByBattleId(roles, result.unitId);
        role->Frozen = result.frozenFrames;
        role->Pos = result.position;
        role->Velocity = result.velocity;
        role->Acceleration = result.acceleration;
        if (result.facing.norm() > 0.01)
        {
            role->RealTowards = result.facing;
        }
    }
}
```

- [ ] **Step 6: Update `BattleSceneHades`**

Replace:

```cpp
applyBattleMovementFrameResults(frameResult.movement, battle_roles_);
applyBattleMovementPhysicsFrameResults(frameResult.movementPhysicsResults, battle_roles_);
```

with:

```cpp
applyBattleMovementPresentationResults(frameResult.movementPresentationResults, battle_roles_);
```

Update the `using` declarations at the top of `BattleSceneHades.cpp`.

- [ ] **Step 7: Remove obsolete frame movement fields if no longer used**

Run:

```powershell
rg -n "frameResult\\.movement|movementPhysicsResults|applyBattleMovementFrameResults|applyBattleMovementPhysicsFrameResults" src tests
```

If `BattleFrameResult::movement` and `BattleFrameResult::movementPhysicsResults` are only used to populate `movementPresentationResults` and tests can be migrated, remove them from `BattleFrameResult`. Keep local variables inside `BattleFrameRunner::runFrame(...)` for internal sequencing instead of returning both intermediate payloads.

---

## Task 4: Full Verification and Boundary Searches

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

- [ ] **Step 3: Run unit tests**

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

Expected: MSBuild exits with code `0`. Existing warning noise is acceptable; link failure is acceptable only if the executable is locked by a running game process.

- [ ] **Step 5: Run migration artifact boundary searches**

Run:

```powershell
rg -n "runtimeForSetupConfiguration|runtimeForTests|enqueueAttackSpawn" src tests
rg -n "BattleActionFrameApplyResult|attackSoundIds|magic->SoundID" src\BattleSceneBattleAdapter.* src\BattleSceneHades.cpp src\battle
rg -n "applyBattleMovementFrameResults|applyBattleMovementPhysicsFrameResults|frameResult\.movement|movementPhysicsResults" src tests
```

Expected:

- No session mutable runtime escape hatch remains.
- Adapter frame apply no longer derives attack sound ids from `Magic`.
- Scene has one movement presentation apply call.
- If `movement` or `movementPhysicsResults` remain in core, they are local/internal or test-only with a documented reason in the code review notes for the next cleanup batch.

---

## Self-Review

- Spec coverage: The plan covers all three review findings.
- Placeholder scan: No open-ended implementation placeholders remain; each task names files, intended code shape, and verification commands.
- Type consistency: `BattleRuntimeSetupConfiguration`, `commitSetupConfiguration`, `BattleCastDecision::soundId`, and `BattleFrameMovementPresentationUnitResult` are introduced before later tasks use them.
