#include "BattleSceneImpactPlayer.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneImpactPlayer_PlansNormalHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.operationType = KysChess::Battle::BattleOperationType::RangedProjectile;
    hit.totalFrame = 30;
    hit.ultimate = false;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitId == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_PlansUltimateHitShake", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.operationType = KysChess::Battle::BattleOperationType::TrackingProjectile;
    hit.totalFrame = 30;
    hit.ultimate = true;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 10);
    CHECK(commands[0].sceneShake == 10);
    CHECK(commands[0].rumble);
}

TEST_CASE("BattleSceneImpactPlayer_ScriptedImpactOnlyShakesUnit", "[battle][scene_impact]")
{
    KysChess::Battle::BattleAttackEvent hit;
    hit.type = KysChess::Battle::BattleAttackEventType::Hit;
    hit.unitId = 1;
    hit.scriptedDamage = 12;

    auto commands = BattleSceneImpactPlanner().plan({ hit });

    REQUIRE(commands.size() == 1);
    CHECK(commands[0].unitShake == 5);
    CHECK(commands[0].sceneShake == 0);
    CHECK_FALSE(commands[0].rumble);
}
