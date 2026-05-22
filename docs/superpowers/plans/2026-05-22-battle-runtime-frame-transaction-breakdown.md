# Battle Runtime Frame Transaction Phase 2 Research Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the broad phase 2 direction from `2026-05-21-battle-runtime-frame-direction.md` into researched, reviewable implementation slices.

**Architecture:** `BattleRuntimeSession::runFrame()` remains the only gameplay frame advance path. `BattleFrameRunner::runFrame()` keeps visible imperative ordering, with a one-frame `BattleFrameContext` collecting temporary outputs before any subsystem ownership changes are attempted. Persistent mirror-world deletion is split by gameplay domain after consumer and synchronization inventories are complete.

**Tech Stack:** C++17, Catch2 unit tests, Visual Studio/MSBuild via `.github/build-command.ps1`, PowerShell, ripgrep.

---

## Why This Exists

The original phase 2 says to introduce `BattleFrameContext`, make frame ordering explicit, convert subsystem APIs, and collapse duplicated event/application channels. Those are not one implementation task. They touch different risk classes:

- mechanical frame-local cleanup
- public/result consumer behavior
- runtime-to-subsystem mirror synchronization
- scene/report presentation contracts
- frame-level test coverage

Phase 2 should begin with the safe mechanical slice, then use research output to decide each deeper migration.

---

## Current Baseline

This plan assumes phase 0 and phase 1 are committed at:

```text
ae5f919 refactor: move death combo consequences into runtime
```

Known current shape:

| Area | Current fact | Initial classification |
| --- | --- | --- |
| `BattleFrameRunner::runFrame()` | Already owns visible frame order in `src/battle/BattleCore.cpp`. | Keep this shape; do not hide ordering behind subsystem callbacks. |
| Frame-local vectors | `runFrame()` owns `frameCommands`, `gameplayEvents`, `logEvents`, `visualEvents`, `deferredCommands`, and `runtimeCommits`. | Safe phase 2A candidate for `BattleFrameContext`. |
| `BattleFrameResult` | `src/battle/BattleCore.h` exposes many parallel vectors: movement, attacks, applications, damage transactions, state applications, and presentation frame. | Needs consumer audit before deletion or consolidation. |
| Movement | Runtime builds `BattleMovementFrameInput`, runs `BattleCore(movementInput).tickMovement()`, then writes movement state back. | Mirror-like frame world; research before replacement. |
| Damage | Runtime builds `BattleDamageUnitState` from canonical units and writes selected fields back through damage runtime helpers. | Mirror-like damage state; migrate only after damage/status ownership map is written. |
| Scene consumption | `BattleSceneHades` consumes `BattleFrameResult.frame.visualEvents`, `.logEvents`, and frame delta outputs for presentation. | Keep presentation contract stable during phase 2A/2B. |
| Tests | `tests/BattleCoreUnitTests.cpp` and `tests/BattleFrameRunnerRuntimeUnitTests.cpp` assert many frame result fields directly. | Test consumers must be included in the result-channel audit. |

---

## Phase 2 Decision

Do not implement all of phase 2 in one pass.

The first implementation slice should be:

```text
Phase 2A: Introduce BattleFrameContext as a one-frame local holder.
```

The following work requires research and smaller plans:

```text
Phase 2B: Route top-level frame helpers through BattleFrameContext.
Phase 2C: Audit and simplify BattleFrameResult channels.
Phase 2D+: Delete mirror-world synchronization one gameplay domain at a time.
```

---

## Files And Responsibilities

| File | Responsibility in this plan |
| --- | --- |
| `docs/superpowers/plans/2026-05-21-battle-runtime-frame-direction.md` | High-level battle runtime direction; points phase 2 readers here. |
| `docs/superpowers/plans/2026-05-22-battle-runtime-frame-transaction-breakdown.md` | This research and breakdown plan. |
| `src/battle/BattleCore.cpp` | Owns frame runner implementation, frame helper signatures, and anonymous-namespace frame context. |
| `src/battle/BattleCore.h` | Owns public frame result shape and `BattleFrameRunner` API. |
| `src/BattleSceneHades.cpp` | Main scene-side `BattleFrameResult` consumer. |
| `src/BattleSceneFrameDeltaBuilder.cpp` | Presentation mapping from runtime result into scene delta. |
| `tests/BattleCoreUnitTests.cpp` | Existing broad frame-runner scenario coverage. |
| `tests/BattleFrameRunnerRuntimeUnitTests.cpp` | Existing runtime-owned frame-state coverage. |

---

## Research Task 1: Frame Transaction Inventory

**Goal:** Capture the exact current frame order and all temporary frame carriers before `BattleFrameContext` is introduced.

**Files:**

- Read: `src/battle/BattleCore.cpp`
- Read: `src/battle/BattleCore.h`
- Update research notes in this file after running commands.

- [x] **Step 1: Confirm the working tree is clean**

Run:

```powershell
git status --short
```

Expected: no output before implementation work starts.

Result: no output after committing the phase 2 planning docs.

- [x] **Step 2: Record the current `runFrame()` order**

Run:

```powershell
rg -n "BattleFrameRunner::runFrame|advanceStatus|advanceRuntimeUnits|applyRuntimeComboEvents|applyPendingTeamEffects|reduceFrameGameplayCommands|advanceMotionFrame|advanceActionFrameUnits|advanceAttacksAndResolveHits|applyDamageAndLifecycle|publishMovementPresentationResults|publishFrameApplyOutputs|emitPresentationFrame|pruneFinishedRuntimeAttacks" src/battle/BattleCore.cpp
```

Expected: one ordered chain inside `BattleFrameRunner::runFrame()` plus helper definitions above it.

Result: `BattleFrameRunner::runFrame()` currently calls, in order, `advanceStatus`, `advanceRuntimeUnits`, `applyRuntimeComboEvents`, `applyPendingTeamEffects`, `reduceFrameGameplayCommands`, `advanceMotionFrame`, `advanceActionFrameUnits`, `reduceFrameGameplayCommands`, `advanceAttacksAndResolveHits`, `applyDamageAndLifecycle`, final projectile cancellation logging, late `reduceFrameGameplayCommands`, `publishMovementPresentationResults`, `publishFrameApplyOutputs`, `emitPresentationFrame`, and `pruneFinishedRuntimeAttacks`.

- [x] **Step 3: Record current frame-local carriers**

Run:

```powershell
rg -n "std::vector<BattleGameplayCommand> frameCommands|std::vector<BattleGameplayEvent> gameplayEvents|std::vector<BattleLogEvent> logEvents|std::vector<BattleVisualEvent> visualEvents|std::vector<BattleGameplayCommand> deferredCommands|std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits|frameStartMotion" src/battle/BattleCore.cpp
```

Expected: these carriers are local to `BattleFrameRunner::runFrame()` and helper parameters.

Result: `frameCommands`, `gameplayEvents`, `logEvents`, `visualEvents`, `deferredCommands`, `runtimeCommits`, and `frameStartMotion` are currently local to `BattleFrameRunner::runFrame()` and passed through helper parameters.

- [x] **Step 4: Decide the exact `BattleFrameContext` shape for phase 2A**

Use this shape unless research finds a compile-level blocker:

```cpp
struct BattleFrameContext
{
    BattleFrameResult result;
    std::vector<BattleGameplayCommand> frameCommands;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<BattleGameplayCommand> deferredCommands;
    std::vector<BattleRuntimeUnitFrameCommit> runtimeCommits;
    UnitMotionSnapshotMap frameStartMotion;
};
```

Constraint: `BattleFrameContext` belongs in `src/battle/BattleCore.cpp` near `BattleFrameRunner::runFrame()` or in the anonymous namespace. Do not expose it from `BattleCore.h` unless a later task proves external callers need it.

Decision: use the shape above. Place `BattleFrameContext` in the anonymous namespace after `UnitMotionSnapshotMap` is available, and add `makeBattleFrameContext()` after `makeUnitMotionSnapshot()` so initialization can call `makeUnitMotionSnapshot(state.unitStore)`. No `BattleCore.h` exposure is needed for phase 2A.

---

## Implementation Slice 2A: Introduce `BattleFrameContext`

**Goal:** Make frame-local state explicit without changing behavior or subsystem APIs.

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`
- Test: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [x] **Step 1: Add `BattleFrameContext` in `BattleCore.cpp`**

Add a local struct that owns only one-frame data. Keep initialization mechanical:

```cpp
BattleFrameContext makeBattleFrameContext(const BattleRuntimeState& state)
{
    BattleFrameContext frame;
    frame.frameStartMotion = makeUnitMotionSnapshot(state.unitStore);
    return frame;
}
```

Expected compile constraint: `UnitMotionSnapshotMap` must already be declared before the struct/helper.

Result: `BattleFrameContext` is local to `src/battle/BattleCore.cpp`, after `UnitMotionSnapshotMap` and `makeUnitMotionSnapshot()` are available.

- [x] **Step 2: Replace `runFrame()` locals with context fields**

Change `BattleFrameRunner::runFrame()` from separate locals to:

```cpp
auto frame = makeBattleFrameContext(state);
```

Then pass `frame.frameCommands`, `frame.runtimeCommits`, `frame.result`, and other fields into the existing helper signatures.

Do not change helper signatures in this slice unless a reference cannot be passed safely.

Result: `BattleFrameRunner::runFrame()` now uses `auto frame = makeBattleFrameContext(state);` and passes existing helper signatures the corresponding `frame.*` fields.

- [x] **Step 3: Return `frame.result`**

End `runFrame()` with:

```cpp
return std::move(frame.result);
```

Expected behavior: tests that assert `BattleFrameResult` fields keep passing unchanged.

Result: `runFrame()` returns `std::move(frame.result)`.

- [x] **Step 4: Run focused runtime/frame tests**

Run:

```powershell
x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"
```

Expected: all selected tests pass. If the Catch2 filter syntax does not select the intended tests, run `x64\Debug\kys_tests.exe` instead and record that result in the implementation notes.

Result: `x64\Debug\kys_tests.exe "[battle][core],[battle][frame_runner],[battle][runtime_session]"` passed 893 assertions in 124 test cases.

- [x] **Step 5: Run full tests and build**

Run:

```powershell
x64\Debug\kys_tests.exe
.github\build-command.ps1
```

Expected: tests exit 0 and MSBuild exits 0. Final linking failure is acceptable only when the output states the game executable is locked by a running process.

Result: `x64\Debug\kys_tests.exe` passed 5632 assertions in 418 test cases. `.github\build-command.ps1` exited 0 and produced `x64\Debug\kys.exe`; MSBuild still reported the existing `BattleCore.cpp(1264)` C4244 warning.

- [x] **Step 6: Commit phase 2A**

Run:

```powershell
git add src/battle/BattleCore.cpp tests/BattleCoreUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp docs/superpowers/plans/2026-05-22-battle-runtime-frame-transaction-breakdown.md
git commit -m "refactor: introduce battle frame context"
```

Expected: one mechanical commit with no result-channel deletion and no subsystem mirror deletion.

Result: phase 2A is committed as a mechanical `BattleFrameContext` slice with no result-channel deletion and no subsystem mirror deletion.

---

## Research Task 2: Helper Signature Routing For Phase 2B

**Goal:** Decide which helpers should accept `BattleFrameContext&` so the top-level transaction stops passing many parallel vectors.

**Files:**

- Read: `src/battle/BattleCore.cpp`
- Update this file with the chosen phase 2B helper list.

- [x] **Step 1: Find helpers with three or more frame-output parameters**

Run:

```powershell
rg -n "std::vector<BattleGameplayCommand>&|std::vector<BattleGameplayEvent>&|std::vector<BattleLogEvent>&|std::vector<BattleVisualEvent>&|BattleFrameResult&|BattleFrameApplications&|std::vector<BattleEffectCommand>&|std::vector<BattleTeamEffectEvent>&" src/battle/BattleCore.cpp
```

Expected: the high-churn candidates include `reduceFrameGameplayCommands`, `applyRuntimeComboEvents`, `advanceActionFrameUnits`, `advanceAttacksAndResolveHits`, `applyDamageAndLifecycle`, and `emitPresentationFrame`.

Result: the broad search also found lower-level reducers and append helpers, but the top-level phase 2B routing candidates remain `reduceFrameGameplayCommands`, `applyRuntimeComboEvents`, `applyPendingTeamEffects`, `advanceActionFrameUnits`, `advanceAttacksAndResolveHits`, `applyDamageAndLifecycle`, and `emitPresentationFrame`.

- [x] **Step 2: Classify each helper**

Use this table:

| Helper | Phase 2B action |
| --- | --- |
| `reduceFrameGameplayCommands` | Convert to `BattleFrameContext&` after 2A. |
| `applyRuntimeComboEvents` | Convert to `BattleFrameContext&` after 2A. |
| `advanceActionFrameUnits` | Convert only after cast/action tests are green under 2A. |
| `advanceAttacksAndResolveHits` | Convert after action helper conversion, because it reduces hit commands immediately. |
| `applyDamageAndLifecycle` | Convert after attack/hit helper conversion, because it uses `frameStartMotion` and appends death/battle-end events. |
| `emitPresentationFrame` | Convert last; it consumes accumulated gameplay/log/visual vectors. |

Decision: keep the existing order, and include `applyPendingTeamEffects` beside `applyRuntimeComboEvents` because it shares the same `teamEffectEvents`, `logEvents`, and `visualEvents` frame outputs.

- [x] **Step 3: Write phase 2B as a mechanical implementation plan**

Create a separate plan file only if the helper conversion list is larger than one commit:

```text
docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md
```

Expected: phase 2B should not delete result fields or change subsystem internals.

Result: phase 2B is split into a dedicated plan at `docs/superpowers/plans/2026-05-22-battle-frame-context-routing.md`, with one commit per routing group and no result-field or subsystem-ownership changes.

---

## Research Task 3: `BattleFrameResult` Consumer Audit

**Goal:** Know which result channels are public contracts, scene contracts, test-only probes, and removable implementation details.

**Files:**

- Read: `src/battle/BattleCore.h`
- Read: `src/BattleSceneHades.cpp`
- Read: `src/BattleSceneFrameDeltaBuilder.cpp`
- Read: `tests/BattleCoreUnitTests.cpp`
- Read: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [x] **Step 1: List direct field consumers**

Run:

```powershell
rg -n "frameResult\\.|result\\.(frame|movement|attackEvents|applications|projectileCancelDamageCommands|unitApplications|movementPhysicsResults|movementPresentationResults|hitResults|actionResults|damageTransactions|damageRenderApplications|rescueResults|teamEffectEvents|effectCommands|stateApplications)" src tests
```

Expected: scene code primarily consumes `frame`, `movementPresentationResults`, and state/presentation applications; tests consume many more internals.

Result: production consumers are narrower than the initial expectation. `src/BattleSceneHades.cpp` consumes `projectileCancelDamageCommands`, `frame.visualEvents`, and `frame.logEvents`; `src/BattleSceneFrameDeltaBuilder.cpp` consumes `applications`, `actionResults`, `frame.gameplayEvents`, `damageRenderApplications`, and `rescueResults`; `src/BattleSceneImpactPlayer.cpp` consumes `attackEvents`. `movementPhysicsResults`, `movementPresentationResults`, `hitResults`, `unitApplications`, `teamEffectEvents`, `effectCommands`, and `stateApplications` have no direct scene consumer today. They are runtime internals, tests, or publication fields waiting for a clearer contract.

- [x] **Step 2: Classify fields**

Final classification after the consumer audit:

| `BattleFrameResult` field | Consumer class | Rule |
| --- | --- | --- |
| `frame` | Public scene/report presentation contract | Keep stable until scene playback is simplified. |
| `movement` | Runtime staging plus test/debug probe | Keep until action/cast no longer reads movement decisions from `BattleFrameResult`. |
| `attackEvents` | Scene impact contract plus runtime presentation source | Keep until projectile visual/log emission and `BattleSceneImpactPlayer` no longer read it. |
| `applications` | Scene sound/rumble contract | Keep until sound/rumble commands are ordered frame events. |
| `projectileCancelDamageCommands` | Report/log playback command | Keep until report player consumes ordered frame events. |
| `unitApplications` | Runtime-unit publication and test probe | Candidate only after runtime-unit scene synchronization is removed or replaced. |
| `movementPhysicsResults` | Test/debug probe and local presentation input | First removal candidate: move to `BattleFrameContext`, keep it private, and update tests to assert canonical runtime state plus presentation output. |
| `movementPresentationResults` | Test/debug publication; no direct scene consumer today | Candidate after `movementPhysicsResults` is private and scene movement presentation ownership is confirmed. |
| `hitResults` | Runtime test/debug probe and damage bridge | Candidate after hit-to-damage behavior has frame-level coverage that does not read `hitResults`. |
| `actionResults` | Scene blink sound source plus runtime test/debug probe | Keep until `BattleSceneFrameDeltaBuilder` no longer reads blink teleports from action results. |
| `damageTransactions` | Runtime test/debug probe and damage lifecycle source | Do not delete until damage render applications and frame events cover all scene/report needs. |
| `damageRenderApplications` | Scene presentation input | Keep while scene delta uses it. |
| `rescueResults` | Scene presentation input | Keep while rescue visuals are scene-mapped. |
| `teamEffectEvents` | Runtime test/debug probe | Candidate after team effect events are represented by ordered frame events or applications. |
| `effectCommands` | Runtime test/debug probe and command bridge | Candidate after frame commands have a single ordered event/command publication path. |
| `stateApplications` | Render-state publication; currently test-only direct consumer | Candidate after scene render-state synchronization is confirmed to use runtime stores or ordered events. |

- [x] **Step 3: Decide the first removable field**

Do not remove a field in the audit task. Pick exactly one first candidate and write a dedicated implementation plan for that field.

Expected first candidates after audit: `movementPhysicsResults`, `hitResults`, or `actionResults`, because scene code should not need them directly.

Decision: remove or narrow `movementPhysicsResults` first. It has no production consumer outside `src/battle/BattleCore.cpp`, while tests can assert the same behavior through canonical runtime state and `movementPresentationResults`. The dedicated plan is:

```text
docs/superpowers/plans/2026-05-23-battle-frame-result-movement-physics-channel-plan.md
```

---

## Research Task 4: Mirror-World Synchronization Inventory

**Goal:** Identify persistent mirror synchronization pairs and split deletion work by gameplay domain.

**Files:**

- Read: `src/battle/`
- Read: `src/BattleSceneHades.cpp`
- Read: `src/BattleSceneFrameDeltaBuilder.cpp`

- [x] **Step 1: Run the broad mirror search**

Run:

```powershell
rg -n "make.*World|write.*World|make.*State|write.*State|make.*Input|apply.*Snapshot|apply.*Frame" src/battle src/BattleSceneHades.cpp src/BattleSceneFrameDeltaBuilder.cpp
```

Expected: matches include setup-time conversion, pure calculation input creation, one-frame scratch, and persistent mirror synchronization.

Result: matches include scene setup (`BattleSceneHades::makeBattleRuntimeSessionCreationInput`), scene/result application (`applyCoreFrameResult`, `applySceneFrameDelta`, `applyLegacyBattleFrameResult`), movement world construction (`makeMovementFrameInput`, `applyMovementFrameState`, `makeBattleWorldUnitState`), damage state conversion/writeback (`makeBattleDamageUnitStateFromRuntime`, `writeBattleDamageRuntimeUnitImpl`, `BattleUnitStore::writeDamageUnit`, `applyDamageResultToFrameState`), action state conversion/writeback (`makeActionRuntimeState`, `writeActionStateToUnitStore`), hit/cast/rescue calculation input builders, and status/runtime setup conversion in `BattleRuntimeSession`.

- [x] **Step 2: Classify each match**

Use these categories:

| Category | Definition | Action |
| --- | --- | --- |
| Pure calculation input | Contains only data needed by a stateless rule and does not write back to runtime. | Keep unless it hides ordering. |
| Setup-time conversion | Converts scene/config setup into runtime creation state. | Keep narrow; not active during frame execution. |
| One-frame scratch | Cannot outlive a single `runFrame()` and writes back immediately under runner order. | Prefer keeping until a direct mutator is clearer. |
| Persistent mirror synchronization | Copies runtime-shaped state into another owner, runs logic, then writes gameplay fields back. | Delete one domain at a time. |

Inventory classification:

| Match group | Category | Decision |
| --- | --- | --- |
| `BattleSceneHades::makeBattleRuntimeSessionCreationInput`, `BattleRuntimeSession::createInitialized`, initial damage/status runtime conversion | Setup-time conversion | Keep out of phase 2D; this is startup ownership, not frame transaction ownership. |
| `makeRuntimeUnitTickInput`, `makeRuntimeCastSkillState`, `makeCommittedCastActionInput`, `makeRuntimeBlinkGeometry`, `makeHitResolutionInput`, `makeRescueInput`, `makeFrameDamageApplicationInput` | Pure calculation input or one-frame scratch | Keep narrow unless a domain plan replaces the paired writeback in the same slice. |
| `BattleFrameContext`, `runtimeCommits`, `frameStartMotion` | One-frame scratch | Keep private to `BattleCore.cpp`; do not pass to subsystem classes. |
| `makeMovementFrameInput`, `applyMovementFrameState`, `commitFrameMovement`, `makeBattleWorldUnitState` | Persistent mirror synchronization | Movement domain plan. |
| `makeBattleDamageUnitStateFromRuntime`, `writeBattleDamageRuntimeUnitImpl`, `BattleUnitStore::writeDamageUnit`, `applyDamageResultToFrameState` | Persistent mirror synchronization | Damage/status domain plan. |
| `makeActionRuntimeState`, `writeActionStateToUnitStore`, `BattleFrameActionUnitResult::state` | Persistent mirror synchronization | Action/cast domain plan. |
| `BattleAttackSystem` event list, `makeHitResolutionInput`, `resolveHitEvents`, `hitResults`, projectile cancellation outputs | Mixed persistent attack store plus one-frame hit scratch | Attack/hit/projectile domain plan. |
| `applyRescueResultToFrameState`, pending team effects, death combo consequences | Persistent runtime mutation driven by lifecycle events | Rescue/death/team-effect domain plan after damage lifecycle work. |
| `applyCoreFrameResult`, `applySceneFrameDelta`, `applyLegacyBattleFrameResult` | Scene presentation synchronization | Keep stable while runtime ownership changes; simplify after runtime result channels shrink. |

- [x] **Step 3: Split mirror deletion by domain**

Use this order unless the inventory shows a lower-risk candidate:

| Domain | Reason for order |
| --- | --- |
| Movement presentation/test probes | Most visible current mirror, but scene/test consumers are explicit. |
| Damage runtime extras and status writeback | High gameplay risk; needs dedicated tests around death prevention, MP gain, poison/bleed, freeze. |
| Action/cast commit state | High ordering risk; must preserve pending casts, cooldown, operation count, MP, combo updates. |
| Attack/hit/projectile resolution | High event-order risk; must preserve projectile cancellation, bounce, target lost, invincible aggregation. |
| Rescue/death/team effects | Depends on damage lifecycle and battle-end ordering. |

Decision: split 2D+ into five domain plans and keep result-channel removal separate from mirror-world deletion. The domain order remains movement, damage/status, action/cast, attack/hit/projectile, then rescue/death/team effects.

- [x] **Step 4: Write one dedicated implementation plan per domain**

Plan filenames:

```text
docs/superpowers/plans/2026-05-23-battle-movement-frame-state-plan.md
docs/superpowers/plans/2026-05-23-battle-damage-frame-state-plan.md
docs/superpowers/plans/2026-05-23-battle-action-frame-state-plan.md
docs/superpowers/plans/2026-05-23-battle-attack-hit-frame-state-plan.md
docs/superpowers/plans/2026-05-23-battle-rescue-death-team-effect-frame-state-plan.md
```

Each domain plan must include exact tests to add or preserve before code changes.

---

## Research Task 5: Scenario Coverage Gate

**Goal:** Decide which frame-level tests must exist before deleting a mirror path or result channel.

**Files:**

- Read: `tests/BattleCoreUnitTests.cpp`
- Read: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`
- Update this file with coverage gaps.

- [x] **Step 1: Inventory existing frame-level tests by behavior**

Run:

```powershell
rg -n "TEST_CASE\\(\"BattleFrameRunner|TEST_CASE\\(\"BattleRuntimeSession" tests/BattleCoreUnitTests.cpp tests/BattleFrameRunnerRuntimeUnitTests.cpp
```

Expected: current coverage already includes movement ordering, cast planning/commit, damage lifecycle, projectile cancellation, rescue, auto ultimate, and runtime session ownership.

Result: the exact search found the `BattleFrameRunner_*` and `BattleRuntimeSession_*` scenario tests in `tests/BattleCoreUnitTests.cpp` and `tests/BattleFrameRunnerRuntimeUnitTests.cpp`. The broader `TEST_CASE` inventory confirms coverage for movement ordering, cast planning and commit, damage lifecycle, projectile cancellation, rescue, auto ultimate, runtime session ownership, and death combo scene consumption.

- [x] **Step 2: Map each domain to required scenario checks**

Use this checklist:

| Domain | Required frame-level checks | Current coverage |
| --- | --- |
| Movement | Post-movement cast origin, moving corpse physics, death kick presentation, frozen physics timer. | Covered by `BattleFrameRunner_AdvanceFrame_CastOriginUsesPostMovementPosition`, `BattleFrameRunner_AdvanceFrame_KeepsMovingCorpsesInMovementPhysics`, `BattleFrameRunner_AdvanceFrame_AppliesDeathKickVelocity`, `BattleFrameRunner_AdvanceFrame_RunsMovementPhysicsInsideCore`, and related movement tests. |
| Damage/status | Poison/bleed to damage transaction, death prevention, damage-taken MP gain, impact freeze, battle end emits once. | Poison/bleed, MP gain, impact freeze, and battle-end-once are covered. Death prevention is the gap: add a frame-level death-prevention test before changing damage writeback ownership. |
| Action/cast | Cast start without attack spawn, pending cast refresh, target death cancel/retarget, MP gain/spend, combo post-skill effects. | Covered by cast start, pending cast refresh, retarget/cancel, MP gain, pending cast input, and cast-scoped combo tests. MP spend is covered through committed cast MP delta expectations, but keep it in the action plan gate. |
| Attack/hit/projectile | Hit-to-damage same frame, dodge before damage, projectile target lost, cancel pair, bounce chain terminal logs. | Covered by `BattleFrameRunner_AdvanceFrame_ReducesHitDamageInsideSameFrame`, dodge, target lost, chained stop logs, cancel pair, invincible aggregation, bounce, and terminal reason tests. |
| Rescue/death/team effects | Protect rescue, execute rescue counterattack, death combo consequences, pending team effect source death. | Covered by protect rescue, execute rescue, `BattleRuntimeSession_RunFrame_AppliesDeathComboConsequencesBeforeSceneConsumption`, and pending team effect source death tests. |

- [x] **Step 3: Require red/green proof for behavior-changing migrations**

For behavior-changing migrations, the implementation plan must include:

```text
1. Add or identify a frame-level test that fails without the migrated behavior.
2. Run the focused test and observe failure if adding new coverage.
3. Implement the migration.
4. Run the focused test and full test binary.
5. Run `.github\build-command.ps1`.
```

Mechanical refactors such as phase 2A may use existing tests only.

Decision: every 2D+ domain plan includes a coverage gate and the red/green proof requirement. The `movementPhysicsResults` public-channel removal is a mechanical API narrowing and may use rewritten existing tests plus full verification; domain ownership migrations must use red/green proof for any added behavior coverage.

---

## Ready Criteria For Implementation

Phase 2A is ready when:

- the working tree is clean
- `BattleFrameContext` is confirmed as one-frame local state only
- no result channel deletion is included
- no subsystem API ownership change is included

Phase 2B is ready when:

- phase 2A is committed
- the helper list in Research Task 2 is confirmed
- each helper conversion can be reviewed mechanically

Phase 2C is ready when:

- the `BattleFrameResult` consumer audit is complete
- exactly one first field/channel is chosen for removal or narrowing
- scene and test consumers for that field are listed

Phase 2D+ is ready when:

- the mirror synchronization inventory is complete
- one domain plan names exact source files, tests, behavior gates, and commit boundaries
- that domain has frame-level scenario coverage before runtime ownership changes

---

## Completion Gate

Before marking any slice complete:

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
