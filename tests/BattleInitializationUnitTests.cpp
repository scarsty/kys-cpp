#include "battle/BattleInitialization.h"
#include "battle/BattleEffectEventBridge.h"
#include "battle/BattleRuntimeEffects.h"
#include "battle/BattleRuntimeSession.h"
#include "BattleCoreTestHelpers.h"
#include "ChessGameSessionTestHelpers.h"
#include "Find.h"
#include "ChessGameplayEffect.h"
#include <yaml-cpp/yaml.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <utility>

using namespace KysChess::Battle;
using namespace KysChess;
using namespace KysChess::Battle::Test;

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

BattleSetupComboThreshold gameplayThreshold(
    int count,
    const std::string& yaml,
    std::uint64_t& nextRuleId)
{
    BattleSetupComboThreshold threshold;
    threshold.count = count;
    std::vector<GameplayEffect> effects;
    REQUIRE(parseGameplayEffects(YAML::Load(yaml), effects, threshold.rules, nextRuleId, "羈絆互動測試"));
    return threshold;
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
    unit.actionPlan = BattleActionPlanSeed{
        .hasEquippedSkill = true,
        .normalSkill = std::move(normalSkill),
        .ultimateSkill = std::move(ultimateSkill),
    };
    input.units.push_back(unit);
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
    preInitializationSource.status.effects.statuses.push_back(
        boundStatusBehaviorContribution(
            BattleStatusKind::Poison,
            poisonStatusBehavior(7),
            99,
            9401,
            2,
            1,
            60));
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
    damageBlock.quantity = SetDamageBlockCharges{ 3 };
    damageBlock.behavior = damageBlockStatusBehavior();

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 45;
    int expectedShield = 45;
    SECTION("固定護盾") {}
    SECTION("附加星級項與初始化生命上限共同求值")
    {
        shield.amount.base = EffectNumberBase::TargetMaxHp;
        shield.amount.percent = 10;
        shield.additionalAmount = EffectNumber{.base = EffectNumberBase::SourceStar, .percent = 7000};
        expectedShield = 125;
    }

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
    CHECK_FALSE(source.status.effects.has(BattleStatusKind::Poison));
    CHECK(source.unit.shield == expectedShield);
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

    REQUIRE(source.status.effects.statuses.size() == 1);
    const auto& sourceStatus = source.status.effects.statuses[0];
    CHECK(sourceStatus.kind == BattleStatusKind::DamageBlockLayer);
    CHECK(sourceStatus.sourceUnitId == 0);
    CHECK(sourceStatus.remainingFrames == 0);
    CHECK(sourceStatus.stacks == 3);
    CHECK(sourceStatus.familyLocalLimit == 3);

    CHECK(output.effectCommands.antiComboInitializationRecords.empty());
    CHECK(output.effectCommands.antiComboAttributeBases.empty());
}

TEST_CASE("BattleStartInitializer preserves status producer identity for all clone combo tiers",
          "[battle][initialization][effect_rule][status][clone][tiers]")
{
    // 測試自有羈絆：成員數 3/5/7 與各疇克隆數 1/2/3 都屬於本測試資料，
    // 只受本測試提供的克隆格數（3）約束，不依賴頂層設定的羈絆內容。
    constexpr std::array memberCounts{3, 5, 7};
    constexpr std::array cloneCounts{1, 2, 3};
    constexpr std::array memberRoleIds{1, 2, 3, 4, 5, 6, 7};

    for (std::size_t tier = 0; tier < cloneCounts.size(); ++tier)
    {
        CAPTURE(tier);
        const int memberCount = memberCounts[tier];
        const int expectedClones = cloneCounts[tier];
        BattleRuntimeSetupSeed setup;
        BattleSetupComboDefinition combo{
            .id = 77,
            .name = "測試克隆羈絆",
            .memberRoleIds = {memberRoleIds.begin(), memberRoleIds.end()},
        };
        for (std::size_t index = 0; index < cloneCounts.size(); ++index)
        {
            EffectRule cloneRule;
            cloneRule.id = EffectRuleId{9100 + index};
            cloneRule.event = EffectEvent::BattleInitialized;
            cloneRule.actions.push_back(EffectAction{
                StateMachineAction{GenerateClonesAction{.count = cloneCounts[index]}}});
            combo.thresholds.push_back({
                .count = memberCounts[index],
                .rules = {cloneRule},
            });
        }

        ApplyStatusAction trueQi;
        trueQi.status = BattleStatusKind::TrueQi;
        trueQi.quantity = AddStatusLayers{ .count = 1, .limit = 2 };
        trueQi.behavior = trueQiStatusBehavior(9);
        EffectRule inheritedStatusRule;
        inheritedStatusRule.id = EffectRuleId{ 99001 };
        inheritedStatusRule.event = EffectEvent::HitBeforeDamage;
        inheritedStatusRule.selector.kind = EffectSelectorKind::HitTarget;
        inheritedStatusRule.actions.push_back(EffectAction{ trueQi });
        for (auto& threshold : combo.thresholds)
            threshold.rules.push_back(inheritedStatusRule);
        setup.comboDefinitions.push_back(std::move(combo));

        std::vector<BattleRuntimeUnitSpawn> spawns;
        for (int index = 0; index < memberCount; ++index)
        {
            const int roleId = memberRoleIds[index];
            spawns.push_back(runtimeSpawn(runtimeUnit(
                index, 0, 100, 20, 30, 40)));
            setup.units.push_back({
                .unitId = index,
                .realRoleId = roleId,
                .team = 0,
                .star = 1,
                .cost = 1,
                .baseMaxHp = 100,
                .baseAttack = 20,
                .baseDefence = 30,
                .baseSpeed = 40,
            });
            setup.allyRoster.push_back({
                .unitId = index,
                .realRoleId = roleId,
                .team = 0,
                .star = 1,
                .cost = 1,
                .chessInstanceId = 1000 + index,
                .sourceOrder = index,
            });
            setup.cloneSources.push_back({
                .sourceUnitId = index,
                .sourceRealRoleId = roleId,
                .power = 1000 - index,
                .star = 1,
                .chessInstanceId = 1000 + index,
                .sourceOrder = index,
            });
        }
        setup.cloneCells = {
            { .x = 10, .y = 10, .walkable = true, .team = 0 },
            { .x = 11, .y = 10, .walkable = true, .team = 0 },
            { .x = 12, .y = 10, .walkable = true, .team = 0 },
        };

        auto output = initializeBattleStartForTest(std::move(spawns), setup);
        std::vector<int> cloneUnitIds;
        for (const auto& spawn : output.spawns)
        {
            if (spawn.unit.cloneSourceUnitId >= 0)
                cloneUnitIds.push_back(spawn.unit.id);
        }
        REQUIRE(cloneUnitIds.size()
            == static_cast<std::size_t>(expectedClones));

        BattleRuntimeState runtime;
        runtime.gridTransform = testInitializationContext().gridTransform;
        for (auto& spawn : output.spawns)
            appendRuntimeUnit(runtime, std::move(spawn));
        runtime.effectRules = std::move(output.effectRules);
        runtime.effectCommands = std::move(output.effectCommands);

        BattleStatusUnitState holder = runtime.units.require(0).statusDamageState();
        std::set<int> logicalOwners;
        for (std::size_t cloneIndex = 0; cloneIndex < cloneUnitIds.size(); ++cloneIndex)
        {
            const int cloneUnitId = cloneUnitIds[cloneIndex];
            const BattleCastProvenance cast{
                .rootCastId = BattleCastId(100 + cloneIndex),
                .castId = BattleCastId(100 + cloneIndex),
                .sourceUnitId = cloneUnitId,
                .magicId = 0,
                .ultimate = false,
                .origin = CastOriginKind::Normal,
                .propagation = CastPropagationPolicy::SourceRules,
            };
            const BattleAttackProvenance attack{
                .cast = cast,
                .propagation = cast.propagation,
                .origin = BattleAttackOriginKind::Initial,
                .attackId = BattleAttackId(100 + cloneIndex),
                .rootAttack = true,
                .mainProjectile = true,
            };
            const auto dispatched = BattleEffectEventBridge().dispatch(
                runtime,
                {
                    .frame = 1,
                    .eventOrdinal = 1 + cloneIndex,
                    .ownerUnitId = cloneUnitId,
                },
                EffectEvent::HitBeforeDamage,
                HitEventData{
                    .provenance = attack,
                    .targetUnitId = 0,
                    .originalTargetUnitId = 0,
                    .acceptedHit = true,
                });
            REQUIRE(dispatched.commands.size() == 1);
            const auto& command = dispatched.commands.front();
            CHECK(command.metadata.binding.kind == EffectSourceKind::Combo);
            CHECK(command.metadata.binding.sourceId == 77);
            CHECK(command.metadata.binding.ownerUnitId == cloneUnitId);
            CHECK(command.metadata.ruleId == EffectRuleId{ 99001 });
            CHECK(command.metadata.authoredActionOrder == 0);
            holder = BattleEffectCommandSystem::applyStatusCommand(
                std::move(holder),
                command.metadata,
                std::get<ApplyStatusEffectCommand>(command.value),
                { .frame = 1 },
                {},
                false).target;
            REQUIRE(holder.effects.statuses.back().producerFamily);
            logicalOwners.insert(
                holder.effects.statuses.back().producerFamily->logicalOwnerUnitId);
        }

        CHECK(logicalOwners.size() == cloneUnitIds.size());
        REQUIRE(holder.effects.statuses.size() == cloneUnitIds.size());
        for (const auto& contribution : holder.effects.statuses)
        {
            CHECK(contribution.stacks == 1);
            CHECK(contribution.familyLocalLimit == 2);
        }
    }
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
        711,
        0);
    REQUIRE(transfer.coreAttributeDeltas.size() == 1);
    CHECK(transfer.coreAttributeDeltas.front().attribute == BattleAttribute::Attack);
    CHECK(transfer.coreAttributeDeltas.front().delta == 10);
    REQUIRE(output.effectCommands.antiComboInitializationRecords.size() == 2);
    CHECK(output.effectCommands.antiComboInitializationRecords.back()
              .metadata.binding.ownerUnitId == 0);
}

TEST_CASE("BattleRuntimeSession_LoadsSelectedUltimateRulesOnce", "[battle][initialization][effect_rule]")
{
    auto makeInput = []
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
        });
        return input;
    };

    SECTION("normal 與 ultimate 相同也只按 selected ultimate 載入一次")
    {
        auto session = BattleRuntimeSession::createInitialized(makeInput()).session;
        const auto& store = session.runtime().effectRules;
        const auto rules = store.rules();
        REQUIRE(rules.size() == 3);
        CHECK(rules[0].rule().id == EffectRuleId{ 101 });
        CHECK(rules[1].rule().id == EffectRuleId{ 102 });
        CHECK(rules[2].rule().id == EffectRuleId{ 103 });
        CHECK(store.runtime(rules[2].binding, rules[2].rule().id).intervalFramesRemaining == 30);
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
    record.comboFacts.addMember(33);
    record.comboFacts.addMember(44);
    record.comboFacts.addApplied(33);
    record.status.effects.statusShield = 70;
    record.status.effects.staggerShield = 80;
    record.status.effects.statuses.push_back(boundStatusBehaviorContribution(
        BattleStatusKind::Poison,
        poisonStatusBehavior(4),
        2,
        9402,
        1,
        1,
        30));
    record.status.effects.statuses.push_back({
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
    CHECK(direct.martialCategory == EffectMartialCategory::Sword);
    CHECK((direct.magicIds == std::pmr::vector<int>{ 14, 47 }));
    CHECK((direct.comboIds == std::pmr::vector<int>{ 33, 44 }));
    CHECK(direct.hasState(BattleStatusKind::Poison));
    CHECK(direct.stackCount(BattleStatusKind::Poison) == 1);
    CHECK(direct.hasState(BattleStatusKind::SevenStarMark));
    CHECK(direct.stackCount(BattleStatusKind::SevenStarMark) == 3);

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

TEST_CASE("talent initialization amplifies complete growth and adds opening MP and independent strengthening", "[battle][talent][initialization]")
{
    auto spawns = runtimeSpawns({runtimeUnit(0, 0, 100, 20, 30, 40)});
    spawns[0].unit.vitals.maxMp = 100;
    spawns[0].unit.vitals.mp = 25;
    BattleRuntimeSetupSeed setup;
    setup.allyRoster.push_back({.unitId = 0, .realRoleId = 1001, .star = 2, .cost = 3,
        .fightsWon = 3, .amplifiedGrowthPercent = 100, .openingMp = 50,
        .openingStrengthening = {3, 3, 50},
        .lethalRecovery = BattleLethalRecovery{70, 1, 120}, .openingStatusShield = 810});
    setup.units.push_back({.unitId = 0, .realRoleId = 1001, .star = 2, .cost = 3,
        .baseMaxHp = 100, .baseAttack = 20, .baseDefence = 30, .baseSpeed = 40});
    BattleSetupComboDefinition combo;
    combo.id = 99;
    combo.name = "開場與勝場";
    combo.memberRoleIds = {1001};
    ChangeResourceAction mp;
    mp.resource = BattleResource::Mp;
    mp.kind = ResourceChangeKind::Restore;
    mp.amount.flat = 60;
    EffectRule rule;
    rule.id = {1};
    rule.event = EffectEvent::BattleInitialized;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.actions = {{mp}};
    combo.thresholds.push_back({.count = 1, .rules = {rule}, .fightWinGrowth = {2, 1, 3}});
    setup.comboDefinitions.push_back(combo);
    const auto output = initializeBattleStartForTest(std::move(spawns), setup);
    REQUIRE(output.spawns.size() == 1);
    const auto& spawn = output.spawns[0];
    CHECK(spawn.unit.vitals.mp == 100);
    CHECK(spawn.damage.strengthening == BattleStrengthening{3, 3, 50});
    const auto expected = computeStarBoostedStats({100, 20, 30, 40}, setup.starGrowth, 2, 3, 2, 1, 3, 100);
    CHECK(spawn.unit.vitals.maxHp == expected.hp);
    CHECK(spawn.unit.stats.attack == expected.atk);
    CHECK(spawn.unit.stats.defence == expected.def);
    REQUIRE(spawn.damage.lethalRecovery);
    CHECK(spawn.damage.lethalRecovery->chancePercent == 70);
    CHECK(spawn.status.effects.statusShield == 810);
}


TEST_CASE("Lore equipment initializes its configurable defense and conversion only for the bonded wearer",
          "[battle][initialization][lore-equipment]")
{
    std::vector<GameplayEffect> effects;
    std::vector<EffectRule> rules;
    std::uint64_t nextId = 100;
    REQUIRE(parseGameplayEffects(YAML::Load(R"([
      {類型: 輪轉護身, 開場層數: 2, 層數上限: 7, 減傷百分比: 42, 回復間隔幀數: 9, 每次回復層數: 3},
      {類型: 化毒養身, 毒傷轉化百分比: 33, 治療上限窗口幀數: 11, 治療上限生命百分比: 4}
    ])"), effects, rules, nextId, "測試"));
    auto spawns = runtimeSpawns({runtimeUnit(0, 0, 100, 20, 30, 40), runtimeUnit(1, 0, 100, 20, 30, 40)});
    BattleRuntimeSetupSeed setup;
    setup.units = {{0, 1001, 0, 1, 1, 100, 20, 30, 40}, {1, 1002, 0, 1, 1, 100, 20, 30, 40}};
    setup.allyRoster = {{.unitId = 0, .realRoleId = 1001, .weaponId = 777},
                       {.unitId = 1, .realRoleId = 1002, .weaponId = 777}};
    setup.equipmentDefinitions.push_back({.itemId = 777});
    setup.equipmentSynergies.push_back({.roleIds = {1001}, .equipmentId = 777, .rules = rules});
    auto output = initializeBattleStartForTest(std::move(spawns), setup, testInitializationContext(5));
    REQUIRE(output.effectCommands.guardCharges.size() == 1);
    REQUIRE(output.effectCommands.poisonConversions.size() == 1);
    auto& guard = output.effectCommands.guardCharges.at(0);
    CHECK(guard.charges == 2);
    guard.advanceTo(13);
    CHECK(guard.charges == 2);
    guard.advanceTo(14);
    CHECK(guard.charges == 5);
    guard.advanceTo(23);
    CHECK(guard.charges == 7);
    CHECK(guard.config.reductionPct == 42);
    const auto& poison = output.effectCommands.poisonConversions.at(0);
    CHECK(poison.config.conversionPct == 33);
    CHECK(poison.config.healingWindowFrames == 11);
    CHECK(poison.config.healingMaxHpPct == 4);
    CHECK(poison.binding.ownerUnitId == 0);
}

TEST_CASE("Alliance synergy points activate suppression tiers without adding debuff owners",
          "[battle][initialization][alliance][enemy-top-debuff][regression]")
{
    // 測試自有門檻與效果數值，不依賴頂層設定。
    for (const auto [memberCount, selected] : {std::pair{1, true}, {3, true}, {4, true}, {5, false}})
    {
        CAPTURE(memberCount, selected);
        std::uint64_t nextRuleId{};
        BattleRuntimeSessionCreationInput input;
        input.rules = makeHadesBattleRuntimeRules(36.0, 18);
        input.setup.comboModifiersByTeam[0] = {selected ? 7 : -1, 1, 6, 1};
        input.setup.comboDefinitions.push_back({
            .id = 7,
            .name = "陰險",
            .memberRoleIds = {1001, 1002, 1003, 1004, 1005},
            .thresholds = {
                gameplayThreshold(2, "[{類型: 壓制最強敵人, 敵人數: 1, 攻擊點數: -7, 防禦點數: -7}]", nextRuleId),
                gameplayThreshold(4, "[{類型: 壓制最強敵人, 敵人數: 2, 攻擊點數: -11, 防禦點數: -11}]", nextRuleId),
                gameplayThreshold(6, "[{類型: 壓制最強敵人, 敵人數: 3, 攻擊點數: -13, 防禦點數: -13}]", nextRuleId),
            },
        });
        for (int id = 1; id <= memberCount; ++id)
            addInitializedRuntimeTestUnit(input, id, 1000 + id, 0, 10 + id, 10, Towards_RightDown);
        for (int id = 10; id < 14; ++id)
            addInitializedRuntimeTestUnit(input, id, 2000 + id, 1, 30 + id, 30, Towards_LeftUp);

        const int tier = memberCount == 1 ? 0 : memberCount == 3 ? 1 : 2;
        const int targetCount = tier + 1;
        const int amount = std::array{7, 11, 13}[tier];
        const auto resolved = resolveBattleSetupCombos(input.setup.allyRoster, input.setup);
        REQUIRE(resolved.size() == 1);
        CHECK(resolved.front().physicalMemberCount == memberCount);
        CHECK(resolved.front().effectiveMemberCount == memberCount + (selected ? 1 : 0));
        CHECK(resolved.front().activeThresholdIndex == tier);
        CHECK(resolved.front().requiredCounts == std::vector<int>{2, 4, 5});
        auto created = BattleRuntimeSession::createInitialized(std::move(input));
        auto runtime = created.session.runtime();
        CHECK(runtime.units.size() == memberCount + 4);

        BattleEffectEventBridge bridge;
        const auto dispatch = [&](int frame, int livingOwners)
        {
            const auto event = bridge.makeEvent(runtime,
                {.frame = frame, .eventOrdinal = static_cast<std::uint64_t>(frame), .ownerUnitId = 1},
                EffectEvent::FrameAdvanced, FrameTickEventData{.deltaFrames = 1});
            const auto dispatched = bridge.dispatchFrameAdvanced(runtime, event);
            REQUIRE(dispatched.commands.size() == livingOwners * targetCount * 2);
            for (const auto& command : dispatched.commands)
            {
                CHECK(command.metadata.binding.ownerUnitId >= 1);
                CHECK(command.metadata.binding.ownerUnitId <= livingOwners);
                CHECK(command.metadata.targetUnitId >= 10);
                CHECK(command.metadata.targetUnitId < 10 + targetCount);
            }
            BattleEffectCommandSystem().reduce(runtime, dispatched.commands);
            for (int id = 10; id < 14; ++id)
                for (const auto attribute : {BattleAttribute::Attack, BattleAttribute::Defence})
                    CHECK(BattleEffectCommandSystem::queryAttribute(runtime,
                        {.unitId = id, .attribute = attribute, .baseValue = 200, .frame = frame})
                        == 200 - (id < 10 + targetCount ? livingOwners * amount : 0));
        };
        dispatch(1, memberCount);
        if (memberCount > 1)
        {
            runtime.units.requireCore(memberCount).alive = false;
            runtime.units.requireCore(memberCount).vitals.hp = 0;
            dispatch(2, memberCount - 1);
        }
    }
}

TEST_CASE("Alliance Mingjiao counts real and equipment members for deaths and last survivor",
          "[battle][initialization][alliance][berserk][death-heal][regression]")
{
    std::uint64_t nextRuleId{};
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(36.0, 18);
    input.setup.comboModifiersByTeam[0] = {7, 1, 6, 1};
    input.setup.comboDefinitions.push_back({
        .id = 7,
        .name = "明教",
        .memberRoleIds = {1001, 1002},
        .thresholds = {{.count = 2}, gameplayThreshold(4, R"([
            {類型: 同羈絆陣亡屬性加成, 屬性: 攻擊, 點數: 17},
            {類型: 同羈絆陣亡屬性加成, 屬性: 防禦, 點數: 17},
            {類型: 陣亡治療羈絆成員, 生命百分比: 13},
            {類型: 最後羈絆成員狂暴, 暴擊百分比: 50, 冷卻縮減百分比: 20, 吸血百分比: 40, 持續幀數: 9}
        ])", nextRuleId)},
    });
    for (int id = 1; id <= 4; ++id)
        addInitializedRuntimeTestUnit(input, id, 1000 + id, 0, 10 + id, 10, Towards_RightDown);
    input.units[2].weaponId = 777;
    input.setup.allyRoster[2].weaponId = 777;
    input.setup.equipmentDefinitions.push_back({.itemId = 777, .countsAsComboNames = {"明教"}});
    addInitializedRuntimeTestUnit(input, 10, 1001, 1, 40, 30, Towards_LeftUp);

    const auto resolved = resolveBattleSetupCombos(input.setup.allyRoster, input.setup);
    REQUIRE(resolved.size() == 1);
    CHECK(resolved.front().physicalMemberCount == 3);
    CHECK(resolved.front().effectiveMemberCount == 4);
    CHECK(resolved.front().memberRoleIds == std::set<int>{1001, 1002, 1003});
    CHECK(resolved.front().activeThresholdIndex == 1);
    auto created = BattleRuntimeSession::createInitialized(std::move(input));
    auto runtime = created.session.runtime();
    REQUIRE(runtime.units.size() == 5);
    for (int id : {2, 3, 4, 10}) runtime.units.requireCore(id).vitals.hp = 60;

    BattleEffectEventBridge bridge;
    const auto kill = [&](int id, int frame, std::vector<int> survivingMemberIds)
    {
        const auto before = makeEffectUnitSnapshot(runtime, runtime.units.require(id));
        auto& dead = runtime.units.requireCore(id);
        dead.alive = false;
        dead.vitals.hp = 0;
        const DeathEventData death{
            .deadBefore = before,
            .deadAfter = makeEffectUnitSnapshot(runtime, runtime.units.require(id)),
            .cause = EffectEnvironmentDamageOrigin{},
            .allyOfOwner = true,
        };
        const auto healed = bridge.dispatch(runtime,
            {.frame = frame, .eventOrdinal = static_cast<std::uint64_t>(frame * 2), .ownerUnitId = id},
            EffectEvent::UnitDied, death);
        REQUIRE(healed.commands.size() == survivingMemberIds.size());
        for (const auto& command : healed.commands)
        {
            CHECK(std::ranges::find(survivingMemberIds, command.metadata.targetUnitId) != survivingMemberIds.end());
            CHECK(std::get<ChangeResourceEffectCommand>(command.value).resolvedAmount() == 15);
        }
        BattleEffectCommandSystem().reduce(runtime, healed.commands);
        for (const int ownerId : survivingMemberIds)
        {
            const auto buffs = bridge.dispatch(runtime,
                {.frame = frame, .eventOrdinal = static_cast<std::uint64_t>(frame * 10 + ownerId), .ownerUnitId = ownerId},
                EffectEvent::AllyDied, death);
            REQUIRE(buffs.commands.size() == 2 + (survivingMemberIds.size() == 1 ? 3 : 0));
            for (const auto& command : buffs.commands) CHECK(command.metadata.targetUnitId == ownerId);
            BattleEffectCommandSystem().reduce(runtime, buffs.commands);
        }
    };
    kill(1, 1, {2, 3});
    CHECK_FALSE(runtime.units.require(2).status.effects.has(BattleStatusKind::Berserk));
    CHECK(runtime.units.requireCore(2).vitals.hp == 75);
    CHECK(runtime.units.requireCore(3).vitals.hp == 75);
    kill(3, 2, {2});
    CHECK(runtime.units.require(2).status.effects.remainingFrames(BattleStatusKind::Berserk) == 9);
    CHECK(runtime.units.requireCore(2).vitals.hp == 90);
    for (int id : {4, 10})
    {
        CHECK(runtime.units.requireCore(id).vitals.hp == 60);
        CHECK_FALSE(runtime.units.require(id).status.effects.has(BattleStatusKind::Berserk));
    }
    for (const auto attribute : {BattleAttribute::Attack, BattleAttribute::Defence})
        CHECK(BattleEffectCommandSystem::queryAttribute(runtime,
            {.unitId = 2, .attribute = attribute, .baseValue = 100, .frame = 2}) == 134);
}

TEST_CASE("Alliance equipment membership excludes unequipped duplicates on either team",
          "[battle][initialization][alliance][equipment][berserk][death-heal][regression]")
{
    for (const int dyingTeam : {0, 1})
    {
        std::uint64_t nextRuleId{};
        BattleRuntimeSessionCreationInput input;
        input.rules = makeHadesBattleRuntimeRules(36.0, 18);
        input.setup.starGrowth.hpPerWin = 3;
        input.setup.starGrowth.attackPerWin = 2;
        input.setup.starGrowth.defencePerWin = 1;
        auto threshold = gameplayThreshold(4, R"([
            {類型: 同羈絆陣亡屬性加成, 屬性: 攻擊, 點數: 17},
            {類型: 陣亡治療羈絆成員, 生命百分比: 13},
            {類型: 最後羈絆成員狂暴, 暴擊百分比: 50, 冷卻縮減百分比: 20, 吸血百分比: 40, 持續幀數: 9}
        ])", nextRuleId);
        threshold.fightWinGrowth = {2, 3, 4};
        input.setup.comboDefinitions.push_back({
            .id = 7, .name = "明教", .memberRoleIds = {1001, 1002},
            .thresholds = {{.count = 2}, std::move(threshold)},
        });
        input.setup.equipmentDefinitions.push_back({.itemId = 777, .countsAsComboNames = {"明教"}});
        for (const int team : {0, 1})
        {
            input.setup.comboModifiersByTeam[team] = {7, 1, 6, 1};
            for (int offset = 1; offset <= 4; ++offset)
            {
                const int roleId = offset == 4 ? 1003 : 1000 + offset;
                addInitializedRuntimeTestUnit(input, team * 10 + offset, roleId, team,
                    10 + team * 20 + offset, 10 + team * 20,
                    team == 0 ? Towards_RightDown : Towards_LeftUp);
                auto& roster = team == 0 ? input.setup.allyRoster : input.setup.enemyRoster;
                roster.back().fightsWon = 2;
                if (offset == 3)
                {
                    input.units.back().weaponId = 777;
                    roster.back().weaponId = 777;
                }
            }
            const auto& roster = team == 0 ? input.setup.allyRoster : input.setup.enemyRoster;
            const auto resolved = resolveBattleSetupCombos(roster, input.setup);
            REQUIRE(resolved.size() == 1);
            CHECK(resolved.front().physicalMemberCount == 3);
            CHECK(resolved.front().effectiveMemberCount == 4);
            CHECK(resolved.front().activeThresholdIndex == 1);
            REQUIRE(resolved.front().contributions.size() == 3);
            CHECK(resolved.front().contributions.back().unitIds == std::vector<int>{team * 10 + 3});
        }
        auto created = BattleRuntimeSession::createInitialized(std::move(input));
        auto runtime = created.session.runtime();
        REQUIRE(runtime.units.size() == 8);
        for (const int team : {0, 1})
        {
            for (int offset = 1; offset <= 4; ++offset)
            {
                const int id = team * 10 + offset;
                CHECK(runtime.units.require(id).comboFacts.hasApplied(7) == (offset != 4));
                CHECK(runtime.units.requireCore(id).vitals.maxHp == (offset != 4 ? 130 : 126));
                CHECK(runtime.units.requireCore(id).stats.attack == (offset != 4 ? 40 : 34));
                CHECK(runtime.units.requireCore(id).stats.defence == (offset != 4 ? 30 : 22));
                runtime.units.requireCore(id).vitals.hp = 60;
            }
        }

        const int base = dyingTeam * 10;
        BattleEffectEventBridge bridge;
        for (const int offset : {1, 2})
        {
            const int deadId = base + offset;
            const auto before = makeEffectUnitSnapshot(runtime, runtime.units.require(deadId));
            auto& dead = runtime.units.requireCore(deadId);
            dead.alive = false;
            dead.vitals.hp = 0;
            const DeathEventData death{
                .deadBefore = before,
                .deadAfter = makeEffectUnitSnapshot(runtime, runtime.units.require(deadId)),
                .cause = EffectEnvironmentDamageOrigin{}, .allyOfOwner = true,
            };
            const auto heal = bridge.dispatch(runtime,
                {.frame = offset, .eventOrdinal = static_cast<std::uint64_t>(offset), .ownerUnitId = deadId},
                EffectEvent::UnitDied, death);
            REQUIRE(heal.commands.size() == 3 - offset);
            for (const auto& command : heal.commands)
            {
                CHECK(command.metadata.binding.sourceTeam == dyingTeam);
                CHECK(command.metadata.targetUnitId > deadId);
                CHECK(command.metadata.targetUnitId <= base + 3);
                CHECK(std::get<ChangeResourceEffectCommand>(command.value).resolvedAmount() == 16);
            }
            BattleEffectCommandSystem().reduce(runtime, heal.commands);
            const auto buffs = bridge.dispatch(runtime,
                {.frame = offset, .eventOrdinal = static_cast<std::uint64_t>(10 + offset), .ownerUnitId = base + 3},
                EffectEvent::AllyDied, death);
            REQUIRE(buffs.commands.size() == (offset == 2 ? 4 : 1));
            for (const auto& command : buffs.commands)
                CHECK(command.metadata.targetUnitId == base + 3);
            BattleEffectCommandSystem().reduce(runtime, buffs.commands);
            const auto plainBuffs = bridge.dispatch(runtime,
                {.frame = offset, .eventOrdinal = static_cast<std::uint64_t>(20 + offset), .ownerUnitId = base + 4},
                EffectEvent::AllyDied, death);
            CHECK(plainBuffs.commands.empty());
            CHECK(runtime.units.require(base + 3).status.effects.has(BattleStatusKind::Berserk) == (offset == 2));
            CHECK(runtime.units.requireCore(base + 3).vitals.hp == 60 + offset * 16);
        }
        CHECK(runtime.units.require(base + 3).status.effects.remainingFrames(BattleStatusKind::Berserk) == 9);
        for (const int id : {base + 4, (1 - dyingTeam) * 10 + 1, (1 - dyingTeam) * 10 + 2,
                            (1 - dyingTeam) * 10 + 3, (1 - dyingTeam) * 10 + 4})
        {
            CHECK(runtime.units.requireCore(id).vitals.hp == 60);
            CHECK_FALSE(runtime.units.require(id).status.effects.has(BattleStatusKind::Berserk));
        }
    }
}

TEST_CASE("Shendiao low health buffs only other allied combo members including with Alliance",
          "[battle][initialization][alliance][member-team-attack][regression]")
{
    for (const int team : {0, 1})
    for (const int sourceRoleId : {1001, 1002})
    for (const bool solo : {false, true})
    {
        CAPTURE(team, sourceRoleId, solo);
        std::uint64_t nextRuleId{};
        BattleRuntimeSessionCreationInput input;
        input.rules = makeHadesBattleRuntimeRules(36.0, 18);
        if (solo) input.setup.comboModifiersByTeam[team] = {7, 1, 6, 1};
        input.setup.comboDefinitions.push_back({
            .id = 7,
            .name = "神雕俠侶",
            .memberRoleIds = {1001, 1002},
            .thresholds = {gameplayThreshold(2,
                "[{類型: 成員低血全員增攻, 生命門檻百分比: 50, 攻擊百分比: 23, 持續幀數: 9}]", nextRuleId)},
        });
        const int partnerRoleId = sourceRoleId == 1001 ? 1002 : 1001;
        addInitializedRuntimeTestUnit(input, 1, sourceRoleId, team, 11, 10, Towards_RightDown);
        if (!solo) addInitializedRuntimeTestUnit(input, 2, partnerRoleId, team, 12, 10, Towards_RightDown);
        addInitializedRuntimeTestUnit(input, 3, 2002, team, 13, 10, Towards_RightDown);
        addInitializedRuntimeTestUnit(input, 10, partnerRoleId, 1 - team, 40, 30, Towards_LeftUp);
        const auto resolved = resolveBattleSetupCombos(
            team == 0 ? input.setup.allyRoster : input.setup.enemyRoster, input.setup);
        REQUIRE(resolved.size() == 1);
        CHECK(resolved.front().physicalMemberCount == (solo ? 1 : 2));
        CHECK(resolved.front().effectiveMemberCount == 2);
        CHECK(resolved.front().activeThresholdIndex == 0);
        auto created = BattleRuntimeSession::createInitialized(std::move(input));
        auto runtime = created.session.runtime();
        BattleEffectEventBridge bridge;
        const auto dispatch = [&](int frame)
        {
            const auto event = bridge.makeEvent(runtime,
                {.frame = frame, .eventOrdinal = static_cast<std::uint64_t>(frame), .ownerUnitId = 1},
                EffectEvent::FrameAdvanced, FrameTickEventData{.deltaFrames = 1});
            return bridge.dispatchFrameAdvanced(runtime, event);
        };
        const auto attack = [&](int id, int frame)
        {
            return BattleEffectCommandSystem::queryAttribute(runtime,
                {.unitId = id, .attribute = BattleAttribute::Attack, .baseValue = 100, .frame = frame});
        };

        runtime.units.requireCore(1).vitals.hp = 60;
        CHECK(dispatch(1).commands.empty());
        runtime.units.requireCore(1).vitals.hp = 59;
        const auto first = dispatch(2);
        REQUIRE(first.commands.size() == (solo ? 0 : 1));
        if (!solo)
        {
            CHECK(first.commands.front().metadata.binding.ownerUnitId == 1);
            CHECK(first.commands.front().metadata.targetUnitId == 2);
        }
        BattleEffectCommandSystem().reduce(runtime, first.commands);
        for (int id : {1, 3, 10}) CHECK(attack(id, 2) == 100);
        if (!solo)
        {
            CHECK(attack(2, 2) == 123);
            runtime.units.requireCore(2).vitals.hp = 59;
            const auto second = dispatch(3);
            REQUIRE(second.commands.size() == 1);
            CHECK(second.commands.front().metadata.binding.ownerUnitId == 2);
            CHECK(second.commands.front().metadata.targetUnitId == 1);
            BattleEffectCommandSystem().reduce(runtime, second.commands);
            CHECK(attack(1, 3) == 123);
            CHECK(attack(2, 3) == 123);
            CHECK(attack(3, 3) == 100);
            CHECK(attack(10, 3) == 100);
            CHECK(attack(2, 10) == 123);
            CHECK(attack(2, 11) == 100);
            CHECK(attack(1, 11) == 123);
            CHECK(attack(1, 12) == 100);
        }
        CHECK(dispatch(204).commands.empty());
    }
}
