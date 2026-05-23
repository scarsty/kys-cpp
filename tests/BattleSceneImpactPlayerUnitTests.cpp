#include "BattleSceneImpactPlayer.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneImpactPlayer_PlansNormalHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleVisualEvent hit;
    hit.type = KysChess::Battle::BattleVisualEventType::ProjectileHit;
    hit.targetUnitId = 1;
    hit.effectId = 10;
    hit.impactUnitShake = 5;
    hit.impactRumble = true;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitId == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_PlansUltimateHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleVisualEvent hit;
    hit.type = KysChess::Battle::BattleVisualEventType::ProjectileHit;
    hit.targetUnitId = 1;
    hit.effectId = 10;
    hit.impactUnitShake = 10;
    hit.impactSceneShake = 10;
    hit.impactRumble = true;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 10);
    CHECK(commands[0].sceneShake == 10);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_ScriptedImpactOnlyShakesUnit", "[battle][scene_impact]")
{
    KysChess::Battle::BattleVisualEvent hit;
    hit.type = KysChess::Battle::BattleVisualEventType::ProjectileHit;
    hit.targetUnitId = 1;
    hit.effectId = 10;
    hit.impactUnitShake = 5;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK_FALSE(commands[0].rumble);
}
