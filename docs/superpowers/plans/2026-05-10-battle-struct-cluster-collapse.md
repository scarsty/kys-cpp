# Battle Struct Cluster Collapse Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Collapse remaining duplicated battle struct field clusters into focused shared value objects without adding inheritance, compatibility accessors, or broad DTOs.

**Architecture:** Reuse existing `BattleUnitVitals`, `BattleUnitStats`, `BattleUnitMotion`, and `BattleUnitAnimationState` where a struct truly carries the same concept. Add one new status-only value object for duplicated status timers. Leave intentionally narrow subsystem DTOs flat unless they carry the same owned cluster.

**Tech Stack:** C++20, MSVC, Catch2, existing `.github\build-command.ps1` verification.

---

## File Map

- `src/battle/BattleStatusSystem.h`
  - Add `BattleStatusEffectState`.
  - Replace duplicated status effect/timer fields in `BattleStatusUnitState` and `BattleStatusRuntimeUnit`.

- `src/battle/BattleStatusSystem.cpp`
  - Update tick helpers, runtime/status conversion helpers, and status runtime writes to use `effects`.

- `src/battle/BattleDamageSystem.h`
  - Replace vitals fields in `BattleDamageUnitState` and `BattleResourceUnitState` with `BattleUnitVitals`.
  - Keep `attack` as a scalar because damage state does not need full `BattleUnitStats`.

- `src/battle/BattleDamageSystem.cpp`
  - Update damage resolution/resource code to read and write `vitals`.

- `src/battle/BattleHitResolver.h`
  - Replace duplicated hit-unit vitals/stats/motion/animation fields with shared value objects.

- `src/battle/BattleHitResolver.cpp`
  - Update hit resolution reads to use grouped fields.

- `src/battle/BattleCore.cpp`
  - Update projections into status, damage, hit, presentation, and initialization outputs.

- `src/battle/BattleInitialization.h`
  - Replace `BattleInitializationRoleDelta` scalar vitals/stat fields with `BattleUnitVitals` and `BattleUnitStats`.

- `src/battle/BattleInitialization.cpp`
  - Populate role deltas and clone intent deltas using grouped fields.

- Scene/adapter tests:
  - `tests/BattleInitializationUnitTests.cpp`
  - `tests/BattleCoreUnitTests.cpp`
  - existing status/damage/hit resolver tests if present.

---

## Non-Goals

- Do not introduce inheritance or base classes.
- Do not add compatibility getters like `hp()` just to hide renamed fields.
- Do not collapse `id/team/alive` into a generic identity/presence struct in this batch; that repetition is not yet proven to reduce code.
- Do not force `BattleUnitStats` into damage structs that only need `attack`.
- Do not force `BattleUnitMotion` into structs that only need `position`.
- Do not change runtime/scene ownership boundaries.

---

## Task 1: Collapse Status Effect Timers

**Files:**
- Modify: `src/battle/BattleStatusSystem.h`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Test: status system tests, or add to `tests/BattleCoreUnitTests.cpp` if no dedicated file exists.

- [ ] **Step 1: Write failing status conversion test**

Add a test proving status-only fields move as a single effect state between frame status and runtime status:

```cpp
TEST_CASE("BattleStatusSystem_CopiesStatusEffectsAsACluster", "[battle][status]")
{
    BattleStatusUnitState source;
    source.id = 7;
    source.effects.poisonTimer = 9;
    source.effects.poisonTickPct = 5;
    source.effects.poisonSourceId = 3;
    source.effects.bleedStacks = 2;
    source.effects.frozenTimer = 4;
    source.effects.mpBlockTimer = 6;
    source.effects.tempAttackBuffs.push_back({ 11, 12 });
    source.effects.damageReduceDebuffs.push_back({ 13, 14 });

    auto runtime = makeBattleStatusRuntimeUnit(source);

    CHECK(runtime.id == 7);
    CHECK(runtime.effects.poisonTimer == 9);
    CHECK(runtime.effects.poisonTickPct == 5);
    CHECK(runtime.effects.poisonSourceId == 3);
    CHECK(runtime.effects.bleedStacks == 2);
    CHECK(runtime.effects.frozenTimer == 4);
    CHECK(runtime.effects.mpBlockTimer == 6);
    REQUIRE(runtime.effects.tempAttackBuffs.size() == 1);
    CHECK(runtime.effects.tempAttackBuffs[0].attackBonus == 11);
    REQUIRE(runtime.effects.damageReduceDebuffs.size() == 1);
    CHECK(runtime.effects.damageReduceDebuffs[0].pct == 14);
}
```

Expected red: `effects` does not exist.

- [ ] **Step 2: Add `BattleStatusEffectState`**

In `BattleStatusSystem.h`, add after `DamageReduceDebuff`:

```cpp
struct BattleStatusEffectState
{
    int poisonTimer = 0;
    int poisonTickPct = 0;
    int poisonSourceId = -1;

    int bleedStacks = 0;
    int bleedTimer = 0;
    int bleedSourceId = -1;

    int frozenTimer = 0;
    int frozenMaxTimer = 0;
    int freezeReductionPct = 0;
    int shieldFreezeResPct = 0;
    int controlImmunityFrames = 0;
    int mpBlockTimer = 0;

    int damageImmunityAfterFrames = 0;
    int damageImmunityDuration = 0;
    int damageImmunityTimer = 0;

    std::vector<TimedAttackBuff> tempAttackBuffs;
    std::vector<DamageReduceDebuff> damageReduceDebuffs;
};
```

Then add `BattleStatusEffectState effects;` to both `BattleStatusUnitState` and `BattleStatusRuntimeUnit`.

- [ ] **Step 3: Migrate status code to `effects`**

In `BattleStatusSystem.cpp`, replace status effect field reads/writes:

```cpp
status.poisonTimer -> status.effects.poisonTimer
status.poisonTickPct -> status.effects.poisonTickPct
status.poisonSourceId -> status.effects.poisonSourceId
status.bleedStacks -> status.effects.bleedStacks
status.bleedTimer -> status.effects.bleedTimer
status.bleedSourceId -> status.effects.bleedSourceId
status.frozenTimer -> status.effects.frozenTimer
status.frozenMaxTimer -> status.effects.frozenMaxTimer
status.freezeReductionPct -> status.effects.freezeReductionPct
status.shieldFreezeResPct -> status.effects.shieldFreezeResPct
status.controlImmunityFrames -> status.effects.controlImmunityFrames
status.mpBlockTimer -> status.effects.mpBlockTimer
status.damageImmunityAfterFrames -> status.effects.damageImmunityAfterFrames
status.damageImmunityDuration -> status.effects.damageImmunityDuration
status.damageImmunityTimer -> status.effects.damageImmunityTimer
status.tempAttackBuffs -> status.effects.tempAttackBuffs
status.damageReduceDebuffs -> status.effects.damageReduceDebuffs
```

Make conversion helpers copy the cluster directly:

```cpp
runtime.effects = unit.effects;
frame.effects = status.effects;
status.effects = unit.effects;
```

- [ ] **Step 4: Delete duplicated status fields**

Remove the individual status effect fields from both status structs. Keep only:

```cpp
int id = -1;
bool alive = true;       // only on BattleStatusUnitState
int hp = 0;              // only on BattleStatusUnitState
int maxHp = 0;           // only on BattleStatusUnitState
int attack = 0;          // only on BattleStatusUnitState
int invincible = 0;      // only on BattleStatusUnitState
BattleStatusEffectState effects;
```

Runtime status keeps:

```cpp
int id = -1;
BattleStatusEffectState effects;
```

- [ ] **Step 5: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleStatusSystem_CopiesStatusEffectsAsACluster"
```

- [ ] **Step 6: Search for old status fields**

Run:

```powershell
rg -n "\.(poisonTimer|poisonTickPct|poisonSourceId|bleedStacks|bleedTimer|bleedSourceId|frozenTimer|frozenMaxTimer|freezeReductionPct|shieldFreezeResPct|controlImmunityFrames|mpBlockTimer|damageImmunityAfterFrames|damageImmunityDuration|damageImmunityTimer|tempAttackBuffs|damageReduceDebuffs)\b" src\battle src tests
```

Expected: references should go through `.effects.` except for struct declarations inside `BattleStatusEffectState`.

---

## Task 2: Use `BattleUnitVitals` in Damage and Resource State

**Files:**
- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: damage system tests or add focused tests in `tests/BattleCoreUnitTests.cpp`.

- [ ] **Step 1: Write failing damage vitals test**

Add a test that proves damage state uses grouped vitals:

```cpp
TEST_CASE("BattleDamageSystem_UsesGroupedVitalsForDamageState", "[battle][damage]")
{
    BattleDamageUnitState defender;
    defender.id = 1;
    defender.alive = true;
    defender.vitals = { 30, 100, 5, 20 };

    BattleDamageSystem system;
    auto result = system.applyDamageTaken(defender, 10);

    CHECK(result.defender.vitals.hp == 20);
    CHECK(result.defender.vitals.maxHp == 100);
    CHECK(result.defender.vitals.mp == 5);
    CHECK(result.defender.alive);
}
```

Expected red: `BattleDamageUnitState::vitals` does not exist.

- [ ] **Step 2: Add grouped vitals to damage structs**

In `BattleDamageSystem.h`, replace:

```cpp
int hp = 0;
int maxHp = 0;
int mp = 0;
int maxMp = 0;
```

with:

```cpp
BattleUnitVitals vitals;
```

for both `BattleDamageUnitState` and `BattleResourceUnitState`.

Include `BattleCore.h` or move the value object definitions to a smaller shared header if include cycles appear. Prefer including `BattleCore.h` only if it does not create a cycle; otherwise create `src/battle/BattleUnitValues.h` containing the four value structs and include that from `BattleCore.h`, `BattleDamageSystem.h`, `BattleHitResolver.h`, and `BattleInitialization.h`.

- [ ] **Step 3: Migrate damage implementation**

Replace:

```cpp
unit.hp -> unit.vitals.hp
unit.maxHp -> unit.vitals.maxHp
unit.mp -> unit.vitals.mp
unit.maxMp -> unit.vitals.maxMp
```

in `BattleDamageSystem.cpp` and damage-related sections of `BattleCore.cpp`.

Keep scalar `attack` on `BattleDamageUnitState`.

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleDamageSystem_UsesGroupedVitalsForDamageState"
```

- [ ] **Step 5: Search for damage scalar vitals**

Run:

```powershell
rg -n "\b(result|input|unit|defender|attacker|killer|resource)\.(hp|maxHp|mp|maxMp)\b" src\battle\BattleDamageSystem.cpp src\battle\BattleCore.cpp tests
```

Expected: no `BattleDamageUnitState` or `BattleResourceUnitState` scalar vitals remain. Matches on unrelated DTOs must be inspected.

---

## Task 3: Use Shared Value Objects in Hit Unit Snapshot

**Files:**
- Modify: `src/battle/BattleHitResolver.h`
- Modify: `src/battle/BattleHitResolver.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Test: hit resolver tests or add focused tests in `tests/BattleCoreUnitTests.cpp`.

- [ ] **Step 1: Write failing hit snapshot test**

Add or update a hit resolver test so the snapshot is initialized through grouped fields:

```cpp
TEST_CASE("BattleHitResolver_UsesGroupedUnitSnapshotFields", "[battle][hit]")
{
    BattleHitUnitSnapshot attacker;
    attacker.id = 1;
    attacker.team = 0;
    attacker.alive = true;
    attacker.vitals = { 80, 100, 10, 20 };
    attacker.stats = { 30, 12, 9 };
    attacker.motion.position = { 1, 2, 0 };
    attacker.motion.facing = { 1, 0, 0 };
    attacker.animation = { 0, 5, 2, 1 };

    CHECK(attacker.vitals.hp == 80);
    CHECK(attacker.stats.attack == 30);
    CHECK(attacker.motion.position.x == 1);
    CHECK(attacker.animation.actType == 1);
}
```

Expected red: grouped fields do not exist.

- [ ] **Step 2: Replace duplicated hit fields**

In `BattleHitResolver.h`, replace:

```cpp
int hp;
int maxHp;
int mp;
int maxMp;
int attack;
double defence;
int speed;
int cooldown;
int cooldownMax;
int actType;
Pointf position;
Pointf facing;
```

with:

```cpp
BattleUnitVitals vitals;
BattleUnitStats stats;
BattleUnitMotion motion;
BattleUnitAnimationState animation;
```

If `double defence` precision is required by current formulas, inspect usage first. If all sources are integer runtime defence, use `stats.defence`. Do not keep both `defence` and `stats.defence` unless a test proves fractional defence is required.

- [ ] **Step 3: Migrate hit resolver implementation**

Replace hit snapshot reads:

```cpp
unit.hp -> unit.vitals.hp
unit.maxHp -> unit.vitals.maxHp
unit.mp -> unit.vitals.mp
unit.maxMp -> unit.vitals.maxMp
unit.attack -> unit.stats.attack
unit.defence -> unit.stats.defence
unit.speed -> unit.stats.speed
unit.cooldown -> unit.animation.cooldown
unit.cooldownMax -> unit.animation.cooldownMax
unit.actType -> unit.animation.actType
unit.position -> unit.motion.position
unit.facing -> unit.motion.facing
```

Update `makeHitUnitSnapshot(const BattleRuntimeUnit&)` in `BattleCore.cpp` to assign:

```cpp
snapshot.vitals = unit.vitals;
snapshot.stats = unit.stats;
snapshot.motion = unit.motion;
snapshot.animation = unit.animation;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleHitResolver_UsesGroupedUnitSnapshotFields"
```

- [ ] **Step 5: Search hit snapshot scalar fields**

Run:

```powershell
rg -n "\b(attacker|defender|unit)\.(hp|maxHp|mp|maxMp|attack|defence|speed|cooldown|cooldownMax|actType|position|facing)\b" src\battle\BattleHitResolver.cpp src\battle\BattleCore.cpp tests
```

Expected: no `BattleHitUnitSnapshot` scalar cluster access remains.

---

## Task 4: Collapse Initialization Role Delta Stats

**Files:**
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Write failing clone delta grouped test**

Update the clone/delta tests to assert grouped values:

```cpp
CHECK(result.roleDeltas[0].vitals.maxHp == 125);
CHECK(result.roleDeltas[0].vitals.hp == 125);
CHECK(result.roleDeltas[0].stats.attack == expectedAttack);
CHECK(result.roleDeltas[0].stats.defence == expectedDefence);
CHECK(result.roleDeltas[0].stats.speed == expectedSpeed);
```

For clone intent:

```cpp
CHECK(cloneIntent->roleValues.vitals.hp == sourceHp);
CHECK(cloneIntent->roleValues.stats.attack == sourceAttack);
```

Expected red: `vitals` and `stats` do not exist on `BattleInitializationRoleDelta`.

- [ ] **Step 2: Replace scalar delta fields**

In `BattleInitializationRoleDelta`, replace:

```cpp
int maxHp{};
int hp{};
int attack{};
int defence{};
int speed{};
```

with:

```cpp
BattleUnitVitals vitals;
BattleUnitStats stats;
```

Keep:

```cpp
int unitId{};
int star{};
int fist{};
int sword{};
int knife{};
int unusual{};
int hiddenWeapon{};
```

- [ ] **Step 3: Populate grouped deltas**

In `BattleInitialization.cpp`, update role-delta construction:

```cpp
delta.vitals = unit.vitals;
delta.stats = unit.stats;
```

When only HP/stat fields are intended, do not manually assign individual fields unless the target type is not grouped.

- [ ] **Step 4: Update scene adapter delta application**

In `BattleSceneBattleAdapter.cpp`, replace delta reads:

```cpp
delta.maxHp -> delta.vitals.maxHp
delta.hp -> delta.vitals.hp
delta.attack -> delta.stats.attack
delta.defence -> delta.stats.defence
delta.speed -> delta.stats.speed
```

- [ ] **Step 5: Run focused initialization tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleRuntimeSession_ConsumesSetupAndInitializesOwnedRuntime"
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror"
```

- [ ] **Step 6: Search old delta fields**

Run:

```powershell
rg -n "\.(maxHp|hp|attack|defence|speed)\b" src\battle\BattleInitialization.* src\BattleSceneBattleAdapter.cpp tests\BattleInitializationUnitTests.cpp
```

Expected: no `BattleInitializationRoleDelta` scalar stat/vital access remains. Matches on runtime/setup/scene units must be inspected.

---

## Task 5: Decide Presentation Snapshot Scope

**Files:**
- Inspect: `src/battle/BattlePresentation.h`
- Inspect: `src/battle/BattleCore.cpp`
- Inspect: `src/BattleScenePresentationPlayer.cpp`
- Optional modify: same files
- Optional test: presentation tests

- [ ] **Step 1: Verify actual presentation snapshot usage**

Run:

```powershell
rg -n "BattlePresentationUnitSnapshot|\\.snapshot\\.units|snapshot\\.units|\\.cooldown|\\.position|\\.velocity|\\.hp|\\.maxHp|\\.mp|\\.maxMp" src tests
```

Inspect whether `BattlePresentationUnitSnapshot` is consumed as a broad state snapshot or only as event metadata.

- [ ] **Step 2: If presentation uses full vitals, collapse vitals only**

If consumers use `hp/maxHp/mp/maxMp` together, replace them with:

```cpp
BattleUnitVitals vitals;
```

Do not add `BattleUnitMotion` unless acceleration/facing are needed. Current presentation snapshot only has `position` and `velocity`, which is not the same as full motion.

- [ ] **Step 3: If position/velocity repeats enough, introduce a smaller kinematics type**

Only if at least three independent DTOs carry exactly `position + velocity` and no acceleration/facing, add:

```cpp
struct BattleUnitKinematics
{
    Pointf position;
    Pointf velocity;
};
```

Do not use `BattleUnitMotion` for this narrower concept.

- [ ] **Step 4: If usage is weak, leave presentation flat**

If presentation snapshot is mostly temporary recorder state and grouping does not delete code, leave it unchanged and document the decision in the final implementation summary.

---

## Task 6: Final Duplication Search and Verification

**Files:**
- All modified files.

- [ ] **Step 1: Run cluster searches**

Run:

```powershell
rg -n "poisonTimer|poisonTickPct|poisonSourceId|bleedStacks|bleedTimer|bleedSourceId|frozenTimer|frozenMaxTimer|freezeReductionPct|shieldFreezeResPct|controlImmunityFrames|mpBlockTimer|damageImmunityAfterFrames|damageImmunityDuration|damageImmunityTimer|tempAttackBuffs|damageReduceDebuffs" src\battle tests
rg -n "BattleDamageUnitState|BattleResourceUnitState|BattleHitUnitSnapshot|BattleInitializationRoleDelta" src\battle src tests
rg -n "\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|facing|cooldown|cooldownMax|actType)\s*=\s*.*\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|facing|cooldown|cooldownMax|actType)" src\battle src tests
```

Expected:

- Status effect references go through `.effects`.
- Damage/resource vitals go through `.vitals`.
- Hit snapshot shared fields go through grouped value objects.
- Initialization role deltas use `.vitals` and `.stats`.
- Remaining scalar assignment matches are narrow DTO outputs, not broad struct mirrors.

- [ ] **Step 2: Run diff check**

Run:

```powershell
git diff --check
```

Expected: exit code 0.

- [ ] **Step 3: Build tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: exit code 0.

- [ ] **Step 4: Run all tests**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 5: Build game target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: exit code 0. Existing warnings are acceptable. Link failure is acceptable only if the game executable is locked by a running process.

---

## Recommended Execution Order

1. Status effect cluster.
2. Damage/resource vitals.
3. Hit snapshot shared values.
4. Initialization role delta grouped values.
5. Presentation snapshot decision.
6. Final searches and verification.

This order keeps the cleanest duplicate block first, then moves through increasingly boundary-facing DTOs. Each task should leave the project compiling before moving to the next.

---

## Self-Review

- **Spec coverage:** Covers all reviewed collapse targets: status timers, damage/resource vitals, hit snapshots, initialization deltas, and presentation scope.
- **No inheritance:** Explicitly prohibited.
- **No fake broad structs:** The plan avoids collapsing `id/team/alive`, avoids full stats in damage state, and avoids full motion where only position/velocity exist.
- **Deletion gates:** Each task has a targeted search to catch old scalar access.
- **Verification:** Includes diff check, tests build, all tests, and game build.
