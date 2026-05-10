# Battle Adapter Real Subtraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the adapter-only DTOs and repeated setup conversion passes that still keep battle initialization split across scene, adapter, and runtime.

**Architecture:** This is subtraction-first. Setup should build the runtime seed/action seed data directly while the legacy `Role`/`Magic` facts are still in hand, instead of creating one broad adapter setup row and re-projecting it several times later. The adapter should shrink to orchestration for this batch; deeper runtime configuration ownership can follow after the duplicate DTO/pass removal is complete.

**Tech Stack:** C++20, MSBuild solution `kys.sln`, Catch2 unit tests, existing `BattleSceneSetupBuilder`, `BattleSceneBattleAdapter`, and battle runtime modules.

---

## File Map

- `src/BattleSceneBattleAdapter.h/.cpp`: delete adapter-only skill DTO and setup-seed/action-seed conversion helpers; narrow build context fields.
- `src/BattleSceneSetupBuilder.h/.cpp`: emit runtime setup seed rows and action plan seeds directly while building setup units.
- `src/BattleSceneHades.cpp/.h`: pass the already-built setup seed/action seeds into runtime initialization instead of relying on adapter re-projection.
- `tests/BattleSceneSetupBuilderUnitTests.cpp`: assert direct runtime seed/action seed output.
- `tests/BattleInitializationUnitTests.cpp`: update adapter creation tests to provide direct runtime setup seed.

---

## Non-Goals

- Do not remove `BattleSceneBattleAdapter` entirely in this batch.
- Do not introduce a new broad DTO that recreates `BattleRuntimeBuildContext` under a different name.
- Do not move combo/equipment/neigong definition extraction in this batch; that is mostly ownership cleanup, not real code subtraction.
- Do not change battle behavior.

---

### Task 1: Delete `BattleSetupSkillSnapshot`

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `tests/BattleSceneSetupBuilderUnitTests.cpp`

- [x] **Step 1: Replace adapter skill fields with core action skill seeds**

In `src/BattleSceneBattleAdapter.h`, delete:

```cpp
struct BattleSetupSkillSnapshot
{
    int id = -1;
    std::string name;
    int soundId = -1;
    int hurtType = 0;
    int attackAreaType = -1;
    int magicType = -1;
    int visualEffectId = -1;
    int selectDistance = 1;
    int actProperty = 0;
    int magicPower = 0;
};
```

Change `BattleSetupUnitInput`:

```cpp
Battle::BattleActionSkillSeed normalSkill;
Battle::BattleActionSkillSeed ultimateSkill;
```

- [x] **Step 2: Make setup builder emit core skill seed directly**

In `src/BattleSceneSetupBuilder.cpp`, change `makeSkillSnapshot(...)` to return `Battle::BattleActionSkillSeed`:

```cpp
Battle::BattleActionSkillSeed makeSkillSeed(
    const Role& role,
    int star,
    Magic* magic)
{
    Battle::BattleActionSkillSeed seed;
    if (!magic)
    {
        return seed;
    }

    seed.id = magic->ID;
    seed.name = magic->Name;
    seed.soundId = magic->SoundID;
    seed.hurtType = magic->HurtType;
    seed.attackAreaType = magic->AttackAreaType;
    seed.magicType = magic->MagicType;
    seed.visualEffectId = magic->EffectID;
    seed.selectDistance = magic->SelectDistance;
    seed.actProperty = actPropertyForMagicType(role, magic->MagicType);
    seed.magicPower = magicPowerForStar(role, magic->ID, star);
    return seed;
}
```

Update assignments in `makeSetupUnit(...)`:

```cpp
unit.normalSkill = makeSkillSeed(role, request.star, selectNormalSkill(role, request.star, request.magicSlots));
unit.ultimateSkill = makeSkillSeed(role, request.star, selectUltimateSkill(role, request.star, request.magicSlots));
```

- [x] **Step 3: Delete adapter skill conversion helper**

In `src/BattleSceneBattleAdapter.cpp`, delete:

```cpp
Battle::BattleActionSkillSeed makeBattleActionSkillSeed(const BattleSetupSkillSnapshot& skill)
```

Change `makeBattleActionPlanSeed(...)`:

```cpp
Battle::BattleActionPlanSeed makeBattleActionPlanSeed(const BattleSetupUnitInput& role)
{
    Battle::BattleActionPlanSeed seed;
    seed.unitId = role.unitId;
    seed.hasEquippedSkill = role.hasEquippedSkill;
    seed.normalSkill = role.normalSkill;
    seed.ultimateSkill = role.ultimateSkill;
    return seed;
}
```

- [x] **Step 4: Verify no duplicate skill DTO remains**

Run:

```powershell
rg -n "BattleSetupSkillSnapshot|makeBattleActionSkillSeed" src tests
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected:
- `rg` returns no matches.
- Build succeeds.
- Tests pass.

---

### Task 2: Emit Runtime Setup Seed Rows During Scene Setup

**Files:**
- Modify: `src/BattleSceneSetupBuilder.h`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleSceneSetupBuilderUnitTests.cpp`

- [x] **Step 1: Extend setup build result with runtime seed fragments**

In `src/BattleSceneSetupBuilder.h`, change `BattleSceneSetupBuildResult`:

```cpp
struct BattleSceneSetupBuildResult
{
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> units;
    std::vector<KysChess::Battle::BattleInitializationUnitSeed> initializationUnits;
    std::vector<KysChess::Battle::BattleSetupRosterUnit> allyRoster;
    std::vector<KysChess::Battle::BattleSetupRosterUnit> enemyRoster;
    std::vector<KysChess::Battle::BattleInitializationCloneSource> cloneSources;
};
```

- [x] **Step 2: Add builders that consume the already-created setup unit**

In `src/BattleSceneSetupBuilder.cpp`, add local helpers:

```cpp
KysChess::Battle::BattleInitializationUnitSeed makeInitializationUnitSeed(
    const KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput& unit)
{
    return {
        unit.unitId,
        unit.realRoleId,
        unit.team,
        unit.star,
        unit.cost,
        unit.vitals.maxHp,
        unit.stats.attack,
        unit.stats.defence,
        unit.stats.speed,
        unit.fist,
        unit.sword,
        unit.knife,
        unit.unusual,
        unit.hiddenWeapon,
        {},
    };
}

KysChess::Battle::BattleSetupRosterUnit makeRosterUnit(
    const KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput& unit)
{
    return {
        unit.unitId,
        unit.realRoleId,
        unit.team,
        unit.star,
        unit.cost,
        unit.weaponId,
        unit.armorId,
        unit.chessInstanceId,
        unit.fightsWon,
        unit.sourceOrder,
    };
}

KysChess::Battle::BattleInitializationCloneSource makeCloneSource(
    const KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput& unit)
{
    return {
        unit.unitId,
        unit.realRoleId,
        unit.vitals.maxHp + unit.stats.attack + unit.stats.defence,
        unit.star,
        unit.chessInstanceId,
        unit.sourceOrder,
    };
}
```

- [x] **Step 3: Populate seed fragments in `buildSetupUnits(...)`**

In `buildSetupUnits(...)`, after each `makeSetupUnit(...)` call:

```cpp
auto setupUnit = makeSetupUnit(request, opponents);
result.initializationUnits.push_back(makeInitializationUnitSeed(setupUnit));

auto rosterUnit = makeRosterUnit(setupUnit);
if (setupUnit.team == 0)
{
    result.allyRoster.push_back(rosterUnit);
    result.cloneSources.push_back(makeCloneSource(setupUnit));
}
else
{
    result.enemyRoster.push_back(rosterUnit);
}

result.units.push_back(std::move(setupUnit));
```

At the end of `buildSetupUnits(...)`:

```cpp
std::ranges::sort(result.allyRoster, {}, &KysChess::Battle::BattleSetupRosterUnit::sourceOrder);
std::ranges::sort(result.enemyRoster, {}, &KysChess::Battle::BattleSetupRosterUnit::sourceOrder);
```

- [x] **Step 4: Delete duplicate seed row construction from adapter**

In `src/BattleSceneBattleAdapter.cpp`, remove the loops in `makeBattleRuntimeSetupSeed(...)` that rebuild:

```cpp
setup.allyRoster
setup.enemyRoster
setup.units
setup.cloneSources
```

This function should no longer derive those from `context.units`.

- [x] **Step 5: Verify direct setup seed output**

Add or update a test in `tests/BattleSceneSetupBuilderUnitTests.cpp`:

```cpp
TEST_CASE("BattleSceneSetupBuilder_EmitsRuntimeSeedRowsWithSetupUnits", "[battle][scene_setup]")
{
    auto role = makeRoleForSetupTest();
    BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.source = &role;
    request.team = 0;
    request.gridX = 3;
    request.gridY = 4;
    request.star = 2;
    request.sourceOrder = 7;
    request.position = { 128, 256, 0 };

    auto result = buildSetupUnits(std::span<const BattleSceneSetupUnitRequest>(&request, 1));

    REQUIRE(result.units.size() == 1);
    REQUIRE(result.initializationUnits.size() == 1);
    CHECK(result.initializationUnits[0].unitId == result.units[0].unitId);
    CHECK(result.initializationUnits[0].realRoleId == result.units[0].realRoleId);
    CHECK(result.initializationUnits[0].maxHp == result.units[0].vitals.maxHp);
    REQUIRE(result.allyRoster.size() == 1);
    CHECK(result.allyRoster[0].unitId == 0);
    CHECK(result.allyRoster[0].sourceOrder == 7);
    REQUIRE(result.cloneSources.size() == 1);
    CHECK(result.cloneSources[0].sourceUnitId == 0);
}
```

Use existing setup-builder test helpers for role creation if present; otherwise create the minimal valid `Role` inside the test.

- [x] **Step 6: Verify task 2**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected: build succeeds and tests pass.

---

### Task 3: Make Runtime Setup Seed an Explicit Input

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [x] **Step 1: Add setup seed to scene setup input**

In `src/BattleSceneBattleAdapter.h`, change `BattleRuntimeSceneSetupInput`:

```cpp
struct BattleRuntimeSceneSetupInput
{
    std::vector<BattleSetupUnitInput> units;
    Battle::BattleRuntimeSetupSeed runtimeSetupSeed;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};
```

Delete from this struct:

```cpp
std::vector<int> obtainedNeigongMagicIds;
std::vector<std::pair<int, int>> cloneSpawnCells;
```

- [x] **Step 2: Use supplied runtime setup seed directly**

In `createInitializedBattleRuntimeSession(...)`, replace:

```cpp
init.setup = makeBattleRuntimeSetupSeed(setup);
```

with:

```cpp
init.setup = setup.runtimeSetupSeed;
```

- [x] **Step 3: Delete adapter-side `makeBattleRuntimeSetupSeed(...)`**

In `src/BattleSceneBattleAdapter.cpp`, delete:

```cpp
Battle::BattleRuntimeSetupSeed makeBattleRuntimeSetupSeed(const BattleRuntimeSceneSetupInput& context)
```

Keep combo/equipment/neigong definition extraction in place for now. Replace `makeBattleRuntimeSetupSeed(...)` with a narrow helper that fills only those definition tables onto the supplied seed:

```cpp
void populateBattleRuntimeSetupDefinitions(Battle::BattleRuntimeSetupSeed& setup)
{
    setup.comboDefinitions = makeBattleSetupComboDefinitions();
    setup.equipmentDefinitions = makeBattleSetupEquipmentDefinitions();
    setup.equipmentSynergies = makeBattleSetupEquipmentSynergyDefinitions();
    setup.neigongDefinitions = makeBattleSetupNeigongDefinitions();
}
```

- [x] **Step 4: Update Hades to pass seed fragments from setup build result**

In `BattleSceneHades::onEntrance()`, keep the full build result instead of extracting only units:

```cpp
auto setupBuild = KysChess::BattleSceneSetupBuilder::buildSetupUnits(setupRequests);
```

Change `initializeBattleRuntime(...)` and `makeBattleRuntimeBuildContext(...)` signatures to take the build result:

```cpp
void initializeBattleRuntime(KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild);

KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext makeBattleRuntimeBuildContext(
    KysChess::BattleSceneSetupBuilder::BattleSceneSetupBuildResult setupBuild);
```

Inside `makeBattleRuntimeBuildContext(...)`:

```cpp
context.setup.units = std::move(setupBuild.units);
context.setup.runtimeSetupSeed.units = std::move(setupBuild.initializationUnits);
context.setup.runtimeSetupSeed.allyRoster = std::move(setupBuild.allyRoster);
context.setup.runtimeSetupSeed.enemyRoster = std::move(setupBuild.enemyRoster);
context.setup.runtimeSetupSeed.cloneSources = std::move(setupBuild.cloneSources);
```

- [x] **Step 5: Populate clone cells where clone positions are known**

Still in `BattleSceneHades::makeBattleRuntimeBuildContext(...)`, replace adapter-owned clone cell population with direct seed population:

```cpp
for (const auto& [x, y] : clone_spawn_positions_)
{
    bool occupied = false;
    for (const auto& unit : context.setup.units)
    {
        if (unit.alive && unit.gridX == x && unit.gridY == y)
        {
            occupied = true;
            break;
        }
    }
    context.setup.runtimeSetupSeed.cloneCells.push_back({ x, y, true, occupied });
}
```

- [x] **Step 6: Verify task 3**

Run:

```powershell
rg -n "makeBattleRuntimeSetupSeed|cloneSpawnCells|obtainedNeigongMagicIds" src\BattleSceneBattleAdapter.* src\BattleSceneHades.*
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected:
- Adapter no longer references `makeBattleRuntimeSetupSeed`, `cloneSpawnCells`, or `obtainedNeigongMagicIds`.
- Build succeeds.
- Tests pass.

---

### Task 4: Emit Action Plan Seeds Directly During Setup

**Files:**
- Modify: `src/BattleSceneSetupBuilder.h`
- Modify: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleSceneSetupBuilderUnitTests.cpp`

- [x] **Step 1: Add action plan seed output**

In `BattleSceneSetupBuildResult`, add:

```cpp
std::vector<KysChess::Battle::BattleActionPlanSeed> actionPlanSeeds;
```

- [x] **Step 2: Build action seed from setup unit inside setup builder**

In `src/BattleSceneSetupBuilder.cpp`, add:

```cpp
KysChess::Battle::BattleActionPlanSeed makeActionPlanSeed(
    const KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput& unit)
{
    KysChess::Battle::BattleActionPlanSeed seed;
    seed.unitId = unit.unitId;
    seed.hasEquippedSkill = unit.hasEquippedSkill;
    seed.normalSkill = unit.normalSkill;
    seed.ultimateSkill = unit.ultimateSkill;
    return seed;
}
```

In `buildSetupUnits(...)`, after `setupUnit` is built and before moving it into `result.units`:

```cpp
result.actionPlanSeeds.push_back(makeActionPlanSeed(setupUnit));
```

- [x] **Step 3: Store action seeds in build context**

In `BattleRuntimeSceneSetupInput`, add:

```cpp
std::vector<Battle::BattleActionPlanSeed> actionPlanSeeds;
```

In `BattleSceneHades::makeBattleRuntimeBuildContext(...)`:

```cpp
context.setup.actionPlanSeeds = std::move(setupBuild.actionPlanSeeds);
```

- [x] **Step 4: Delete adapter action seed conversion pass**

In `src/BattleSceneBattleAdapter.cpp`, delete:

```cpp
struct BattleActionPlanInputContext
makeBattleActionPlanSeed
initializeBattleActionPlanInputs
```

In `makeInitializedBattleRuntimeConfiguration(...)`, replace the context setup with direct map population:

```cpp
config.action.planSeeds.clear();
for (const auto& seed : context.setup.actionPlanSeeds)
{
    config.action.planSeeds.emplace(seed.unitId, seed);
}
config.action.castConfig = context.rules.castConfig;
config.action.castGeometry = context.rules.castGeometry;
config.action.actionRules = context.rules.action;
```

- [x] **Step 5: Verify task 5**

Run:

```powershell
rg -n "BattleActionPlanInputContext|initializeBattleActionPlanInputs|makeBattleActionPlanSeed" src\BattleSceneBattleAdapter.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected:
- `rg` returns no matches.
- Build succeeds.
- Tests pass.

---

### Task 5: Narrow Adapter Build Context to Runtime Creation Inputs

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [x] **Step 1: Rename the broad setup bucket**

In `src/BattleSceneBattleAdapter.h`, rename `BattleRuntimeSceneSetupInput` to `BattleRuntimeCreationInput`.

Keep only fields still required by `createInitializedBattleRuntimeSession(...)`:

```cpp
struct BattleRuntimeCreationInput
{
    std::vector<BattleSetupUnitInput> units;
    Battle::BattleRuntimeSetupSeed runtimeSetupSeed;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::vector<Battle::BattleActionPlanSeed> actionPlanSeeds;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};
```

Change `BattleRuntimeBuildContext`:

```cpp
struct BattleRuntimeBuildContext
{
    BattleRuntimeCreationInput input;
    Battle::BattleRuntimeRulesConfig rules;
    unsigned int randomSeed = 1;
};
```

- [x] **Step 2: Update adapter implementation names**

In `src/BattleSceneBattleAdapter.cpp`, replace `context.setup` with `context.input` and update helper signatures:

```cpp
const BattleSetupUnitInput& requireSetupUnit(
    const BattleRuntimeCreationInput& input,
    int unitId)
```

```cpp
BattleInitializationSceneApplyResult makeBattleInitializationSceneApplyResult(
    const BattleRuntimeCreationInput& input,
    const Battle::BattleInitializationResult& initialization,
    const Battle::BattleRuntimeState& runtime)
```

- [x] **Step 3: Update Hades and tests**

In `src/BattleSceneHades.cpp` and `tests/BattleInitializationUnitTests.cpp`, replace:

```cpp
context.setup.
```

with:

```cpp
context.input.
```

- [x] **Step 4: Verify naming reflects narrowed responsibility**

Run:

```powershell
rg -n "BattleRuntimeSceneSetupInput|context\\.setup" src\BattleSceneBattleAdapter.* src\BattleSceneHades.* tests\BattleInitializationUnitTests.cpp
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
```

Expected:
- No old setup bucket name remains.
- Build succeeds.
- Tests pass.

---

## Final Verification

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Run reduction searches:

```powershell
rg -n "BattleSetupSkillSnapshot|makeBattleActionSkillSeed|BattleActionPlanInputContext|initializeBattleActionPlanInputs|makeBattleActionPlanSeed" src tests
rg -n "makeBattleRuntimeSetupSeed|BattleRuntimeSceneSetupInput|context\\.setup" src tests
```

Expected:
- Adapter-only skill DTO is gone.
- Adapter no longer derives setup seed rows.
- Adapter no longer converts setup unit skill fields into action plan seeds.
- Adapter build context is named as runtime creation input, not a mixed scene setup bucket.

## Expected Code Reduction

This batch should remove real bridge code, not only relocate it:

- Delete `BattleSetupSkillSnapshot`.
- Delete `makeBattleActionSkillSeed`.
- Delete `BattleActionPlanInputContext`.
- Delete `initializeBattleActionPlanInputs`.
- Delete adapter-side runtime setup seed construction.

The adapter will still contain runtime unit creation and initialized scene unit creation after this batch. Those are the next candidates, but they should be handled only after this plan removes the duplicate setup seed/action seed bridges.
