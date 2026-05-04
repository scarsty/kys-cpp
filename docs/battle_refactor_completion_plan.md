# Battle Refactor Execution Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to execute this plan task-by-task. Use `superpowers:test-driven-development` for every production-code change. Do not mark a task complete without running the task verification. Do not mark a slice complete without running the slice gate.

**Last updated:** 2026-05-04

**Goal:** Finish the battle refactor by moving battle decisions and state transitions out of `BattleSceneHades` into deterministic, headless systems under `src/battle`.

**Architecture:** `BattleSceneHades` becomes an adapter: build core input from legacy runtime state, call a core frame runner, apply committed state back during migration, and play presentation events. Core systems own movement, skill/cast selection, projectile resolution, damage transactions, combo triggers, death effects, and battle result events. `AttackEffect` becomes visual playback state only.

**Tech Stack:** C++23, MSBuild solution targets `kys_tests` and `kys`, Catch2 unit tests, existing `src/battle` core modules.

---

## Execution Rules

- Execute tasks in order. Later tasks assume earlier ownership boundaries are in place.
- Keep this document current as work proceeds. After each completed task, review loop, slice gate, or meaningful scope correction, update `## Current Progress` and append verification evidence under `## Verification Log` in the same branch.
- For each task:
  - Write or update the focused test first.
  - Run the focused test and confirm it fails for the intended missing behavior.
  - Implement the smallest production change.
  - Run the focused test and confirm it passes.
  - Run nearby tests listed for that task.
- Run the slice gate only after all tasks in that slice pass.
- Do not check final completion boxes because a build passed. Builds are verification gates, not architecture completion.
- Do not preserve backwards compatibility for migrated paths. Once a path is core-owned, delete the duplicate scene logic.
- Do not copy-paste behavior into tests. Use pinned fixtures for stats/ranges/speeds/effects when behavior depends on real game data.
- Do not treat bridge expansion as completion. A path is not migrated if the scene still decides gameplay semantics and then converts them into a core payload. Prefer deleting the scene decision and constructing the core input at the gameplay origin.
- Do not add paired overloads that duplicate eligibility or normalization logic across legacy and core representations. If a helper needs both `AttackEffect` and core request overloads, that is a temporary smell to remove before the task can be marked complete.
- Treat core request types as authoritative after construction. Validate invariants at the owner that creates the request, then downstream systems should consume the request or assert impossible state; do not re-check the same gameplay eligibility through broad defensive branches.

## Current Baseline

Current extracted core:

- `BattleMovementPlanner`, `BattleMovementSimulator`, and real-stat movement regressions.
- `BattleCombatIntentPlanner` helper coverage.
- `BattleAttackSystem` for projectile movement, target selection, hit eligibility, shared hit bookkeeping, expiration, tracking, bounce spawning, and projectile-cancel pair events.
- `BattleDamageSystem` helper functions for damage modifiers, defense layers, damage taken, kill rewards, cooldown extension, execute checks, on-hit resources, poison, bleed, and debuffs.
- `BattleStatusSystem`, `BattleTeamEffectSystem`, `BattleDeathEffectSystem`, `BattleEffectSystem`, and `BattleComboTriggerSystem` helper coverage.
- `BattlePresentationFrame`, `BattlePresentationRecorder`, and playback command mapping for logs/text/role effects/damage numbers/projectile lifecycle categories.
- Initial `BattleFrameRunner::advanceFrame()` covering movement plus projectile update only.

Current scene blockers:

- `src/BattleSceneHades.cpp:3147` `BattleSceneHades::backRun1()` still owns frame orchestration.
- `src/BattleSceneHades.cpp:4312` `BattleSceneHades::Action()` still owns cast timing and action state mutation.
- `src/BattleSceneHades.cpp:4521` `BattleSceneHades::createSkillAttackEffect()` still creates gameplay projectile state in `AttackEffect`.
- `BattleSceneHades::applyLegacyMagicHitTransaction()` still owns legacy damage calculation, reflection decisions, and damage presentation/log calls while routing committed damage through core transactions.
- `BattleSceneHades::applyScriptedHitTransaction()` still adapts scripted projectile impacts and queues their HP damage for the scene drain.
- `src/BattleSceneHades.cpp:756` and `src/BattleSceneHades.cpp:787` still bridge `AttackEffect` to and from `BattleAttackWorld`.

Baseline verification history:

- Last known `kys_tests` gate: passed, 70 test cases, 3141 assertions.
- Last known `kys` Debug x64 build: passed.
- These are baseline facts only. They do not complete any unfinished slice.

---

## Current Progress

- Slice 1: Complete and slice gate recorded.
- Slice 2: Complete and slice gate recorded.
- Slice 3 Task 3.1: Complete. Cast state/planner exists and selection readiness is honored.
- Slice 3 Task 3.2: Complete under manual review flow. Cast outputs include committed deltas/events/spawn requests, cast constants are explicit input config, attack vector thresholds are explicit adapter/test config, and focused/full tests pass.
- Slice 3 Task 3.3: Complete under manual flow. `AI()` builds `BattleCastInput` through the adapter, starts actions from `BattleCastPlanner`, and stores pending core cast results for `Action()` to commit. `Action()` consumes pending core cast spawn requests and applies result-derived MP/cooldown state. `createSkillAttackEffect()` is now a core spawn-request bridge instead of a scene-side skill/projectile decision tree.
- Slice 4 Task 4.1: Complete under manual flow. Core attack state and hit events now carry damage-request payload ids/values for source unit, skill, operation type, hidden weapon item id, scripted damage/status values, and execute/invincible behavior. Focused tests cover melee/ranged hit reach, through-hit behavior, payload propagation, and no on-hit bounce side effect without a hit event.
- Slice 4 Task 4.2: Complete under manual flow. Projectile cancellation events now carry both source unit ids and both operation-scaled cancel damage payloads. Ultimate and ignore-cancel rules remain core-owned, deterministic pair ordering is pinned, and the scene uses the core operation-damage multiplier during migration.
- Slice 4 Task 4.3: Complete under the current migration boundary. Core owns hit target selection, target lost, projectile cancellation pair decisions, and projectile bounce target/spawn decisions. Scene still consumes core hit/cancel events to invoke legacy damage functions until Slice 5 replaces `defaultMagicEffect()` and `applyScriptedAttackEffect()` with core damage transactions.
- Slice 5 Task 5.1: Complete under manual TDD flow. `BattleDamageSystem` now exposes a damage transaction API that composes existing modifier, defense, damage-taken, execute, MP damage, death-prevention, and kill-reward helpers into deltas/events without scene objects.
- Slice 5 Task 5.2: Complete under manual TDD flow. Damage transactions now emit accepted-hit resource, cooldown, and status deltas/events for HP/MP restore, MP drain, cooldown extension, poison, bleed, damage-reduce debuff, frozen, and MP block. Blocked hits do not trigger these on-hit effects.
- Slice 5 Task 5.3: Complete under manual review flow. Scripted hit status handling now goes through `BattleDamageSystem::resolveTransaction()` with accepted zero-damage hits, including freeze resistance/control-immunity inputs derived from combo state. Legacy hit-side resource/status/cooldown branches now route through accepted-hit damage transactions, with MP block/recovery bonus derived from combo state. Final MP-damage application now routes through a damage transaction. Legacy, reflected, current-HP blast, scripted HP, poison tick, and bleed tick hit sites now enqueue named pre-resolved HP damage transactions. The pending HP damage drain aggregates per target, commits through core damage transactions, derives HP damage presentation from committed `DamageApplied` events, and no longer uses `HurtThisFrame` in `BattleSceneHades`.
- Slice 5: Complete and slice gate recorded.
- Slice 6 Task 6.1: Complete under manual review flow. `BattleComboTriggerSystem` now defines battle runtime hooks for frame, cast, attack, damage, death, and battle lifecycle points, and can emit deterministic trigger event records with source/target/effect payloads. Focused tests cover hook coverage, effect-order dispatch, same-seed deterministic event streams, and max-count consumption only after activation.
- Slice 6 Task 6.2: Complete under manual review flow. `BattleEffectSystem` shared executors now cover Traditional Chinese registry keys for heal, shield, resource restore, cooldown reduction, dedicated effects, and shared invincibility labels for `受傷無敵`, `死亡無敵`, and `技能後無敵`. Focused tests cover target selection, per-executor mutation/events, named invincibility aliases, resource deltas, dedicated event payloads, and activation limits.
- Slice 6 Task 6.3: Complete under manual review flow. `BattleSceneHades` and `BattleSceneBattleAdapter` no longer directly scan legacy `triggeredEffects` or legacy `Trigger::` branches for battle-runtime trigger decisions. Scene code now builds `BattleComboTriggerInput`, queries or collects core trigger events, records activation only after the migrated action actually commits, and routes post-skill invincibility through the shared `技能後無敵` effect executor while keeping out-of-battle combo/economy setup outside the core battle runtime.
- Slice 6: Complete and slice gate recorded.
- Slice 7 Task 7.1: Complete under manual review flow. `BattleFrameState` now composes headless frame-owned state for movement, attacks, cast planning, damage transactions, status ticks, combo trigger state, death effects, battle result state, and pending attack spawns. Presentation snapshots now overlay committed status state for matching units instead of reporting combat fields as zero.
- Slice 7 Task 7.2: Complete under manual review flow. `BattleFrameRunner::advanceFrame()` now runs status ticks, movement, cast planning, attack spawn/update, queued damage transactions, death effects, and one-shot battle result emission in a deterministic frame order. Cast inputs are refreshed from committed frame world/status state before planning, committed cast outputs feed attack spawn requests, damage transactions update world/status/death-effect state, and damage/death/result facts are emitted as gameplay events before presentation playback.
- Slice 7 Task 7.3: Superseded/delegated. This document records the historical Slice 7.3 path and checkpoint evidence, but it is no longer the actionable finish-line plan. The earlier backend-exit completion claim was rejected after source review; continue remaining `BattleSceneHades` backend-exit work from `docs/battle_scene_hades_backend_exit_continuation_plan.md`.
- Boundary reset for 7.3: complete. The duplicated `primeProjectileBounce` overloads and generic `AttackEffect -> BattleAttackSpawnRequest` / `AttackEffect -> BattleAttackWorld` conversions have been removed. Frame combo/effect timer and unit idle-runtime decisions now have core-facing runtime APIs, with scene code applying emitted state/events. Scripted and legacy magic hit adaptation now consume core hit events instead of `AttackEffect`, and special hit-spawn helpers build requests from event payloads instead of cloning visual projectiles.
- Operation type cleanup: Core systems now use `BattleOperationType` instead of raw integer operation types. Legacy scene integers are converted only at adapter/presentation boundaries.
- Cast config assert correction: committed casts validate shared cooldown config plus only the selected operation slot, preserving `{}` placeholders for unrelated operation slots. Follow-up review trimmed nonessential operation-shape config asserts; remaining cast asserts guard active invariants such as selected operation ids, cooldown math, and normalization/division inputs.
- Workflow note: Subagent dispatch is paused because the review agents were unstable. Continue manually while preserving the same task order, local review discipline, and verification gates.
- Current user-feedback constraints carried forward:
  - Do not add source-text tests that assert words/phrases are absent.
  - Do not hard-code gameplay geometry when a source exists; derive scene geometry from `Scene::TILE_W` through the adapter.
  - Prefer explicit `{}` placeholders for fields assigned later; keep only meaningful default/sentinel values.
  - Prefer Traditional Chinese for source string literals.
  - Express conditional invariants as single `assert(!condition || invariant)` expressions rather than `if` blocks wrapping `assert`.

---

## Slice 1: Make The Frame Contract Complete Enough To Carry Gameplay

**Purpose:** Every migrated gameplay transition needs a deterministic event or state delta before scene code stops doing the work.

**Slice completion:** Core event types cover casts, projectile lifecycle, hit requests, damage transactions, status/resource deltas, death effects, and battle result. Presentation playback can map visual events without scene gameplay inference.

### Task 1.1: Split Gameplay Events From Presentation Events

**Files:**

- Modify: `src/battle/BattlePresentation.h`
- Modify: `src/battle/BattlePresentation.cpp`
- Modify: `src/battle/BattlePresentationPlayback.h`
- Modify: `src/battle/BattlePresentationPlayback.cpp`
- Test: `tests/BattlePresentationUnitTests.cpp`
- Test: `tests/BattlePresentationPlaybackUnitTests.cpp`

- [ ] Add a `BattleGameplayEventType` enum and `BattleGameplayEvent` struct for non-visual deterministic facts:
  - `CastStarted`
  - `AttackSpawned`
  - `ProjectileMoved`
  - `ProjectileHit`
  - `ProjectileExpired`
  - `ProjectileCancelled`
  - `DamageApplied`
  - `StatusApplied`
  - `ResourceChanged`
  - `UnitDied`
  - `BattleEnded`
- [ ] Keep `BattlePresentationEvent` for visual/log playback only.
- [ ] Extend `BattlePresentationFrame` to store both `gameplayEvents` and `presentationEvents`.
- [ ] Update `BattlePresentationRecorder` with `recordGameplay()` and `recordPresentation()`.
- [ ] Test frame stamping and order preservation separately for gameplay and presentation events.
- [ ] Update playback planner to consume only presentation events.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][presentation]"
```

### Task 1.2: Add Visual Projectile Playback Commands

**Files:**

- Modify: `src/battle/BattlePresentationPlayback.h`
- Modify: `src/battle/BattlePresentationPlayback.cpp`
- Modify: `src/BattleScenePresentationPlayer.h`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Test: `tests/BattlePresentationPlaybackUnitTests.cpp`

- [ ] Keep projectile visual commands in presentation playback:
  - `SpawnProjectile`
  - `MoveProjectile`
  - `ImpactProjectile`
  - `ExpireProjectile`
  - `CancelProjectile`
  - `BounceProjectile`
- [ ] Add payload fields needed by scene playback without consulting gameplay state:
  - visual effect id,
  - attack id,
  - source unit id,
  - target unit id,
  - position,
  - velocity,
  - duration frames,
  - operation/visual kind.
- [ ] Implement `BattleScenePresentationPlayer` handlers that update visual-only `AttackEffect` entries by event payload.
- [ ] Do not apply damage or modify role combat state in playback.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][presentation]"
```

### Slice 1 Gate

Run after Tasks 1.1 and 1.2:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Record the result under `## Verification Log`; do not mark final completion.

---

## Slice 2: Make Attack Instances Core-Owned And Visual Effects Playback-Only

**Purpose:** Remove gameplay state from `AttackEffect`. Core attacks should be data in `BattleAttackWorld`; visual effects should be created/updated only from presentation commands.

**Slice completion:** `BattleSceneHades` no longer writes gameplay fields into `AttackEffect`, and no gameplay branch reads `AttackEffect::Attacker`, `Defender`, `UsingMagic`, scripted payloads, bounce state, or target-tracking fields.

### Task 2.1: Add Core Attack Spawn Payload

**Files:**

- Modify: `src/battle/BattleAttackSystem.h`
- Modify: `src/battle/BattleAttackSystem.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`

- [ ] Add `BattleAttackSpawnRequest` with:
  - `attackerUnitId`,
  - `skillId`,
  - `operationType`,
  - `visualEffectId`,
  - `preferredTargetUnitId`,
  - `position`,
  - `velocity`,
  - `totalFrame`,
  - through/tracking/bounce/shared-hit flags.
- [ ] Add `BattleAttackSystem::spawn(BattleAttackWorld&, const BattleAttackSpawnRequest&)`.
- [ ] Emit gameplay `AttackSpawned` and presentation `SpawnProjectile` events from the frame runner, not from scene code.
- [ ] Tests:
  - spawn assigns deterministic attack ids,
  - spawn stores attacker/skill/operation payload,
  - spawn emits visual payload without `Role*` or `Magic*`.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack]"
```

### Task 2.2: Move Scene Attack Bridge Into A Dedicated Adapter

**Files:**

- Create: `src/BattleSceneBattleAdapter.h`
- Create: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Modify: `src/CMakeLists.txt`
- Modify: `src/BattleSceneHades.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] Move `makeBattleAttackWorld()` and `writeBattleAttackWorld()` out of `BattleSceneHades.cpp`.
- [ ] Adapter may read `Role*`, `Magic*`, and `AttackEffect` during migration.
- [ ] Core headers must remain free of `Role*`, `Magic*`, `Item*`, `Engine`, `TextureManager`, `Font`, SDL, and scene classes.
- [ ] Add a test using synthetic adapter input where possible; if direct adapter test is not practical due legacy types, add a search-based verification note and keep core tests focused.

Focused verification:

```powershell
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core]"
```

### Task 2.3: Delete Gameplay Reads From Visual Projectile Playback

**Files:**

- Modify: `src/BattleSceneAct.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleScenePresentationPlayer.cpp`
- Test: `tests/BattlePresentationPlaybackUnitTests.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`

- [ ] Remove or stop reading these `AttackEffect` fields for gameplay:
  - `Attacker`,
  - `Defender`,
  - `UsingMagic`,
  - `UsingHiddenWeapon`,
  - `OperationType`,
  - `NoHurt`,
  - `Track`,
  - `Through`,
  - `BounceRemaining`,
  - `ScriptedDamage`,
  - `ScriptedStunFrames`,
  - `ScriptedBleedStacks`.
- [ ] Keep only visual fields needed for rendering:
  - visual effect id,
  - frame/total frame,
  - position/velocity/acceleration,
  - follow role for visual-only role effects,
  - alpha/scale/render flags.
- [ ] Delete scene gameplay branches that become unreachable after core attack events carry the payload.

Focused verification:

```powershell
rg -n "\.Attacker|\.Defender|\.UsingMagic|\.UsingHiddenWeapon|ScriptedDamage|ScriptedStunFrames|ScriptedBleedStacks|BounceRemaining" src\BattleSceneHades.cpp src\BattleSceneAct.h
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack],[battle][presentation]"
```

### Slice 2 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 3: Extract Skill Selection And Cast Execution

**Purpose:** Scene code stops deciding when and what to cast. Core emits cast results and attack spawn requests as data.

**Slice completion:** `BattleSceneHades::Action()` no longer chooses skills or mutates cooldown/MP for cast execution. `BattleSceneHades::createSkillAttackEffect()` is removed or becomes visual adapter code only.

### Task 3.1: Add Cast State And Planner

**Files:**

- Create: `src/battle/BattleCastSystem.h`
- Create: `src/battle/BattleCastSystem.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/kys.vcxproj`
- Modify: `tests/kys_tests.vcxproj`
- Test: `tests/BattleCastSystemUnitTests.cpp`

- [ ] Add core structs:
  - `BattleCastUnitState`,
  - `BattleCastSkillState`,
  - `BattleCastInput`,
  - `BattleCastDecision`,
  - `BattleCastResult`.
- [ ] Implement selection rules:
  - ultimate may be selected only when `mp == maxMp`,
  - normal skill remains selected when ultimate is unavailable,
  - melee normal attack may pair with ranged ultimate,
  - arbitrary range-based rewrites are not allowed.
- [ ] Tests:
  - ultimate only casts at max MP,
  - normal skill remains behavior-compatible,
  - hybrid melee/ranged ultimate case,
  - no cast while dead/frozen/stunned/no target.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][cast]"
```

### Task 3.2: Convert Cast Result Into Attack Spawn Requests

**Files:**

- Modify: `src/battle/BattleCastSystem.h`
- Modify: `src/battle/BattleCastSystem.cpp`
- Modify: `src/battle/BattleAttackSystem.h`
- Test: `tests/BattleCastSystemUnitTests.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`

- [ ] Cast output must include:
  - cooldown delta,
  - MP delta,
  - cast animation timing,
  - `BattleAttackSpawnRequest` list,
  - gameplay `CastStarted` event,
  - presentation events for cast text/effects.
- [ ] Tests:
  - cooldown and MP changes after cast,
  - post-skill invincibility hook triggers after any skill cast, not only ultimate,
  - cast output is deterministic for same seed/input.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][cast],[battle][attack]"
```

### Task 3.3: Replace Scene Cast Logic With Adapter Calls

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: `tests/BattleCastSystemUnitTests.cpp`

- [ ] Adapter builds `BattleCastInput` from `Role`, `Magic`, combo state, cooldown, MP, target, operation type, and visual ids.
- [ ] Scene applies committed cast deltas from core result back to `Role` during migration.
- [ ] Delete skill choice branches from `Action()`.
- [ ] Delete or shrink `createSkillAttackEffect()` to visual-only compatibility until Slice 2 removes it fully.

Focused verification:

```powershell
rg -n "createSkillAttackEffect|Ultimate|MP|CoolDown|OperationType" src\BattleSceneHades.cpp
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][cast]"
```

### Slice 3 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 4: Move Hit-To-Damage Requests Out Of Scene

**Purpose:** Projectile hits become core damage requests, not scene calls into `defaultMagicEffect()` or `applyScriptedAttackEffect()`.

**Slice completion:** `BattleSceneHades` no longer loops over `attack_effects_` to decide who takes gameplay damage.

### Task 4.1: Add Attack Hit Payloads

**Files:**

- Modify: `src/battle/BattleAttackSystem.h`
- Modify: `src/battle/BattleAttackSystem.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`

- [ ] Extend `BattleAttackInstance` with damage request payload ids, not legacy pointers:
  - source unit id,
  - skill id,
  - operation type,
  - hidden weapon item id when applicable,
  - scripted damage/status payload id or explicit value,
  - flags for execute/invincible behavior.
- [ ] Extend `BattleAttackEvent` hit events with enough data to create a damage request.
- [ ] Tests:
  - melee hit only emits when hit volume reaches target,
  - ranged hit only emits when projectile reaches target,
  - through projectile emits one hit per target,
  - on-hit effects cannot trigger without hit event.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack]"
```

### Task 4.2: Move Projectile Cancellation Damage Scaling To Core

**Files:**

- Modify: `src/battle/BattleAttackSystem.h`
- Modify: `src/battle/BattleAttackSystem.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`

- [ ] Add core cancellation result:
  - left attack id,
  - right attack id,
  - left source unit id,
  - right source unit id,
  - scaled damage requests for both sides.
- [ ] Preserve current ultimate immunity and ignore-cancel rules.
- [ ] Tests:
  - projectile cancellation damage scaling,
  - ultimate projectile does not cancel,
  - ignored projectile does not cancel,
  - deterministic pair ordering.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack]"
```

### Task 4.3: Delete Scene Hit Resolution Loop

**Files:**

- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Test: `tests/BattleAttackSystemUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] Replace the `attack_effects_` gameplay hit loop in `backRun1()` with consumption of core hit/damage-request events.
- [ ] Keep visual cleanup in scene, but base it on playback state, not gameplay outcomes.
- [ ] Delete scene branches for projectile target lost, bounce target choice, and cancellation pair decision.

Focused verification:

```powershell
rg -n "defaultMagicEffect|applyScriptedAttackEffect|ProjectileCancel|BattleAttackEventType::Hit|attack_effects_" src\BattleSceneHades.cpp
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][attack],[battle][core]"
```

### Slice 4 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 5: Consolidate Damage Transactions In Core

**Purpose:** Replace `defaultMagicEffect()` and `applyScriptedAttackEffect()` with a tested core damage transaction pipeline.

**Slice completion:** Scene code no longer applies combat damage, status-on-hit, cooldown extension, reflection, execute, lifesteal, kill rewards, shield, invincibility, death prevention, or damage text creation.

### Task 5.1: Add Damage Transaction API

**Files:**

- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Test: `tests/BattleDamageSystemUnitTests.cpp`

- [ ] Add:
  - `BattleDamageRequest`,
  - `BattleDamageTransactionInput`,
  - `BattleDamageTransactionResult`,
  - `BattleUnitDelta`,
  - `BattleDamageEvent`.
- [ ] Compose existing helper methods into one transaction method.
- [ ] Tests:
  - physical damage,
  - MP damage,
  - shield absorption,
  - damage reduction/amplification,
  - invincibility prevents damage,
  - death prevention,
  - execute threshold,
  - reflection does not recursively create invalid cooldown stacks,
  - kill rewards.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage]"
```

### Task 5.2: Add Status And Resource Deltas To Damage Transactions

**Files:**

- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Modify: `src/battle/BattleStatusSystem.h`
- Test: `tests/BattleDamageSystemUnitTests.cpp`
- Test: `tests/BattleEffectSystemUnitTests.cpp`

- [ ] Transaction emits deltas/events for:
  - stun/frozen,
  - poison,
  - bleed,
  - damage-reduce debuff,
  - MP drain,
  - HP/MP on-hit restore,
  - cooldown extension only from valid hit events.
- [ ] Tests:
  - cooldown cannot be extended by a unit that did not actually attack/hit,
  - on-hit resources apply only after accepted damage/hit transaction,
  - stronger poison refreshes, weaker poison does not,
  - bleed stacks to cap.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage],[battle][effects]"
```

### Task 5.3: Replace Scene Damage Functions

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: `tests/BattleDamageSystemUnitTests.cpp`

- [ ] Adapter builds `BattleDamageTransactionInput` from legacy role/config/combo state.
- [ ] Core result applies deltas back to `Role` during migration.
- [ ] Presentation damage log and damage number come from core events.
- [ ] Delete `defaultMagicEffect()` and `applyScriptedAttackEffect()` once all call sites are gone.

Focused verification:

```powershell
rg -n "defaultMagicEffect|applyScriptedAttackEffect|HurtThisFrame|logBattleDamage|addDamageText|LastAttacker" src\BattleSceneHades.cpp
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][damage]"
```

### Slice 5 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 6: Centralize Combo Trigger Runtime

**Purpose:** Scene code stops scattering trigger checks through action, hit, damage, death, and frame paths.

**Slice completion:** Combo trigger evaluation and effect execution are core-owned for battle hooks. Scene only adapts config/state and applies committed deltas.

### Task 6.1: Define Battle Trigger Hooks

**Files:**

- Modify: `src/battle/BattleComboTriggerSystem.h`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Test: `tests/BattleComboTriggerSystemUnitTests.cpp`

- [ ] Add hook enum values:
  - `FrameTick`,
  - `BeforeCast`,
  - `AfterSkillCast`,
  - `AttackLaunched`,
  - `ProjectileHitEnemy`,
  - `ProjectileHitAllyOrSource`,
  - `DamageTaken`,
  - `DamageDealt`,
  - `UnitDeath`,
  - `AllyDeath`,
  - `BattleStart`,
  - `BattleEnd`.
- [ ] Add deterministic trigger event output.
- [ ] Tests:
  - trigger dispatch ordering,
  - same seed produces same trigger event stream,
  - trigger max counts are consumed only on activation.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][combo]"
```

### Task 6.2: Move Shared Executors Into Core

**Files:**

- Modify: `src/battle/BattleEffectSystem.h`
- Modify: `src/battle/BattleEffectSystem.cpp`
- Modify: `src/battle/BattleComboTriggerSystem.cpp`
- Test: `tests/BattleEffectSystemUnitTests.cpp`
- Test: `tests/BattleComboTriggerSystemUnitTests.cpp`

- [ ] Ensure shared executor coverage for:
  - `受傷無敵`,
  - `死亡無敵`,
  - `技能後無敵`,
  - heal/shield/resource/cooldown modifiers,
  - special dedicated effects that should not become generic.
- [ ] Tests:
  - one unit-level test per executor,
  - special effects emit explicit events/deltas,
  - economy/out-of-battle hooks are excluded from battle runtime.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][effects],[battle][combo]"
```

### Task 6.3: Delete Scene Trigger Branches

**Files:**

- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/ChessBattleEffects.h`
- Test: `tests/BattleComboTriggerSystemUnitTests.cpp`
- Test: `tests/BattleEffectSystemUnitTests.cpp`

- [ ] Scene builds trigger input and applies deltas only.
- [ ] Delete scene-side trigger calls that now map to core hooks.
- [ ] Keep explicit out-of-battle economy/special behavior outside core battle runtime.

Focused verification:

```powershell
rg -n "trigger|Trigger|combo|Combo|受傷無敵|死亡無敵|技能後無敵" src\BattleSceneHades.cpp
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][combo],[battle][effects]"
```

### Slice 6 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 7: Own The Full Frame In `BattleFrameRunner`

**Purpose:** One core method advances one deterministic battle frame and returns one committed snapshot/event frame.

**Slice completion:** `BattleFrameRunner::advanceFrame()` owns status/effects, movement, casts, attack spawn/update, hit resolution, damage transactions, death effects, battle result, and frame event emission.

### Task 7.1: Expand `BattleFrameState`

**Files:**

- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] Add frame state fields for:
  - movement world,
  - attack world,
  - cast state,
  - damage state,
  - status state,
  - combo trigger state,
  - battle result state.
- [ ] Keep `BattleFrameState` headless. No legacy pointers.
- [ ] Tests:
  - no `Role*` or scene dependency needed to construct a frame,
  - snapshot includes committed unit state after systems run.

Focused verification:

```powershell
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core]"
```

### Task 7.2: Implement Full Frame Order

**Files:**

- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleCore.h`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] Implement order:
  1. tick status/effects,
  2. plan movement,
  3. plan casts,
  4. spawn attacks,
  5. update attacks,
  6. resolve hit damage transactions,
  7. resolve deaths/death effects,
  8. resolve battle result,
  9. emit committed snapshot/events.
- [ ] Tests:
  - full frame deterministic replay,
  - death effects occur after damage and before result,
  - no action while frozen/stunned/dead,
  - movement and cast order does not allow attacks from impossible positions,
  - battle-end event emits once.

Focused verification:

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][core]"
```

### Task 7.3: Replace `backRun1()` With Adapter Pipeline

> **Superseded/delegated:** This historical 7.3 task is no longer the executable finish-line plan. The first backend-exit finish-line plan is also historical because its completion claim was rejected. Use `docs/battle_scene_hades_backend_exit_continuation_plan.md` for the remaining work from the current branch state. Keep this section only as context and checkpoint history.

**Files:**

- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Test: existing core tests plus build gate.

- [ ] `backRun1()` does only:
  - handle scene-only pause/camera/render timing,
  - build `BattleFrameState`,
  - call `BattleFrameRunner::advanceFrame()`,
  - apply committed deltas back to legacy roles during migration,
  - call `BattleScenePresentationPlayer`,
  - remove expired visual playback objects.
- [ ] Delete duplicated movement/projectile/damage/status/death/battle-result algorithms from scene.

#### 7.3 Boundary-Reset Checkpoint

The current branch state is a checkpoint that can be committed after verification. It is allowed to leave `BattleSceneHades` large, but the commit must not claim 7.3 is complete. Remaining implementation must make `BattleSceneHades` an adapter by moving battle decisions into core systems and using presentation events only for visual/log playback.

#### Boundary Reset Requirements

The current migration must be corrected before 7.3 can be completed. Do not continue by adding more adapter payload fields or paired legacy/core helper overloads unless the same checkpoint also deletes the legacy-side gameplay decision.

- [x] Stop using `AttackEffect` as a gameplay input representation. Existing `AttackEffect` reads in gameplay branches must either become presentation reads or be replaced by core event/request data.
- [x] Remove duplicated bridge helpers such as paired `primeProjectileBounce(AttackEffect&)` and `primeProjectileBounce(BattleAttackSpawnRequest&)`. Bounce eligibility, chance, range, and roll consumption must have one owner. Prefer core-owned bounce priming at projectile request creation time; the scene may supply only explicit legacy inputs such as attacker id and RNG roll when unavoidable.
- [x] Do not make request consumers repeat request-construction decisions. Once `BattleAttackSpawnRequest` is built, consumers should trust it as the gameplay input and assert impossible invariants; they should not branch over both "legacy `AttackEffect` source" and "core request source" shapes.
- [x] Build `BattleAttackSpawnRequest` directly at gameplay origins:
  - cast planner,
  - item use,
  - scripted effect executor,
  - combo effect executor.
  Do not create an `AttackEffect` and then convert it into a request.
- [x] Delete generic `AttackEffect -> BattleAttackSpawnRequest` conversion once direct request creation is in place. If a temporary converter remains during migration, it must be listed in `## Current Progress` as an open blocker, not treated as completed architecture.
- [ ] Move hit damage adaptation out of the projectile event loop. `BattleAttackEvent::Hit` should carry the data needed to build a core damage transaction; scene code may play sound, camera, rumble, and visual reactions, but must not recompute gameplay damage or trigger gameplay side effects from `AttackEffect`.
- [ ] Move frame combo/effect timer decisions out of `backRun1()`. Scene may apply presentation output and legacy state writes, but low-HP burst, trigger timers, ramping idle decay, post-skill invincibility, team heal, regen, aura, and MP/cooldown tick decisions must be core-owned or clearly isolated in a core-facing system.
- [x] Keep assertions for invariants instead of broad defensive checks. Do not add boundary `if` guards to make invalid bridge data silently no-op.
- [x] After each migrated path, delete the old scene branch in the same checkpoint. Do not leave both the old scene algorithm and the new core path active.

#### 7.3 Completion Bar

- [ ] `backRun1()` is short enough to inspect in one screenful and reads as an adapter pipeline, not a second battle engine.
- [x] `rg -n "attack_effects_" src\BattleSceneHades.cpp` shows rendering, presentation playback, adapter application, and visual cleanup only.
- [x] `rg -n "AttackEffect .*;|makeCoreAttackSpawnRequest|primeProjectileBounce" src\BattleSceneHades.cpp src\BattleSceneHades.h` does not show gameplay bridge construction paths.
- [ ] `rg -n "HP|MP|CoolDown|Dead|Invincible|LastAttacker|Poison|Bleed|Shield" src\BattleSceneHades.cpp` is reviewed and remaining hits are either rendering/presentation or committed core-result application.
- [ ] `rg -n "logBattle|addFloatingText|addDamageText|TextEffect|recordDamage|recordStatus" src\BattleSceneHades.cpp` is reviewed and remaining hits are presentation only, not gameplay branch decisions.

Focused verification:

```powershell
Select-String -Path src\BattleSceneHades.cpp -Pattern "void BattleSceneHades::backRun1" -Context 0,220
rg -n "defaultMagicEffect|applyScriptedAttackEffect|createSkillAttackEffect|calMagicHurt|HurtThisFrame|attack_effects_" src\BattleSceneHades.cpp
rg -n "AttackEffect .*;|makeCoreAttackSpawnRequest|primeProjectileBounce" src\BattleSceneHades.cpp src\BattleSceneHades.h
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

### Slice 7 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Slice 8: Delete Legacy Bridges And Prove Boundaries

**Purpose:** Remove compatibility scaffolding and duplicate constants after ownership has moved.

**Slice completion:** Scene is an adapter plus renderer. Core has no forbidden dependencies. Duplicate gameplay constants and scene helper algorithms are gone.

### Task 8.1: Remove Temporary Bridge State

**Files:**

- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneAct.h`
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] Delete bridge maps/fields that only existed to mirror scene state into core.
- [ ] Delete duplicate projectile/movement/damage constants from scene if core owns them.
- [ ] Keep shared constants in `src/battle/BattleGeometry.h` or another core config header.
- [ ] Delete temporary `AttackEffect` gameplay payload fields after all gameplay branches stop reading them:
  - `Attacker`,
  - `PreferredTarget`,
  - `Defender`,
  - `UsingMagic`,
  - `UsingHiddenWeapon`,
  - `OperationType`,
  - `Weaken`,
  - `Strengthen`,
  - `Track`,
  - `Through`,
  - `NoHurt`,
  - `IsUltimate`,
  - `IsMain`,
  - `ScriptedDamage`,
  - `ScriptedStunFrames`,
  - `ScriptedBleedStacks`,
  - `SharedHitGroupId`,
  - `IgnoreProjectileCancel`,
  - `RequirePreferredTarget`,
  - `SuppressNearbyTrackingProjectileProc`,
  - bounce and spiral gameplay fields.
- [ ] Delete temporary adapter conversion functions once `AttackEffect` is visual-only:
  - `makeBattleAttackWorld`,
  - `writeBattleAttackWorld`,
  - `makeCoreAttackSpawnRequest`,
  - any `AttackEffect` gameplay request bridge.
- [ ] Delete `shared_hit_group_targets_` after shared-hit ownership lives entirely in `BattleAttackWorld` or another core-owned runtime.

Focused verification:

```powershell
rg -n "shared_hit_group_targets_|makeBattleAttackWorld|writeBattleAttackWorld|makeCoreAttackSpawnRequest|PROJECTILE_|DASH_|MELEE_|HurtThisFrame|OperationType|UsingMagic|UsingHiddenWeapon|ScriptedDamage|SharedHitGroupId|SuppressNearbyTrackingProjectileProc" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneAct.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h
```

### Task 8.2: Boundary Search Gate

**Files:**

- Modify only files found by the searches below.

- [ ] Search for direct writes to combat state from scene code.
- [ ] Search for direct event/log/text spawning from gameplay branches.
- [ ] Search for forbidden dependencies in core.

Required searches:

```powershell
rg -n "HP|MP|CoolDown|HurtThisFrame|Dead|Invincible|LastAttacker|Poison|Bleed|Shield" src\BattleSceneHades.cpp
rg -n "logBattle|addFloatingText|addDamageText|TextEffect|recordDamage|recordStatus" src\BattleSceneHades.cpp
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected result:

- Scene search may show adapter application code and rendering only.
- Core forbidden dependency search returns no matches.

### Slice 8 Gate

```powershell
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

---

## Final Acceptance

Run this only after all slices are complete:

```powershell
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
rg -n "defaultMagicEffect|applyScriptedAttackEffect|createSkillAttackEffect|calMagicHurt|HurtThisFrame|attack_effects_" src\BattleSceneHades.cpp
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Completion is valid only if:

- `src/battle` has no forbidden dependencies.
- `BattleSceneHades::backRun1()` is a short adapter pipeline.
- Scene code does not choose skills, resolve projectile hits, apply damage, evaluate combo triggers, tick status, resolve death effects, or decide battle result.
- Presentation playback consumes events and does not infer gameplay state.
- `kys_tests` passes.
- `kys` builds in Debug x64. If final linking fails only because the running game executable locks `kys.exe`, compile success before the locked executable is acceptable.

## Verification Log

Append entries here after each slice gate. Do not use this log as a completion checklist.

- 2026-05-03 baseline before executable-plan rewrite: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe` passed 70 test cases / 3141 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 1 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe` passed 77 test cases / 3259 assertions; `kys` Debug x64 build passed after restoring local `mlcc` submodule files in the isolated worktree.
- 2026-05-03 Slice 2 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe` passed 84 test cases / 3349 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 3 Task 3.2 manual gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][cast],[battle][attack],[battle][core]"` passed 51 test cases / 527 assertions; `x64\Debug\kys_tests.exe` passed 107 test cases / 3637 assertions; `rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene|SDL" src\battle` returned no matches; hidden cast constants and vector thresholds are explicit config/input.
- 2026-05-03 Slice 3 Task 3.3 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][cast]"` passed 20 test cases / 282 assertions; core forbidden dependency search returned no matches; cast assert/source-constant spot checks returned no bad assert wrappers, stale `unit.actProperty`, or `BattleTileWidth`; `kys` Debug x64 build passed. Abort reporting was adjusted so Windows test aborts keep stderr output while suppressing the report dialog.
- 2026-05-03 Slice 3 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe` passed 109 test cases / 3646 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 4 Task 4.1 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack]"` passed 20 test cases / 124 assertions; `x64\Debug\kys_tests.exe "[battle][cast]"` passed 21 test cases / 289 assertions after narrowing cast config asserts to the selected operation; `kys` Debug x64 build passed.
- 2026-05-03 Slice 4 Task 4.2 gate: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack],[battle][core]"` passed 40 test cases / 292 assertions; after assert-trim review, `x64\Debug\kys_tests.exe "[battle][cast],[battle][attack],[battle][core]"` passed 61 test cases / 581 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 4 Task 4.3 bounce checkpoint: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack],[battle][core],[battle][presentation]"` passed 51 test cases / 399 assertions; `kys` Debug x64 build passed.
- 2026-05-03 operation enum checkpoint: `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][cast],[battle][attack],[battle][core],[battle][damage]"` passed 73 test cases / 654 assertions; core/test raw operation-type integer spot check returned only the explicit legacy conversion helper; `kys` Debug x64 build passed.
- 2026-05-03 Slice 4 gate: `x64\Debug\kys_tests.exe` passed 117 test cases / 3691 assertions; search gate showed scene hit/cancel event consumption only as the temporary legacy damage bridge, while target lost, bounce selection, and projectile-cancel pair decisions are core-owned; `kys` Debug x64 build passed.
- 2026-05-03 Slice 5 Task 5.1 gate: test-first compile failed on missing `BattleDamageTransactionInput`, `BattleDamageEventType`, and `BattleDamageSystem::resolveTransaction`; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 17 test cases / 132 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 5 Task 5.2 gate: test-first compile failed on missing transaction resource/status fields and event types; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage],[battle][effects]"` passed 23 test cases / 203 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 5 Task 5.3 scripted-status checkpoint: test-first compile failed on missing accepted zero-damage hit support; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 22 test cases / 187 assertions; `kys` Debug x64 build passed with existing warnings; `rg -n "defaultMagicEffect|applyScriptedAttackEffect" src\BattleSceneHades.cpp src\BattleSceneHades.h` returned no matches.
- 2026-05-03 Slice 5 Task 5.3 delayed-damage checkpoint: test-first compile failed on missing pre-resolved damage support; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 23 test cases / 196 assertions; `kys` Debug x64 build passed with existing warnings; `rg -n "applyDamageTaken\(|applyKillReward\(|defaultMagicEffect|applyScriptedAttackEffect" src\BattleSceneHades.cpp src\BattleSceneHades.h` returned no matches.
- 2026-05-03 Slice 5 Task 5.3 hit-side resource/status checkpoint: test-first compile failed on missing `BattleDamageUnitState::mpRecoveryBonusPct` and `BattleDamageUnitState::mpBlocked`; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage],[battle][effects]"` passed 27 test cases / 235 assertions; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 24 test cases / 204 assertions; `kys` Debug x64 build passed with existing warnings; `rg -n "applyOnHitResources\(|applyPoisonIfStronger\(|extendActiveCooldown\(|applyBleed\(|applyDamageReduceDebuff\(" src\BattleSceneHades.cpp src\BattleSceneHades.h` returned no matches.
- 2026-05-03 Slice 5 Task 5.3 pre-resolved HP queue checkpoint: legacy, reflected, current-HP blast, scripted HP, poison tick, and bleed tick hit sites now call `queuePreResolvedHpDamage()` instead of writing `HurtThisFrame` directly; the drain aggregates pending transaction damage in local frame maps instead of transporting through `HurtThisFrame`; execute projection includes pending transaction damage; `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 24 test cases / 204 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 5 broad checkpoint after HP queue narrowing: `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 129 test cases / 3822 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 5 Task 5.3 completion: delayed HP damage presentation/death handling now runs after committed core damage transactions; pending HP damage aggregates per target before commit; zero-damage HP hits no longer enqueue asserting pending damage; `BattleSceneHades` has no remaining `HurtThisFrame`, `semanticDamageTextAmounts_`, `execution_popup_roles_`, `defaultMagicEffect`, or `applyScriptedAttackEffect` references; `kys_tests` Debug x64 build passed for the active worktree; `x64\Debug\kys_tests.exe "[battle][damage]"` passed 24 test cases / 204 assertions.
- 2026-05-03 Slice 5 gate: `rg -n "defaultMagicEffect|applyScriptedAttackEffect|HurtThisFrame|logBattleDamage|addDamageText|LastAttacker" src\BattleSceneHades.cpp` showed only adapter/drain `LastAttacker` and committed transaction log usage; helper searches for scene calls to `applyDamageTaken`, `applyKillReward`, `applyOnHitResources`, `applyPoisonIfStronger`, `extendActiveCooldown`, `applyBleed`, and `applyDamageReduceDebuff` returned no matches; `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 129 test cases / 3822 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 6 Task 6.1 gate: test-first compile failed on missing `BattleComboTriggerHook`, `BattleComboTriggerInput`, `BattleComboTriggerEvent`, and `BattleComboTriggerSystem::collectTriggerEvents`; after implementation and review-gap coverage, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][combo]"` passed 10 test cases / 77 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 6 Task 6.2 gate: test-first compile failed on missing `BattleEffectCommandType::ModifyResource`; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][effects],[battle][combo]"` passed 15 test cases / 129 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 6 Task 6.3 gate: `rg -n "triggeredEffects|collectChanceEffects\(|collectTeamHeal\(|Trigger::" src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.h` returned no matches; `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][combo],[battle][effects]"` passed 18 test cases / 155 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 6 gate: `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 138 test cases / 3906 assertions; `kys` Debug x64 build passed.
- 2026-05-03 Slice 7 Task 7.1 gate: test-first compile failed on missing `BattleFrameState` cast/damage/status/combo/death/result fields, then tightened snapshot test failed because committed status fields were still zero; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][core]"` passed 19 test cases / 174 assertions; `rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle` returned no matches; `kys` Debug x64 build passed with existing linker option warnings.
- 2026-05-03 Slice 7 Task 7.2 gate: test-first focused runner tests failed on missing status tick, cast commit, damage commit, and battle-end emission; follow-up tests failed until cast inputs were refreshed from committed world/status state before planning; after implementation, `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][core]"` passed 25 test cases / 211 assertions; `x64\Debug\kys_tests.exe "[battle][core],[battle][cast],[battle][damage],[battle][death_effect]"` passed 73 test cases / 723 assertions; `rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle` returned no matches; `kys` Debug x64 build passed with existing linker option warnings.
- 2026-05-03 Slice 7 Task 7.3 projectile-runner checkpoint: scene projectile update now builds a `BattleFrameState`, calls `BattleFrameRunner::advanceFrame()`, consumes `frameResult.attackEvents`, and writes the committed `BattleAttackWorld` back to `AttackEffect` state; `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 146 test cases / 3959 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 7 Task 7.3 status/damage/death runner checkpoint: direct scene calls to `BattleStatusSystem`, `BattleDamageSystem::resolveTransaction()`, and `BattleDeathEffectSystem::applyAllyDeathEffects()` were replaced with `BattleFrameRunner::advanceFrame()` phases plus adapter writes; added core regression coverage that death-effect state sees committed kill rewards before ally-death effects; corrected status-damage source lookup so source-less poison/bleed events do not hit the asserting adapter lookup; `kys_tests` Debug x64 build passed; focused `BattleFrameRunner_AdvanceFrame_DeathEffectWorldSeesCommittedDamageRewards` passed 1 test case / 8 assertions; full `x64\Debug\kys_tests.exe` passed 147 test cases / 3967 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-03 Slice 7 Task 7.3 cast-spawn runner checkpoint: `Action()` and `queueCoreSkillAttackSpawn()` now queue `BattleAttackSpawnRequest` payloads for the projectile frame-runner phase instead of constructing cast-driven `AttackEffect` entries directly; `writeBattleAttackWorld()` can now materialize first-generation core-spawned visual effects from committed attack state, the obsolete cast-to-`AttackEffect` adapter helper was removed, and the old `createSkillAttackEffect` bridge name was dropped; `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 147 test cases / 3967 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-04 Slice 7 Task 7.3 battle-result runner checkpoint: `BattleFrameRunner` result state now accepts pending alive counts by team so queued enemies can suppress premature battle-end events; `backRun1()` resolves battle result through a core `BattleFrameState` instead of calling `checkResult()` directly; test-first build failed on missing `pendingAliveByTeam`, then `kys_tests` Debug x64 build passed; focused `BattleFrameRunner_AdvanceFrame_BattleResultCountsPendingTeamUnits` passed 1 test case / 6 assertions; `x64\Debug\kys_tests.exe "[battle][core]"` passed 27 test cases / 225 assertions; full `x64\Debug\kys_tests.exe` passed 148 test cases / 3973 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-04 Slice 7 Task 7.3 projectile-cancel/core-damage checkpoint: test-first build failed on missing `BattleAttackState::projectileCancelWeaken`, `BattleAttackSystem::applyProjectileCancelDamage`, `BattleMagicBaseDamageInput`, and `BattleDamageSystem::resolveMagicBaseDamage`; after implementation, projectile cancel weaken/expiry is core-owned, `AttackEffect::Weaken` is adapter-mapped, and `BattleSceneHades` no longer defines or calls `calMagicHurt`; `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack]"` passed 24 test cases / 142 assertions; `x64\Debug\kys_tests.exe "[battle][damage],[battle][attack]"` passed 49 test cases / 349 assertions.
- 2026-05-04 Slice 7 Task 7.3 movement-physics checkpoint: test-first build failed on missing `BattleMovementPhysicsInput` and `BattleMovementPhysicsSystem`; after implementation, the legacy per-role slide/friction/gravity/dash-runtime update is in `src/battle/BattleMovement` and `backRun1()` calls it as an adapter; `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][movement],[battle][attack],[battle][damage]"` passed 57 test cases / 3123 assertions.
- 2026-05-04 Slice 7 Task 7.3 hidden/basic projectile spawn checkpoint: hidden-weapon item attacks and the basic-skill auto projectile helper now queue `BattleAttackSpawnRequest` payloads instead of constructing first-generation gameplay `AttackEffect` entries directly; `writeBattleAttackWorld()` materializes hidden-weapon effects from committed core attack state; `x64\Debug\kys_tests.exe` passed 151 test cases / 3998 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-04 Slice 7 Task 7.3 scripted/clone projectile spawn checkpoint: `BattleAttackSpawnRequest` now carries initial frame, acceleration, spiral motion, strength, and nearby-tracking suppression payloads; nearby tracking, spiral bleed, area impact, and spread projectile helpers now queue core attack spawns instead of pushing gameplay `AttackEffect` entries directly; focused attack/core tests passed 51 test cases / 381 assertions; full `x64\Debug\kys_tests.exe` passed 151 test cases / 4012 assertions; `kys` Debug x64 build passed with existing warnings; `rg -n "attack_effects_\\.push_back|AttackEffect [a-zA-Z0-9_]+;" src\BattleSceneHades.cpp` now reports only the visual blood effect push plus local queued projectile variables.
- 2026-05-04 boundary reset plan update: user review identified that the migration was expanding coupling through duplicated bridge helpers and generic `AttackEffect`/core conversion rather than drawing firm ownership boundaries. Updated Execution Rules, Task 7.3, Slice 8 cleanup, and final search expectations so remaining work must delete bridge logic, build core requests at gameplay origins, remove duplicated `primeProjectileBounce`-style overloads, and keep 7.3 open until `backRun1()` is genuinely short adapter code.
- 2026-05-04 Slice 7 Task 7.3 bounce/request boundary reset checkpoint: test-first build failed on missing `applyProjectileBouncePrime`; after implementation, bounce priming payload writes are centralized in `BattleAttackSystem`, duplicated scene `primeProjectileBounce` overloads were removed, generic `makeCoreAttackSpawnRequest(const AttackEffect&)` and `queueCoreAttackSpawn(const AttackEffect&)` were deleted, scripted/spiral/area projectile origins now build `BattleAttackSpawnRequest` directly, and the scene search for `primeProjectileBounce|makeCoreAttackSpawnRequest|queueCoreAttackSpawn|AttackEffect .*;` reports only the committed-damage visual blood effect local. `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack]"` passed 25 test cases / 160 assertions.
- 2026-05-04 Slice 7 Task 7.3 combo-frame runtime checkpoint: test-first build failed on missing combo frame runtime API; after implementation, `BattleComboTriggerSystem::advanceFrameRuntime()` owns auto-ultimate timer readiness, periodic HP regen/aura readiness, frame-trigger actions, ramping idle decay, trigger timer countdowns, and last-alive flag updates, while `collectSkillFinishedRuntimeEvents()` owns post-skill invincibility readiness. `backRun1()` now consumes these core runtime events and applies presentation/legacy state effects instead of deciding those timers inline. `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][combo]"` passed 16 test cases / 126 assertions.
- 2026-05-04 Slice 7 Task 7.3 scripted-hit event checkpoint: scripted projectile hit handling now consumes `BattleAttackEvent` payloads for source, fixed damage, stun, and bleed instead of mutating or reading `AttackEffect` gameplay fields. Non-through/through consumption remains owned by `BattleAttackSystem` before the adapter writes visual state back. `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack],[battle][damage],[battle][combo]"` passed 66 test cases / 493 assertions.
- 2026-05-04 Slice 7 Task 7.3 projectile-cancel state checkpoint: projectile-cancel damage adaptation now reads committed `BattleAttackWorld` attack state and core event ids instead of `AttackEffect` visual state. Hidden-weapon spawn requests explicitly ignore projectile cancel to preserve the previous non-magic projectile behavior after deleting the generic `AttackEffect` converter. `kys_tests` Debug x64 build passed; `x64\Debug\kys_tests.exe "[battle][attack],[battle][core]"` passed 52 test cases / 385 assertions.
- 2026-05-04 Slice 7 Task 7.3 broad reset verification: scripted hit handling no longer indexes `attack_effects_`; projectile-cancel handling no longer reads visual `AttackEffect` state; hit audio uses core event skill ids. `rg -n "primeProjectileBounce|makeCoreAttackSpawnRequest|queueCoreAttackSpawn|AttackEffect .*;" src\BattleSceneHades.cpp src\BattleSceneHades.h` reports only the committed-damage blood visual local; `rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle` returned no matches; `git diff --check` passed; `kys_tests` Debug x64 build passed; full `x64\Debug\kys_tests.exe` passed 155 test cases / 4039 assertions; `kys` Debug x64 build passed with existing warnings.
- 2026-05-04 Slice 7 Task 7.3 rejected completion checkpoint: test-first build failed on missing hit-event payload fields for `mainProjectile`, `track`, `sharedHitGroupId`, and nearby-tracking suppression; after implementation, legacy magic hit handling and special hit-spawn helpers consume `BattleAttackEvent` payloads instead of `AttackEffect`, non-scripted hits no longer index `attack_effects_[event.attackId]`, extra projectile requests are built directly from event data, and `backRun1()` was reduced to a one-screen adapter pipeline over named frame phases. User review later rejected this as completion because `BattleSceneHades` still owns substantial battle decisions hidden in helpers and frame phases. `x64\Debug\kys_tests.exe "[battle][attack],[battle][damage],[battle][combo]"` passed 66 test cases / 506 assertions; full `x64\Debug\kys_tests.exe` passed 155 test cases / 4052 assertions; `kys` Debug x64 build passed with existing warnings; `git diff --check` passed.
- 2026-05-04 Slice 7 Task 7.3 review-fix checkpoint: review found that `BattleSceneBattleAdapter::makeBattleAttackInstance()` still rebuilt core gameplay attacks from visual `AttackEffect`, and that the shortened `backRun1()` hid scene-owned cooldown/MP/action-reset decisions inside a helper. Removed the visual-to-core attack bridge, added persistent scene-owned `BattleAttackWorld` as the migration gameplay state, recorded core projectile presentation events for visual `AttackEffect` playback, added `BattleFrameUnitRuntimeSystem` for cooldown/MP/physical-power/action-reset decisions, moved bounce request eligibility into `BattleAttackSystem::tryApplyProjectileBouncePrime()`, and converted touched Simplified Chinese source strings in `BattleSceneHades.cpp` to Traditional Chinese. Follow-up re-review found frozen units bypassed the runtime API; the runtime call now happens before the frozen movement/action skip, so frozen eligibility is supplied to core instead of decided by early scene control flow. Test-first builds failed on missing `BattleFrameUnitRuntimeSystem` and `tryApplyProjectileBouncePrime`; after implementation, `x64\Debug\kys_tests.exe "[battle][attack],[battle][core]"` passed 54 test cases / 412 assertions; `x64\Debug\kys_tests.exe "[battle][core]"` passed 28 test cases / 234 assertions after the frozen-order fix; full `x64\Debug\kys_tests.exe` passed 157 test cases / 4066 assertions; `kys` Debug x64 build passed with existing warnings; `git diff --check` passed.
