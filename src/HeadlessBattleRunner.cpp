#include "HeadlessBattleRunner.h"
#include "ChessGameContent.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>

namespace KysChess
{

struct HeadlessBattleStatusContributionDigest
{
    BattleStatusKind kind{};
    std::optional<Battle::StatusProducerKey> producer;
    std::optional<Battle::StatusProducerFamilyKey> producerFamily;
    std::optional<int> familyLocalLimit;
    std::optional<ChessSha256> behaviorFingerprint;
    std::vector<EffectRuleRuntimeState> behaviorRuntime;
    int sourceUnitId = -1;
    int remainingFrames{};
    int maximumFrames{};
    int stacks = 1;
    std::optional<Battle::BattleStatusEffectOrigin> origin;
    std::uint64_t appliedSequence{};
};

struct HeadlessBattleStatusDigest
{
    int unitId = -1;
    int freezeReductionPct{};
    int shieldFreezeResPct{};
    int controlImmunityFrames{};
    int statusShield{};
    int staggerShield{};
    std::uint64_t nextStatusSequence = 1;
    std::vector<HeadlessBattleStatusContributionDigest> statuses;
};

struct HeadlessBattleDigestUnit
{
    int id{};
    int realRoleId{};
    int team{};
    bool alive{};
    Battle::BattleUnitVitals vitals;
    int shield{};
    int invincible{};
    Battle::BattleUnitStats stats;
    int star{};
    int chessInstanceId{};
};

struct HeadlessBattleAreaDigest
{
    int id = -1;
    EffectSourceBinding source;
    int sourceTeam{};
    Battle::BattleAreaGeometry geometry;
    Battle::BattleAreaAnchorKind anchorKind{};
    float anchorX{};
    float anchorY{};
    float anchorZ{};
    int anchorSourceUnitId = -1;
    int createdFrame{};
    int expiresFrameExclusive{};
    AreaSourceDeathPolicy sourceDeath{};
    Battle::BattleAreaMergeKey mergeKey;
    AreaMergePolicy merge{};
    std::vector<AreaModifier> modifiers;
};

struct HeadlessBattleAreaStateDigest
{
    int nextAreaId{};
    std::vector<HeadlessBattleAreaDigest> areas;
};

struct HeadlessBattleCastProvenanceDigest
{
    std::uint64_t rootCastId{};
    std::uint64_t castId{};
    std::optional<std::uint64_t> parentCastId;
    int sourceUnitId = -1;
    int magicId = -1;
    bool ultimate = false;
    Battle::CastOriginKind origin = Battle::CastOriginKind::Normal;
    CastPropagationPolicy defaultPropagation = CastPropagationPolicy::SourceRules;
};

struct HeadlessBattleWorkTokenDigest
{
    std::uint64_t workId{};
    std::uint64_t castId{};
};

struct HeadlessBattleAttackProvenanceDigest
{
    HeadlessBattleCastProvenanceDigest cast;
    CastPropagationPolicy effectivePropagation = CastPropagationPolicy::SourceRules;
    Battle::BattleAttackOriginKind origin = Battle::BattleAttackOriginKind::CastDerived;
    std::optional<std::uint64_t> attackId;
    std::optional<std::uint64_t> parentAttackId;
    int attackOrdinal{};
    bool rootAttack = false;
    bool mainProjectile = true;
    Battle::SharedHitGroupId sharedHitGroupId{};
};

struct HeadlessBattleCastAttackAggregateDigest
{
    std::uint64_t attackId{};
    int attackOrdinal{};
    std::vector<int> hitUnitIds;
    int highestActualHpDamage{};
    std::int64_t totalActualHpDamage{};
    std::optional<Battle::AttackFinishReason> finishReason;
};

struct HeadlessBattleCastAggregateDigest
{
    std::vector<int> distinctHitUnitIds;
    int highestActualHpDamage{};
    std::int64_t totalActualHpDamage{};
    std::vector<HeadlessBattleCastAttackAggregateDigest> attacks;
};

struct HeadlessBattleCastRuntimeDigest
{
    HeadlessBattleCastProvenanceDigest provenance;
    int outstandingWork{};
    HeadlessBattleCastAggregateDigest aggregate;
    bool cancelledBeforeCommit = false;
    bool continuationDispatched = false;
    bool settlementQueued = false;
    bool settledDispatched = false;
    std::optional<int> continuationFrame;
    std::optional<int> settledFrame;
    std::optional<int> cancelledFrame;
    std::optional<Battle::BattleCastTerminalReason> terminalReason;
};

struct HeadlessBattleActiveCastDigest
{
    HeadlessBattleCastRuntimeDigest runtime;
    int nextAttackOrdinal{};
    bool rootAttackReserved = false;
    std::optional<HeadlessBattleWorkTokenDigest> parentChildWork;
    bool continuationWindowOpen = false;
};

struct HeadlessBattleCastWorkDigest
{
    HeadlessBattleWorkTokenDigest token;
    Battle::CastWorkKind kind = Battle::CastWorkKind::CommitBarrier;
    std::optional<std::uint64_t> attackId;
    std::optional<int> attackOrdinal;
};

struct HeadlessBattleCastLifecycleDigest
{
    Battle::BattleCastLifecycleTerminalState terminalState
        = Battle::BattleCastLifecycleTerminalState::Running;
    std::optional<int> battleEndedFrame;
    std::uint64_t nextCastId = 1;
    std::uint64_t nextWorkId = 1;
    std::vector<HeadlessBattleActiveCastDigest> activeCasts;
    std::vector<HeadlessBattleCastRuntimeDigest> retiredCasts;
    std::vector<HeadlessBattleCastWorkDigest> work;
};

struct HeadlessBattleRandomDigest
{
    unsigned int seed = 1;
    std::uint64_t rawDrawCount{};
};

struct HeadlessBattleAttackPayloadDigest
{
    int attackSourceUnitId = -1;
    int skillId = -1;
    std::string skillName;
    int skillHurtType{};
    int skillMagicType{};
    int skillEffectId = -1;
    int skillAttackerActProperty{};
    int skillMagicPower{};
    int preferredTargetUnitId = Battle::OptionalPreferredTargetUnitId;
    bool requirePreferredTarget = false;
    int totalFrame = 1;
    bool track = false;
    bool through = false;
    bool executeCanHitInvincible = false;
    bool ignoreProjectileCancel = false;
    int bounceRemaining{};
    int bounceRange{};
    int bounceChancePct{};
    int bounceRollPct{};
    int visualEffectId = -1;
    Battle::BattleOperationType operationType = Battle::BattleOperationType::None;
    int scriptedDamage{};
    bool scriptedDamageAppliesModifiers{};
    bool scriptedDamageTriggersDefenseEffects{};
    int scriptedStunFrames{};
    int scriptedBleedStacks{};
    int projectileCancelDamage{};
    int projectileCancelWeaken{};
    int projectilePressurePct = 100;
    int projectileClearRadiusPct{};
    Battle::BattleAttackCastSubrequestKind castSubrequestKind
        = Battle::BattleAttackCastSubrequestKind::None;
    int roleAttackEchoActType = -1;
    int strengthPct = 100;
    bool suppressNearbyTrackingProjectileProc = false;
    BattleDamageKind damageKind = BattleDamageKind::Physical;
    float positionX{};
    float positionY{};
    float positionZ{};
    float velocityX{};
    float velocityY{};
    float velocityZ{};
    Battle::BattleAttackDeliveryKind delivery{};
    Battle::BattleProjectilePayloadKind payloadKind{};
    Battle::BattleAttackReflectionLineageKind reflectionLineage{};
    std::optional<int> potencyEffectiveAttack;
    std::optional<int> potencyMagicPower;
};

struct HeadlessBattleLiveAttackDigest
{
    int runtimeAttackId = -1;
    HeadlessBattleAttackProvenanceDigest provenance;
    HeadlessBattleWorkTokenDigest work;
    HeadlessBattleAttackPayloadDigest payload;
    int frame{};
    bool noHurt = false;
    bool contactsSuppressed = false;
    std::vector<int> hitUnitIds;
    std::vector<int> invincibleBlockedUnitIds;
    float previousPositionX{};
    float previousPositionY{};
    float previousPositionZ{};
    float accelerationX{};
    float accelerationY{};
    float accelerationZ{};
    bool spiralMotion = false;
    float spiralCenterX{};
    float spiralCenterY{};
    float spiralCenterZ{};
    float spiralRadius{};
    float spiralRadiusGrowth{};
    float spiralAngle{};
    float spiralAngularVelocity{};
    std::optional<Battle::AttackFinishReason> scheduledFinishReason;
    std::optional<Battle::AttackFinishReason> finishReason;
    std::optional<int> pendingContactTargetUnitId;
};

struct HeadlessBattleSharedHitGroupDigest
{
    int sharedHitGroupId{};
    std::vector<int> targetUnitIds;
};

struct HeadlessBattleAttackStateDigest
{
    int frame{};
    double hitRadius{};
    double minimumVectorNorm{};
    int projectileGraceFrames = 5;
    int nextAttackId{};
    double bounceSpawnDistance{};
    double defaultProjectileSpeed{};
    int minimumBounceTotalFrame = 20;
    bool spendNonThroughOnHit = true;
    std::vector<HeadlessBattleLiveAttackDigest> liveAttacks;
    std::vector<HeadlessBattleSharedHitGroupDigest> sharedHitGroups;
};

struct HeadlessBattleQueuedAttackDigest
{
    HeadlessBattleAttackProvenanceDigest provenance;
    HeadlessBattleWorkTokenDigest work;
    HeadlessBattleAttackPayloadDigest payload;
    int initialFrame{};
    int spawnDelayFrames{};
    int attackerDualWieldBlockGainChancePct{};
    float accelerationX{};
    float accelerationY{};
    float accelerationZ{};
    bool spiralMotion = false;
    float spiralCenterX{};
    float spiralCenterY{};
    float spiralCenterZ{};
    float spiralRadius{};
    float spiralRadiusGrowth{};
    float spiralAngle{};
    float spiralAngularVelocity{};
};

struct HeadlessBattleEffectDamageOriginDigest
{
    std::uint64_t variantIndex{};
    std::optional<HeadlessBattleCastProvenanceDigest> cast;
    std::optional<HeadlessBattleAttackProvenanceDigest> attack;
    std::optional<BattleStatusKind> status;
    int statusSourceUnitId = -1;
    int statusHolderUnitId = -1;
    int statusQuantity{};
    std::uint64_t statusContributionSequence{};
    std::uint32_t producerRuleOrder{};
    std::uint32_t producerActionOrder{};
    std::uint32_t behaviorRuleOrder{};
    std::uint32_t behaviorActionOrder{};
    std::uint64_t ruleId{};
    std::uint32_t ruleActionOrder{};
    std::optional<EffectSourceBinding> binding;
};

struct HeadlessBattleQueuedDamageDigest
{
    Battle::BattleDamageRequest request;
    int executeThresholdPct{};
    bool canTriggerDefenderBlock = false;
    HeadlessBattleAttackProvenanceDigest provenance;
    HeadlessBattleWorkTokenDigest delayedCastWork;
    HeadlessBattleEffectDamageOriginDigest effectOrigin;
    std::uint64_t effectCommandContinuationId{};
};

struct HeadlessBattleEffectDamageContinuationDigest
{
    std::uint64_t continuationId{};
    int remainingDamageTransactions{};
    std::uint64_t remainingCommandCount{};
};

struct HeadlessBattleEffectQueueDigest
{
    std::uint64_t nextEventOrdinal = 1;
    std::uint64_t nextDamageTransactionId = 1;
    std::uint64_t nextDamageContinuationId = 1;
    int nextSharedHitGroupId = 1;
    std::vector<std::uint64_t> activeCastIds;
    std::vector<std::uint64_t> queuedCommandBatchSizes;
    std::vector<HeadlessBattleEffectDamageContinuationDigest> damageContinuations;
};

struct HeadlessBattleQueueStateDigest
{
    std::vector<HeadlessBattleQueuedAttackDigest> queuedAttacks;
    std::vector<HeadlessBattleQueuedDamageDigest> queuedDamage;
    HeadlessBattleEffectQueueDigest effects;
};

namespace
{

HeadlessBattleStatusDigest statusDigest(
    int unitId,
    const Battle::BattleStatusEffectState& status)
{
    HeadlessBattleStatusDigest result;
    result.unitId = unitId;
    result.freezeReductionPct = status.freezeReductionPct;
    result.shieldFreezeResPct = status.shieldFreezeResPct;
    result.controlImmunityFrames = status.controlImmunityFrames;
    result.statusShield = status.statusShield;
    result.staggerShield = status.staggerShield;
    result.nextStatusSequence = status.nextStatusSequence;
    result.statuses.reserve(status.statuses.size());
    for (const auto& contribution : status.statuses)
    {
        result.statuses.push_back({
            .kind = contribution.kind,
            .producer = contribution.producer,
            .producerFamily = contribution.producerFamily,
            .familyLocalLimit = contribution.familyLocalLimit,
            .behaviorFingerprint = contribution.behavior
                ? std::optional{ statusBehaviorContentFingerprint(
                    *contribution.behavior) }
                : std::nullopt,
            .behaviorRuntime = contribution.behaviorRuntime,
            .sourceUnitId = contribution.sourceUnitId,
            .remainingFrames = contribution.remainingFrames,
            .maximumFrames = contribution.maximumFrames,
            .stacks = contribution.stacks,
            .origin = contribution.origin,
            .appliedSequence = contribution.appliedSequence,
        });
    }
    return result;
}

HeadlessBattleAreaStateDigest areaDigestState(
    const Battle::BattleAreaEffectState& state)
{
    HeadlessBattleAreaStateDigest result;
    result.nextAreaId = state.nextAreaId;
    result.areas.reserve(state.areas.size());
    for (const auto& area : state.areas)
    {
        result.areas.push_back({
            .id = area.id.value,
            .source = area.source,
            .sourceTeam = area.sourceTeam,
            .geometry = area.geometry,
            .anchorKind = area.anchor.kind,
            .anchorX = area.anchor.fixedPosition.x,
            .anchorY = area.anchor.fixedPosition.y,
            .anchorZ = area.anchor.fixedPosition.z,
            .anchorSourceUnitId = area.anchor.sourceUnitId,
            .createdFrame = area.createdFrame,
            .expiresFrameExclusive = area.expiresFrameExclusive,
            .sourceDeath = area.sourceDeath,
            .mergeKey = area.mergeKey,
            .merge = area.merge,
            .modifiers = area.modifiers,
        });
    }
    std::ranges::sort(result.areas, {}, &HeadlessBattleAreaDigest::id);
    return result;
}

HeadlessBattleCastProvenanceDigest castProvenanceDigest(
    const Battle::BattleCastProvenance& provenance)
{
    return {
        provenance.rootCastId.value(),
        provenance.castId.value(),
        provenance.parentCastId
            ? std::optional{ provenance.parentCastId->value() }
            : std::nullopt,
        provenance.sourceUnitId,
        provenance.magicId,
        provenance.ultimate,
        provenance.origin,
        provenance.propagation,
    };
}

HeadlessBattleWorkTokenDigest workTokenDigest(const Battle::CastWorkToken& token)
{
    return { token.id.value(), token.castId.value() };
}

HeadlessBattleAttackProvenanceDigest attackProvenanceDigest(
    const Battle::BattlePendingAttackProvenance& provenance)
{
    return {
        castProvenanceDigest(provenance.cast),
        provenance.propagation,
        provenance.origin,
        std::nullopt,
        provenance.parentAttackId
            ? std::optional{ provenance.parentAttackId->value() }
            : std::nullopt,
        provenance.attackOrdinal,
        provenance.rootAttack,
        provenance.mainProjectile,
        provenance.sharedHitGroupId,
    };
}

HeadlessBattleAttackProvenanceDigest attackProvenanceDigest(
    const Battle::BattleAttackProvenance& provenance)
{
    auto result = attackProvenanceDigest(Battle::BattlePendingAttackProvenance{
        provenance.cast,
        provenance.propagation,
        provenance.origin,
        provenance.parentAttackId,
        provenance.attackOrdinal,
        provenance.rootAttack,
        provenance.mainProjectile,
        provenance.sharedHitGroupId,
    });
    result.attackId = provenance.attackId.value();
    return result;
}

HeadlessBattleCastAggregateDigest castAggregateDigest(
    const Battle::CastAggregate& aggregate)
{
    HeadlessBattleCastAggregateDigest result;
    result.distinctHitUnitIds.assign(
        aggregate.distinctHitUnitIds.begin(),
        aggregate.distinctHitUnitIds.end());
    result.highestActualHpDamage = aggregate.highestActualHpDamage;
    result.totalActualHpDamage = aggregate.totalActualHpDamage;
    result.attacks.reserve(aggregate.attacksByOrdinal.size());
    for (const auto& [ordinal, attack] : aggregate.attacksByOrdinal)
    {
        assert(ordinal == attack.attackOrdinal);
        result.attacks.push_back({
            attack.attackId.value(),
            attack.attackOrdinal,
            std::vector<int>(
                attack.hitUnitIds.begin(),
                attack.hitUnitIds.end()),
            attack.highestActualHpDamage,
            attack.totalActualHpDamage,
            attack.finishReason,
        });
    }
    return result;
}

HeadlessBattleCastRuntimeDigest castRuntimeDigest(
    const Battle::BattleCastRuntime& runtime)
{
    return {
        castProvenanceDigest(runtime.provenance),
        runtime.outstandingWork,
        castAggregateDigest(runtime.aggregate),
        runtime.cancelledBeforeCommit,
        runtime.continuationDispatched,
        runtime.settlementQueued,
        runtime.settledDispatched,
        runtime.continuationFrame,
        runtime.settledFrame,
        runtime.cancelledFrame,
        runtime.terminalReason,
    };
}

HeadlessBattleCastLifecycleDigest castLifecycleDigest(
    const Battle::BattleCastLifecycle& lifecycle)
{
    const auto snapshot = lifecycle.snapshot();
    HeadlessBattleCastLifecycleDigest result;
    result.terminalState = snapshot.terminalState;
    result.battleEndedFrame = snapshot.battleEndedFrame;
    result.nextCastId = snapshot.nextCastId;
    result.nextWorkId = snapshot.nextWorkId;
    result.activeCasts.reserve(snapshot.activeCasts.size());
    for (const auto& cast : snapshot.activeCasts)
    {
        result.activeCasts.push_back({
            castRuntimeDigest(cast.runtime),
            cast.nextAttackOrdinal,
            cast.rootAttackReserved,
            cast.parentChildWork
                ? std::optional{ workTokenDigest(*cast.parentChildWork) }
                : std::nullopt,
            cast.continuationWindowOpen,
        });
    }
    result.retiredCasts.reserve(snapshot.retiredCasts.size());
    for (const auto& cast : snapshot.retiredCasts)
    {
        result.retiredCasts.push_back(castRuntimeDigest(cast));
    }
    result.work.reserve(snapshot.work.size());
    for (const auto& work : snapshot.work)
    {
        result.work.push_back({
            workTokenDigest(work.token),
            work.kind,
            work.attackId
                ? std::optional{ work.attackId->value() }
                : std::nullopt,
            work.attackOrdinal,
        });
    }
    return result;
}

HeadlessBattleAttackPayloadDigest attackPayloadDigest(
    const Battle::BattleAttackPayload& payload)
{
    return {
        payload.attackSourceUnitId,
        payload.skillId,
        payload.skillName,
        payload.skillHurtType,
        payload.skillMagicType,
        payload.skillEffectId,
        payload.skillAttackerActProperty,
        payload.skillMagicPower,
        payload.preferredTargetUnitId,
        payload.requirePreferredTarget,
        payload.totalFrame,
        payload.track,
        payload.through,
        payload.executeCanHitInvincible,
        payload.ignoreProjectileCancel,
        payload.bounceRemaining,
        payload.bounceRange,
        payload.bounceChancePct,
        payload.bounceRollPct,
        payload.visualEffectId,
        payload.operationType,
        payload.scriptedDamage,
        payload.scriptedDamageAppliesModifiers,
        payload.scriptedDamageTriggersDefenseEffects,
        payload.scriptedStunFrames,
        payload.scriptedBleedStacks,
        payload.projectileCancelDamage,
        payload.projectileCancelWeaken,
        payload.projectilePressurePct,
        payload.projectileClearRadiusPct,
        payload.castSubrequestKind,
        payload.roleAttackEchoActType,
        payload.strengthPct,
        payload.suppressNearbyTrackingProjectileProc,
        payload.damageKind,
        payload.position.x,
        payload.position.y,
        payload.position.z,
        payload.velocity.x,
        payload.velocity.y,
        payload.velocity.z,
        payload.delivery.kind(),
        payload.payloadClass.kind(),
        payload.reflectionLineage,
        payload.potencySnapshot
            ? std::optional{ payload.potencySnapshot->effectiveAttack }
            : std::nullopt,
        payload.potencySnapshot
            ? std::optional{ payload.potencySnapshot->magicPower }
            : std::nullopt,
    };
}

HeadlessBattleAttackStateDigest attackStateDigest(
    const Battle::BattleAttackState& state)
{
    HeadlessBattleAttackStateDigest result;
    result.frame = state.frame;
    result.hitRadius = state.hitRadius;
    result.minimumVectorNorm = state.minimumVectorNorm;
    result.projectileGraceFrames = state.projectileGraceFrames;
    result.nextAttackId = state.nextAttackId;
    result.bounceSpawnDistance = state.bounceSpawnDistance;
    result.defaultProjectileSpeed = state.defaultProjectileSpeed;
    result.minimumBounceTotalFrame = state.minimumBounceTotalFrame;
    result.spendNonThroughOnHit = state.spendNonThroughOnHit;
    result.liveAttacks.reserve(state.attacks.size());
    for (const auto& attack : state.attacks)
    {
        result.liveAttacks.push_back({
            attack.id,
            attackProvenanceDigest(attack.provenance),
            workTokenDigest(attack.castWork),
            attackPayloadDigest(attack.state),
            attack.frame,
            attack.noHurt,
            attack.contactsSuppressed,
            attack.hitUnitIds,
            attack.invincibleBlockedUnitIds,
            attack.previousPosition.x,
            attack.previousPosition.y,
            attack.previousPosition.z,
            attack.acceleration.x,
            attack.acceleration.y,
            attack.acceleration.z,
            attack.spiralMotion,
            attack.spiralCenter.x,
            attack.spiralCenter.y,
            attack.spiralCenter.z,
            attack.spiralRadius,
            attack.spiralRadiusGrowth,
            attack.spiralAngle,
            attack.spiralAngularVelocity,
            attack.scheduledFinishReason,
            attack.finishReason,
            attack.pendingContact
                ? std::optional{ attack.pendingContact->targetUnitId }
                : std::nullopt,
        });
    }
    std::ranges::sort(
        result.liveAttacks,
        {},
        &HeadlessBattleLiveAttackDigest::runtimeAttackId);
    result.sharedHitGroups.reserve(state.sharedHitGroupTargets.size());
    for (const auto& [groupId, targetIds] : state.sharedHitGroupTargets)
    {
        result.sharedHitGroups.push_back({ groupId, targetIds });
        auto& group = result.sharedHitGroups.back();
        std::ranges::sort(group.targetUnitIds);
    }
    std::ranges::sort(
        result.sharedHitGroups,
        {},
        &HeadlessBattleSharedHitGroupDigest::sharedHitGroupId);
    return result;
}

HeadlessBattleQueuedAttackDigest queuedAttackDigest(
    const Battle::BattleAttackSpawnRequest& request)
{
    return {
        attackProvenanceDigest(request.provenance),
        workTokenDigest(request.castWork),
        attackPayloadDigest(request.initial),
        request.initialFrame,
        request.spawnDelayFrames,
        request.attackerDualWieldBlockGainChancePct,
        request.acceleration.x,
        request.acceleration.y,
        request.acceleration.z,
        request.spiralMotion,
        request.spiralCenter.x,
        request.spiralCenter.y,
        request.spiralCenter.z,
        request.spiralRadius,
        request.spiralRadiusGrowth,
        request.spiralAngle,
        request.spiralAngularVelocity,
    };
}

HeadlessBattleEffectDamageOriginDigest effectDamageOriginDigest(
    const Battle::EffectDamageOrigin& origin)
{
    HeadlessBattleEffectDamageOriginDigest result;
    result.variantIndex = static_cast<std::uint64_t>(origin.index());
    if (const auto* attack = effectDamageAttackProvenance(origin))
    {
        result.attack = attackProvenanceDigest(*attack);
    }
    else if (const auto* cast = effectDamageCastProvenance(origin))
    {
        result.cast = castProvenanceDigest(*cast);
    }
    std::visit(
        [&](const auto& value)
        {
            using Value = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, Battle::EffectAttackDamageOrigin>)
            {
            }
            else if constexpr (std::is_same_v<Value, Battle::EffectStatusDamageOrigin>)
            {
                result.status = value.contribution.kind;
                result.statusSourceUnitId = value.contribution.sourceUnitId;
                result.statusHolderUnitId = value.contribution.holderUnitId;
                result.statusQuantity = value.contribution.quantity;
                result.statusContributionSequence =
                    value.contribution.appliedSequence;
                result.producerRuleOrder =
                    value.contribution.producerRuleOrder;
                result.producerActionOrder =
                    value.contribution.producerActionOrder;
                result.behaviorRuleOrder =
                    value.contribution.behaviorRuleOrder;
                result.behaviorActionOrder = value.behaviorActionOrder;
                result.ruleId = value.contribution.producerRuleId.value;
                result.binding = value.binding;
            }
            else if constexpr (std::is_same_v<Value, Battle::EffectRuleDamageOrigin>)
            {
                result.ruleId = value.ruleId.value;
                result.ruleActionOrder = value.actionOrder;
                result.binding = value.binding;
            }
        },
        origin);
    return result;
}

HeadlessBattleQueueStateDigest queueStateDigest(
    const Battle::BattleRuntimeState& state)
{
    HeadlessBattleQueueStateDigest result;
    const auto& attacks = state.nextFrame.queuedAttacks();
    result.queuedAttacks.reserve(attacks.size());
    for (const auto& attack : attacks)
    {
        result.queuedAttacks.push_back(queuedAttackDigest(attack));
    }
    const auto& damage = state.nextFrame.queuedDamage();
    result.queuedDamage.reserve(damage.size());
    for (const auto& pending : damage)
    {
        result.queuedDamage.push_back({
            pending.request,
            pending.executeThresholdPct,
            pending.canTriggerDefenderBlock,
            attackProvenanceDigest(pending.provenance),
            workTokenDigest(pending.delayedCastWork),
            effectDamageOriginDigest(pending.effectOrigin),
            pending.effectCommandContinuationId,
        });
    }

    const auto& integration = state.effectIntegration;
    result.effects.nextEventOrdinal = integration.nextEventOrdinal;
    result.effects.nextDamageTransactionId = integration.nextDamageTransactionId;
    result.effects.nextDamageContinuationId = integration.nextDamageContinuationId;
    result.effects.nextSharedHitGroupId = integration.nextSharedHitGroupId;
    result.effects.activeCastIds.reserve(integration.casts.size());
    for (const auto& [castId, context] : integration.casts)
    {
        (void)context;
        result.effects.activeCastIds.push_back(castId.value());
    }
    result.effects.queuedCommandBatchSizes.reserve(
        integration.queuedCommandBatches.size());
    for (const auto& batch : integration.queuedCommandBatches)
    {
        result.effects.queuedCommandBatchSizes.push_back(
            static_cast<std::uint64_t>(batch.commands.size()));
    }
    result.effects.damageContinuations.reserve(
        integration.damageContinuations.size());
    for (const auto& [continuationId, continuation] : integration.damageContinuations)
    {
        result.effects.damageContinuations.push_back({
            continuationId,
            continuation.remainingDamageTransactions,
            static_cast<std::uint64_t>(
                continuation.commandBatch.commands.size()),
        });
    }
    return result;
}

}  // namespace

ChessSha256 HeadlessBattleRunner::digest(const HeadlessBattleResult& result)
{
    std::vector<HeadlessBattleDigestUnit> units;
    std::vector<HeadlessBattleStatusDigest> statuses;
    units.reserve(result.finalRuntime.units.size());
    statuses.reserve(result.finalRuntime.units.size());
    for (const auto& record : result.finalRuntime.units.all())
    {
        const auto& unit = record.core;
        units.push_back({
            unit.id,
            unit.realRoleId,
            unit.team,
            unit.alive,
            unit.vitals,
            unit.shield,
            unit.invincible,
            unit.stats,
            unit.star,
            unit.chessInstanceId,
        });
        statuses.push_back(statusDigest(unit.id, record.status.effects));
    }
    std::ranges::sort(units, {}, &HeadlessBattleDigestUnit::id);
    std::ranges::sort(statuses, {}, &HeadlessBattleStatusDigest::unitId);
    const auto areas = areaDigestState(result.finalRuntime.areas);
    const auto lifecycle = castLifecycleDigest(result.finalRuntime.castLifecycle);
    const HeadlessBattleRandomDigest random{
        result.finalRuntime.random.seed(),
        result.finalRuntime.random.rawDrawCount(),
    };
    const HeadlessBattleRandomDigest talentRandom{
        result.finalRuntime.talentRandom.seed(), result.finalRuntime.talentRandom.rawDrawCount()};
    std::map<int, std::optional<Battle::BattleLethalRecovery>> lethalRecoveries;
    std::map<int, Battle::BattleStrengthening> strengthening;
    for (const auto& record : result.finalRuntime.units.all())
    {
        lethalRecoveries.emplace(record.core.id, record.damage.lethalRecovery);
        strengthening.emplace(record.core.id, record.damage.strengthening);
    }
    const auto attacks = attackStateDigest(result.finalRuntime.attacks);
    const auto queues = queueStateDigest(result.finalRuntime);
    return chessBeveSha256(
        "KYS_CHESS_BATTLE_LIFECYCLE_V1",
        talentRandom,
        lethalRecoveries,
        strengthening,
        result.digestEvents,
        result.summary.outcome,
        result.summary.endFrame,
        units,
        statuses,
        areas,
        lifecycle,
        random,
        attacks,
        queues,
        result.report.stats());
}

HeadlessBattleResult HeadlessBattleRunner::run(Battle::BattleRuntimeSessionCreationInput input)
{
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    BattleReportCollector collector;
    collector.consumeInitialization(creation.initialization, creation.session);

    HeadlessBattleResult result;
    result.initialization = creation.initialization;
    Battle::BattlePresentationFrame frame;
    while (!creation.session.runtime().result.ended)
    {
        frame = creation.session.runFrame(std::move(frame));
        collector.consumeFrame(frame, creation.session);
        auto projected = Battle::battleDigestEvents(frame);
        result.digestEvents.insert(result.digestEvents.end(), projected.begin(), projected.end());
    }
    result.report = collector.report();
    result.summary = BattleSummaryBuilder::build(creation.session, result.report);
    result.finalRuntime = creation.session.runtime();
    result.digest = HeadlessBattleRunner::digest(result);
    return result;
}

}
