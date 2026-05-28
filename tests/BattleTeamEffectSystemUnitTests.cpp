#include "battle/BattleCore.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <map>

using namespace KysChess::Battle;

namespace
{

BattleRuntimeUnit unit(int id, int team, int hp, int mp)
{
    BattleRuntimeUnit state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.vitals.hp = hp;
    state.vitals.maxHp = 100;
    state.vitals.mp = mp;
    state.vitals.maxMp = 100;
    state.animation.cooldown = 50;
    state.motion.position = { static_cast<float>(id * 10.0), 0.0f, 0.0f };
    return state;
}

const BattleRuntimeUnit& unitById(const BattleRuntimeUnits& store, int id)
{
    return store.requireCore(id);
}

}  // namespace

TEST_CASE("BattleTeamEffectSystem_TeamHeal_HealsAliveSourceTeamOnly", "[battle][team_effect][unit]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 40, 0),
        unit(1, 0, 95, 0),
        unit(2, 1, 20, 0),
        unit(3, 0, 10, 0),
    });
    units.requireCore(3).alive = false;

    auto events = BattleTeamEffectSystem().applyTeamHeal(units, 0, 10, 20);

    REQUIRE(events.size() == 2);
    CHECK(events[0].targetUnitId == 0);
    CHECK(events[0].value == 30);
    CHECK(events[1].targetUnitId == 1);
    CHECK(events[1].value == 5);
    CHECK(unitById(units, 0).vitals.hp == 70);
    CHECK(unitById(units, 1).vitals.hp == 100);
    CHECK(unitById(units, 2).vitals.hp == 20);
    CHECK(unitById(units, 3).vitals.hp == 10);
}

TEST_CASE("BattleTeamEffectSystem_TeamMp_RespectsBlockBonusAndCap", "[battle][team_effect][unit]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 80),
        unit(1, 0, 100, 70),
        unit(2, 0, 100, 40),
        unit(3, 1, 100, 20),
    });
    units.require(2).status.effects.mpBlockTimer = 1;
    std::map<int, KysChess::RoleComboState> combos;
    KysChess::ChessBattleEffects::applyEffect(
        units.require(1).combo,
        { KysChess::EffectType::MPRecoveryBonus, 50 });

    auto events = BattleTeamEffectSystem().applyTeamMp(units, 0, 20);

    REQUIRE(events.size() == 2);
    CHECK(unitById(units, 0).vitals.mp == 100);
    CHECK(unitById(units, 1).vitals.mp == 100);
    CHECK(unitById(units, 2).vitals.mp == 40);
    CHECK(unitById(units, 3).vitals.mp == 20);
    CHECK(events[0].value == 20);
    CHECK(events[1].value == 30);
}

TEST_CASE("BattleTeamEffectSystem_TeamShield_AddsOrRefreshes", "[battle][team_effect][unit]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 0),
        unit(1, 0, 100, 0),
        unit(2, 1, 100, 0),
    });
    units.requireCore(0).shield = 30;
    units.requireCore(1).shield = 5;

    auto refresh = BattleTeamEffectSystem().applyTeamShield(units, 0, 20, true);
    REQUIRE(refresh.size() == 1);
    CHECK(refresh[0].targetUnitId == 1);
    CHECK(refresh[0].value == 15);
    CHECK(unitById(units, 0).shield == 30);
    CHECK(unitById(units, 1).shield == 20);

    auto add = BattleTeamEffectSystem().applyTeamShield(units, 0, 10, false);
    REQUIRE(add.size() == 2);
    CHECK(unitById(units, 0).shield == 40);
    CHECK(unitById(units, 1).shield == 30);
}

TEST_CASE("BattleTeamEffectSystem_HealAura_UsesRadiusAndCooldownReduction", "[battle][team_effect][unit]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 100, 0),
        unit(1, 0, 50, 0),
        unit(2, 0, 40, 0),
        unit(3, 1, 10, 0),
    });
    units.requireCore(0).motion.position.x = 0.0f;
    units.requireCore(1).motion.position.x = 20.0f;
    units.requireCore(2).motion.position.x = 200.0f;
    units.requireCore(3).motion.position.x = 20.0f;

    auto events = BattleTeamEffectSystem().applyHealAura(units, 0, 5, 10, 50.0, 25);

    REQUIRE(events.size() == 2);
    CHECK(events[0].type == BattleTeamEffectEventType::Heal);
    CHECK(events[0].targetUnitId == 1);
    CHECK(events[0].value == 15);
    CHECK(events[1].type == BattleTeamEffectEventType::CooldownReduced);
    CHECK(events[1].targetUnitId == 1);
    CHECK(events[1].value == 13);
    CHECK(unitById(units, 1).vitals.hp == 65);
    CHECK(unitById(units, 1).animation.cooldown == 37);
    CHECK(unitById(units, 2).vitals.hp == 40);
    CHECK(unitById(units, 3).vitals.hp == 10);
}

TEST_CASE("BattleTeamEffectSystem_SelfHeal_EmitsOnlyWhenHpChanges", "[battle][team_effect][unit]")
{
    auto units = KysChess::Battle::Test::runtimeRecords({
        unit(0, 0, 80, 0),
    });

    auto healed = BattleTeamEffectSystem().applySelfHeal(units, 0, 10);
    REQUIRE(healed.size() == 1);
    CHECK(healed[0].value == 10);
    CHECK(unitById(units, 0).vitals.hp == 90);

    units.requireCore(0).vitals.hp = 100;
    auto full = BattleTeamEffectSystem().applySelfHeal(units, 0, 10);
    CHECK(full.empty());
}
