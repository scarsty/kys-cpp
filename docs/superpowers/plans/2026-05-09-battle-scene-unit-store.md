# Battle Scene Unit Store Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the duplicated Hades setup/render rows with one tested scene unit store, and move pure render math out of `BattleSceneHades`.

**Architecture:** `BattleSceneHades` should orchestrate battle setup and rendering, not own low-level unit projection helpers. Introduce one canonical `BattleSceneUnit` row used for setup swap, placement input, rendering, post-battle summary, combo refs, and presentation lookup. Keep pure math as free functions, not builders, and keep the adapter focused on runtime creation.

**Tech Stack:** C++20, Visual Studio/MSBuild, Catch2 tests, existing PowerShell build scripts.

---

## Why This Reduces Copying

Current flow has overlapping rows:

```text
BattleInitializedSceneUnit
    -> BattleSceneSetupUnit
    -> BattleSceneRenderUnit
    -> unit_identities_
    -> active_unit_ids_
    -> ally_unit_ids_
```

The new flow is:

```text
BattleInitializedSceneUnit
    -> BattleSceneUnitStore::initialize(...)
    -> vector<BattleSceneUnit> + id index only
```

`BattleSceneUnitStore` derives active/ally ids by scanning canonical rows when needed. It does not maintain duplicate identity or id-vector mirrors.

## File Responsibilities

- Create `src/BattleSceneRenderMath.h`
  - Header-only pure functions for face/facing conversion, fight-style resolution, render-pic calculation, decrement-to-zero, and hurt-flash color.
  - No dependency on `BattleSceneHades`.

- Create `src/BattleSceneUnitStore.h`
  - Defines `BattleSceneRenderComboState`, `BattleSceneUnit`, and `BattleSceneUnitStore`.
  - Owns canonical scene unit rows and the unit-id index.
  - Exposes setup/render operations currently implemented as Hades private helpers.

- Create `src/BattleSceneUnitStore.cpp`
  - Implements store initialization, lookup, setup position swap, placement input, combo refs, nearest-enemy facing, alive count, and post-battle summary row projection.

- Modify `src/BattleSceneBattleAdapter.h`
  - Include `BattleSceneUnitStore.h` or a small shared unit-state header.
  - Replace `BattleInitializedSceneUnit` with `BattleSceneUnit` in `BattleInitializationSceneApplyResult`.
  - This removes one adapter-to-scene DTO copy.

- Modify `src/BattleSceneBattleAdapter.cpp`
  - Change `makeInitializedSceneUnit(...)` to return `BattleSceneUnit`.
  - Populate the canonical unit row directly.

- Modify `src/BattleSceneHades.h/.cpp`
  - Remove nested `BattleSceneRenderComboState`, `BattleSceneRenderUnit`, and `BattleSceneSetupUnit`.
  - Replace unit/index/id containers with one `BattleSceneUnitStore scene_units_;`.
  - Replace private helper methods with store or math calls.

- Modify `tests/kys_tests.vcxproj`
  - Add new unit test files and `BattleSceneUnitStore.cpp`.

- Create `tests/BattleSceneRenderMathUnitTests.cpp`
  - Tests pure render math.

- Create `tests/BattleSceneUnitStoreUnitTests.cpp`
  - Tests setup swap, placement input, combo refs, facing, alive counts, and initialization behavior.

---

### Task 1: Extract Pure Render Math

**Files:**
- Create: `src/BattleSceneRenderMath.h`
- Test: `tests/BattleSceneRenderMathUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Create the failing render math tests**

Create `tests/BattleSceneRenderMathUnitTests.cpp`:

```cpp
#include "BattleSceneRenderMath.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneRenderMath_ConvertsFaceTowardsToRenderVector", "[battle][scene_render_math]")
{
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightDown).x == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightDown).y == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightUp).x == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_RightUp).y == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftDown).x == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftDown).y == 1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftUp).x == -1);
    CHECK(BattleSceneRenderMath::realTowardsFromFaceTowards(Towards_LeftUp).y == -1);
}

TEST_CASE("BattleSceneRenderMath_ResolvesFallbackFightStyle", "[battle][scene_render_math]")
{
    std::array<int, 5> frames{ 0, 0, 0, 6, 0 };

    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, 0) == 3);
    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, 2) == 3);
    CHECK(BattleSceneRenderMath::resolveRenderFightStyle(frames, -1) == 3);
}

TEST_CASE("BattleSceneRenderMath_CalculatesRenderPicFromFramesAndFacing", "[battle][scene_render_math]")
{
    std::array<int, 5> frames{ 4, 5, 6, 7, 8 };
    Pointf rightDown{ 1, 1, 0 };
    Pointf leftUp{ -1, -1, 0 };

    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, rightDown, -1, 0) == 4 * Towards_RightDown);
    CHECK(BattleSceneRenderMath::calRenderUnitPic(frames, leftUp, 2, 3) == 4 * 4 + 5 * 4 + 6 * Towards_LeftUp + 3);
}

TEST_CASE("BattleSceneRenderMath_DecreasesValuesToZero", "[battle][scene_render_math]")
{
    int integerValue = 2;
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 1);
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 0);
    BattleSceneRenderMath::decreaseToZero(integerValue);
    CHECK(integerValue == 0);

    float floatValue = 0.25f;
    BattleSceneRenderMath::decreaseToZero(floatValue, 0.5f);
    CHECK(floatValue == 0.0f);
}

TEST_CASE("BattleSceneRenderMath_AppliesHurtFlashColorOnlyDuringActiveTimer", "[battle][scene_render_math]")
{
    Color base{ 10, 20, 30, 40 };

    CHECK(BattleSceneRenderMath::hurtFlashColor(0, base).r == 10);
    CHECK(BattleSceneRenderMath::hurtFlashColor(3, base).r == 255);
    CHECK(BattleSceneRenderMath::hurtFlashColor(3, base).a == 40);
}
```

- [ ] **Step 2: Add tests to the test project**

In `tests/kys_tests.vcxproj`, add:

```xml
    <ClCompile Include="BattleSceneRenderMathUnitTests.cpp" />
```

inside the existing test `<ItemGroup>` with the other `*UnitTests.cpp` entries.

- [ ] **Step 3: Run the tests and verify they fail because the header is missing**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile fails with missing `BattleSceneRenderMath.h`.

- [ ] **Step 4: Create `BattleSceneRenderMath.h`**

Create `src/BattleSceneRenderMath.h`:

```cpp
#pragma once

#include "Point.h"
#include "Types.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace BattleSceneRenderMath
{
inline constexpr int FightStyleCount = 5;
inline constexpr int HurtFlashPeriod = 3;

inline constexpr int FightStyleFallbacks[FightStyleCount][FightStyleCount - 1] = {
    { 1, 2, 3, 4 },
    { 4, 2, 3, 0 },
    { 3, 1, 4, 0 },
    { 2, 1, 4, 0 },
    { 1, 2, 3, 0 },
};

inline Pointf realTowardsFromFaceTowards(int faceTowards)
{
    switch (faceTowards)
    {
    case Towards_RightDown:
        return { 1, 1, 0 };
    case Towards_RightUp:
        return { 1, -1, 0 };
    case Towards_LeftDown:
        return { -1, 1, 0 };
    case Towards_LeftUp:
        return { -1, -1, 0 };
    default:
        assert(false);
        return {};
    }
}

inline int firstAvailableRenderFightStyle(const std::array<int, FightStyleCount>& fightFrames)
{
    for (int style = 0; style < FightStyleCount; ++style)
    {
        if (fightFrames[style] > 0)
        {
            return style;
        }
    }
    return -1;
}

inline int resolveRenderFightStyle(const std::array<int, FightStyleCount>& fightFrames, int style)
{
    if (style >= 0 && style < FightStyleCount && fightFrames[style] > 0)
    {
        return style;
    }
    if (style >= 0 && style < FightStyleCount)
    {
        for (int fallback : FightStyleFallbacks[style])
        {
            if (fightFrames[fallback] > 0)
            {
                return fallback;
            }
        }
    }
    return firstAvailableRenderFightStyle(fightFrames);
}

inline int calRenderUnitPic(
    const std::array<int, FightStyleCount>& fightFrames,
    const Pointf& realTowards,
    int style,
    int frame)
{
    const int faceTowards = realTowardsToFaceTowards(realTowards);
    if (style < 0 || style >= FightStyleCount)
    {
        style = resolveRenderFightStyle(fightFrames, -1);
        return style >= 0 ? fightFrames[style] * faceTowards : faceTowards;
    }

    style = resolveRenderFightStyle(fightFrames, style);
    if (style < 0)
    {
        return faceTowards;
    }

    int total = 0;
    for (int i = 0; i < FightStyleCount; ++i)
    {
        if (i == style)
        {
            const int frameCount = fightFrames[style];
            const int clampedFrame = frame < frameCount - 2 ? frame : frameCount - 2;
            return total + frameCount * faceTowards + clampedFrame;
        }
        total += fightFrames[i] * 4;
    }
    return faceTowards;
}

inline void decreaseToZero(int& value)
{
    if (value > 0)
    {
        --value;
    }
}

template <typename T>
void decreaseToZero(T& value, T step)
{
    if (value > 0)
    {
        value -= step;
    }
    if (value < 0)
    {
        value = 0;
    }
}

inline Color hurtFlashColor(int timer, const Color& baseColor)
{
    if (timer <= 0)
    {
        return baseColor;
    }
    const int phase = timer % HurtFlashPeriod;
    if (phase < 2)
    {
        return { 255, 100, 100, baseColor.a };
    }
    return baseColor;
}
}  // namespace BattleSceneRenderMath
```

- [ ] **Step 5: Run render math tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneRenderMath*"
```

Expected: build succeeds and all `BattleSceneRenderMath*` tests pass.

---

### Task 2: Introduce Canonical Scene Unit Store

**Files:**
- Create: `src/BattleSceneUnitStore.h`
- Create: `src/BattleSceneUnitStore.cpp`
- Test: `tests/BattleSceneUnitStoreUnitTests.cpp`
- Modify: `tests/kys_tests.vcxproj`

- [ ] **Step 1: Create the failing unit store tests**

Create `tests/BattleSceneUnitStoreUnitTests.cpp`:

```cpp
#include "BattleSceneUnitStore.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
BattleSceneUnit makeUnit(int id, int team, int x, int y, Pointf position)
{
    BattleSceneUnit unit;
    unit.identity = {
        id,
        1000 + id,
        team,
        10 + id,
        std::format("unit{}", id),
    };
    unit.unitId = id;
    unit.gridX = x;
    unit.gridY = y;
    unit.faceTowards = Towards_RightDown;
    unit.alive = true;
    unit.active = true;
    unit.position = position;
    unit.realTowards = { 1, 1, 0 };
    unit.maxHp = 100;
    unit.hp = 100;
    unit.cost = 2;
    return unit;
}
}

TEST_CASE("BattleSceneUnitStore_InitializesRowsAndIndexesByUnitId", "[battle][scene_unit_store]")
{
    KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult result;
    result.units.push_back(makeUnit(0, 0, 1, 2, { 10, 20, 0 }));
    result.units.push_back(makeUnit(1, 1, 5, 6, { 30, 40, 0 }));

    BattleSceneUnitStore store;
    store.initialize(result);

    CHECK(store.requireUnit(0).identity.realRoleId == 1000);
    CHECK(store.requireUnit(1).identity.team == 1);
    CHECK(store.aliveUnitsOnTeam(0) == 1);
    CHECK(store.aliveUnitsOnTeam(1) == 1);
}

TEST_CASE("BattleSceneUnitStore_SwapsSetupPositionAndUpdatesWorldPosition", "[battle][scene_unit_store]")
{
    KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult result;
    result.units.push_back(makeUnit(0, 0, 1, 2, { 10, 20, 0 }));
    result.units.push_back(makeUnit(1, 0, 3, 4, { 30, 40, 0 }));

    BattleSceneUnitStore store;
    store.initialize(result);
    store.swapSetupUnitPositions(
        0,
        1,
        [](int x, int y)
        {
            return Pointf{ static_cast<float>(x * 10), static_cast<float>(y * 10), 0 };
        });

    CHECK(store.requireUnit(0).gridX == 3);
    CHECK(store.requireUnit(0).gridY == 4);
    CHECK(store.requireUnit(0).position.x == 30);
    CHECK(store.requireUnit(0).position.y == 40);
    CHECK(store.requireUnit(1).gridX == 1);
    CHECK(store.requireUnit(1).gridY == 2);
    CHECK(store.requireUnit(1).position.x == 10);
    CHECK(store.requireUnit(1).position.y == 20);
}

TEST_CASE("BattleSceneUnitStore_BuildsSetupPlacementOnlyForActiveUnits", "[battle][scene_unit_store]")
{
    KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult result;
    auto active = makeUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto inactive = makeUnit(1, 0, 3, 4, { 30, 40, 0 });
    inactive.active = false;
    result.units.push_back(active);
    result.units.push_back(inactive);

    BattleSceneUnitStore store;
    store.initialize(result);
    auto placement = store.makeSetupPlacementInput();

    REQUIRE(placement.units.size() == 1);
    CHECK(placement.units[0].unitId == 0);
    CHECK(placement.units[0].gridX == 1);
    CHECK(placement.units[0].gridY == 2);
    CHECK(placement.units[0].faceTowards == Towards_RightDown);
}

TEST_CASE("BattleSceneUnitStore_BuildsComboRefsFromCanonicalRows", "[battle][scene_unit_store]")
{
    KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult result;
    auto alive = makeUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto dead = makeUnit(1, 1, 3, 4, { 30, 40, 0 });
    dead.alive = false;
    result.units.push_back(alive);
    result.units.push_back(dead);

    BattleSceneUnitStore store;
    store.initialize(result);
    auto refs = store.makeComboBattleUnitRefs();

    REQUIRE(refs.size() == 2);
    CHECK(refs[0].battleId == 0);
    CHECK(refs[0].alive);
    CHECK(refs[1].battleId == 1);
    CHECK(!refs[1].alive);
}

TEST_CASE("BattleSceneUnitStore_FacesTowardNearestAliveEnemy", "[battle][scene_unit_store]")
{
    KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult result;
    result.units.push_back(makeUnit(0, 0, 1, 2, { 0, 0, 0 }));
    result.units.push_back(makeUnit(1, 1, 3, 4, { 10, 0, 0 }));
    result.units.push_back(makeUnit(2, 1, 5, 6, { 100, 0, 0 }));

    BattleSceneUnitStore store;
    store.initialize(result);
    auto facing = store.facingTowardNearestEnemy(0);

    CHECK(facing.x == 1.0f);
    CHECK(facing.y == 0.0f);
}
```

- [ ] **Step 2: Add store files to the test project**

In `tests/kys_tests.vcxproj`, add:

```xml
    <ClCompile Include="BattleSceneUnitStoreUnitTests.cpp" />
    <ClCompile Include="..\src\BattleSceneUnitStore.cpp" />
```

- [ ] **Step 3: Run the tests and verify they fail because the store is missing**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
```

Expected: compile fails with missing `BattleSceneUnitStore.h`.

- [ ] **Step 4: Create `BattleSceneUnitStore.h`**

Create `src/BattleSceneUnitStore.h`:

```cpp
#pragma once

#include "BattlePostBattleSummary.h"
#include "ChessCombo.h"
#include "Point.h"
#include "battle/BattleRuntimeSession.h"

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace KysChess::BattleSceneBattleAdapter
{
struct BattleInitializationSceneApplyResult;
}

struct BattleSceneRenderComboState
{
    int shield = 0;
    int blockFirstHitsRemaining = 0;
};

struct BattleSceneUnit
{
    BattleUnitIdentity identity;
    int unitId = -1;
    int sourceUnitId = -1;

    int gridX = 0;
    int gridY = 0;
    int faceTowards = Towards_None;
    int star = 1;
    int chessInstanceId = -1;
    int weaponId = -1;
    int armorId = -1;
    int cost = 0;
    std::string skillNames;

    bool alive = true;
    bool active = true;
    Pointf position;
    Pointf velocity;
    Pointf acceleration;
    Pointf realTowards;
    std::array<int, 5> fightFrames{};
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
    int frozen = 0;
    int frozenMax = 0;
    int invincible = 0;
    int shake = 0;
    int attention = 0;
    BattleSceneRenderComboState combo;
};

class BattleSceneUnitStore
{
public:
    using PositionForCell = std::function<Pointf(int, int)>;

    void initialize(const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result);

    BattleSceneUnit& requireUnit(int unitId);
    const BattleSceneUnit& requireUnit(int unitId) const;

    std::vector<BattleSceneUnit>& units() { return units_; }
    const std::vector<BattleSceneUnit>& units() const { return units_; }

    void swapSetupUnitPositions(int firstUnitId, int secondUnitId, PositionForCell positionForCell);
    KysChess::Battle::BattleSetupPlacementInput makeSetupPlacementInput() const;
    std::vector<KysChess::ChessComboBattleUnitRef> makeComboBattleUnitRefs() const;
    Pointf facingTowardNearestEnemy(int unitId) const;
    int aliveUnitsOnTeam(int team) const;
    std::vector<int> allyUnitIds() const;
    BattlePostBattleSummary makePostBattleSummary(const BattleTracker& tracker, int battleResult) const;

private:
    std::vector<BattleSceneUnit> units_;
    std::unordered_map<int, std::size_t> indexByUnitId_;
};
```

- [ ] **Step 5: Create `BattleSceneUnitStore.cpp`**

Create `src/BattleSceneUnitStore.cpp`:

```cpp
#include "BattleSceneUnitStore.h"

#include "BattleSceneBattleAdapter.h"
#include "BattleTracker.h"
#include "battle/BattleFind.h"

#include <algorithm>
#include <cassert>

void BattleSceneUnitStore::initialize(
    const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result)
{
    units_ = result.units;
    indexByUnitId_.clear();
    indexByUnitId_.reserve(units_.size());
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        assert(units_[index].unitId >= 0);
        indexByUnitId_[units_[index].unitId] = index;
    }
}

BattleSceneUnit& BattleSceneUnitStore::requireUnit(int unitId)
{
    auto it = indexByUnitId_.find(unitId);
    assert(it != indexByUnitId_.end());
    return units_[it->second];
}

const BattleSceneUnit& BattleSceneUnitStore::requireUnit(int unitId) const
{
    auto it = indexByUnitId_.find(unitId);
    assert(it != indexByUnitId_.end());
    return units_[it->second];
}

void BattleSceneUnitStore::swapSetupUnitPositions(
    int firstUnitId,
    int secondUnitId,
    PositionForCell positionForCell)
{
    auto& first = requireUnit(firstUnitId);
    auto& second = requireUnit(secondUnitId);
    std::swap(first.gridX, second.gridX);
    std::swap(first.gridY, second.gridY);
    first.position = positionForCell(first.gridX, first.gridY);
    second.position = positionForCell(second.gridX, second.gridY);
}

KysChess::Battle::BattleSetupPlacementInput BattleSceneUnitStore::makeSetupPlacementInput() const
{
    KysChess::Battle::BattleSetupPlacementInput input;
    input.units.reserve(units_.size());
    for (const auto& unit : units_)
    {
        if (!unit.active)
        {
            continue;
        }
        input.units.push_back({
            unit.unitId,
            unit.gridX,
            unit.gridY,
            unit.faceTowards,
        });
    }
    return input;
}

std::vector<KysChess::ChessComboBattleUnitRef> BattleSceneUnitStore::makeComboBattleUnitRefs() const
{
    std::vector<KysChess::ChessComboBattleUnitRef> refs;
    refs.reserve(units_.size());
    for (const auto& unit : units_)
    {
        refs.push_back({
            unit.unitId,
            unit.identity.realRoleId,
            unit.identity.team,
            unit.alive,
            unit.cost,
        });
    }
    return refs;
}

Pointf BattleSceneUnitStore::facingTowardNearestEnemy(int unitId) const
{
    const auto& source = requireUnit(unitId);
    const BattleSceneUnit* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const auto& candidate : units_)
    {
        if (!candidate.alive || candidate.identity.team == source.identity.team)
        {
            continue;
        }
        auto delta = candidate.position - source.position;
        const auto distance = delta.norm();
        if (!nearest || distance < nearestDistance)
        {
            nearest = &candidate;
            nearestDistance = distance;
        }
    }
    if (!nearest)
    {
        return source.realTowards;
    }

    auto facing = nearest->position - source.position;
    facing.z = 0;
    facing.normTo(1.0f);
    return facing;
}

int BattleSceneUnitStore::aliveUnitsOnTeam(int team) const
{
    return static_cast<int>(std::ranges::count_if(
        units_,
        [team](const BattleSceneUnit& unit)
        {
            return unit.active && unit.identity.team == team && unit.alive;
        }));
}

std::vector<int> BattleSceneUnitStore::allyUnitIds() const
{
    std::vector<int> ids;
    for (const auto& unit : units_)
    {
        if (unit.active && unit.alive && unit.identity.team == 0)
        {
            ids.push_back(unit.unitId);
        }
    }
    return ids;
}

BattlePostBattleSummary BattleSceneUnitStore::makePostBattleSummary(
    const BattleTracker& tracker,
    int battleResult) const
{
    BattlePostBattleSummary summary;
    summary.battleResult = battleResult;

    auto append = [&tracker](const BattleSceneUnit& source, std::vector<BattlePostBattleUnitSummary>& target)
    {
        BattlePostBattleUnitSummary unit;
        unit.identity = source.identity;
        unit.star = source.star;
        unit.chessInstanceId = source.chessInstanceId;
        unit.hp = source.maxHp;
        unit.maxHp = source.maxHp;
        unit.attack = source.attack;
        unit.defence = source.defence;
        unit.speed = source.speed;
        unit.weaponId = source.weaponId;
        unit.armorId = source.armorId;
        unit.skillNames = source.skillNames;
        unit.hpRemaining = source.hp;
        unit.maxHpRemaining = source.maxHp;
        unit.dead = !source.alive;
        unit.cancelDmg = tracker.cancelDamageForUnit(source.unitId);
        target.push_back(std::move(unit));
    };

    for (const auto& unit : units_)
    {
        if (unit.identity.team == 0)
        {
            append(unit, summary.allies);
        }
        else if (unit.identity.team == 1)
        {
            append(unit, summary.enemies);
        }
    }
    return summary;
}
```

- [ ] **Step 6: Run store tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneUnitStore*"
```

Expected: build succeeds and all `BattleSceneUnitStore*` tests pass.

---

### Task 3: Make Adapter Output Canonical Scene Units

**Files:**
- Modify: `src/BattleSceneBattleAdapter.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `tests/BattleInitializationUnitTests.cpp`

- [ ] **Step 1: Replace adapter initialization output row type**

In `src/BattleSceneBattleAdapter.h`, include the store header:

```cpp
#include "BattleSceneUnitStore.h"
```

Delete `struct BattleInitializedSceneUnit`.

Change `BattleInitializationSceneApplyResult`:

```cpp
struct BattleInitializationSceneApplyResult
{
    std::map<int, RoleComboState> comboStates;
    std::vector<BattleSceneUnit> units;
    std::vector<Battle::BattleLogEvent> logEvents;
    std::vector<Battle::BattleVisualEvent> visualEvents;
};
```

- [ ] **Step 2: Change adapter scene unit construction**

In `src/BattleSceneBattleAdapter.cpp`, rename:

```cpp
BattleInitializedSceneUnit makeInitializedSceneUnit(...)
```

to:

```cpp
BattleSceneUnit makeInitializedSceneUnit(...)
```

Inside the function, change:

```cpp
BattleInitializedSceneUnit unit;
```

to:

```cpp
BattleSceneUnit unit;
```

Set `unit.active = role.alive;` before return:

```cpp
    unit.alive = role.alive;
    unit.active = role.alive;
    unit.skillNames = role.skillNames;
    return unit;
```

- [ ] **Step 3: Update adapter tests to use canonical row type**

In `tests/BattleInitializationUnitTests.cpp`, replace lambda parameter types:

```cpp
[](const Adapter::BattleInitializedSceneUnit& unit)
```

with:

```cpp
[](const BattleSceneUnit& unit)
```

- [ ] **Step 4: Run initialization tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneBattleAdapter*"
```

Expected: build succeeds and adapter clone scene-row tests pass.

---

### Task 4: Migrate Hades To `BattleSceneUnitStore`

**Files:**
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/kys.vcxproj`
- Modify: `src/kys.vcxproj.filters`

- [ ] **Step 1: Add store source to the game project**

In `src/kys.vcxproj`, add:

```xml
    <ClCompile Include="BattleSceneUnitStore.cpp" />
```

with other `BattleScene*.cpp` files, and add:

```xml
    <ClInclude Include="BattleSceneUnitStore.h" />
    <ClInclude Include="BattleSceneRenderMath.h" />
```

with other headers.

In `src/kys.vcxproj.filters`, add matching entries under the existing source/header filters used for `BattleSceneHades`.

- [ ] **Step 2: Replace nested Hades unit structs with store include**

In `src/BattleSceneHades.h`, add:

```cpp
#include "BattleSceneRenderMath.h"
#include "BattleSceneUnitStore.h"
```

Delete nested structs:

```cpp
BattleSceneRenderComboState
BattleSceneRenderUnit
BattleSceneSetupUnit
```

Remove fields:

```cpp
std::unordered_map<int, BattleUnitIdentity> unit_identities_;
std::vector<BattleSceneSetupUnit> setup_units_;
std::unordered_map<int, std::size_t> setup_unit_index_by_id_;
std::vector<int> active_unit_ids_;
std::vector<int> ally_unit_ids_;
std::vector<BattleSceneRenderUnit> render_units_;
std::unordered_map<int, std::size_t> render_unit_index_by_id_;
```

Add:

```cpp
BattleSceneUnitStore scene_units_;
```

- [ ] **Step 3: Delete Hades private store helper declarations**

In `src/BattleSceneHades.h`, remove these declarations:

```cpp
BattleSceneRenderUnit& requireRenderUnit(int unitId);
const BattleSceneRenderUnit& requireRenderUnit(int unitId) const;
BattleSceneSetupUnit& requireSetupUnit(int unitId);
const BattleSceneSetupUnit& requireSetupUnit(int unitId) const;
void applyBattleInitializationSceneResult(
    const KysChess::BattleSceneBattleAdapter::BattleInitializationSceneApplyResult& result);
void swapSetupUnitPositions(int firstUnitId, int secondUnitId);
std::vector<KysChess::ChessComboBattleUnitRef> makeComboBattleUnitRefs() const;
Pointf facingTowardNearestEnemy(int unitId) const;
int aliveRenderUnitsOnTeam(int team) const;
int calRenderUnitPic(const BattleSceneRenderUnit& unit, int style, int frame) const;
Color calculateHurtFlashColor(int unitId, const Color& baseColor) const;
static Pointf realTowardsFromFaceTowards(int faceTowards);
static void decreaseToZero(int& value);
```

Change render signatures:

```cpp
void renderExtraRoleInfo(const BattleSceneUnit& unit, double x, double y);
```

- [ ] **Step 4: Replace Hades initialization application**

In `BattleSceneHades::initializeBattleRuntime()`, replace:

```cpp
applyBattleInitializationSceneResult(created.sceneInitialization);
```

with:

```cpp
KysChess::ChessCombo::getMutableStates() = created.sceneInitialization.comboStates;
scene_units_.initialize(created.sceneInitialization);
```

- [ ] **Step 5: Replace direct helper calls with store/math calls**

Apply these replacements in `src/BattleSceneHades.cpp`:

```text
requireRenderUnit(unitId) -> scene_units_.requireUnit(unitId)
requireSetupUnit(unitId) -> scene_units_.requireUnit(unitId)
swapSetupUnitPositions(a, b) -> scene_units_.swapSetupUnitPositions(a, b, [this](int x, int y) { return battle_map_.pos45To90(x, y); })
makeSetupPlacementInput() -> scene_units_.makeSetupPlacementInput()
makeComboBattleUnitRefs() -> scene_units_.makeComboBattleUnitRefs()
facingTowardNearestEnemy(unitId) -> scene_units_.facingTowardNearestEnemy(unitId)
aliveRenderUnitsOnTeam(team) -> scene_units_.aliveUnitsOnTeam(team)
render_units_ -> scene_units_.units()
ally_unit_ids_ -> scene_units_.allyUnitIds()
calRenderUnitPic(unit, style, frame) -> BattleSceneRenderMath::calRenderUnitPic(unit.fightFrames, unit.realTowards, style, frame)
calculateHurtFlashColor(unitId, color) -> BattleSceneRenderMath::hurtFlashColor(timer, color)
decreaseToZero(value) -> BattleSceneRenderMath::decreaseToZero(value)
realTowardsFromFaceTowards(faceTowards) -> BattleSceneRenderMath::realTowardsFromFaceTowards(faceTowards)
```

For hurt flash, keep the existing timer lookup in Hades and call pure math:

```cpp
Color BattleSceneHades::calculateHurtFlashColor(int unitId, const Color& baseColor) const
{
    auto it = hurt_flash_timers_.find(unitId);
    const int timer = it != hurt_flash_timers_.end() ? it->second : 0;
    return BattleSceneRenderMath::hurtFlashColor(timer, baseColor);
}
```

Keep this wrapper only if it avoids repeated map lookup code in draw. If all callers are local, make it an anonymous-namespace helper instead of a class method.

- [ ] **Step 6: Delete moved Hades helper definitions**

Delete these definitions from `src/BattleSceneHades.cpp` after replacements compile:

```cpp
BattleSceneHades::requireRenderUnit
BattleSceneHades::requireSetupUnit
BattleSceneHades::realTowardsFromFaceTowards
BattleSceneHades::decreaseToZero
BattleSceneHades::applyBattleInitializationSceneResult
BattleSceneHades::swapSetupUnitPositions
BattleSceneHades::makeComboBattleUnitRefs
BattleSceneHades::facingTowardNearestEnemy
BattleSceneHades::calRenderUnitPic
BattleSceneHades::aliveRenderUnitsOnTeam
```

- [ ] **Step 7: Replace post-battle summary**

Replace `BattleSceneHades::makePostBattleSummary()` body with:

```cpp
BattlePostBattleSummary BattleSceneHades::makePostBattleSummary() const
{
    return scene_units_.makePostBattleSummary(tracker_, result_);
}
```

- [ ] **Step 8: Build game and tests**

Run:

```powershell
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe "BattleSceneUnitStore*"
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected: store tests pass and both targets compile.

---

### Task 5: Remove Remaining Copying Artifacts And Boundary Leaks

**Files:**
- Modify: `src/BattleSceneHades.cpp`
- Modify: `src/BattleSceneHades.h`
- Modify: `src/BattleSceneBattleAdapter.cpp`
- Modify: `src/BattleSceneBattleAdapter.h`

- [ ] **Step 1: Run copying-artifact search**

Run:

```powershell
rg -n "BattleSceneSetupUnit|BattleSceneRenderUnit|BattleInitializedSceneUnit|setup_units_|render_units_|setup_unit_index_by_id_|render_unit_index_by_id_|unit_identities_|active_unit_ids_|ally_unit_ids_|applyBattleInitializationSceneResult|swapSetupUnitPositions|makeComboBattleUnitRefs|facingTowardNearestEnemy|aliveRenderUnitsOnTeam|calRenderUnitPic\\(" src\BattleSceneHades.cpp src\BattleSceneHades.h src\BattleSceneBattleAdapter.cpp src\BattleSceneBattleAdapter.h src\BattleSceneUnitStore.*
```

Expected:

- No `BattleSceneSetupUnit`.
- No `BattleSceneRenderUnit`.
- No `BattleInitializedSceneUnit`.
- No old Hades unit containers.
- Store-owned method names only appear in `BattleSceneUnitStore.*` and Hades call sites.

- [ ] **Step 2: Delete obsolete constants from Hades**

In `src/BattleSceneHades.cpp`, delete constants now owned by `BattleSceneRenderMath.h`:

```cpp
constexpr int HURT_FLASH_PERIOD = 3;
constexpr int RENDER_FIGHT_STYLE_COUNT = 5;
constexpr int RENDER_FIGHT_STYLE_FALLBACKS[RENDER_FIGHT_STYLE_COUNT][RENDER_FIGHT_STYLE_COUNT - 1] = { ... };
```

Keep `HURT_FLASH_DURATION` in Hades because it controls scene effect duration, not math.

- [ ] **Step 3: Verify no extra setup copying remains in Hades**

Run:

```powershell
rg -n "BattleSceneUnit unit|BattleSceneUnit render|BattleSceneUnit setup|setup\\.|render\\." src\BattleSceneHades.cpp
```

Expected: no Hades-side construction of scene unit rows. Hades may still mutate fields on `scene_units_.requireUnit(...)` during frame application.

- [ ] **Step 4: Run full verification**

Run:

```powershell
git diff --check
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys_tests -Verbosity minimal
x64\Debug\kys_tests.exe
.github\build-command.ps1 -Solution "D:\projects\kys-cpp\kys-cpp\kys.sln" -Configuration Debug -Platform x64 -Target kys -Verbosity minimal
```

Expected:

- No whitespace errors.
- All tests pass.
- `kys` debug target compiles, or final link is blocked only if the executable is running.

---

## Final Acceptance Criteria

- `BattleSceneHades` has one field for scene unit state: `BattleSceneUnitStore scene_units_;`.
- Hades no longer declares or owns `BattleSceneSetupUnit`, `BattleSceneRenderUnit`, unit id maps, active id vectors, or ally id vectors.
- Adapter initialization output directly uses `BattleSceneUnit`.
- `BattleSceneRenderMath` has unit tests for fight-style math, facing conversion, decrementing, and hurt flash.
- `BattleSceneUnitStore` has unit tests for initialization, lookup, setup swap, placement input, combo refs, nearest-enemy facing, and alive counts.
- Full build and test commands pass.

## Self-Review Notes

- Scope is one subsystem: scene unit state and render math extraction.
- The plan avoids adding builder classes; it deletes two scene row types and replaces them with one canonical row.
- The only remaining pre-runtime setup DTO is `BattleSetupRoleSnapshot`, which is still needed because it reads legacy `Role`/`Magic` before runtime initialization.
- The plan keeps `BattleSceneBattleAdapter` as runtime creation glue for now; deleting or renaming it is a later cleanup after this store boundary is stable.
