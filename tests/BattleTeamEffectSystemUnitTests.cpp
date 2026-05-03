#include "battle/BattleTeamEffectSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

namespace
{

BattleTeamEffectUnit unit(int id, int team, int hp, int mp, int shield = 0)
{
    BattleTeamEffectUnit state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.hp = hp;
    state.maxHp = 100;
    state.mp = mp;
    state.maxMp = 100;
    state.cooldown = 50;
    state.shield = shield;
    state.x = id * 10.0;
    state.y = 0.0;
    return state;
}

const BattleTeamEffectUnit& unitById(const BattleTeamEffectWorld& world, int id)
{
    for (const auto& unit : world.units)
    {
        if (unit.id == id)
        {
            return unit;
        }
    }
    FAIL("unit not found");
}

}  // namespace

TEST_CASE("BattleTeamEffectSystem_TeamHeal_HealsAliveSourceTeamOnly", "[battle][team_effect][unit]")
{
    BattleTeamEffectWorld world;
    world.units = {
        unit(1, 0, 40, 0),
        unit(2, 0, 95, 0),
        unit(3, 1, 20, 0),
        unit(4, 0, 10, 0),
    };
    world.units[3].alive = false;

    auto events = BattleTeamEffectSystem().applyTeamHeal(world, 1, 10, 20);

    REQUIRE(events.size() == 2);
    CHECK(events[0].targetUnitId == 1);
    CHECK(events[0].value == 30);
    CHECK(events[1].targetUnitId == 2);
    CHECK(events[1].value == 5);
    CHECK(unitById(world, 1).hp == 70);
    CHECK(unitById(world, 2).hp == 100);
    CHECK(unitById(world, 3).hp == 20);
    CHECK(unitById(world, 4).hp == 10);
}

TEST_CASE("BattleTeamEffectSystem_TeamMp_RespectsBlockBonusAndCap", "[battle][team_effect][unit]")
{
    BattleTeamEffectWorld world;
    world.units = {
        unit(1, 0, 100, 80),
        unit(2, 0, 100, 70),
        unit(3, 0, 100, 40),
        unit(4, 1, 100, 20),
    };
    world.units[1].mpRecoveryBonusPct = 50;
    world.units[2].mpBlocked = true;

    auto events = BattleTeamEffectSystem().applyTeamMp(world, 1, 20);

    REQUIRE(events.size() == 2);
    CHECK(unitById(world, 1).mp == 100);
    CHECK(unitById(world, 2).mp == 100);
    CHECK(unitById(world, 3).mp == 40);
    CHECK(unitById(world, 4).mp == 20);
    CHECK(events[0].value == 20);
    CHECK(events[1].value == 30);
}

TEST_CASE("BattleTeamEffectSystem_TeamShield_AddsOrRefreshes", "[battle][team_effect][unit]")
{
    BattleTeamEffectWorld world;
    world.units = {
        unit(1, 0, 100, 0, 30),
        unit(2, 0, 100, 0, 5),
        unit(3, 1, 100, 0, 0),
    };

    auto refresh = BattleTeamEffectSystem().applyTeamShield(world, 1, 20, true);
    REQUIRE(refresh.size() == 1);
    CHECK(refresh[0].targetUnitId == 2);
    CHECK(refresh[0].value == 15);
    CHECK(unitById(world, 1).shield == 30);
    CHECK(unitById(world, 2).shield == 20);

    auto add = BattleTeamEffectSystem().applyTeamShield(world, 1, 10, false);
    REQUIRE(add.size() == 2);
    CHECK(unitById(world, 1).shield == 40);
    CHECK(unitById(world, 2).shield == 30);
}

TEST_CASE("BattleTeamEffectSystem_HealAura_UsesRadiusAndCooldownReduction", "[battle][team_effect][unit]")
{
    BattleTeamEffectWorld world;
    world.units = {
        unit(1, 0, 100, 0),
        unit(2, 0, 50, 0),
        unit(3, 0, 40, 0),
        unit(4, 1, 10, 0),
    };
    world.units[0].x = 0.0;
    world.units[1].x = 20.0;
    world.units[2].x = 200.0;
    world.units[3].x = 20.0;

    auto events = BattleTeamEffectSystem().applyHealAura(world, 1, 5, 10, 50.0, 25);

    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleTeamEffectEventType::Heal);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].value == 15);
    CHECK(events[1].type == BattleTeamEffectEventType::CooldownReduced);
    CHECK(events[1].targetUnitId == 2);
    CHECK(events[1].value == 13);
    CHECK(unitById(world, 2).hp == 65);
    CHECK(unitById(world, 2).cooldown == 37);
    CHECK(unitById(world, 3).hp == 40);
    CHECK(unitById(world, 4).hp == 10);
}

TEST_CASE("BattleTeamEffectSystem_SelfHeal_EmitsOnlyWhenHpChanges", "[battle][team_effect][unit]")
{
    BattleTeamEffectWorld world;
    world.units = {
        unit(1, 0, 80, 0),
    };

    auto healed = BattleTeamEffectSystem().applySelfHeal(world, 1, 10);
    REQUIRE(healed.size() == 1);
    CHECK(healed[0].value == 10);
    CHECK(unitById(world, 1).hp == 90);

    world.units[0].hp = 100;
    auto full = BattleTeamEffectSystem().applySelfHeal(world, 1, 10);
    CHECK(full.empty());
}
