# Battle Unit Value Object Scalar Deletion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `BattleUnitVitals`, `BattleUnitStats`, `BattleUnitMotion`, and `BattleUnitAnimationState` the single storage for duplicated unit fields, then delete the redundant scalar mirrors and per-field copy/sync assignments.

**Architecture:** Use composition, not inheritance. Runtime, scene, and setup unit records should carry shared value objects directly; systems read/write those grouped fields in place. The temporary sync helpers exist only during migration and must be deleted once scalar mirrors are gone.

**Tech Stack:** C++20, MSVC, Catch2, existing `build-command.ps1` verification.

---

## Current State

The previous slice added shared value objects, but runtime and scene units still store duplicated scalar fields:

- `BattleRuntimeUnit` has `vitals/stats/motion/animation` plus scalar mirrors like `hp`, `maxHp`, `attack`, `position`, `cooldown`, `actFrame`.
- `BattleSceneUnit` has the same grouped fields plus scalar mirrors.
- `syncBattleRuntimeUnitSharedValueObjects()` and `syncBattleSceneUnitSharedValueObjects()` keep mirrors aligned.
- `BattleSetupUnitInput` still exposes setup-time scalar fields, so adapter setup still contains broad `.abc = .abc` mapping.

The final target is:

- Setup input stores grouped values for stats/vitals/motion/animation.
- Runtime unit stores grouped values only.
- Scene unit stores grouped values only.
- No sync helpers.
- No assignments whose only purpose is copying grouped values back into scalar mirrors.

---

## File Map

- `src/battle/BattleCore.h`
  - Owns shared value object definitions.
  - Will remove duplicate scalar fields from `BattleRuntimeUnit`.
  - Will remove `syncBattleRuntimeUnitSharedValueObjects()` declaration.

- `src/battle/BattleCore.cpp`
  - Main runtime migration surface.
  - Will update frame advancement, planning, damage, rescue, movement, and publishing code to read/write grouped fields.
  - Will delete `syncBattleRuntimeUnitSharedValueObjects()` implementation and calls.

- `src/battle/BattleInitialization.cpp`
  - Will write initialization buffs into `runtimeUnit.vitals` and `runtimeUnit.stats` directly.

- `src/BattleSceneBattleAdapter.h`
  - Will migrate `BattleSetupUnitInput` to grouped value fields.

- `src/BattleSceneBattleAdapter.cpp`
  - Will remove setup/runtime/scene scalar mirror assignments.
  - Will keep conversion from legacy `Role` into grouped setup facts, because `Role` is still an initialization source.

- `src/BattleSceneSetupBuilder.h`
  - Will update setup request/result construction to grouped value objects if this file still owns request building.

- `src/BattleSceneSetupBuilder.cpp`
  - Will populate `BattleSetupUnitInput` groups directly from legacy role facts.

- `src/BattleSceneUnitStore.h`
  - Will remove duplicate scalar fields from `BattleSceneUnit`.
  - Will remove `syncBattleSceneUnitSharedValueObjects()` declaration.

- `src/BattleSceneUnitStore.cpp`
  - Will update store helpers and post-battle summary to use grouped fields.
  - Will delete scene sync implementation and calls.

- `src/BattleSceneHades.cpp`
  - Will migrate render and presentation application reads/writes to grouped fields.
  - This is the largest scene-side edit but should remain mechanical.

- Tests:
  - `tests/BattleInitializationUnitTests.cpp`
  - `tests/BattleSceneUnitStoreUnitTests.cpp`
  - `tests/BattleSceneSetupBuilderUnitTests.cpp`
  - Existing runtime tests in `tests/BattleCoreUnitTests.cpp`

---

## Task 1: Make Setup Input Carry Grouped Unit Values

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Test: `tests/BattleSceneSetupBuilderUnitTests.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Write failing setup-builder test**

Add or update a test that proves setup output carries grouped fields as the authoritative setup shape:

```cpp
TEST_CASE("BattleSceneSetupBuilder_BuildsGroupedUnitValues", "[battle][scene_setup]")
{
    BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.team = 0;
    request.gridX = 2;
    request.gridY = 3;
    request.source = makeRoleForSetup(1001, "測試角色", 120, 35, 20, 8);
    request.positionForCell = [](int x, int y) { return Pointf{ static_cast<float>(x), static_cast<float>(y), 0 }; };
    request.fightFramesForHeadId = [](int) { return std::array<int, 5>{}; };
    request.magicById = [](int) -> Magic* { return nullptr; };

    auto result = BattleSceneSetupBuilder::buildSetupUnits({ request });

    REQUIRE(result.units.size() == 1);
    const auto& unit = result.units[0];
    CHECK(unit.vitals.maxHp == 120);
    CHECK(unit.vitals.hp == 120);
    CHECK(unit.stats.attack == 35);
    CHECK(unit.stats.defence == 20);
    CHECK(unit.stats.speed == 8);
    CHECK(unit.motion.position.x == 2);
    CHECK(unit.motion.position.y == 3);
    CHECK(unit.animation.actType == -1);
}
```

Use the existing local role factory/helpers in `BattleSceneSetupBuilderUnitTests.cpp`; do not introduce a mock system.

- [ ] **Step 2: Verify red**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile failure or test failure because `BattleSetupUnitInput` does not yet expose grouped value fields.

- [ ] **Step 3: Add grouped fields to setup input**

In `BattleSetupUnitInput`, add:

```cpp
KysChess::Battle::BattleUnitVitals vitals;
KysChess::Battle::BattleUnitStats stats;
KysChess::Battle::BattleUnitMotion motion;
KysChess::Battle::BattleUnitAnimationState animation;
```

Populate these in `BattleSceneSetupBuilder.cpp` at the point where setup units are built from `Role`.

- [ ] **Step 4: Migrate adapter setup reads to groups**

Replace helper bodies in `BattleSceneBattleAdapter.cpp`:

```cpp
Battle::BattleUnitVitals makeBattleUnitVitals(const BattleSetupUnitInput& source)
{
    return source.vitals;
}

Battle::BattleUnitStats makeBattleUnitStats(const BattleSetupUnitInput& source)
{
    return source.stats;
}

Battle::BattleUnitMotion makeBattleUnitMotion(const BattleSetupUnitInput& source)
{
    return source.motion;
}

Battle::BattleUnitAnimationState makeBattleUnitAnimationState(const BattleSetupUnitInput& source)
{
    return source.animation;
}
```

Then update `makeBattleRuntimeSetupSeed()` and clone source power calculation to read:

```cpp
unit.vitals.maxHp
unit.stats.attack
unit.stats.defence
unit.stats.speed
```

- [ ] **Step 5: Keep setup scalars temporarily**

Do not delete `BattleSetupUnitInput` scalar fields yet. This task only makes grouped setup data available and starts using it where safe.

- [ ] **Step 6: Run focused tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneSetupBuilder_BuildsGroupedUnitValues"
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror"
```

Expected: all pass.

---

## Task 2: Make Runtime Unit Groups Primary

**Files:**
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Test: `tests/BattleInitializationUnitTests.cpp`
- Test: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Write failing runtime mutation test**

Add a runtime test proving mutation happens directly on grouped state. Use an existing frame/session test if possible:

```cpp
TEST_CASE("BattleRuntimeSession_FrameAdvanceMutatesGroupedUnitState", "[battle][runtime]")
{
    BattleRuntimeInit init = makeMinimalRuntimeInitWithUnit(0);
    init.runtime.units.units[0].vitals = { 10, 20, 0, 10 };
    init.runtime.units.units[0].animation = { 0, 5, 0, -1 };

    BattleRuntimeSession session(std::move(init));
    auto result = session.runFrame(makeAdvanceInputForUnitTick());

    const auto& unit = session.runtime().units.requireUnit(0);
    CHECK(unit.vitals.mp >= 0);
    CHECK(unit.animation.actFrame >= 0);
    CHECK(result.unitApplications[0].finalMp == unit.vitals.mp);
}
```

Use existing test helper names in `BattleCoreUnitTests.cpp`; if no generic helper exists, adapt the nearest current runtime session frame test instead of adding broad fixture machinery.

- [ ] **Step 2: Verify red**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: the new assertion fails or requires grouped writes that do not exist yet.

- [ ] **Step 3: Migrate runtime read/write sites mechanically**

In `BattleCore.cpp`, replace runtime unit scalar access with grouped access:

```cpp
unit.hp          -> unit.vitals.hp
unit.maxHp       -> unit.vitals.maxHp
unit.mp          -> unit.vitals.mp
unit.maxMp       -> unit.vitals.maxMp
unit.attack      -> unit.stats.attack
unit.defence     -> unit.stats.defence
unit.speed       -> unit.stats.speed
unit.position    -> unit.motion.position
unit.velocity    -> unit.motion.velocity
unit.acceleration-> unit.motion.acceleration
unit.facing      -> unit.motion.facing
unit.cooldown    -> unit.animation.cooldown
unit.cooldownMax -> unit.animation.cooldownMax
unit.actFrame    -> unit.animation.actFrame
unit.actType     -> unit.animation.actType
```

Important runtime hotspots to migrate explicitly:

- `BattleUnitStore::writeDamageUnit`
- `BattleUnitStore::setPosition`
- `BattleUnitStore::setMotion`
- `findNearestEnemyUnitId`
- `findFarthestEnemyUnitId`
- runtime snapshot builders near the top of `BattleCore.cpp`
- action tick commit around `advanceFrameUnitApplications`
- action/cast input builders
- status/damage/rescue projection helpers
- movement presentation publishing
- end-of-frame state application publishing

- [ ] **Step 4: Migrate initialization buffs to groups**

In `BattleInitialization.cpp`, update star/combo/enemy-top/clone writes:

```cpp
unit.vitals.maxHp = starBoostedStats.hp + combo.flatHP;
unit.stats.attack = starBoostedStats.atk + combo.flatATK;
unit.stats.defence = starBoostedStats.def + combo.flatDEF;
unit.stats.speed = starBoostedStats.spd + combo.flatSPD;
unit.vitals.hp = unit.vitals.maxHp;
```

Role deltas should read from grouped values:

```cpp
unit.vitals.maxHp
unit.vitals.hp
unit.stats.attack
unit.stats.defence
unit.stats.speed
```

- [ ] **Step 5: Keep runtime scalar fields temporarily**

Do not delete scalar fields in this task. The build should pass while groups and scalars coexist, but runtime code should now use groups.

- [ ] **Step 6: Run runtime scalar usage search**

Run:

```powershell
rg -n "\bunit\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|cooldown|cooldownMax|actFrame|actType)\b" src\battle\BattleCore.cpp src\battle\BattleInitialization.cpp
```

Expected: only lines inside the temporary sync helper, comments, or unavoidable non-`BattleRuntimeUnit` local DTO variables. For ambiguous matches, inspect and migrate instead of assuming.

- [ ] **Step 7: Run focused runtime tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleRuntimeSession_ConsumesSetupAndInitializesOwnedRuntime"
x64\Debug\kys_tests.exe "BattleRuntimeUnit_UsesSharedUnitValueObjects"
```

Expected: all pass.

---

## Task 3: Make Scene Unit Groups Primary

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneUnitStore.cpp`
- Test: `tests/BattleSceneUnitStoreUnitTests.cpp`

- [ ] **Step 1: Write failing scene-store test**

Change `BattleSceneUnitStore_UsesSharedUnitValueObjects` so it writes grouped fields directly and proves store operations preserve them:

```cpp
TEST_CASE("BattleSceneUnitStore_UsesGroupedFieldsAsPrimaryState", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    auto unit = makeUnit(0, 0, 1, 2, { 10, 20, 0 });
    unit.vitals = { 75, 100, 4, 10 };
    unit.stats = { 30, 40, 50 };
    unit.motion = { { 10, 20, 0 }, { 1, 2, 3 }, { 0, 0, 9 }, { 1, 0, 0 } };
    unit.animation = { 3, 5, 7, 9 };
    units.push_back(unit);

    BattleSceneUnitStore store;
    store.initialize(std::move(units));

    const auto& stored = store.requireUnit(0);
    CHECK(stored.vitals.hp == 75);
    CHECK(stored.stats.attack == 30);
    CHECK(stored.motion.position.x == 10);
    CHECK(stored.animation.actFrame == 7);
}
```

- [ ] **Step 2: Verify red**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: test still depends on scalar sync or fails once sync is disabled locally.

- [ ] **Step 3: Migrate scene render reads**

In `BattleSceneHades.cpp`, update draw/status code:

```cpp
unit.position      -> unit.motion.position
unit.realTowards   -> unit.motion.facing
unit.actType       -> unit.animation.actType
unit.actFrame      -> unit.animation.actFrame
unit.cooldown      -> unit.animation.cooldown
unit.cooldownMax   -> unit.animation.cooldownMax
unit.hp            -> unit.vitals.hp
unit.maxHp         -> unit.vitals.maxHp
unit.mp            -> unit.vitals.mp
unit.maxMp         -> unit.vitals.maxMp
```

Use this for:

- map visibility checks
- sprite render position
- `calRenderUnitPic`
- extra role info bars/text
- battle effect positioning
- camera focus calculations

- [ ] **Step 4: Migrate scene frame application writes**

In `BattleSceneHades.cpp`, update presentation application to write grouped fields directly:

```cpp
unit.animation.cooldown = application.cooldown;
unit.animation.actFrame = application.actFrame;
unit.animation.actType = application.actType;
unit.vitals.mp = application.finalMp;
unit.motion.velocity = { 0, 0, 0 };
```

Apply the same pattern to:

- movement presentation results
- action results
- blink teleports
- damage render applications
- rescue teleport/heal
- team effect events
- knockbacks
- MP restores
- unit heals
- clamp pass in `applyLegacyBattleFrameResult`

- [ ] **Step 5: Migrate scene store helpers**

In `BattleSceneUnitStore.cpp`, use grouped state:

```cpp
auto delta = candidate.motion.position - source.motion.position;
return source.motion.facing;
unit.hp = source.vitals.maxHp;
unit.maxHp = source.vitals.maxHp;
unit.attack = source.stats.attack;
unit.defence = source.stats.defence;
unit.speed = source.stats.speed;
unit.hpRemaining = source.vitals.hp;
```

Here `unit` is `BattlePostBattleUnitSummary`, so scalar writes into the summary remain correct. Only `source` reads should use grouped state.

- [ ] **Step 6: Run scene scalar usage search**

Run:

```powershell
rg -n "\bunit\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|realTowards|cooldown|cooldownMax|actType|actFrame)\b" src\BattleSceneHades.cpp src\BattleSceneUnitStore.cpp
```

Expected: no `BattleSceneUnit` reads/writes remain outside the temporary sync helper. Scalar fields on summary DTOs may remain, but source reads must use grouped fields.

- [ ] **Step 7: Run focused scene tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneUnitStore_UsesGroupedFieldsAsPrimaryState"
x64\Debug\kys_tests.exe "BattleSceneUnitStore_SwapsSetupPositionAndUpdatesWorldPosition"
```

Expected: all pass.

---

## Task 4: Delete Runtime Scalar Mirrors

**Files:**
- Modify: `src/battle/BattleCore.h`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Test: runtime and initialization tests

- [ ] **Step 1: Delete runtime scalar fields**

From `BattleRuntimeUnit`, remove:

```cpp
int hp = 0;
int maxHp = 0;
int mp = 0;
int maxMp = 0;
int attack = 0;
int defence = 0;
int speed = 0;
int cooldown = 0;
int cooldownMax = 0;
int actFrame = 0;
int actType = -1;
Pointf position;
Pointf velocity;
Pointf acceleration;
Pointf facing;
```

Keep fields that are not represented by the grouped value objects:

```cpp
bool haveAction;
BattleOperationType operationType;
int operationCount;
int physicalPower;
int invincible;
int hurtFrame;
int shield;
bool mpBlocked;
int mpRecoveryBonusPct;
std::map<int, int> actPropertiesByMagicType;
Point grid;
bool canAttack;
double reach;
CombatStyle style;
int star;
int cost;
```

- [ ] **Step 2: Delete runtime sync helper**

Remove:

```cpp
void syncBattleRuntimeUnitSharedValueObjects(BattleRuntimeUnit& unit);
```

and its implementation/calls.

- [ ] **Step 3: Compile and fix only real misses**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: any failures should point to remaining scalar `BattleRuntimeUnit` access. Fix by using grouped fields directly, not by reintroducing bridge accessors or sync helpers.

- [ ] **Step 4: Runtime deletion search**

Run:

```powershell
rg -n "syncBattleRuntimeUnitSharedValueObjects|\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|cooldown|cooldownMax|actFrame|actType)\b" src\battle\BattleCore.h src\battle\BattleCore.cpp src\battle\BattleInitialization.cpp
```

Expected: no sync helper, no deleted `BattleRuntimeUnit` scalar fields. Matches for unrelated DTOs are allowed only after inspecting each one.

---

## Task 5: Delete Scene Scalar Mirrors

**Files:**
- Modify: `src/BattleSceneUnitStore.h`
- Modify: `src/BattleSceneUnitStore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Test: scene unit and adapter tests

- [ ] **Step 1: Delete scene scalar fields**

From `BattleSceneUnit`, remove:

```cpp
Pointf position;
Pointf velocity;
Pointf acceleration;
Pointf realTowards;
int actType = -1;
int actFrame = 0;
int hp = 0;
int maxHp = 0;
int mp = 0;
int maxMp = 0;
int attack = 0;
int defence = 0;
int speed = 0;
int cooldown = 0;
int cooldownMax = 0;
```

Keep fields that are not represented by the grouped value objects:

```cpp
std::array<int, 5> fightFrames;
int frozen;
int frozenMax;
int invincible;
int shake;
int attention;
BattleSceneRenderComboState combo;
```

- [ ] **Step 2: Delete scene sync helper**

Remove:

```cpp
void syncBattleSceneUnitSharedValueObjects(BattleSceneUnit& unit);
```

and implementation/calls.

- [ ] **Step 3: Compile and fix only real misses**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: failures show remaining scene scalar use. Fix by using grouped fields directly.

- [ ] **Step 4: Scene deletion search**

Run:

```powershell
rg -n "syncBattleSceneUnitSharedValueObjects|\bunit\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|realTowards|cooldown|cooldownMax|actType|actFrame)\b" src\BattleSceneHades.cpp src\BattleSceneUnitStore.cpp src\BattleSceneBattleAdapter.cpp src\BattleSceneUnitStore.h
```

Expected: no deleted `BattleSceneUnit` scalar access. Matches for setup DTOs or summary DTOs must be inspected and justified.

---

## Task 6: Delete Setup Scalar Mirrors

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: tests that still initialize setup scalars

- [ ] **Step 1: Delete setup scalar fields**

From `BattleSetupUnitInput`, remove fields now represented by groups:

```cpp
int hp;
int maxHp;
int mp;
int maxMp;
int attack;
int defence;
int speed;
int cooldown;
int cooldownMax;
int actFrame;
int actType;
Pointf position;
Pointf velocity;
Pointf acceleration;
Pointf facing;
```

- [ ] **Step 2: Update test setup literals**

Replace setup test construction:

```cpp
source.hp = 100;
source.maxHp = 100;
source.attack = 20;
source.defence = 30;
source.speed = 40;
source.position = { 10, 20, 0 };
source.velocity = { 0, 0, 0 };
source.acceleration = { 0, 0, 9 };
source.facing = { 1, 1, 0 };
```

with:

```cpp
source.vitals = { 100, 100, 0, 0 };
source.stats = { 20, 30, 40 };
source.motion = { { 10, 20, 0 }, { 0, 0, 0 }, { 0, 0, 9 }, { 1, 1, 0 } };
source.animation = { 0, 0, 0, -1 };
```

- [ ] **Step 3: Compile and fix adapter misses**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: failures identify remaining `BattleSetupUnitInput` scalar access. Fix by reading grouped values.

- [ ] **Step 4: Setup deletion search**

Run:

```powershell
rg -n "\b(source|snapshot|unit)\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|cooldown|cooldownMax|actFrame|actType)\b" src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneSetupBuilder.cpp tests\BattleInitializationUnitTests.cpp tests\BattleSceneSetupBuilderUnitTests.cpp
```

Expected: no setup-unit scalar access. Matches against legacy `Role` are allowed because `Role` remains the initialization source.

---

## Task 7: Collapse Per-Field Initialization Assignments

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/BattleSceneUnitStore.cpp`

- [ ] **Step 1: Replace grouped mirror builders with direct assignment**

Delete `makeBattleUnitVitals`, `makeBattleUnitStats`, `makeBattleUnitMotion`, and `makeBattleUnitAnimationState` if they only return existing grouped objects.

Use direct assignments:

```cpp
unit.vitals = runtimeUnit.vitals;
unit.stats = runtimeUnit.stats;
unit.motion = runtimeUnit.motion;
unit.animation = runtimeUnit.animation;
```

and:

```cpp
unit.vitals = snapshot_.vitals;
unit.stats = snapshot_.stats;
unit.motion = snapshot_.motion;
unit.animation = snapshot_.animation;
```

- [ ] **Step 2: Replace `.abc = .abc` groups with aggregate construction**

Where a function builds a whole unit, prefer:

```cpp
Battle::BattleRuntimeUnit unit;
unit.id = snapshot_.unitId;
unit.realRoleId = snapshot_.realRoleId;
unit.name = snapshot_.name;
unit.team = snapshot_.team;
unit.alive = snapshot_.alive;
unit.vitals = snapshot_.vitals;
unit.stats = snapshot_.stats;
unit.motion = snapshot_.motion;
unit.animation = snapshot_.animation;
```

Do not add inheritance. Do not add broad base classes. Keep explicit identity/setup fields separate from value-object groups.

- [ ] **Step 3: Search for redundant grouped copy boilerplate**

Run:

```powershell
rg -n "\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|realTowards|cooldown|cooldownMax|actFrame|actType)\s*=\s*.*\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|realTowards|cooldown|cooldownMax|actFrame|actType)" src tests
```

Expected: no broad unit-to-unit mirror assignment remains. Field assignments into narrow DTO outputs are allowed when the DTO really has separate fields, such as damage presentation or post-battle summaries.

- [ ] **Step 4: Run focused adapter tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror"
x64\Debug\kys_tests.exe "BattleSceneSetupBuilder_BuildsGroupedUnitValues"
```

Expected: all pass.

---

## Task 8: Final Boundary and Build Verification

**Files:**
- All touched files

- [ ] **Step 1: Run deletion searches**

Run:

```powershell
rg -n "syncBattle(Runtime|Scene)UnitSharedValueObjects" src tests
rg -n "struct BattleRuntimeUnit|struct BattleSceneUnit|struct BattleSetupUnitInput" src\battle\BattleCore.h src\BattleSceneUnitStore.h src\BattleSceneBattleAdapter.h
rg -n "\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|realTowards|cooldown|cooldownMax|actFrame|actType)\s*=\s*.*\.(hp|maxHp|mp|maxMp|attack|defence|speed|position|velocity|acceleration|facing|realTowards|cooldown|cooldownMax|actFrame|actType)" src tests
```

Expected:

- no sync helper references
- unit structs contain grouped fields, not redundant mirrors
- no broad unit-to-unit scalar mirror assignments

- [ ] **Step 2: Run formatting check**

Run:

```powershell
git diff --check
```

Expected: no output and exit code 0.

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

Expected: exit code 0. Existing warnings are acceptable; link failure is acceptable only if the executable is locked by a running game process.

---

## Non-Goals

- Do not introduce inheritance between scene, runtime, and setup units.
- Do not add compatibility accessors like `hp()` just to hide old scalar names.
- Do not keep sync helpers after deleting scalar mirrors.
- Do not migrate legacy `Role` itself in this plan; `Role` remains an initialization source.
- Do not broaden render DTO abstractions in this plan.

---

## Self-Review

- **Spec coverage:** The plan starts from the current grouped-value slice and ends with deleting runtime, scene, and setup scalar mirrors plus sync helpers.
- **Deletion coverage:** Tasks 4, 5, and 6 are explicit deletion gates. Task 7 removes leftover per-field assignment boilerplate.
- **Boundary coverage:** Runtime, scene, adapter, setup builder, and tests are all included.
- **No placeholders:** Each task has concrete file targets, code patterns, commands, and expected outcomes.
- **Risk:** The biggest risk is ambiguous search matches against narrow DTOs. The plan requires inspection instead of suppressing those with helper bridges.
