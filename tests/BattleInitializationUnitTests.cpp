#include "battle/BattleInitialization.h"
#include "battle/BattleRuntimeEffects.h"
#include "battle/BattleRuntimeSession.h"
#include "Find.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <utility>

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
    unit.vitals = { maxHp, maxHp, 0, 0 };
    unit.stats = { attack, defence, speed };
    return unit;
}

BattleRuntimeUnitSpawn runtimeSpawn(BattleRuntimeUnit unit)
{
    return makeRuntimeUnitSpawn(std::move(unit));
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

BattleInitializationContext testInitializationContext(int frame = 0)
{
    return { { 36.0, 18 }, frame };
}

BattleInitializationOutput initializeBattleStartForTest(
    std::vector<BattleRuntimeUnitSpawn> spawns,
    const BattleRuntimeSetupSeed& setup,
    BattleInitializationContext context = testInitializationContext())
{
    return BattleStartInitializer(std::move(spawns), setup, context).initialize();
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
    }
    else
    {
        input.setup.enemyRoster.push_back(roster);
    }
    input.setup.cloneSources.push_back({
        unit.unitId,
        unit.realRoleId,
        unit.vitals.maxHp + unit.stats.attack + unit.stats.defence,
        unit.star,
        unit.chessInstanceId,
        unit.sourceOrder,
    });
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


TEST_CASE("BattleStartInitializer_CompilesSpawnInitializationApi", "[battle][initialization]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });

    BattleRuntimeSetupSeed setup;

    auto output = initializeBattleStartForTest(std::move(spawns), setup);

    CHECK(output.result.roleDeltas.empty());
    CHECK(output.result.logEvents.empty());
}

TEST_CASE("BattleStartInitializer_AppliesStarGrowthFromRosterAndComboFightWins", "[battle][initialization]")
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
        .count = 1,
        .fightWinGrowth = {
            .maxHp = 2,
            .attack = 1,
            .defence = 3,
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

    auto output = initializeBattleStartForTest(std::move(spawns), setup);

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
        setup.starGrowth,
        2,
        3,
        2,
        1,
        3);

    const auto& unit = requireSpawn(output.spawns, 0).unit;
    CHECK(unit.vitals.maxHp == expected.hp);
    CHECK(unit.vitals.hp == expected.hp);
    CHECK(unit.stats.attack == expected.atk);
    CHECK(unit.stats.defence == expected.def);
    CHECK(unit.stats.speed == expected.spd);
    REQUIRE(output.result.roleDeltas.size() == 1);
    CHECK(output.result.roleDeltas[0].star == 2);
    CHECK(output.result.roleDeltas[0].fist == expected.fist);
    CHECK(output.result.roleDeltas[0].sword == expected.sword);
    CHECK(output.result.roleDeltas[0].knife == expected.knife);
    CHECK(output.result.roleDeltas[0].unusual == expected.unusual);
    CHECK(output.result.roleDeltas[0].hiddenWeapon == expected.hidden);
}

TEST_CASE("BattleStartInitializer clones the complete post-initialization runtime baseline", "[battle][initialization][effect_rule][damage][status][clone]")
{
    auto spawns = runtimeSpawns({ runtimeUnit(0, 0, 100, 20, 30, 40) });
    auto& preInitializationSource = requireSpawn(spawns, 0);
    preInitializationSource.status.effects.poisonTimer = 60;
    preInitializationSource.status.effects.poisonStacks = 2;
    preInitializationSource.status.effects.poisonTickPct = 7;
    preInitializationSource.status.effects.poisonSourceId = 99;
    preInitializationSource.damage.hurtInvincFrames = 12;
    preInitializationSource.damage.deathPreventionUsed = true;
    preInitializationSource.rescue.forcePullExecuteRemaining = 8;

    BattleRuntimeSetupSeed setup;
    setup.allyRoster.push_back({
        0,
        1001,
        0,
        1,
        1,
        -1,
        -1,
        77,
        0,
        0,
    });
    setup.units.push_back({
        0,
        1001,
        0,
        1,
        1,
        100,
        20,
        30,
        40,
    });

    ModifyDamageAction incomingDamage;
    incomingDamage.perspective = DamageModifierPerspective::Incoming;
    incomingDamage.stage = DamageModifierStage::BeforeDefense;
    incomingDamage.channel = DamageChannel::All;
    incomingDamage.amount.flat = -12;
    incomingDamage.operation = DamageModifierOperation::FlatAdd;

    ApplyStatusAction damageBlock;
    damageBlock.status = BattleStatusKind::DamageBlockLayer;
    damageBlock.stacks = 3;
    damageBlock.stack = EffectStackPolicy::Replace;

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 45;

    ChangeResourceAction statusShield;
    statusShield.resource = BattleResource::StatusShield;
    statusShield.kind = ResourceChangeKind::Grant;
    statusShield.amount.flat = 80;

    EffectRule initializedRule;
    initializedRule.id = EffectRuleId{ 7001 };
    initializedRule.event = EffectEvent::BattleInitialized;
    initializedRule.actions = {
        { EffectActionValue{ incomingDamage } },
        { EffectActionValue{ shield } },
        { EffectActionValue{ statusShield } },
        { EffectActionValue{ damageBlock } },
        { EffectActionValue{ StateMachineAction{ GenerateClonesAction{ 1 } } } },
        { EffectActionValue{ StateMachineAction{ PreventDeathAction{ 90 } } } },
        { EffectActionValue{ StateMachineAction{
            ConfigureRescueRepositionAction{ RescueRepositionMode::Protect, 2 },
        } } },
    };

    BattleSetupComboDefinition combo;
    combo.id = 701;
    combo.name = "初始化繼承測試";
    combo.memberRoleIds = { 1001 };
    combo.thresholds.push_back({
        .count = 1,
        .rules = { initializedRule },
    });
    setup.comboDefinitions.push_back(std::move(combo));
    setup.cloneSources.push_back({ 0, 1001, 150, 1, 77, 0 });
    setup.cloneCells.push_back({ 3, 4, true, false, 0 });

    auto output = initializeBattleStartForTest(std::move(spawns), setup);

    REQUIRE(output.spawns.size() == 2);
    const auto& source = requireSpawn(output.spawns, 0);
    const auto& clone = requireSpawn(output.spawns, 1);
    CHECK(clone.unit.cloneSourceUnitId == 0);
    CHECK(source.damage.deathPrevention);
    CHECK(source.damage.deathPreventionFrames == 90);
    CHECK(source.rescue.forcePullProtectRemaining == 2);
    CHECK(source.rescue.forcePullExecuteRemaining == 0);
    CHECK(source.damage.hurtInvincFrames == 0);
    CHECK_FALSE(source.damage.deathPreventionUsed);
    CHECK(source.status.effects.poisonStacks == 0);
    CHECK(source.unit.shield == 45);
    CHECK(source.status.effects.statusShield == 80);

    auto expectedCloneStatus = source.status;
    rewriteBattleStatusSourceUnitId(expectedCloneStatus, 0, 1);
    CHECK(clone.status == expectedCloneStatus);
    CHECK(clone.damage == source.damage);
    CHECK(clone.rescue == source.rescue);
    CHECK(clone.unit.shield == source.unit.shield);

    REQUIRE(output.effectCommands.damageModifiers.size() == 2);
    const auto& sourceDamage = output.effectCommands.damageModifiers[0];
    CHECK(sourceDamage.binding.ownerUnitId == 0);
    CHECK(sourceDamage.targetUnitId == 0);
    CHECK(sourceDamage.amount == -12);
    CHECK_FALSE(sourceDamage.expiresFrameExclusive);
    const auto& cloneDamage = output.effectCommands.damageModifiers[1];
    CHECK(cloneDamage.binding.ownerUnitId == 1);
    CHECK(cloneDamage.binding.sourceTeam == 0);
    CHECK(cloneDamage.binding.runtimeInstanceId == 0);
    CHECK(cloneDamage.targetUnitId == 1);
    CHECK(cloneDamage.amount == -12);
    CHECK_FALSE(cloneDamage.expiresFrameExclusive);

    REQUIRE(source.status.effects.typedStatuses.size() == 1);
    const auto& sourceStatus = source.status.effects.typedStatuses[0];
    CHECK(sourceStatus.kind == BattleStatusKind::DamageBlockLayer);
    CHECK(sourceStatus.sourceUnitId == 0);
    CHECK(sourceStatus.remainingFrames == 0);
    CHECK(sourceStatus.stacks == 3);

    CHECK(output.effectCommands.antiComboInitializationRecords.empty());
    CHECK(output.effectCommands.antiComboAttributeBases.empty());
}

TEST_CASE("BattleStartInitializer records only active anti-combo initialization for chained transfer", "[battle][initialization][effect_rule][anti_combo]")
{
    auto spawns = runtimeSpawns({
        runtimeUnit(0, 0, 100, 100, 30, 40),
        runtimeUnit(1, 0, 100, 100, 30, 40),
    });
    BattleRuntimeSetupSeed setup;
    setup.allyRoster = {
        {
            .unitId = 0,
            .realRoleId = 1001,
            .team = 0,
            .star = 1,
            .cost = 1,
        },
        {
            .unitId = 1,
            .realRoleId = 1002,
            .team = 0,
            .star = 1,
            .cost = 2,
        },
    };
    setup.units = {
        {
            .unitId = 0,
            .realRoleId = 1001,
            .team = 0,
            .star = 1,
            .cost = 1,
            .baseMaxHp = 100,
            .baseAttack = 100,
            .baseDefence = 30,
            .baseSpeed = 40,
        },
        {
            .unitId = 1,
            .realRoleId = 1002,
            .team = 0,
            .star = 1,
            .cost = 2,
            .baseMaxHp = 100,
            .baseAttack = 100,
            .baseDefence = 30,
            .baseSpeed = 40,
        },
    };

    ModifyAttributeAction normalAttack;
    normalAttack.attribute = BattleAttribute::Attack;
    normalAttack.operation = AttributeOperation::FlatAdd;
    normalAttack.amount.flat = 5;
    EffectRule normalRule;
    normalRule.id = EffectRuleId{ 7101 };
    normalRule.event = EffectEvent::BattleInitialized;
    normalRule.actions = { { EffectActionValue{ normalAttack } } };
    BattleSetupComboDefinition normalCombo;
    normalCombo.id = 710;
    normalCombo.name = "一般初始化";
    normalCombo.memberRoleIds = { 1001 };
    normalCombo.thresholds.push_back({
        .count = 1,
        .rules = { normalRule },
    });
    setup.comboDefinitions.push_back(std::move(normalCombo));

    ModifyAttributeAction antiComboAttack;
    antiComboAttack.attribute = BattleAttribute::Attack;
    antiComboAttack.operation = AttributeOperation::FlatAdd;
    antiComboAttack.amount.flat = 10;
    EffectRule antiComboRule;
    antiComboRule.id = EffectRuleId{ 7111 };
    antiComboRule.event = EffectEvent::BattleInitialized;
    antiComboRule.actions = { { EffectActionValue{ antiComboAttack } } };
    BattleSetupComboDefinition antiCombo;
    antiCombo.id = 711;
    antiCombo.name = "反向初始化";
    antiCombo.memberRoleIds = { 1001, 1002 };
    antiCombo.thresholds.push_back({
        .count = 1,
        .rules = { antiComboRule },
    });
    antiCombo.isAntiCombo = true;
    setup.comboDefinitions.push_back(std::move(antiCombo));

    auto output = initializeBattleStartForTest(std::move(spawns), setup);

    CHECK(requireSpawn(output.spawns, 0).unit.stats.attack == 105);
    CHECK(requireSpawn(output.spawns, 1).unit.stats.attack == 110);
    REQUIRE(output.effectCommands.antiComboInitializationRecords.size() == 1);
    const auto& record = output.effectCommands.antiComboInitializationRecords.front();
    CHECK(record.metadata.binding.sourceId == 711);
    CHECK(record.metadata.binding.ownerUnitId == 1);
    CHECK(record.metadata.targetUnitId == 1);
    CHECK(output.effectCommands.antiComboAttributeBases.size() == 8);

    const auto transfer = BattleEffectCommandSystem::transferAntiComboInitialization(
        output.effectCommands,
        1,
        0,
        0,
        711);
    REQUIRE(transfer.coreAttributeDeltas.size() == 1);
    CHECK(transfer.coreAttributeDeltas.front().attribute == BattleAttribute::Attack);
    CHECK(transfer.coreAttributeDeltas.front().delta == 10);
    REQUIRE(output.effectCommands.antiComboInitializationRecords.size() == 2);
    CHECK(output.effectCommands.antiComboInitializationRecords.back()
              .metadata.binding.ownerUnitId == 0);
}

TEST_CASE("BattleRuntimeSession_LoadsOnlyEnabledSelectedUltimateRulesOnce", "[battle][initialization][effect_rule]")
{
    auto makeInput = [](bool enabled)
    {
        BattleRuntimeSessionCreationInput input;
        input.rules = makeHadesBattleRuntimeRules(36.0, 18);

        auto normal = makeHadesTestSkillSeed();
        normal.id = 59;
        auto ultimate = makeHadesTestSkillSeed();
        ultimate.id = 59;
        addInitializedRuntimeTestUnit(
            input,
            7,
            1001,
            1,
            3,
            4,
            Towards_LeftUp,
            normal,
            ultimate);

        EffectRule firstRule;
        firstRule.id = EffectRuleId{ 101 };
        firstRule.event = EffectEvent::UltimateCommitted;
        ChangeResourceAction committedShield;
        committedShield.resource = BattleResource::Shield;
        committedShield.kind = ResourceChangeKind::Grant;
        committedShield.amount.flat = 1;
        firstRule.actions = { { EffectActionValue{ committedShield } } };
        EffectRule secondRule;
        secondRule.id = EffectRuleId{ 102 };
        secondRule.event = EffectEvent::HitBeforeDamage;
        ModifyDamageAction hitModifier;
        hitModifier.amount.flat = 1;
        secondRule.actions = { { EffectActionValue{ hitModifier } } };
        ModifyCastAction autoUltimate;
        autoUltimate.autoUltimate = AutoUltimateCastRequest{};
        EffectRule periodicRule;
        periodicRule.id = EffectRuleId{ 103 };
        periodicRule.event = EffectEvent::FrameAdvanced;
        periodicRule.intervalFrames = 30;
        periodicRule.actions = { { EffectActionValue{ autoUltimate } } };
        input.setup.magicEffectDefinitions.push_back({
            .magicId = 59,
            .name = "五虎斷門刀",
            .rules = { firstRule, secondRule, periodicRule },
            .enabled = enabled,
        });

        EffectRule unselectedRule;
        unselectedRule.id = EffectRuleId{ 201 };
        unselectedRule.event = EffectEvent::BattleInitialized;
        ModifyAttributeAction unselectedAttribute;
        unselectedAttribute.attribute = BattleAttribute::Attack;
        unselectedAttribute.operation = AttributeOperation::FlatAdd;
        unselectedAttribute.amount.flat = 1;
        unselectedRule.actions = { { EffectActionValue{ unselectedAttribute } } };
        input.setup.magicEffectDefinitions.push_back({
            .magicId = 26,
            .name = "降龍十八掌",
            .rules = { unselectedRule },
            .enabled = true,
        });
        return input;
    };

    SECTION("停用定義不進入 runtime store")
    {
        auto session = BattleRuntimeSession::createInitialized(makeInput(false)).session;
        CHECK(session.runtime().effectRules.rules().empty());
    }

    SECTION("normal 與 ultimate 相同也只按 selected ultimate 載入一次")
    {
        auto session = BattleRuntimeSession::createInitialized(makeInput(true)).session;
        const auto& store = session.runtime().effectRules;
        const auto rules = store.rules();
        REQUIRE(rules.size() == 3);
        CHECK(rules[0].rule.id == EffectRuleId{ 101 });
        CHECK(rules[1].rule.id == EffectRuleId{ 102 });
        CHECK(rules[2].rule.id == EffectRuleId{ 103 });
        CHECK(store.runtime(rules[2].binding, rules[2].rule.id).intervalFramesRemaining == 30);
        for (const auto& rule : rules)
        {
            CHECK(rule.binding.kind == EffectSourceKind::Magic);
            CHECK(rule.binding.sourceId == 59);
            CHECK(rule.binding.ownerUnitId == 7);
            CHECK(rule.binding.sourceTeam == 1);
        }
    }
}

TEST_CASE("BattleEffectRuntimeSnapshot_CopiesStableUnitFactsStatusesAndResources", "[battle][effect_rule][snapshot]")
{
    BattleRuntimeState runtime;
    runtime.gridTransform = { 36.0, 18 };
    runtime.movement.frame = 10;

    BattleRuntimeUnitRecord record;
    record.core.id = 9;
    record.core.team = 1;
    record.core.star = 3;
    record.core.alive = true;
    record.core.vitals = { 73, 120, 41, 90 };
    record.core.shield = 28;
    record.core.stats = { 57, 46, 35 };
    record.core.motion.position = { 11.0f, 22.0f, 0.0f };
    BattleActionPlanSeed actionPlan;
    actionPlan.normalSkill.id = 14;
    actionPlan.normalSkill.magicType = 2;
    actionPlan.ultimateSkill.id = 47;
    record.setActionPlan(actionPlan);
    record.comboFacts.memberComboIds = { 33, 44 };
    record.status.effects.statusShield = 70;
    record.status.effects.staggerShield = 80;
    record.status.effects.poisonTimer = 30;
    record.status.effects.poisonStacks = 1;
    record.status.effects.poisonTickPct = 4;
    record.status.effects.poisonSourceId = 2;
    record.status.effects.poisonAppliedSequence = 1;
    record.status.effects.typedStatuses.push_back({
        .kind = BattleStatusKind::SevenStarMark,
        .sourceUnitId = 2,
        .remainingFrames = 90,
        .stacks = 3,
        .appliedSequence = 2,
    });
    runtime.units.append(std::move(record));
    runtime.effectCommands.attributeModifiers.push_back({
        .sequence = 1,
        .binding = {
            .kind = EffectSourceKind::Combo,
            .sourceId = 33,
            .ownerUnitId = 9,
            .sourceTeam = 1,
        },
        .ruleId = EffectRuleId{ 1 },
        .targetUnitId = 9,
        .attribute = BattleAttribute::Attack,
        .operation = AttributeOperation::FlatAdd,
        .amount = 5,
        .appliedFrame = 0,
        .expiresFrameExclusive = 20,
    });

    BattleRuntimeUnitRecord earlierId;
    earlierId.core.id = 2;
    earlierId.core.team = 0;
    earlierId.core.alive = true;
    earlierId.core.vitals = { 10, 10, 0, 10 };
    runtime.units.append(std::move(earlierId));

    const auto direct = makeEffectUnitSnapshot(runtime, runtime.units.require(9));
    CHECK(direct.id == 9);
    CHECK(direct.hp == 73);
    CHECK(direct.maxHp == 120);
    CHECK(direct.mp == 41);
    CHECK(direct.maxMp == 90);
    CHECK(direct.shield == 28);
    CHECK(direct.statusShield == 70);
    CHECK(direct.staggerShield == 80);
    CHECK(direct.star == 3);
    CHECK(direct.attack == 62);
    CHECK(direct.defence == 46);
    CHECK(direct.speed == 35);
    CHECK(direct.weaponType == 1);
    CHECK((direct.magicIds == std::set<int>{ 14, 47 }));
    CHECK((direct.comboIds == std::set<int>{ 33, 44 }));
    CHECK(direct.hasState("中毒"));
    CHECK(direct.stackCount("中毒") == 1);
    CHECK(direct.hasState("七星"));
    CHECK(direct.stackCount("七星") == 3);

    BattleEffectRuntimeSnapshot snapshot(runtime);
    REQUIRE(snapshot.units().size() == 2);
    CHECK(snapshot.units()[0].id == 2);
    CHECK(snapshot.units()[1].id == 9);
    const auto view = snapshot.readView();
    REQUIRE(view.findUnit(9) != nullptr);
    CHECK(view.findUnit(9)->position.x == 11.0f);
    CHECK(view.tileWidth() == 36.0f);

    runtime.units.requireCore(9).vitals.hp = 1;
    CHECK(view.findUnit(9)->hp == 73);
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

    CHECK(session.runtime().rescue.counterAttack.skillId == 1);
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

TEST_CASE("BattleRuntimeSession_InitializesRuntimeRandomFromCreationInput", "[battle][initialization]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
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

TEST_CASE("BattleRuntimeSession_InitializedSessionAdvancesUnitsAfterSetupPlacement", "[battle][initialization][runtime]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    addInitializedRuntimeTestUnit(input, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(input, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    const auto initialAlly = session.runtime().units.requireCore(0).motion.position;
    const auto initialEnemy = session.runtime().units.requireCore(1).motion.position;

    bool anyUnitMoved = false;
    for (int frame = 0; frame < 90 && !anyUnitMoved; ++frame)
    {
        session.runFrame();
        const auto& ally = session.runtime().units.requireCore(0).motion.position;
        const auto& enemy = session.runtime().units.requireCore(1).motion.position;
        anyUnitMoved = ally.x != initialAlly.x
            || ally.y != initialAlly.y
            || enemy.x != initialEnemy.x
            || enemy.y != initialEnemy.y;
    }

    CHECK(anyUnitMoved);
}

TEST_CASE("BattleRuntimeSession_InitializedSessionSeedsAttackUnits", "[battle][initialization][runtime]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);

    addInitializedRuntimeTestUnit(input, 0, 1001, 0, 3, 3, Towards_RightDown);
    addInitializedRuntimeTestUnit(input, 1, 1002, 1, 10, 10, Towards_LeftUp);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    REQUIRE(session.runtime().units.size() == 2);
    CHECK(session.runtime().units.requireCore(0).id == 0);
    CHECK(session.runtime().units.requireCore(0).team == 0);
    CHECK(session.runtime().units.requireCore(0).alive);
    CHECK(session.runtime().units.requireCore(1).id == 1);
    CHECK(session.runtime().units.requireCore(1).team == 1);
    CHECK(session.runtime().units.requireCore(1).alive);
}

TEST_CASE("BattleRuntimeSession_InitializedSessionResolvesProjectileCombat", "[battle][initialization][runtime]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
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
