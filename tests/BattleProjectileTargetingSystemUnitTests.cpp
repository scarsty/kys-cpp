#include "battle/BattleProjectileTargetingSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleProjectileTargetUnit unit(int id, int team, double x, double y, int gridX, int gridY)
{
    BattleProjectileTargetUnit state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.x = x;
    state.y = y;
    state.gridX = gridX;
    state.gridY = gridY;
    return state;
}

BattleProjectileTargetUnit combatUnit(int id, int team, int hp, int maxHp, int defense, int invincible)
{
    auto state = unit(id, team, 0.0, 0.0, 0, 0);
    state.hp = hp;
    state.maxHp = maxHp;
    state.defense = defense;
    state.invincible = invincible;
    return state;
}

}  // namespace

TEST_CASE("BattleProjectileTargetingSystem_NearbyTargets_AreEnemiesSortedByDistance", "[battle][projectile][unit]")
{
    BattleProjectileTargetWorld world;
    world.units = {
        unit(1, 0, 0.0, 0.0, 0, 0),
        unit(2, 1, 100.0, 0.0, 1, 0),
        unit(3, 1, 20.0, 0.0, 0, 1),
        unit(4, 0, 10.0, 0.0, 0, 0),
        unit(5, 1, 500.0, 0.0, 5, 0),
    };

    auto ids = BattleProjectileTargetingSystem().selectNearbyTargets(world, 1, 2, 120.0);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 2);
    CHECK(ids[1] == 3);
}

TEST_CASE("BattleProjectileTargetingSystem_AreaImpact_UsesGridAreaLimitAndTrackedTarget", "[battle][projectile][unit]")
{
    BattleProjectileTargetWorld world;
    world.units = {
        unit(1, 0, 0.0, 0.0, 0, 0),
        unit(2, 1, 10.0, 0.0, 1, 0),
        unit(3, 1, 20.0, 0.0, 2, 0),
        unit(4, 1, 300.0, 0.0, 9, 9),
        unit(5, 0, 5.0, 0.0, 1, 1),
    };

    auto ids = BattleProjectileTargetingSystem().selectAreaImpactTargets(world, 1, 2, 1, 4);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 2);
    CHECK(ids[1] == 4);
}

TEST_CASE("BattleProjectileTargetingSystem_AreaImpact_DoesNotDuplicateTrackedTarget", "[battle][projectile][unit]")
{
    BattleProjectileTargetWorld world;
    world.units = {
        unit(1, 0, 0.0, 0.0, 0, 0),
        unit(2, 1, 10.0, 0.0, 1, 0),
        unit(3, 1, 20.0, 0.0, 2, 0),
    };

    auto ids = BattleProjectileTargetingSystem().selectAreaImpactTargets(world, 1, 2, 0, 2);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 2);
    CHECK(ids[1] == 3);
}

TEST_CASE("BattleProjectileTargetingSystem_RandomEnemy_UsesExplicitIndex", "[battle][projectile][unit]")
{
    BattleProjectileTargetWorld world;
    world.units = {
        unit(1, 0, 0.0, 0.0, 0, 0),
        unit(2, 1, 10.0, 0.0, 1, 0),
        unit(3, 1, 20.0, 0.0, 2, 0),
        unit(4, 0, 30.0, 0.0, 3, 0),
    };

    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(world, 0, 0) == 2);
    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(world, 0, 1) == 3);
    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(world, 0, 2) == 2);
}

TEST_CASE("BattleProjectileTargetingSystem_WeakestVulnerableEnemy_UsesEffectiveHp", "[battle][projectile][unit]")
{
    BattleProjectileTargetWorld world;
    world.units = {
        combatUnit(1, 0, 100, 100, 0, 0),
        combatUnit(2, 1, 60, 200, 1, 0),
        combatUnit(3, 1, 70, 100, 20, 0),
        combatUnit(4, 1, 5, 100, 0, 3),
        combatUnit(5, 1, 1, 100, 0, 0),
    };
    world.units[4].alive = false;

    CHECK(BattleProjectileTargetingSystem().selectWeakestVulnerableEnemy(world, 0, 4.0) == 3);
    CHECK(BattleProjectileTargetingSystem().selectWeakestVulnerableEnemy(world, 1, 4.0) == 1);
}
