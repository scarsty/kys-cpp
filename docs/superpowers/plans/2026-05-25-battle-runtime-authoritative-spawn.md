# Battle Runtime Authoritative Spawn Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make normal units and summoned clones enter battle through one fresh runtime spawn path, with each mutable battle field owned by exactly one persistent store.

**Architecture:** Initialization should resolve `BattleRuntimeUnitSpawn` packets first, then append every packet through one `appendRuntimeUnit(...)` function. Original units and clones differ only in how their spawn packet is produced; once produced, unit/status/damage/movement/combo/action rows are attached by the same code. Runtime snapshots may still copy fields for one transaction, but persistent ownership must be unambiguous.

**Tech Stack:** C++20, Catch2, PowerShell, ripgrep, Visual Studio/MSBuild via `.github/build-command.ps1`.

---

## Authoritative Ownership

Use this table as the implementation contract.

| Field group | Persistent owner | Notes |
| --- | --- | --- |
| Unit identity/profile/team | `BattleRuntimeUnit` | `id`, `cloneSourceUnitId`, `realRoleId`, `name`, `headId`, `fightFrames`, `skillNames`, `team`, equipment/chess instance ownership. |
| Current life/resources/stats | `BattleRuntimeUnit` | `alive`, `vitals`, `stats`, `shield`, `invincible`, action/animation fields. |
| Status timers/effects | `BattleStatusRuntimeUnit` | Poison, bleed, frozen, mp-block timer, periodic immunity timer, temp attack buffs, damage reduce debuffs. |
| Damage lifecycle | `BattleDamageRuntimeUnit` | Hurt-invinc source value, first-hit block counter, death prevention used/state, kill-heal/kill-invinc hooks. |
| Combo config and combo-trigger state | `RoleComboState` | Effect configuration, trigger counters, adaptation/ramping counters, force-pull remaining, clone marker, pending combo actions. It must not own current shield, status timers, or damage counters. |
| Movement AI/physics | `BattleMovementAgentState` | Spawn initializes from unit motion. Runtime movement owns target/slot/dash transient state. |
| Action plan | `runtime.action.planSeeds` | Spawn may provide a plan seed. Clone copies source plan with only `unitId` changed. |

DTOs such as `BattleDamageUnitState`, `BattleStatusUnitState`, and hit resolver inputs may copy values for one frame transaction. They are not persistent owners.

---

## File Structure

- Create `src/battle/BattleRuntimeUnitSpawn.h`: spawn packet type plus factory/append declarations.
- Create `src/battle/BattleRuntimeUnitSpawn.cpp`: shared initialization for status/damage/movement/action rows.
- Modify `src/battle/BattleRuntimeSession.cpp`: build baseline spawn packets, run initialization on packets, append packets into `BattleRuntimeState`, and configure global runtime stores.
- Modify `src/battle/BattleInitialization.h`: change initialization API from mutating `BattleRuntimeState` to mutating spawn packets.
- Modify `src/battle/BattleInitialization.cpp`: resolve original spawns, create clone spawns from source spawn templates, and remove direct store writes.
- Modify `src/battle/BattleUnitStore.h`: remove duplicate mutable fields after their authoritative owners are in place.
- Modify `src/battle/BattleDamageSystem.h` / `src/battle/BattleDamageSystem.cpp`: keep damage lifecycle state in damage DTO/runtime types.
- Modify `src/battle/BattleStatusSystem.h` / `src/battle/BattleStatusSystem.cpp`: keep status runtime as mp-block owner and remove unit mirror sync.
- Modify `src/battle/BattleTeamEffectSystem.h` / `src/battle/BattleTeamEffectSystem.cpp`: read mp-block from status and mp recovery bonus from combo.
- Modify `src/battle/BattleCore.cpp`: update snapshot builders and resource paths to read authoritative owners.
- Modify `src/ChessBattleEffects.h` / `src/ChessBattleEffects.cpp`: remove persistent mutable fields that move to status/damage/unit.
- Modify `src/BattleSceneHades.cpp`: read first-hit block presentation from damage runtime or presentation snapshots, not `BattleRuntimeUnit`.
- Modify project files: `src/CMakeLists.txt`, `src/kys.vcxproj`, `src/kys.vcxproj.filters`.
- Modify tests under `tests/`: update ownership assertions and add merged-spawn clone tests.

---

## Task 1: Add Characterization Tests For Fresh Clone Spawn Ownership

**Files:**
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Add a clone fresh-store test**

Add this test after `BattleRuntimeSession_CloneProfileKeepsRenderingWithoutRosterOwnership` in `tests/BattleInitializationUnitTests.cpp`:

```cpp
TEST_CASE("BattleRuntimeSession_CloneUsesFreshSpawnStores", "[battle][initialization][runtime_session]")
{
    std::map<int, RoleComboState> comboStates;
    auto& sourceCombo = comboStates[0];
    sourceCombo.cloneSummonCount = 1;
    sourceCombo.shieldPctMaxHP = 25;
    sourceCombo.blockFirstHitsCount = 2;
    sourceCombo.damageImmunityAfterFrames = 12;
    sourceCombo.damageImmunityDuration = 5;
    sourceCombo.mpRecoveryBonusPct = 50;

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    auto source = makeRuntimeProfileTestSource();
    source.animation = { 9, 10, 3, 1 };
    source.haveAction = true;
    source.operationType = BattleOperationType::Melee;
    source.operationCount = 4;
    source.physicalPower = 33;
    source.invincible = 8;
    source.motion.velocity = { 7, 8, 0 };
    source.motion.acceleration = { 0, 0, -4 };
    source.hasEquippedSkill = true;
    source.normalSkill = makeHadesTestSkillSeed();
    source.ultimateSkill = makeHadesTestSkillSeed(1, 2);

    input.actionPlanSeeds.push_back({
        source.unitId,
        source.hasEquippedSkill,
        source.normalSkill,
        source.ultimateSkill,
    });
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    const auto& sourceUnit = session.requireRuntimeUnit(0);
    const auto& cloneUnit = session.requireRuntimeUnit(1);
    const auto& runtime = session.runtime();

    CHECK(cloneUnit.cloneSourceUnitId == 0);
    CHECK(cloneUnit.realRoleId == sourceUnit.realRoleId);
    CHECK(cloneUnit.vitals.maxHp == sourceUnit.vitals.maxHp);
    CHECK(cloneUnit.vitals.hp == cloneUnit.vitals.maxHp);
    CHECK(cloneUnit.stats.attack == sourceUnit.stats.attack);
    CHECK(cloneUnit.grid.x == 3);
    CHECK(cloneUnit.grid.y == 4);
    CHECK(cloneUnit.animation.cooldown == 0);
    CHECK(cloneUnit.animation.actFrame == 0);
    CHECK(cloneUnit.animation.actType == -1);
    CHECK_FALSE(cloneUnit.haveAction);
    CHECK(cloneUnit.operationType == BattleOperationType::None);
    CHECK(cloneUnit.operationCount == 0);
    CHECK(cloneUnit.physicalPower == sourceUnit.physicalPower);
    CHECK(cloneUnit.invincible == sourceUnit.invincible);
    CHECK(cloneUnit.weaponId == -1);
    CHECK(cloneUnit.armorId == -1);
    CHECK(cloneUnit.chessInstanceId == -1);

    const auto& cloneStatus = requireById(runtime.status.units, 1);
    CHECK(cloneStatus.effects.damageImmunityAfterFrames == 12);
    CHECK(cloneStatus.effects.damageImmunityDuration == 5);
    CHECK(cloneStatus.effects.damageImmunityTimer == 12);

    const auto& cloneDamage = requireById(runtime.damage.unitExtras, 1);
    CHECK(cloneDamage.blockFirstHitsRemaining == 2);

    const auto cloneAgentIt = runtime.movement.agents.find(1);
    REQUIRE(cloneAgentIt != runtime.movement.agents.end());
    CHECK(cloneAgentIt->second.targetId == -1);
    CHECK(cloneAgentIt->second.physics.position.x == cloneUnit.motion.position.x);
    CHECK(cloneAgentIt->second.physics.position.y == cloneUnit.motion.position.y);
    CHECK(cloneAgentIt->second.physics.velocity.x == cloneUnit.motion.velocity.x);
    CHECK(cloneAgentIt->second.physics.acceleration.z == cloneUnit.motion.acceleration.z);

    const auto clonePlanIt = runtime.action.planSeeds.find(1);
    REQUIRE(clonePlanIt != runtime.action.planSeeds.end());
    CHECK(clonePlanIt->second.unitId == 1);
    CHECK(clonePlanIt->second.normalSkill.id == source.normalSkill.id);
}
```

- [ ] **Step 2: Run the focused test and verify the current failure**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "BattleRuntimeSession_CloneUsesFreshSpawnStores"
```

Expected before implementation: the test fails because the clone movement agent is copied from the source agent and/or clone damage/status rows are not attached through a shared spawn path. If the local test executable path differs, run the existing battle test target from the Visual Studio test adapter or the generated build directory.

- [ ] **Step 3: Commit the failing characterization test**

Run:

```powershell
git add tests\BattleInitializationUnitTests.cpp
git commit -m "test: characterize clone fresh runtime stores"
```

Expected: commit succeeds with only the new test staged.

---

## Task 2: Introduce The Runtime Spawn Packet

**Files:**
- Create: `src/battle/BattleRuntimeUnitSpawn.h`
- Create: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/CMakeLists.txt`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`

- [ ] **Step 1: Create the spawn header**

Create `src/battle/BattleRuntimeUnitSpawn.h`:

```cpp
#pragma once

#include "BattleCore.h"

#include <optional>

namespace KysChess::Battle
{

struct BattleRuntimeUnitSpawn
{
    BattleRuntimeUnit unit;
    RoleComboState combo;
    BattleStatusRuntimeUnit status;
    BattleDamageRuntimeUnit damage;
    BattleMovementAgentState movement;
    std::optional<BattleActionPlanSeed> actionPlan;
};

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit,
    const RoleComboState& combo);

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(
    int unitId,
    const RoleComboState& combo);

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit);

void refreshRuntimeUnitSpawnStores(BattleRuntimeUnitSpawn& spawn);

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    RoleComboState combo,
    std::optional<BattleActionPlanSeed> actionPlan = std::nullopt);

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn);

}  // namespace KysChess::Battle
```

- [ ] **Step 2: Create the spawn implementation**

Create `src/battle/BattleRuntimeUnitSpawn.cpp`:

```cpp
#include "BattleRuntimeUnitSpawn.h"

#include "BattleStatusSystem.h"

#include <cassert>

namespace KysChess::Battle
{

namespace
{

constexpr int NormalDamageTextSize = 30;
constexpr int UltDamageTextSize = 44;

BattlePresentationColor damageTextColor(int team, bool emphasized)
{
    if (team == 0)
    {
        return emphasized
            ? BattlePresentationColor{ 255, 45, 85, 255 }
            : BattlePresentationColor{ 255, 90, 79, 255 };
    }
    return emphasized
        ? BattlePresentationColor{ 47, 128, 255, 255 }
        : BattlePresentationColor{ 102, 207, 255, 255 };
}

BattleDamagePresentationStyle makeDamagePresentationStyle(int team)
{
    BattleDamagePresentationStyle style;
    style.normalDamageColor = damageTextColor(team, false);
    style.emphasizedDamageColor = damageTextColor(team, true);
    style.executeTextColor = { 255, 136, 48, 255 };
    style.normalDamageTextSize = NormalDamageTextSize;
    style.emphasizedDamageTextSize = UltDamageTextSize;
    style.executeTextSize = UltDamageTextSize;
    return style;
}

}  // namespace

BattleStatusRuntimeUnit makeInitialStatusRuntimeUnit(
    const BattleRuntimeUnit& unit,
    const RoleComboState& combo)
{
    return makeBattleStatusRuntimeUnit(makeBattleStatusUnitState(unit, combo));
}

BattleDamageRuntimeUnit makeInitialDamageRuntimeUnit(
    int unitId,
    const RoleComboState& combo)
{
    BattleDamageRuntimeUnit damage;
    damage.id = unitId;
    damage.hurtInvincFrames = combo.hurtInvincFrames;
    damage.blockFirstHitsRemaining = combo.blockFirstHitsRemaining;
    damage.deathPrevention = combo.deathPrevention;
    damage.deathPreventionUsed = combo.deathPreventionUsed;
    damage.deathPreventionFrames = combo.deathPreventionFrames;
    damage.killHealPct = combo.killHealPct;
    damage.killInvincFrames = combo.killInvincFrames;
    damage.bloodlustAttackPerKill = combo.bloodlustATKPerKill;
    return damage;
}

BattleMovementAgentState makeInitialMovementAgent(
    const BattleRuntimeUnit& unit)
{
    BattleMovementAgentState agent;
    agent.physics.position = unit.motion.position;
    agent.physics.velocity = unit.motion.velocity;
    agent.physics.acceleration = unit.motion.acceleration;
    return agent;
}

void refreshRuntimeUnitSpawnStores(BattleRuntimeUnitSpawn& spawn)
{
    assert(spawn.unit.id >= 0);

    spawn.unit.shield = spawn.combo.shield;
    spawn.unit.mpBlocked = spawn.combo.mpBlockTimer > 0;
    spawn.unit.mpRecoveryBonusPct = spawn.combo.mpRecoveryBonusPct;
    spawn.status = makeInitialStatusRuntimeUnit(spawn.unit, spawn.combo);
    spawn.damage = makeInitialDamageRuntimeUnit(spawn.unit.id, spawn.combo);
    spawn.movement = makeInitialMovementAgent(spawn.unit);
    if (spawn.actionPlan)
    {
        spawn.actionPlan->unitId = spawn.unit.id;
    }
}

BattleRuntimeUnitSpawn makeRuntimeUnitSpawn(
    BattleRuntimeUnit unit,
    RoleComboState combo,
    std::optional<BattleActionPlanSeed> actionPlan)
{
    BattleRuntimeUnitSpawn spawn;
    spawn.unit = std::move(unit);
    spawn.combo = std::move(combo);
    spawn.actionPlan = std::move(actionPlan);
    refreshRuntimeUnitSpawnStores(spawn);
    return spawn;
}

void appendRuntimeUnit(BattleRuntimeState& runtime, BattleRuntimeUnitSpawn spawn)
{
    const int unitId = spawn.unit.id;
    assert(unitId >= 0);
    assert(spawn.status.id == unitId);
    assert(spawn.damage.id == unitId);
    if (spawn.actionPlan)
    {
        assert(spawn.actionPlan->unitId == unitId);
    }

    runtime.unitStore.units.push_back(std::move(spawn.unit));
    runtime.status.units.push_back(std::move(spawn.status));
    runtime.damage.unitExtras.push_back(std::move(spawn.damage));
    runtime.movement.agents.emplace(unitId, std::move(spawn.movement));
    runtime.combo.units.emplace(unitId, std::move(spawn.combo));
    runtime.damage.presentationStylesByDefender.emplace(
        unitId,
        makeDamagePresentationStyle(runtime.unitStore.requireUnit(unitId).team));
    if (spawn.actionPlan)
    {
        runtime.action.planSeeds.emplace(unitId, std::move(*spawn.actionPlan));
    }
}

}  // namespace KysChess::Battle
```

- [ ] **Step 3: Add the files to build metadata**

In `src/CMakeLists.txt`, add:

```cmake
    battle/BattleRuntimeUnitSpawn.cpp
```

near the other `battle/*.cpp` runtime sources.

In `src/kys.vcxproj`, add:

```xml
    <ClCompile Include="battle\BattleRuntimeUnitSpawn.cpp" />
```

near the other battle `<ClCompile>` items, and add:

```xml
    <ClInclude Include="battle\BattleRuntimeUnitSpawn.h" />
```

near the other battle `<ClInclude>` items.

In `src/kys.vcxproj.filters`, add matching filter entries:

```xml
    <ClCompile Include="battle\BattleRuntimeUnitSpawn.cpp">
      <Filter>battle</Filter>
    </ClCompile>
```

and:

```xml
    <ClInclude Include="battle\BattleRuntimeUnitSpawn.h">
      <Filter>battle</Filter>
    </ClInclude>
```

- [ ] **Step 4: Build to verify the new module compiles**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .github\build-command.ps1
```

Expected: compile succeeds. If final linking fails because the game executable is running, treat compilation as successful per repo instructions.

- [ ] **Step 5: Commit the spawn packet module**

Run:

```powershell
git add src\battle\BattleRuntimeUnitSpawn.h src\battle\BattleRuntimeUnitSpawn.cpp src\CMakeLists.txt src\kys.vcxproj src\kys.vcxproj.filters
git commit -m "refactor: add battle runtime unit spawn packet"
```

---

## Task 3: Route BattleRuntimeSession Through Spawn Packets

**Files:**
- Modify: `src/battle/BattleRuntimeSession.cpp`

- [ ] **Step 1: Include the spawn module**

In `src/battle/BattleRuntimeSession.cpp`, add:

```cpp
#include "BattleRuntimeUnitSpawn.h"
```

- [ ] **Step 2: Remove combo mirroring from `makeRuntimeUnit`**

Change:

```cpp
BattleRuntimeUnit makeRuntimeUnit(
    const BattleSetupUnitInput& setup,
    const RoleComboState* combo)
```

to:

```cpp
BattleRuntimeUnit makeRuntimeUnit(const BattleSetupUnitInput& setup)
```

Then delete this block from the function:

```cpp
    if (combo)
    {
        unit.shield = combo->shield;
        unit.mpBlocked = combo->mpBlockTimer > 0;
        unit.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
    }
```

- [ ] **Step 3: Delete local row constructors now owned by the spawn module**

Delete `damageTextColor(...)`, `makeDamagePresentationStyle(...)`, `makeInitialDamageRuntimeUnit(...)`, and `makeInitializedMovementAgent(...)` from `src/battle/BattleRuntimeSession.cpp`.

- [ ] **Step 4: Add canonical spawn construction**

Replace `buildCanonicalRuntime(...)` with:

```cpp
std::map<int, BattleActionPlanSeed> makeActionPlanSeedMap(
    const std::vector<BattleActionPlanSeed>& seeds)
{
    std::map<int, BattleActionPlanSeed> result;
    for (const auto& seed : seeds)
    {
        result.emplace(seed.unitId, seed);
    }
    return result;
}

std::vector<BattleRuntimeUnitSpawn> buildCanonicalSpawns(
    const BattleRuntimeSessionCreationInput& input)
{
    const auto actionPlans = makeActionPlanSeedMap(input.actionPlanSeeds);

    std::vector<BattleRuntimeUnitSpawn> spawns;
    spawns.reserve(input.units.size());
    for (const auto& setup : input.units)
    {
        RoleComboState combo;
        if (const auto comboIt = input.comboStates.find(setup.unitId);
            comboIt != input.comboStates.end())
        {
            combo = comboIt->second;
        }

        std::optional<BattleActionPlanSeed> actionPlan;
        if (const auto actionIt = actionPlans.find(setup.unitId);
            actionIt != actionPlans.end())
        {
            actionPlan = actionIt->second;
        }

        spawns.push_back(makeRuntimeUnitSpawn(
            makeRuntimeUnit(setup),
            std::move(combo),
            std::move(actionPlan)));
    }
    return spawns;
}
```

- [ ] **Step 5: Add runtime construction from spawns**

Add:

```cpp
BattleRuntimeState buildRuntimeFromSpawns(
    const BattleRuntimeSessionCreationInput& input,
    std::vector<BattleRuntimeUnitSpawn> spawns)
{
    BattleRuntimeState runtime;
    runtime.unitStore.gridTransform = input.rules.gridTransform;
    runtime.random = BattleRuntimeRandom(input.randomSeed);
    runtime.unitStore.units.reserve(spawns.size());
    runtime.status.units.reserve(spawns.size());
    runtime.damage.unitExtras.reserve(spawns.size());

    for (auto& spawn : spawns)
    {
        appendRuntimeUnit(runtime, std::move(spawn));
    }
    return runtime;
}
```

- [ ] **Step 6: Restrict `deriveRuntimeStores` to global runtime configuration**

In `deriveRuntimeStores(...)`, delete these unit-row rebuild blocks:

```cpp
    const auto existingMovement = runtime.movement;
```

```cpp
    runtime.movement.agents.clear();
    for (const auto& unit : runtime.unitStore.units)
    {
        if (!unit.alive)
        {
            continue;
        }
        runtime.movement.agents.emplace(
            unit.id,
            makeInitializedMovementAgent(unit.id, existingMovement, unit));
    }
```

```cpp
    runtime.action.planSeeds.clear();
    for (const auto& seed : input.actionPlanSeeds)
    {
        runtime.action.planSeeds.emplace(seed.unitId, seed);
    }
    for (const auto& unit : runtime.unitStore.units)
    {
        if (unit.cloneSourceUnitId < 0)
        {
            continue;
        }
        const auto sourceSeedIt = runtime.action.planSeeds.find(unit.cloneSourceUnitId);
        if (sourceSeedIt == runtime.action.planSeeds.end())
        {
            continue;
        }
        auto cloneSeed = sourceSeedIt->second;
        cloneSeed.unitId = unit.id;
        runtime.action.planSeeds.emplace(unit.id, std::move(cloneSeed));
    }
```

```cpp
    runtime.damage.unitExtras.clear();
    runtime.damage.presentationStylesByDefender.clear();
    runtime.damage.unitEffects.clear();
    for (const auto& unit : runtime.unitStore.units)
    {
        const auto stateIt = runtime.combo.units.find(unit.id);
        runtime.damage.unitExtras.push_back(makeInitialDamageRuntimeUnit(
            unit,
            stateIt != runtime.combo.units.end() ? &stateIt->second : nullptr));
        runtime.damage.presentationStylesByDefender.emplace(unit.id, makeDamagePresentationStyle(unit.team));
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            runtime.damage.unitEffects.emplace(
                unit.id,
                BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }
```

Then add the death-AOE effect population back as a narrow global pass:

```cpp
    runtime.damage.unitEffects.clear();
    for (const auto& unit : runtime.unitStore.units)
    {
        const auto stateIt = runtime.combo.units.find(unit.id);
        if (stateIt != runtime.combo.units.end() && stateIt->second.deathAOEPct > 0)
        {
            runtime.damage.unitEffects.emplace(
                unit.id,
                BattleDamageApplicationUnitEffects{
                    stateIt->second.deathAOEPct,
                    stateIt->second.deathAOEStunFrames,
                    stateIt->second.deathAOEMaxTargets,
                });
        }
    }
```

- [ ] **Step 7: Temporarily keep setup flow compiling**

In `setupBattleRuntime(...)`, replace:

```cpp
    auto runtime = buildCanonicalRuntime(input);
    runtime.movement.frame = input.battleFrame;
    auto initialization = BattleInitializationSystem().initialize(runtime, input.setup);
    deriveRuntimeStores(runtime, std::move(input));
```

with:

```cpp
    auto spawns = buildCanonicalSpawns(input);
    auto runtime = buildRuntimeFromSpawns(input, std::move(spawns));
    runtime.movement.frame = input.battleFrame;
    auto initialization = BattleInitializationSystem().initialize(runtime, input.setup);
    deriveRuntimeStores(runtime, std::move(input));
```

This step still lets `BattleInitializationSystem` mutate runtime. The next task moves initialization before append.

- [ ] **Step 8: Build**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .github\build-command.ps1
```

Expected: compile succeeds. The new clone test may still fail until Task 4.

- [ ] **Step 9: Commit**

Run:

```powershell
git add src\battle\BattleRuntimeSession.cpp
git commit -m "refactor: build runtime session rows from spawn packets"
```

---

## Task 4: Move Initialization Before Runtime Append

**Files:**
- Modify: `src/battle/BattleInitialization.h`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleRuntimeSession.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Change the initialization API**

In `src/battle/BattleInitialization.h`, include the spawn header:

```cpp
#include "BattleRuntimeUnitSpawn.h"
```

Add:

```cpp
struct BattleInitializationContext
{
    BattleGridTransform gridTransform;
    int frame{};
};
```

Change the class API from:

```cpp
    BattleInitializationResult initialize(BattleRuntimeState& runtime,
                                          const BattleRuntimeSetupSeed& setup) const;
```

to:

```cpp
    BattleInitializationResult initialize(std::vector<BattleRuntimeUnitSpawn>& spawns,
                                          const BattleRuntimeSetupSeed& setup,
                                          const BattleInitializationContext& context) const;
```

- [ ] **Step 2: Add spawn lookup helpers**

In the anonymous namespace of `src/battle/BattleInitialization.cpp`, add:

```cpp
BattleRuntimeUnitSpawn& requireSpawnByUnitId(
    std::vector<BattleRuntimeUnitSpawn>& spawns,
    int unitId)
{
    auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn)
        {
            return spawn.unit.id == unitId;
        });
    assert(it != spawns.end());
    return *it;
}

const BattleRuntimeUnitSpawn& requireSpawnByUnitId(
    const std::vector<BattleRuntimeUnitSpawn>& spawns,
    int unitId)
{
    auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn)
        {
            return spawn.unit.id == unitId;
        });
    assert(it != spawns.end());
    return *it;
}

std::map<int, RoleComboState> comboMapFromSpawns(
    const std::vector<BattleRuntimeUnitSpawn>& spawns)
{
    std::map<int, RoleComboState> combos;
    for (const auto& spawn : spawns)
    {
        combos.emplace(spawn.unit.id, spawn.combo);
    }
    return combos;
}
```

- [ ] **Step 3: Replace direct runtime access in normal initialization**

Change the function definition to:

```cpp
BattleInitializationResult BattleInitializationSystem::initialize(
    std::vector<BattleRuntimeUnitSpawn>& spawns,
    const BattleRuntimeSetupSeed& setup,
    const BattleInitializationContext& context) const
```

Inside the first `for (const auto& seed : setup.units)` loop, replace:

```cpp
        auto& unit = runtime.unitStore.requireUnit(seed.unitId);
        auto& status = requireById(runtime.status.units, seed.unitId);
```

with:

```cpp
        auto& spawn = requireSpawnByUnitId(spawns, seed.unitId);
        auto& unit = spawn.unit;
```

Replace:

```cpp
        status.effects.damageImmunityAfterFrames = combo.damageImmunityAfterFrames;
        status.effects.damageImmunityDuration = combo.damageImmunityDuration;
        status.effects.damageImmunityTimer = combo.damageImmunityTimer;
        status.effects.freezeReductionPct = combo.freezeReductionPct;
        status.effects.shieldFreezeResPct = combo.shieldFreezeResPct;
        status.effects.controlImmunityFrames = combo.controlImmunityFrames;

        runtime.combo.units[seed.unitId] = combo;
```

with:

```cpp
        spawn.combo = std::move(combo);
        refreshRuntimeUnitSpawnStores(spawn);
```

Replace log frame reads:

```cpp
runtime.movement.frame
```

with:

```cpp
context.frame
```

- [ ] **Step 4: Replace combo map reads**

Before computing team shields, add:

```cpp
    auto comboStates = comboMapFromSpawns(spawns);
```

Replace reads of:

```cpp
runtime.combo.units
```

that only need current combo state with:

```cpp
comboStates
```

When a later change mutates a spawn combo, update `comboStates[unitId]` immediately after `refreshRuntimeUnitSpawnStores(spawn)`.

- [ ] **Step 5: Apply team flat shield to spawn packets**

Replace:

```cpp
        auto& unit = runtime.unitStore.requireUnit(seed.unitId);
        auto comboIt = runtime.combo.units.find(seed.unitId);
        assert(comboIt != runtime.combo.units.end());

        comboIt->second.shield += teamShield;
        unit.shield = comboIt->second.shield;
```

with:

```cpp
        auto& spawn = requireSpawnByUnitId(spawns, seed.unitId);
        spawn.combo.shield += teamShield;
        comboStates[seed.unitId] = spawn.combo;
        refreshRuntimeUnitSpawnStores(spawn);
```

- [ ] **Step 6: Replace clone construction with source spawn cloning**

Delete `cloneStatusUnit(...)`.

Replace `makeCloneRuntimeUnit(...)` with:

```cpp
BattleRuntimeUnit makeCloneRuntimeUnit(
    const BattleRuntimeUnit& sourceUnit,
    int cloneUnitId,
    const BattleGridTransform& gridTransform,
    const BattleInitializationCloneSpawnCell& cell)
{
    auto clone = sourceUnit;
    clone.id = cloneUnitId;
    clone.cloneSourceUnitId = sourceUnit.id;
    clone.grid = { cell.x, cell.y, 0 };
    clone.motion.position = positionForCloneCell(gridTransform, cell.x, cell.y);
    clone.motion.velocity = sourceUnit.motion.velocity;
    clone.motion.acceleration = sourceUnit.motion.acceleration;
    clone.animation = BattleUnitAnimationState{};
    clone.haveAction = false;
    clone.operationType = BattleOperationType::None;
    clone.operationCount = 0;
    clone.vitals.hp = clone.vitals.maxHp;
    clone.alive = true;
    clone.weaponId = -1;
    clone.armorId = -1;
    clone.chessInstanceId = -1;
    return clone;
}
```

In the clone loop, replace source runtime reads:

```cpp
            const auto& sourceUnit = runtime.unitStore.requireUnit(source.sourceUnitId);
            const auto& sourceStatus = requireById(runtime.status.units, source.sourceUnitId);
            const auto sourceComboIt = runtime.combo.units.find(source.sourceUnitId);
            assert(sourceComboIt != runtime.combo.units.end());
```

with:

```cpp
            const auto& sourceSpawn = requireSpawnByUnitId(spawns, source.sourceUnitId);
            const auto& sourceUnit = sourceSpawn.unit;
```

Replace:

```cpp
                sourceComboIt->second,
```

with:

```cpp
                sourceSpawn.combo,
```

Replace direct runtime pushes:

```cpp
            runtime.unitStore.units.push_back(cloneUnit);
            runtime.status.units.push_back(cloneStatusUnit(sourceStatus, nextRuntimeUnitId, cloneCombo));
            runtime.combo.units[nextRuntimeUnitId] = cloneCombo;

            BattleMovementAgentState cloneAgent;
            if (const auto sourceAgent = runtime.movement.agents.find(source.sourceUnitId);
                sourceAgent != runtime.movement.agents.end())
            {
                cloneAgent = sourceAgent->second;
            }
            cloneAgent.physics.position = cloneUnit.motion.position;
            cloneAgent.physics.velocity = cloneUnit.motion.velocity;
            cloneAgent.physics.acceleration = cloneUnit.motion.acceleration;
            runtime.movement.agents.emplace(nextRuntimeUnitId, cloneAgent);
```

with:

```cpp
            auto cloneActionPlan = sourceSpawn.actionPlan;
            if (cloneActionPlan)
            {
                cloneActionPlan->unitId = nextRuntimeUnitId;
            }
            auto cloneSpawn = makeRuntimeUnitSpawn(
                std::move(cloneUnit),
                std::move(cloneCombo),
                std::move(cloneActionPlan));
            spawns.push_back(std::move(cloneSpawn));
            const auto& clone = spawns.back().unit;
```

Use `clone.star`, `clone.vitals`, and `clone.stats` for `result.roleDeltas`.

- [ ] **Step 7: Update session setup order**

In `src/battle/BattleRuntimeSession.cpp`, replace the temporary Task 3 setup flow:

```cpp
    auto spawns = buildCanonicalSpawns(input);
    auto runtime = buildRuntimeFromSpawns(input, std::move(spawns));
    runtime.movement.frame = input.battleFrame;
    auto initialization = BattleInitializationSystem().initialize(runtime, input.setup);
    deriveRuntimeStores(runtime, std::move(input));
```

with:

```cpp
    auto spawns = buildCanonicalSpawns(input);
    auto initialization = BattleInitializationSystem().initialize(
        spawns,
        input.setup,
        BattleInitializationContext{
            input.rules.gridTransform,
            input.battleFrame,
        });
    auto runtime = buildRuntimeFromSpawns(input, std::move(spawns));
    runtime.movement.frame = input.battleFrame;
    deriveRuntimeStores(runtime, std::move(input));
```

- [ ] **Step 8: Update direct initialization tests**

Tests that currently construct `BattleRuntimeState runtime;` and call `initialize(runtime, setup)` must create spawn packets instead. Replace each direct pattern:

```cpp
BattleRuntimeState runtime;
runtime.unitStore.gridTransform = { 36.0, 18 };
runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
runtime.status.units.push_back(statusRuntime(0, 100, 20));

auto result = BattleInitializationSystem().initialize(runtime, setup);
```

with:

```cpp
std::vector<BattleRuntimeUnitSpawn> spawns;
spawns.push_back(makeRuntimeUnitSpawn(runtimeUnit(0, 0, 100, 20, 30, 40), RoleComboState{}));

auto result = BattleInitializationSystem().initialize(
    spawns,
    setup,
    BattleInitializationContext{ { 36.0, 18 }, 0 });
```

When the test needs a runtime after initialization, build one with:

```cpp
BattleRuntimeState runtime;
runtime.unitStore.gridTransform = { 36.0, 18 };
for (auto& spawn : spawns)
{
    appendRuntimeUnit(runtime, std::move(spawn));
}
```

- [ ] **Step 9: Run focused tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][initialization]"
```

Expected: initialization tests pass, including `BattleRuntimeSession_CloneUsesFreshSpawnStores`.

- [ ] **Step 10: Commit**

Run:

```powershell
git add src\battle\BattleInitialization.h src\battle\BattleInitialization.cpp src\battle\BattleRuntimeSession.cpp tests\BattleInitializationUnitTests.cpp
git commit -m "refactor: initialize battle units as spawn packets"
```

---

## Task 5: Move First-Hit Block Ownership To Damage Runtime

**Files:**
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleUnitStore.h`
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleHitResolverUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Initialize damage counter from combo config**

In `src/battle/BattleRuntimeUnitSpawn.cpp`, change:

```cpp
    damage.blockFirstHitsRemaining = combo.blockFirstHitsRemaining;
```

to:

```cpp
    damage.blockFirstHitsRemaining = combo.blockFirstHitsCount;
```

- [ ] **Step 2: Remove duplicate first-hit fields**

In `src/battle/BattleUnitStore.h`, delete:

```cpp
    int blockFirstHitsRemaining = 0;
```

In `src/ChessBattleEffects.h`, delete:

```cpp
    int blockFirstHitsRemaining = 0;
```

In `src/ChessBattleEffects.cpp`, delete:

```cpp
    cloneState.blockFirstHitsRemaining = cloneState.blockFirstHitsCount;
```

In `src/battle/BattleInitialization.cpp`, delete:

```cpp
        combo.blockFirstHitsRemaining = combo.blockFirstHitsCount;
```

- [ ] **Step 3: Update initialization assertions**

In `tests/BattleInitializationUnitTests.cpp`, replace:

```cpp
    CHECK(initialized.blockFirstHitsRemaining == 2);
```

with a runtime damage assertion after appending spawns:

```cpp
    CHECK(requireById(runtime.damage.unitExtras, 0).blockFirstHitsRemaining == 2);
```

Delete clone setup and assertions that write/read `BattleRuntimeUnit::blockFirstHitsRemaining`:

```cpp
    source.blockFirstHitsRemaining = 5;
```

and:

```cpp
    CHECK(clone->blockFirstHitsRemaining == 0);
```

- [ ] **Step 4: Update hit resolver test that used combo as fake damage owner**

In `tests/BattleHitResolverUnitTests.cpp`, replace:

```cpp
    input.defenderCombo.blockFirstHitsRemaining = 1;
```

with no setup line. Replace:

```cpp
    CHECK(result.defenderCombo.blockFirstHitsRemaining == 1);
```

with:

```cpp
    CHECK(result.finalHpDamage == 50);
```

This test is specifically proving hit resolution does not consume first-hit blocks; the damage system owns that counter.

- [ ] **Step 5: Update scene protection display**

In `src/BattleSceneHades.cpp`, replace the block around:

```cpp
        bool hasDamageProtection = unit.invincible > 0 || unit.blockFirstHitsRemaining > 0;
        if (hasDamageProtection)
        {
            Color protectionColor = unit.blockFirstHitsRemaining > 0 ? Color{ 255, 220, 110, 255 } : Color{ 255, 170, 95, 255 };
```

with:

```cpp
        const auto* damageRuntime = battle_session_
            ? tryFindBy(battle_session_->runtime().damage.unitExtras, unit.id, &KysChess::Battle::BattleDamageRuntimeUnit::id)
            : nullptr;
        const bool hasFirstHitBlock = damageRuntime && damageRuntime->blockFirstHitsRemaining > 0;
        bool hasDamageProtection = unit.invincible > 0 || hasFirstHitBlock;
        if (hasDamageProtection)
        {
            Color protectionColor = hasFirstHitBlock ? Color{ 255, 220, 110, 255 } : Color{ 255, 170, 95, 255 };
```

If `tryFindBy` is not visible in this file, include:

```cpp
#include "Find.h"
```

- [ ] **Step 6: Run ownership grep**

Run:

```powershell
rg -n "blockFirstHitsRemaining" src tests
```

Expected remaining owners:
- `BattleDamageUnitState`
- `BattleDamageRuntimeUnit`
- damage system logic/tests
- tests that explicitly inspect `runtime.damage.unitExtras`

Expected absent:
- `BattleRuntimeUnit::blockFirstHitsRemaining`
- `RoleComboState::blockFirstHitsRemaining`
- clone initialization reset code for this field

- [ ] **Step 7: Run tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][initialization],[battle][damage],[battle][hit_resolver]"
```

Expected: all selected tests pass.

- [ ] **Step 8: Commit**

Run:

```powershell
git add src\battle\BattleRuntimeUnitSpawn.cpp src\battle\BattleUnitStore.h src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle\BattleInitialization.cpp src\BattleSceneHades.cpp tests\BattleInitializationUnitTests.cpp tests\BattleHitResolverUnitTests.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: make damage runtime own first hit blocks"
```

---

## Task 6: Move MP Block To Status And MP Recovery Bonus To Combo

**Files:**
- Modify: `src/battle/BattleUnitStore.h`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `src/battle/BattleTeamEffectSystem.h`
- Modify: `src/battle/BattleTeamEffectSystem.cpp`
- Modify: `src/battle/BattleDamageSystem.h`
- Modify: `src/battle/BattleDamageSystem.cpp`
- Modify: tests that reference `mpBlocked` or `mpRecoveryBonusPct` on `BattleRuntimeUnit`

- [ ] **Step 1: Remove unit mirrors**

In `src/battle/BattleUnitStore.h`, delete:

```cpp
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
```

In `src/battle/BattleRuntimeUnitSpawn.cpp`, delete:

```cpp
    spawn.unit.mpBlocked = spawn.combo.mpBlockTimer > 0;
    spawn.unit.mpRecoveryBonusPct = spawn.combo.mpRecoveryBonusPct;
```

In `src/battle/BattleStatusSystem.cpp`, change:

```cpp
    void syncMpBlocked() { unit.mpBlocked = status.effects.mpBlockTimer > 0; }
```

to:

```cpp
    void syncMpBlocked() {}
```

- [ ] **Step 2: Add runtime MP restore helper**

In the anonymous namespace of `src/battle/BattleCore.cpp`, add:

```cpp
bool isMpBlocked(const BattleRuntimeState& state, int unitId)
{
    const auto& status = requireById(state.status.units, unitId);
    return status.effects.mpBlockTimer > 0;
}

int mpRecoveryBonusPct(const BattleRuntimeState& state, int unitId)
{
    const auto comboIt = state.combo.units.find(unitId);
    return comboIt != state.combo.units.end() ? comboIt->second.mpRecoveryBonusPct : 0;
}

int adjustedRuntimeMpRestore(const BattleRuntimeState& state, int unitId, int amount)
{
    return adjustedMpRestore(
        isMpBlocked(state, unitId),
        mpRecoveryBonusPct(state, unitId),
        amount);
}
```

- [ ] **Step 3: Update runtime MP delta**

Change `applyRuntimeUnitMpDelta(...)` from:

```cpp
void applyRuntimeUnitMpDelta(BattleRuntimeUnit& unit, int mpDelta)
{
    if (mpDelta == 0)
    {
        return;
    }
    if (mpDelta > 0)
    {
        if (unit.mpBlocked)
        {
            return;
        }
        unit.vitals.mp += mpDelta * (100 + unit.mpRecoveryBonusPct) / 100;
    }
    else
    {
        unit.vitals.mp += mpDelta;
    }
    unit.vitals.mp = std::clamp(unit.vitals.mp, 0, unit.vitals.maxMp);
}
```

to:

```cpp
void applyRuntimeUnitMpDelta(BattleRuntimeState& state, BattleRuntimeUnit& unit, int mpDelta)
{
    if (mpDelta == 0)
    {
        return;
    }
    if (mpDelta > 0)
    {
        unit.vitals.mp += adjustedRuntimeMpRestore(state, unit.id, mpDelta);
    }
    else
    {
        unit.vitals.mp += mpDelta;
    }
    unit.vitals.mp = std::clamp(unit.vitals.mp, 0, unit.vitals.maxMp);
}
```

Replace calls:

```cpp
applyRuntimeUnitMpDelta(unit, tick.mpDelta);
```

with:

```cpp
applyRuntimeUnitMpDelta(state, unit, tick.mpDelta);
```

- [ ] **Step 4: Update team MP effects to receive status/combo stores**

In `src/battle/BattleTeamEffectSystem.h`, change:

```cpp
    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleUnitStore& units,
                                                   int sourceUnitId,
                                                   int amount) const;
```

to:

```cpp
    std::vector<BattleTeamEffectEvent> applyTeamMp(BattleUnitStore& units,
                                                   const std::vector<BattleStatusRuntimeUnit>& statuses,
                                                   const std::map<int, RoleComboState>& combos,
                                                   int sourceUnitId,
                                                   int amount) const;
```

Add includes:

```cpp
#include "../ChessBattleEffects.h"
#include "BattleStatusSystem.h"

#include <map>
```

In `src/battle/BattleTeamEffectSystem.cpp`, replace:

```cpp
std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamMp(BattleUnitStore& units,
                                                                       int sourceUnitId,
                                                                       int amount) const
```

with:

```cpp
std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamMp(
    BattleUnitStore& units,
    const std::vector<BattleStatusRuntimeUnit>& statuses,
    const std::map<int, RoleComboState>& combos,
    int sourceUnitId,
    int amount) const
```

Replace:

```cpp
        int restore = adjustedMpRestore(unit.mpBlocked, unit.mpRecoveryBonusPct, amount);
```

with:

```cpp
        const auto& status = requireById(statuses, unit.id);
        const auto comboIt = combos.find(unit.id);
        const int recoveryBonus = comboIt != combos.end() ? comboIt->second.mpRecoveryBonusPct : 0;
        int restore = adjustedMpRestore(status.effects.mpBlockTimer > 0, recoveryBonus, amount);
```

Include `Find.h` in the `.cpp` if needed:

```cpp
#include "../Find.h"
```

- [ ] **Step 5: Update call sites**

In `src/battle/BattleCore.cpp`, replace calls shaped like:

```cpp
teamEffectSystem.applyTeamMp(state.unitStore, sourceUnitId, amount)
```

with:

```cpp
teamEffectSystem.applyTeamMp(
    state.unitStore,
    state.status.units,
    state.combo.units,
    sourceUnitId,
    amount)
```

- [ ] **Step 6: Update damage snapshots**

In `makeBattleDamageUnitStateFromRuntime(...)`, remove:

```cpp
    damage.mpBlocked = unit.mpBlocked;
    damage.mpRecoveryBonusPct = unit.mpRecoveryBonusPct;
```

At the call site that builds attacker/defender damage states for hit transactions, set these DTO fields from authoritative owners:

```cpp
    transaction.attacker.mpBlocked = isMpBlocked(state, transaction.attacker.id);
    transaction.attacker.mpRecoveryBonusPct = mpRecoveryBonusPct(state, transaction.attacker.id);
    transaction.defender.mpBlocked = isMpBlocked(state, transaction.defender.id);
    transaction.defender.mpRecoveryBonusPct = mpRecoveryBonusPct(state, transaction.defender.id);
```

- [ ] **Step 7: Run ownership grep**

Run:

```powershell
rg -n "mpBlocked|mpRecoveryBonusPct" src tests
```

Expected absent:
- `BattleRuntimeUnit::mpBlocked`
- `BattleRuntimeUnit::mpRecoveryBonusPct`
- code that writes these fields to units

Expected remaining:
- `BattleDamageUnitState` DTO fields
- `RoleComboState::mpRecoveryBonusPct`
- status mp-block timer
- resource-rule tests

- [ ] **Step 8: Run tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][core],[battle][damage],[battle][team_effect],[battle][initialization]"
```

Expected: all selected tests pass after updating test setup to write status/combo owners instead of unit mirrors.

- [ ] **Step 9: Commit**

Run:

```powershell
git add src\battle\BattleUnitStore.h src\battle\BattleRuntimeUnitSpawn.cpp src\battle\BattleStatusSystem.cpp src\battle\BattleCore.cpp src\battle\BattleTeamEffectSystem.h src\battle\BattleTeamEffectSystem.cpp src\battle\BattleDamageSystem.h src\battle\BattleDamageSystem.cpp tests
git commit -m "refactor: read mp resource modifiers from status and combo"
```

---

## Task 7: Move Current Shield Ownership To BattleRuntimeUnit

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`
- Modify: `tests/BattleCoreUnitTests.cpp`

- [ ] **Step 1: Compute initial shield in spawn module**

In `src/battle/BattleRuntimeUnitSpawn.cpp`, add:

```cpp
int initialShieldFor(const BattleRuntimeUnit& unit, const RoleComboState& combo)
{
    int shield = combo.flatShield;
    if (combo.shieldPctMaxHP > 0)
    {
        shield += unit.vitals.maxHp * combo.shieldPctMaxHP / 100;
    }
    return shield;
}
```

Then change:

```cpp
    spawn.unit.shield = spawn.combo.shield;
```

to:

```cpp
    spawn.unit.shield = initialShieldFor(spawn.unit, spawn.combo);
```

- [ ] **Step 2: Remove shield from combo state**

In `src/ChessBattleEffects.h`, delete:

```cpp
    int shield = 0;
```

In `src/ChessBattleEffects.cpp`, delete the shield calculation block from `makeSummonedCloneState(...)`:

```cpp
    if (cloneState.shieldPctMaxHP > 0 && cloneMaxHP > 0)
    {
        cloneState.shield = cloneMaxHP * cloneState.shieldPctMaxHP / 100;
    }
```

- [ ] **Step 3: Stop writing combo shield in initialization**

In `src/battle/BattleInitialization.cpp`, replace:

```cpp
            combo.shield = unit.vitals.maxHp * combo.shieldPctMaxHP / 100;
```

with:

```cpp
            const int shield = unit.vitals.maxHp * combo.shieldPctMaxHP / 100;
```

Then replace log uses of `combo.shield` in that block with `shield`.

For team flat shield, replace:

```cpp
        spawn.combo.shield += teamShield;
        comboStates[seed.unitId] = spawn.combo;
        refreshRuntimeUnitSpawnStores(spawn);
```

with:

```cpp
        comboStates[seed.unitId] = spawn.combo;
        refreshRuntimeUnitSpawnStores(spawn);
        spawn.unit.shield += teamShield;
```

The invariant is that `refreshRuntimeUnitSpawnStores(...)` computes combo-derived initial shield; team flat shield is a post-combo team bonus applied to the unit owner.

- [ ] **Step 4: Update tests**

In `tests/BattleInitializationUnitTests.cpp`, replace:

```cpp
    CHECK(initialized.shield == 65);
```

with:

```cpp
    CHECK(unit.shield == 65);
```

In `tests/BattleCoreUnitTests.cpp`, replace assertions that use `state.combo.units.at(id).shield` as a current shield owner with assertions against `state.unitStore.requireUnit(id).shield`.

- [ ] **Step 5: Run ownership grep**

Run:

```powershell
rg -n "\.shield" src\ChessBattleEffects.* src\battle tests
```

Expected absent:
- `RoleComboState::shield`
- `combo.shield` as current shield

Expected remaining:
- `BattleRuntimeUnit::shield`
- damage DTO shield snapshots
- shield percentage/config fields such as `shieldPctMaxHP`

- [ ] **Step 6: Run tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][initialization],[battle][core],[battle][damage],[battle][death_effect]"
```

Expected: selected tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle\BattleRuntimeUnitSpawn.cpp src\battle\BattleInitialization.cpp src\battle\BattleCore.cpp tests\BattleInitializationUnitTests.cpp tests\BattleCoreUnitTests.cpp
git commit -m "refactor: make runtime unit own current shield"
```

---

## Task 8: Remove Status Timer Duplicates From RoleComboState

**Files:**
- Modify: `src/ChessBattleEffects.h`
- Modify: `src/ChessBattleEffects.cpp`
- Modify: `src/battle/BattleStatusSystem.cpp`
- Modify: `src/battle/BattleRuntimeUnitSpawn.cpp`
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `src/battle/BattleCore.cpp`
- Modify: tests that write status timers through combo

- [ ] **Step 1: Add initial status effect construction from combo config**

In `src/battle/BattleStatusSystem.cpp`, replace `makeBattleStatusUnitState(const BattleRuntimeUnit& unit, const RoleComboState& state)` with a version that does not read mutable status timer fields from `RoleComboState`:

```cpp
BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit, const KysChess::RoleComboState& state)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    status.attack = unit.stats.attack;
    status.invincible = unit.invincible;
    status.effects.freezeReductionPct = state.freezeReductionPct;
    status.effects.shieldFreezeResPct = state.shieldFreezeResPct;
    status.effects.controlImmunityFrames = state.controlImmunityFrames;
    status.effects.damageImmunityAfterFrames = state.damageImmunityAfterFrames;
    status.effects.damageImmunityDuration = state.damageImmunityDuration;
    if (state.damageImmunityAfterFrames > 0)
    {
        status.effects.damageImmunityTimer = state.damageImmunityAfterFrames;
    }
    return status;
}
```

- [ ] **Step 2: Remove status mutable fields from combo**

In `src/ChessBattleEffects.h`, delete:

```cpp
    int poisonTimer = 0;
    int poisonTickDmg = 0;
    int poisonSourceId = -1;
    int bleedStacks = 0;
    int bleedTimer = 0;
    int bleedSourceId = -1;
    int mpBlockTimer = 0;
    struct TempAttackBuffInstance { int attackBonus = 0; int remainingFrames = 0; };
    std::vector<TempAttackBuffInstance> tempAttackBuffs;
    int damageImmunityTimer = 0;
    struct DmgReduceDebuffInstance { int remainingFrames = 0; int pct = 0; };
    std::vector<DmgReduceDebuffInstance> dmgReduceDebuffs;
```

In `src/ChessBattleEffects.cpp`, delete:

```cpp
        cloneState.damageImmunityTimer = cloneState.damageImmunityAfterFrames;
```

- [ ] **Step 3: Remove initialization writes to combo status timers**

In `src/battle/BattleInitialization.cpp`, delete:

```cpp
            combo.damageImmunityTimer = combo.damageImmunityAfterFrames;
```

Status spawn construction now initializes `damageImmunityTimer` from `damageImmunityAfterFrames`.

- [ ] **Step 4: Update modifier reads to status owner**

In `src/battle/BattleCore.cpp`, replace code that copies poison/damage-reduce state from combo with reads from `BattleStatusRuntimeUnit`.

Replace:

```cpp
    modifier.poisonTimer = status->effects.poisonTimer;
```

only where `status` is already a `BattleStatusRuntimeUnit*`; do not read poison from combo.

For tests in `tests/BattleCoreUnitTests.cpp` that assign:

```cpp
combo.poisonTimer = transaction.attackerModifiers.poisonTimer;
combo.dmgReduceDebuffs.clear();
```

replace the setup with status runtime writes:

```cpp
auto& status = requireById(state.status.units, unitId);
status.effects.poisonTimer = transaction.attackerModifiers.poisonTimer;
status.effects.damageReduceDebuffs.clear();
```

- [ ] **Step 5: Run ownership grep**

Run:

```powershell
rg -n "poisonTimer|bleedStacks|bleedTimer|mpBlockTimer|damageImmunityTimer|tempAttackBuffs|dmgReduceDebuffs" src\ChessBattleEffects.* src\battle tests
```

Expected absent:
- status timer fields in `RoleComboState`
- clone state status timer initialization

Expected remaining:
- `BattleStatusEffectState`
- damage/status systems and DTO tests

- [ ] **Step 6: Run tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][status],[battle][damage],[battle][core],[battle][initialization]"
```

Expected: selected tests pass.

- [ ] **Step 7: Commit**

Run:

```powershell
git add src\ChessBattleEffects.h src\ChessBattleEffects.cpp src\battle\BattleStatusSystem.cpp src\battle\BattleRuntimeUnitSpawn.cpp src\battle\BattleInitialization.cpp src\battle\BattleCore.cpp tests
git commit -m "refactor: make status runtime own status timers"
```

---

## Task 9: Remove Clone-Specific Initialization Branches

**Files:**
- Modify: `src/battle/BattleInitialization.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Simplify clone unit creation to template overrides**

After Tasks 5-8, `makeCloneRuntimeUnit(...)` should not reset fields that are no longer on `BattleRuntimeUnit`. Confirm the function only contains:

```cpp
BattleRuntimeUnit makeCloneRuntimeUnit(
    const BattleRuntimeUnit& sourceUnit,
    int cloneUnitId,
    const BattleGridTransform& gridTransform,
    const BattleInitializationCloneSpawnCell& cell)
{
    auto clone = sourceUnit;
    clone.id = cloneUnitId;
    clone.cloneSourceUnitId = sourceUnit.id;
    clone.grid = { cell.x, cell.y, 0 };
    clone.motion.position = positionForCloneCell(gridTransform, cell.x, cell.y);
    clone.animation = BattleUnitAnimationState{};
    clone.haveAction = false;
    clone.operationType = BattleOperationType::None;
    clone.operationCount = 0;
    clone.vitals.hp = clone.vitals.maxHp;
    clone.alive = true;
    clone.weaponId = -1;
    clone.armorId = -1;
    clone.chessInstanceId = -1;
    return clone;
}
```

Do not add resets for status, damage, combo, movement, or action plan fields here. Those stores are initialized by `makeRuntimeUnitSpawn(...)`.

- [ ] **Step 2: Ensure clone loop has no direct store writes**

Run:

```powershell
rg -n "runtime\.(unitStore|status|combo|movement|damage|action)" src\battle\BattleInitialization.cpp
```

Expected: no direct runtime store writes from clone initialization. Initialization should operate on `spawns`, not `runtime`.

- [ ] **Step 3: Remove obsolete direct-runtime initialization tests**

In `tests/BattleInitializationUnitTests.cpp`, delete or rewrite tests whose only purpose was to prove `BattleInitializationSystem` could mutate an externally prepared `BattleRuntimeState`. Keep tests that assert behavior through spawn packets or `BattleRuntimeSession::createInitialized(...)`.

The test named:

```cpp
BattleInitializationSystem_CompilesPureRuntimeInitializationApi
```

should be deleted because the pure runtime mutation API no longer exists.

- [ ] **Step 4: Run initialization tests**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][initialization]"
```

Expected: all initialization tests pass.

- [ ] **Step 5: Commit**

Run:

```powershell
git add src\battle\BattleInitialization.cpp tests\BattleInitializationUnitTests.cpp
git commit -m "refactor: reduce clone initialization to spawn overrides"
```

---

## Task 10: Final Verification And Line-Reduction Check

**Files:**
- Read-only verification across repo

- [ ] **Step 1: Verify no direct append bypasses remain**

Run:

```powershell
rg -n "unitStore\.units\.push_back|status\.units\.push_back|damage\.unitExtras\.push_back|movement\.agents\.emplace|combo\.units\[|action\.planSeeds\.emplace" src\battle
```

Expected:
- `appendRuntimeUnit(...)` is the only initializer path that appends unit/status/damage/movement/combo/action rows.
- Frame runtime code may still mutate existing stores, but it must not create initial rows except through the spawn path.

- [ ] **Step 2: Verify removed duplicate owners**

Run:

```powershell
rg -n "blockFirstHitsRemaining|mpBlocked|mpRecoveryBonusPct|poisonTimer|bleedStacks|mpBlockTimer|damageImmunityTimer|tempAttackBuffs|dmgReduceDebuffs|\.shield" src\battle src\ChessBattleEffects.* tests
```

Expected:
- `blockFirstHitsRemaining` appears only in damage DTO/runtime and tests that inspect damage.
- `mpBlocked` appears only in DTO/resource calculations, not `BattleRuntimeUnit`.
- `mpRecoveryBonusPct` appears in combo config and DTO/resource calculations, not `BattleRuntimeUnit`.
- status timers appear in status runtime/DTO code, not `RoleComboState`.
- current shield appears on `BattleRuntimeUnit` and damage DTOs, not `RoleComboState`.

- [ ] **Step 3: Measure initialization simplification**

Run:

```powershell
git diff --stat HEAD~9 -- src\battle\BattleInitialization.cpp src\battle\BattleRuntimeSession.cpp src\battle\BattleRuntimeUnitSpawn.*
```

Expected:
- `BattleInitialization.cpp` loses clone-specific status/movement append code and direct runtime mutation.
- `BattleRuntimeSession.cpp` loses duplicated row derivation loops for status/damage/movement/action.
- New shared spawn module contains the single row attachment path.

- [ ] **Step 4: Run focused test suites**

Run:

```powershell
.\build\tests\Debug\battle_tests.exe "[battle][initialization],[battle][core],[battle][damage],[battle][status],[battle][team_effect],[battle][hit_resolver]"
```

Expected: all selected tests pass.

- [ ] **Step 5: Run build**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .github\build-command.ps1
```

Expected: compile succeeds. If final linking fails because the game executable is running, treat compilation as successful per repo instructions.

- [ ] **Step 6: Commit verification cleanup**

If verification required small cleanup changes, commit them:

```powershell
git add src tests
git commit -m "test: verify authoritative battle spawn ownership"
```

If no files changed, do not create an empty commit.

---

## Self-Review

Spec coverage:
- Merged path: Tasks 2-4 introduce and route through `BattleRuntimeUnitSpawn`.
- Clone simplification: Tasks 4 and 9 reduce clone to source-template overrides plus shared spawn append.
- Authoritative stores: Tasks 5-8 move first-hit blocks, mp resource modifiers, shield, and status timers to single owners.
- Initialization line reduction: Task 10 explicitly verifies direct append deletion and diff stats.

Placeholder scan:
- No unresolved placeholders remain. Every task names concrete files, code snippets, and verification commands.

Type consistency:
- `BattleRuntimeUnitSpawn`, `makeRuntimeUnitSpawn`, `refreshRuntimeUnitSpawnStores`, and `appendRuntimeUnit` are defined before use.
- Initialization API uses `std::vector<BattleRuntimeUnitSpawn>&` consistently after Task 4.
- Resource ownership changes keep DTO fields for transaction snapshots while removing persistent unit/combo mirrors.
