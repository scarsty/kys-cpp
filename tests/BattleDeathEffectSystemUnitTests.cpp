#include "battle/BattleCore.h"
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

BattleRuntimeUnit unit(int id, int team, bool alive)
{
    BattleRuntimeUnit state;
    state.id = id;
    state.team = team;
    state.alive = alive;
    state.hp = 50;
    state.maxHp = 100;
    state.attack = 10;
    state.defence = 20;
    return state;
}

BattleDeathEffectExtras extras(int id)
{
    BattleDeathEffectExtras state;
    state.id = id;
    return state;
}

const BattleDeathEffectExtras& extrasById(const BattleDeathEffectStore& store, int id)
{
    for (const auto& extras : store.units)
    {
        if (extras.id == id)
        {
            return extras;
        }
    }
    FAIL("extras not found");
}

}  // namespace

TEST_CASE("BattleDeathEffectSystem_AllyDeathStatBoost_RequiresRegularSharedCombo", "[battle][death_effect][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, false),
        unit(1, 0, true),
        unit(2, 1, true),
    };
    BattleDeathEffectStore effects;
    effects.regularSynergyComboIds = { 7 };
    auto dead = extras(0);
    dead.comboIds = { 7 };
    auto ally = extras(1);
    ally.appliedEffects = {
        effect(EffectType::AllyDeathStatBoost, 5, 7),
        effect(EffectType::AllyDeathStatBoost, 9, 8),
    };
    auto enemy = extras(2);
    enemy.appliedEffects = { effect(EffectType::AllyDeathStatBoost, 50, 7) };
    effects.units = { dead, ally, enemy };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(units, effects, 0);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::AllyStatBoost);
    CHECK(events[0].targetUnitId == 1);
    CHECK(events[0].value == 5);
    CHECK(units.requireUnit(1).attack == 15);
    CHECK(units.requireUnit(1).defence == 25);
    CHECK(units.requireUnit(2).attack == 10);
}

TEST_CASE("BattleDeathEffectSystem_DeathMedical_UsesDeadUnitEffectAndHealsComboAllies", "[battle][death_effect][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, false),
        unit(1, 0, true),
        unit(2, 0, true),
    };
    units.requireUnit(1).hp = 80;
    units.requireUnit(2).hp = 20;
    BattleDeathEffectStore effects;
    effects.regularSynergyComboIds = { 3 };
    auto dead = extras(0);
    dead.appliedEffects = { effect(EffectType::DeathMedical, 30, 3) };
    auto ally = extras(1);
    ally.comboIds = { 3 };
    auto outsider = extras(2);
    outsider.comboIds = { 4 };
    effects.units = { dead, ally, outsider };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(units, effects, 0);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::DeathMedicalHeal);
    CHECK(events[0].targetUnitId == 1);
    CHECK(events[0].value == 20);
    CHECK(units.requireUnit(1).hp == 100);
    CHECK(units.requireUnit(2).hp == 20);
}

TEST_CASE("BattleDeathEffectSystem_ShieldOnAllyDeath_TracksDeathsAndAwardsShield", "[battle][death_effect][unit]")
{
    BattleUnitStore units;
    units.units = {
        unit(0, 0, false),
        unit(1, 0, true),
    };
    units.requireUnit(1).maxHp = 200;
    BattleDeathEffectStore effects;
    effects.regularSynergyComboIds = { 5 };
    auto dead = extras(0);
    dead.comboIds = { 5 };
    auto ally = extras(1);
    ally.shieldPctMaxHp = 25;
    ally.shieldOnAllyDeathTracker = 1;
    ally.appliedEffects = { effect(EffectType::ShieldOnAllyDeath, 2, 5) };
    effects.units = { dead, ally };

    auto events = BattleDeathEffectSystem().applyAllyDeathEffects(units, effects, 0);

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == BattleDeathEffectEventType::ShieldOnAllyDeath);
    CHECK(events[0].targetUnitId == 1);
    CHECK(events[0].value == 50);
    CHECK(units.requireUnit(1).shield == 50);
    CHECK(extrasById(effects, 1).shieldOnAllyDeathTracker == 0);
}
