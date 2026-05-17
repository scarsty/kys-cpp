#include "BattleSceneUnitStore.h"
#include "BattleReport.h"
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

TEST_CASE("BattleRuntimeSession_SwapsSetupPositionInsideRuntime", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(1, 0, 3, 4, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));
    fixture.session.swapSetupUnitPositions(0, 1);

    const auto& first = fixture.session.runtime().unitStore.requireUnit(0);
    const auto& second = fixture.session.runtime().unitStore.requireUnit(1);
    CHECK(first.grid.x == 3);
    CHECK(first.grid.y == 4);
    CHECK(first.motion.position.x == 30);
    CHECK(first.motion.position.y == 40);
    CHECK(second.grid.x == 1);
    CHECK(second.grid.y == 2);
    CHECK(second.motion.position.x == 10);
    CHECK(second.motion.position.y == 20);
}

TEST_CASE("BattleSceneUnitStore_InitializesSummonedClonePlacementAndSummary", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    auto source = makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 }, 120);
    source.stats = { 30, 40, 50 };
    units.push_back(source);

    auto input = BattleSceneTest::makeSessionInput(std::move(units));
    input.actionPlanSeeds.push_back({ 0, false, {}, {} });
    input.setup.cloneSummonCount = 1;
    input.setup.cloneSources.push_back({ 0, 1000, 190, 1, -1, 0 });
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    auto creation = KysChess::Battle::BattleRuntimeSession::createInitialized(input);
    auto session = std::move(creation.session);
    BattleSceneUnitStore store;
    store.initializeFromRuntimeCreation(session, input);

    const auto cloneUnitId = 1;
    const auto& cloneUnit = session.runtime().unitStore.requireUnit(cloneUnitId);
    CHECK(cloneUnit.presentationSourceUnitId == 0);
    CHECK(cloneUnit.vitals.maxHp > 0);
    CHECK(cloneUnit.stats.attack > 0);
    CHECK(cloneUnit.grid.x == 3);
    CHECK(cloneUnit.grid.y == 4);

    BattleReport report;
    const auto summary = store.makePostBattleSummary(report, 0);
    REQUIRE(summary.allies.size() == 2);
    CHECK(summary.allies[1].identity.battleId == cloneUnitId);
    CHECK(summary.allies[1].hp == cloneUnit.vitals.maxHp);
    CHECK(summary.allies[1].attack == cloneUnit.stats.attack);
    CHECK(summary.allies[1].defence == cloneUnit.stats.defence);
    CHECK(summary.allies[1].speed == cloneUnit.stats.speed);
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
