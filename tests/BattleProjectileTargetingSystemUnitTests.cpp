#include "battle/BattleCore.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleRuntimeUnit unit(int id, int team, double x, double y, int gridX, int gridY)
{
    BattleRuntimeUnit state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.motion.position = { static_cast<float>(x), static_cast<float>(y), 0.0f };
    state.grid = { gridX, gridY };
    return state;
}

BattleRuntimeUnit combatUnit(int id, int team, int hp, int maxHp, int defense, int invincible)
{
    auto state = unit(id, team, 0.0, 0.0, 0, 0);
    state.vitals.hp = hp;
    state.vitals.maxHp = maxHp;
    state.stats.defence = defense;
    state.invincible = invincible;
    return state;
}

}  // namespace

TEST_CASE("BattleProjectileTargetingSystem_NearbyTargets_AreEnemiesSortedByDistance", "[battle][projectile][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, 0.0, 0.0, 0, 0),
        unit(1, 1, 100.0, 0.0, 1, 0),
        unit(2, 1, 20.0, 0.0, 0, 1),
        unit(3, 0, 10.0, 0.0, 0, 0),
        unit(4, 1, 500.0, 0.0, 5, 0),
    };

    auto ids = BattleProjectileTargetingSystem().selectNearbyTargets(units, 0, 1, 120.0);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 2);
}

TEST_CASE("BattleProjectileTargetingSystem_AreaImpact_UsesGridAreaLimitAndTrackedTarget", "[battle][projectile][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, 0.0, 0.0, 0, 0),
        unit(1, 1, 10.0, 0.0, 1, 0),
        unit(2, 1, 20.0, 0.0, 2, 0),
        unit(3, 1, 300.0, 0.0, 9, 9),
        unit(4, 0, 5.0, 0.0, 1, 1),
    };

    auto ids = BattleProjectileTargetingSystem().selectAreaImpactTargets(units, 0, 2, 1, 3);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 3);
}

TEST_CASE("BattleProjectileTargetingSystem_AreaImpact_DoesNotDuplicateTrackedTarget", "[battle][projectile][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, 0.0, 0.0, 0, 0),
        unit(1, 1, 10.0, 0.0, 1, 0),
        unit(2, 1, 20.0, 0.0, 2, 0),
    };

    auto ids = BattleProjectileTargetingSystem().selectAreaImpactTargets(units, 0, 2, 0, 1);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == 1);
    CHECK(ids[1] == 2);
}

TEST_CASE("BattleProjectileTargetingSystem_RandomEnemy_UsesExplicitIndex", "[battle][projectile][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, 0.0, 0.0, 0, 0),
        unit(1, 1, 10.0, 0.0, 1, 0),
        unit(2, 1, 20.0, 0.0, 2, 0),
        unit(3, 0, 30.0, 0.0, 3, 0),
    };

    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(units, 0, 0) == 1);
    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(units, 0, 1) == 2);
    CHECK(BattleProjectileTargetingSystem().selectRandomEnemy(units, 0, 2) == 1);
}

TEST_CASE("BattleProjectileTargetingSystem_WeakestVulnerableEnemy_UsesEffectiveHp", "[battle][projectile][unit]")
{
    BattleUnitStore units;
    units.units = {
        combatUnit(0, 0, 100, 100, 0, 0),
        combatUnit(1, 1, 60, 200, 1, 0),
        combatUnit(2, 1, 70, 100, 20, 0),
        combatUnit(3, 1, 5, 100, 0, 3),
        combatUnit(4, 1, 1, 100, 0, 0),
    };
    units.units[4].alive = false;

    CHECK(BattleProjectileTargetingSystem().selectWeakestVulnerableEnemy(units, 0, 4.0) == 2);
    CHECK(BattleProjectileTargetingSystem().selectWeakestVulnerableEnemy(units, 1, 4.0) == 0);
}
