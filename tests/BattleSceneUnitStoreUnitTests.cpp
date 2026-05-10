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
    unit.position = position;
    unit.realTowards = { 1, 1, 0 };
    unit.maxHp = 100;
    unit.hp = 100;
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

TEST_CASE("BattleSceneUnitStore_UsesSharedUnitValueObjects", "[battle][scene_unit_store]")
{
    BattleSceneUnit unit;
    unit.hp = 10;
    unit.maxHp = 20;
    unit.mp = 3;
    unit.maxMp = 8;
    unit.attack = 30;
    unit.defence = 40;
    unit.speed = 50;
    unit.position = { 1, 2, 3 };
    unit.velocity = { 4, 5, 6 };
    unit.acceleration = { 7, 8, 9 };
    unit.realTowards = { 10, 11, 12 };
    unit.cooldown = 13;
    unit.cooldownMax = 14;
    unit.actFrame = 15;
    unit.actType = 16;

    syncBattleSceneUnitSharedValueObjects(unit);

    CHECK(unit.vitals.hp == 10);
    CHECK(unit.vitals.maxHp == 20);
    CHECK(unit.vitals.mp == 3);
    CHECK(unit.vitals.maxMp == 8);
    CHECK(unit.stats.attack == 30);
    CHECK(unit.stats.defence == 40);
    CHECK(unit.stats.speed == 50);
    CHECK(unit.motion.position.x == 1);
    CHECK(unit.motion.velocity.y == 5);
    CHECK(unit.motion.acceleration.z == 9);
    CHECK(unit.motion.facing.x == 10);
    CHECK(unit.animation.cooldown == 13);
    CHECK(unit.animation.cooldownMax == 14);
    CHECK(unit.animation.actFrame == 15);
    CHECK(unit.animation.actType == 16);
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
    CHECK(store.requireUnit(0).position.x == 30);
    CHECK(store.requireUnit(0).position.y == 40);
    CHECK(store.requireUnit(1).gridX == 1);
    CHECK(store.requireUnit(1).gridY == 2);
    CHECK(store.requireUnit(1).position.x == 10);
    CHECK(store.requireUnit(1).position.y == 20);
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
