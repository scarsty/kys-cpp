#include "BattleSceneFrameDeltaBuilder.h"
#include "BattleSceneTestRuntimeFixture.h"
#include "ChessCombo.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <unordered_map>

namespace
{
BattleSceneFrameDeltaBuildContext testApplyContext(
    BattleSceneUnitStore& units,
    std::map<int, KysChess::RoleComboState>& comboStates,
    std::unordered_map<int, int>& hurtFlashTimers,
    RandomDouble& random)
{
    BattleSceneFrameDeltaBuildContext context;
    context.units = &units;
    context.comboStates = &comboStates;
    context.hurtFlashTimers = &hurtFlashTimers;
    context.random = &random;
    context.transferAntiCombo = [](int) {};
    context.hurtFlashDuration = 15;
    context.blinkSoundEffectId = 11;
    context.deathZoomFrames = 30;
    context.battleEndZoomFrames = 30;
    context.deathSlowFrames = 10;
    context.battleEndSlowFrames = 30;
    return context;
}
}  // namespace

TEST_CASE("BattleSceneFrameDeltaBuilder_CollectsDamageBeforeDeathEffects", "[battle][scene_frame_delta]")
{
    BattleSceneTest::StoreFixture fixture({
        BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 }),
        BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 }),
    });
    std::map<int, KysChess::RoleComboState> comboStates;
    comboStates[1].onSkillTeamHealPending = true;
    std::unordered_map<int, int> hurtFlashTimers;
    RandomDouble random;
    random.set_seed(1);

    KysChess::Battle::BattleFrameResult frame;
    frame.damageRenderApplications.push_back({
        { 1, 0, 0, 0, false },
        { 0, 100, 20, 0, true },
        12,
        30,
        7,
        44,
        true,
        false,
    });
    frame.frame.gameplayEvents.push_back({
        KysChess::Battle::BattleGameplayEventType::UnitDied,
        90,
        0,
        1,
    });

    auto result = BattleSceneFrameDeltaBuilder().build(
        frame,
        -1,
        testApplyContext(fixture.store, comboStates, hurtFlashTimers, random));

    CHECK_FALSE(comboStates[1].onSkillTeamHealPending);
    CHECK(hurtFlashTimers.at(1) == 15);
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
    std::map<int, KysChess::RoleComboState> comboStates;
    std::unordered_map<int, int> hurtFlashTimers;
    RandomDouble random;

    KysChess::Battle::BattleFrameResult frame;
    KysChess::Battle::BattleRescueRepositionResult rescue;
    rescue.teleport = KysChess::Battle::BattleRescueTeleportDelta{
        0,
        1,
        { 3, 4 },
        {},
    };
    rescue.heal = { 0, 25 };
    rescue.invincibility = { 0, 120 };
    frame.rescueResults.push_back(rescue);

    BattleSceneFrameDeltaBuilder().build(
        frame,
        -1,
        testApplyContext(fixture.store, comboStates, hurtFlashTimers, random));

    const auto& rescued = fixture.store.requireRuntimeUnit(0);
    CHECK(rescued.grid.x == -32);
    CHECK(rescued.grid.y == 32);
    CHECK(rescued.vitals.hp == 100);
    CHECK(rescued.invincible == 0);
}

TEST_CASE("BattleSceneFrameDeltaBuilder_ReturnsBattleEndSideEffects", "[battle][scene_frame_delta]")
{
    BattleSceneTest::StoreFixture fixture({
        BattleSceneTest::makeSetupUnit(0, 0, 0, 0, { 0, 0, 0 }),
        BattleSceneTest::makeSetupUnit(1, 1, 1, 0, { 10, 0, 0 }),
    });
    std::map<int, KysChess::RoleComboState> comboStates;
    std::unordered_map<int, int> hurtFlashTimers;
    RandomDouble random;

    KysChess::Battle::BattleFrameResult frame;
    frame.frame.gameplayEvents.push_back({
        KysChess::Battle::BattleGameplayEventType::BattleEnded,
        100,
        -1,
        -1,
        0,
    });

    auto result = BattleSceneFrameDeltaBuilder().build(
        frame,
        -1,
        testApplyContext(fixture.store, comboStates, hurtFlashTimers, random));

    CHECK(result.battleEnded);
    CHECK(result.battleResult == 0);
    CHECK(result.closeUpFrames == 30);
    CHECK(result.frozenFrames == 60);
    CHECK(result.slowFrames == 30);
    CHECK(result.sceneShake == 60);
}