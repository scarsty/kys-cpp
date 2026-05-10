#include "battle/BattleInitialization.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleSceneBattleAdapter.h"
#include "BattleStarStats.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"

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

const std::vector<ComboDef>& ChessCombo::getAllCombos()
{
    static const std::vector<ComboDef> combos{
        {
            9001,
            "測試分身連攜",
            { 1001 },
            {
                {
                    1,
                    "一人",
                    {
                        ComboEffect{ EffectType::CloneSummon, 1 },
                    },
                },
            },
            false,
            false,
        },
    };
    return combos;
}

std::vector<int> ChessCombo::getCombosForRole(int)
{
    return {};
}

const std::vector<EquipmentDef>& ChessEquipment::getAll()
{
    static const std::vector<EquipmentDef> equipment;
    return equipment;
}

const std::vector<EquipmentSynergyDef>& ChessEquipment::getAllSynergies()
{
    static const std::vector<EquipmentSynergyDef> synergies;
    return synergies;
}

const std::vector<NeigongDef>& ChessNeigong::getPool()
{
    static const std::vector<NeigongDef> pool;
    return pool;
}

}  // namespace KysChess

TEST_CASE("BattleInitializationSystem_CompilesPureRuntimeInitializationApi", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(0, 100, 20));

    BattleRuntimeSetupSeed setup;

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    CHECK(result.roleDeltas.empty());
    CHECK(result.cloneIntents.empty());
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleInitializationSystem_AppliesComboStatsToImportedRuntimeUnit", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(0, 100, 20));

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
    seed.unitId = 0;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& unit = runtime.units.requireUnit(0);
    CHECK(unit.maxHp == 150);
    CHECK(unit.hp == 150);
    CHECK(unit.attack == 27);
    CHECK(unit.defence == 49);
    CHECK(unit.speed == 42);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 0);
    CHECK(result.roleDeltas[0].maxHp == 150);
}

TEST_CASE("BattleInitializationSystem_AppliesStarGrowthFromRosterAndComboFightWins", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(0, 100, 20));

    BattleRuntimeSetupSeed setup;
    setup.allyRoster.push_back({
        0,
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
    seed.unitId = 0;
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

    const auto& unit = runtime.units.requireUnit(0);
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
    runtime.units.units.push_back(runtimeUnit(0, 0, 200, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(0, 200, 20));

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
    seed.unitId = 0;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 200;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo = combo;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    const auto& initialized = runtime.combo.units.at(0);
    const auto& unit = runtime.units.requireUnit(0);
    const auto& status = requireStatusRuntime(runtime, 0);
    CHECK(initialized.shield == 65);
    CHECK(initialized.damageImmunityTimer == 12);
    CHECK(initialized.autoUltimateTimer == 30);
    CHECK(initialized.blockFirstHitsRemaining == 2);
    CHECK(unit.shield == 65);
    CHECK(status.damageImmunityTimer == 12);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].text == "獲取50護盾");
    CHECK(result.logEvents[1].text == "全隊獲取15護盾");
}

TEST_CASE("BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.units.gridTransform = { 36.0, 18 };
    runtime.units.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
    runtime.status.units.push_back(statusRuntime(0, 100, 20));
    runtime.combo.units[0].cloneSummonCount = 1;

    BattleRuntimeSetupSeed setup;
    setup.cloneSummonCount = 1;
    setup.cloneSources.push_back({ 0, 1001, 999, 3, 7, 0 });
    setup.cloneCells.push_back({ 3, 4, true, false });

    auto result = BattleInitializationSystem().initialize(runtime, setup);

    REQUIRE(result.cloneIntents.size() == 1);
    const auto& intent = result.cloneIntents[0];
    CHECK(intent.sourceUnitId == 0);
    CHECK(intent.cloneUnitId == 1);
    CHECK(intent.gridX == 3);
    CHECK(intent.gridY == 4);
    CHECK(intent.roleValues.unitId == 1);
    CHECK(intent.roleValues.maxHp == 100);
    CHECK(intent.roleValues.hp == 100);
    CHECK(intent.roleValues.attack == 20);
    CHECK(intent.roleValues.defence == 30);
    CHECK(intent.roleValues.speed == 40);
    CHECK(runtime.units.findUnit(1) != nullptr);
    CHECK(runtime.combo.units.find(1) != runtime.combo.units.end());
}

TEST_CASE("BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror", "[battle][initialization]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;
    comboStates[0].cloneSummonCount = 1;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.setup.comboStates = &comboStates;
    context.setup.cloneSpawnCells.push_back({ 3, 4 });

    Adapter::BattleSetupUnitInput source;
    source.unitId = 0;
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.headId = 23;
    source.team = 0;
    source.sourceOrder = 0;
    source.alive = true;
    source.gridX = 1;
    source.gridY = 2;
    source.faceTowards = Towards_RightDown;
    source.position = { 100, 200, 0 };
    source.facing = { 1, 1, 0 };
    source.star = 3;
    source.cost = 7;
    source.chessInstanceId = 99;
    source.hp = 100;
    source.maxHp = 100;
    source.attack = 20;
    source.defence = 30;
    source.speed = 40;
    context.setup.units.push_back(source);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    const auto& units = created.sceneInitialization.units;
    REQUIRE(units.size() == 2);
    const auto sourceIt = std::find_if(
        units.begin(),
        units.end(),
        [](const BattleSceneUnit& unit)
        {
            return unit.unitId == 0;
        });
    const auto cloneIt = std::find_if(
        units.begin(),
        units.end(),
        [](const BattleSceneUnit& unit)
        {
            return unit.unitId == 1;
        });
    REQUIRE(sourceIt != units.end());
    REQUIRE(cloneIt != units.end());
    CHECK(cloneIt->sourceUnitId == 0);
    CHECK(cloneIt->identity.realRoleId == 1001);
    CHECK(cloneIt->identity.battleId == 1);
    CHECK(cloneIt->hp == sourceIt->hp);
    CHECK(cloneIt->maxHp == sourceIt->maxHp);
    CHECK(cloneIt->vitals.hp == sourceIt->vitals.hp);
    CHECK(cloneIt->vitals.maxHp == sourceIt->vitals.maxHp);
    CHECK(cloneIt->attack == sourceIt->attack);
    CHECK(cloneIt->defence == sourceIt->defence);
    CHECK(cloneIt->speed == sourceIt->speed);
    CHECK(cloneIt->stats.attack == sourceIt->stats.attack);
    CHECK(cloneIt->stats.defence == sourceIt->stats.defence);
    CHECK(cloneIt->stats.speed == sourceIt->stats.speed);
    CHECK(cloneIt->gridX == 3);
    CHECK(cloneIt->gridY == 4);
}

TEST_CASE("BattleRuntimeSession_ConsumesSetupAndInitializesOwnedRuntime", "[battle][initialization][runtime_session]")
{
    BattleRuntimeInit init;
    init.runtime.units.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
    init.runtime.status.units.push_back(statusRuntime(0, 100, 20));

    BattleInitializationUnitSeed seed;
    seed.unitId = 0;
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

    const auto& unit = session.runtime().units.requireUnit(0);
    CHECK(unit.maxHp == 125);
    CHECK(unit.hp == 125);
    CHECK(unit.vitals.maxHp == 125);
    CHECK(unit.vitals.hp == 125);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 0);
    CHECK(result.roleDeltas[0].maxHp == 125);
}

TEST_CASE("BattleRuntimeUnit_UsesSharedUnitValueObjects", "[battle][initialization][runtime_session]")
{
    BattleRuntimeUnit unit;
    unit.vitals = { 10, 20, 3, 8 };
    unit.stats = { 30, 40, 50 };
    unit.motion.position = { 1, 2, 3 };
    unit.motion.velocity = { 4, 5, 6 };
    unit.animation = { 7, 9, 11, 13 };

    CHECK(unit.vitals.hp == 10);
    CHECK(unit.stats.attack == 30);
    CHECK(unit.motion.position.x == 1);
    CHECK(unit.animation.cooldown == 7);
}
