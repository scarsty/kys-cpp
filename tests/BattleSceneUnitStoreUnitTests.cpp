#include "BattleSceneUnitStore.h"
#include "BattleReport.h"
#include "BattleSceneTestRuntimeFixture.h"

#include "ChessBattleEffects.h"

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

    CHECK(fixture.store.requirePresentation(0).unitId == 0);
    CHECK(fixture.store.requirePresentation(1).unitId == 1);
    CHECK(fixture.session.requireRuntimeUnit(0).identity().realRoleId == 1000);
    CHECK(fixture.session.requireRuntimeUnit(1).identity().team == 1);
}

TEST_CASE("BattleSceneUnitStore_IndexesPresentationByNonZeroUnitId", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(1, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(2, 1, 5, 6, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));

    CHECK(fixture.store.requirePresentation(1).unitId == 1);
    CHECK(fixture.store.requirePresentation(2).unitId == 2);
}

TEST_CASE("BattleSceneUnitStore_IndexesPresentationByNonContiguousUnitId", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(2, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(7, 1, 5, 6, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));

    CHECK(fixture.store.requirePresentation(2).unitId == 2);
    CHECK(fixture.store.requirePresentation(7).unitId == 7);
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

TEST_CASE("BattleSceneUnitStore_OwnsPresentationFightFrames", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(3, 0, 1, 2, {10, 20, 0}));
    StoreFixture fixture(std::move(units));

    fixture.store.setFightFrames(3, {2, 4, 6, 8, 10});

    CHECK(fixture.store.requirePresentation(3).fightFrames == std::array<int, 5>{2, 4, 6, 8, 10});
    CHECK(fixture.store.requireRuntimeUnit(3).fightFrames == std::array<int, 5>{});
}

TEST_CASE("BattleRuntimeSession_SwapsSetupPositionInsideRuntime", "[battle][scene_unit_store]")
{
    std::vector<KysChess::Battle::BattleSetupUnitInput> units;
    units.push_back(makeSetupUnit(0, 0, 1, 2, { 10, 20, 0 }));
    units.push_back(makeSetupUnit(1, 0, 3, 4, { 30, 40, 0 }));

    StoreFixture fixture(std::move(units));
    fixture.session.swapSetupUnitPositions(0, 1);

    const auto& first = fixture.session.runtime().units.requireCore(0);
    const auto& second = fixture.session.runtime().units.requireCore(1);
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
    input.units[0].baseCombo.applyConfiguredEffect({ KysChess::EffectType::CloneSummon, 1 });
    input.setup.cloneSources.push_back({ 0, 1000, 190, 1, -1, 0 });
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    auto creation = KysChess::Battle::BattleRuntimeSession::createInitialized(input);
    auto session = std::move(creation.session);
    BattleSceneUnitStore store;
    store.initialize(session);

    const auto cloneUnitId = 1;
    const auto& cloneUnit = session.runtime().units.requireCore(cloneUnitId);
    CHECK(cloneUnit.cloneSourceUnitId == 0);
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
