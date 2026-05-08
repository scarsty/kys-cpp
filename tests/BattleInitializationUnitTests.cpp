#include "battle/BattleInitialization.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace KysChess::Battle;
using namespace KysChess;

namespace
{

BattleRuntimeUnit runtimeUnit(int id, int team, int maxHp, int attack, int defence, int speed)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = maxHp > 0;
    unit.hp = maxHp;
    unit.maxHp = maxHp;
    unit.attack = attack;
    unit.defence = defence;
    unit.speed = speed;
    return unit;
}

BattleStatusRuntimeUnit statusRuntime(int id, int hp, int attack)
{
    BattleStatusRuntimeUnit unit;
    unit.id = id;
    (void)hp;
    (void)attack;
    return unit;
}

BattleStatusRuntimeUnit& requireStatusRuntime(BattleRuntimeState& runtime, int unitId)
{
    const auto it = std::find_if(
        runtime.status.units.begin(),
        runtime.status.units.end(),
        [unitId](const BattleStatusRuntimeUnit& unit) { return unit.id == unitId; });
    REQUIRE(it != runtime.status.units.end());
    return *it;
}

}  // namespace

TEST_CASE("BattleInitializationSystem_CompilesPureRuntimeInitializationApi", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 100, 20));

    BattleRuntimeSetupSeed setup;

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    CHECK(result.roleDeltas.empty());
    CHECK(result.cloneIntents.empty());
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleInitializationSystem_AppliesComboStatsToImportedRuntimeUnit", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 100, 20));

    RoleComboState combo;
    combo.flatHP = 25;
    combo.flatATK = 5;
    combo.flatDEF = 3;
    combo.flatSPD = 2;
    combo.pctHP = 20;
    combo.pctATK = 10;
    combo.pctDEF = 50;

    BattleRuntimeSetupSeed setup;
    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& unit = runtime.units.requireUnit(10);
    CHECK(unit.maxHp == 150);
    CHECK(unit.hp == 150);
    CHECK(unit.attack == 27);
    CHECK(unit.defence == 49);
    CHECK(unit.speed == 42);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 10);
    CHECK(result.roleDeltas[0].maxHp == 150);
}

TEST_CASE("BattleInitializationSystem_InitializesShieldTimersAndBlockCounters", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 200, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 200, 20));

    RoleComboState combo;
    combo.shieldPctMaxHP = 25;
    combo.damageImmunityAfterFrames = 12;
    combo.autoUltimateAfterFrames = 30;
    combo.blockFirstHitsCount = 2;
    AppliedEffectInstance teamShield;
    teamShield.type = EffectType::FlatShield;
    teamShield.trigger = Trigger::Always;
    teamShield.value = 15;
    teamShield.sourceComboId = 77;
    combo.appliedEffects.push_back(teamShield);

    BattleRuntimeSetupSeed setup;
    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 200;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& initialized = runtime.combo.units.at(10);
    const auto& unit = runtime.units.requireUnit(10);
    const auto& status = requireStatusRuntime(runtime, 10);
    CHECK(initialized.shield == 65);
    CHECK(initialized.damageImmunityTimer == 12);
    CHECK(initialized.autoUltimateTimer == 30);
    CHECK(initialized.blockFirstHitsRemaining == 2);
    CHECK(unit.shield == 65);
    CHECK(status.damageImmunityTimer == 12);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 10);
    CHECK(result.logEvents[0].text == "獲取50護盾");
    CHECK(result.logEvents[1].text == "全隊獲取15護盾");
}