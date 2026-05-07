# Battle Scene Remaining Backend Cuts Plan

## Goal
Remove the remaining gameplay/backend decisions from `BattleSceneHades` by cutting dependency paths in order. The scene may keep legacy `Role*` apply and rendering, but frame gameplay facts and value outputs must come from `Battle::BattleFrameResult`.

## Current Problem
The runtime is closer to owning battle state, but several scene paths still bypass the output contract:

- `BattleSceneHades::applyCoreFrameResult()` reads committed vectors from `battleRuntime()` directly.
- `applyCoreDamageTransactions()` reconstructs tracker lifecycle events from `battleRuntime().damage.lifecycleEvents`.
- hit handling in the scene applies freeze gameplay from `BattleAttackEventType::Hit`.
- `commitAutoUltimate()` still resolves and commits gameplay actions from the scene.
- blink log text is emitted by adapter apply instead of core action commit.

These are not rename problems. Each remaining cut must delete a scene-owned gameplay decision or runtime-internal read.

## Dependency Order

### 1. Add explicit frame apply outputs
**Purpose:** Give the scene one result object to consume before deleting runtime-internal reads.

**Files**
- `src/battle/BattleCore.h`
- `src/battle/BattleCore.cpp`
- `src/BattleSceneHades.cpp`
- `src/BattleSceneHades.h`
- `tests/BattleCoreUnitTests.cpp`
- `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

**Change**
- Add output fields to `BattleFrameResult`:
  - `damageTransactions`
  - `damageLifecycleEvents`
  - `rescueResults`
  - `teamEffectEvents`
  - `effectCommands`
  - `deathEffectTrackers`
- Populate them in `BattleFrameRunner::runFrame()` after the systems commit their frame outputs.
- Migrate `BattleSceneHades` to consume those fields.
- Keep runtime committed vectors temporarily for tests and core internals, but the scene must stop reading them.

**Required deletion/search target**
- `rg -n "damage\\.committed|damage\\.lifecycle|rescue\\.committed|teamEffects\\.committed|effects\\.committed|deathEffects\\.store" src\\BattleSceneHades.cpp`
- Expected after this task: no matches except initialization/configuration writes.

**Commit**
- `refactor: expose battle frame apply outputs`

### 2. Drive tracker from frame gameplay events
**Purpose:** Stop reconstructing battle lifecycle facts from damage lifecycle internals.

**Files**
- `src/BattleSceneHades.cpp`
- `src/BattleSceneHades.h`
- `src/BattleSceneBattleAdapter.cpp`
- `src/BattleSceneBattleAdapter.h`
- `src/battle/BattleCore.cpp`

**Change**
- Use `frameResult.frame.gameplayEvents` or an explicit `frameResult.gameplayEvents` field as tracker input.
- Delete lifecycle reconstruction in `applyCoreDamageTransactions()`.
- If the scene still needs a died-unit set for death visuals, derive it from the explicit gameplay events passed into the apply method, not from runtime damage internals.

**Required deletion/search target**
- `rg -n "BattleDamageLifecycleEventType|lifecycleEvents|BattleGameplayEventType::UnitDied|BattleGameplayEventType::BattleEnded" src\\BattleSceneHades.cpp`
- Expected after this task: no damage lifecycle reconstruction in scene.

**Commit**
- `refactor: drive tracker from frame gameplay events`

### 3. Move blink logs into core action commit
**Purpose:** Adapter apply should apply position/facing only; it should not create battle facts.

**Files**
- `src/battle/BattleCastSystem.h`
- `src/battle/BattleCastSystem.cpp`
- `src/BattleSceneBattleAdapter.cpp`
- `src/BattleSceneBattleAdapter.h`
- `tests/BattleCoreUnitTests.cpp`

**Change**
- Add `logEvents` to the core blink/action commit output if it does not already exist.
- Emit `"閃擊追殺"` and `"閃擊突襲"` from core when creating the blink teleport command.
- Delete `BattleLogEvent` creation from `applyBlinkTeleportDelta()`.

**Required deletion/search target**
- `rg -n "閃擊追殺|閃擊突襲|BattleLogEvent" src\\BattleSceneBattleAdapter.cpp`
- Expected after this task: no blink log construction in adapter.

**Commit**
- `refactor: emit blink logs from action commit`

### 4. Move hit impact freeze into core
**Purpose:** A projectile hit should not cause the scene to mutate gameplay status.

**Files**
- `src/battle/BattleCore.h`
- `src/battle/BattleCore.cpp`
- `src/battle/BattleStatusSystem.h`
- `src/battle/BattleStatusSystem.cpp`
- `src/BattleSceneHades.cpp`
- `src/BattleSceneHades.h`
- `tests/BattleCoreUnitTests.cpp`

**Change**
- Core applies hit impact freeze/stun using runtime status/combo/unit state.
- Preserve existing rules:
  - scripted impact only shakes/visuals.
  - main projectile applies freeze.
  - ultimate hit uses longer freeze.
  - low HP and shield/freeze resistance are gameplay status rules.
- Scene hit handling keeps only sound, shake, camera, rumble, and visual bookkeeping.
- Delete or shrink `applyFrozen()`.

**Required deletion/search target**
- `rg -n "applyFrozen|role->Frozen = 0|Frozen.*event" src\\BattleSceneHades.cpp src\\BattleSceneHades.h`
- Expected after this task: no scene gameplay freeze apply.

**Commit**
- `refactor: apply hit impact freeze in core`

### 5. Move auto ultimate commit into core
**Purpose:** The scene should not select magic, target, operation type, combo mutations, MP consumption, or attack spawns.

**Files**
- `src/battle/BattleCore.h`
- `src/battle/BattleCore.cpp`
- `src/battle/BattleCastSystem.h`
- `src/battle/BattleCastSystem.cpp`
- `src/BattleSceneHades.cpp`
- `src/BattleSceneHades.h`
- `src/BattleSceneBattleAdapter.cpp`
- `src/BattleSceneBattleAdapter.h`
- `tests/BattleCoreUnitTests.cpp`
- `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

**Change**
- Core consumes auto-ultimate requests from runtime action/combo state.
- Core selects the stored ultimate skill intent and target from runtime units.
- Core emits attack spawns, MP delta, combo updates, presentation logs/visuals, and gameplay commands.
- Scene only plays output and applies legacy deltas.
- Delete `BattleSceneHades::commitAutoUltimate()` once no frame path calls it.

**Required deletion/search target**
- `rg -n "commitAutoUltimate|commitBattleSelectedSkillAction|autoUltimateRequests" src\\BattleSceneHades.cpp src\\BattleSceneHades.h src\\BattleSceneBattleAdapter.cpp src\\BattleSceneBattleAdapter.h src\\battle`
- Expected after this task: no scene auto ultimate gameplay commit.

**Commit**
- `refactor: commit auto ultimate in core runner`

### 6. Final boundary cleanup
**Purpose:** Confirm the scene no longer prepares backend state or inspects runtime internals during frame apply.

**Required searches**
- `rg -n "battleRuntime\\(\\)\\.(damage|teamEffects|effects|rescue|deathEffects)" src\\BattleSceneHades.cpp`
- `rg -n "recordBattleStatusLog|recordFloatingTextPresentation|recordRoleEffectPresentation|recordDamageNumberPresentation" src\\BattleSceneHades.cpp`
- `rg -n "BattleLogEvent" src\\BattleSceneBattleAdapter.cpp src\\BattleSceneHades.cpp`
- `rg -n "Role\\*|Magic\\*|Item\\*|Engine|TextureManager|Font|Scene" src\\battle`

**Verification**
- `git diff --check`
- `.github\\build-command.ps1 -Solution "D:\\projects\\kys-cpp\\kys-cpp\\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal`
- `x64\\Debug\\kys_tests.exe`
- `.github\\build-command.ps1 -Solution "D:\\projects\\kys-cpp\\kys-cpp\\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal`

**Commit**
- `refactor: finish battle scene backend boundary cleanup`
