#include "battle/BattleDeathEffectSystem.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Battle;

namespace
{

AppliedEffectInstance effect(EffectType type, int value, int comboId)
{
    AppliedEffectInstance instance;
    instance.type = type;
    instance.value = value;
    instance.sourceComboId = comboId;
    return instance;
}

BattleDeathEffectUnit unit(int id, int team, bool alive)
{
    BattleDeathEffectUnit state;
    state.id = id;
    state.team = team;
    state.alive = alive;
    state.hp = 50;
    state.maxHp = 100;
    state.attack = 10;
    state.defence = 20;
    return state;
}

const BattleDeathEffectUnit& unitById(const BattleDeathEffectWorld& world, int id)
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

TEST_CASE("BattleDeathEffectSystem_AllyDeathStatBoost_RequiresRegularSharedCombo", "[battle][death_effect][unit]")
{
    BattleDeathEffectWorld world;
    world.regularSynergyComboIds = { 7 };
    auto dead = unit(1, 0, false);
    dead.comboIds = { 7 };
    auto ally = unit(2, 0, true);
    ally.appliedEffects = {
        effect(EffectType::AllyDeathStatBoost, 5, 7),
        effect(EffectType::AllyDeathStatBoost, 9, 8),
    };
    auto enemy = unit(3, 1, true);
    enemy.appliedEffects = { effect(EffectType::AllyDeathStatBoost, 50, 7) };
    world.units = { dead, ally, enemy };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(world, 1);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::AllyStatBoost);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].value == 5);
    CHECK(unitById(world, 2).attack == 15);
    CHECK(unitById(world, 2).defence == 25);
    CHECK(unitById(world, 3).attack == 10);
}

TEST_CASE("BattleDeathEffectSystem_DeathMedical_UsesDeadUnitEffectAndHealsComboAllies", "[battle][death_effect][unit]")
{
    BattleDeathEffectWorld world;
    world.regularSynergyComboIds = { 3 };
    auto dead = unit(1, 0, false);
    dead.appliedEffects = { effect(EffectType::DeathMedical, 30, 3) };
    auto ally = unit(2, 0, true);
    ally.hp = 80;
    ally.comboIds = { 3 };
    auto outsider = unit(3, 0, true);
    outsider.hp = 20;
    outsider.comboIds = { 4 };
    world.units = { dead, ally, outsider };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(world, 1);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::DeathMedicalHeal);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].value == 20);
    CHECK(unitById(world, 2).hp == 100);
    CHECK(unitById(world, 3).hp == 20);
}

TEST_CASE("BattleDeathEffectSystem_ShieldOnAllyDeath_TracksDeathsAndAwardsShield", "[battle][death_effect][unit]")
{
    BattleDeathEffectWorld world;
    world.regularSynergyComboIds = { 5 };
    auto dead = unit(1, 0, false);
    dead.comboIds = { 5 };
    auto ally = unit(2, 0, true);
    ally.maxHp = 200;
    ally.shieldPctMaxHp = 25;
    ally.shieldOnAllyDeathTracker = 1;
    ally.appliedEffects = { effect(EffectType::ShieldOnAllyDeath, 2, 5) };
    world.units = { dead, ally };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(world, 1);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::ShieldOnAllyDeath);
    CHECK(events[0].targetUnitId == 2);
    CHECK(events[0].value == 50);
    CHECK(unitById(world, 2).shield == 50);
    CHECK(unitById(world, 2).shieldOnAllyDeathTracker == 0);
}
