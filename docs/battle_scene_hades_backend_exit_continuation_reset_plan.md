# BattleSceneHades Backend Exit Breakthrough Reset Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create the real one-frame breakthrough: `BattleFrameRunner::advanceFrame()` consumes gameplay produced during the frame and returns committed frame state, so `BattleSceneHades` and `BattleSceneBattleAdapter` only snapshot legacy inputs, apply committed deltas, and play presentation.

**Architecture:** `BattleFrameState` is the frame world. Hit resolution, gameplay-command reduction, damage application, lifecycle, rescue reposition, movement physics, action/cast progression, and battle result must complete inside one runner call before control returns to the scene. The adapter may translate between legacy `Role`/`Magic`/`Item` objects and headless frame snapshots, but it may not run core gameplay systems or interpret gameplay commands after the runner.

**Tech Stack:** C++23, existing `src/battle` systems, Catch2 tests under `tests`, MSBuild through `.github\build-command.ps1`.

---

## Why This Reset Exists

The previous continuation direction produced a false positive. It reduced the number of visible `BattleFrameRunner().advanceFrame(...)` calls, but still allowed the scene and adapter to run gameplay before and after the runner:

- pre-runner scene movement/timer mutation;
- adapter-side command variant dispatch;
- post-runner damage application;
- post-runner rescue reposition;
- post-runner auto-ultimate and projectile spawn side effects.

That model is incorrect. One runner call is only meaningful if the runner consumes the gameplay work produced by that frame before returning. Moving helpers from `BattleSceneHades` to `BattleSceneBattleAdapter` is not progress unless the helper is purely snapshot build or committed-delta application.

## Non-Negotiable Rules

- Execute tasks in order. Do not start scene cleanup before Task 1 and Task 2 prove the core pattern.
- Do not create a second orchestrator, sidecar frame runner, or callback-based gameplay router.
- Do not make `BattleSceneBattleAdapter` larger by moving scene gameplay into it. Adapter code may build snapshots and apply committed deltas only.
- Do not let `BattleGameplayCommand` cross the runner boundary for gameplay application. Commands may exist as internal core facts, but they must be reduced to `BattleFrameState` mutations, committed transactions, queued spawns, lifecycle events, or presentation facts before `advanceFrame()` returns.
- Do not add source-text tests that assert forbidden words are absent. Use search gates as manual verification evidence.
- Use Traditional Chinese for touched source comments and string literals.
- Do not add broad defensive no-op branches at function boundaries. Use `assert` for invariants.
- Drop backwards compatibility. Once a core path owns behavior, delete the duplicate scene/adapter path in the same task.
- Derive gameplay geometry from existing scene/core constants through snapshots or adapter-provided config. Do not hard-code alternate gameplay geometry.

## Target Frame Contract

`BattleSceneHades::backRun1()` must end at this shape:

```cpp
void BattleSceneHades::backRun1()
{
    auto frame = battle_adapter_.buildFrameBundle();
    auto result = KysChess::Battle::BattleFrameRunner().advanceFrame(frame.state);
    battle_adapter_.applyFrameResult(frame, result);
    presentation_player_.play(result.frame);
    cleanupVisualOnlyBattleFrameState(result);
}
```

The important part is not the exact helper names. The important part is ownership:

- `buildFrameBundle()` snapshots current legacy state into `BattleFrameState` and adapter lookup data.
- `advanceFrame()` owns all gameplay decisions and same-frame command reduction.
- `applyFrameResult()` writes committed deltas back to legacy objects without re-deciding gameplay.
- Presentation playback consumes presentation facts only.

## Breakthrough Definition

The first breakthrough is one full vertical slice:

```text
projectile hit
  -> hit resolution
  -> HP damage command/fact
  -> damage transaction
  -> HP/status/cooldown/combo state mutation
  -> death/lifecycle/battle-result update
  -> presentation/gameplay events
```

All of that must happen inside one `BattleFrameRunner::advanceFrame(state)` call.

An implementation has not achieved the breakthrough if:

- `BattleFrameRunner` returns `BattleHpDamageCommand` and the adapter applies it later;
- the scene calls `BattleDamageApplicationSystem` after the runner;
- death, rescue, or battle-end decisions happen after `advanceFrame()`;
- the adapter calls `BattleDamageApplicationSystem`, `BattleRescueRepositionSystem`, `BattleActionCommitSystem`, `BattleCastPlanner`, `BattleHitResolver`, `BattleComboTriggerSystem`, `BattleTeamEffectSystem`, or `BattleEffectDispatcher` for gameplay after the frame result exists.

## File Responsibilities

- `src/battle/BattleCore.h`
  - Owns `BattleFrameState`, `BattleFrameResult`, frame-owned command reduction result types, and the public `BattleFrameRunner` contract.
- `src/battle/BattleCore.cpp`
  - Owns frame order and private reducers that turn same-frame gameplay facts into committed frame-state mutations.
- `src/battle/BattleDamageApplicationSystem.h/.cpp`
  - May remain as a reusable core subsystem, but it is called by core frame code, not by scene or adapter code.
- `src/battle/BattleDamageSystem.h/.cpp`
  - Owns single transaction resolution.
- `src/BattleSceneBattleAdapter.h/.cpp`
  - Owns legacy snapshot construction and committed-delta application only.
- `src/BattleSceneHades.h/.cpp`
  - Owns input, timing, camera/audio/rumble playback, presentation playback, visual cleanup, and battle exit flow only.
- `tests/BattleCoreUnitTests.cpp`
  - Owns breakthrough tests that prove same-frame core reduction and prevent cosmetic one-call solutions.

---

## Task 0: Authority Reset On This Branch

**Purpose:** Make this reset plan the only active plan for the remaining backend exit work.

**Files:**

- Modify: `docs/battle_refactor_completion_plan.md`
- Modify: `docs/battle_scene_hades_backend_exit_plan.md`
- Modify: `docs/battle_scene_hades_backend_exit_continuation_plan.md`
- Create: `docs/battle_scene_hades_backend_exit_continuation_reset_plan.md`

- [x] Add a superseded notice to `docs/battle_scene_hades_backend_exit_continuation_plan.md` that points here.
- [x] Update `docs/battle_refactor_completion_plan.md` Slice 7.3 references to point here.
- [x] Update `docs/battle_scene_hades_backend_exit_plan.md` superseded notice to point here.
- [x] Run:

```powershell
git diff --check -- docs\battle_refactor_completion_plan.md docs\battle_scene_hades_backend_exit_plan.md docs\battle_scene_hades_backend_exit_continuation_plan.md docs\battle_scene_hades_backend_exit_continuation_reset_plan.md
rg -n "battle_scene_hades_backend_exit_continuation_plan\.md|battle_scene_hades_backend_exit_plan\.md" docs\battle_refactor_completion_plan.md docs\battle_scene_hades_backend_exit_plan.md docs\battle_scene_hades_backend_exit_continuation_plan.md
```

Expected: diff check passes. Any matches for old plan names are historical/superseded context only, not active execution instructions.

Task 0 verification on May 5, 2026: `git diff --check -- docs\battle_refactor_completion_plan.md docs\battle_scene_hades_backend_exit_plan.md docs\battle_scene_hades_backend_exit_continuation_plan.md docs\battle_scene_hades_backend_exit_continuation_reset_plan.md` passed. The old-plan reference search returned only historical references inside superseded plan bodies.

## Task 1: Prove Same-Frame Hit-To-Damage Reduction In Core

**Purpose:** Pin the breakthrough with a failing core test before moving more scene code.

**Files:**

- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`

- [x] Add a focused test named:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame", "[battle][core][breakthrough]")
```

The test must construct one `BattleFrameState` with:

- attacker unit `1`, team `0`, alive;
- defender unit `2`, team `1`, alive with 100 HP;
- one projectile attack from attacker to defender;
- hit unit snapshots for both units;
- hit skill/scalar input producing positive HP damage;
- damage unit/status/cooldown snapshots for both units.

Then call:

```cpp
auto result = BattleFrameRunner().advanceFrame(state);
```

The test must assert all of these:

```cpp
REQUIRE(state.hits.committedResults.size() == 1);
REQUIRE(state.damage.committedTransactions.size() == 1);
CHECK(state.damage.committedTransactions.front().defender.id == 2);
CHECK(state.damage.committedTransactions.front().finalHpDamage > 0);
CHECK(state.damage.committedTransactions.front().defender.hp < 100);
CHECK(std::none_of(
    result.commands.begin(),
    result.commands.end(),
    [](const BattleGameplayCommand& command)
    {
        return std::holds_alternative<BattleHpDamageCommand>(command);
    }));
```

- [x] Add a second focused test named:

```cpp
TEST_CASE("BattleFrameRunner_AdvanceFrame_ReducesLethalHitToDeathAndBattleEndInsideSameFrame", "[battle][core][breakthrough]")
```

The test must use the same path, but make the hit lethal and assert:

```cpp
REQUIRE(state.damage.committedTransactions.size() == 1);
CHECK_FALSE(state.damage.committedTransactions.front().defender.alive);
CHECK(state.result.ended);
CHECK(state.result.winningTeam == 0);
CHECK(std::any_of(
    result.frame.gameplayEvents.begin(),
    result.frame.gameplayEvents.end(),
    [](const BattleGameplayEvent& event)
    {
        return event.type == BattleGameplayEventType::UnitDied
            && event.sourceUnitId == 1
            && event.targetUnitId == 2;
    }));
CHECK(std::any_of(
    result.frame.gameplayEvents.begin(),
    result.frame.gameplayEvents.end(),
    [](const BattleGameplayEvent& event)
    {
        return event.type == BattleGameplayEventType::BattleEnded
            && event.value == 0;
    }));
```

- [x] Run the focused tests before implementation:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough]"
```

Expected before implementation: tests fail because hit damage still exits as `BattleHpDamageCommand` instead of being committed inside the frame.

- [x] Implement the minimal core change:
  - add a private reducer in `BattleCore.cpp` that converts `BattleHpDamageCommand`, `BattleMpDamageCommand`, and `BattleAcceptedHitSideEffectCommand` into `BattleDamageTransactionInput` using `state.damage.units`, `state.damage.cooldowns`, `state.status.units`, and combo/modifier state already present in `BattleFrameState`;
  - call this reducer between `resolveHitEvents(...)` and `applyDamageAndLifecycle(...)`;
  - leave non-damage commands in an internal remainder only if they cannot be reduced yet;
  - do not call adapter or scene helpers.

- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core]"
```

Expected after implementation: breakthrough tests and existing core tests pass.

- [x] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: reduce hit damage inside battle frame runner"
```

Task 1 verification on May 5, 2026: The first focused run failed for the intended reason: both breakthrough tests had `state.damage.committedTransactions.size() == 0` after the runner. After implementing frame damage-command reduction in `BattleFrameRunner`, `kys_tests` Debug x64 built successfully and `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core]"` passed 340 assertions in 43 test cases.

## Task 2: Move Damage Application Result Shape Fully Into `BattleFrameState`

**Purpose:** Remove the need for scene/adapter-side `BattleDamageApplicationSystem` calls after the runner.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [x] Add or finalize frame-owned damage application state:

```cpp
struct BattleFrameState::DamageState
{
    std::vector<BattleDamageUnitState> units;
    std::map<int, BattleCooldownState> cooldowns;
    std::vector<BattleDamageTransactionInput> pendingTransactions;
    std::vector<BattleDamageTransactionResult> committedTransactions;
    std::vector<BattleGameplayEvent> lifecycleEvents;
    std::vector<BattlePresentationEvent> presentationEvents;
};
```

Use the existing nested `DamageState` if it already exists; extend it instead of creating a duplicate state bag.

- [x] Add core tests proving:
  - pending damage transactions are aggregated inside the runner when configured;
  - damage presentation events are emitted by the frame result, not by scene helpers;
  - death-effect commands produced by damage application are reduced or queued for same-frame core reduction.

- [x] Implement by moving the reusable logic from `BattleDamageApplicationSystem` into the runner path, either by:
  - calling `BattleDamageApplicationSystem` inside `BattleFrameRunner`, or
  - extracting shared pure helpers under `src/battle` and using them from the runner.

Do not call `BattleDamageApplicationSystem` from scene or adapter after this task.

- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][damage_application][unit]"
```

- [x] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: own damage application in battle frame state"
```

Task 2 verification on May 5, 2026: The focused red build failed because `BattleFrameState::DamageState` had no aggregation, pending presentation, lifecycle, or presentation result fields. After extending `DamageState` and routing the frame damage phase through `BattleDamageApplicationSystem` inside `BattleFrameRunner`, `kys_tests` Debug x64 built successfully and `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][damage_application][unit]"` passed 409 assertions in 51 test cases.

## Task 3: Delete Post-Runner Damage Application From Scene And Adapter

**Purpose:** Enforce the breakthrough at the integration boundary.

**Files:**

- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Test: `tests/BattleCoreUnitTests.cpp`

- [x] Replace scene calls to `makeBattleDamageApplicationInput(...)` and `applyBattleDamageApplication(...)` with adapter application of `frameState.damage.committedTransactions`, `frameState.damage.lifecycleEvents`, and `frameState.damage.presentationEvents`.
- [x] Delete adapter helpers whose only job is to run `BattleDamageApplicationSystem` after the frame.
- [x] Delete scene-owned pending HP damage drain after the runner. Pending legacy damage may be adapted into `state.damage.pendingTransactions` before the runner only.
- [x] Ensure `BattleSceneBattleAdapter` applies committed damage deltas but does not re-run damage calculation.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][damage_application][unit]"
rg -n "BattleDamageApplicationSystem|makeBattleDamageApplicationInput|applyBattleDamageApplication|pendingPreResolvedHpDamage|calculateQueuedHpDamage|applyAcceptedHitSideEffectTransaction" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected: tests pass. Search returns no scene/adapter post-runner damage application path. If a name remains only in historical comments, delete the comment.

- [x] Commit:

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: delete scene damage application pass"
```

Task 3 verification on May 5, 2026: `kys_tests` Debug x64 built successfully. `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][damage_application][unit]"` passed 81 assertions in 10 test cases, and full `x64\Debug\kys_tests.exe` passed 4598 assertions in 239 test cases. The Task 3 boundary search for `BattleDamageApplicationSystem|makeBattleDamageApplicationInput|applyBattleDamageApplication|pendingPreResolvedHpDamage|calculateQueuedHpDamage|applyAcceptedHitSideEffectTransaction` across `BattleSceneHades` and `BattleSceneBattleAdapter` returned no matches.

## Task 4: Reduce Gameplay Commands Inside Core Before Returning

**Purpose:** Prevent future drift where agents return commands and apply them in the adapter.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`

- [x] Add a core-only command reducer in `BattleCore.cpp` that handles each `BattleGameplayCommand` alternative:
  - HP/MP/accepted-hit damage: append frame damage transaction;
  - team heal/MP/shield: mutate `state.teamEffects.world` and append committed events;
  - projectile spawn: append `state.pendingAttackSpawns`;
  - current HP blast, spiral bleed, nearby tracking, hit extra projectiles, shield explosion, death AOE: expand through `BattleProjectileFollowUpContext` before returning;
  - MP restore, unit heal, unit shield, temp attack buff, last attacker, knockback: mutate appropriate frame-owned unit/combo/status/movement state or append explicit application deltas;
  - auto ultimate: append an explicit frame action request consumed in the action phase, not a scene callback;
  - rumble: append presentation/application fact only.

- [x] Add tests for at least these command classes in `BattleCoreUnitTests.cpp`:
  - team heal mutates frame team-effect world and emits presentation;
  - projectile spawn becomes a same-frame pending attack spawn;
  - temp attack buff mutates combo/frame unit state without adapter dispatch;
  - auto ultimate does not leave the runner as a command to scene.

- [x] Change `BattleFrameResult` so `commands` is not the normal gameplay output. If it remains temporarily, document it as `unreducedCommandsForMigration` and assert it is empty in breakthrough tests.
- [x] Delete adapter command variant dispatch for any command reduced in core.
- [ ] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][team_effect]"
rg -n "std::get_if<.*Battle.*Command|BattleGameplayCommand" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

Expected: no command variant dispatch remains in scene or adapter. Remaining `BattleGameplayCommand` mentions in adapter headers are not acceptable unless the task records a concrete migration blocker and a follow-up task deletes it.

- [ ] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: reduce gameplay commands inside battle core"
```

Task 4 verification on May 5, 2026: `kys_tests` Debug x64 built successfully. `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][team_effect]"` passed 415 assertions in 53 test cases, and full `x64\Debug\kys_tests.exe` passed 4623 assertions in 243 test cases. The Task 4 boundary search for `std::get_if<.*Battle.*Command|BattleGameplayCommand` across `BattleSceneHades` and `BattleSceneBattleAdapter` returned no matches.

## Task 5: Move Rescue Reposition Into The Damage/Lifecycle Phase

**Purpose:** Rescue/protect/execute pull is lifecycle gameplay and must not run after the frame in scene code.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`

- [x] Add rescue input snapshots to `BattleFrameState`, using the existing `BattleRescueRepositionSystem` input types where practical:
  - unit/team snapshots;
  - force-pull counters;
  - legal grid cells;
  - occupancy;
  - deterministic tie-break inputs.

- [x] In the runner damage/lifecycle phase, detect the same conditions currently handled after damage:
  - HP crosses below 25 percent for protect pull;
  - HP crosses below 15 percent and unattended for execute pull.

- [x] Run `BattleRescueRepositionSystem` inside the runner and return explicit committed deltas:
  - teleport destination;
  - counter decrement;
  - protection heal;
  - temporary invincibility;
  - basic counterattack spawn request;
  - presentation facts.

- [x] Add core tests proving:
  - protect pull is emitted in the same frame as damage;
  - execute pull decrements execute counter and queues the basic counterattack spawn;
  - no rescue delta is emitted when no legal cell exists;
  - scene does not decide unattended/position selection after the runner.

- [x] Delete scene calls to `BattleRescueRepositionSystem`.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][rescue_reposition][unit],[battle][core]"
rg -n "BattleRescueRepositionSystem|tryForcePull|forcePullProtect|forcePullExecute|isUnattended" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: focused tests pass. Search returns no scene-owned rescue decision path.

- [x] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp tests\BattleCoreUnitTests.cpp src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: move rescue reposition into frame runner"
```

Task 5 verification on May 5, 2026: The red build failed first because `BattleFrameState` had no rescue state/API. After adding frame rescue snapshots, committed rescue results, and runner-side rescue reduction, `kys_tests` Debug x64 built successfully. `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][rescue_reposition][unit],[battle][core]"` passed 435 assertions in 54 test cases, and full `x64\Debug\kys_tests.exe` passed 4662 assertions in 246 test cases. The Task 5 boundary search for `BattleRescueRepositionSystem|tryForcePull|forcePullProtect|forcePullExecute|isUnattended` across `BattleSceneHades.cpp` and `BattleSceneHades.h` returned no matches.

## Task 6: Move Movement Physics Into Frame State

**Purpose:** Stop mutating role position/velocity before building the frame.

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleMovement.h`
- Modify: `src/battle/BattleMovement.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`
- Modify: `tests/BattleMovementRealStatsTests.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`

- [x] Add frame-owned movement physics input for each unit:
  - position;
  - velocity;
  - acceleration;
  - dash runtime counters;
  - gravity/friction config;
  - legal movement/standability snapshot, not scene callback.

- [x] Move the current scene `BattleMovementPhysicsSystem().advance(...)` loop into `BattleFrameRunner`.
- [x] Return committed movement physics deltas and apply them through adapter after the runner.
- [x] Ensure the scene build path snapshots movement state without mutating `Role::Pos`, `Role::Velocity`, or `Role::Acceleration`.
- [x] Add a core test proving frozen units tick timers but do not run movement physics, while non-frozen units commit position/velocity in the same frame.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][movement]"
rg -n "BattleMovementPhysicsSystem|decreaseToZero\\(r->Frozen\\)|r->Pos =|r->Velocity =|r->Acceleration =" src\BattleSceneHades.cpp
```

Expected: no pre-runner movement physics mutation remains in `BattleSceneHades`.

- [x] Commit:

```powershell
git add src\battle\BattleCore.h src\battle\BattleCore.cpp src\battle\BattleMovement.h src\battle\BattleMovement.cpp tests\BattleCoreUnitTests.cpp tests\BattleMovementRealStatsTests.cpp src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
git commit -m "refactor: advance movement physics inside frame runner"
```

Task 6 verification on May 5, 2026: The red build failed first because `BattleFrameState` had no movement physics state/API. After adding frame movement physics snapshots, collision cell/unit snapshots, runner-side physics advancement, and adapter committed-result application, `kys_tests` Debug x64 built successfully. `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][movement]"` passed 2873 assertions in 19 test cases, and full `x64\Debug\kys_tests.exe` passed 4674 assertions in 247 test cases. The Task 6 boundary search for `BattleMovementPhysicsSystem|decreaseToZero\(r->Frozen\)|r->Pos =|r->Velocity =|r->Acceleration =` in `BattleSceneHades.cpp` returned no matches.

## Task 7: Collapse Scene Frame Path To Bundle/Advance/Apply

**Purpose:** Make the production path match the real frame contract after the core breakthrough exists.

**Files:**

- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Modify: `src/BattleScenePresentationPlayer.h`

- [x] Introduce or finalize `BattleSceneFrameBundle` as:

```cpp
struct BattleSceneFrameBundle
{
    KysChess::Battle::BattleFrameState state;
    std::unordered_map<int, Role*> rolesByBattleId;
    std::vector<KysChess::Battle::BattlePresentationEvent> pendingPresentationEvents;
};
```

Only add adapter-only lookup/application data that is required to write committed deltas. Do not store gameplay work queues such as pending damage, pending casts, or active attack worlds outside `state`.

- [x] Rewrite `BattleSceneHades::backRun1()` into the target shape.
- [x] Delete transitional helpers named `advanceCoreBattleFrame`, `applyCoreBattleFrameResult`, `applyBattleGameplayCommand`, `applyResolvedBattleHit`, `Action(r)`, and `AI(r)` once their responsibilities are runner-owned.
- [x] Keep scene-only camera, audio, rumble, text-effect movement, enemy entrance, and exit timing outside the runner only when driven by explicit frame result facts.
- [x] Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][presentation]"
rg -n "BattleFrameRunner\\(\\)\\.advanceFrame" src\BattleSceneHades.cpp
rg -n "advanceCoreBattleFrame|applyCoreBattleFrameResult|applyBattleGameplayCommand|applyResolvedBattleHit|void BattleSceneHades::Action|void BattleSceneHades::AI|Action\\(r\\)|AI\\(r\\)" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: one runner call in `BattleSceneHades.cpp`, inside the bundle/advance/apply path. No scene gameplay helper matches remain.

- [x] Commit:

```powershell
git add src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleScenePresentationPlayer.cpp src\BattleScenePresentationPlayer.h
git commit -m "refactor: collapse battle scene frame path"
```

Task 7 verification on May 6, 2026: `kys_tests` Debug x64 built successfully. `x64\Debug\kys_tests.exe "[battle][core][breakthrough],[battle][core],[battle][presentation]"` passed 534 assertions in 63 test cases. The runner-call search returned exactly one `BattleFrameRunner().advanceFrame` occurrence in `BattleSceneHades.cpp`, and the transitional-helper search for `advanceCoreBattleFrame|applyCoreBattleFrameResult|applyBattleGameplayCommand|applyResolvedBattleHit|void BattleSceneHades::Action|void BattleSceneHades::AI|Action\(r\)|AI\(r\)` returned no matches.

## Task 8: Final Boundary Gate

**Purpose:** Prove the backend exit is real and prevent another cosmetic completion claim.

**Files:**

- Modify only files required to fix verification failures.
- Modify: `docs/battle_scene_hades_backend_exit_continuation_reset_plan.md`

- [ ] Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: diff check passes, all tests pass, and game target builds. If final link fails only because the running game locks the executable, record the exact linker message and treat compilation before link as the useful build evidence.

- [ ] Run boundary searches:

```powershell
rg -n "BattleFrameRunner\\(\\)\\.advanceFrame" src\BattleSceneHades.cpp
rg -n "std::get_if<.*Battle.*Command|BattleGameplayCommand|BattleDamageApplicationSystem|BattleRescueRepositionSystem|BattleMovementPhysicsSystem|BattleHitResolver|BattleActionCommitSystem|BattleCastPlanner|BattleTeamEffectSystem|BattleComboTriggerSystem|BattleEffectDispatcher" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
rg -n "Role\\*|Magic\\*|Item\\*|Engine|TextureManager|Font|Scene" src\battle
rg -n "BattleFrameOrchestrator|frame_orchestrator|std::function<.*Battle|callback.*Battle|unreducedCommandsForMigration" src tests
```

Expected:

- `BattleFrameRunner().advanceFrame` appears once in `BattleSceneHades.cpp`.
- Scene/adapter have no gameplay command dispatch and no direct calls to backend systems for gameplay.
- `src/battle` has no legacy/render dependencies.
- No second orchestrator, callback gameplay routing, or unreduced migration command queue remains.

- [ ] Record the exact verification results in this file under `## Verification Log`.
- [ ] Commit:

```powershell
git add docs\battle_scene_hades_backend_exit_continuation_reset_plan.md
git commit -m "docs: record battle backend exit verification"
```

## Completion Criteria

- The breakthrough tests pass and prove hit-to-damage-to-death/result happens inside one `BattleFrameRunner::advanceFrame()` call.
- `BattleGameplayCommand` does not cross from core to scene/adapter for gameplay application.
- Scene and adapter do not call core gameplay systems after the runner returns.
- Movement physics, damage application, rescue reposition, lifecycle, and battle result are frame-owned.
- `BattleSceneHades::backRun1()` is a thin bundle/advance/apply/presentation path.
- `BattleSceneBattleAdapter` only builds snapshots and applies committed deltas.
- Full Debug x64 `kys_tests` passes and Debug x64 `kys` builds.

## Verification Log

- 2026-05-05 reset branch created from `42f5d5dc5401114803572b4a445d9b57f31f65ab` on `codex/battle-backend-exit-breakthrough`. This plan replaces the previous continuation approach because the prior implementation direction could satisfy runner call-count searches while still leaving gameplay split across scene and adapter passes.
