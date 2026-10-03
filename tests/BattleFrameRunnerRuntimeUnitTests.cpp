#include "EffectCommandTestHelpers.h"
#include "battle/BattleCore.h"
#include "BattleLogTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "battle/BattleHitResolver.h"
#include "battle/BattlePresentationVisuals.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "battle/BattleStatusSystem.h"
#include "battle/BattleEffectEventBridge.h"
#include "BattleCoreTestHelpers.h"
#include "Find.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <string_view>
#include <utility>
#include <vector>

using namespace KysChess::Battle;
using namespace KysChess;
using namespace BattlePresentationTest;

namespace
{

constexpr double SceneTileWidth = 36.0;
constexpr double MaxEffectiveBattleReach = 480.0;
constexpr double TestMinimumVectorNorm = 0.0001;

BattleAttackPayload ordinaryProjectilePayload()
{
    return {
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary,
    };
}

BattlePresentationFrame runBattleFrame(BattleRuntimeState& state)
{
    return BattleFrameRunner().runFrame(state);
}

void seedDamageExtrasFromUnits(BattleRuntimeState& state);

BattleStatusContribution& appendStatus(
    BattleStatusEffectState& effects,
    BattleStatusKind kind,
    int remainingFrames,
    int stacks = 1,
    int potency = 0,
    int sourceUnitId = -1,
    int tickFramesRemaining = 0,
    int maximumFrames = 0)
{
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 8000 + static_cast<int>(kind),
        .ownerUnitId = sourceUnitId >= 0 ? sourceUnitId : 0,
    };
    const EffectRuleId ruleId{
        static_cast<std::uint64_t>(8000 + static_cast<int>(kind)) };
    auto behavior = kind == BattleStatusKind::Poison
        ? KysChess::Battle::Test::poisonStatusBehavior(potency)
        : (kind == BattleStatusKind::Bleed
            ? makeRuntimeBleedStatusBehavior()
            : nullptr);
    effects.statuses.push_back({
        .kind = kind,
        .producer = behavior
            ? std::optional{ StatusProducerKey{ .binding = binding, .ruleId = ruleId } }
            : std::nullopt,
        .producerFamily = behavior
            ? std::optional{ StatusProducerFamilyKey{
                .sourceKind = binding.kind,
                .sourceId = binding.sourceId,
                .logicalOwnerUnitId = binding.ownerUnitId,
                .ruleId = ruleId,
            } }
            : std::nullopt,
        .targetTotalLimit = kind == BattleStatusKind::Bleed
            ? std::optional{ stacks }
            : std::nullopt,
        .behavior = behavior,
        .behaviorRuntime = behavior
            ? std::vector{ EffectRuleRuntimeState{
                .intervalFramesRemaining = tickFramesRemaining > 0
                    ? tickFramesRemaining
                    : behavior->rules.front().intervalFrames,
            } }
            : std::vector<EffectRuleRuntimeState>{},
        .sourceUnitId = sourceUnitId,
        .remainingFrames = remainingFrames,
        .maximumFrames = std::max(remainingFrames, maximumFrames),
        .stacks = stacks,
        .origin = behavior
            ? std::optional{ BattleStatusEffectOrigin{ binding, ruleId, 0 } }
            : std::nullopt,
        .appliedSequence = effects.nextStatusSequence++,
    });
    return effects.statuses.back();
}

void appendOwnerEffectRule(
    BattleRuntimeState& state,
    int ownerUnitId,
    int sourceId,
    KysChess::EffectRule rule)
{
    state.effectRules.append({
        .kind = KysChess::EffectSourceKind::Combo,
        .sourceId = sourceId,
        .ownerUnitId = ownerUnitId,
        .sourceTeam = state.units.requireCore(ownerUnitId).team,
    }, rule);
}

BattleMovementConfig runtimeMovementConfig()
{
    BattleMovementGeometry geometry;
    geometry.tileWidth = SceneTileWidth;
    geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    geometry.meleeAttackHitRadius = SceneTileWidth * 2.0;
    geometry.dashFrames = 5;
    geometry.dashCooldownFrames = 18;
    geometry.maxRangedReach = MaxEffectiveBattleReach;
    return BattleGeometry(geometry).movementConfig();
}

BattleUnitState runtimeUnit(int id, int team, Pointf position)
{
    BattleUnitState state;
    state.id = id;
    state.team = team;
    state.alive = true;
    state.position = position;
    state.speed = 5.0;
    state.reach = 137.5;
    return state;
}

BattleRuntimeUnit runtimeUnitFromWorld(const BattleUnitState& worldUnit)
{
    BattleRuntimeUnit unit;
    unit.id = worldUnit.id;
    unit.realRoleId = worldUnit.id;
    unit.team = worldUnit.team;
    unit.alive = worldUnit.alive;
    unit.vitals = { 100, 100, 20, 100 };
    unit.stats.speed = static_cast<int>(worldUnit.speed);
    unit.motion.position = worldUnit.position;
    unit.motion.velocity = worldUnit.velocity;
    unit.style = worldUnit.style;
    return unit;
}

void configureRuntimeGrid(BattleRuntimeState& state)
{
    constexpr int CoordCount = 18;
    state.gridTransform = { SceneTileWidth, CoordCount };
    state.movementPhysics.terrain.tileWidth = SceneTileWidth;
    state.movementPhysics.terrain.coordCount = CoordCount;
    state.movementPhysics.terrain.defaultSeparationDistance = SceneTileWidth * 1.5;
    state.movementPhysics.terrain.walkableByCell.assign(CoordCount * CoordCount, 1);
}

void seedCanonicalUnitsFromMovementUnits(BattleRuntimeState& state, const std::vector<BattleUnitState>& units)
{
    state.units = {};
    for (const auto& worldUnit : units)
    {
        appendRuntimeUnit(
            state,
            makeRuntimeUnitSpawn(runtimeUnitFromWorld(worldUnit)));
    }
}

BattleRuntimeState runtimeFrameState()
{
    BattleRuntimeState state;
    state.movement.frame = 6;
    state.movement.config = runtimeMovementConfig();
    configureRuntimeGrid(state);
    seedCanonicalUnitsFromMovementUnits(state, {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    });
    state.attacks.hitRadius = SceneTileWidth * 2.0;
    state.attacks.minimumVectorNorm = TestMinimumVectorNorm;
    state.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    state.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    seedDamageExtrasFromUnits(state);
    return state;
}

BattleRuntimeState ownedRuntimeState()
{
    BattleRuntimeState runtime;
    runtime.movement.frame = 6;
    runtime.movement.config = runtimeMovementConfig();
    configureRuntimeGrid(runtime);
    seedCanonicalUnitsFromMovementUnits(runtime, {
        runtimeUnit(0, 0, { 100, 100, 0 }),
        runtimeUnit(1, 1, { 500, 100, 0 }),
    });
    runtime.attacks.hitRadius = SceneTileWidth * 2.0;
    runtime.attacks.minimumVectorNorm = TestMinimumVectorNorm;
    runtime.attacks.bounceSpawnDistance = SceneTileWidth * 1.5;
    runtime.attacks.defaultProjectileSpeed = SceneTileWidth / 3.0;
    seedDamageExtrasFromUnits(runtime);
    return runtime;
}

BattleRuntimeState forceMoveRuntimeState(double blockerOffset)
{
    auto runtime = ownedRuntimeState();
    constexpr float TargetX = 18.0f * static_cast<float>(SceneTileWidth);
    constexpr float PositionY = 3.0f * static_cast<float>(SceneTileWidth);
    seedCanonicalUnitsFromMovementUnits(runtime, {
        runtimeUnit(0, 0, { TargetX - static_cast<float>(SceneTileWidth), PositionY, 0 }),
        runtimeUnit(1, 1, { TargetX, PositionY, 0 }),
        runtimeUnit(2, 1, { TargetX + static_cast<float>(blockerOffset), PositionY, 0 }),
    });
    for (auto& record : runtime.units.all())
    {
        record.core.stats.speed = 0;
    }
    return runtime;
}

void queueForceMoveEffect(
    BattleRuntimeState& state,
    KysChess::ForceMoveAction action)
{
    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Combo,
        .sourceId = 9003,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 1 };
    metadata.event = KysChess::EffectEvent::HitBeforeDamage;
    metadata.targetUnitId = 1;
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ EffectCommand{
            .metadata = metadata,
            .value = ForceMoveEffectCommand{ std::move(action) },
        } }, { .frame = state.movement.frame + 1 }) });
}

BattleAttackInstance cancelProjectile(int id, int attackerUnitId)
{
    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = id;
    attack.frame = 5;
    attack.state.attackSourceUnitId = attackerUnitId;
    attack.state.totalFrame = 30;
    attack.state.operationType = BattleOperationType::RangedProjectile;
    attack.state.position = { 500, 500, 0 };
    return attack;
}

void registerEffectCastContext(
    BattleRuntimeState& state,
    const BattleCastStart& cast,
    int originalTargetUnitId)
{
    state.effectIntegration.casts.emplace(
        cast.provenance.castId,
        BattleEffectCastRuntimeContext{
            .originalTargetUnitId = originalTargetUnitId,
        });
}

void queueTrackedRootAttack(
    BattleRuntimeState& state,
    BattleAttackSpawnRequest request)
{
    assert(!request.provenance.valid());
    assert(!request.castWork.valid());
    const auto cast = state.castLifecycle.beginRootCast({
        .sourceUnitId = request.initial.attackSourceUnitId,
        .magicId = request.initial.skillId,
    });
    const auto reservation = state.castLifecycle.reserveAttack(
        cast.provenance.castId,
        { .rootAttack = true });
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;
    registerEffectCastContext(
        state,
        cast,
        request.initial.preferredTargetUnitId);
    state.castLifecycle.completeWork(cast.commitBarrier);
    state.nextFrame.queueAttack(std::move(request));
}

void appendTrackedRootAttack(
    BattleRuntimeState& state,
    BattleAttackInstance attack)
{
    assert(attack.id >= 0);
    assert(!attack.provenance.valid());
    assert(!attack.castWork.valid());
    const auto cast = state.castLifecycle.beginRootCast({
        .sourceUnitId = attack.state.attackSourceUnitId,
        .magicId = attack.state.skillId,
    });
    const auto reservation = state.castLifecycle.reserveAttack(
        cast.provenance.castId,
        { .rootAttack = true });
    attack.provenance = completeAttackProvenance(
        reservation.provenance,
        battleAttackIdFromRuntimeId(attack.id));
    attack.castWork = reservation.work;
    state.castLifecycle.transferToLiveAttack(
        attack.castWork,
        attack.provenance.attackId);
    registerEffectCastContext(
        state,
        cast,
        attack.state.preferredTargetUnitId);
    state.castLifecycle.completeWork(cast.commitBarrier);
    state.attacks.attacks.push_back(std::move(attack));
}

BattleRuntimeUnit teamRuntimeUnit(int id, int team, int hp)
{
    BattleRuntimeUnit unit;
    unit.id = id;
    unit.team = team;
    unit.alive = true;
    unit.vitals.hp = hp;
    unit.vitals.maxHp = 100;
    unit.vitals.mp = 20;
    unit.vitals.maxMp = 100;
    return unit;
}

void seedRuntimeUnits(BattleRuntimeState& state, const std::vector<BattleRuntimeUnit>& units)
{
    std::map<int, BattleComboRuntimeFacts> savedComboFacts;
    for (const auto& record : state.units.all())
    {
        savedComboFacts.emplace(record.id(), record.comboFacts);
    }
    std::map<int, BattleStatusRuntimeUnit> previousStatuses;
    for (const auto& record : state.units.all())
    {
        previousStatuses.emplace(record.id(), record.status);
    }
    std::map<int, BattleDamageRuntimeUnit> previousDamageExtras;
    for (const auto& record : state.units.all())
    {
        previousDamageExtras.emplace(record.id(), record.damage);
    }

    state.units = {};
    state.movement.movementReservations.clear();
    state.damage.presentationStylesByDefender.clear();
    for (auto unit : units)
    {
        BattleComboRuntimeFacts comboFacts;
        if (const auto comboIt = savedComboFacts.find(unit.id);
            comboIt != savedComboFacts.end())
        {
            comboFacts = comboIt->second;
        }

        auto spawn = makeRuntimeUnitSpawn(std::move(unit), std::move(comboFacts));
        if (const auto statusIt = previousStatuses.find(spawn.unit.id);
            statusIt != previousStatuses.end())
        {
            spawn.status = statusIt->second;
        }
        if (const auto damageIt = previousDamageExtras.find(spawn.unit.id);
            damageIt != previousDamageExtras.end())
        {
            spawn.damage = damageIt->second;
        }
        appendRuntimeUnit(state, std::move(spawn));
    }
}

BattleStatusRuntimeUnit runtimeStatusUnit(const BattleStatusUnitState& unit)
{
    return makeBattleStatusRuntimeUnit(unit);
}

void seedDamageExtrasFromUnits(BattleRuntimeState& state)
{
    for (const auto& unit : state.units.cores())
    {
        state.units.require(unit.id).damage = makeBattleDamageRuntimeUnit(
            makeBattleDamageUnitState(unit, static_cast<const BattleDamageRuntimeUnit*>(nullptr)));
    }
}

std::vector<const BattleVisualEvent*> semanticCueEvents(
    const BattlePresentationFrame& frame)
{
    std::vector<const BattleVisualEvent*> events;
    for (const auto& event : frame.visualEvents)
    {
        if (event.type == BattleVisualEventType::RoleEffect
            && event.visualPath.starts_with(BattleCueVisualPathPrefix))
        {
            events.push_back(&event);
        }
    }
    return events;
}

EffectCommandMetadata cueEffectMetadata(
    const BattleRuntimeState& state,
    EffectEvent event = EffectEvent::HitBeforeDamage,
    int targetUnitId = 1)
{
    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 500,
        .ownerUnitId = 0,
        .sourceTeam = state.units.requireCore(0).team,
    };
    metadata.ruleId = EffectRuleId{ 1 };
    metadata.event = event;
    metadata.targetUnitId = targetUnitId;
    return metadata;
}

void queueEffectCommandBatch(
    BattleRuntimeState& state,
    std::vector<EffectCommand> commands)
{
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::move(commands), { .frame = state.movement.frame + 1 }) });
}

std::vector<BattleLogEvent> effectLogs(
    const BattlePresentationFrame& frame,
    BattleStatusSemanticId status)
{
    std::vector<BattleLogEvent> result;
    for (const auto& log : frame.logEvents)
    {
        if (log.statusId == status) result.push_back(log);
    }
    return result;
}

}  // namespace

TEST_CASE("BattleFrameRunner_LogsAreaCreationRefreshAndRemovalWithItsSource", "[battle][logging][area]")
{
    auto state = runtimeFrameState();
    auto metadata = cueEffectMetadata(state);
    state.effectSourceNames[{ EffectSourceKind::Magic, 500 }] = "測試區域";
    CreateAreaEffectCommand area;
    area.request.source = metadata.binding;
    area.request.ruleId = metadata.ruleId;
    area.request.geometry = { .shape = AreaShape::Circle, .radiusTiles = 5 };
    area.request.anchor = { .kind = BattleAreaAnchorKind::FollowSourceUnit, .sourceUnitId = 0 };
    area.request.durationFrames = 3;
    area.request.modifiers = { { .kind = AreaModifierKind::ForcedMoveImmunity,
        .relation = EffectTeamFilter::Ally } };
    area.request.sourceDeath = AreaSourceDeathPolicy::RemoveImmediately;
    area.request.merge = AreaMergePolicy::RefreshSameSource;
    const auto apply = [&]
    {
        area.request.currentFrame = state.movement.frame + 1;
        queueEffectCommandBatch(state, { { metadata, area } });
        return runBattleFrame(state);
    };
    const auto created = effectLogs(apply(), BattleStatusSemanticId::AreaCreated);
    REQUIRE(created.size() == 1);
    CHECK(created.front().amount == 3);
    CHECK(created.front().sourceUnitId == 0);
    CHECK(created.front().semanticSourceName == "測試區域");
    CHECK(created.front().semanticSourceKind == "magic");
    CHECK(created.front().frame == state.movement.frame);
    const auto refreshed = effectLogs(apply(), BattleStatusSemanticId::AreaRefreshed);
    REQUIRE(refreshed.size() == 1);
    CHECK(refreshed.front().effectId == created.front().effectId);
    CHECK(refreshed.front().amount == 3);

    SECTION("到期只記錄一次")
    {
        CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::AreaRemoved).empty());
        CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::AreaRemoved).empty());
        const auto removed = effectLogs(runBattleFrame(state), BattleStatusSemanticId::AreaRemoved);
        REQUIRE(removed.size() == 1);
        CHECK(removed.front().effectId == created.front().effectId);
        CHECK(removed.front().semanticSourceName == "測試區域");
        CHECK(removed.front().frame == state.movement.frame);
        CHECK(state.areas.areas.empty());
        CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::AreaRemoved).empty());
    }
    SECTION("來源陣亡即移除")
    {
        state.nextFrame.queueDamage({ .request = {
            .attackerUnitId = 1, .defenderUnitId = 0,
            .baseDamage = 10000, .preResolvedDamage = true,
        } });
        const auto removed = effectLogs(runBattleFrame(state), BattleStatusSemanticId::AreaRemoved);
        REQUIRE(removed.size() == 1);
        CHECK(removed.front().semanticSourceName == "測試區域");
        CHECK(state.areas.areas.empty());
    }
}

TEST_CASE("BattleFrameRunner_LogsOnlySuccessfulCleanseAndStatusConsumption", "[battle][logging][status]")
{
    auto state = runtimeFrameState();
    const auto metadata = cueEffectMetadata(state);
    state.effectSourceNames[{ EffectSourceKind::Magic, 500 }] = "測試狀態";
    SECTION("淨化包含負面修正但不記錄空操作")
    {
        ApplyStatusAction stun;
        stun.status = BattleStatusKind::Stun;
        stun.durationFrames = 30;
        stun.quantity = NoStatusQuantity{};
        stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
        ModifyAttributeAction slow;
        slow.attribute = BattleAttribute::Speed;
        slow.operation = AttributeOperation::PercentAdd;
        slow.durationFrames = 30;
        queueEffectCommandBatch(state, {
            { metadata, KysChess::Battle::Test::statusApplication(stun) },
            { metadata, prepareModifyAttribute(slow, -20) },
        });
        runBattleFrame(state);
        RemoveStatusAction cleanse;
        cleanse.negativeOnly = true;
        const EffectCommand command{ metadata, KysChess::Battle::Test::statusRemoval(cleanse) };
        queueEffectCommandBatch(state, { command });
        const auto removed = effectLogs(runBattleFrame(state), BattleStatusSemanticId::StatusRemoved);
        REQUIRE(removed.size() == 2);
        CHECK(std::ranges::all_of(removed, [](const auto& log)
        {
            return log.sourceUnitId == 0 && log.targetUnitId == 1
                && log.semanticSourceName == "測試狀態";
        }));
        CHECK_FALSE(state.units.require(1).frozen());
        CHECK(state.effectCommands.attributeModifiers.empty());
        queueEffectCommandBatch(state, { command });
        CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::StatusRemoved).empty());
    }
    SECTION("消耗至零會記錄後續眩暈")
    {
        appendStatus(state.units.require(1).status.effects, BattleStatusKind::SevenStarMark, 150, 1, 0, 0);
        ApplyStatusAction stun;
        stun.status = BattleStatusKind::Stun;
        stun.durationFrames = 30;
        stun.quantity = NoStatusQuantity{};
        stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
        const EffectCommand command{ metadata, ConsumeStatusEffectCommand{
            .request = { .kind = BattleStatusKind::SevenStarMark },
            .whenDepleted = KysChess::Battle::Test::statusApplication(stun),
        } };
        queueEffectCommandBatch(state, { command });
        const auto frame = runBattleFrame(state);
        const auto consumed = effectLogs(frame, BattleStatusSemanticId::StatusConsumed);
        REQUIRE(consumed.size() == 1);
        CHECK(consumed.front().amount == 1);
        CHECK(consumed.front().previousAmount == 1);
        CHECK(consumed.front().newAmount == 0);
        CHECK(effectLogs(frame, BattleStatusSemanticId::Stun).size() == 1);
        CHECK(state.units.require(1).frozen());
        queueEffectCommandBatch(state, { command });
        const auto noop = runBattleFrame(state);
        CHECK(effectLogs(noop, BattleStatusSemanticId::StatusConsumed).empty());
        CHECK(effectLogs(noop, BattleStatusSemanticId::Stun).empty());
    }
}

TEST_CASE("BattleFrameRunner_LogsCappedModifierChangesButNotIdenticalPermanentApplications", "[battle][logging][modifier]")
{
    for (const bool damageModifier : { false, true })
    {
        for (const int duration : { 0, 30 })
        {
            auto state = runtimeFrameState();
            const auto metadata = cueEffectMetadata(state);
            const auto status = damageModifier ? BattleStatusSemanticId::DamageModifier : BattleStatusSemanticId::AttributeModifier;
            const auto apply = [&](int amount)
            {
                EffectCommand command;
                command.metadata = metadata;
                if (damageModifier)
                {
                    ModifyDamageAction action;
                    action.operation = DamageModifierOperation::PercentAdd;
                    action.durationFrames = duration;
                    action.stack = EffectStackPolicy::AddStack;
                    action.stackLimit = 1;
                    command.value = prepareModifyDamage(action, amount);
                }
                else
                {
                    ModifyAttributeAction action;
                    action.attribute = BattleAttribute::Attack;
                    action.durationFrames = duration;
                    action.stack = EffectStackPolicy::AddStack;
                    action.stackLimit = 1;
                    command.value = prepareModifyAttribute(action, amount);
                }
                queueEffectCommandBatch(state, { command });
                return effectLogs(runBattleFrame(state), status);
            };
            REQUIRE(apply(20).size() == 1);
            CHECK(apply(20).size() == (duration == 0 ? 0 : 1));
            const auto changed = apply(40);
            REQUIRE(changed.size() == 1);
            CHECK(changed.front().amount == 40);
            CHECK(changed.front().stackCount == 1);
            CHECK(BattleLogTest::joinSegments(changed.front().segments)
                == (duration == 0
                    ? (damageModifier ? "輸出傷害百分比+40%" : "攻擊數值+40")
                    : (damageModifier ? "輸出傷害百分比+40%（30幀）" : "攻擊數值+40（30幀）")));
        }
    }
}

TEST_CASE("BattleFrameRunner_LogsAbsorptionActivationRefreshAndEmptySettlement", "[battle][logging][absorption]")
{
    auto state = runtimeFrameState();
    const auto metadata = cueEffectMetadata(state, EffectEvent::AttackCommitted, 0);
    state.effectSourceNames[{ EffectSourceKind::Magic, 500 }] = "吸收測試";
    StartDamageAbsorptionAction absorption;
    absorption.slot = EffectStateSlot::AbsorbedDamage;
    absorption.absorbedPct = 40;
    absorption.durationFrames = 2;
    absorption.settlementTarget.kind = EffectSelectorKind::Enemies;
    const EffectCommand command{ metadata, StateMachineEffectCommand{ absorption } };
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        queueEffectCommandBatch(state, { command });
        const auto logs = effectLogs(runBattleFrame(state), BattleStatusSemanticId::DamageAbsorption);
        REQUIRE(logs.size() == 1);
        CHECK(logs.front().amount == 40);
        CHECK(logs.front().semanticSourceName == "吸收測試");
    }
    CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::DamageAbsorptionEnded).empty());
    const auto ended = effectLogs(runBattleFrame(state), BattleStatusSemanticId::DamageAbsorptionEnded);
    REQUIRE(ended.size() == 1);
    CHECK(ended.front().amount == 0);
    CHECK(ended.front().secondaryAmount == 0);
    CHECK(ended.front().semanticSourceName == "吸收測試");
    CHECK(effectLogs(runBattleFrame(state), BattleStatusSemanticId::DamageAbsorptionEnded).empty());
}

TEST_CASE("BattleFrameRunner_LogsDamageBlockAndHitCapConsumption", "[battle][logging][status][charge]")
{
    for (const auto kind : { BattleStatusKind::DamageBlockLayer, BattleStatusKind::SingleHitCapLayer })
    {
        for (const int damage : { 20, 50 })
        {
            CAPTURE(kind, damage);
            auto state = runtimeFrameState();
            const auto metadata = cueEffectMetadata(state);
            state.effectSourceNames[{ EffectSourceKind::Magic, 500 }] = "護身測試";
            ApplyStatusAction protection;
            protection.status = kind;
            if (kind == BattleStatusKind::DamageBlockLayer)
            {
                protection.quantity = SetDamageBlockCharges{ 1 };
                protection.behavior = KysChess::Battle::Test::damageBlockStatusBehavior();
            }
            else
            {
                protection.quantity = SetStatusTriggerCharges{ 1 };
                protection.behavior = KysChess::Battle::Test::singleHitCapStatusBehavior(30);
            }
            queueEffectCommandBatch(state, { { metadata, KysChess::Battle::Test::statusApplication(protection) } });
            runBattleFrame(state);
            const int beforeHp = state.units.requireCore(1).vitals.hp;
            state.nextFrame.queueDamage({ .request = {
                .attackerUnitId = 0, .defenderUnitId = 1,
                .baseDamage = damage, .preResolvedDamage = true,
            } });
            const auto logs = effectLogs(runBattleFrame(state), BattleStatusSemanticId::StatusConsumed);
            REQUIRE(logs.size() == 1);
            CHECK(logs.front().semanticSourceName == "護身測試");
            CHECK(logs.front().sourceUnitId == 0);
            CHECK(logs.front().targetUnitId == 1);
            CHECK(logs.front().amount == 1);
            CHECK(logs.front().newAmount == 0);
            CHECK_FALSE(state.units.require(1).statusEffects().has(kind));
            CHECK(state.units.requireCore(1).vitals.hp
                == beforeHp - (kind == BattleStatusKind::DamageBlockLayer ? 0 : std::min(damage, 30)));
        }
    }
}

TEST_CASE("BattleFrameRunner_LogsTileMovementOnlyWhenImpulseIsApplied", "[battle][logging][force-move]")
{
    for (const auto direction : { ForceMoveDirection::AwayFromSource, ForceMoveDirection::TowardSource })
    {
        for (const bool immune : { false, true })
        {
            CAPTURE(direction, immune);
            auto state = runtimeFrameState();
            const auto metadata = cueEffectMetadata(state);
            state.effectSourceNames[{ EffectSourceKind::Magic, 500 }] = "移動測試";
            if (immune)
            {
                BattleAreaCreateRequest ward;
                ward.source = metadata.binding;
                ward.currentFrame = state.movement.frame;
                ward.durationFrames = 100;
                ward.geometry = { .shape = AreaShape::Circle, .radiusTiles = 100 };
                ward.anchor = { .kind = BattleAreaAnchorKind::FollowSourceUnit, .sourceUnitId = 0 };
                ward.modifiers = { { .kind = AreaModifierKind::ForcedMoveImmunity,
                    .relation = EffectTeamFilter::Any, .blockedDirection = direction } };
                BattleAreaEffectSystem::create(state.areas, std::move(ward));
            }
            ForceMoveAction move;
            move.direction = direction;
            move.distanceTiles = 3;
            move.lockFrames = 10;
            queueEffectCommandBatch(state, { { metadata, ForceMoveEffectCommand{ move } } });
            const auto logs = effectLogs(runBattleFrame(state), BattleStatusSemanticId::Knockback);
            REQUIRE(logs.size() == (immune ? 0 : 1));
            if (!immune)
            {
                CHECK(logs.front().amount == 3 * SceneTileWidth);
                CHECK(logs.front().semanticSourceName == "移動測試");
                CHECK(logs.front().targetUnitId == 1);
            }
        }
    }
}

TEST_CASE("BattleFrameRunner_SuppressesDebuffCuesAndKeepsPositiveStatusColors", "[battle][frame_runner][runtime][effect_cue]")
{
    struct Case
    {
        BattleStatusKind status{};
        std::string_view path;
        BattlePresentationColor color;
    };
    const std::array cases{
        Case{ BattleStatusKind::Poison, {}, {} },
        Case{ BattleStatusKind::Bleed, {}, {} },
        Case{ BattleStatusKind::Stun, {}, {} },
        Case{ BattleStatusKind::WitheredBone, {}, {} },
        Case{ BattleStatusKind::DamageBlockLayer, BattleCuePositiveVisualPath, { 112, 224, 255, 210 } },
        Case{ BattleStatusKind::BattleSpirit, BattleCuePositiveVisualPath, { 255, 204, 96, 220 } },
    };

    for (const auto& test : cases)
    {
        auto state = runtimeFrameState();
        auto metadata = cueEffectMetadata(state);
        ApplyStatusAction action;
        action.status = test.status;
        action.durationFrames = 60;
        switch (test.status)
        {
        case BattleStatusKind::Poison:
            action.quantity = SetStatusTriggerCharges{ 3 };
            action.reapplication = StatusReapplicationPolicy::ReplaceExistingPoison;
            action.behavior = KysChess::Battle::Test::poisonStatusBehavior(10);
            break;
        case BattleStatusKind::Bleed:
            action.durationFrames = 0;
            action.quantity = AddSharedStatusLayers{ 1, 3 };
            action.behavior = makeCatalogOwnedStatusBehavior(action);
            break;
        case BattleStatusKind::Stun:
            action.quantity = NoStatusQuantity{};
            action.reapplication = StatusReapplicationPolicy::KeepLongerDuration;
            break;
        case BattleStatusKind::WitheredBone:
            action.quantity = NoStatusQuantity{};
            action.behavior = makeCatalogOwnedStatusBehavior(action);
            break;
        case BattleStatusKind::DamageBlockLayer:
            action.durationFrames = 0;
            action.quantity = SetDamageBlockCharges{ 1 };
            action.behavior = KysChess::Battle::Test::damageBlockStatusBehavior();
            break;
        case BattleStatusKind::BattleSpirit:
            action.durationFrames = 0;
            action.quantity = AddStatusLayers{ 1, 5 };
            action.behavior = KysChess::Battle::Test::battleSpiritStatusBehavior(10, 1);
            break;
        default:
            FAIL("unexpected status cue case");
        }
        queueEffectCommandBatch(state, {
            EffectCommand{
                metadata,
                KysChess::Battle::Test::statusApplication(action),
            },
        });

        const auto frame = runBattleFrame(state);
        const auto cues = semanticCueEvents(frame);
        CHECK(state.units.require(1).status.effects.has(test.status));
        if (test.path.empty())
        {
            CHECK(cues.empty());
            continue;
        }
        REQUIRE(cues.size() == 1);
        CHECK(cues.front()->targetUnitId == 1);
        CHECK(cues.front()->visualPath == test.path);
        CHECK(cues.front()->color.r == test.color.r);
        CHECK(cues.front()->color.g == test.color.g);
        CHECK(cues.front()->color.b == test.color.b);
        CHECK(cues.front()->color.a == test.color.a);
        CHECK(std::ranges::none_of(
            frame.visualEvents,
            [](const BattleVisualEvent& event)
            {
                return event.type == BattleVisualEventType::FloatingText;
            }));
    }
}

TEST_CASE("BattleFrameRunner_CoalescesProtectionCuesAndSuppressesRefreshAndInitialization", "[battle][frame_runner][runtime][effect_cue]")
{
    auto state = runtimeFrameState();
    auto metadata = cueEffectMetadata(state);

    ModifyAttributeAction blockChance;
    blockChance.attribute = BattleAttribute::BlockChance;
    blockChance.operation = AttributeOperation::FlatAdd;
    blockChance.durationFrames = 60;
    blockChance.stack = EffectStackPolicy::Refresh;

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;

    auto shieldMetadata = metadata;
    shieldMetadata.actionOrder = 1;
    shieldMetadata.commandOrdinal = 2;
    queueEffectCommandBatch(state, {
        EffectCommand{ metadata, prepareModifyAttribute(blockChance, 20) },
        EffectCommand{ shieldMetadata, prepareChangeResource(shield, 25) },
    });

    const auto applied = runBattleFrame(state);
    const auto appliedCues = semanticCueEvents(applied);
    REQUIRE(appliedCues.size() == 1);
    CHECK(appliedCues.front()->visualPath == BattleCuePositiveVisualPath);
    CHECK(appliedCues.front()->color.r == 112);
    CHECK(appliedCues.front()->color.g == 224);
    CHECK(appliedCues.front()->color.b == 255);

    queueEffectCommandBatch(state, {
        EffectCommand{ metadata, prepareModifyAttribute(blockChance, 20) },
    });
    CHECK(semanticCueEvents(runBattleFrame(state)).empty());

    auto openingState = runtimeFrameState();
    auto openingMetadata = cueEffectMetadata(
        openingState,
        EffectEvent::BattleInitialized);
    queueEffectCommandBatch(openingState, {
        EffectCommand{
            openingMetadata,
            prepareModifyAttribute(blockChance, 20),
        },
    });
    CHECK(semanticCueEvents(runBattleFrame(openingState)).empty());
}

TEST_CASE("BattleFrameRunner_MaintainedAttackBuffStaysSilentAcrossExpiry", "[battle][frame_runner][runtime][effect_cue][logging]")
{
    auto state = runtimeFrameState();
    ModifyAttributeAction buff;
    buff.attribute = BattleAttribute::Attack;
    buff.operation = AttributeOperation::FlatAdd;
    buff.durationFrames = 1;
    buff.stack = EffectStackPolicy::Refresh;
    for (int frame = 0; frame < 30; ++frame)
    {
        queueEffectCommandBatch(state, {
            EffectCommand{cueEffectMetadata(state, EffectEvent::FrameAdvanced),
                prepareModifyAttribute(buff, 150)},
        });
        const auto presentation = runBattleFrame(state);
        CHECK(semanticCueEvents(presentation).empty());
        CHECK(effectLogs(presentation, BattleStatusSemanticId::AttributeModifier).empty());
        CHECK(BattleEffectCommandSystem::queryAttribute(state, {
            .unitId = 1,
            .attribute = BattleAttribute::Attack,
            .baseValue = 100,
            .frame = state.movement.frame,
        }) == 250);
    }
}

TEST_CASE("BattleFrameRunner_GuaranteedHitCuesEachRecipientWithoutRefreshFlash", "[battle][ultimate][effect_cue]")
{
    auto state = runtimeFrameState();
    ModifyAttributeAction buff;
    buff.attribute = BattleAttribute::GuaranteedHit;
    buff.operation = AttributeOperation::Override;
    buff.durationFrames = 100;
    buff.stack = EffectStackPolicy::Refresh;
    const auto apply = [&] {
        for (int id : {0, 1})
            queueEffectCommandBatch(state, {
                EffectCommand{cueEffectMetadata(state, EffectEvent::AttackCommitted, id),
                    prepareModifyAttribute(buff, 1)},
            });
        return runBattleFrame(state);
    };
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        const auto frame = apply();
        const auto cues = semanticCueEvents(frame);
        if (repeat > 0)
        {
            CHECK(cues.empty());
            continue;
        }
        REQUIRE(cues.size() == 2);
        for (const auto* cue : cues)
        {
            CHECK(cue->durationFrames == 12);
            CHECK(cue->visualPath == BattleCueSwordVisualPath);
            CHECK(cue->color.r == 160);
            CHECK(cue->color.g == 225);
            CHECK(cue->color.b == 255);
        }
        CHECK(cues[0]->targetUnitId != cues[1]->targetUnitId);
    }
}

TEST_CASE("BattleFrameRunner_OnlyCuesFirstStackAndSuccessfulCleanse", "[battle][frame_runner][runtime][effect_cue]")
{
    SECTION("只有第一層會顯示提示")
    {
        auto state = runtimeFrameState();
        auto metadata = cueEffectMetadata(state);
        ApplyStatusAction action;
        action.status = BattleStatusKind::BattleSpirit;
        action.quantity = AddStatusLayers{ 1, 5 };
        action.behavior = KysChess::Battle::Test::battleSpiritStatusBehavior(10, 1);
        const EffectCommand command{
            metadata,
            KysChess::Battle::Test::statusApplication(action),
        };

        queueEffectCommandBatch(state, { command });
        CHECK(semanticCueEvents(runBattleFrame(state)).size() == 1);
        queueEffectCommandBatch(state, { command });
        CHECK(semanticCueEvents(runBattleFrame(state)).empty());
    }

    SECTION("只有實際移除負面狀態才顯示淨化提示")
    {
        auto state = runtimeFrameState();
        appendStatus(
            state.units.require(1).status.effects,
            BattleStatusKind::Poison,
            90,
            1,
            10,
            0);
        auto metadata = cueEffectMetadata(state);
        RemoveStatusAction action;
        action.negativeOnly = true;
        const EffectCommand command{
            metadata,
            KysChess::Battle::Test::statusRemoval(action),
        };

        queueEffectCommandBatch(state, { command });
        const auto removedFrame = runBattleFrame(state);
        const auto removed = semanticCueEvents(removedFrame);
        REQUIRE(removed.size() == 1);
        CHECK(removed.front()->visualPath == BattleCueCleanseVisualPath);
        CHECK(removed.front()->color.r == 184);
        CHECK(removed.front()->color.g == 255);
        CHECK(removed.front()->color.b == 246);

        queueEffectCommandBatch(state, { command });
        CHECK(semanticCueEvents(runBattleFrame(state)).empty());
    }
}

TEST_CASE("BattleFrameRunner_PresentsFixedAndFollowSourceAreaVisuals", "[battle][frame_runner][runtime][area_visual]")
{
    auto state = runtimeFrameState();
    const int createdFrame = state.movement.frame;
    state.areas.areas = {
        {
            .id = { 10 },
            .source = {
                .kind = EffectSourceKind::Magic,
                .sourceId = YellowSandWhipMagicId,
                .ownerUnitId = 0,
                .sourceTeam = 0,
            },
            .sourceTeam = 0,
            .geometry = { .shape = AreaShape::Circle, .radiusTiles = 6 },
            .anchor = {
                .kind = BattleAreaAnchorKind::FixedWorldPosition,
                .fixedPosition = { 240.0f, 270.0f, 0.0f },
            },
            .createdFrame = createdFrame,
            .expiresFrameExclusive = createdFrame + 100,
        },
        {
            .id = { 11 },
            .source = {
                .kind = EffectSourceKind::Magic,
                .sourceId = DemonSubduingStaffMagicId,
                .ownerUnitId = 0,
                .sourceTeam = 0,
            },
            .sourceTeam = 0,
            .geometry = { .shape = AreaShape::Circle, .radiusTiles = 5 },
            .anchor = {
                .kind = BattleAreaAnchorKind::FollowSourceUnit,
                .sourceUnitId = 0,
            },
            .createdFrame = createdFrame,
            .expiresFrameExclusive = createdFrame + 100,
        },
    };

    const auto first = runBattleFrame(state);
    REQUIRE(first.areas.size() == 2);
    const auto sand = std::ranges::find(
        first.areas,
        BattleAreaVisualStyle::Sand,
        &BattleAreaPresentation::style);
    const auto ward = std::ranges::find(
        first.areas,
        BattleAreaVisualStyle::ProtectiveWard,
        &BattleAreaPresentation::style);
    REQUIRE(sand != first.areas.end());
    REQUIRE(ward != first.areas.end());
    CHECK(sand->radiusTiles == 6);
    CHECK(sand->center.x == 240.0f);
    CHECK(sand->center.y == 270.0f);
    CHECK(ward->radiusTiles == 5);
    CHECK(ward->center.x == state.units.requireCore(0).motion.position.x);
    CHECK(ward->center.y == state.units.requireCore(0).motion.position.y);
    CHECK(ward->tileWidth == SceneTileWidth);

    state.units.requireCore(0).motion.position = { 180.0f, 210.0f, 0.0f };
    const auto moved = runBattleFrame(state);
    const auto movedSand = std::ranges::find(
        moved.areas,
        BattleAreaVisualStyle::Sand,
        &BattleAreaPresentation::style);
    const auto movedWard = std::ranges::find(
        moved.areas,
        BattleAreaVisualStyle::ProtectiveWard,
        &BattleAreaPresentation::style);
    REQUIRE(movedSand != moved.areas.end());
    REQUIRE(movedWard != moved.areas.end());
    CHECK(movedSand->center.x == 240.0f);
    CHECK(movedSand->center.y == 270.0f);
    CHECK(movedWard->center.x == state.units.requireCore(0).motion.position.x);
    CHECK(movedWard->center.y == state.units.requireCore(0).motion.position.y);

    for (auto& area : state.areas.areas)
    {
        area.expiresFrameExclusive = state.movement.frame + 1;
    }
    CHECK(runBattleFrame(state).areas.empty());
}

TEST_CASE("BattleRuntimeState_RunFrame_OwnsPendingAttackSpawnsAcrossFrames", "[battle][frame_runner][runtime][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;

    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.preferredTargetUnitId = 0;
    request.initial.skillId = 101;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 44;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    queueTrackedRootAttack(runtime, request);

    auto first = runBattleFrame(runtime);
    auto second = runBattleFrame(runtime);

    CHECK(runtime.nextFrame.queuedAttacks().empty());
    REQUIRE(runtime.attacks.attacks.size() == 1);
    CHECK(runtime.attacks.attacks[0].id == 70);
    CHECK(runtime.attacks.nextAttackId == 71);
    CHECK(first.frame == 7);
    CHECK(second.frame == 8);
}

TEST_CASE("BattleRuntimeState_RunFrame_DelaysDualWieldSpawnAndAddsAttackBlock", "[battle][frame_runner][runtime][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.random = BattleRuntimeRandom(5489u);
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.preferredTargetUnitId = 1;
    request.initial.requirePreferredTarget = true;
    request.initial.skillId = 101;
    request.initial.skillName = "左右互搏";
    request.initial.skillMagicType = 2;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 44;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    request.initial.castSubrequestKind = BattleAttackCastSubrequestKind::DualWieldFollowUp;
    request.initial.roleAttackEchoActType = 7;
    request.spawnDelayFrames = 2;
    request.attackerDualWieldBlockGainChancePct = 50;
    runtime.units.requireCore(0).shield = 30;
    queueTrackedRootAttack(runtime, request);

    auto first = runBattleFrame(runtime);
    REQUIRE(runtime.nextFrame.queuedAttacks().size() == 1);
    CHECK(runtime.nextFrame.queuedAttacks()[0].spawnDelayFrames == 1);
    CHECK(runtime.attacks.attacks.empty());
    CHECK(runtime.units.requireCore(0).shield == 30);
    CHECK(runtime.units.require(0).damage.dualWieldBlocksRemaining == 0);
    CHECK(first.visualEvents.empty());

    auto second = runBattleFrame(runtime);
    REQUIRE(runtime.nextFrame.queuedAttacks().size() == 1);
    CHECK(runtime.nextFrame.queuedAttacks()[0].spawnDelayFrames == 0);
    CHECK(runtime.attacks.attacks.empty());
    CHECK(runtime.units.requireCore(0).shield == 30);
    CHECK(runtime.units.require(0).damage.dualWieldBlocksRemaining == 0);
    CHECK(second.visualEvents.empty());

    auto third = runBattleFrame(runtime);
    CHECK(runtime.nextFrame.queuedAttacks().empty());
    REQUIRE(runtime.attacks.attacks.size() == 1);
    CHECK(runtime.units.requireCore(0).shield == 30);
    CHECK(runtime.units.require(0).damage.dualWieldBlocksRemaining == 1);
    CHECK(runtime.random.rawDrawCount() == 1);
    CHECK(std::ranges::any_of(third.logEvents, [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "左右互搏·互搏抵擋+1";
        }));
    CHECK(std::ranges::any_of(third.visualEvents, [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleAttackEcho
                && event.sourceUnitId == 0
                && event.targetUnitId == 1
                && event.animationActType == 7;
        }));

    auto cappedRequest = request;
    cappedRequest.spawnDelayFrames = 0;
    queueTrackedRootAttack(runtime, std::move(cappedRequest));
    const auto capped = runBattleFrame(runtime);
    CHECK(runtime.units.require(0).damage.dualWieldBlocksRemaining == 1);
    CHECK(runtime.random.rawDrawCount() == 1);
    CHECK(std::ranges::none_of(capped.logEvents, [](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == "左右互搏·互搏抵擋+1";
        }));
}

TEST_CASE("BattleRuntimeSession_RunFrame_OwnsRuntimeAcrossFrames", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.attacks.nextAttackId = 70;

    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.preferredTargetUnitId = 0;
    request.initial.skillId = 101;
    request.initial.totalFrame = 30;
    request.initial.visualEffectId = 44;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    queueTrackedRootAttack(runtime, request);

    BattleRuntimeSession session(std::move(runtime));

    const auto first = session.runFrame();
    const auto second = session.runFrame();

    CHECK(session.runtime().nextFrame.queuedAttacks().empty());
    REQUIRE(session.runtime().attacks.attacks.size() == 1);
    CHECK(session.runtime().attacks.attacks[0].id == 70);
    CHECK(session.runtime().attacks.nextAttackId == 71);
    CHECK(first.frame == 7);
    CHECK(second.frame == 8);
}

TEST_CASE("BattleFrameRunner_ForceMoveCollisionChoosesWhetherUnitsBlock",
    "[battle][frame_runner][runtime][force_move]")
{
    const auto movedX = [](KysChess::ForceMoveCollision collision)
    {
        auto state = forceMoveRuntimeState(18.0);
        KysChess::ForceMoveAction action{
            .direction = KysChess::ForceMoveDirection::AwayFromSource,
            .distancePixels = 18,
            .lockFrames = 1,
            .collision = collision,
            .blocked = KysChess::ForceMoveBlockedResult::Shorten,
        };
        queueForceMoveEffect(state, std::move(action));

        runBattleFrame(state);

        return state.units.requireCore(1).motion.position.x;
    };

    constexpr float TargetX = 18.0f * static_cast<float>(SceneTileWidth);
    CHECK(movedX(KysChess::ForceMoveCollision::StopBeforeOccupied)
        == Catch::Approx(TargetX + 9.0f));
    CHECK(movedX(KysChess::ForceMoveCollision::StopBeforeBlocked)
        == Catch::Approx(TargetX + 18.0f));
}

TEST_CASE("BattleFrameRunner_ForceMoveBlockedResultChoosesPartialOrCancelledMove",
    "[battle][frame_runner][runtime][force_move]")
{
    const auto movedX = [](KysChess::ForceMoveBlockedResult blocked,
                           double blockerOffset)
    {
        auto state = forceMoveRuntimeState(blockerOffset);
        KysChess::ForceMoveAction action{
            .direction = KysChess::ForceMoveDirection::AwayFromSource,
            .distancePixels = 18,
            .lockFrames = 1,
            .collision = KysChess::ForceMoveCollision::StopBeforeOccupied,
            .blocked = blocked,
        };
        queueForceMoveEffect(state, std::move(action));

        runBattleFrame(state);

        return state.units.requireCore(1).motion.position.x;
    };

    constexpr float TargetX = 18.0f * static_cast<float>(SceneTileWidth);
    CHECK(movedX(KysChess::ForceMoveBlockedResult::Shorten, 18.0)
        == Catch::Approx(TargetX + 9.0f));
    CHECK(movedX(KysChess::ForceMoveBlockedResult::Stop, 18.0)
        == Catch::Approx(TargetX));
    CHECK(movedX(KysChess::ForceMoveBlockedResult::Stop, 4.0 * SceneTileWidth)
        == Catch::Approx(TargetX + 18.0f));
}

TEST_CASE("BattleFrameRunner_ForceMoveLockFramesControlsMovementDuration",
    "[battle][frame_runner][runtime][force_move]")
{
    auto state = forceMoveRuntimeState(4.0 * SceneTileWidth);
    KysChess::ForceMoveAction action{
        .direction = KysChess::ForceMoveDirection::AwayFromSource,
        .distanceTiles = 1,
        .lockFrames = 3,
        .collision = KysChess::ForceMoveCollision::StopBeforeBlocked,
        .blocked = KysChess::ForceMoveBlockedResult::Shorten,
    };
    queueForceMoveEffect(state, std::move(action));

    runBattleFrame(state);

    const auto& physics = state.units.require(1).movement.physics;
    CHECK(physics.knockbackFrames == 2);
    CHECK(physics.knockbackControlFrames == 3);
}

TEST_CASE("BattleRuntimeSession_RunFrame_DoesNotReplayKnockback", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.units.requireCore(0).stats.speed = 0;
    runtime.units.requireCore(1).stats.speed = 10;
    runtime.units.requireCore(1).vitals.hp = 100;
    runtime.units.requireCore(1).motion.facing = { -1, 0, 0 };
    const float attackerX = 17.0f * static_cast<float>(SceneTileWidth);
    const float defenderX = 18.0f * static_cast<float>(SceneTileWidth);
    runtime.units.requireCore(0).motion.position = { attackerX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    runtime.units.requireCore(1).motion.position = { defenderX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    seedDamageExtrasFromUnits(runtime);

    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = 10;
    attack.state.attackSourceUnitId = 0;
    attack.state.preferredTargetUnitId = 1;
    attack.state.skillId = 101;
    attack.state.skillMagicPower = 120;
    attack.state.totalFrame = 30;
    attack.frame = 29;
    attack.state.operationType = BattleOperationType::Melee;
    attack.state.position = runtime.units.requireCore(1).motion.position;
    attack.state.velocity = { 1, 0, 0 };
    appendTrackedRootAttack(runtime, std::move(attack));

    BattleRuntimeSession session(std::move(runtime));

    session.runFrame();

    const auto& hitFrameUnit = session.runtime().units.requireCore(1);
    const float hitFrameX = hitFrameUnit.motion.position.x;
    CHECK(hitFrameUnit.motion.position.x >= defenderX);
    CHECK(hitFrameUnit.motion.velocity.x == Catch::Approx(1.0f));
    CHECK(hitFrameUnit.motion.facing.x == Catch::Approx(-1.0f));
    CHECK(hitFrameUnit.motion.facing.y == Catch::Approx(0.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 1);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 2);
    CHECK(session.runtime().units.require(1).frozenFrames() == 5);

    session.runFrame();

    const auto& pushedUnit = session.runtime().units.requireCore(1);
    CHECK(session.runtime().attacks.attacks.empty());
    CHECK(pushedUnit.motion.position.x == Catch::Approx(hitFrameX + 1.0f));
    CHECK(pushedUnit.motion.velocity.x == Catch::Approx(0.9f));
    CHECK(pushedUnit.motion.facing.x == Catch::Approx(-1.0f));
    CHECK(pushedUnit.motion.facing.y == Catch::Approx(0.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 0);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 1);
    CHECK(session.runtime().units.require(1).frozenFrames() == 4);

    session.runFrame();

    const auto& lockedUnit = session.runtime().units.requireCore(1);
    CHECK(lockedUnit.motion.facing.x == Catch::Approx(-1.0f));
    CHECK(lockedUnit.motion.facing.y == Catch::Approx(0.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 0);
    CHECK(session.runtime().units.require(1).frozenFrames() == 3);
}

TEST_CASE("BattleFrameRunner_SkippedStatusDamageModifiersCannotAffectTheCurrentHit",
    "[battle][frame_runner][runtime][effect][status][liveness]")
{
    struct Result
    {
        int damage{};
        bool statusRemoved{};
    };

    const auto resolveHit = [](bool addSelfConsumingStatus)
    {
        auto state = ownedRuntimeState();
        auto& attacker = state.units.requireCore(0);
        auto& defender = state.units.requireCore(1);
        attacker.stats.speed = 0;
        attacker.stats.attack = 100;
        defender.stats.speed = 0;
        defender.stats.defence = 100;
        defender.vitals.hp = 1'000;
        defender.vitals.maxHp = 1'000;
        attacker.motion.position = { 17.0f * static_cast<float>(SceneTileWidth),
                                     3.0f * static_cast<float>(SceneTileWidth), 0 };
        defender.motion.position = { 18.0f * static_cast<float>(SceneTileWidth),
                                     3.0f * static_cast<float>(SceneTileWidth), 0 };
        attacker.motion.facing = { 1, 0, 0 };
        defender.motion.facing = { -1, 0, 0 };
        seedDamageExtrasFromUnits(state);

        if (addSelfConsumingStatus)
        {
            EffectRule consume;
            consume.id = EffectRuleId{ 1 };
            consume.event = EffectEvent::HitBeforeDamage;
            consume.observation = EffectObservationScope::StatusHolderEventSource;
            consume.selector.kind = EffectSelectorKind::StatusHolder;
            consume.actions.push_back(EffectAction{ ConsumeThisStatusAction{} });

            ModifyDamageAction ignoreDefense;
            ignoreDefense.perspective = DamageModifierPerspective::Outgoing;
            ignoreDefense.stage = DamageModifierStage::BeforeDefense;
            ignoreDefense.channel = DamageChannel::All;
            ignoreDefense.operation = DamageModifierOperation::IgnoreDefensePercent;
            ignoreDefense.amount.flat = 100;
            EffectRule ignoreDefenseRule;
            ignoreDefenseRule.id = EffectRuleId{ 2 };
            ignoreDefenseRule.event = EffectEvent::HitBeforeDamage;
            ignoreDefenseRule.observation = EffectObservationScope::StatusHolderEventSource;
            ignoreDefenseRule.selector.kind = EffectSelectorKind::HitTarget;
            ignoreDefenseRule.actions.push_back(EffectAction{ ignoreDefense });

            ModifyDamageAction doubleDamage;
            doubleDamage.perspective = DamageModifierPerspective::Outgoing;
            doubleDamage.stage = DamageModifierStage::Final;
            doubleDamage.channel = DamageChannel::All;
            doubleDamage.operation = DamageModifierOperation::PercentAdd;
            doubleDamage.amount.flat = 100;
            EffectRule doubleDamageRule;
            doubleDamageRule.id = EffectRuleId{ 3 };
            doubleDamageRule.event = EffectEvent::HitBeforeDamage;
            doubleDamageRule.observation = EffectObservationScope::StatusHolderEventSource;
            doubleDamageRule.selector.kind = EffectSelectorKind::HitTarget;
            doubleDamageRule.actions.push_back(EffectAction{ doubleDamage });

            auto behavior = std::make_shared<StatusBehaviorDefinition>();
            behavior->rules = {
                std::move(consume),
                std::move(ignoreDefenseRule),
                std::move(doubleDamageRule),
            };
            const EffectSourceBinding binding{
                .kind = EffectSourceKind::Magic,
                .sourceId = 9100,
                .ownerUnitId = 0,
                .sourceTeam = attacker.team,
            };
            const EffectRuleId producerRuleId{ 9100 };
            auto& contribution = appendStatus(
                state.units.require(0).status.effects,
                BattleStatusKind::TrueQi,
                60,
                1,
                0,
                0);
            contribution.producer = StatusProducerKey{
                .binding = binding,
                .ruleId = producerRuleId,
            };
            contribution.producerFamily = StatusProducerFamilyKey{
                .sourceKind = binding.kind,
                .sourceId = binding.sourceId,
                .logicalOwnerUnitId = binding.ownerUnitId,
                .ruleId = producerRuleId,
            };
            contribution.behavior = behavior;
            contribution.behaviorRuntime =
                KysChess::Battle::Test::initialStatusBehaviorRuntime(behavior);
            contribution.sourceUnitId = 0;
            contribution.origin = BattleStatusEffectOrigin{
                binding,
                producerRuleId,
                0,
            };
        }

        BattleAttackInstance attack{ ordinaryProjectilePayload() };
        attack.id = 10;
        attack.state.attackSourceUnitId = 0;
        attack.state.preferredTargetUnitId = 1;
        attack.state.skillId = 101;
        attack.state.skillMagicPower = 200;
        attack.state.totalFrame = 30;
        attack.frame = 29;
        attack.state.operationType = BattleOperationType::Melee;
        attack.state.position = defender.motion.position;
        attack.state.velocity = { 1, 0, 0 };
        appendTrackedRootAttack(state, std::move(attack));

        const auto frame = runBattleFrame(state);
        const auto damage = damageLogAmountsFor(frame, 1);
        REQUIRE(damage.size() == 1);
        return Result{
            .damage = damage.front(),
            .statusRemoved = !state.units.require(0).status.effects.has(
                BattleStatusKind::TrueQi),
        };
    };

    const auto baseline = resolveHit(false);
    const auto withInvalidatedModifiers = resolveHit(true);

    CHECK(baseline.damage > 0);
    CHECK(withInvalidatedModifiers.statusRemoved);
    CHECK(withInvalidatedModifiers.damage == baseline.damage);
}

TEST_CASE("BattleRuntimeSession_RunFrame_StacksRegularAndProcKnockbackVelocity", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.units.requireCore(0).stats.speed = 0;
    runtime.units.requireCore(1).stats.speed = 10;
    runtime.units.requireCore(1).vitals.hp = 100;
    runtime.units.requireCore(1).motion.facing = { -1, 0, 0 };
    const float attackerX = 17.0f * static_cast<float>(SceneTileWidth);
    const float defenderX = 18.0f * static_cast<float>(SceneTileWidth);
    runtime.units.requireCore(0).motion.position = { attackerX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    runtime.units.requireCore(1).motion.position = { defenderX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    seedDamageExtrasFromUnits(runtime);
    KysChess::ForceMoveAction procKnockback{
        .direction = KysChess::ForceMoveDirection::AwayFromSource,
        .distancePixels = 7,
        .lockFrames = 4,
        .collision = KysChess::ForceMoveCollision::StopBeforeBlocked,
        .blocked = KysChess::ForceMoveBlockedResult::Shorten,
    };
    KysChess::EffectRule procKnockbackRule;
    procKnockbackRule.id = KysChess::EffectRuleId{ 1 };
    procKnockbackRule.event = KysChess::EffectEvent::MainProjectileBeforeDamage;
    procKnockbackRule.selector.kind = KysChess::EffectSelectorKind::HitTarget;
    procKnockbackRule.actions = { { KysChess::EffectActionValue{ procKnockback } } };
    appendOwnerEffectRule(runtime, 0, 9001, std::move(procKnockbackRule));

    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = 10;
    attack.state.attackSourceUnitId = 0;
    attack.state.preferredTargetUnitId = 1;
    attack.state.skillId = 101;
    attack.state.skillMagicPower = 120;
    attack.state.totalFrame = 30;
    attack.frame = 29;
    attack.state.operationType = BattleOperationType::Melee;
    attack.state.position = runtime.units.requireCore(1).motion.position;
    attack.state.velocity = { 1, 0, 0 };
    appendTrackedRootAttack(runtime, std::move(attack));

    BattleRuntimeSession session(std::move(runtime));

    session.runFrame();

    const auto& hitFrameUnit = session.runtime().units.requireCore(1);
    const float hitFrameX = hitFrameUnit.motion.position.x;
    CHECK(hitFrameUnit.motion.position.x >= defenderX);
    CHECK(hitFrameUnit.motion.velocity.x == Catch::Approx(2.15f));
    CHECK(hitFrameUnit.motion.facing.x == Catch::Approx(-1.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 4);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 5);
    CHECK(session.runtime().units.require(1).frozenFrames() == 5);

    session.runFrame();

    const auto& pushedUnit = session.runtime().units.requireCore(1);
    CHECK(pushedUnit.motion.position.x == Catch::Approx(hitFrameX + 2.15f));
    CHECK(pushedUnit.motion.velocity.x == Catch::Approx(2.05f).margin(0.0001f));
    CHECK(pushedUnit.motion.facing.x == Catch::Approx(-1.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 3);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 4);
    CHECK(session.runtime().units.require(1).frozenFrames() == 4);

    session.runFrame();
    session.runFrame();
    session.runFrame();

    const auto& settledUnit = session.runtime().units.requireCore(1);
    CHECK(settledUnit.motion.position.x == Catch::Approx(hitFrameX + 8.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 0);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 1);
    CHECK(session.runtime().units.require(1).frozenFrames() == 1);
}

TEST_CASE("BattleRuntimeSession_RunFrame_TaXueIgnoresKnockback", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    runtime.units.requireCore(0).stats.speed = 0;
    runtime.units.requireCore(1).stats.speed = 0;
    runtime.units.requireCore(1).vitals.hp = 100;
    runtime.units.requireCore(1).motion.facing = { -1, 0, 0 };
    const float attackerX = 17.0f * static_cast<float>(SceneTileWidth);
    const float defenderX = 18.0f * static_cast<float>(SceneTileWidth);
    runtime.units.requireCore(0).motion.position = { attackerX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    runtime.units.requireCore(1).motion.position = { defenderX, 3.0f * static_cast<float>(SceneTileWidth), 0 };
    seedDamageExtrasFromUnits(runtime);
    KysChess::ModifyCastAction dashAttack;
    dashAttack.mobility = KysChess::CastMobilityPolicy::DashAttack;
    KysChess::EffectRule dashAttackRule;
    dashAttackRule.id = KysChess::EffectRuleId{ 1 };
    dashAttackRule.event = KysChess::EffectEvent::CastPlanned;
    dashAttackRule.selector.kind = KysChess::EffectSelectorKind::Self;
    dashAttackRule.actions = { { KysChess::EffectActionValue{ dashAttack } } };
    appendOwnerEffectRule(runtime, 1, 9002, std::move(dashAttackRule));
    BattleActionPlanSeed defenderPlan;
    defenderPlan.normalSkill.id = 101;
    runtime.units.require(1).setActionPlan(std::move(defenderPlan));
    runtime.units.require(1).status.effects.setFrames(BattleStatusKind::Stun, 100);

    BattleAttackInstance attack{ ordinaryProjectilePayload() };
    attack.id = 10;
    attack.state.attackSourceUnitId = 0;
    attack.state.preferredTargetUnitId = 1;
    attack.state.skillId = 101;
    attack.state.skillMagicPower = 120;
    attack.state.totalFrame = 30;
    attack.frame = 29;
    attack.state.operationType = BattleOperationType::Melee;
    attack.state.position = runtime.units.requireCore(1).motion.position;
    attack.state.velocity = { 1, 0, 0 };
    appendTrackedRootAttack(runtime, std::move(attack));

    BattleRuntimeSession session(std::move(runtime));

    session.runFrame();

    const auto& hitUnit = session.runtime().units.requireCore(1);
    CHECK(hitUnit.motion.position.x == Catch::Approx(defenderX));
    CHECK(hitUnit.motion.velocity.x == Catch::Approx(0.0f));
    CHECK(session.runtime().units.require(1).movement.physics.knockbackFrames == 0);
    CHECK(session.runtime().units.require(1).movement.physics.knockbackControlFrames == 0);
    CHECK(session.runtime().units.require(1).movement.physics.postDashChaosFrames == 0);
}

TEST_CASE("BattleRuntimeSession_RunFrame_PostDashRetreatDoesNotFlipFacing", "[battle][runtime_session][ownership]")
{
    auto runtime = ownedRuntimeState();
    auto& attacker = runtime.units.requireCore(0);
    attacker.motion.position = { 17.0f * static_cast<float>(SceneTileWidth), 3.0f * static_cast<float>(SceneTileWidth), 0 };
    attacker.motion.facing = { 1, 0, 0 };
    attacker.motion.velocity = {};
    runtime.units.requireCore(1).motion.position = { 18.0f * static_cast<float>(SceneTileWidth), 4.0f * static_cast<float>(SceneTileWidth), 0 };

    auto& physics = runtime.units.require(0).movement.physics;
    physics.position = attacker.motion.position;
    physics.velocity = {};
    physics.postDashRetreatVelocity = { -4, 0, 0 };
    physics.postDashRetreatFrames = 2;

    BattleRuntimeSession session(std::move(runtime));

    session.runFrame();

    const auto& retreated = session.runtime().units.requireCore(0);
    CHECK(retreated.motion.velocity.x < 0.0f);
    CHECK(retreated.motion.facing.x == Catch::Approx(1.0f));
    CHECK(retreated.motion.facing.y == Catch::Approx(0.0f));
}

TEST_CASE("BattleRuntimeUnitSpawn_AppendsUnitRecordWithPerUnitFacts", "[battle][runtime_session][ownership]")
{
    BattleRuntimeState runtime;
    BattleRuntimeUnit unit;
    unit.id = 4;
    unit.team = 1;
    unit.alive = true;
    unit.vitals.maxHp = 100;
    unit.motion.position = { 32.0f, 48.0f, 0.0f };

    BattleComboRuntimeFacts comboFacts;
    comboFacts.addMember(12);
    comboFacts.addApplied(34);

    BattleActionPlanSeed plan;
    plan.normalSkill.id = 99;

    appendRuntimeUnit(runtime, makeRuntimeUnitSpawn(std::move(unit), comboFacts, plan));

    REQUIRE(runtime.units.size() == 1);
    const auto& record = runtime.units.require(4);
    CHECK(record.core.id == 4);
    CHECK(record.comboFacts.isMember(12));
    CHECK(record.comboFacts.hasApplied(34));
    CHECK(record.movement.physics.position.x == 32.0f);
    REQUIRE(record.actionPlan() != nullptr);
    CHECK(record.actionPlan()->normalSkill.id == 99);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsOwnedRuntimeRecords", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);
    input.battleFrame = 42;

    BattleSetupUnitInput unit;
    unit.unitId = 0;
    unit.realRoleId = 1000;
    unit.name = "測試";
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 100, 100, 0, 100 };
    unit.stats = { 10, 10, 10 };
    unit.motion.position = { 128, 256, 0 };
    input.units.push_back(unit);

    const auto expectedCastFrames = input.rules.castConfig.castFrames;
    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().movement.frame == 42);
    REQUIRE(session.runtime().units.size() == 1);
    CHECK(session.runtime().units.requireCore(0).id == 0);
    REQUIRE(session.runtime().units.size() == 1);
    CHECK(session.runtime().units.require(0).id() == 0);
    CHECK(session.runtime().action.castConfig.castFrames == expectedCastFrames);
    CHECK(session.runtime().damage.sortPendingDamageByDefenderMagnitude);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedBuildsMovementAgentRowsForDeadUnits", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);

    BattleSetupUnitInput live;
    live.unitId = 0;
    live.realRoleId = 1000;
    live.name = "測試";
    live.team = 0;
    live.alive = true;
    live.vitals = { 100, 100, 0, 100 };
    live.stats = { 10, 10, 10 };
    live.motion.position = { 128, 256, 0 };
    input.units.push_back(live);

    BattleSetupUnitInput dead = live;
    dead.unitId = 1;
    dead.realRoleId = 1001;
    dead.name = "死亡測試";
    dead.team = 1;
    dead.alive = false;
    dead.vitals.hp = 0;
    dead.motion.position = { 256, 256, 0 };
    input.units.push_back(dead);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    REQUIRE(session.runtime().units.size() == 2);
    CHECK(session.runtime().units.require(0).id() == 0);
    CHECK(session.runtime().units.require(1).id() == 1);
    CHECK(session.runtime().units.require(0).movement.active);
    CHECK_FALSE(session.runtime().units.require(1).movement.active);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedSpendsNonThroughProjectilesOnHit", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);

    BattleSetupUnitInput unit;
    unit.unitId = 0;
    unit.realRoleId = 1000;
    unit.name = "測試";
    unit.team = 0;
    unit.alive = true;
    unit.vitals = { 100, 100, 0, 100 };
    unit.stats = { 10, 10, 10 };
    unit.motion.position = { 128, 256, 0 };
    input.units.push_back(unit);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;

    CHECK(session.runtime().attacks.spendNonThroughOnHit);
}

TEST_CASE("BattleRuntimeSession_CreateInitializedKeepsDerivedMotionStateAlignedAfterFrame", "[battle][runtime_session][ownership]")
{
    BattleRuntimeSessionCreationInput input;
    input.rules = makeHadesBattleRuntimeRules(SceneTileWidth, 18);
    input.rules.movementCollisionWorld.walkableByCell.assign(18 * 18, 1);

    BattleSetupUnitInput ally;
    ally.unitId = 0;
    ally.realRoleId = 1000;
    ally.name = "我方";
    ally.team = 0;
    ally.alive = true;
    ally.vitals = { 100, 100, 0, 100 };
    ally.stats = { 10, 10, 10 };
    ally.motion.position = { 128, 128, 0 };
    input.units.push_back(ally);

    BattleSetupUnitInput enemy;
    enemy.unitId = 1;
    enemy.realRoleId = 1001;
    enemy.name = "敵方";
    enemy.team = 1;
    enemy.alive = true;
    enemy.vitals = { 100, 100, 0, 100 };
    enemy.stats = { 10, 10, 10 };
    enemy.motion.position = { 360, 128, 0 };
    input.units.push_back(enemy);

    auto session = BattleRuntimeSession::createInitialized(std::move(input)).session;
    session.runFrame();

    const auto& runtime = session.runtime();
    const auto& unit = runtime.units.requireCore(0);
    REQUIRE(runtime.units.size() == 2);
    REQUIRE(runtime.units.require(0).id() == 0);
    CHECK(runtime.units.require(0).movement.physics.position.x == unit.motion.position.x);
    CHECK(runtime.units.require(0).movement.physics.position.y == unit.motion.position.y);
}

TEST_CASE("BattleFrameRunner_RunFrame_UsesRuntimeOwnedFrameState", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();

    BattleFrameRunner().runFrame(state);

    REQUIRE(state.units.size() == 2);
    CHECK(state.units.requireCore(0).alive);
    CHECK(state.units.requireCore(1).alive);
}

TEST_CASE("BattleFrameRunner_RunFrame_PublishesStateApplications", "[battle][frame_runner][runtime]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
    });
    state.units.requireCore(0).shield = 12;
    BattleDamageRuntimeUnit damage;
    damage.dualWieldBlocksRemaining = 1;
    state.units.require(0).damage = damage;
    state.units.requireCore(0).invincible = 4;
    BattleStatusRuntimeUnit status;
    status.effects.setFrames(BattleStatusKind::Stun, 3, 9);
    state.units.require(0).status = status;

    runBattleFrame(state);

    const auto& runtimeUnit = state.units.requireCore(0);
    const auto& statusUnit = state.units.require(0).status;
    CHECK(runtimeUnit.invincible == 3);
    CHECK(statusUnit.effects.remainingFrames(BattleStatusKind::Stun) == 2);
    CHECK(statusUnit.effects.maximumFrames(BattleStatusKind::Stun) == 9);
    CHECK(runtimeUnit.shield == 12);
    CHECK(state.units.require(0).damage.dualWieldBlocksRemaining == 1);
}

TEST_CASE("BattleFrameRunner_RunFrame_AdvancesRuntimeUnits", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    auto& unit = state.units.requireCore(0);
    unit.animation.cooldown = 1;
    unit.haveAction = true;
    unit.operationType = BattleOperationType::RangedProjectile;
    unit.animation.actType = 2;
    unit.physicalPower = 4;

    runBattleFrame(state);

    const auto& updated = state.units.requireCore(0);
    CHECK(updated.animation.cooldown == 0);
    CHECK(updated.animation.actType == -1);
    CHECK(updated.motion.velocity.x == 0.0f);
    CHECK(updated.motion.velocity.y == 0.0f);
    CHECK(updated.vitals.mp == 21);
    CHECK(updated.physicalPower == 5);
    CHECK_FALSE(updated.haveAction);
}

TEST_CASE("BattleFrameRunner_RunFrame_AppliesRuntimeMpRegenBlockAndRecovery", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 80),
        teamRuntimeUnit(1, 1, 100),
    });
    state.units.require(0).status.effects.setFrames(BattleStatusKind::MpBlocked, 2);
    KysChess::ModifyAttributeAction recoveryBonus;
    recoveryBonus.attribute = KysChess::BattleAttribute::MpRecoveryBonus;
    recoveryBonus.operation = KysChess::AttributeOperation::PercentagePointAdd;
    recoveryBonus.stack = KysChess::EffectStackPolicy::Independent;
    BattleEffectCommandSystem::applyPersistentAttributeModifier(
        state.effectCommands,
        {
            .binding = {
                .kind = KysChess::EffectSourceKind::Combo,
                .sourceId = 91,
                .ownerUnitId = 0,
                .sourceTeam = 0,
            },
            .ruleId = KysChess::EffectRuleId{ 1 },
            .event = KysChess::EffectEvent::BattleInitialized,
            .targetUnitId = 0,
        },
        prepareModifyAttribute(recoveryBonus, 100),
        0);

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.mp == 20);
    CHECK(state.units.requireCore(1).vitals.mp == 21);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsPoisonTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 30;
    state.status.config.poisonDamageIntervalFrames = 30;

    BattleStatusUnitState poisoned;
    poisoned.id = 1;
    poisoned.alive = true;
    poisoned.hp = 80;
    poisoned.maxHp = 100;
    appendStatus(poisoned.effects, BattleStatusKind::Poison, 3, 2, 10, 0, 1);
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    state.units.require(1).status = runtimeStatusUnit(poisoned);
    seedDamageExtrasFromUnits(state);

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 8 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 0 });
    CHECK(state.units.requireCore(1).vitals.hp == 72);
    REQUIRE(state.units.require(1).status.effects.find(BattleStatusKind::Poison));
    CHECK(state.units.require(1).status.effects.find(BattleStatusKind::Poison)->stacks == 1);
    CHECK(state.units.require(1).status.effects.find(BattleStatusKind::Poison)->remainingFrames == 2);
}

TEST_CASE("BattleFrameRunner_XuanmingSettlesTheCanonicalPoisonSchedule", "[battle][frame_runner][runtime][effect][poison]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 40;
    state.status.config.poisonDamageIntervalFrames = 30;
    auto target = teamRuntimeUnit(1, 1, 101);
    target.vitals.maxHp = 200;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        target,
        teamRuntimeUnit(7, 0, 100),
    });
    auto& poison = state.units.require(1).status.effects;
    auto& contribution = appendStatus(
        poison,
        BattleStatusKind::Poison,
        65,
        3,
        10,
        7);
    auto behavior = std::make_shared<StatusBehaviorDefinition>(
        *contribution.behavior);
    EffectRule extraPeriodicRule;
    extraPeriodicRule.id = EffectRuleId{ 9001 };
    extraPeriodicRule.event = EffectEvent::FrameAdvanced;
    extraPeriodicRule.observation = EffectObservationScope::StatusHolderEventSource;
    extraPeriodicRule.selector.kind = EffectSelectorKind::StatusHolder;
    extraPeriodicRule.intervalFrames = 10;
    ChangeResourceAction extraShield;
    extraShield.resource = BattleResource::Shield;
    extraShield.kind = ResourceChangeKind::Grant;
    extraShield.amount.flat = 1;
    extraPeriodicRule.actions.push_back(EffectAction{ extraShield });
    behavior->rules.insert(behavior->rules.begin(), std::move(extraPeriodicRule));
    contribution.behavior = std::move(behavior);
    contribution.behaviorRuntime = {
        EffectRuleRuntimeState{ .intervalFramesRemaining = 5 },
        EffectRuleRuntimeState{ .intervalFramesRemaining = 30 },
    };
    seedDamageExtrasFromUnits(state);

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 21 };
    metadata.event = KysChess::EffectEvent::UltimateCommitted;
    metadata.commandOrdinal = 1;
    metadata.targetUnitId = 1;

    const KysChess::SettleRemainingStatusDamageAction settlementAction{
        .status = KysChess::BattleStatusKind::Poison,
    };
    const EffectCommand settlementCommand{
        metadata,
        StateMachineEffectCommand{
            .value = settlementAction,
        },
    };

    metadata.actionOrder = 1;
    metadata.commandOrdinal = 2;
    KysChess::RemoveStatusAction removeAction;
    removeAction.statuses.push_back(KysChess::BattleStatusKind::Poison);
    const EffectCommand removeCommand{
        metadata,
        KysChess::Battle::Test::statusRemoval(std::move(removeAction)),
    };

    metadata.actionOrder = 2;
    metadata.commandOrdinal = 3;
    KysChess::ApplyStatusAction applyAction;
    applyAction.status = KysChess::BattleStatusKind::Poison;
    applyAction.durationFrames = 150;
    applyAction.quantity = KysChess::SetStatusTriggerCharges{ 5 };
    applyAction.reapplication = KysChess::StatusReapplicationPolicy::ReplaceExistingPoison;
    applyAction.behavior = KysChess::Battle::Test::poisonStatusBehavior(10);
    const EffectCommand applyCommand{
        metadata,
        KysChess::Battle::Test::statusApplication(applyAction),
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ settlementCommand, removeCommand, applyCommand }, { .frame = 41 }) });

    const auto result = runBattleFrame(state);

    CHECK(state.movement.frame == 41);
    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 19 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 0 });
    CHECK(state.units.requireCore(1).vitals.hp == 82);
    const auto* poisonAfterPayload = state.units.require(1).status.effects.find(BattleStatusKind::Poison);
    REQUIRE(poisonAfterPayload);
    CHECK(poisonAfterPayload->remainingFrames == 150);
    CHECK(poisonAfterPayload->stacks == 5);
    CHECK(poisonDamagePercent(poisonAfterPayload->behavior) == 10);
    CHECK(poisonAfterPayload->sourceUnitId == 0);

    const auto payload = std::ranges::find(
        result.logEvents,
        BattleStatusSemanticId::PoisonPayload,
        &BattleLogEvent::statusId);
    REQUIRE(payload != result.logEvents.end());
    CHECK(payload->sourceUnitId == 0);
    CHECK(payload->targetUnitId == 1);
    CHECK(payload->amount == 10);
    CHECK(payload->secondaryAmount == 5);

    const auto applied = std::ranges::find(
        result.logEvents,
        BattleStatusSemanticId::Poison,
        &BattleLogEvent::statusId);
    REQUIRE(applied != result.logEvents.end());
    CHECK(applied->sourceUnitId == 0);
    CHECK(applied->targetUnitId == 1);
    CHECK(applied->amount == 10);
    CHECK(BattleLogTest::joinSegments(applied->segments) == "中毒10%（150幀）");
}

TEST_CASE("BattleFrameRunner_StatusDamageSettlementHonorsClearAfterSettle", "[battle][frame_runner][runtime][effect][poison]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 40;
    state.status.config.poisonDamageIntervalFrames = 30;
    auto target = teamRuntimeUnit(1, 1, 101);
    target.vitals.maxHp = 200;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        target,
        teamRuntimeUnit(7, 0, 100),
    });
    auto& poison = state.units.require(1).status.effects;
    appendStatus(poison, BattleStatusKind::Poison, 32, 1, 10, 7);
    seedDamageExtrasFromUnits(state);

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 21 };
    metadata.event = KysChess::EffectEvent::UltimateCommitted;
    metadata.commandOrdinal = 1;
    metadata.targetUnitId = 1;

    const KysChess::SettleRemainingStatusDamageAction action{
        .status = KysChess::BattleStatusKind::Poison,
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{
            EffectCommand{
                metadata,
                StateMachineEffectCommand{
                    .value = action,
                },
            },
        }, { .frame = 41 }) });

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 10 });
    CHECK(state.units.requireCore(1).vitals.hp == 91);
    const auto* poisonAfterBlockedSettlement = state.units.require(1).status.effects.find(BattleStatusKind::Poison);
    REQUIRE(poisonAfterBlockedSettlement);
    CHECK(poisonAfterBlockedSettlement->remainingFrames > 0);
    CHECK(poisonAfterBlockedSettlement->stacks == 1);
    CHECK(poisonDamagePercent(poisonAfterBlockedSettlement->behavior) == 10);
    CHECK(poisonAfterBlockedSettlement->sourceUnitId == 7);
}

TEST_CASE("BattleFrameRunner_StatusDamageSettlementPreservesPoisonWhenNoDamageRemains", "[battle][frame_runner][runtime][effect][poison]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 40;
    state.status.config.poisonDamageIntervalFrames = 30;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
        teamRuntimeUnit(7, 0, 100),
    });
    auto& poison = state.units.require(1).status.effects;
    appendStatus(poison, BattleStatusKind::Poison, 19, 1, 10, 7);
    seedDamageExtrasFromUnits(state);

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 21 };
    metadata.event = KysChess::EffectEvent::UltimateCommitted;
    metadata.commandOrdinal = 1;
    metadata.targetUnitId = 1;

    const KysChess::SettleRemainingStatusDamageAction action{
        .status = KysChess::BattleStatusKind::Poison,
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{
            EffectCommand{
                metadata,
                StateMachineEffectCommand{
                    .value = action,
                },
            },
        }, { .frame = 41 }) });

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).empty());
    CHECK(state.units.requireCore(1).vitals.hp == 100);
    const auto* poisonAfterZeroSettlement = state.units.require(1).status.effects.find(BattleStatusKind::Poison);
    REQUIRE(poisonAfterZeroSettlement);
    CHECK(poisonAfterZeroSettlement->remainingFrames > 0);
    CHECK(poisonAfterZeroSettlement->stacks == 1);
    CHECK(poisonDamagePercent(poisonAfterZeroSettlement->behavior) == 10);
    CHECK(poisonAfterZeroSettlement->sourceUnitId == 7);
}

TEST_CASE("BattleFrameRunner_PoisonPayloadIsReportedWhenStrongerPoisonPreventsApplication", "[battle][frame_runner][runtime][effect][poison][report]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 40;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
        teamRuntimeUnit(7, 0, 100),
    });
    auto& poison = state.units.require(1).status.effects;
    appendStatus(poison, BattleStatusKind::Poison, 90, 3, 12, 7);

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Combo,
        .sourceId = 81,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 31 };
    metadata.event = KysChess::EffectEvent::HitBeforeDamage;
    metadata.targetUnitId = 1;

    KysChess::ApplyStatusAction action;
    action.status = KysChess::BattleStatusKind::Poison;
    action.durationFrames = 90;
    action.quantity = KysChess::SetStatusTriggerCharges{ 3 };
    action.reapplication = KysChess::StatusReapplicationPolicy::KeepHigherDamage;
    action.poisonSameEventMerge = KysChess::PoisonSameEventMerge::SumDamagePercent;
    action.behavior = KysChess::Battle::Test::poisonStatusBehavior(7);
    const EffectCommand command{
        metadata,
        KysChess::Battle::Test::statusApplication(action),
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ command }, { .frame = 41 }) });

    const auto result = runBattleFrame(state);

    const auto* poisonAfterWeakerPayload = state.units.require(1).status.effects.find(BattleStatusKind::Poison);
    REQUIRE(poisonAfterWeakerPayload);
    CHECK(poisonAfterWeakerPayload->stacks == 3);
    CHECK(poisonDamagePercent(poisonAfterWeakerPayload->behavior) == 12);
    CHECK(poisonAfterWeakerPayload->sourceUnitId == 7);
    const auto payloads = std::ranges::count(
        result.logEvents,
        BattleStatusSemanticId::PoisonPayload,
        &BattleLogEvent::statusId);
    const auto applications = std::ranges::count(
        result.logEvents,
        BattleStatusSemanticId::Poison,
        &BattleLogEvent::statusId);
    CHECK(payloads == 1);
    CHECK(applications == 0);
    const auto payload = std::ranges::find(
        result.logEvents,
        BattleStatusSemanticId::PoisonPayload,
        &BattleLogEvent::statusId);
    REQUIRE(payload != result.logEvents.end());
    CHECK(payload->amount == 7);
    CHECK(payload->secondaryAmount == 3);
}

TEST_CASE("BattleFrameRunner_MpDrainReportsActualRemovedAndRestoredDeltas", "[battle][frame_runner][runtime][effect][resource][report]")
{
    auto state = runtimeFrameState();
    state.movement.frame = 7;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
    });
    state.units.requireCore(0).vitals.mp = 95;
    state.units.requireCore(1).vitals.mp = 12;

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Combo,
        .sourceId = 81,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 32 };
    metadata.event = KysChess::EffectEvent::DamageResolved;
    metadata.targetUnitId = 1;

    KysChess::ChangeResourceAction action;
    action.resource = KysChess::BattleResource::Mp;
    action.kind = KysChess::ResourceChangeKind::Drain;
    const EffectCommand command{
        metadata,
        prepareChangeResource(action, 20),
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ command }, { .frame = 8 }) });

    const auto result = runBattleFrame(state);

    CHECK(state.units.requireCore(0).vitals.mp == 100);
    CHECK(state.units.requireCore(1).vitals.mp == 0);
    const auto drained = std::ranges::find(
        result.logEvents,
        BattleStatusSemanticId::MagicPointsDrained,
        &BattleLogEvent::statusId);
    REQUIRE(drained != result.logEvents.end());
    CHECK(drained->type == BattleLogEventType::Status);
    CHECK(drained->sourceUnitId == 0);
    CHECK(drained->targetUnitId == 1);
    CHECK(drained->amount == 12);
    CHECK(drained->resourceId == BattleResourceSemanticId::MagicPoints);

    const auto restored = std::ranges::find_if(
        result.logEvents,
        [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Heal
                && event.resourceId == BattleResourceSemanticId::MagicPoints;
        });
    REQUIRE(restored != result.logEvents.end());
    CHECK(restored->sourceUnitId == 0);
    CHECK(restored->targetUnitId == 0);
    CHECK(restored->amount == 5);
}

TEST_CASE("BattleFrameRunner_EnemyTopDebuffReportCoalescesPairedActionsAndTracksLivingOwners", "[battle][frame_runner][runtime][effect][enemy-top-debuff][report]")
{
    auto state = runtimeFrameState();
    auto target = teamRuntimeUnit(1, 1, 100);
    target.stats.attack = 40;
    target.stats.defence = 30;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        target,
        teamRuntimeUnit(2, 0, 100),
        teamRuntimeUnit(3, 1, 100),
        teamRuntimeUnit(4, 0, 100),
    });

    constexpr int ComboId = 82;
    state.effectSourceNames.emplace(
        std::pair{ KysChess::EffectSourceKind::Combo, ComboId },
        "陰險");

    KysChess::EffectRule rule;
    rule.id = KysChess::EffectRuleId{ 33 };
    rule.event = KysChess::EffectEvent::FrameAdvanced;
    rule.selector.kind = KysChess::EffectSelectorKind::StrongestEnemies;
    rule.selector.count = 1;

    KysChess::ModifyAttributeAction attack;
    attack.attribute = KysChess::BattleAttribute::Attack;
    attack.operation = KysChess::AttributeOperation::FlatAdd;
    attack.amount.flat = -22;
    attack.durationFrames = 1;
    attack.stack = KysChess::EffectStackPolicy::AddStack;
    attack.stackLimit = 10;
    rule.actions.push_back({ attack });

    auto defence = attack;
    defence.attribute = KysChess::BattleAttribute::Defence;
    rule.actions.push_back({ defence });

    appendOwnerEffectRule(state, 0, ComboId, rule);
    appendOwnerEffectRule(state, 2, ComboId, std::move(rule));

    const auto enemyTopEvents = [](const BattlePresentationFrame& frame)
    {
        std::vector<const BattleLogEvent*> result;
        for (const auto& event : frame.logEvents)
        {
            if (event.statusId == BattleStatusSemanticId::EnemyTopAttackDebuff)
            {
                result.push_back(&event);
            }
        }
        return result;
    };

    const auto opening = runBattleFrame(state);
    const auto openingEvents = enemyTopEvents(opening);
    REQUIRE(openingEvents.size() == 1);
    CHECK(openingEvents[0]->targetUnitId == 1);
    CHECK(openingEvents[0]->amount == -44);
    CHECK(openingEvents[0]->previousAmount == 0);
    CHECK(openingEvents[0]->newAmount == -44);
    CHECK(openingEvents[0]->semanticSourceTeam == 0);
    CHECK(openingEvents[0]->semanticSourceKind == "combo");
    CHECK(openingEvents[0]->semanticSourceName == "陰險");
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1,
        .attribute = KysChess::BattleAttribute::Attack,
        .baseValue = 40,
        .frame = state.movement.frame,
    }) == -4);
    CHECK(BattleEffectCommandSystem::queryAttribute(state, {
        .unitId = 1,
        .attribute = KysChess::BattleAttribute::Defence,
        .baseValue = 30,
        .frame = state.movement.frame,
    }) == -14);

    for (int frame = 0; frame < 120; ++frame)
    {
        for (int hit = 0; hit < 4; ++hit)
        {
            const auto dispatched = BattleEffectEventBridge().dispatch(state,
                { .frame = state.movement.frame,
                  .eventOrdinal = static_cast<std::uint64_t>(frame * 4 + hit + 1),
                  .ownerUnitId = 0 },
                EffectEvent::HitBeforeDamage,
                HitEventData{
                    .provenance = {
                        .cast = {
                            .rootCastId = BattleCastId{1},
                            .castId = BattleCastId{1},
                            .sourceUnitId = 0,
                            .magicId = 1,
                        },
                        .attackId = BattleAttackId{1},
                        .rootAttack = true,
                        .mainProjectile = true,
                    },
                    .targetUnitId = 1,
                    .originalTargetUnitId = 1,
                });
            CHECK(dispatched.commands.empty());
        }
        const auto refresh = runBattleFrame(state);
        CHECK(enemyTopEvents(refresh).empty());
        REQUIRE(state.effectCommands.attributeModifiers.size() == 2);
        for (const auto& modifier : state.effectCommands.attributeModifiers)
            CHECK(modifier.stackCount == 2);
        CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 40) == 0);
        CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Defence, 30) == 0);
    }

    auto& secondOwner = state.units.requireCore(2);
    secondOwner.alive = false;
    secondOwner.vitals.hp = 0;
    const auto oneOwner = runBattleFrame(state);
    const auto oneOwnerEvents = enemyTopEvents(oneOwner);
    REQUIRE(oneOwnerEvents.size() == 1);
    CHECK(oneOwnerEvents[0]->targetUnitId == 1);
    CHECK(oneOwnerEvents[0]->amount == 22);
    CHECK(oneOwnerEvents[0]->previousAmount == -44);
    CHECK(oneOwnerEvents[0]->newAmount == -22);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 40) == 18);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Defence, 30) == 8);

    auto& firstOwner = state.units.requireCore(0);
    firstOwner.alive = false;
    firstOwner.vitals.hp = 0;
    const auto noOwners = runBattleFrame(state);
    const auto noOwnerEvents = enemyTopEvents(noOwners);
    REQUIRE(noOwnerEvents.size() == 1);
    CHECK(noOwnerEvents[0]->targetUnitId == 1);
    CHECK(noOwnerEvents[0]->amount == 22);
    CHECK(noOwnerEvents[0]->previousAmount == -22);
    CHECK(noOwnerEvents[0]->newAmount == 0);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 40) == 40);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Defence, 30) == 30);
}

TEST_CASE("Enemy top debuff reporting handles a shield blocking only one attribute", "[battle][frame_runner][enemy-top-debuff][regression]")
{
    auto state = runtimeFrameState();
    auto target = teamRuntimeUnit(1, 1, 100);
    target.stats.attack = 40;
    target.stats.defence = 30;
    seedRuntimeUnits(state, { teamRuntimeUnit(0, 0, 100), target, teamRuntimeUnit(2, 0, 100) });
    state.units.require(1).status.effects.statusShield = 1;
    constexpr int comboId = 82;
    state.effectSourceNames.emplace(std::pair{ EffectSourceKind::Combo, comboId }, "陰險");
    EffectRule rule;
    rule.id = EffectRuleId{ 33 };
    rule.event = EffectEvent::FrameAdvanced;
    rule.selector.kind = EffectSelectorKind::StrongestEnemies;
    rule.selector.count = 1;
    ModifyAttributeAction attack;
    attack.attribute = BattleAttribute::Attack;
    attack.operation = AttributeOperation::FlatAdd;
    attack.amount.flat = -22;
    attack.durationFrames = 1;
    rule.actions.push_back({ attack });
    auto defence = attack;
    defence.attribute = BattleAttribute::Defence;
    defence.amount.flat = -11;
    rule.actions.push_back({ defence });
    appendOwnerEffectRule(state, 0, comboId, std::move(rule));

    const auto changes = [](const BattlePresentationFrame& frame)
    {
        std::map<BattleStatusSemanticId, BattleLogEvent> result;
        for (const auto& event : frame.logEvents)
            if (event.statusId == BattleStatusSemanticId::EnemyTopAttackDebuff
                || event.statusId == BattleStatusSemanticId::EnemyTopDefenceDebuff)
                REQUIRE(result.emplace(event.statusId, event).second);
        return result;
    };
    const auto opening = changes(runBattleFrame(state));
    REQUIRE(opening.size() == 1);
    const auto& first = opening.at(BattleStatusSemanticId::EnemyTopDefenceDebuff);
    CHECK(first.previousAmount == 0);
    CHECK(first.newAmount == -11);
    CHECK(first.amount == -11);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 40) == 40);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Defence, 30) == 19);

    const auto refreshed = changes(runBattleFrame(state));
    REQUIRE(refreshed.size() == 1);
    CHECK(refreshed.at(BattleStatusSemanticId::EnemyTopAttackDebuff).newAmount == -22);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Attack, 40) == 18);
    CHECK(effectAdjustedAttribute(state, 1, BattleAttribute::Defence, 30) == 19);
    CHECK(changes(runBattleFrame(state)).empty());

    state.units.requireCore(0).alive = false;
    state.units.requireCore(0).vitals.hp = 0;
    const auto expired = changes(runBattleFrame(state));
    REQUIRE(expired.size() == 2);
    CHECK(expired.at(BattleStatusSemanticId::EnemyTopAttackDebuff).previousAmount == -22);
    CHECK(expired.at(BattleStatusSemanticId::EnemyTopAttackDebuff).amount == 22);
    CHECK(expired.at(BattleStatusSemanticId::EnemyTopDefenceDebuff).previousAmount == -11);
    CHECK(expired.at(BattleStatusSemanticId::EnemyTopDefenceDebuff).amount == 11);
    for (const auto& [attribute, event] : expired) CHECK(event.newAmount == 0);
}

TEST_CASE("BattleFrameRunner_ContinuesCompoundEffectsAfterQueuedDamageSettles", "[battle][frame_runner][runtime][effect][damage][continuation]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
    });
    seedDamageExtrasFromUnits(state);

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = KysChess::EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = KysChess::EffectRuleId{ 21 };
    metadata.event = KysChess::EffectEvent::UltimateCommitted;
    metadata.commandOrdinal = 1;
    metadata.targetUnitId = 1;

    KysChess::DealDamageAction damageAction;
    damageAction.kind = KysChess::BattleDamageKind::Effect;
    const EffectCommand damageCommand{
        metadata,
        prepareDealDamage(damageAction, 20, 1),
    };

    metadata.actionOrder = 1;
    metadata.commandOrdinal = 2;
    KysChess::ChangeResourceAction healAction;
    healAction.resource = KysChess::BattleResource::Hp;
    healAction.kind = KysChess::ResourceChangeKind::Restore;
    const EffectCommand healCommand{
        metadata,
        prepareChangeResource(healAction, 10),
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ damageCommand, healCommand }, { .frame = 1 }) });

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 20 });
    CHECK(state.units.requireCore(1).vitals.hp == 90);
    CHECK(state.heals.committedTransactions.size() == 1);
}

TEST_CASE("BattleFrameRunner_DamageContinuationStopsAtStatusContributionRuleBoundary",
          "[battle][frame_runner][runtime][effect][damage][continuation][status]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
    });
    seedDamageExtrasFromUnits(state);
    auto& effects = state.units.require(1).status.effects;
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .stacks = 1,
        .appliedSequence = 7,
    });
    effects.statuses.push_back({
        .kind = BattleStatusKind::TrueQi,
        .stacks = 1,
        .appliedSequence = 8,
    });

    EffectCommandMetadata metadata;
    metadata.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 21,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    metadata.ruleId = EffectRuleId{ 21 };
    metadata.event = EffectEvent::HitBeforeDamage;
    metadata.executionLane = EffectExecutionLane::StatusBehavior;
    metadata.commandOrdinal = 1;
    metadata.targetUnitId = 1;
    metadata.statusContribution = EffectStatusContributionContext{
        .holderUnitId = 1,
        .sourceUnitId = 0,
        .kind = BattleStatusKind::TrueQi,
        .quantity = 1,
        .appliedSequence = 7,
        .producerRuleId = EffectRuleId{ 21 },
    };

    DealDamageAction damageAction;
    damageAction.kind = BattleDamageKind::Effect;
    const EffectCommand damageCommand{
        metadata,
        prepareDealDamage(damageAction, 20, 1),
    };

    metadata.actionOrder = 1;
    metadata.commandOrdinal = 2;
    metadata.statusContribution->appliedSequence = 8;
    ChangeResourceAction shieldAction;
    shieldAction.resource = BattleResource::Shield;
    shieldAction.kind = ResourceChangeKind::Grant;
    const EffectCommand shieldCommand{
        metadata,
        prepareChangeResource(shieldAction, 20),
    };
    state.effectIntegration.queuedCommandBatches.push_back({ .commands = KysChess::Battle::Test::commandFixture(std::vector<EffectCommand>{ damageCommand, shieldCommand }, { .frame = 1 }) });

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1).empty());
    CHECK(state.units.requireCore(1).vitals.hp == 100);
    CHECK(state.units.requireCore(1).shield == 0);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_ConvertsBleedTickToDamageTransaction", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    state.status.config.bleedDamageIntervalFrames = 10;
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
    });
    seedDamageExtrasFromUnits(state);

    ApplyStatusAction applyBleed;
    applyBleed.status = BattleStatusKind::Bleed;
    applyBleed.quantity = AddSharedStatusLayers{ 6, 6 };
    applyBleed.behavior = makeCatalogOwnedStatusBehavior(applyBleed);
    EffectCommandMetadata applyMetadata;
    applyMetadata.binding = {
        .kind = EffectSourceKind::Magic,
        .sourceId = 8001,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    };
    applyMetadata.ruleId = EffectRuleId{ 8001 };
    applyMetadata.event = EffectEvent::UltimateCommitted;
    applyMetadata.targetUnitId = 1;
    const BattleCastProvenance applicationCast{
        .rootCastId = BattleCastId{ 70 },
        .castId = BattleCastId{ 70 },
        .sourceUnitId = 0,
        .magicId = 8001,
        .ultimate = true,
    };
    BattleEffectCommandSystem().reduce(state, KysChess::Battle::Test::commandFixture(EffectCommand{
            applyMetadata,
            KysChess::Battle::Test::statusApplication(applyBleed),
        }, {
            .frame = 0,
            .cast = applicationCast,
        }));
    auto* appliedBleed = state.units.require(1).status.effects.find(
        BattleStatusKind::Bleed);
    REQUIRE(appliedBleed);
    REQUIRE(appliedBleed->behaviorRuntime.size() == 1);
    appliedBleed->behaviorRuntime.front().intervalFramesRemaining = 1;

    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = 13;
    EffectRule fabricatedCastLineage;
    fabricatedCastLineage.id = EffectRuleId{ 8002 };
    fabricatedCastLineage.event = EffectEvent::DamageResolved;
    fabricatedCastLineage.selector.kind = EffectSelectorKind::Self;
    fabricatedCastLineage.conditions = {
        IsUltimateCondition{},
        DamageKindInCondition{ { "流血" } },
    };
    fabricatedCastLineage.actions = { EffectAction{ shield } };
    appendOwnerEffectRule(state, 0, 8002, fabricatedCastLineage);

    auto fabricatedAttackLineage = fabricatedCastLineage;
    fabricatedAttackLineage.id = EffectRuleId{ 8003 };
    fabricatedAttackLineage.conditions = {
        DamageOriginIsAttackCondition{},
        DamageKindInCondition{ { "流血" } },
    };
    std::get<ChangeResourceAction>(
        fabricatedAttackLineage.actions.front().value).amount.flat = 17;
    appendOwnerEffectRule(state, 0, 8003, std::move(fabricatedAttackLineage));

    auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 6 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 0 });
    CHECK(state.units.requireCore(1).vitals.hp == 74);
    CHECK(state.units.requireCore(0).shield == 0);
    auto bleedLog = std::find_if(result.logEvents.begin(), result.logEvents.end(), [](const BattleLogEvent& event)
        {
            return event.type == BattleLogEventType::Damage
                && event.targetUnitId == 1
                && event.amount == 6
                && BattleLogTest::textOf(event) == "流血";
        });
    CHECK(bleedLog != result.logEvents.end());
    auto bleedNumber = std::find_if(result.visualEvents.begin(), result.visualEvents.end(), [](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::DamageNumber
                && event.targetUnitId == 1
                && event.amount == 6
                && event.color.r == 190
                && event.color.g == 120
                && event.color.b == 60;
        });
    CHECK(bleedNumber != result.visualEvents.end());
}

TEST_CASE("BattleFrameRunner combines simultaneous bleed producers into one holder-local transaction",
          "[battle][frame_runner][runtime][status][bleed][contribution]")
{
    auto state = runtimeFrameState();
    state.status.config.bleedDamageIntervalFrames = 10;

    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 80),
        teamRuntimeUnit(2, 0, 100),
        teamRuntimeUnit(3, 1, 100),
    });
    seedDamageExtrasFromUnits(state);

    const auto& bleedingRecord = state.units.require(1);
    auto bleeding = makeBattleStatusUnitState(
        bleedingRecord.status,
        bleedingRecord.core);
    auto first = BattleDamageSystem().applyBleed(
        bleeding,
        makeBattleStatusProducerProvenance({
            .kind = EffectSourceKind::Magic,
            .sourceId = 8001,
            .ownerUnitId = 0,
        }, EffectRuleId{ 8001 }),
        3,
        3);
    auto second = BattleDamageSystem().applyBleed(
        std::move(first.target),
        makeBattleStatusProducerProvenance({
            .kind = EffectSourceKind::Magic,
            .sourceId = 8002,
            .ownerUnitId = 2,
        }, EffectRuleId{ 8002 }),
        4,
        7);
    auto* bleed = second.target.effects.find(BattleStatusKind::Bleed);
    REQUIRE(bleed);
    REQUIRE(bleed->behaviorRuntime.size() == 1);
    bleed->behaviorRuntime.front().intervalFramesRemaining = 1;
    state.units.require(1).status = makeBattleStatusRuntimeUnit(second.target);

    const auto result = runBattleFrame(state);

    CHECK(damageLogAmountsFor(result, 1) == std::vector<int>{ 7 });
    CHECK(damageLogSourceIdsFor(result, 1) == std::vector<int>{ 2 });
    CHECK(state.units.requireCore(1).vitals.hp == 73);
    CHECK(std::ranges::count_if(result.logEvents, [](const BattleLogEvent& event)
    {
        return event.type == BattleLogEventType::Damage
            && event.targetUnitId == 1
            && BattleLogTest::textOf(event) == "流血";
    }) == 1);
    CHECK(std::ranges::count_if(result.visualEvents, [](const BattleVisualEvent& event)
    {
        return event.type == BattleVisualEventType::DamageNumber
            && event.targetUnitId == 1
            && event.color.r == 190
            && event.color.g == 120
            && event.color.b == 60;
    }) == 1);

    auto& deadHolder = state.units.require(1);
    deadHolder.core.alive = false;
    auto* deadBleed = deadHolder.status.effects.find(BattleStatusKind::Bleed);
    REQUIRE(deadBleed);
    deadBleed->behaviorRuntime.front().intervalFramesRemaining = 1;
    const auto deadFrame = runBattleFrame(state);
    CHECK(damageLogAmountsFor(deadFrame, 1).empty());
    CHECK(deadBleed->behaviorRuntime.front().intervalFramesRemaining == 1);
}

TEST_CASE("BattleFrameRunner_StatusDotsApplyOnlyLiveTypedDefenderModifiers", "[battle][frame_runner][runtime][status][damage]")
{
    const auto damageAfterTick = [](bool poison,
                                    std::vector<BattleStatusContribution> statuses)
    {
        auto state = runtimeFrameState();
        state.status.config.poisonDamageIntervalFrames = 30;
        state.status.config.bleedDamageIntervalFrames = 10;
        if (poison)
        {
            state.movement.frame = 30;
        }

        BattleStatusUnitState status;
        status.id = 1;
        status.alive = true;
        status.hp = 80;
        status.maxHp = 100;
        status.effects.statuses = std::move(statuses);
        for (const auto& contribution : status.effects.statuses)
        {
            status.effects.nextStatusSequence = std::max(
                status.effects.nextStatusSequence,
                contribution.appliedSequence + 1);
        }
        if (poison)
        {
            appendStatus(status.effects, BattleStatusKind::Poison, 3, 1, 10, 0, 1);
        }
        else
        {
            appendStatus(status.effects, BattleStatusKind::Bleed, 0, 8, 1, 0, 1);
        }
        seedRuntimeUnits(state, {
            teamRuntimeUnit(0, 0, 100),
            teamRuntimeUnit(1, 1, 80),
        });
        state.units.require(1).status = runtimeStatusUnit(status);
        seedDamageExtrasFromUnits(state);

        runBattleFrame(state);
        return 80 - state.units.requireCore(1).vitals.hp;
    };

    const auto witheredBone = KysChess::Battle::Test::boundStatusBehaviorContribution(
        KysChess::BattleStatusKind::WitheredBone,
        KysChess::Battle::Test::witheredBoneStatusBehavior(25, 75),
        0,
        9201);
    const auto battleSpirit = KysChess::Battle::Test::boundStatusBehaviorContribution(
        KysChess::BattleStatusKind::BattleSpirit,
        KysChess::Battle::Test::battleSpiritStatusBehavior(0, 50),
        1,
        9202);

    CHECK(damageAfterTick(true, {}) == 8);
    CHECK(damageAfterTick(false, {}) == 8);
    CHECK(damageAfterTick(true, { witheredBone }) == 10);
    CHECK(damageAfterTick(false, { witheredBone }) == 10);
    CHECK(damageAfterTick(true, { battleSpirit }) == 4);
    CHECK(damageAfterTick(false, { battleSpirit }) == 4);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_DecrementsInvincibility", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    seedRuntimeUnits(state, {
        teamRuntimeUnit(0, 0, 100),
        teamRuntimeUnit(1, 1, 100),
    });
    BattleStatusUnitState status0;
    status0.id = 0;
    BattleStatusUnitState status1;
    status1.id = 1;
    state.units.require(0).status = runtimeStatusUnit(status0);
    state.units.require(1).status = runtimeStatusUnit(status1);
    state.units.requireCore(0).invincible = 3;

    runBattleFrame(state);

    CHECK(state.units.requireCore(0).invincible == 2);
}

TEST_CASE("BattleFrameRunner_AdvanceFrame_AppliesProjectileCancelDamageCommand", "[battle][frame_runner][runtime][unit]")
{
    auto state = runtimeFrameState();
    appendTrackedRootAttack(state, cancelProjectile(10, 0));
    appendTrackedRootAttack(state, cancelProjectile(20, 1));
    state.attacks.attacks[0].state.projectileCancelDamage = 25;
    state.attacks.attacks[1].state.projectileCancelDamage = 12;
    auto result = runBattleFrame(state);

    REQUIRE(result.gameplayEvents.size() == 3);
    CHECK(result.gameplayEvents[2].type == BattleGameplayEventType::ProjectileCancelled);
    CHECK(result.gameplayEvents[2].effectId == 10);
    CHECK(result.gameplayEvents[2].otherAttackId == 20);
    REQUIRE_FALSE(result.visualEvents.empty());
    CHECK(std::ranges::all_of(result.gameplayEvents, [&](const BattleGameplayEvent& event)
        {
            return event.frame == result.frame;
        }));
    CHECK(std::ranges::all_of(result.visualEvents, [&](const BattleVisualEvent& event)
        {
            return event.frame == result.frame;
        }));

    REQUIRE(state.attacks.attacks.size() == 2);
    CHECK(state.attacks.attacks[0].state.projectileCancelWeaken == 12);
    CHECK(state.attacks.attacks[1].state.projectileCancelWeaken == 25);

    REQUIRE(result.logEvents.size() == 1);
    const auto& log = result.logEvents[0];
    CHECK(log.type == BattleLogEventType::Status);
    CHECK(log.category == BattleLogCategory::ProjectileCancel);
    CHECK(log.sourceUnitId == 0);
    CHECK(log.targetUnitId == 1);
    CHECK(log.amount == 25);
    CHECK(log.secondaryAmount == 12);
    CHECK(log.frame == result.frame);
}
