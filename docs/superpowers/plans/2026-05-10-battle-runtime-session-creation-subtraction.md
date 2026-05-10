# Battle Runtime Session Creation Subtraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Delete the fragile post-construction runtime configuration loop without adding another broad per-unit copy struct.

**Architecture:** Keep `BattleSetupUnitInput` as the single per-unit setup row for this batch. `BattleRuntimeSession` owns construction of canonical runtime units and all derived runtime stores from the existing build context. The adapter may still translate legacy `Role`/`Magic` facts into setup rows and scene initialization output, but it must not read a partially initialized session runtime to build live runtime stores and write them back.

**Tech Stack:** C++20, MSBuild solution `kys.sln`, Catch2 unit tests, existing `BattleRuntimeSession`, `BattleSceneBattleAdapter`, and battle runtime modules.

---

## Current Problem

The current creation flow is split-brained:

```text
BattleSceneBattleAdapter builds BattleRuntimeInit
  -> BattleRuntimeSession constructor runs BattleInitializationSystem
  -> adapter reads session.runtime()
  -> adapter builds BattleRuntimeSetupConfiguration
  -> session.applySetupConfiguration writes live stores back
```

`BattleRuntimeSetupConfiguration` is not configuration. It contains live runtime stores:

- `world`
- `attacks`
- `damage`
- `result`
- `teamEffects`
- `rescue`
- `movementPhysics`
- `action`
- `projectileFollowUps`
- `deathEffects`

That is why refactors lose behavior. Rendering can still work from scene units while simulation silently loses one required runtime store such as `attacks.units`.

---

## Target Shape

Use one construction path:

```text
BattleRuntimeBuildContext
  -> BattleRuntimeSession::createInitialized(context)
  -> session builds canonical runtime units
  -> BattleInitializationSystem mutates canonical runtime and returns initialization result
  -> session derives world/attack/damage/action/movement/rescue/death stores internally
  -> adapter builds only scene initialization output
```

No new `BattleRuntimeUnitSetup`.
No `BattleRuntimeSetupConfiguration`.
No callback that receives `session.runtime()` during creation.

---

## File Map

- `src/battle/BattleRuntimeSession.h/.cpp`: add `createInitialized(...)`; delete `BattleRuntimeSetupConfiguration`, `createConfigured(...)`, and `applySetupConfiguration(...)`; keep construction helpers private here first.
- `src/BattleSceneBattleAdapter.h/.cpp`: stop constructing runtime subsystem stores; keep legacy-to-setup input and scene initialization output only.
- `src/BattleSceneSetupBuilder.h/.cpp`: no structural change required unless a test needs setup rows.
- `tests/BattleInitializationUnitTests.cpp`: keep/add real initialized-session behavior tests.
- `tests/BattleFrameRunnerRuntimeUnitTests.cpp`: update any direct `createConfigured(...)` tests.

---

## Non-Goals

- Do not add a new per-unit runtime setup DTO.
- Do not create `BattleRuntimeBootstrap.*` in this batch.
- Do not remove `BattleSceneBattleAdapter` completely.
- Do not change combat behavior.
- Do not move combo/equipment/neigong definition extraction unless needed to delete the config loop.
- Do not preserve backward-compatible creation APIs.

---

### Task 1: Add Tests That Catch Missing Runtime Stores

**Files:**
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Keep or add initialized-session movement test**

Add or keep this test shape:

```cpp
TEST_CASE("BattleSceneBattleAdapter_InitializedSessionAdvancesUnitsAfterSetupPlacement", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;
    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    addInitializedRuntimeTestUnit(context, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(context, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    BattleSetupPlacementInput placement;
    placement.units.push_back({ 0, 3, 3, Towards_RightDown });
    placement.units.push_back({ 1, 10, 10, Towards_LeftUp });
    created.session.commitSetupPlacement(placement);

    const auto initialAlly = created.session.runtime().units.requireUnit(0).motion.position;
    const auto initialEnemy = created.session.runtime().units.requireUnit(1).motion.position;

    bool anyUnitMoved = false;
    for (int frame = 0; frame < 90 && !anyUnitMoved; ++frame)
    {
        created.session.runFrame();
        const auto& ally = created.session.runtime().units.requireUnit(0).motion.position;
        const auto& enemy = created.session.runtime().units.requireUnit(1).motion.position;
        anyUnitMoved = ally.x != initialAlly.x
            || ally.y != initialAlly.y
            || enemy.x != initialEnemy.x
            || enemy.y != initialEnemy.y;
    }

    CHECK(anyUnitMoved);
}
```

- [ ] **Step 2: Keep or add attack store test**

Add or keep:

```cpp
TEST_CASE("BattleSceneBattleAdapter_InitializedSessionSeedsAttackUnits", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;
    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;

    addInitializedRuntimeTestUnit(context, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(context, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    REQUIRE(created.session.runtime().attacks.units.size() == 2);
    CHECK(created.session.runtime().attacks.units[0].id == 0);
    CHECK(created.session.runtime().attacks.units[0].team == 0);
    CHECK(created.session.runtime().attacks.units[0].alive);
    CHECK(created.session.runtime().attacks.units[1].id == 1);
    CHECK(created.session.runtime().attacks.units[1].team == 1);
    CHECK(created.session.runtime().attacks.units[1].alive);
}
```

- [ ] **Step 3: Keep or add projectile combat test**

Add or keep:

```cpp
TEST_CASE("BattleSceneBattleAdapter_InitializedSessionResolvesProjectileCombat", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;
    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    auto projectileSkill = makeHadesTestSkillSeed(1, 4, "飛刀", 55, 44);
    addInitializedRuntimeTestUnit(context, 0, 1001, 0, 3, 3, Towards_RightDown, projectileSkill, projectileSkill);
    addInitializedRuntimeTestUnit(context, 1, 1002, 1, 5, 3, Towards_LeftUp, projectileSkill, projectileSkill);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    BattleSetupPlacementInput placement;
    placement.units.push_back({ 0, 3, 3, Towards_RightDown });
    placement.units.push_back({ 1, 5, 3, Towards_LeftUp });
    created.session.commitSetupPlacement(placement);

    bool playedAttackSound = false;
    bool emittedProjectile = false;
    bool appliedDamage = false;
    for (int frame = 0; frame < 90 && !(playedAttackSound && emittedProjectile && appliedDamage); ++frame)
    {
        const auto frameResult = created.session.runFrame();
        playedAttackSound = playedAttackSound || !frameResult.applications.attackSoundIds.empty();
        emittedProjectile = emittedProjectile
            || std::any_of(
                frameResult.frame.visualEvents.begin(),
                frameResult.frame.visualEvents.end(),
                [](const BattleVisualEvent& event)
                {
                    return event.type == BattleVisualEventType::ProjectileSpawned;
                });
        appliedDamage = appliedDamage || !frameResult.damageRenderApplications.empty();
    }

    CHECK(playedAttackSound);
    CHECK(emittedProjectile);
    CHECK(appliedDamage);
}
```

- [ ] **Step 4: Run tests before deletion**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: these tests pass with the user's current fixes. They become the safety net while deleting the creation loop.

---

### Task 2: Move Runtime Store Construction Into Session Private Helpers

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] **Step 1: Include adapter context only at the session API boundary**

In `BattleRuntimeSession.h`, include the adapter header would create a bad dependency direction. Instead, add a core-facing session creation input that reuses the existing per-unit setup row through a span-free vector reference wrapper is not worth it.

Use this minimal struct in `BattleRuntimeSession.h`:

```cpp
struct BattleRuntimeSessionCreationInput
{
    std::vector<BattleSceneBattleAdapter::BattleSetupUnitInput> units;
    BattleRuntimeSetupSeed setup;
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleTerrainCell> terrainCells;
    std::vector<BattleRescueCellSnapshot> rescueCells;
    std::vector<BattleActionPlanSeed> actionPlanSeeds;
    BattleRuntimeRulesConfig rules;
    unsigned int randomSeed = 1;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};
```

If the include cycle is unacceptable during implementation, put this struct in `BattleSceneBattleAdapter.h` temporarily and pass `BattleRuntimeBuildContext` directly into a session factory declared in the adapter namespace. Do not create a second per-unit DTO.

- [ ] **Step 2: Add private helper to construct canonical runtime units**

Move the logic currently in adapter `makeBattleRuntimeUnit(...)` into `BattleRuntimeSession.cpp` as an anonymous-namespace helper that takes the existing `BattleSetupUnitInput`:

```cpp
BattleRuntimeUnit makeRuntimeUnit(
    const BattleSceneBattleAdapter::BattleSetupUnitInput& setup,
    const RoleComboState* combo,
    const BattleGridTransform& gridTransform)
{
    BattleRuntimeUnit unit;
    unit.id = setup.unitId;
    unit.realRoleId = setup.realRoleId;
    unit.name = setup.name;
    unit.team = setup.team;
    unit.alive = setup.alive;
    unit.vitals = setup.vitals;
    unit.stats = setup.stats;
    unit.motion = setup.motion;
    unit.animation = setup.animation;
    unit.haveAction = setup.haveAction;
    unit.operationType = setup.operationType;
    unit.operationCount = setup.operationCount;
    unit.physicalPower = setup.physicalPower;
    unit.invincible = setup.invincible;
    unit.hurtFrame = setup.hurtFrame;
    for (int magicType = 0; magicType <= 4; ++magicType)
    {
        unit.actPropertiesByMagicType.emplace(magicType, setup.actPropertiesByMagicType[magicType]);
    }
    unit.grid = gridTransform.toGrid(setup.motion.position);
    if (combo)
    {
        unit.shield = combo->shield;
        unit.mpBlocked = combo->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
    }
    return unit;
}
```

- [ ] **Step 3: Add private helper to derive world unit from runtime unit**

Move adapter `makeInitializedBattleWorldUnit(...)` into `BattleRuntimeSession.cpp` and keep it private. It should derive from `BattleRuntimeUnit`, combo state, existing world row if any, and rules.

- [ ] **Step 4: Add private helper to derive attack unit from runtime unit**

Move adapter `makeInitializedBattleAttackUnit(...)` into `BattleRuntimeSession.cpp` and keep it private:

```cpp
BattleAttackUnit makeAttackUnit(const BattleRuntimeUnit& runtimeUnit)
{
    BattleAttackUnit unit;
    unit.id = runtimeUnit.id;
    unit.team = runtimeUnit.team;
    unit.alive = runtimeUnit.alive;
    unit.invincible = runtimeUnit.invincible > 0;
    unit.hurtFrame = runtimeUnit.hurtFrame > 0;
    unit.position = runtimeUnit.motion.position;
    return unit;
}
```

- [ ] **Step 5: Add private helper to configure attack world**

Move `configureBattleAttackWorld(...)` from adapter into `BattleRuntimeSession.cpp` private namespace.

- [ ] **Step 6: Add private helper to build death effect store**

Move `makeBattleDeathEffectStore(...)` from adapter into `BattleRuntimeSession.cpp` private namespace.

---

### Task 3: Add `BattleRuntimeSession::createInitialized(...)`

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`

- [ ] **Step 1: Add session factory**

Add to `BattleRuntimeSession`:

```cpp
static BattleRuntimeSession createInitialized(BattleRuntimeSessionCreationInput input);
```

- [ ] **Step 2: Implement canonical construction first**

Implementation order inside `createInitialized(...)`:

```cpp
BattleRuntimeInit init;
init.runtime.units.gridTransform = input.rules.gridTransform;
init.runtime.random = BattleRuntimeRandom(input.randomSeed);
init.runtime.combo.units = input.comboStates;
init.setup = std::move(input.setup);

for (const auto& setupUnit : input.units)
{
    const auto comboIt = input.comboStates.find(setupUnit.unitId);
    auto runtimeUnit = makeRuntimeUnit(
        setupUnit,
        comboIt != input.comboStates.end() ? &comboIt->second : nullptr,
        input.rules.gridTransform);

    if (comboIt != input.comboStates.end())
    {
        auto statusUnit = makeBattleStatusUnitState(runtimeUnit, comboIt->second);
        statusUnit.effects.frozenTimer = setupUnit.frozen;
        statusUnit.effects.frozenMaxTimer = setupUnit.frozenMax;
        init.runtime.status.units.push_back(makeBattleStatusRuntimeUnit(statusUnit));
    }
    else
    {
        init.runtime.status.units.push_back(BattleStatusRuntimeUnit{ .id = setupUnit.unitId });
    }

    init.runtime.world.units.push_back(makeBattleWorldUnitState(runtimeUnit, 22.0));
    init.runtime.units.units.push_back(std::move(runtimeUnit));
}
```

- [ ] **Step 3: Run initialization**

Construct session or directly run initialization:

```cpp
BattleRuntimeSession session(std::move(init));
```

Keep the existing constructor while this task is in progress.

- [ ] **Step 4: Derive runtime stores inside the session**

After initialization has run, use `session.runtime_` internally to populate:

```cpp
session.runtime_.world.frame = input.battleFrame;
session.runtime_.world.config = input.rules.movementConfig;
session.runtime_.world.terrainCells = std::move(input.terrainCells);
session.runtime_.world.units.clear();

for (const auto& unit : session.runtime_.units.units)
{
    if (!unit.alive)
    {
        continue;
    }
    session.runtime_.world.units.push_back(makeInitializedWorldUnit(...));
}

session.runtime_.attacks.units.clear();
configureAttackWorld(session.runtime_.attacks, input.rules);
for (const auto& unit : session.runtime_.units.units)
{
    if (unit.alive)
    {
        session.runtime_.attacks.units.push_back(makeAttackUnit(unit));
    }
}
```

Also populate damage, team effects, rescue, movement physics, action, projectile follow-ups, and death effects using the same code currently in `makeInitializedBattleRuntimeConfiguration(...)`.

- [ ] **Step 5: Update adapter creation to call the factory**

In `createInitializedBattleRuntimeSession(...)`, replace:

```cpp
Battle::BattleRuntimeInit init;
...
Battle::BattleRuntimeSession::createConfigured(...)
```

with:

```cpp
Battle::BattleRuntimeSessionCreationInput sessionInput;
sessionInput.units = input.units;
sessionInput.setup = input.runtimeSetupSeed;
populateBattleRuntimeSetupDefinitions(sessionInput.setup);
sessionInput.comboStates = *input.comboStates;
sessionInput.terrainCells = input.terrainCells;
sessionInput.rescueCells = input.rescueCells;
sessionInput.actionPlanSeeds = input.actionPlanSeeds;
sessionInput.rules = context.rules;
sessionInput.randomSeed = context.randomSeed;
sessionInput.battleFrame = input.battleFrame;
sessionInput.rescueCounterAttackSkillId = input.rescueCounterAttackSkillId;

BattleRuntimeCreationResult result{
    Battle::BattleRuntimeSession::createInitialized(std::move(sessionInput)),
    {},
};
```

The adapter should still build `sceneInitialization` after this call.

---

### Task 4: Delete the Post-Construction Config API

**Files:**
- Modify: `src/battle/BattleRuntimeSession.h`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleFrameRunnerRuntimeUnitTests.cpp`

- [ ] **Step 1: Delete `BattleRuntimeSetupConfiguration`**

Remove the struct from `BattleRuntimeSession.h`.

- [ ] **Step 2: Delete `createConfigured(...)`**

Remove the templated factory from `BattleRuntimeSession.h`.

- [ ] **Step 3: Delete `applySetupConfiguration(...)`**

Remove declaration and implementation.

- [ ] **Step 4: Delete `setupConfigured_`**

Remove the member because setup configuration no longer exists.

- [ ] **Step 5: Update tests using `createConfigured(...)`**

Search:

```powershell
rg -n "createConfigured|BattleRuntimeSetupConfiguration|applySetupConfiguration" src tests
```

Update any tests to use `BattleRuntimeSession::createInitialized(...)` or direct `BattleRuntimeSession(init)` if they are intentionally testing initialization only.

---

### Task 5: Delete Adapter Runtime Store Builders

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`

- [ ] **Step 1: Delete runtime store construction helpers from adapter**

Delete from adapter:

```cpp
configureBattleAttackWorld
makeBattleRuntimeUnit
makeInitializedBattleWorldUnit
makeInitializedBattleAttackUnit
makeInitializedBattleRuntimeConfiguration
makeBattleDamagePresentationStyle
makeBattleDeathEffectStore
```

If a helper is still needed for scene initialization output, keep only the scene-specific helper.

- [ ] **Step 2: Keep scene initialization result construction**

Keep:

```cpp
makeInitializedSceneUnit(...)
makeBattleInitializationSceneApplyResult(...)
```

These are scene output joins and still belong in the adapter for now.

- [ ] **Step 3: Run reduction search**

Run:

```powershell
rg -n "makeInitializedBattleRuntimeConfiguration|makeInitializedBattleWorldUnit|makeInitializedBattleAttackUnit|configureBattleAttackWorld|makeBattleRuntimeUnit|BattleRuntimeSetupConfiguration|createConfigured|applySetupConfiguration" src tests
```

Expected: no production matches.

---

### Task 6: Add One Runtime Store Consistency Test

**Files:**
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Add frame consistency test**

Add a test that runs one frame after initialized session creation and checks canonical motion is reflected in derived stores:

```cpp
TEST_CASE("BattleFrameRunner_KeepsDerivedMotionStoresAligned", "[battle][core][runtime]")
{
    // Build a two-unit initialized session using the same helper as BattleInitializationUnitTests.
    // Commit setup placement and run one frame.
    // Then compare canonical, world, and attack positions for unit 0.
}
```

If the helper cannot be shared without bad includes, keep this test in `BattleInitializationUnitTests.cpp` beside the session smoke tests.

Use checks:

```cpp
const auto& runtime = created.session.runtime();
const auto& unit = runtime.units.requireUnit(0);
const auto& world = requireById(runtime.world.units, 0);
const auto& attack = requireById(runtime.attacks.units, 0);
CHECK(world.position.x == unit.motion.position.x);
CHECK(world.position.y == unit.motion.position.y);
CHECK(attack.position.x == unit.motion.position.x);
CHECK(attack.position.y == unit.motion.position.y);
```

---

## Final Verification

Run:

```powershell
git diff --check -- src\battle\BattleRuntimeSession.cpp src\battle\BattleRuntimeSession.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h tests\BattleInitializationUnitTests.cpp tests\BattleFrameRunnerRuntimeUnitTests.cpp tests\BattleCoreUnitTests.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Run boundary searches:

```powershell
rg -n "BattleRuntimeSetupConfiguration|createConfigured|applySetupConfiguration|makeInitializedBattleRuntimeConfiguration" src tests
rg -n "BattleRuntimeUnitSetup|BattleRuntimeBootstrap" src tests
```

Expected:

- No post-construction runtime configuration API remains.
- No new per-unit setup DTO was introduced.
- No bootstrap file/name was introduced.
- Adapter no longer builds runtime subsystem stores.
- Initialized-session tests prove units move, attacks are seeded, projectile combat resolves, and derived motion stores stay aligned.

---

## Expected Code Reduction

This plan should reduce code by deleting the callback/config loop and adapter runtime store builders. Some helper code moves into `BattleRuntimeSession.cpp`, but the net improvement is that there is only one runtime construction path and one owner for derived runtime stores.

The important maintainability win is not line count alone. It is removing the class of bugs where a refactor updates `runtime.units` but forgets `runtime.world`, `runtime.attacks`, `runtime.damage`, or `runtime.action`.

