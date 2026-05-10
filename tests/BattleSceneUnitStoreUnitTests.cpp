#include "BattleSceneUnitStore.h"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <utility>
#include <vector>

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
    unit.vitals = { 100, 100, 0, 0 };
    unit.motion = { position, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    unit.animation = { 0, 0, 0, -1 };
    unit.cost = 2;
    return unit;
}
}

TEST_CASE("BattleSceneUnitStore_InitializesDenseRowsAndRequiresByUnitId", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    units.push_back(makeUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeUnit(1, 1, 5, 6, { 30, 40, 0 }));

    BattleSceneUnitStore store;
    store.initialize(std::move(units));

    CHECK(store.requireUnit(0).identity.realRoleId == 1000);
    CHECK(store.requireUnit(1).identity.team == 1);
    CHECK(store.aliveUnitsOnTeam(0) == 1);
    CHECK(store.aliveUnitsOnTeam(1) == 1);
}

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

TEST_CASE("BattleSceneUnitStore_SwapsSetupPositionAndUpdatesWorldPosition", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    units.push_back(makeUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeUnit(1, 0, 3, 4, { 30, 40, 0 }));

    BattleSceneUnitStore store;
    store.initialize(std::move(units));
    store.swapSetupUnitPositions(
        0,
        1,
        [](int x, int y)
        {
            return Pointf{ static_cast<float>(x * 10), static_cast<float>(y * 10), 0 };
        });

    CHECK(store.requireUnit(0).gridX == 3);
    CHECK(store.requireUnit(0).gridY == 4);
    CHECK(store.requireUnit(0).motion.position.x == 30);
    CHECK(store.requireUnit(0).motion.position.y == 40);
    CHECK(store.requireUnit(1).gridX == 1);
    CHECK(store.requireUnit(1).gridY == 2);
    CHECK(store.requireUnit(1).motion.position.x == 10);
    CHECK(store.requireUnit(1).motion.position.y == 20);
}

TEST_CASE("BattleSceneUnitStore_BuildsSetupPlacementOnlyForActiveUnits", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    auto active = makeUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto inactive = makeUnit(1, 0, 3, 4, { 30, 40, 0 });
    inactive.active = false;
    units.push_back(active);
    units.push_back(inactive);

    BattleSceneUnitStore store;
    store.initialize(std::move(units));
    auto placement = store.makeSetupPlacementInput();

    REQUIRE(placement.units.size() == 1);
    CHECK(placement.units[0].unitId == 0);
    CHECK(placement.units[0].x == 1);
    CHECK(placement.units[0].y == 2);
    CHECK(placement.units[0].faceTowards == Towards_RightDown);
}

TEST_CASE("BattleSceneUnitStore_BuildsComboRefsFromCanonicalRows", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    auto alive = makeUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto dead = makeUnit(1, 1, 3, 4, { 30, 40, 0 });
    dead.alive = false;
    units.push_back(alive);
    units.push_back(dead);

    BattleSceneUnitStore store;
    store.initialize(std::move(units));
    auto refs = store.makeComboBattleUnitRefs();

    REQUIRE(refs.size() == 2);
    CHECK(refs[0].battleId == 0);
    CHECK(refs[0].alive);
    CHECK(refs[1].battleId == 1);
    CHECK(!refs[1].alive);
}

TEST_CASE("BattleSceneUnitStore_FacesTowardNearestAliveEnemy", "[battle][scene_unit_store]")
{
    std::vector<BattleSceneUnit> units;
    units.push_back(makeUnit(0, 0, 1, 2, { 0, 0, 0 }));
    units.push_back(makeUnit(1, 1, 3, 4, { 10, 0, 0 }));
    units.push_back(makeUnit(2, 1, 5, 6, { 100, 0, 0 }));

    BattleSceneUnitStore store;
    store.initialize(std::move(units));
    auto facing = store.facingTowardNearestEnemy(0);

    CHECK(facing.x == 1.0f);
    CHECK(facing.y == 0.0f);
}
