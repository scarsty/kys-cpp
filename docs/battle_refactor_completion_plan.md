# Battle Refactor Execution Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to execute this plan task-by-task. Use `superpowers:test-driven-development` for every production-code change. Do not mark a task complete without running the task verification. Do not mark a slice complete without running the slice gate.

**Last updated:** 2026-05-03

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
- `src/BattleSceneHades.cpp:5833` `BattleSceneHades::defaultMagicEffect()` still owns the damage transaction.
- `src/BattleSceneHades.cpp:6527` `BattleSceneHades::applyScriptedAttackEffect()` still owns scripted projectile impacts.
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
- Cast config assert correction: committed casts now validate shared config plus only the selected operation slot, preserving `{}` placeholders for unrelated operation slots.
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

Focused verification:

```powershell
Select-String -Path src\BattleSceneHades.cpp -Pattern "void BattleSceneHades::backRun1" -Context 0,220
rg -n "defaultMagicEffect|applyScriptedAttackEffect|createSkillAttackEffect|calMagicHurt|HurtThisFrame|attack_effects_" src\BattleSceneHades.cpp
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

Focused verification:

```powershell
rg -n "shared_hit_group_targets_|makeBattleAttackWorld|writeBattleAttackWorld|PROJECTILE_|DASH_|MELEE_|HurtThisFrame|OperationType" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneAct.h
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
