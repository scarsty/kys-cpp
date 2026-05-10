# Battle Initialization Conversion Subtraction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the initialization back-and-forth conversion path by making legacy `Role`/`Magic` extraction one-way into canonical setup units, then building runtime and scene rows directly from those units and final runtime state.

**Architecture:** `BattleSceneHades` should collect encounter facts only: saved role ids, cells, team, stars, gear, clone cells, and map/rules facts. A focused setup builder owned by the scene/adapter boundary converts legacy `Role`/`Magic` data into canonical setup units; after that, runtime initialization and scene unit initialization consume those setup units directly. The adapter keeps temporary legacy extraction helpers, but deletes the post-initialization conversion back into `BattleSetupRoleSnapshot`.

**Tech Stack:** C++20, MSVC Debug x64, Catch2-style unit tests, existing `build-command.ps1` verification.

---

## File Structure

- Create `src/BattleSceneSetupBuilder.h`
  - Owns testable setup-building types and functions that currently live as private helpers in `BattleSceneHades.cpp`.
  - Converts `const Role&` plus explicit Hades setup overrides into canonical setup rows without mutating or copying `Role` as a staging object.

- Create `src/BattleSceneSetupBuilder.cpp`
  - Moves legacy helper logic from Hades: skill selection, skill snapshot projection, fight-frame loading, skill name formatting, initial facing and world-position calculation.
  - Uses `TextureManager` and `Save` where legacy data requires them.

- Modify `src/BattleSceneBattleAdapter.h`
  - Replace `BattleSetupRoleSnapshot` plus separate `allyRoster` / `enemyRoster` setup input with one canonical setup unit type.
  - Keep `BattleSetupSkillSnapshot` only if it remains the direct action-plan seed format; otherwise rename it under setup unit ownership.

- Modify `src/BattleSceneBattleAdapter.cpp`
  - Derive runtime setup seed, runtime init stores, setup configuration, and scene initialization from canonical setup units.
  - Delete `makeRuntimeSetupRoleSnapshot()` and `makeOrderedRuntimeRoleSnapshots()`.

- Modify `src/BattleSceneHades.h`
  - Remove `setup_role_snapshots_`, `setup_ally_roster_`, `setup_enemy_roster_`, `assignSetupUnitId()`, `realRoleIdForRole()`, `makeInitialRoleState()`, and `makeSetupRoleSnapshot()`.
  - Keep only Hades-owned setup facts and `BattleSceneUnitStore`.

- Modify `src/BattleSceneHades.cpp`
  - Replace temporary `std::deque<Role>` staging with setup requests passed to `BattleSceneSetupBuilder`.
  - Build runtime context from canonical setup units.

- Modify `src/BattleSceneUnitStore.h` and `src/BattleSceneUnitStore.cpp`
  - Change `initialize()` to accept `std::vector<BattleSceneUnit>` directly.
  - Remove adapter header dependency from the store.

- Create `tests/BattleSceneSetupBuilderUnitTests.cpp`
  - Covers the extracted setup builder without needing a live `BattleSceneHades`.

- Modify `tests/BattleInitializationUnitTests.cpp`
  - Update adapter/session initialization tests to canonical setup units.

- Modify `src/kys.vcxproj`, `src/kys.vcxproj.filters`, and `tests/kys_tests.vcxproj`
  - Add new source and test files.

---

## Canonical Setup Shape

Introduce one setup unit type that replaces the scene-side combination of `BattleSetupRoleSnapshot` and `BattleSetupRosterUnit`:

```cpp
namespace KysChess::BattleSceneBattleAdapter
{
struct BattleSetupUnitInput
{
    int unitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    int team = -1;
    int sourceOrder = 0;

    int gridX = 0;
    int gridY = 0;
    int faceTowards = 0;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf facing;
    std::array<int, 5> fightFrames{};

    int star = 1;
    int cost = 0;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;
    int fightsWon = 0;

    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int fist = 0;
    int sword = 0;
    int knife = 0;
    int unusual = 0;
    int hiddenWeapon = 0;

    int cooldown = 0;
    int cooldownMax = 0;
    int physicalPower = 0;
    int invincible = 0;
    int hurtFrame = 0;
    int frozen = 0;
    int frozenMax = 0;
    std::array<int, 5> actPropertiesByMagicType{};

    bool hasEquippedSkill = false;
    std::string skillNames;
    BattleSetupSkillSnapshot normalSkill;
    BattleSetupSkillSnapshot ultimateSkill;
};
}
```

`BattleRuntimeSceneSetupInput` should then hold:

```cpp
struct BattleRuntimeSceneSetupInput
{
    std::vector<BattleSetupUnitInput> units;
    std::map<int, RoleComboState>* comboStates = nullptr;
    std::vector<Battle::BattleTerrainCell> terrainCells;
    std::vector<int> obtainedNeigongMagicIds;
    std::vector<std::pair<int, int>> cloneSpawnCells;
    std::vector<Battle::BattleRescueCellSnapshot> rescueCells;
    std::map<std::pair<int, int>, Pointf> rescuePositionsByCell;
    int battleFrame = 0;
    int rescueCounterAttackSkillId = -1;
};
```

Runtime `allyRoster` and `enemyRoster` are derived inside `makeBattleRuntimeSetupSeed()` by filtering `units` by `team`, preserving `sourceOrder`.

---

## Task 1: Add Setup Builder Tests

**Files:**
- Create: `tests/BattleSceneSetupBuilderUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add the test file with source-role mutation protection**

Create `tests/BattleSceneSetupBuilderUnitTests.cpp` with tests shaped like this:

```cpp
#include "BattleSceneSetupBuilder.h"

#include "Save.h"
#include "catch.hpp"

namespace
{
Role makeSavedRole(int id)
{
    Role role;
    role.ID = id;
    role.Name = "測試角色";
    role.HeadID = 23;
    role.MaxHP = 100;
    role.MaxMP = 30;
    role.Attack = 20;
    role.Defence = 10;
    role.Speed = 40;
    role.Fist = 5;
    role.Sword = 6;
    role.Knife = 7;
    role.Unusual = 8;
    role.HiddenWeapon = 9;
    role.Cost = 3;
    role.Team = 99;
    role.Star = 1;
    role.FaceTowards = Towards_LeftUp;
    role.setPositionOnly(9, 9);
    return role;
}
}

TEST_CASE("BattleSceneSetupBuilder_BuildsSetupUnitWithoutMutatingSavedRole", "[battle][scene_setup]")
{
    Role source = makeSavedRole(1001);
    const int originalTeam = source.Team;
    const int originalStar = source.Star;
    const int originalX = source.X();
    const int originalY = source.Y();

    KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest request;
    request.unitId = 0;
    request.source = &source;
    request.team = 0;
    request.gridX = 2;
    request.gridY = 3;
    request.star = 4;
    request.faceTowardsFallback = Towards_RightDown;
    request.sourceOrder = 0;
    request.positionForCell = [](int x, int y)
    {
        return Pointf{ static_cast<float>(x * 10), static_cast<float>(y * 10), 0 };
    };

    auto unit = KysChess::BattleSceneSetupBuilder::makeSetupUnit(
        request,
        std::span<const KysChess::BattleSceneSetupBuilder::BattleSceneSetupOpponentCell>{});

    CHECK(unit.unitId == 0);
    CHECK(unit.realRoleId == 1001);
    CHECK(unit.team == 0);
    CHECK(unit.gridX == 2);
    CHECK(unit.gridY == 3);
    CHECK(unit.star == 4);
    CHECK(unit.hp == source.MaxHP);
    CHECK(unit.mp == 0);
    CHECK(unit.physicalPower >= 90);
    CHECK(source.Team == originalTeam);
    CHECK(source.Star == originalStar);
    CHECK(source.X() == originalX);
    CHECK(source.Y() == originalY);
}
```

- [ ] **Step 2: Add a dense-id build result test**

Append:

```cpp
TEST_CASE("BattleSceneSetupBuilder_AssignsDenseUnitsInProvidedOrder", "[battle][scene_setup]")
{
    Role first = makeSavedRole(1001);
    Role second = makeSavedRole(1002);

    std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest> requests;
    requests.push_back({ .unitId = 0, .source = &first, .team = 1, .gridX = 1, .gridY = 2, .star = 2, .faceTowardsFallback = Towards_RightDown, .sourceOrder = 0 });
    requests.push_back({ .unitId = 1, .source = &second, .team = 0, .gridX = 5, .gridY = 6, .star = 3, .faceTowardsFallback = Towards_LeftUp, .sourceOrder = 0 });
    for (auto& request : requests)
    {
        request.positionForCell = [](int x, int y)
        {
            return Pointf{ static_cast<float>(x), static_cast<float>(y), 0 };
        };
    }

    auto result = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests);

    REQUIRE(result.units.size() == 2);
    CHECK(result.units[0].unitId == 0);
    CHECK(result.units[1].unitId == 1);
}
```

- [ ] **Step 3: Add the test file to `tests/kys_tests.vcxproj`**

Add:

```xml
<ClCompile Include="BattleSceneSetupBuilderUnitTests.cpp" />
```

- [ ] **Step 4: Run the new tests and verify they fail to compile**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile failure because `BattleSceneSetupBuilder.h` does not exist.

---

## Task 2: Create the Setup Builder

**Files:**
- Create: `src/BattleSceneSetupBuilder.h`
- Create: `src/BattleSceneSetupBuilder.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Add public builder types**

Create `src/BattleSceneSetupBuilder.h`:

```cpp
#pragma once

#include "BattleSceneBattleAdapter.h"
#include "Point.h"
#include "Types.h"

#include <functional>
#include <span>
#include <vector>

namespace KysChess::BattleSceneSetupBuilder
{
struct BattleSceneSetupOpponentCell
{
    int team = -1;
    int x = 0;
    int y = 0;
};

struct BattleSceneSetupUnitRequest
{
    int unitId = -1;
    const Role* source = nullptr;
    int team = -1;
    int gridX = 0;
    int gridY = 0;
    int star = 1;
    int faceTowardsFallback = Towards_None;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;
    int fightsWon = 0;
    int sourceOrder = 0;
    std::function<Pointf(int, int)> positionForCell;
};

struct BattleSceneSetupBuildResult
{
    std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> units;
};

std::array<int, 5> readBattleFightFrames(int headId);
KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput makeSetupUnit(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents);
BattleSceneSetupBuildResult buildSetupUnits(
    std::span<const BattleSceneSetupUnitRequest> requests);
}
```

- [ ] **Step 2: Implement legacy extraction without mutating `Role`**

Create `src/BattleSceneSetupBuilder.cpp` by moving the logic currently in `BattleSceneHades.cpp` helpers:

```cpp
#include "BattleSceneSetupBuilder.h"

#include "BattleSceneRenderMath.h"
#include "Save.h"
#include "TextureManager.h"
#include "strfunc.h"

#include <algorithm>
#include <cassert>
#include <format>

namespace KysChess::BattleSceneSetupBuilder
{
namespace
{
int actPropertyForMagicType(const Role& role, int type)
{
    switch (type)
    {
    case 0: return role.Medicine;
    case 1: return role.Fist;
    case 2: return role.Sword;
    case 3: return role.Knife;
    case 4:
    case 5:
    case 6:
    case 7:
        return role.Unusual;
    default:
        return 0;
    }
}

std::vector<Magic*> learnedMagicsForStar(const Role& role, int star)
{
    std::vector<Magic*> magics;
    const int start = RoleSave::getMagicSlotStart(star);
    const int end = RoleSave::getMagicSlotEnd(star);
    for (int i = start; i < end; ++i)
    {
        auto* magic = Save::getInstance()->getMagic(role.MagicID[i]);
        if (magic)
        {
            magics.push_back(magic);
        }
    }
    return magics;
}

int magicPowerForStar(const Role& role, int magicId, int star)
{
    const int start = RoleSave::getMagicSlotStart(star);
    const int end = RoleSave::getMagicSlotEnd(star);
    for (int i = start; i < end; ++i)
    {
        if (role.MagicID[i] == magicId)
        {
            return role.MagicPower[i];
        }
    }
    return 0;
}

template<typename Cmp>
Magic* selectBattleMagic(const Role& role, int star, Cmp cmp)
{
    auto learned = learnedMagicsForStar(role, star);
    if (learned.empty())
    {
        return nullptr;
    }
    Magic* chosen = learned.front();
    int chosenPower = magicPowerForStar(role, chosen->ID, star);
    for (std::size_t i = 1; i < learned.size(); ++i)
    {
        const int candidatePower = magicPowerForStar(role, learned[i]->ID, star);
        if (cmp(candidatePower, chosenPower))
        {
            chosen = learned[i];
            chosenPower = candidatePower;
        }
    }
    return chosen;
}

KysChess::BattleSceneBattleAdapter::BattleSetupSkillSnapshot makeSkillSnapshot(
    const Role& role,
    int star,
    Magic* magic)
{
    KysChess::BattleSceneBattleAdapter::BattleSetupSkillSnapshot snapshot;
    if (!magic)
    {
        return snapshot;
    }
    snapshot.id = magic->ID;
    snapshot.name = magic->Name;
    snapshot.soundId = magic->SoundID;
    snapshot.hurtType = magic->HurtType;
    snapshot.attackAreaType = magic->AttackAreaType;
    snapshot.magicType = magic->MagicType;
    snapshot.visualEffectId = magic->EffectID;
    snapshot.selectDistance = magic->SelectDistance;
    snapshot.actProperty = actPropertyForMagicType(role, magic->MagicType);
    snapshot.magicPower = magicPowerForStar(role, magic->ID, star);
    return snapshot;
}

std::string skillNamesForStar(const Role& role, int star)
{
    std::string names;
    for (auto* magic : learnedMagicsForStar(role, star))
    {
        if (!names.empty())
        {
            names += " ";
        }
        names += magic->Name;
    }
    return names;
}

int facingTowardNearestOpponent(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents)
{
    int faceTowards = request.faceTowardsFallback;
    int minDistance = COORD_COUNT * COORD_COUNT;
    for (const auto& opponent : opponents)
    {
        if (opponent.team == request.team)
        {
            continue;
        }
        const int distance = calDistance(request.gridX, request.gridY, opponent.x, opponent.y);
        if (distance < minDistance)
        {
            minDistance = distance;
            const int candidate = calTowards(request.gridX, request.gridY, opponent.x, opponent.y);
            if (candidate != Towards_None)
            {
                faceTowards = candidate;
            }
        }
    }
    return faceTowards;
}
}

std::array<int, 5> readBattleFightFrames(int headId)
{
    std::array<int, 5> frames{};
    const std::string textGroup = std::format("fight/fight{:03}", headId);
    const std::string frameText = TextureManager::getInstance()->getTextureGroup(textGroup)->getFileContent("fightframe.txt");
    std::vector<int> values;
    strfunc::findNumbers(frameText, &values);
    for (int i = 0; i < static_cast<int>(values.size()) / 2; ++i)
    {
        frames[values[i * 2]] = values[i * 2 + 1];
    }
    return frames;
}

KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput makeSetupUnit(
    const BattleSceneSetupUnitRequest& request,
    std::span<const BattleSceneSetupOpponentCell> opponents)
{
    assert(request.source);
    assert(request.unitId >= 0);
    assert(request.team == 0 || request.team == 1);
    assert(request.positionForCell);

    const auto& role = *request.source;
    const int faceTowards = facingTowardNearestOpponent(request, opponents);

    KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput unit;
    unit.unitId = request.unitId;
    unit.realRoleId = role.RealID >= 0 ? role.RealID : role.ID;
    unit.name = role.Name;
    unit.headId = role.HeadID;
    unit.team = request.team;
    unit.sourceOrder = request.sourceOrder;
    unit.gridX = request.gridX;
    unit.gridY = request.gridY;
    unit.faceTowards = faceTowards;
    unit.position = request.positionForCell(request.gridX, request.gridY);
    unit.acceleration = { 0, 0, -4.0f };
    unit.facing = BattleSceneRenderMath::realTowardsFromFaceTowards(faceTowards);
    unit.fightFrames = readBattleFightFrames(role.HeadID);
    unit.star = request.star;
    unit.cost = role.Cost;
    unit.weaponId = request.weaponId;
    unit.armorId = request.armorId;
    unit.chessInstanceId = request.chessInstanceId;
    unit.fightsWon = request.fightsWon;
    unit.alive = true;
    unit.hp = role.MaxHP;
    unit.maxHp = role.MaxHP;
    unit.mp = 0;
    unit.maxMp = role.MaxMP;
    unit.attack = role.Attack;
    unit.defence = role.Defence;
    unit.speed = role.Speed;
    unit.fist = role.Fist;
    unit.sword = role.Sword;
    unit.knife = role.Knife;
    unit.unusual = role.Unusual;
    unit.hiddenWeapon = role.HiddenWeapon;
    unit.physicalPower = std::max(role.PhysicalPower, 90);
    for (int magicType = 0; magicType < BattleSceneRenderMath::FightStyleCount; ++magicType)
    {
        unit.actPropertiesByMagicType[magicType] = actPropertyForMagicType(role, magicType);
    }

    Magic* normalMagic = role.UsingMagic;
    Magic* ultimateMagic = role.UsingMagic;
    unit.hasEquippedSkill = normalMagic != nullptr;
    if (!normalMagic)
    {
        normalMagic = selectBattleMagic(role, request.star, [](int candidate, int current) { return candidate < current; });
        ultimateMagic = selectBattleMagic(role, request.star, [](int candidate, int current) { return candidate > current; });
    }
    unit.normalSkill = makeSkillSnapshot(role, request.star, normalMagic);
    unit.ultimateSkill = makeSkillSnapshot(role, request.star, ultimateMagic);
    unit.skillNames = skillNamesForStar(role, request.star);
    return unit;
}

BattleSceneSetupBuildResult buildSetupUnits(std::span<const BattleSceneSetupUnitRequest> requests)
{
    std::vector<BattleSceneSetupOpponentCell> cells;
    cells.reserve(requests.size());
    for (const auto& request : requests)
    {
        cells.push_back({ request.team, request.gridX, request.gridY });
    }

    BattleSceneSetupBuildResult result;
    result.units.reserve(requests.size());
    for (const auto& request : requests)
    {
        result.units.push_back(makeSetupUnit(request, cells));
    }
    std::sort(result.units.begin(), result.units.end(), [](const auto& lhs, const auto& rhs)
    {
        return lhs.unitId < rhs.unitId;
    });
    for (std::size_t index = 0; index < result.units.size(); ++index)
    {
        assert(result.units[index].unitId == static_cast<int>(index));
    }
    return result;
}
}
```

Before implementing this exact code, replace the hard-coded `-4.0f` acceleration with a request field if `gravity_` must remain Hades-owned in this batch.

- [ ] **Step 3: Add project entries**

Add `BattleSceneSetupBuilder.cpp` / `.h` to the game project and filters, and add `..\src\BattleSceneSetupBuilder.cpp` to `tests/kys_tests.vcxproj`.

- [ ] **Step 4: Build and run setup builder tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneSetupBuilder_*"
```

Expected: setup builder tests pass.

---

## Task 3: Replace Hades Temporary `Role` Staging

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`

- [ ] **Step 1: Remove private setup snapshot helper declarations**

Delete these declarations from `BattleSceneHades.h`:

```cpp
int assignSetupUnitId();
int realRoleIdForRole(const Role& role) const;
BattleSceneInitialRoleState makeInitialRoleState(Role& role, const std::vector<Role*>& opposingRoles);
KysChess::BattleSceneBattleAdapter::BattleSetupRoleSnapshot makeSetupRoleSnapshot(
    int unitId,
    Role& role,
    const BattleSceneInitialRoleState& initialState) const;
```

- [ ] **Step 2: Replace persistent setup vectors**

Replace:

```cpp
std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupRoleSnapshot> setup_role_snapshots_;
std::vector<KysChess::Battle::BattleSetupRosterUnit> setup_ally_roster_;
std::vector<KysChess::Battle::BattleSetupRosterUnit> setup_enemy_roster_;
int next_setup_unit_id_ = 0;
```

with:

```cpp
std::vector<KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput> setup_units_;
```

- [ ] **Step 3: Rework `onEntrance()` to build setup requests**

In `BattleSceneHades::onEntrance()`, remove `std::deque<Role> enemySetupRoles`, `std::deque<Role> allySetupRoles`, and all `resetBattleInfo()` mutation. Build request rows from saved role pointers:

```cpp
std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest> enemyRequests;
std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest> allyRequests;
```

Assign enemy unit ids in the same sorted order as today:

```cpp
std::vector<std::size_t> enemyIndexes(enemySources.size());
std::iota(enemyIndexes.begin(), enemyIndexes.end(), 0);
std::sort(enemyIndexes.begin(), enemyIndexes.end(), [&](std::size_t lhs, std::size_t rhs)
{
    const auto& left = *enemySources[lhs].source;
    const auto& right = *enemySources[rhs].source;
    return left.MaxHP + left.Attack < right.MaxHP + right.Attack;
});
int nextUnitId = 0;
for (auto index : enemyIndexes)
{
    enemySources[index].unitId = nextUnitId++;
}
for (auto& ally : allyRequests)
{
    ally.unitId = nextUnitId++;
}
```

Pass a `positionForCell` lambda to every request:

```cpp
auto positionForCell = [this](int x, int y)
{
    auto position = battle_map_.pos45To90(x, y);
    return Pointf{ position.x, position.y, 0 };
};
```

Build `setup_units_` once:

```cpp
std::vector<KysChess::BattleSceneSetupBuilder::BattleSceneSetupUnitRequest> requests;
requests.reserve(enemyRequests.size() + allyRequests.size());
requests.insert(requests.end(), enemyRequests.begin(), enemyRequests.end());
requests.insert(requests.end(), allyRequests.begin(), allyRequests.end());
for (auto& request : requests)
{
    request.positionForCell = positionForCell;
}
setup_units_ = KysChess::BattleSceneSetupBuilder::buildSetupUnits(requests).units;
```

- [ ] **Step 4: Update build context**

In `makeBattleRuntimeBuildContext()`, replace:

```cpp
context.setup.roles = setup_role_snapshots_;
context.setup.allyRoster = setup_ally_roster_;
context.setup.enemyRoster = setup_enemy_roster_;
```

with:

```cpp
context.setup.units = setup_units_;
```

- [ ] **Step 5: Build and run initialization tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][initialization]"
```

Expected: tests may fail until Task 4 updates the adapter to consume `setup.units`.

---

## Task 4: Make Adapter Consume Canonical Setup Units

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Replace setup input fields**

In `BattleRuntimeSceneSetupInput`, replace:

```cpp
std::vector<BattleSetupRoleSnapshot> roles;
std::vector<Battle::BattleSetupRosterUnit> allyRoster;
std::vector<Battle::BattleSetupRosterUnit> enemyRoster;
```

with:

```cpp
std::vector<BattleSetupUnitInput> units;
```

- [ ] **Step 2: Derive runtime roster rows from setup units**

Update `makeBattleRuntimeSetupSeed()`:

```cpp
for (const auto& unit : context.units)
{
    Battle::BattleSetupRosterUnit roster{
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
    if (unit.team == 0)
    {
        setup.allyRoster.push_back(roster);
    }
    else
    {
        setup.enemyRoster.push_back(roster);
    }

    setup.units.push_back({
        unit.unitId,
        unit.realRoleId,
        unit.team,
        unit.star,
        unit.cost,
        unit.maxHp,
        unit.attack,
        unit.defence,
        unit.speed,
        unit.fist,
        unit.sword,
        unit.knife,
        unit.unusual,
        unit.hiddenWeapon,
        {},
    });
}
```

Sort `setup.allyRoster` and `setup.enemyRoster` by `sourceOrder` after filling them.

- [ ] **Step 3: Update runtime init store creation**

Replace loops over `setup.roles` with loops over `setup.units` in `createInitializedBattleRuntimeSession()`. Existing projection methods should be renamed from role snapshot terminology to setup unit terminology:

```cpp
init.runtime.units.units.push_back(makeBattleRuntimeUnit(unit, state, context.rules.gridTransform));
init.runtime.status.units.push_back(Battle::makeBattleStatusRuntimeUnit(makeBattleStatusUnit(unit, *state)));
init.runtime.world.units.push_back(makeBattleWorldUnit(unit));
```

- [ ] **Step 4: Update action plan seed initialization**

Change `BattleActionPlanInputContext::setupRoles` to:

```cpp
const std::vector<BattleSetupUnitInput>* setupUnits = nullptr;
```

Then seed action plans from `unit.normalSkill` and `unit.ultimateSkill`.

- [ ] **Step 5: Update adapter initialization tests**

In `tests/BattleInitializationUnitTests.cpp`, replace `context.setup.roles.push_back(source)` and `context.setup.allyRoster.push_back(...)` with a single setup unit:

```cpp
Adapter::BattleSetupUnitInput source;
source.unitId = 0;
source.realRoleId = 1001;
source.name = "測試角色";
source.headId = 23;
source.team = 0;
source.sourceOrder = 0;
source.gridX = 1;
source.gridY = 2;
source.faceTowards = Towards_RightDown;
source.position = { 100, 200, 0 };
source.facing = { 1, 1, 0 };
source.star = 3;
source.cost = 7;
source.chessInstanceId = 99;
source.hp = 100;
source.maxHp = 100;
source.attack = 20;
source.defence = 30;
source.speed = 40;
context.setup.units.push_back(source);
```

- [ ] **Step 6: Build and run initialization tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "[battle][initialization]"
```

Expected: initialization tests pass.

---

## Task 5: Delete Post-Initialization Snapshot Round-Trip

**Files:**
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Delete snapshot reconstruction helpers**

Remove:

```cpp
BattleSetupRoleSnapshot makeRuntimeSetupRoleSnapshot(...);
std::vector<BattleSetupRoleSnapshot> makeOrderedRuntimeRoleSnapshots(...);
const BattleSetupRoleSnapshot& requireSetupRoleSnapshot(...);
```

- [ ] **Step 2: Build scene units from runtime units plus setup metadata**

Add helper:

```cpp
const BattleSetupUnitInput& requireSetupUnit(
    const BattleRuntimeSceneSetupInput& setup,
    int unitId)
{
    return Battle::requireDenseById(setup.units, unitId);
}
```

Add clone-aware scene unit creation:

```cpp
BattleSceneUnit makeInitializedSceneUnit(
    const Battle::BattleRuntimeUnit& runtime,
    const BattleSetupUnitInput& setup,
    const Battle::BattleInitializationCloneIntent* clone)
{
    BattleSceneUnit unit;
    unit.identity = {
        runtime.id,
        runtime.realRoleId,
        runtime.team,
        setup.headId,
        setup.name,
    };
    unit.unitId = runtime.id;
    unit.sourceUnitId = clone ? clone->sourceUnitId : runtime.id;
    unit.gridX = runtime.grid.x;
    unit.gridY = runtime.grid.y;
    unit.faceTowards = setup.faceTowards;
    unit.position = runtime.position;
    unit.velocity = runtime.velocity;
    unit.acceleration = runtime.acceleration;
    unit.realTowards = runtime.facing;
    unit.fightFrames = setup.fightFrames;
    unit.star = runtime.star;
    unit.chessInstanceId = clone ? -1 : setup.chessInstanceId;
    unit.weaponId = clone ? -1 : setup.weaponId;
    unit.armorId = clone ? -1 : setup.armorId;
    unit.cost = runtime.cost;
    unit.hp = runtime.hp;
    unit.maxHp = runtime.maxHp;
    unit.mp = runtime.mp;
    unit.maxMp = runtime.maxMp;
    unit.attack = runtime.attack;
    unit.defence = runtime.defence;
    unit.speed = runtime.speed;
    unit.cooldown = runtime.cooldown;
    unit.cooldownMax = runtime.cooldownMax;
    unit.frozen = setup.frozen;
    unit.frozenMax = setup.frozenMax;
    unit.invincible = runtime.invincible;
    unit.actType = runtime.actType;
    unit.actFrame = runtime.actFrame;
    unit.alive = runtime.alive;
    unit.active = runtime.alive;
    unit.skillNames = setup.skillNames;
    return unit;
}
```

For clones, use `clone->sourceUnitId` to read source setup metadata for render metadata:

```cpp
const auto* clone = findCloneIntent(initialization, runtimeUnit.id);
const auto& setupUnit = clone
    ? requireSetupUnit(setup, clone->sourceUnitId)
    : requireSetupUnit(setup, runtimeUnit.id);
```

- [ ] **Step 3: Build setup configuration from runtime units directly**

In `commitInitializedBattleRuntimeConfiguration()`, iterate `runtime.units.units` directly. Use runtime fields for world/damage/status state and setup metadata only for action skill seeds, presentation styles, and fight metadata.

- [ ] **Step 4: Run the clone scene row test**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror"
```

Expected: clone scene row test passes and still verifies clone source mapping.

- [ ] **Step 5: Search for deleted round-trip names**

Run:

```powershell
rg -n "makeRuntimeSetupRoleSnapshot|makeOrderedRuntimeRoleSnapshots|BattleSetupRoleSnapshot|setup_role_snapshots_|setup_ally_roster_|setup_enemy_roster_" src tests
```

Expected: no matches.

---

## Task 6: Decouple `BattleSceneUnitStore` from Adapter Result

**Files:**
- Modify: `src/BattleSceneUnitStore.h`
- Modify: `src/BattleSceneUnitStore.cpp`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `tests/BattleSceneUnitStoreUnitTests.cpp`

- [ ] **Step 1: Change store initialize signature**

Replace:

```cpp
void initialize(const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result);
```

with:

```cpp
void initialize(std::vector<BattleSceneUnit> units);
```

Implementation:

```cpp
void BattleSceneUnitStore::initialize(std::vector<BattleSceneUnit> units)
{
    units_ = std::move(units);
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        assert(units_[index].unitId == static_cast<int>(index));
    }
}
```

- [ ] **Step 2: Update Hades caller**

Replace:

```cpp
scene_units_.initialize(created.sceneInitialization);
```

with:

```cpp
scene_units_.initialize(std::move(created.sceneInitialization.units));
```

- [ ] **Step 3: Update store tests**

Each store test should construct a `std::vector<BattleSceneUnit>` directly and pass it to `initialize(std::move(units))`.

- [ ] **Step 4: Remove adapter include from store**

Delete from `BattleSceneUnitStore.h`:

```cpp
namespace KysChess::BattleSceneBattleAdapter
{
struct BattleInitializationSceneApplyResult;
}
```

Delete `#include "BattleSceneBattleAdapter.h"` from `BattleSceneUnitStore.cpp`.

- [ ] **Step 5: Run store tests**

Run:

```powershell
x64\Debug\kys_tests.exe "BattleSceneUnitStore_*"
```

Expected: store tests pass.

---

## Task 7: Delete Hades Legacy Helper Code

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`

- [ ] **Step 1: Remove moved helpers from Hades anonymous namespace**

Delete:

```cpp
template<typename Cmp>
Magic* selectBattleMagic(Role& role, Cmp cmp);
BattleSetupSkillSnapshot makeBattleSetupSkillSnapshot(Role& role, Magic* magic);
std::string makeBattleSkillNames(Role& role, int star);
```

- [ ] **Step 2: Remove moved member functions**

Delete implementations for:

```cpp
int BattleSceneHades::assignSetupUnitId();
int BattleSceneHades::realRoleIdForRole(const Role& role) const;
BattleSceneHades::BattleSceneInitialRoleState BattleSceneHades::makeInitialRoleState(...);
BattleSceneHades::makeSetupRoleSnapshot(...);
```

- [ ] **Step 3: Search for setup-stage `Role` staging**

Run:

```powershell
rg -n "std::deque<Role>|std::vector<Role\\*>|resetBattleInfo\\(|setPositionOnly\\(|makeSetupRoleSnapshot|makeInitialRoleState|setup_role_snapshots_|setup_ally_roster_|setup_enemy_roster_" src\BattleSceneHades.cpp src\BattleSceneHades.h
```

Expected: no matches for setup staging in Hades.

---

## Task 8: Full Verification

**Files:**
- No source edits unless verification exposes a compile or test failure.

- [ ] **Step 1: Formatting check**

Run:

```powershell
git diff --check
```

Expected: no output, exit code 0.

- [ ] **Step 2: Build test target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: MSBuild exit code 0.

- [ ] **Step 3: Run full tests**

Run:

```powershell
x64\Debug\kys_tests.exe
```

Expected: all tests pass.

- [ ] **Step 4: Build game target**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: MSBuild exit code 0. Existing warning noise is acceptable; new compile errors are not.

- [ ] **Step 5: Boundary searches**

Run:

```powershell
rg -n "BattleSetupRoleSnapshot|makeRuntimeSetupRoleSnapshot|makeOrderedRuntimeRoleSnapshots|setup_role_snapshots_|setup_ally_roster_|setup_enemy_roster_|std::deque<Role>|resetBattleInfo\\(" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h tests
rg -n "BattleSceneBattleAdapter.h" src\BattleSceneUnitStore.h src\BattleSceneUnitStore.cpp
```

Expected: no matches.

---

## Self-Review Notes

- The adapter/setup boundary still owns legacy `Role` and `Magic` extraction, but only in `BattleSceneSetupBuilder`; this is intentional for this batch.
- The scene no longer stages setup through mutable copied `Role` objects.
- Runtime initialization consumes canonical setup units directly.
- Scene initialization consumes final runtime unit state plus setup metadata directly.
- The post-initialization snapshot reconstruction path is deleted rather than renamed.
