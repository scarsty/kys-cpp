#include "battle/BattleRuntimeRules.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double SceneProjectileSpeed = SceneTileWidth / 3.0;
constexpr int BattleCoordCount = 64;

}  // namespace

TEST_CASE("BattleRuntimeRules_HadesRulesDeriveCurrentSceneValuesFromGrid")
{
    const auto rules = makeHadesBattleRuntimeRules(SceneTileWidth, BattleCoordCount);

    REQUIRE(rules.gridTransform.tileWidth == SceneTileWidth);
    REQUIRE(rules.gridTransform.coordCount == BattleCoordCount);
    REQUIRE(rules.projectileFollowUps.projectileSpeed == SceneProjectileSpeed);
    REQUIRE(rules.projectileFollowUps.minimumProjectileFrames == 20);
    REQUIRE(rules.projectileFollowUps.nearbyProjectileFramePadding == 18);
    REQUIRE(rules.projectileFollowUps.areaProjectileFramePadding == 15);
    REQUIRE(rules.projectileFollowUps.areaSpawnDistance == SceneTileWidth * 1.5);
    REQUIRE(rules.rescueCounterAttack.skillId == 1);
    REQUIRE(rules.rescueCounterAttack.projectileSpeed == SceneProjectileSpeed);
    REQUIRE(rules.rescueCounterAttack.meleeAttackEffectOffset == SceneTileWidth * 2.0);
    REQUIRE(rules.castConfig.recoveryFrames[0] == 4);
    REQUIRE(rules.castConfig.recoveryFrames[3] == 5);
    REQUIRE(rules.movementConfig.dashFrames == 5);
    REQUIRE(rules.action.heavyAttackReach == SceneTileWidth * 4.0);
    REQUIRE(rules.action.projectileBounceRange == 90);
}
