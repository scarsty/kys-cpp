#include "BattleSceneUnitStore.h"
#include "BattleSceneTestRuntimeFixture.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

using BattleSceneTest::StoreFixture;
using BattleSceneTest::makeSetupUnit;

TEST_CASE("BattleSceneUnitStore_InitializesDenseRowsAndRequiresByUnitId", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(1, 1, 5, 6, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));

    CHECK(fixture.store.requirePresentation(0).identity.realRoleId == 1000);
    CHECK(fixture.store.requirePresentation(1).identity.team == 1);
    CHECK(fixture.store.aliveUnitsOnTeam(0) == 1);
    CHECK(fixture.store.aliveUnitsOnTeam(1) == 1);
}

TEST_CASE("BattleSceneUnitStore_ReadsRuntimeFieldsFromSession", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    auto unit = makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 });
    unit.vitals = { 75, 100, 4, 10 };
    unit.stats = { 30, 40, 50 };
    unit.motion = { { 10, 20, 0 }, { 1, 2, 3 }, { 0, 0, 9 }, { 1, 0, 0 } };
    unit.animation = { 3, 5, 7, 9 };
    units.push_back(unit);

    StoreFixture fixture(std::move(units));

    const auto& stored = fixture.store.requireRuntimeUnit(0);
    CHECK(stored.vitals.maxHp == 100);
    CHECK(stored.stats.attack == 30);
    CHECK(stored.motion.position.x == 10);
    CHECK(stored.animation.actFrame == 7);
}

TEST_CASE("BattleSceneUnitStore_SwapsSetupPositionAndUpdatesWorldPosition", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(1, 0, 3, 4, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));
    fixture.store.swapSetupUnitPositions(0, 1);

    CHECK(fixture.store.requireSetupPlacement(0).gridX == 3);
    CHECK(fixture.store.requireSetupPlacement(0).gridY == 4);
    CHECK(fixture.store.requireSetupPlacement(0).position.x == 30);
    CHECK(fixture.store.requireSetupPlacement(0).position.y == 40);
    CHECK(fixture.store.requireSetupPlacement(1).gridX == 1);
    CHECK(fixture.store.requireSetupPlacement(1).gridY == 2);
    CHECK(fixture.store.requireSetupPlacement(1).position.x == 10);
    CHECK(fixture.store.requireSetupPlacement(1).position.y == 20);
}

TEST_CASE("BattleSceneUnitStore_BuildsSetupPlacementOnlyForActiveUnits", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    auto active = makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto inactive = makeSetupUnit(1, 0, 3, 4, { 30, 40, 0 });
    inactive.alive = false;
    units.push_back(active);
    units.push_back(inactive);

    StoreFixture fixture(std::move(units));
    auto placement = fixture.store.makeSetupPlacementInput();

    REQUIRE(placement.units.size() == 1);
    CHECK(placement.units[0].unitId == 0);
    CHECK(placement.units[0].x == 1);
    CHECK(placement.units[0].y == 2);
    CHECK(placement.units[0].faceTowards == Towards_RightDown);
}

TEST_CASE("BattleSceneUnitStore_BuildsComboRefsFromCanonicalRows", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    auto alive = makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 });
    auto dead = makeSetupUnit(1, 1, 3, 4, { 30, 40, 0 });
    dead.alive = false;
    units.push_back(alive);
    units.push_back(dead);

    StoreFixture fixture(std::move(units));
    auto refs = fixture.store.makeComboBattleUnitRefs();

    REQUIRE(refs.size() == 2);
    CHECK(refs[0].battleId == 0);
    CHECK(refs[0].alive);
    CHECK(refs[1].battleId == 1);
    CHECK(!refs[1].alive);
}

TEST_CASE("BattleSceneUnitStore_FacesTowardNearestAliveEnemy", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(0, 0, 1, 2, { 0, 0, 0 }));
    units.push_back(makeSetupUnit(1, 1, 3, 4, { 10, 0, 0 }));
    units.push_back(makeSetupUnit(2, 1, 5, 6, { 100, 0, 0 }));

    StoreFixture fixture(std::move(units));
    auto facing = fixture.store.facingTowardNearestEnemy(0);

    CHECK(facing.x == 1.0f);
    CHECK(facing.y == 0.0f);
}
