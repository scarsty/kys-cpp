#include "BattleSceneFrameDeltaBuilder.h"
#include "BattleSceneTestRuntimeFixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
BattleSceneFrameDeltaBuildContext testApplyContext(
    BattleSceneUnitStore& units,
    RandomDouble& random)
{
    BattleSceneFrameDeltaBuildContext context;
    context.units = &units;
    context.random = &random;
    context.hurtFlashDuration = 15;
    context.blinkSoundEffectId = 11;
    context.deathZoomFrames = 30;
    context.battleEndZoomFrames = 30;
    context.deathSlowFrames = 10;
    context.battleEndSlowFrames = 30;
    return context;
}
}  // namespace

TEST_CASE("BattleSceneFrameDeltaBuilder_CollectsDeathPresentationEffects", "[battle][scene_frame_delta]")
{
    BattleSceneTest::StoreFixture fixture({
        BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 }),
        BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 }),
    });
    RandomDouble random;
    random.set_seed(1);

    KysChess::Battle::BattlePresentationFrame frame;
    KysChess::Battle::BattleFrameApplications applications;
    frame.logEvents.push_back({
        KysChess::Battle::BattleLogEventType::Damage,
        90,
        0,
        1,
        44,
    });
    frame.gameplayEvents.push_back({
        KysChess::Battle::BattleGameplayEventType::UnitDied,
        90,
        0,
        1,
    });

    auto result = BattleSceneFrameDeltaBuilder().build(
        frame,
        applications,
        -1,
        testApplyContext(fixture.store, random));

    REQUIRE(result.hurtFlashes.size() == 1);
    CHECK(result.hurtFlashes[0].unitId == 1);
    CHECK(result.hurtFlashes[0].frames == 15);
    REQUIRE(result.bloodEffects.size() == 1);
    CHECK(result.bloodEffects[0].followUnitId == 1);
    CHECK(result.unitDied);
    CHECK(result.diedUnitIds == std::vector<int>{ 1 });
    CHECK(result.sceneShake == 10);
    CHECK(result.frozenFrames == 5);
    CHECK(result.slowFrames == 10);
    REQUIRE(result.cameraFocus);
}

TEST_CASE("BattleSceneFrameDeltaBuilder_DoesNotReplayRescueRuntimeMutations", "[battle][scene_frame_delta]")
{
    BattleSceneTest::StoreFixture fixture({
        BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 }, 30),
        BattleSceneTest::makeSetupUnit(1, 0, 1, 0, { 10, 0, 0 }),
        BattleSceneTest::makeSetupUnit(2, 1, 5, 0, { 500, 0, 0 }),
    });
    RandomDouble random;

    KysChess::Battle::BattlePresentationFrame frame;
    KysChess::Battle::BattleFrameApplications applications;

    BattleSceneFrameDeltaBuilder().build(
        frame,
        applications,
        -1,
        testApplyContext(fixture.store, random));

    const auto& rescued = fixture.store.requireRuntimeUnit(0);
    CHECK(rescued.grid.x == 0);
    CHECK(rescued.grid.y == 0);
    CHECK(rescued.vitals.hp == 100);
    CHECK(rescued.invincible == 0);
}

TEST_CASE("BattleSceneFrameDeltaBuilder_ReturnsBattleEndSideEffects", "[battle][scene_frame_delta]")
{
    BattleSceneTest::StoreFixture fixture({
        BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 }),
        BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 }),
    });
    RandomDouble random;

    KysChess::Battle::BattlePresentationFrame frame;
    KysChess::Battle::BattleFrameApplications applications;
    frame.gameplayEvents.push_back({
        KysChess::Battle::BattleGameplayEventType::BattleEnded,
        100,
        -1,
        -1,
        0,
    });

    auto result = BattleSceneFrameDeltaBuilder().build(
        frame,
        applications,
        -1,
        testApplyContext(fixture.store, random));

    CHECK(result.battleEnded);
    CHECK(result.battleResult == 0);
    CHECK(result.closeUpFrames == 30);
    CHECK(result.frozenFrames == 60);
    CHECK(result.slowFrames == 30);
    CHECK(result.sceneShake == 60);
}
