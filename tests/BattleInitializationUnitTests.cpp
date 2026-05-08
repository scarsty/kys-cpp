#include "battle/BattleInitialization.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleStarStats.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace KysChess::Battle;
using namespace KysChess;

namespace
{

const BalanceConfig& chessBalanceConfig()
{
    static BalanceConfig config;
    return config;
}

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

namespace KysChess
{

const BalanceConfig& ChessBalance::config()
{
    return chessBalanceConfig();
}

}  // namespace KysChess

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

TEST_CASE("BattleInitializationSystem_AppliesStarGrowthFromRosterAndComboFightWins", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(10, 100, 20));

    BattleRuntimeSetupSeed setup;
    setup.allyRoster.push_back({
        10,
        1001,
        0,
        2,
        1,
        -1,
        -1,
        7,
        3,
        0,
    });

    BattleSetupComboDefinition combo;
    combo.id = 99;
    combo.name = "測試連攜";
    combo.memberRoleIds = { 1001 };
    combo.thresholds.push_back({
        1,
        {
            ComboEffect{ EffectType::FightWinHP, 2 },
            ComboEffect{ EffectType::FightWinATKDEF, 1, 3 },
        },
    });
    setup.comboDefinitions.push_back(combo);

    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.star = 2;
    seed.cost = 1;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseFist = 10;
    seed.baseSword = 11;
    seed.baseKnife = 12;
    seed.baseUnusual = 13;
    seed.baseHiddenWeapon = 14;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto expected = computeStarBoostedStats(
        {
            100,
            20,
            30,
            40,
            10,
            11,
            12,
            13,
            14,
        },
        2,
        3,
        2,
        1,
        3);

    const auto& unit = runtime.units.requireUnit(10);
    CHECK(unit.maxHp == expected.hp);
    CHECK(unit.hp == expected.hp);
    CHECK(unit.attack == expected.atk);
    CHECK(unit.defence == expected.def);
    CHECK(unit.speed == expected.spd);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].star == 2);
    CHECK(result.roleDeltas[0].fist == expected.fist);
    CHECK(result.roleDeltas[0].sword == expected.sword);
    CHECK(result.roleDeltas[0].knife == expected.knife);
    CHECK(result.roleDeltas[0].unusual == expected.unusual);
    CHECK(result.roleDeltas[0].hiddenWeapon == expected.hidden);
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

TEST_CASE("BattleRuntimeSession_ConsumesSetupAndInitializesOwnedRuntime", "[battle][initialization][runtime_session]")
{
    BattleRuntimeInit init;
    init.runtime.units.units.push_back(runtimeUnit(10, 0, 100, 20, 30, 40));
    init.runtime.status.units.push_back(statusRuntime(10, 100, 20));

    BattleInitializationUnitSeed seed;
    seed.unitId = 10;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo.flatHP = 25;
    init.setup.units.push_back(seed);

    BattleRuntimeSession session(std::move(init));
    auto result = session.releaseInitializationResult();

    const auto& unit = session.runtime().units.requireUnit(10);
    CHECK(unit.maxHp == 125);
    CHECK(unit.hp == 125);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 10);
    CHECK(result.roleDeltas[0].maxHp == 125);
}