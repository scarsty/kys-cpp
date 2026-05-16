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

void addRuntimeSetupSeed(
    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext& context,
    const KysChess::BattleSceneBattleAdapter::BattleSetupUnitInput& unit)
{
    context.input.runtimeSetupSeed.units.push_back({
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
        context.input.runtimeSetupSeed.allyRoster.push_back(roster);
        context.input.runtimeSetupSeed.cloneSources.push_back({
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
        context.input.runtimeSetupSeed.enemyRoster.push_back(roster);
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
    KysChess::BattleSceneBattleAdapter::BattleRuntimeBuildContext& context,
    int unitId,
    int realRoleId,
    int team,
    int gridX,
    int gridY,
    int faceTowards,
    BattleActionSkillSeed normalSkill = makeHadesTestSkillSeed(),
    BattleActionSkillSeed ultimateSkill = makeHadesTestSkillSeed())
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    Adapter::BattleSetupUnitInput unit;
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
    context.input.units.push_back(unit);
    context.input.actionPlanSeeds.push_back({
        unitId,
        true,
        unit.normalSkill,
        unit.ultimateSkill,
    });
    addRuntimeSetupSeed(context, unit);
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
    runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
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
    runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
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

    const auto& unit = runtime.unitStore.requireUnit(0);
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
    BattleRuntimeState runtime;
    runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
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

    const auto& unit = runtime.unitStore.requireUnit(0);
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
    BattleRuntimeState runtime;
    runtime.unitStore.units.push_back(runtimeUnit(0, 0, 200, 20, 30, 40));
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
    const auto& unit = runtime.unitStore.requireUnit(0);
    const auto& status = requireStatusRuntime(runtime, 0);
    CHECK(initialized.shield == 65);
    CHECK(initialized.damageImmunityTimer == 12);
    CHECK(initialized.autoUltimateTimer == 30);
    CHECK(initialized.blockFirstHitsRemaining == 2);
    CHECK(unit.shield == 65);
    CHECK(status.effects.damageImmunityTimer == 12);
    REQUIRE(result.logEvents.size() == 2);
    CHECK(result.logEvents[0].type == BattleLogEventType::Status);
    CHECK(result.logEvents[0].sourceUnitId == 0);
    CHECK(result.logEvents[0].text == "獲取50護盾");
    CHECK(result.logEvents[1].text == "全隊獲取15護盾");
}

TEST_CASE("BattleInitializationSystem_CreatesRuntimeCloneBeforeSceneMirror", "[battle][initialization]")
{
    BattleRuntimeState runtime;
    runtime.unitStore.gridTransform = { 36.0, 18 };
    runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
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
    CHECK(intent.roleValues.vitals.maxHp == 100);
    CHECK(intent.roleValues.vitals.hp == 100);
    CHECK(intent.roleValues.stats.attack == 20);
    CHECK(intent.roleValues.stats.defence == 30);
    CHECK(intent.roleValues.stats.speed == 40);
    CHECK(runtime.unitStore.findUnit(1) != nullptr);
    CHECK(runtime.combo.units.find(1) != runtime.combo.units.end());
}

TEST_CASE("BattleSceneBattleAdapter_CreatesCloneSceneRowsWithoutRoleMirror", "[battle][initialization]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;
    comboStates[0].cloneSummonCount = 1;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.input.runtimeSetupSeed.cloneCells.push_back({ 3, 4, true, false });

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
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    source.star = 3;
    source.cost = 7;
    source.chessInstanceId = 99;
    context.input.units.push_back(source);
    addRuntimeSetupSeed(context, source);

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
    CHECK(cloneIt->vitals.hp == sourceIt->vitals.hp);
    CHECK(cloneIt->vitals.maxHp == sourceIt->vitals.maxHp);
    CHECK(cloneIt->stats.attack == sourceIt->stats.attack);
    CHECK(cloneIt->stats.defence == sourceIt->stats.defence);
    CHECK(cloneIt->stats.speed == sourceIt->stats.speed);
    CHECK(cloneIt->gridX == 3);
    CHECK(cloneIt->gridY == 4);
}

TEST_CASE("BattleSceneBattleAdapter_InitializesRuntimeRandomFromBuildContext", "[battle][initialization]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.randomSeed = 777u;

    Adapter::BattleSetupUnitInput source;
    source.unitId = 0;
    source.realRoleId = 1001;
    source.name = "測試角色";
    source.team = 0;
    source.alive = true;
    source.vitals = { 100, 100, 0, 0 };
    source.stats = { 20, 30, 40 };
    source.motion = { { 100, 200, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 1, 1, 0 } };
    source.animation = { 0, 0, 0, -1 };
    context.input.units.push_back(source);
    addRuntimeSetupSeed(context, source);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    CHECK(created.session.runtime().random.seed() == 777u);
}

TEST_CASE("BattleSceneBattleAdapter_InitializedSessionAdvancesUnitsAfterSetupPlacement", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    addInitializedRuntimeTestUnit(context, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(context, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    BattleSetupPlacementInput placement;
    placement.units.push_back({ 0, 3, 3, Towards_RightDown });
    placement.units.push_back({ 1, 10, 10, Towards_LeftUp });
    created.session.commitSetupPlacement(placement);

    const auto initialAlly = created.session.runtime().unitStore.requireUnit(0).motion.position;
    const auto initialEnemy = created.session.runtime().unitStore.requireUnit(1).motion.position;

    bool anyUnitMoved = false;
    for (int frame = 0; frame < 90 && !anyUnitMoved; ++frame)
    {
        created.session.runFrame();
        const auto& ally = created.session.runtime().unitStore.requireUnit(0).motion.position;
        const auto& enemy = created.session.runtime().unitStore.requireUnit(1).motion.position;
        anyUnitMoved = ally.x != initialAlly.x
            || ally.y != initialAlly.y
            || enemy.x != initialEnemy.x
            || enemy.y != initialEnemy.y;
    }

    CHECK(anyUnitMoved);
}

TEST_CASE("BattleSceneBattleAdapter_InitializedSessionSeedsAttackUnits", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;

    addInitializedRuntimeTestUnit(context, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(context, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    REQUIRE(created.session.runtime().unitStore.units.size() == 2);
    CHECK(created.session.runtime().unitStore.units[0].id == 0);
    CHECK(created.session.runtime().unitStore.units[0].team == 0);
    CHECK(created.session.runtime().unitStore.units[0].alive);
    CHECK(created.session.runtime().unitStore.units[1].id == 1);
    CHECK(created.session.runtime().unitStore.units[1].team == 1);
    CHECK(created.session.runtime().unitStore.units[1].alive);
}

TEST_CASE("BattleSceneBattleAdapter_InitializedSessionResolvesProjectileCombat", "[battle][initialization][runtime]")
{
    namespace Adapter = KysChess::BattleSceneBattleAdapter;

    std::map<int, RoleComboState> comboStates;

    Adapter::BattleRuntimeBuildContext context;
    context.rules = makeHadesBattleRuntimeRules(36.0, 18);
    context.input.comboStates = &comboStates;
    context.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    auto projectileSkill = makeHadesTestSkillSeed(1, 4, "飛刀", 55, 44);
    addInitializedRuntimeTestUnit(
        context,
        0,
        1001,
        0,
        3,
        3,
        Towards_RightDown,
        projectileSkill,
        projectileSkill);
    addInitializedRuntimeTestUnit(
        context,
        1,
        1002,
        1,
        5,
        3,
        Towards_LeftUp,
        projectileSkill,
        projectileSkill);

    auto created = Adapter::createInitializedBattleRuntimeSession(context);

    BattleSetupPlacementInput placement;
    placement.units.push_back({ 0, 3, 3, Towards_RightDown });
    placement.units.push_back({ 1, 5, 3, Towards_LeftUp });
    created.session.commitSetupPlacement(placement);

    bool playedAttackSound = false;
    bool emittedProjectile = false;
    bool appliedDamage = false;
    for (int frame = 0; frame < 90 && !(playedAttackSound && emittedProjectile && appliedDamage); ++frame)
    {
        const auto frameResult = created.session.runFrame();
        playedAttackSound = playedAttackSound || !frameResult.applications.attackSoundIds.empty();
        emittedProjectile = emittedProjectile
            || std::any_of(
                frameResult.frame.visualEvents.begin(),
                frameResult.frame.visualEvents.end(),
                [](const BattleVisualEvent& event)
                {
                    return event.type == BattleVisualEventType::ProjectileSpawned;
                });
        appliedDamage = appliedDamage || !frameResult.damageRenderApplications.empty();
    }

    CHECK(playedAttackSound);
    CHECK(emittedProjectile);
    CHECK(appliedDamage);
}

TEST_CASE("BattleRuntimeSession_ConsumesSetupAndInitializesOwnedRuntime", "[battle][initialization][runtime_session]")
{
    BattleRuntimeInit init;
    init.runtime.unitStore.units.push_back(runtimeUnit(0, 0, 100, 20, 30, 40));
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

    const auto& unit = session.runtime().unitStore.requireUnit(0);
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
