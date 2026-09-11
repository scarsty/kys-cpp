#pragma once

#include "battle/BattleCore.h"
#include "battle/BattleAreaEffectSystem.h"
#include "battle/BattleLogSegments.h"
#include "battle/BattleMovement.h"
#include "battle/BattleRuntimeSession.h"
#include "battle/BattleRuntimeRules.h"
#include "battle/BattleRuntimeUnitSpawn.h"
#include "ChessEftIds.h"
#include "ChessCombo.h"
#include "Find.h"
#include "BattleLogTestHelpers.h"
#include "BattleMovementTestHelpers.h"
#include "BattleRuntimeStateTestHelpers.h"
#include "BattlePresentationTestHelpers.h"
#include "BattleRuntimeRecordTestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace KysChess::Battle::Test
{

inline std::shared_ptr<const StatusBehaviorDefinition> periodicDamageBehavior(
    BattleDamageKind kind,
    EffectNumber amount,
    int intervalFrames,
    bool consumeStatus = false)
{
    DealDamageAction damage;
    damage.amount = amount;
    damage.kind = kind;
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::FrameAdvanced;
    rule.observation = EffectObservationScope::StatusHolderEventSource;
    rule.selector.kind = EffectSelectorKind::StatusHolder;
    rule.intervalFrames = intervalFrames;
    rule.actions.push_back(EffectAction{ damage });
    if (consumeStatus)
    {
        rule.actions.push_back(EffectAction{ ConsumeThisStatusAction{} });
    }
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(rule));
    return behavior;
}

inline std::vector<EffectRuleRuntimeState> initialStatusBehaviorRuntime(
    const std::shared_ptr<const StatusBehaviorDefinition>& behavior)
{
    std::vector<EffectRuleRuntimeState> result;
    if (!behavior) return result;
    result.reserve(behavior->rules.size());
    for (const auto& rule : behavior->rules)
    {
        result.push_back({ .intervalFramesRemaining = rule.intervalFrames });
    }
    return result;
}

inline BattleStatusContribution boundStatusBehaviorContribution(
    BattleStatusKind kind,
    std::shared_ptr<const StatusBehaviorDefinition> behavior,
    int sourceUnitId,
    int sourceId,
    int stacks = 1,
    std::uint64_t appliedSequence = 1,
    int remainingFrames = 120,
    std::uint32_t producerRuleOrder = 0)
{
    assert(behavior);
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = sourceId,
        .ownerUnitId = sourceUnitId,
    };
    const EffectRuleId ruleId{ static_cast<std::uint64_t>(sourceId) };
    auto behaviorRuntime = initialStatusBehaviorRuntime(behavior);
    return {
        .kind = kind,
        .producer = StatusProducerKey{ binding, ruleId, 0 },
        .producerFamily = StatusProducerFamilyKey{
            binding.kind,
            binding.sourceId,
            binding.ownerUnitId,
            ruleId,
            0,
        },
        .behavior = std::move(behavior),
        .behaviorRuntime = std::move(behaviorRuntime),
        .sourceUnitId = sourceUnitId,
        .remainingFrames = remainingFrames,
        .maximumFrames = remainingFrames,
        .stacks = stacks,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, producerRuleOrder },
        .appliedSequence = appliedSequence,
    };
}

inline std::shared_ptr<const StatusBehaviorDefinition> poisonStatusBehavior(
    int currentHpDamagePercent,
    int intervalFrames = 30)
{
    EffectNumber amount;
    amount.base = EffectNumberBase::TargetCurrentHp;
    amount.percent = currentHpDamagePercent;
    amount.minimum = 1;
    return periodicDamageBehavior(
        BattleDamageKind::Poison,
        amount,
        intervalFrames,
        true);
}

inline std::shared_ptr<const StatusBehaviorDefinition> bleedStatusBehavior(
    int maxHpDamagePercent = 1,
    int intervalFrames = 10)
{
    EffectNumber amount;
    amount.base = EffectNumberBase::TargetMaxHp;
    amount.percent = maxHpDamagePercent;
    amount.minimum = 1;
    amount.statusScale = StatusNumberScale::PerContributionLayer;
    return periodicDamageBehavior(
        BattleDamageKind::Bleed,
        amount,
        intervalFrames);
}

inline std::shared_ptr<const StatusBehaviorDefinition> trueQiStatusBehavior(
    int pureDamagePerLayer)
{
    DealDamageAction damage;
    damage.amount.flat = pureDamagePerLayer;
    damage.amount.statusScale = StatusNumberScale::PerContributionLayer;
    damage.kind = BattleDamageKind::Pure;
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::HitBeforeDamage;
    rule.observation = EffectObservationScope::StatusHolderEventSource;
    rule.selector.kind = EffectSelectorKind::HitTarget;
    rule.actions.push_back(EffectAction{ damage });
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(rule));
    return behavior;
}

inline std::shared_ptr<const StatusBehaviorDefinition> sevenStarStatusBehavior()
{
    ModifyDamageAction ignoreDefense;
    ignoreDefense.stage = DamageModifierStage::BeforeDefense;
    ignoreDefense.channel = DamageChannel::Skill;
    ignoreDefense.amount.flat = 50;
    ignoreDefense.operation = DamageModifierOperation::IgnoreDefensePercent;

    ApplyStatusAction stun;
    stun.status = BattleStatusKind::Stun;
    stun.durationFrames = 30;
    stun.quantity = NoStatusQuantity{};
    stun.reapplication = StatusReapplicationPolicy::KeepLongerDuration;

    ConsumeThisStatusAction consume;
    consume.quantity = 1;
    consume.whenDepleted = std::move(stun);

    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::HitBeforeDamage;
    rule.observation = EffectObservationScope::SourceOwnerTeamEventSource;
    rule.selector.kind = EffectSelectorKind::HitTarget;
    rule.conditions.push_back(TargetIsStatusHolderCondition{});
    rule.actions = { EffectAction{ ignoreDefense }, EffectAction{ consume } };

    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(rule));
    return behavior;
}

inline std::shared_ptr<const StatusBehaviorDefinition> persistentStatusBehavior(
    std::vector<EffectAction> actions)
{
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::StatusPersistent;
    rule.observation = EffectObservationScope::StatusHolderEventSource;
    rule.selector.kind = EffectSelectorKind::StatusHolder;
    rule.actions = std::move(actions);
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(rule));
    return behavior;
}

inline std::shared_ptr<const StatusBehaviorDefinition> attackSuppressionStatusBehavior(
    BattleStatusKind kind,
    int mpRecoveryAmount = 0)
{
    if (kind == BattleStatusKind::NeutralizeForce)
    {
        ApplyStatusAction action;
        action.status = kind;
        action.neutralizeMpRecovery = EffectNumber{ .flat = mpRecoveryAmount };
        return makeCatalogOwnedStatusBehavior(action);
    }
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::HitBeforeDamage;
    rule.selector.kind = EffectSelectorKind::StatusHolder;
    if (kind == BattleStatusKind::NextAttackMiss)
    {
        rule.observation = EffectObservationScope::StatusHolderEventTarget;
        rule.actions.push_back(EffectAction{ MakeIncomingAttackMissAction{} });
    }
    else
    {
        assert(kind == BattleStatusKind::Blinded);
        rule.observation = EffectObservationScope::StatusHolderEventSource;
        rule.actions.push_back(EffectAction{ SuppressCurrentCastContactsAction{} });
    }
    auto behavior = std::make_shared<StatusBehaviorDefinition>();
    behavior->rules.push_back(std::move(rule));
    return behavior;
}

inline std::shared_ptr<const StatusBehaviorDefinition> witheredBoneStatusBehavior(
    int damageTakenPercent,
    int receivedHealingReductionPercent)
{
    ModifyDamageAction damage;
    damage.perspective = DamageModifierPerspective::Incoming;
    damage.stage = DamageModifierStage::Final;
    damage.channel = DamageChannel::All;
    damage.amount.flat = damageTakenPercent;
    damage.operation = DamageModifierOperation::PercentAdd;
    ModifyHealTransactionAction healing;
    healing.operation = HealModifierOperation::MultiplyReceived;
    healing.kinds = {
        "直接", "隊伍", "光環", "命中", "擊殺獎勵",
        "死亡醫療", "救援", "生命回復", "吸血",
    };
    healing.percent = 100 - receivedHealingReductionPercent;
    return persistentStatusBehavior({ EffectAction{ damage }, EffectAction{ healing } });
}

inline std::shared_ptr<const StatusBehaviorDefinition> battleSpiritStatusBehavior(
    int skillDamagePercentPerLayer,
    int damageReductionPercentPerLayer)
{
    ModifyDamageAction outgoing;
    outgoing.perspective = DamageModifierPerspective::Outgoing;
    outgoing.stage = DamageModifierStage::BeforeDefense;
    outgoing.channel = DamageChannel::Skill;
    outgoing.amount.flat = skillDamagePercentPerLayer;
    outgoing.amount.statusScale = StatusNumberScale::PerContributionLayer;
    outgoing.operation = DamageModifierOperation::PercentAdd;
    ModifyDamageAction incoming;
    incoming.perspective = DamageModifierPerspective::Incoming;
    incoming.stage = DamageModifierStage::BeforeDefense;
    incoming.channel = DamageChannel::All;
    incoming.amount.flat = -damageReductionPercentPerLayer;
    incoming.amount.statusScale = StatusNumberScale::PerContributionLayer;
    incoming.operation = DamageModifierOperation::PercentAdd;
    return persistentStatusBehavior({ EffectAction{ outgoing }, EffectAction{ incoming } });
}

inline std::shared_ptr<const StatusBehaviorDefinition> damageBlockStatusBehavior()
{
    return persistentStatusBehavior({ EffectAction{ BlockPositiveDamageAction{} } });
}

inline std::shared_ptr<const StatusBehaviorDefinition> singleHitCapStatusBehavior(int cap)
{
    ModifyDamageAction action;
    action.perspective = DamageModifierPerspective::Incoming;
    action.stage = DamageModifierStage::Final;
    action.channel = DamageChannel::All;
    action.amount.flat = cap;
    action.operation = DamageModifierOperation::CapSingleHitAtValue;
    return persistentStatusBehavior({ EffectAction{ action } });
}

inline BattleAttackPayload ordinaryProjectilePayload()
{
    return {
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary,
    };
}


inline bool hasVisualEvent(const BattlePresentationFrame& frame, BattleVisualEventType type)
{
    return std::any_of(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type](const BattleVisualEvent& event)
        {
            return event.type == type;
        });
}

inline const BattleVisualEvent* findVisualEvent(const BattlePresentationFrame& frame, BattleVisualEventType type)
{
    const auto it = std::find_if(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type](const BattleVisualEvent& event)
        {
            return event.type == type;
        });
    return it != frame.visualEvents.end() ? &*it : nullptr;
}

inline const BattleVisualEvent* findVisualEvent(
    const BattlePresentationFrame& frame,
    BattleVisualEventType type,
    int effectId)
{
    const auto it = std::find_if(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [type, effectId](const BattleVisualEvent& event)
        {
            return event.type == type && event.effectId == effectId;
        });
    return it != frame.visualEvents.end() ? &*it : nullptr;
}

inline bool hasGameplayEvent(const BattlePresentationFrame& frame, BattleGameplayEventType type)
{
    return std::any_of(
        frame.gameplayEvents.begin(),
        frame.gameplayEvents.end(),
        [type](const BattleGameplayEvent& event)
        {
            return event.type == type;
        });
}

inline bool hasLogText(const BattlePresentationFrame& frame, const std::string& text)
{
    return std::any_of(
        frame.logEvents.begin(),
        frame.logEvents.end(),
        [&text](const BattleLogEvent& event)
        {
            return BattleLogTest::textOf(event) == text;
        });
}

inline bool hasHealVisualEvent(const BattlePresentationFrame& frame, int targetUnitId)
{
    return std::any_of(
        frame.visualEvents.begin(),
        frame.visualEvents.end(),
        [targetUnitId](const BattleVisualEvent& event)
        {
            return event.type == BattleVisualEventType::RoleEffect
                && event.targetUnitId == targetUnitId
                && event.effectId == KysChess::EFT_HEAL;
        });
}

inline bool hasProjectilePresentationEvent(const BattlePresentationFrame& frame)
{
    return hasVisualEvent(frame, BattleVisualEventType::ProjectileSpawned)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileMoved)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileHit)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileExpired)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileTargetLost)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileCancelled)
        || hasVisualEvent(frame, BattleVisualEventType::ProjectileBounced);
}
inline BattlePresentationFrame runBattleFrame(BattleRuntimeState& state)
{
    if (state.action.castConfig.castFrames.front() == 0)
    {
        state.action.castConfig.castFrames = { 6, 6, 6, 6 };
        state.action.castConfig.recoveryFrames = { 4, 4, 4, 5 };
    }
    if (state.action.castConfig.minimumFacingNorm <= 0.0)
    {
        state.action.castConfig.minimumFacingNorm = TestMinimumVectorNorm;
    }
    return BattleFrameRunner().runFrame(state);
}

inline void appendTrackedAttack(
    BattleRuntimeState& state,
    BattleAttackInstance attack,
    std::optional<int> syntheticParentRuntimeAttackId = std::nullopt,
    bool ultimate = false)
{
    assert(attack.id >= 0);
    assert(attack.state.attackSourceUnitId >= 0);
    assert(!attack.provenance.valid());
    assert(!attack.castWork.valid());

    const auto cast = state.castLifecycle.beginRootCast({
        .sourceUnitId = attack.state.attackSourceUnitId,
        .magicId = attack.state.skillId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
    });
    state.effectIntegration.casts.emplace(
        cast.provenance.castId,
        BattleEffectCastRuntimeContext{
            .originalTargetUnitId = attack.state.preferredTargetUnitId,
        });
    BattleAttackReservation reservation{};
    if (syntheticParentRuntimeAttackId)
    {
        const auto parent = state.castLifecycle.reserveAttack(
            cast.provenance.castId,
            { .rootAttack = true });
        const auto parentAttackId = battleAttackIdFromRuntimeId(
            *syntheticParentRuntimeAttackId);
        state.castLifecycle.transferToLiveAttack(parent.work, parentAttackId);
        reservation = state.castLifecycle.reserveAttack(
            cast.provenance.castId,
            {
                .parentAttackId = parentAttackId,
                .origin = BattleAttackOriginKind::Bounce,
            });
        state.castLifecycle.completeWork(
            parent.work,
            CastWorkResult::attackFinished(AttackFinishReason::SpentOnHit));
    }
    else
    {
        reservation = state.castLifecycle.reserveAttack(
            cast.provenance.castId,
            { .rootAttack = true });
    }

    attack.provenance = completeAttackProvenance(
        reservation.provenance,
        battleAttackIdFromRuntimeId(attack.id));
    attack.castWork = reservation.work;
    state.castLifecycle.transferToLiveAttack(
        attack.castWork,
        attack.provenance.attackId);
    state.castLifecycle.completeWork(cast.commitBarrier);
    state.attacks.attacks.push_back(std::move(attack));
}

inline void queueTrackedAttack(
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
        {
            .rootAttack = true,
            .mainProjectile = request.provenance.mainProjectile,
            .sharedHitGroupId = request.provenance.sharedHitGroupId,
        });
    request.provenance = reservation.provenance;
    request.castWork = reservation.work;
    state.effectIntegration.casts.emplace(
        cast.provenance.castId,
        BattleEffectCastRuntimeContext{
            .originalTargetUnitId = request.initial.preferredTargetUnitId,
        });
    state.castLifecycle.completeWork(cast.commitBarrier);
    state.nextFrame.queueAttack(std::move(request));
}

inline BattleAttackSpawnRequest attackSpawnRequest()
{
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.skillId = 101;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = 44;
    request.initial.preferredTargetUnitId = 0;
    request.initial.position = { 100, 120, 0 };
    request.initial.velocity = { 6, 0, 0 };
    request.initial.totalFrame = 30;
    request.initial.track = true;
    request.initial.through = true;
    request.provenance.sharedHitGroupId = 7;
    return request;
}

inline BattleStatusUnitState statusUnitSnapshot(int id, int hp)
{
    BattleStatusUnitState state;
    state.id = id;
    state.alive = true;
    state.hp = hp;
    state.maxHp = 100;
    return state;
}

inline BattleStatusRuntimeUnit statusRuntimeSnapshot(int unitIndex, int hp)
{
    return makeBattleStatusRuntimeUnit(statusUnitSnapshot(unitIndex, hp));
}

struct HitDamageFrameState
{
    BattleRuntimeState state;
};

inline HitDamageFrameState hitDamageFrameState(
    int resolvedBaseDamage,
    int defenderHp,
    bool ultimate = false)
{
    HitDamageFrameState frame;
    auto& state = frame.state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, 100);

    BattleAttackInstance projectile{ ordinaryProjectilePayload() };
    projectile.id = 10;
    projectile.state.attackSourceUnitId = 0;
    projectile.state.skillId = 101;
    projectile.state.skillMagicPower = resolvedBaseDamage * 12;
    projectile.state.totalFrame = 30;
    projectile.state.operationType = BattleOperationType::RangedProjectile;
    projectile.state.position = { 100, 100, 0 };
    projectile.state.velocity = { 5, 0, 0 };
    appendTrackedAttack(state, std::move(projectile), std::nullopt, ultimate);

    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 80, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 105, 100, 0 }),
});
    state.units.require(0).status = statusRuntimeSnapshot(0, 80);
    state.units.require(1).status = statusRuntimeSnapshot(1, defenderHp);
    for (const auto& unit : state.units.cores())
    {
        const auto damage = makeBattleDamageUnitState(
            unit,
            static_cast<const BattleDamageRuntimeUnit*>(nullptr));
        state.units.require(unit.id).damage = makeBattleDamageRuntimeUnit(damage);
        state.units.require(unit.id).comboFacts = KysChess::Battle::BattleComboRuntimeFacts{};
    }
    return frame;
}

inline BattleRuntimeState attackSuppressionFrameState()
{
    BattleRuntimeState state;
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }, CombatStyle::Ranged),
        unit(1, 1, { 105, 100, 0 }),
        unit(2, 1, { 125, 100, 0 }),
    }));
    state.attacks = attackWorld();
    seedRuntimeUnitsFromWorld(state);
    return state;
}

inline void addAttackSuppressionStatus(
    BattleRuntimeState& state,
    int unitId,
    BattleStatusKind kind,
    int potency = 0,
    int stacks = 1)
{
    auto& effects = state.units.require(unitId).status.effects;
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Magic,
        .sourceId = 700 + static_cast<int>(kind),
        .ownerUnitId = unitId,
        .sourceTeam = state.units.requireCore(unitId).team,
    };
    const EffectRuleId ruleId{ static_cast<std::uint64_t>(700 + static_cast<int>(kind)) };
    const auto behavior = attackSuppressionStatusBehavior(kind, potency);
    effects.statuses.push_back({
        .kind = kind,
        .producer = StatusProducerKey{ .binding = binding, .ruleId = ruleId },
        .producerFamily = StatusProducerFamilyKey{
            .sourceKind = binding.kind,
            .sourceId = binding.sourceId,
            .logicalOwnerUnitId = binding.ownerUnitId,
            .ruleId = ruleId,
        },
        .behavior = behavior,
        .behaviorRuntime = initialStatusBehaviorRuntime(behavior),
        .sourceUnitId = unitId,
        .remainingFrames = 120,
        .stacks = stacks,
        .origin = BattleStatusEffectOrigin{ binding, ruleId, 0 },
        .appliedSequence = effects.nextStatusSequence++,
    });
}

inline bool hasAttackSuppressionStatus(
    const BattleRuntimeState& state,
    int unitId,
    BattleStatusKind kind)
{
    return BattleStatusSystem({}).snapshot(
        state.units.require(unitId).statusDamageState()).has(kind);
}

inline void addAttackContactShieldRule(BattleRuntimeState& state, int amount)
{
    ChangeResourceAction shield;
    shield.resource = BattleResource::Shield;
    shield.kind = ResourceChangeKind::Grant;
    shield.amount.flat = amount;
    EffectRule rule;
    rule.id = EffectRuleId{ 1 };
    rule.event = EffectEvent::HitBeforeDamage;
    rule.selector.kind = EffectSelectorKind::HitTarget;
    rule.actions = { EffectAction{ EffectActionValue{ shield } } };
    state.effectRules.append({
        .kind = EffectSourceKind::Combo,
        .sourceId = 777,
        .ownerUnitId = 0,
        .sourceTeam = 0,
    }, rule);
}

inline EffectSourceBinding testOwnerRuleBinding(
    const BattleRuntimeState& state,
    int ownerUnitId,
    int sourceId)
{
    return {
        .kind = EffectSourceKind::Combo,
        .sourceId = sourceId,
        .ownerUnitId = ownerUnitId,
        .sourceTeam = state.units.requireCore(ownerUnitId).team,
    };
}

inline void addTestOwnerRule(
    BattleRuntimeState& state,
    int ownerUnitId,
    int sourceId,
    int ruleId,
    EffectEvent event,
    EffectActionValue action,
    int chancePct = 100,
    int intervalFrames = 0)
{
    EffectRule rule;
    rule.id = { static_cast<std::uint64_t>(ruleId) };
    rule.event = event;
    rule.selector.kind = EffectSelectorKind::Self;
    rule.chancePct = chancePct;
    rule.intervalFrames = intervalFrames;
    rule.actions = { EffectAction{ std::move(action) } };
    state.effectRules.append(
        testOwnerRuleBinding(state, ownerUnitId, sourceId),
        rule);
}

inline void addTestCastMobilityRule(
    BattleRuntimeState& state,
    int ownerUnitId,
    CastMobilityPolicy mobility)
{
    ModifyCastAction action;
    action.mobility = mobility;
    addTestOwnerRule(
        state,
        ownerUnitId,
        9100 + static_cast<int>(mobility),
        1,
        EffectEvent::CastPlanned,
        EffectActionValue{ action });
}

inline void addTestExecuteRule(BattleRuntimeState& state, int ownerUnitId, int thresholdPct)
{
    ModifyDamageAction action;
    action.perspective = DamageModifierPerspective::Outgoing;
    action.stage = DamageModifierStage::Final;
    action.channel = DamageChannel::All;
    action.amount.flat = thresholdPct;
    action.operation = DamageModifierOperation::ExecuteBelowMaxHpPercent;
    addTestOwnerRule(
        state,
        ownerUnitId,
        9150,
        1,
        EffectEvent::HitBeforeDamage,
        EffectActionValue{ action });
}

inline void addTestIgnoreDefenseRule(BattleRuntimeState& state, int ownerUnitId, int amountPct)
{
    ModifyDamageAction action;
    action.perspective = DamageModifierPerspective::Outgoing;
    action.stage = DamageModifierStage::BeforeDefense;
    action.channel = DamageChannel::All;
    action.amount.flat = amountPct;
    action.operation = DamageModifierOperation::IgnoreDefensePercent;
    addTestOwnerRule(
        state,
        ownerUnitId,
        9160,
        1,
        EffectEvent::HitBeforeDamage,
        EffectActionValue{ action });
}

inline EffectRule shippedGusuMurongDamageDebuffRule()
{
    ChessDiagnosticCollector diagnostics;
    const auto identity = [](std::string_view text)
    {
        return std::string(text);
    };
    const auto combos = loadChessCombos(
        (std::filesystem::current_path() / "config" / "chess_combos.yaml").string(),
        identity,
        diagnostics.sink());
    INFO("姑蘇慕容配置載入診斷數量: " << diagnostics.diagnostics().size());
    REQUIRE_FALSE(combos.empty());
    const auto combo = std::ranges::find_if(combos, [](const ComboDef& candidate)
    {
        return std::ranges::find(candidate.memberRoleIds, 113)
            != candidate.memberRoleIds.end();
    });
    REQUIRE(combo != combos.end());
    const auto threshold = std::ranges::find(
        combo->thresholds,
        4,
        &ComboThreshold::count);
    REQUIRE(threshold != combo->thresholds.end());
    const auto rule = std::ranges::find_if(threshold->rules, [](const EffectRule& candidate)
    {
        if (candidate.event != EffectEvent::DamageResolved)
        {
            return false;
        }
        return std::ranges::any_of(candidate.actions, [](const EffectAction& action)
        {
            const auto* damage = std::get_if<ModifyDamageAction>(&action.value);
            return damage
                && damage->perspective == DamageModifierPerspective::Outgoing
                && damage->operation == DamageModifierOperation::PercentAdd
                && damage->amount.flat == -45
                && damage->durationFrames == 70;
        });
    });
    REQUIRE(rule != threshold->rules.end());
    return *rule;
}

struct TestEffectRuleHandle
{
    EffectSourceBinding binding;
    EffectRuleId ruleId;
};

inline TestEffectRuleHandle addShippedGusuMurongDamageDebuffRule(
    BattleRuntimeState& state,
    int ownerUnitId)
{
    const EffectSourceBinding binding{
        .kind = EffectSourceKind::Combo,
        .sourceId = 10'113,
        .ownerUnitId = ownerUnitId,
        .sourceTeam = state.units.requireCore(ownerUnitId).team,
    };
    const auto rule = shippedGusuMurongDamageDebuffRule();
    state.effectRules.append(binding, rule);
    return { binding, rule.id };
}

inline bool hasGusuMurongDamageDebuff(
    const BattleRuntimeState& state,
    int ownerUnitId,
    int targetUnitId)
{
    return std::ranges::any_of(
        state.effectCommands.damageModifiers,
        [=](const BattleDamageModifierInstance& modifier)
        {
            return modifier.binding.ownerUnitId == ownerUnitId
                && modifier.targetUnitId == targetUnitId
                && modifier.perspective == DamageModifierPerspective::Outgoing
                && modifier.operation == DamageModifierOperation::PercentAdd
                && modifier.amount == -45
                && modifier.expiresFrameExclusive
                    == static_cast<std::int64_t>(modifier.appliedFrame) + 70;
        });
}

inline void addTestPeriodicAutoUltimateRule(
    BattleRuntimeState& state,
    int ownerUnitId,
    int intervalFrames = 1)
{
    ModifyCastAction action;
    action.autoUltimate = AutoUltimateCastRequest{
        .consumeMp = false,
        .announce = true,
    };
    addTestOwnerRule(
        state,
        ownerUnitId,
        9200,
        1,
        EffectEvent::FrameAdvanced,
        EffectActionValue{ action },
        100,
        intervalFrames);
}

inline BattleAttackSpawnRequest attackSuppressionRequest(int scriptedDamage = 25)
{
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        scriptedDamage > 0
            ? BattleProjectilePayloadClass::scriptedDamage()
            : BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.skillId = 101;
    request.initial.skillMagicPower = 840;
    request.initial.scriptedDamage = scriptedDamage;
    request.initial.totalFrame = 30;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 100, 0 };
    request.initial.velocity = { 5, 0, 0 };
    request.initial.through = true;
    return request;
}

struct TrackedAttackHandle
{
    int attackId = -1;
    BattleCastId castId;
};

struct TrackedAttackCastReservation
{
    BattleCastId castId;
    std::vector<BattleAttackSpawnRequest> requests;
};

inline TrackedAttackCastReservation reserveTrackedAttackCast(
    BattleRuntimeState& state,
    std::vector<BattleAttackSpawnRequest>&& requests,
    int originalTargetUnitId = OptionalPreferredTargetUnitId)
{
    assert(!requests.empty());
    const int sourceUnitId = requests.front().initial.attackSourceUnitId;
    const int skillId = requests.front().initial.skillId;
    const auto cast = state.castLifecycle.beginRootCast({
        .sourceUnitId = sourceUnitId,
        .magicId = skillId,
    });
    for (std::size_t i = 0; i < requests.size(); ++i)
    {
        auto& request = requests[i];
        assert(request.initial.attackSourceUnitId == sourceUnitId);
        assert(request.initial.skillId == skillId);
        const auto reservation = state.castLifecycle.reserveAttack(
            cast.provenance.castId,
            { .rootAttack = i == 0 });
        request.provenance = reservation.provenance;
        request.castWork = reservation.work;
    }
    state.castLifecycle.completeWork(cast.commitBarrier);
    state.effectIntegration.casts.emplace(
        cast.provenance.castId,
        BattleEffectCastRuntimeContext{
            .originalTargetUnitId = originalTargetUnitId,
        });
    return { cast.provenance.castId, std::move(requests) };
}

inline TrackedAttackHandle spawnTrackedAttack(
    BattleRuntimeState& state,
    BattleAttackSpawnRequest request,
    int originalTargetUnitId = OptionalPreferredTargetUnitId)
{
    std::vector<BattleAttackSpawnRequest> requests;
    requests.push_back(std::move(request));
    auto tracked = reserveTrackedAttackCast(
        state,
        std::move(requests),
        originalTargetUnitId);
    const auto spawned = state.attacks.spawn(
        std::move(tracked.requests.front()),
        state.castLifecycle);
    return { spawned.attackId, tracked.castId };
}

inline void advanceUntilAttackContacts(
    BattleRuntimeState& state,
    int attackId,
    std::size_t expectedContactCount)
{
    constexpr int MaximumFrames = 12;
    for (int frame = 0;
         frame < MaximumFrames
         && requireById(state.attacks.attacks, attackId).hitUnitIds.size()
             < expectedContactCount;
         ++frame)
    {
        runBattleFrame(state);
    }
    REQUIRE(requireById(state.attacks.attacks, attackId).hitUnitIds.size()
            == expectedContactCount);
}

inline BattleCastConfig frameCastConfig()
{
    BattleCastConfig config;
    config.castFrames = { 25, 30, 20, 25 };
    config.baseCooldownFrames = { 105, 185, 115, 45 };
    config.minimumCooldownFrames = { 60, 70, 70, 45 };
    config.cooldownActPropertyDivisors = { 2, 1, 2, 0 };
    config.recoveryFrames = { 4, 4, 4, 5 };
    config.maxCooldownSpeed = 150;
    config.maximumSpeedCooldownReductionPct = 50;
    config.minimumCooldownAfterCastPadding = 2;
    config.normalCastMpDelta = 5;
    config.minimumFacingNorm = TestMinimumVectorNorm;
    config.meleeHitTotalFrame = 10;
    config.strengthenedMeleeTotalFrame = 30;
    config.strengthenedMeleeSelectDistanceDivisor = 2.0;
    config.strengthenedMeleeStrengthPct = 200;
    config.meleeSplashTotalFrame = 60;
    config.meleeSplashInitialFrame = 5;
    config.meleeSplashStrengthPct = 50;
    config.trackingProjectileTotalFrame = 120;
    config.dashHitTotalFrame = 30;
    config.strengthenedMeleeOperationCountThreshold = 2;
    return config;
}

inline BattleCastInput frameCastInput(int sourceUnitId, int targetUnitId)
{
    BattleCastInput input;
    input.config = frameCastConfig();
    input.geometry.meleeAttackEffectOffset = SceneTileWidth * 2.0;
    input.geometry.projectileSpeed = SceneProjectileSpeed;
    input.geometry.projectileSpawnOffset = SceneTileWidth * 2.0;
    input.geometry.projectileBaseTravel = SceneTileWidth * 5.0;
    input.geometry.projectileTravelPerSelectDistance = SceneTileWidth;
    input.geometry.meleeSplashProjectileSpeed = 3.0;
    input.geometry.dashHitPositionSpacing = 2.0;
    input.geometry.dashVelocityMagnitude = SceneTileWidth * 2.0 / 5.0;
    input.geometry.dashHitFrameStep = 3;
    input.unit.id = sourceUnitId;
    input.unit.position = { 10.0f, 20.0f, 0.0f };
    input.unit.facing = { 1.0f, 0.0f, 0.0f };
    input.unit.alive = true;
    input.unit.canStartAttack = true;
    input.unit.mp = 20;
    input.unit.maxMp = 100;
    input.unit.meleeAttackReach = 137.5;
    input.targetUnitId = targetUnitId;
    input.targetPosition = { 82.0f, 20.0f, 0.0f };
    input.targetDistance = 100.0;
    input.normalSkill.id = 301;
    input.normalSkill.name = "框架招式";
    input.normalSkill.attackAreaType = 0;
    input.normalSkill.magicType = 1;
    input.normalSkill.visualEffectId = 77;
    input.normalSkill.reach = 137.5;
    input.ultimateSkill.id = 401;
    input.ultimateSkill.name = "絕招";
    input.ultimateSkill.soundId = 55;
    input.ultimateSkill.attackAreaType = 1;
    input.ultimateSkill.magicType = 1;
    input.ultimateSkill.visualEffectId = 88;
    input.ultimateSkill.reach = 400.0;
    input.ultimateSkill.rangedStyle = true;
    return input;
}

inline BattleActionSkillSeed actionSkillSeedFromCastSkill(const BattleCastSkillState& skill)
{
    BattleActionSkillSeed seed;
    seed.id = skill.id;
    seed.name = skill.name;
    seed.soundId = skill.soundId;
    seed.hurtType = skill.hurtType;
    seed.attackAreaType = skill.attackAreaType;
    seed.magicType = skill.magicType;
    seed.visualEffectId = skill.visualEffectId;
    seed.selectDistance = skill.selectDistance;
    seed.actProperty = skill.actProperty;
    seed.magicPower = skill.magicPower;
    return seed;
}

inline BattleActionPlanSeed actionPlanSeedFromCastInput(const BattleCastInput& input)
{
    BattleActionPlanSeed seed;
    seed.hasEquippedSkill = input.unit.hasEquippedSkill;
    seed.normalSkill = actionSkillSeedFromCastSkill(input.normalSkill);
    seed.ultimateSkill = actionSkillSeedFromCastSkill(input.ultimateSkill);
    return seed;
}

inline void configureRuntimeActionPlan(BattleRuntimeState& state, BattleCastInput input)
{
    (void)state.units.require(input.unit.id);
    state.action.castConfig = input.config;
    state.action.castConfig.castFrames = { 6, 6, 6, 6 };
    state.action.castGeometry = input.geometry;
    state.movement.config = testConfig();
    state.movement.config.meleeAttackReach = input.unit.meleeAttackReach;
    const double dashAttackReach = input.unit.dashAttackReach > 0.0
        ? input.unit.dashAttackReach
        : 375.0;
    state.movement.config.meleeLocalTargetRadius =
        dashAttackReach - state.movement.config.meleeAttackReach;
    state.attacks.hitRadius = SceneAttackHitRadius;
    state.action.actionRules.heavyAttackReach = SceneTileWidth * 4.0;
    state.action.actionRules.projectileBounceRange = 90;
    state.units.require(input.unit.id).setActionPlan(actionPlanSeedFromCastInput(input));
}

inline BattlePendingCastAction framePendingCastAction()
{
    auto cast = frameCastInput(0, 1);
    cast.normalSkill.id = 101;
    BattlePendingCastAction pending;
    pending.targetUnitId = 1;
    pending.operationType = BattleOperationType::RangedProjectile;
    pending.castFrame = 6;
    pending.skillPlan = cast.normalSkill;
    return pending;
}

inline void setTrackedPendingCast(
    BattleRuntimeState& state,
    int unitId,
    BattlePendingCastAction pending,
    bool ultimate = false)
{
    assert(!pending.effectCast.provenance.valid());
    const int magicId = pending.skillPlan.id;
    pending.skillPlan.id = -1;
    pending.effectCast = state.castLifecycle.beginRootCast({
        .sourceUnitId = unitId,
        .magicId = magicId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        .propagation = CastPropagationPolicy::SourceRules,
    });
    state.units.require(unitId).setPendingCast(std::move(pending));
}

inline void preparePendingCastCommitFrame(BattleRuntimeState& state,
                                   int unitId,
                                   BattleOperationType operationType = BattleOperationType::RangedProjectile,
                                   int actFrame = 6)
{
    auto& unit = state.units.requireCore(unitId);
    unit.haveAction = true;
    unit.animation.actFrame = actFrame;
    unit.operationType = operationType;
    unit.animation.actType = 1;
    unit.animation.cooldown = 10;
    if (auto* pending = state.units.require(unitId).pendingCast())
    {
        pending->castFrame = actFrame;
    }
}

inline void configureAutoUltimateActionRuntime(BattleRuntimeState& state, int unitId, int targetUnitId)
{
    configureRuntimeActionPlan(state, frameCastInput(unitId, targetUnitId));
    state.action.actionRules.projectileBounceRange = 90;
}

inline BattleCastResult committedFrameCast()
{
    BattleCastResult result;
    result.decision.canCast = true;
    result.decision.unitId = 0;
    result.decision.targetUnitId = 1;
    result.decision.skillId = 101;
    result.decision.operationType = BattleOperationType::RangedProjectile;
    BattleAttackSpawnRequest request{ BattleAttackPayload(
        BattleAttackDelivery::projectile(),
        BattleProjectilePayloadClass::combat(),
        BattleAttackReflectionLineageKind::Ordinary) };
    request.initial.attackSourceUnitId = 0;
    request.initial.skillId = 101;
    request.initial.preferredTargetUnitId = 1;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.position = { 100, 100, 0 };
    request.initial.velocity = { 5, 0, 0 };
    request.initial.totalFrame = 30;
    result.attackSpawnRequests.push_back(request);
    return result;
}

inline BattleDamageTransactionInput lethalDamageInput(int attackerUnitId, int defenderUnitId)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = 20;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.vitals = { 100, 100, 0, 0 };
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.vitals = { 10, 100, 0, 0 };
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = 10;
    input.defenderStatus.maxHp = 100;
    return input;
}

inline BattleDamageTransactionInput preResolvedDamageInput(int attackerUnitId, int defenderUnitId, int hpBefore, int damage)
{
    BattleDamageTransactionInput input;
    input.request.attackerUnitId = attackerUnitId;
    input.request.defenderUnitId = defenderUnitId;
    input.request.baseDamage = damage;
    input.request.preResolvedDamage = true;
    input.attacker.id = attackerUnitId;
    input.attacker.alive = true;
    input.attacker.vitals = { 100, 100, 0, 0 };
    input.defender.id = defenderUnitId;
    input.defender.alive = true;
    input.defender.vitals = { hpBefore, 100, 0, 0 };
    input.defenderStatus.id = defenderUnitId;
    input.defenderStatus.alive = true;
    input.defenderStatus.hp = hpBefore;
    input.defenderStatus.maxHp = 100;
    return input;
}

template <typename T>
T& ensureById(std::vector<T>& items, int id)
{
    if (auto* item = tryFindById(items, id))
    {
        return *item;
    }

    T added;
    added.id = id;
    items.push_back(added);
    return items.back();
}

inline void queuePendingDamage(
    BattleRuntimeState& state,
    BattleDamageTransactionInput transaction,
    BattleDamagePresentationInput presentation = {},
    std::optional<EffectDamageOrigin> effectOrigin = std::nullopt)
{
    if (transaction.attacker.id >= 0)
    {
        state.units.writeDamageUnit(transaction.attacker);
        writeBattleDamageRuntimeUnit(
            state.units.require(transaction.attacker.id).damage,
            transaction.attacker);
    }

    state.units.writeDamageUnit(transaction.defender);
    writeBattleDamageRuntimeUnit(
        state.units.require(transaction.defender.id).damage,
        transaction.defender);
    writeBattleStatusRuntimeUnit(
        state.units.require(transaction.defenderStatus.id).status,
        transaction.defenderStatus);
    state.nextFrame.queueDamage(pendingDamageIntent(
        std::move(transaction),
        std::move(presentation),
        std::move(effectOrigin)));
}

inline BattleRescueCellSnapshot rescueCell(int x, int y, bool walkable = true, bool occupied = false)
{
    return {
        x,
        y,
        walkable,
        occupied,
        occupied ? 99 : -1,
        { static_cast<float>(x * SceneTileWidth), static_cast<float>(y * SceneTileWidth), 0.0f },
    };
}

inline std::vector<BattleRescueCellSnapshot> rescueOpenCells(int width, int height)
{
    std::vector<BattleRescueCellSnapshot> cells;
    cells.reserve(width * height);
    for (int x = 0; x < width; ++x)
    {
        for (int y = 0; y < height; ++y)
        {
            cells.push_back(rescueCell(x, y));
        }
    }
    return cells;
}

inline BattleRuntimeState rescueDamageFrameState(int defenderHp, int damage)
{
    BattleRuntimeState state;
    state.gridTransform = { SceneTileWidth, 64 };
    configureRuntimeMovement(state, worldWith({
        unit(0, 0, { 100, 100, 0 }),
        unit(1, 1, { 180, 180, 0 }),
        unit(2, 1, { 72, 72, 0 }),
    }));
    state.attacks = attackWorld();
    state.units.require(0).status = statusRuntimeSnapshot(0, 100);
    state.units.require(1).status = statusRuntimeSnapshot(1, defenderHp);
    state.units.require(2).status = statusRuntimeSnapshot(2, 100);
    seedRuntimeUnits(state, {
        runtimeUnitSnapshot(0, 0, 100, { 100, 100, 0 }),
        runtimeUnitSnapshot(1, 1, defenderHp, { 180, 180, 0 }),
        runtimeUnitSnapshot(2, 1, 100, { 72, 72, 0 }),
});
    queuePendingDamage(state, preResolvedDamageInput(0, 1, defenderHp, damage));
    state.units.requireCore(0).grid = { 10, 10 };
    state.units.requireCore(1).grid = { 5, 5 };
    state.units.requireCore(2).grid = { 3, 2 };
    state.rescue.cells = {
        rescueCell(2, 2, true, true),
        rescueCell(2, 3),
        rescueCell(3, 2),
        rescueCell(5, 5),
    };
    state.units.require(0).rescue = { 0, 0 };
    state.units.require(1).rescue = { 0, 0 };
    state.units.require(2).rescue = { 1, 0 };
    state.rescue.executeUnattendedRadius = SceneTileWidth * 3.0;
    state.rescue.counterAttack = makeHadesBattleRuntimeRules(SceneTileWidth, 64).rescueCounterAttack;
    return state;
}

}  // namespace KysChess::Battle::Test
