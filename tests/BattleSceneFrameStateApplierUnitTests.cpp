#include "BattleSceneFrameStateApplier.h"
#include "ChessCombo.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <unordered_map>

namespace
{
BattleSceneUnit sceneStateUnit(int unitId, int team, int hp = 100)
{
    BattleSceneUnit unit;
    unit.unitId = unitId;
    unit.identity = { 100 + unitId, 100 + unitId, team, 0, "unit" };
    unit.vitals.hp = hp;
    unit.vitals.maxHp = 100;
    unit.vitals.mp = 0;
    unit.vitals.maxMp = 100;
    unit.motion.position = { float(unitId * 10), 0, 0 };
    return unit;
}

BattleSceneFrameStateApplyContext testApplyContext(
    BattleSceneUnitStore& units,
    std::map<int, KysChess::RoleComboState>& comboStates,
    std::unordered_map<int, int>& hurtFlashTimers,
    RandomDouble& random)
{
    BattleSceneFrameStateApplyContext context;
    context.units = &units;
    context.comboStates = &comboStates;
    context.hurtFlashTimers = &hurtFlashTimers;
    context.random = &random;
    context.pos45To90 = [](int x, int y)
    {
        return Pointf{ float(x * 100), float(y * 100), 0 };
    };
    context.transferAntiCombo = [](int) {};
    context.gravity = -4.0;
    context.hurtFlashDuration = 15;
    context.blinkSoundEffectId = 11;
    context.deathZoomFrames = 30;
    context.battleEndZoomFrames = 30;
    context.deathSlowFrames = 10;
    context.battleEndSlowFrames = 30;
    return context;
}
}  // namespace

TEST_CASE("BattleSceneFrameStateApplier_AppliesDamageBeforeDeathState", "[battle][scene_frame_applier]")
{
    BattleSceneUnitStore units;
    units.initialize({
        sceneStateUnit(0, 0),
        sceneStateUnit(1, 1),
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

    auto result = BattleSceneFrameStateApplier().apply(
        frame,
        -1,
        testApplyContext(units, comboStates, hurtFlashTimers, random));

    const auto& defender = units.requireUnit(1);
    CHECK(defender.vitals.hp == 0);
    CHECK_FALSE(defender.alive);
    CHECK(defender.frozen == 12);
    CHECK(defender.frozenMax == 30);
    CHECK(defender.animation.cooldown == 7);
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

TEST_CASE("BattleSceneFrameStateApplier_AppliesRescueTeleportHealAndInvincibility", "[battle][scene_frame_applier]")
{
    BattleSceneUnitStore units;
    auto pulled = sceneStateUnit(0, 0, 30);
    auto puller = sceneStateUnit(1, 0);
    auto enemy = sceneStateUnit(2, 1);
    enemy.motion.position = { 500, 0, 0 };
    units.initialize({ pulled, puller, enemy });
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

    BattleSceneFrameStateApplier().apply(
        frame,
        -1,
        testApplyContext(units, comboStates, hurtFlashTimers, random));

    const auto& rescued = units.requireUnit(0);
    CHECK(rescued.gridX == 3);
    CHECK(rescued.gridY == 4);
    CHECK(rescued.motion.position.x == 300.0f);
    CHECK(rescued.motion.position.y == 400.0f);
    CHECK(rescued.motion.velocity.x == 0.0f);
    CHECK(rescued.motion.velocity.y == 0.0f);
    CHECK(rescued.motion.velocity.z == 0.0f);
    CHECK(rescued.motion.acceleration.z == -4.0f);
    CHECK(rescued.vitals.hp == 55);
    CHECK(rescued.invincible == 120);
}

TEST_CASE("BattleSceneFrameStateApplier_ReturnsBattleEndSideEffects", "[battle][scene_frame_applier]")
{
    BattleSceneUnitStore units;
    units.initialize({
        sceneStateUnit(0, 0),
        sceneStateUnit(1, 1),
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

    auto result = BattleSceneFrameStateApplier().apply(
        frame,
        -1,
        testApplyContext(units, comboStates, hurtFlashTimers, random));

    CHECK(result.battleEnded);
    CHECK(result.battleResult == 0);
    CHECK(result.closeUpFrames == 30);
    CHECK(result.frozenFrames == 60);
    CHECK(result.slowFrames == 30);
    CHECK(result.sceneShake == 60);
}
