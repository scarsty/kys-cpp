#include "battle/BattleInitialization.h"
#include "BattleLogTestHelpers.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleStarStats.h"
#include "ChessCombo.h"
#include "ChessEquipment.h"
#include "ChessNeigong.h"
#include "Find.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <utility>

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
    unit.vitals = { maxHp, maxHp, 0, 0 };
    unit.stats = { attack, defence, speed };
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

BattleRuntimeUnitSpawn runtimeSpawn(BattleRuntimeUnit unit, RoleComboState combo = {})
{
    return makeRuntimeUnitSpawn(std::move(unit), std::move(combo));
}

std::vector<BattleRuntimeUnitSpawn> runtimeSpawns(std::initializer_list<BattleRuntimeUnit> units)
{
    std::vector<BattleRuntimeUnitSpawn> spawns;
    spawns.reserve(units.size());
    for (auto unit : units)
    {
        spawns.push_back(runtimeSpawn(std::move(unit)));
    }
    return spawns;
}

BattleRuntimeUnitSpawn& requireSpawn(std::vector<BattleRuntimeUnitSpawn>& spawns, int unitId)
{
    const auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn) { return spawn.unit.id == unitId; });
    REQUIRE(it != spawns.end());
    return *it;
}

const BattleRuntimeUnitSpawn& requireSpawn(const std::vector<BattleRuntimeUnitSpawn>& spawns, int unitId)
{
    const auto it = std::find_if(
        spawns.begin(),
        spawns.end(),
        [unitId](const BattleRuntimeUnitSpawn& spawn) { return spawn.unit.id == unitId; });
    REQUIRE(it != spawns.end());
    return *it;
}

BattleInitializationContext testInitializationContext(int frame = 0)
{
    return { { 36.0, 18 }, frame };
}

void addRuntimeSetupSeed(
    BattleRuntimeSessionCreationInput& input,
    const BattleSetupUnitInput& unit)
{
    input.setup.units.push_back({
        unit.unitId,
        unit.realRoleId,
        unit.team,
        unit.star,
        unit.cost,
        unit.vitals.maxHp,
        unit.stats.attack,
        unit.stats.defence,
        unit.stats.speed,
        unit.fist,
        unit.sword,
        unit.knife,
        unit.unusual,
        unit.hiddenWeapon,
        {},
    });
    BattleSetupRosterUnit roster{
        unit.unitId,
        unit.realRoleId,
        unit.team,
        unit.star,
        unit.cost,
        unit.weaponId,
        unit.armorId,
        unit.chessInstanceId,
        unit.fightsWon,
        unit.sourceOrder,
    };
    if (unit.team == 0)
    {
        input.setup.allyRoster.push_back(roster);
        input.setup.cloneSources.push_back({
            unit.unitId,
            unit.realRoleId,
            unit.vitals.maxHp + unit.stats.attack + unit.stats.defence,
            unit.star,
            unit.chessInstanceId,
            unit.sourceOrder,
        });
    }
    else
    {
        input.setup.enemyRoster.push_back(roster);
    }
}

BattleActionSkillSeed makeHadesTestSkillSeed(
    int attackAreaType = 0,
    int selectDistance = 1,
    std::string name = "普通攻擊",
    int soundId = 55,
    int visualEffectId = 44)
{
    BattleActionSkillSeed seed;
    seed.id = 1;
    seed.name = std::move(name);
    seed.soundId = soundId;
    seed.attackAreaType = attackAreaType;
    seed.magicType = 1;
    seed.selectDistance = selectDistance;
    seed.visualEffectId = visualEffectId;
    seed.actProperty = 40;
    seed.magicPower = 40;
    return seed;
}

void addInitializedRuntimeTestUnit(
    BattleRuntimeSessionCreationInput& input,
    int unitId,
    int realRoleId,
    int team,
    int gridX,
    int gridY,
    int faceTowards,
    BattleActionSkillSeed normalSkill = makeHadesTestSkillSeed(),
    BattleActionSkillSeed ultimateSkill = makeHadesTestSkillSeed())
{
    BattleSetupUnitInput unit;
    unit.unitId = unitId;
    unit.realRoleId = realRoleId;
    unit.name = team == 0 ? "我方" : "敵方";
    unit.team = team;
    unit.sourceOrder = unitId;
    unit.alive = true;
    unit.gridX = gridX;
    unit.gridY = gridY;
    unit.faceTowards = faceTowards;
    unit.vitals = { 120, 120, 0, 100 };
    unit.stats = { 30, 20, 40 };
    unit.motion.position = {
        static_cast<float>(-gridY * 36 + gridX * 36 + 18 * 36),
        static_cast<float>(gridY * 36 + gridX * 36),
        0.0f,
    };
    unit.motion.facing = faceTowards == Towards_LeftUp ? Pointf{ -1.0f, 0.0f, 0.0f } : Pointf{ 1.0f, 0.0f, 0.0f };
    unit.animation = { 0, 0, 0, -1 };
    unit.star = 1;
    unit.cost = 1;
    unit.physicalPower = 100;
    unit.hasEquippedSkill = true;
    unit.normalSkill = std::move(normalSkill);
    unit.ultimateSkill = std::move(ultimateSkill);
    input.units.push_back(unit);
    input.actionPlanSeeds.push_back({
        unitId,
        true,
        unit.normalSkill,
        unit.ultimateSkill,
    });
    addRuntimeSetupSeed(input, unit);
}

BattleSetupUnitInput makeRuntimeProfileTestSource()
{
    BattleSetupUnitInput source;
    source.unitId = 0;
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.headId = 23;
    source.team = 0;
    source.alive = true;
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    source.star = 3;
    source.cost = 7;
    source.weaponId = 71;
    source.armorId = 82;
    source.chessInstanceId = 99;
    source.fightFrames = { 0, 4, 8, 12, 16 };
    source.skillNames = "六脈神劍 北冥神功";
    return source;
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

TEST_CASE("BattleInitializationSystem_CompilesSpawnInitializationApi", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });

    BattleRuntimeSetupSeed setup;

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

    CHECK(result.roleDeltas.empty());
    CHECK(result.logEvents.empty());
}

TEST_CASE("BattleInitializationSystem_AppliesComboStatsToImportedRuntimeUnit", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });

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

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

    const auto& unit = requireSpawn(spawns, 0).unit;
    CHECK(unit.vitals.maxHp == 150);
    CHECK(unit.vitals.hp == 150);
    CHECK(unit.stats.attack == 27);
    CHECK(unit.stats.defence == 49);
    CHECK(unit.stats.speed == 42);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 0);
    CHECK(result.roleDeltas[0].vitals.maxHp == 150);
    CHECK(result.roleDeltas[0].vitals.hp == 150);
    CHECK(result.roleDeltas[0].stats.attack == 27);
    CHECK(result.roleDeltas[0].stats.defence == 49);
    CHECK(result.roleDeltas[0].stats.speed == 42);
}

TEST_CASE("BattleInitializationSystem_AppliesStarGrowthFromRosterAndComboFightWins", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });

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

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

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

    const auto& unit = requireSpawn(spawns, 0).unit;
    CHECK(unit.vitals.maxHp == expected.hp);
    CHECK(unit.vitals.hp == expected.hp);
    CHECK(unit.stats.attack == expected.atk);
    CHECK(unit.stats.defence == expected.def);
    CHECK(unit.stats.speed == expected.spd);
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
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 200, 20, 30, 40) });

    RoleComboState combo;
    ChessBattleEffects::applyEffect(combo, { EffectType::ShieldPctMaxHP, 25 });
    ChessBattleEffects::applyEffect(combo, { EffectType::DamageImmunityAfterFrames, 12 });
    ChessBattleEffects::applyEffect(combo, { EffectType::AutoUltimateAfterFrames, 30 });
    ChessBattleEffects::applyEffect(combo, { EffectType::BlockFirstHits, 2 });
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

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext(77));

    const auto& spawn = requireSpawn(spawns, 0);
    const auto& initialized = spawn.combo;
    const auto& unit = spawn.unit;
    const auto& status = spawn.status;
    const auto& damage = spawn.damage;
    CHECK(initialized.effectFrameTimers.at(2) == 30);
    CHECK(unit.shield == 65);
    CHECK(status.effects.damageImmunityTimer == 12);
    CHECK(damage.blockFirstHitsRemaining == 2);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].frame == 77);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(BattleLogTest::textOf(result.logEvents[0]) == "獲取50護盾");
    CHECK(result.logEvents[1].frame == 77);
    CHECK(BattleLogTest::textOf(result.logEvents[1]) == "全隊獲取15護盾");
}

TEST_CASE("BattleInitializationSystem_EnemyTopDebuffEmitsBattleLog", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({
        runtimeUnit(0, 0, 100, 20, 30, 40),
        runtimeUnit(1, 1, 100, 80, 50, 40),
        runtimeUnit(2, 1, 120, 60, 40, 40),
    });

    RoleComboState combo;
    ChessBattleEffects::applyEffect(combo, { EffectType::EnemyTopDebuff, 1, 7 });

    BattleRuntimeSetupSeed setup;
    setup.units.push_back({ .unitId = 0, .realRoleId = 1001, .team = 0, .baseMaxHp = 100, .baseAttack = 20, .baseDefence = 30, .baseSpeed = 40, .baseCombo = combo });
    setup.units.push_back({ .unitId = 1, .realRoleId = 1002, .team = 1, .star = 1, .cost = 3, .baseMaxHp = 100, .baseAttack = 80, .baseDefence = 50, .baseSpeed = 40 });
    setup.units.push_back({ .unitId = 2, .realRoleId = 1003, .team = 1, .star = 1, .cost = 1, .baseMaxHp = 120, .baseAttack = 60, .baseDefence = 40, .baseSpeed = 40 });

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

    REQUIRE(result.enemyTopDebuffs.size() == 1);
    CHECK(result.enemyTopDebuffs[0].unitId == 1);
    CHECK(requireSpawn(spawns, 1).unit.stats.attack == 73);
    CHECK(requireSpawn(spawns, 1).unit.stats.defence == 43);
    auto logIt = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Status
            && event.targetUnitId == 1
            && BattleLogTest::textOf(event) == "陰險：前1名攻防-7（1名存活）";
    });
    CHECK(logIt != result.logEvents.end());
}

TEST_CASE("BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror", "[battle][initialization]")
{
    auto source = runtimeUnit(0, 0, 100, 20, 30, 40);
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.headId = 23;
    source.fightFrames = { 0, 4, 8, 12, 16 };
    source.skillNames = "六脈神劍";
    source.haveAction = true;
    source.operationType = BattleOperationType::Melee;
    source.operationCount = 3;
    source.physicalPower = 9;
    source.invincible = 8;
    source.frozen = 6;
    source.frozenMax = 12;
    source.weaponId = 71;
    source.armorId = 82;
    source.chessInstanceId = 99;

    RoleComboState sourceCombo;
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::CloneSummon, 1 });
    std::vector<BattleRuntimeUnitSpawn> spawns;
    spawns.push_back(runtimeSpawn(source, sourceCombo));

    BattleRuntimeSetupSeed setup;
    setup.cloneSummonCount = 1;
    setup.cloneSources.push_back({ 0, 1001, 999, 3, 7, 0 });
    setup.cloneCells.push_back({ 3, 4, true, false });

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

    REQUIRE(spawns.size() == 2);
    const auto& cloneSpawn = requireSpawn(spawns, 1);
    const auto& clone = cloneSpawn.unit;
    CHECK(clone.cloneSourceUnitId == 0);
    CHECK(clone.name == "測試角色");
    CHECK(clone.headId == 23);
    CHECK(clone.fightFrames[2] == 8);
    CHECK(clone.skillNames == "六脈神劍");
    CHECK(clone.grid.x == 3);
    CHECK(clone.grid.y == 4);
    CHECK(clone.vitals.maxHp == 100);
    CHECK(clone.vitals.hp == 100);
    CHECK(clone.stats.attack == 20);
    CHECK(clone.stats.defence == 30);
    CHECK(clone.stats.speed == 40);
    CHECK_FALSE(clone.haveAction);
    CHECK(clone.operationType == BattleOperationType::None);
    CHECK(clone.operationCount == 0);
    CHECK(clone.physicalPower == 9);
    CHECK(clone.invincible == 8);
    CHECK(clone.frozen == 0);
    CHECK(clone.frozenMax == 0);
    CHECK(clone.weaponId == -1);
    CHECK(clone.armorId == -1);
    CHECK(clone.chessInstanceId == -1);
    CHECK(cloneSpawn.combo.isSummonedClone);
}

TEST_CASE("BattleRuntimeSession_CreatesCloneRuntimeRowsWithoutRoleMirror", "[battle][initialization]")
{
    std::map<int, RoleComboState> comboStates;
    ChessBattleEffects::applyEffect(comboStates[0], { EffectType::CloneSummon, 1 });

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    BattleSetupUnitInput source;
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
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    source.star = 3;
    source.cost = 7;
    source.chessInstanceId = 99;
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto creation = BattleRuntimeSession::createInitialized(std::move(input));
    auto& session = creation.session;

    const auto& units = session.runtime().unitStore.units;
    REQUIRE(units.size() == 2);
    const auto& sourceUnit = session.runtime().unitStore.requireUnit(0);
    const auto& cloneUnit = session.runtime().unitStore.requireUnit(1);
    CHECK(cloneUnit.cloneSourceUnitId == 0);
    CHECK(cloneUnit.realRoleId == 1001);
    CHECK(cloneUnit.vitals.hp == sourceUnit.vitals.hp);
    CHECK(cloneUnit.vitals.maxHp == sourceUnit.vitals.maxHp);
    CHECK(cloneUnit.stats.attack == sourceUnit.stats.attack);
    CHECK(cloneUnit.stats.defence == sourceUnit.stats.defence);
    CHECK(cloneUnit.stats.speed == sourceUnit.stats.speed);
    CHECK(cloneUnit.grid.x == 3);
    CHECK(cloneUnit.grid.y == 4);
}

TEST_CASE("BattleRuntimeSession_OwnsUnitProfileFacts", "[battle][initialization][runtime_session]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);

    auto source = makeRuntimeProfileTestSource();
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    const auto& unit = session.requireRuntimeUnit(0);

    CHECK(unit.identity().battleId == 0);
    CHECK(unit.identity().realRoleId == 1001);
    CHECK(unit.identity().team == 0);
    CHECK(unit.identity().headId == 23);
    CHECK(unit.identity().name == "測試角色");
    CHECK(unit.headId == 23);
    CHECK(unit.fightFrames[2] == 8);
    CHECK(unit.skillNames == "六脈神劍 北冥神功");
    CHECK(unit.weaponId == 71);
    CHECK(unit.armorId == 82);
    CHECK(unit.chessInstanceId == 99);
}

TEST_CASE("BattleRuntimeSession_CloneProfileKeepsRenderingWithoutRosterOwnership", "[battle][initialization][runtime_session]")
{
    std::map<int, RoleComboState> comboStates;
    ChessBattleEffects::applyEffect(comboStates[0], { EffectType::CloneSummon, 1 });

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    auto source = makeRuntimeProfileTestSource();
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    const auto& cloneUnit = session.requireRuntimeUnit(1);

    CHECK(cloneUnit.cloneSourceUnitId == 0);
    CHECK(cloneUnit.identity().battleId == 1);
    CHECK(cloneUnit.identity().realRoleId == 1001);
    CHECK(cloneUnit.identity().headId == 23);
    CHECK(cloneUnit.headId == 23);
    CHECK(cloneUnit.fightFrames[2] == 8);
    CHECK(cloneUnit.skillNames == "六脈神劍 北冥神功");
    CHECK(cloneUnit.weaponId == -1);
    CHECK(cloneUnit.armorId == -1);
    CHECK(cloneUnit.chessInstanceId == -1);
}

TEST_CASE("BattleRuntimeSession_CloneUsesFreshSpawnStores", "[battle][initialization][runtime_session]")
{
    std::map<int, RoleComboState> comboStates;
    auto& sourceCombo = comboStates[0];
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::CloneSummon, 1 });
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::ShieldPctMaxHP, 25 });
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::BlockFirstHits, 2 });
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::DamageImmunityAfterFrames, 5, 2 });
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::DamageImmunityAfterFrames, 12, 5 });
    ChessBattleEffects::applyEffect(sourceCombo, { EffectType::MPRecoveryBonus, 50 });

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.setup.cloneCells.push_back({ 3, 4, true, false });

    auto source = makeRuntimeProfileTestSource();
    source.animation = { 9, 10, 3, 1 };
    source.haveAction = true;
    source.operationType = BattleOperationType::Melee;
    source.operationCount = 4;
    source.physicalPower = 33;
    source.invincible = 8;
    source.motion.velocity = { 7, 8, 0 };
    source.motion.acceleration = { 0, 0, -4 };
    source.hasEquippedSkill = true;
    source.normalSkill = makeHadesTestSkillSeed();
    source.ultimateSkill = makeHadesTestSkillSeed(1, 2);

    input.actionPlanSeeds.push_back({
        source.unitId,
        source.hasEquippedSkill,
        source.normalSkill,
        source.ultimateSkill,
    });
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    const auto& sourceUnit = session.requireRuntimeUnit(0);
    const auto& cloneUnit = session.requireRuntimeUnit(1);
    const auto& runtime = session.runtime();

    CHECK(cloneUnit.cloneSourceUnitId == 0);
    CHECK(cloneUnit.realRoleId == sourceUnit.realRoleId);
    CHECK(cloneUnit.vitals.maxHp == sourceUnit.vitals.maxHp);
    CHECK(cloneUnit.vitals.hp == cloneUnit.vitals.maxHp);
    CHECK(cloneUnit.stats.attack == sourceUnit.stats.attack);
    CHECK(cloneUnit.grid.x == 3);
    CHECK(cloneUnit.grid.y == 4);
    CHECK(cloneUnit.animation.cooldown == 0);
    CHECK(cloneUnit.animation.actFrame == 0);
    CHECK(cloneUnit.animation.actType == -1);
    CHECK_FALSE(cloneUnit.haveAction);
    CHECK(cloneUnit.operationType == BattleOperationType::None);
    CHECK(cloneUnit.operationCount == 0);
    CHECK(cloneUnit.physicalPower == sourceUnit.physicalPower);
    CHECK(cloneUnit.invincible == sourceUnit.invincible);
    CHECK(cloneUnit.weaponId == -1);
    CHECK(cloneUnit.armorId == -1);
    CHECK(cloneUnit.chessInstanceId == -1);

    const auto& cloneStatus = requireById(runtime.status.units, 1);
    CHECK(cloneStatus.effects.damageImmunityAfterFrames == 12);
    CHECK(cloneStatus.effects.damageImmunityDuration == 5);
    CHECK(cloneStatus.effects.damageImmunityTimer == 12);

    const auto& cloneDamage = requireById(runtime.damage.unitExtras, 1);
    CHECK(cloneDamage.blockFirstHitsRemaining == 2);

    const auto cloneAgentIt = runtime.movement.agents.find(1);
    REQUIRE(cloneAgentIt != runtime.movement.agents.end());
    CHECK(cloneAgentIt->second.targetId == -1);
    CHECK(cloneAgentIt->second.physics.position.x == cloneUnit.motion.position.x);
    CHECK(cloneAgentIt->second.physics.position.y == cloneUnit.motion.position.y);
    CHECK(cloneAgentIt->second.physics.velocity.x == cloneUnit.motion.velocity.x);
    CHECK(cloneAgentIt->second.physics.acceleration.z == cloneUnit.motion.acceleration.z);

    const auto clonePlanIt = runtime.action.planSeeds.find(1);
    REQUIRE(clonePlanIt != runtime.action.planSeeds.end());
    CHECK(clonePlanIt->second.unitId == 1);
    CHECK(clonePlanIt->second.normalSkill.id == source.normalSkill.id);
}

TEST_CASE("BattleRuntimeSession_InitializesRuntimeRandomFromCreationInput", "[battle][initialization]")
{
    std::map<int, RoleComboState> comboStates;

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.randomSeed = 777u;

    BattleSetupUnitInput source;
    source.unitId = 0;
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.team = 0;
    source.alive = true;
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().random.seed() == 777u);
}

TEST_CASE("BattleRuntimeSession_StampsInitializationLogsWithCreationFrame", "[battle][initialization]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.battleFrame = 88;

    BattleSetupUnitInput source;
    source.unitId = 0;
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.team = 0;
    source.alive = true;
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    input.units.push_back(source);
    addRuntimeSetupSeed(input, source);
    ChessBattleEffects::applyEffect(input.setup.units[0].baseCombo, { EffectType::ShieldPctMaxHP, 25 });

    auto creation = BattleRuntimeSession::createInitialized(std::move(input));

    REQUIRE(creation.initialization.logEvents.size() == 1);
    CHECK(creation.initialization.logEvents[0].frame == 88);
}

TEST_CASE("BattleRuntimeSession_InitializedSessionAdvancesUnitsAfterSetupPlacement", "[battle][initialization][runtime]")
{
    std::map<int, RoleComboState> comboStates;

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    addInitializedRuntimeTestUnit(input, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(input, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    const auto initialAlly = session.runtime().unitStore.requireUnit(0).motion.position;
    const auto initialEnemy = session.runtime().unitStore.requireUnit(1).motion.position;

    bool anyUnitMoved = false;
    for (int frame = 0; frame < 90 && !anyUnitMoved; ++frame)
    {
        session.runFrame();
        const auto& ally = session.runtime().unitStore.requireUnit(0).motion.position;
        const auto& enemy = session.runtime().unitStore.requireUnit(1).motion.position;
        anyUnitMoved = ally.x != initialAlly.x
            || ally.y != initialAlly.y
            || enemy.x != initialEnemy.x
            || enemy.y != initialEnemy.y;
    }

    CHECK(anyUnitMoved);
}

TEST_CASE("BattleRuntimeSession_InitializedSessionSeedsAttackUnits", "[battle][initialization][runtime]")
{
    std::map<int, RoleComboState> comboStates;

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;

    addInitializedRuntimeTestUnit(input, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(input, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    REQUIRE(session.runtime().unitStore.units.size() == 2);
    CHECK(session.runtime().unitStore.units[0].id == 0);
    CHECK(session.runtime().unitStore.units[0].team == 0);
    CHECK(session.runtime().unitStore.units[0].alive);
    CHECK(session.runtime().unitStore.units[1].id == 1);
    CHECK(session.runtime().unitStore.units[1].team == 1);
    CHECK(session.runtime().unitStore.units[1].alive);
}

TEST_CASE("BattleRuntimeSession_InitializedSessionResolvesProjectileCombat", "[battle][initialization][runtime]")
{
    std::map<int, RoleComboState> comboStates;

    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.comboStates = comboStates;
    input.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    auto projectileSkill = makeHadesTestSkillSeed(1, 4, "飛刀", 55, 44);
    addInitializedRuntimeTestUnit(
        input,
        0,
        1001,
        0,
        3,
        3,
        Towards_RightDown,
        projectileSkill,
        projectileSkill);
    addInitializedRuntimeTestUnit(
        input,
        1,
        1002,
        1,
        5,
        3,
        Towards_LeftUp,
        projectileSkill,
        projectileSkill);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    bool playedAttackSound = false;
    bool emittedProjectile = false;
    bool appliedDamage = false;
    for (int frame = 0; frame < 90 && !(playedAttackSound && emittedProjectile && appliedDamage); ++frame)
    {
        const auto frameResult = session.runFrame();
        playedAttackSound = playedAttackSound || !frameResult.attackSoundIds.empty();
        emittedProjectile = emittedProjectile
            || std::any_of(
                frameResult.visualEvents.begin(),
                frameResult.visualEvents.end(),
                [](const BattleVisualEvent& event)
                {
                    return event.type == BattleVisualEventType::ProjectileSpawned;
                });
        appliedDamage = appliedDamage
            || std::any_of(
                frameResult.logEvents.begin(),
                frameResult.logEvents.end(),
                [](const BattleLogEvent& event)
                {
                    return event.type == BattleLogEventType::Damage
                        && event.amount > 0;
                });
    }

    CHECK(playedAttackSound);
    CHECK(emittedProjectile);
    CHECK(appliedDamage);
}

TEST_CASE("BattleInitializationSystem_ConsumesSetupAndInitializesOwnedRuntime", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });

    BattleInitializationUnitSeed seed;
    seed.unitId = 0;
    seed.realRoleId = 1001;
    seed.team = 0;
    seed.baseMaxHp = 100;
    seed.baseAttack = 20;
    seed.baseDefence = 30;
    seed.baseSpeed = 40;
    seed.baseCombo.flatHP = 25;
    BattleRuntimeSetupSeed setup;
    setup.units.push_back(seed);

    auto result = BattleInitializationSystem().initialize(spawns, setup, testInitializationContext());

    const auto& unit = requireSpawn(spawns, 0).unit;
    CHECK(unit.vitals.maxHp == 125);
    CHECK(unit.vitals.hp == 125);
    CHECK(unit.vitals.maxHp == 125);
    CHECK(unit.vitals.hp == 125);
    REQUIRE(result.roleDeltas.size() == 1);
    CHECK(result.roleDeltas[0].unitId == 0);
    CHECK(result.roleDeltas[0].vitals.maxHp == 125);
    CHECK(result.roleDeltas[0].vitals.hp == 125);
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
