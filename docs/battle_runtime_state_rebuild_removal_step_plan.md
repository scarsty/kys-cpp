# Battle Runtime State Rebuild Removal Step Plan

> **For agentic workers:** execute one task at a time. Every task must either delete a per-frame rebuild/reset from `BattleSceneHades::buildCoreRuntimeContext()` or delete a duplicated persistent subsystem projection. A commit that only renames, wraps, or adds another sync layer fails this plan.

**Goal:** remove per-frame state rebuilding by making `BattleRuntimeState` own one canonical unit model, then make battle subsystems operate on that canonical model plus their private state or frame-local views.

**Design correction:** the recent `sync*` helpers are transitional. They proved where ownership leaks are, but they must not become the permanent architecture. Permanent runtime state should not keep five long-lived copies of HP/alive/shield/position/invincibility under names like `damage.units`, `teamEffects.world.units`, `effects.world.units`, `deathEffects.world.units`, and `projectileFollowUps.targets`. The endpoint is one canonical unit store and narrow subsystem extras.

---

## Target Runtime Shape

```cpp
struct BattleRuntimeState
{
    BattleUnitStore units;          // canonical value state by unitId
    BattleStatusState status;       // timers/debuffs/status-only data by unitId
    BattleComboState combo;         // combo-only data by unitId
    BattleAttackWorld attacks;      // active attacks/projectiles
    BattleMovementState movement;   // movement and physics runtime
    BattleResultState result;       // battle end state

    BattleFrameScratch scratch;     // current-frame inputs, random rolls, pending commands, outputs
};
```

### Canonical Unit Store Owns

`BattleUnitStore` should own values that are currently copied across subsystem structs:

- identity: `id`, `realRoleId`, `name`, `team`
- combat values: `hp`, `maxHp`, `mp`, `maxMp`, `attack`, `defence`
- runtime values: `alive`, `cooldown`, `cooldownMax`, `physicalPower`, `invincible`
- spatial values: `position`, `velocity`, `acceleration`, `grid`
- resource modifiers: `shield`, `mpBlocked`, `mpRecoveryBonusPct`
- targeting values: `canAttack`, `reach`, `style`, `facing`, `defenseWeight inputs if needed`

### Subsystems Own Only Private Extras

- `status`: poison/bleed/freeze/temp attack/damage reduction/control timers. No HP owner.
- `combo`: combo timers, trigger counters, applied effect definitions, adaptation/ramping stacks, shield extras if shield is not moved fully into `units`.
- `damage`: frame damage transactions, presentation styles, lifecycle output, unit effect definitions. No HP/alive owner.
- `teamEffects`: heal aura radius and pending/committed events. No persistent unit world.
- `effects`: activation counts and committed commands. No persistent unit world.
- `deathEffects`: regular synergy combo ids and death-effect trackers/effect definitions. No persistent unit world.
- `projectileFollowUps`: config and next shared hit id. No persistent target world.
- `actions`/`hits`/`projectileCancel`: frame scratch unless and until action/hit ownership is redesigned.

### Frame Scratch Owns

- random rolls
- current-frame runtime inputs
- pending damage transactions and presentation inputs
- action and hit inputs
- projectile cancel base-damage pairs
- committed result vectors
- presentation/log/gameplay outputs

Scratch may be rebuilt every frame. Persistent runtime owners may not.

---

## Current Problem Areas

`BattleSceneHades::buildCoreRuntimeContext()` still does all of this:

- rebuilds `projectileFollowUps`
- resets `runtime`, `damage`, `status`, `combo`, `projectileCancel`, `actions`, `hits`, `applications`
- calls `populateCoreStatusState()`
- calls `populateCoreStatusDamageState()`
- copies `ChessCombo::getMutableStates()` into `battleRuntime().combo.units`
- rebuilds action, rescue, movement physics, and hit inputs from `Role*`

Recent commits removed some world rebuilds, but did so by adding sync between duplicated projections. The new plan is to collapse those projections into `BattleUnitStore`.

---

## Task 1: Introduce `BattleUnitStore` Without Replacing Callers Yet

**Purpose:** create the canonical value model that later tasks can migrate to. This task must not add another persistent projection that lives forever beside the old ones; it is allowed only as the replacement target for the duplicates.

**Files:**
- Modify `src/battle/BattleCore.h`
- Modify `src/battle/BattleCore.cpp`
- Modify `src/BattleSceneBattleAdapter.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Add `BattleUnitStateCore` or `BattleRuntimeUnit` with the canonical fields listed above.
2. Add `BattleUnitStore`:
   - `std::vector<BattleRuntimeUnit> units`
   - lookup helpers by `unitId`
   - mutation helpers for damage, heal, MP, cooldown, shield, invincibility, alive, position.
3. Add tests:
   - can initialize a store with two units and mutate HP/alive by id.
   - mutating position updates grid through a core-owned grid transform.
   - missing unit lookup is an invariant violation in test helpers or asserts, not silent defensive behavior.
4. Initialize `battleRuntime().units` once in `initializeBattleRuntimeSession()` from `Role*`/combo state.
5. Keep existing subsystem worlds for this task, but add a search note in code/doc that they are transitional.
6. Verify and commit: `refactor: introduce canonical battle unit store`

**Search gate:**

```powershell
rg -n "BattleUnitStore|BattleRuntimeUnit" src\battle src\BattleSceneHades.cpp tests
```

Expected: canonical store exists and is initialized once.

---

## Task 2: Replace Projection Sync With Store Mutations

**Purpose:** stop writing sync logic from one projection to another. Core systems should mutate `BattleUnitStore`; transitional projections should be built only as frame-local views when required by an old subsystem API.

**Files:**
- Modify `src/battle/BattleCore.cpp`
- Modify `src/battle/BattleCore.h`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Add tests:
   - damage transaction updates `runtime.units` HP/alive/invincible.
   - status tick updates `runtime.units.invincible` and MP-block flags.
   - team heal/MP/shield/cooldown commands update `runtime.units`.
   - movement physics/rescue updates `runtime.units.position` and grid.
2. Change core mutation sites to update `state.units` first:
   - `applyDamageResultToFrameState`
   - `syncRescueDamageUnit`
   - `syncRescuePosition`
   - `syncTeamEffectEventsToFrameState`
   - `advanceStatus`
   - `advanceMovementPhysics`
   - runtime MP/cooldown advance
3. Leave existing `syncTeamEffect*`, `syncEffect*`, and projectile target syncs temporarily, but make them read from `state.units` or frame-local views.
4. Add a comment near each remaining sync helper: `// Transitional projection bridge; delete when subsystem reads BattleUnitStore.`
5. Verify and commit: `refactor: route unit mutations through battle unit store`

**Search gate:**

```powershell
rg -n "sync.*TeamEffect|sync.*Effect|syncProjectile|Transitional projection bridge" src\battle\BattleCore.cpp
```

Expected: remaining sync helpers are explicitly marked transitional and only bridge from canonical store to old APIs.

---

## Task 3: Convert Team Effects To Read `BattleUnitStore`

**Purpose:** delete persistent `teamEffects.world.units` and the sync calls that keep it alive.

**Files:**
- Modify `src/battle/BattleTeamEffectSystem.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneBattleAdapter.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Change `BattleTeamEffectSystem` to accept `BattleUnitStore&` or a `BattleUnitView` range.
2. Keep `TeamEffectState` as:
   - `double healAuraRadius`
   - `pendingCommands`
   - `committedEvents`
   - no `BattleTeamEffectWorld world`
3. Update tests for:
   - team heal reads team/alive/maxHp from canonical units.
   - heal aura reads canonical positions.
   - MP restore respects canonical `mpBlocked`/`mpRecoveryBonusPct`.
4. Delete:
   - `BattleTeamEffectWorld` from persistent runtime state.
   - `makeBattleTeamEffectWorld()` runtime initialization use.
   - `syncTeamEffectUnit`, `syncTeamEffectPosition`, `syncTeamEffectStatus`.
5. Scene apply uses committed events and canonical unit state:
   - `target->HP = runtime.units.byId(id).hp`
   - `target->MP = runtime.units.byId(id).mp`
   - `target->CoolDown = runtime.units.byId(id).cooldown`
6. Verify and commit: `refactor: make team effects use battle unit store`

**Search gate:**

```powershell
rg -n "BattleTeamEffectWorld|teamEffects\.world|syncTeamEffect" src tests
```

Expected: no persistent team effect world or sync helpers. If a `BattleTeamEffectWorld` compatibility type remains, it must be local to tests or deleted in the same task.

---

## Task 4: Convert Generic Effects To Read `BattleUnitStore`

**Purpose:** delete persistent `effects.world.units` and effect sync calls.

**Files:**
- Modify `src/battle/BattleEffectSystem.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Change `BattleEffectDispatcher::dispatch(...)` to operate on canonical units plus effect activation counts.
2. Keep `EffectState` as:
   - activation counts if needed
   - committed commands
   - no `BattleEffectWorld world`
3. Add tests:
   - post-skill invincibility reads current canonical invincibility and emits only the delta.
   - effect heal/shield/resource/cooldown commands mutate canonical units.
4. Delete:
   - `BattleEffectWorld` from persistent runtime state.
   - `makeBattleEffectWorld()` initialization use.
   - `findBattleEffectUnit()` scene helper if no longer needed.
   - `syncEffectUnit`, `syncEffectStatus`, `syncEffectFromTeamEffectUnit`.
5. Scene apply uses canonical unit values for committed effect commands.
6. Verify and commit: `refactor: make generic effects use battle unit store`

**Search gate:**

```powershell
rg -n "BattleEffectWorld|effects\.world|syncEffect|findBattleEffectUnit|makeBattleEffectWorld" src tests
```

Expected: no persistent effect world or sync helpers.

---

## Task 5: Convert Projectile Follow-Up Targeting To Read `BattleUnitStore`

**Purpose:** delete `projectileFollowUps.targets` as persistent duplicated target state and remove `battleRuntime().projectileFollowUps = makeCoreProjectileFollowUpContext();`.

**Files:**
- Modify `src/battle/BattleProjectileTargetingSystem.h/.cpp`
- Modify `src/battle/BattleHitResolver.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Make projectile targeting operate on `BattleUnitStore` or a frame-local target view derived from it inside core.
2. Keep `BattleProjectileFollowUpContext` as config only:
   - projectile speed
   - padding/distance config
   - next shared hit group id
   - no `targets` world
3. Add tests:
   - movement changes canonical position, then nearby/area follow-up uses the new position.
   - damage death marks canonical unit dead, then follow-up targeting skips it.
   - invincible/defense values come from canonical unit state.
4. Delete:
   - `makeBattleProjectileTargetWorld()` or restrict it to one-time import if still needed during migration.
   - per-frame `battleRuntime().projectileFollowUps = makeCoreProjectileFollowUpContext();`
   - any projectile target sync helper added earlier.
5. Keep only:
   - `battleRuntime().projectileFollowUps.nextSharedHitGroupId = next_shared_hit_group_id_;`
   until that scene field is migrated.
6. Verify and commit: `refactor: make projectile follow ups use unit store`

**Search gate:**

```powershell
rg -n "projectileFollowUps\.targets|makeBattleProjectileTargetWorld|makeCoreProjectileFollowUpContext|projectileFollowUps = makeCoreProjectileFollowUpContext" src tests
```

Expected: no per-frame target rebuild and no persistent projectile target projection.

---

## Task 6: Convert Death Effects To Use Canonical Units Plus Death Extras

**Purpose:** delete persistent `deathEffects.world.units` as another HP/alive/shield/attack projection.

**Files:**
- Modify `src/battle/BattleDeathEffectSystem.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Split `BattleDeathEffectUnit` into:
   - `BattleDeathEffectExtras`: combo ids, applied effects, shield-on-ally-death tracker, shieldPctMaxHp if not moved to combo extras.
   - canonical unit values read from `BattleUnitStore`.
2. Change `BattleDeathEffectSystem::applyAllyDeathEffects(...)` to take canonical units plus death extras.
3. Add tests:
   - death medical heal mutates canonical HP.
   - ally stat boost mutates canonical attack/defence.
   - shield-on-ally-death tracker persists in death extras without duplicating shield owner.
4. Delete:
   - `deathEffects.world.units`
   - `makeBattleDeathEffectWorld()` use as a unit projection.
   - `applyDamageUnitToDeathEffectUnit` sync path.
5. Keep `writeBattleDeathEffectTrackers()` only for legacy combo mirror until combo migration removes it.
6. Verify and commit: `refactor: make death effects use unit store`

**Search gate:**

```powershell
rg -n "BattleDeathEffectWorld|deathEffects\.world\.units|makeBattleDeathEffectWorld|applyDamageUnitToDeathEffectUnit|findDeathEffectUnit" src tests
```

Expected: no death-effect unit projection. A remaining death-effect world/config type may contain only extras and combo ids.

---

## Task 7: Make Status Persistent But Not A Unit Projection

**Purpose:** delete `battleRuntime().status = {};` and `populateCoreStatusState(...)`, while ensuring status owns only status-specific data.

**Files:**
- Modify `src/battle/BattleStatusSystem.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Remove HP/maxHP/attack/invincible/alive from persistent status data where possible.
2. When status needs HP or invincibility, pass canonical unit state to the status tick input.
3. Status output mutates canonical units through explicit commands/results.
4. Add tests:
   - poison/bleed timers persist across two frames without scene repopulation.
   - periodic invincibility updates canonical unit invincibility.
   - temp attack expiration updates canonical attack.
5. Initialize status extras once from legacy combo/role data.
6. Replace:
   - `battleRuntime().status = {};`
   - `populateCoreStatusState(battleRuntime());`
   with:
   - `clearStatusFrameScratch(battleRuntime());`
7. Verify and commit: `refactor: make status extras persistent`

**Search gate:**

```powershell
rg -n "battleRuntime\(\)\.status = \{\}|populateCoreStatusState\(|BattleStatusUnitState.*hp|BattleStatusUnitState.*alive" src tests
```

Expected: no scene status rebuild. Any remaining unit fields in status must be function-local input, not persistent runtime owner state.

---

## Task 8: Split Damage Persistent Config From Damage Frame Scratch

**Purpose:** delete `battleRuntime().damage = {};` and `populateCoreStatusDamageState(...)`.

**Files:**
- Modify `src/battle/BattleDamageSystem.h/.cpp`
- Modify `src/battle/BattleDamageApplicationSystem.h/.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Damage application reads attacker/defender values from `BattleUnitStore`.
2. Damage state keeps only:
   - persistent damage effect definitions/config, presentation styles
   - frame scratch transactions/presentation/output
3. Add tests:
   - damage HP/alive changes persist in canonical store across frames.
   - cooldown and shield interactions use canonical values.
   - death AOE/unit effects remain available after multiple frames without scene rebuild.
4. Replace:
   - `battleRuntime().damage = {};`
   - `populateCoreStatusDamageState(...)`
   with:
   - `clearDamageFrameScratch(battleRuntime());`
5. Delete duplicated `damage.units`, `damage.statusUnits`, `damage.cooldowns` persistent owners.
6. Verify and commit: `refactor: make damage use canonical unit store`

**Search gate:**

```powershell
rg -n "battleRuntime\(\)\.damage = \{\}|populateCoreStatusDamageState|damage\.units|damage\.statusUnits|damage\.cooldowns" src tests
```

Expected: no persistent duplicated damage unit/cooldown/status stores. Pending transaction value snapshots may remain only as frame inputs.

---

## Task 9: Make Combo Runtime Authoritative And `ChessCombo` A Legacy Mirror

**Purpose:** delete `battleRuntime().combo = {};` and `battleRuntime().combo.units = comboStates;`.

**Files:**
- Modify `src/BattleSceneHades.cpp`
- Modify `src/battle/BattleCore.h/.cpp`
- Modify adapter functions that still read `ChessCombo::getMutableStates()` for frame input.
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Initialize `battleRuntime().combo` once.
2. Core reads/writes combo state from runtime.
3. Scene mirrors runtime combo back to `ChessCombo::getMutableStates()` only after frame apply for legacy rendering/save-facing code.
4. Add tests:
   - combo trigger timer persists across frames without external map assignment.
   - shield/activation counters persist in runtime.
5. Replace:
   - `battleRuntime().combo = {};`
   - `battleRuntime().combo.units = comboStates;`
   with:
   - `clearComboFrameScratch(battleRuntime());`
6. Verify and commit: `refactor: make runtime combo state authoritative`

**Search gate:**

```powershell
rg -n "battleRuntime\(\)\.combo = \{\}|battleRuntime\(\)\.combo\.units = comboStates|ChessCombo::getMutableStates\(\)" src\BattleSceneHades.cpp src\BattleSceneBattleAdapter.cpp
```

Expected: no per-frame combo import. Remaining `ChessCombo::getMutableStates()` calls are legacy apply/setup only.

---

## Task 10: Separate Runtime Frame Scratch From Persistent Runtime State

**Purpose:** delete whole-owner resets that are actually scratch resets: `runtime = {}`, `projectileCancel = {}`, `actions = {}`, `hits = {}`, `applications = {}`.

**Files:**
- Modify `src/battle/BattleCore.h/.cpp`
- Modify `src/BattleSceneHades.cpp`
- Modify `src/BattleSceneBattleAdapter.cpp`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Add `BattleFrameScratch`:
   - runtime frame inputs and random rolls
   - action inputs/results
   - hit inputs/results
   - projectile cancel base damages/results
   - applications
2. Runner creates or clears scratch per frame.
3. Persistent `BattleRuntimeState` no longer stores action/hit/projectile-cancel scratch as owner fields.
4. Add tests:
   - repeated `runFrame()` cannot reuse stale action/hit/projectile-cancel input.
   - committed frame outputs are available through `BattleFrameResult`, not persistent runtime owner fields.
5. Replace scene resets:
   - `battleRuntime().runtime = {};`
   - `battleRuntime().projectileCancel = {};`
   - `battleRuntime().actions = {};`
   - `battleRuntime().hits = {};`
   - `battleRuntime().applications = {};`
6. Verify and commit: `refactor: move frame scratch out of runtime owner state`

**Search gate:**

```powershell
rg -n "battleRuntime\(\)\.(runtime|projectileCancel|actions|hits|applications) = \{\}|struct ActionState|struct HitState|struct ProjectileCancelState|struct ApplicationState" src\battle\BattleCore.h src\BattleSceneHades.cpp
```

Expected: no scratch owner structs in persistent runtime and no scene whole-object scratch resets.

---

## Task 11: Shrink Remaining Scene Frame Imports

**Purpose:** after canonical state and scratch separation, remove scene lookups that core can do from runtime state.

**Files:**
- Modify `src/BattleSceneHades.cpp`
- Modify `src/BattleSceneBattleAdapter.cpp`
- Modify `src/battle/*`
- Test `tests/BattleCoreUnitTests.cpp`

**Implementation steps:**
1. Move nearest/farthest enemy selection from `buildBattleActionFrameImport()` into core using `BattleUnitStore`.
2. Move action target snapshots to canonical unit state.
3. Move cast frame/static skill value data into value catalogs initialized at setup or action boundary.
4. Keep `Magic*`/`Item*` only in boundary code that creates value snapshots.
5. Delete scene-side frame imports that no longer provide external input.

**Search gate:**

```powershell
rg -n "findNearestEnemy|findFarthestEnemy|buildBattleActionFrameImport|BattleFrameActionImport|Role\*|Magic\*|Item\*" src\battle src\BattleSceneBattleAdapter.cpp src\BattleSceneHades.cpp
```

Expected: no `Role*`/`Magic*`/`Item*` in `src\battle`; remaining scene adapter uses are explicit legacy boundary imports.

---

## Final Search Gates

```powershell
rg -n "battleRuntime\(\)\.(runtime|damage|status|combo|projectileCancel|actions|hits|applications) = \{\}" src\BattleSceneHades.cpp
rg -n "populateCoreStatusState|populateCoreStatusDamageState|makeCoreProjectileFollowUpContext" src\BattleSceneHades.cpp
rg -n "teamEffects\.world|effects\.world|deathEffects\.world\.units|projectileFollowUps\.targets|damage\.units|damage\.statusUnits|damage\.cooldowns" src tests
rg -n "syncTeamEffect|syncEffect|syncProjectile|applyDamageUnitToDeathEffectUnit" src\battle
rg -n "buildBattleFrameSceneImport|BattleFrameSceneImport|buildCoreFrameBundle|BattleFrameLegacySnapshot" src tests
rg -n "Role\*|Magic\*|Item\*|Engine|TextureManager|Font|Scene" src\battle
```

Expected:
- No whole-owner reset in `BattleSceneHades::buildCoreRuntimeContext()`.
- No duplicated persistent unit projections.
- No projection sync helpers.
- No legacy frame bundle/snapshot path.
- No forbidden scene/engine/legacy pointers in `src\battle`.

## Verification Commands

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

## Commit Discipline

Each commit must satisfy at least one:

- deletes a duplicated persistent subsystem unit projection;
- deletes a per-frame assignment from `buildCoreRuntimeContext()`;
- moves a subsystem to read/write `BattleUnitStore`;
- moves scratch out of persistent runtime state.

Commits that only add sync bridges are allowed only in Task 2 and must mark those bridges as transitional. Any later task that keeps a bridge must delete at least one old projection in the same commit.
