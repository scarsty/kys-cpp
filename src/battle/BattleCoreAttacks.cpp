#include "BattleCoreDetail.h"

#include "BattleMath.h"
#include "BattleLimits.h"
#include "../ChessEftIds.h"
#include "../Find.h"
#include "BattleAreaEffectSystem.h"
#include "BattleEffectAttackCastSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleLogSegments.h"
#include "BattleProjectileEvents.h"
#include "BattleRuntimeEffects.h"
#include "BattleStatusSystem.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <memory_resource>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>



namespace KysChess::Battle
{

namespace
{

struct RuntimeMainHitPolicies
{
    std::vector<BattleKnockbackProcDescriptor> knockbackProcs;
    std::vector<BattleNearbyTrackingProcDescriptor> nearbyTrackingProcs;
};

RuntimeMainHitPolicies runtimeMainHitPolicies(
    std::span<const EffectExactRuntimeRuleMatch> matches)
{
    RuntimeMainHitPolicies result;
    for (const auto& match : matches)
    {
        assert(match.bound);
        const auto& bound = *match.bound;
        for (const auto& effectAction : bound.rule().actions)
        {
            if (const auto* movement = std::get_if<ForceMoveAction>(&effectAction.value);
                movement && movement->distancePixels > 0)
            {
                result.knockbackProcs.push_back({
                    .chancePct = bound.rule().chancePct,
                    .action = *movement,
                });
                continue;
            }
            const auto* attack = std::get_if<ModifyAttackAction>(&effectAction.value);
            if (!attack)
            {
                continue;
            }
            if (const auto* nearby = std::get_if<NearbyTrackingAttackBehavior>(
                    &attack->runtimeBehavior))
            {
                result.nearbyTrackingProcs.push_back({
                    .rule = { bound.binding, bound.rule().id },
                    .chancePct = bound.rule().chancePct,
                    .behavior = *nearby,
                });
            }
        }
    }
    return result;
}

BattleHitUnitSnapshot makeHitUnitSnapshot(const BattleRuntimeUnit& unit)
{
    BattleHitUnitSnapshot snapshot;
    snapshot.id = unit.id;
    snapshot.team = unit.team;
    snapshot.alive = unit.alive;
    snapshot.vitals = unit.vitals;
    snapshot.stats = unit.stats;
    snapshot.motion = unit.motion;
    snapshot.animation = unit.animation;
    snapshot.invincible = unit.invincible;
    snapshot.haveAction = unit.haveAction;
    snapshot.operationType = unit.operationType;
    return snapshot;
}

int actPropertyForMagicType(const BattleRuntimeUnit& unit, int magicType)
{
    auto it = unit.actPropertiesByMagicType.find(magicType);
    return it != unit.actPropertiesByMagicType.end() ? it->second : 0;
}

BattleHitSkillSnapshot makeHitSkillSnapshot(
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker,
    const BattleRuntimeUnit& defender,
    BattleAttackPotencySnapshot potency,
    int resolvedBaseDamage)
{
    BattleHitSkillSnapshot skill;
    skill.id = event.skillId;
    skill.name = event.skillName;
    skill.hurtType = event.skillHurtType;
    skill.magicType = event.skillMagicType;
    skill.effectId = event.skillEffectId;
    const bool reflectedReturn = event.reflectionLineage
        == BattleAttackReflectionLineageKind::ReflectedReturn;
    skill.attackerActProperty = !reflectedReturn && event.skillAttackerActProperty != 0
        ? event.skillAttackerActProperty
        : actPropertyForMagicType(attacker, event.skillMagicType);
    skill.defenderActProperty = actPropertyForMagicType(defender, event.skillMagicType);
    skill.magicPower = event.skillMagicPower;
    skill.resolvedBaseDamage = resolvedBaseDamage;
    skill.potency = potency;
    return skill;
}

int sharedBleedMaxStacks(const BattleAttackEvent& event)
{
    return std::max(1, event.scriptedBleedStacks);
}

BattleAttackPotencySnapshot snapshotHitAttackPotency(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    const BattleRuntimeUnit& attacker)
{
    if (event.potencySnapshot)
    {
        return *event.potencySnapshot;
    }
    return BattleDamageSystem().snapshotAttackPotency(
        effectAdjustedAttribute(
            state,
            attacker.id,
            BattleAttribute::Attack,
            attacker.stats.attack),
        event.skillMagicPower);
}

int resolveHitAttackPotencyAgainstDefender(
    BattleRuntimeState& state,
    const BattleAttackPotencySnapshot& potency,
    const BattleRuntimeUnit& defender,
    int ignoreDefensePct = 0)
{
    assert(ignoreDefensePct >= 0);
    BattleFixed defence = effectAdjustedAttribute(
        state,
        defender.id,
        BattleAttribute::Defence,
        defender.stats.defence);
    defence = defence.scaled(
        100 - std::min(ignoreDefensePct, 100),
        100);

    return BattleDamageSystem().resolveAttackPotencyAgainstDefender(
        potency,
        defence,
        state.random.symmetricInt(10));
}

int resolveProjectileCancelDamage(
    BattleRuntimeState& state,
    const BattleAttackInstance& attack,
    const BattleAttackInstance& otherAttack,
    int currentDamage)
{
    const auto& attacker = state.units.requireCore(attack.state.attackSourceUnitId);
    int damage = currentDamage;
    if (attack.state.skillId >= 0 || attack.state.potencySnapshot)
    {
        const auto& defender = state.units.requireCore(otherAttack.state.attackSourceUnitId);
        BattleAttackEvent event;
        event.sourceUnitId = attack.state.attackSourceUnitId;
        event.skillId = attack.state.skillId;
        event.skillMagicPower = attack.state.skillMagicPower;
        event.potencySnapshot = attack.state.potencySnapshot;
        const auto potency = snapshotHitAttackPotency(state, event, attacker);
        damage = scaleProjectileCancelDamage(
            resolveHitAttackPotencyAgainstDefender(
                state,
                potency,
                defender),
            attack.state.operationType);
    }

    return BattleFixed::fromInteger(damage)
        .scaled(attack.state.projectilePressurePct, 100)
        .toInt();
}

BattleLogEvent dodgeStatusEvent(int defenderUnitId, int attackerUnitId)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = defenderUnitId;
    event.targetUnitId = attackerUnitId;
    event.perspective = BattleLogPerspective::SourceOnly;
    event.segments = battleLogText("閃避了來襲攻擊", BattleLogTextTone::SkillName);
    return event;
}

void applyProjectileCancelDamageResults(
    BattleRuntimeState& state,
    std::pmr::vector<BattleAttackEvent>& events)
{
    for (auto& event : events)
    {
        if (event.type != BattleAttackEventType::ProjectileCancel)
        {
            continue;
        }

        assert(event.attackId >= 0);
        assert(event.otherAttackId >= 0);
        const auto& attack = requireById(state.attacks.attacks, event.attackId);
        const auto& otherAttack = requireById(state.attacks.attacks, event.otherAttackId);

        event.projectileCancelDamage = resolveProjectileCancelDamage(
            state,
            attack,
            otherAttack,
            event.projectileCancelDamage);
        event.otherProjectileCancelDamage = resolveProjectileCancelDamage(
            state,
            otherAttack,
            attack,
            event.otherProjectileCancelDamage);

        state.attacks.applyProjectileCancelDamage(event);
    }
}

BattleHitResolutionInput makeHitResolutionInput(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    int ignoreDefensePct,
    const RuntimeMainHitPolicies& runtimePolicies)
{
    const auto& attacker = state.units.require(event.sourceUnitId);
    const auto& defender = state.units.require(event.unitId);

    BattleHitResolutionInput input;
    input.attackEvent = event;
    input.attacker = makeHitUnitSnapshot(attacker.core);
    input.defender = makeHitUnitSnapshot(defender.core);
    input.attacker.stats.attack = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::Attack,
        input.attacker.stats.attack);
    input.attacker.stats.speed = effectAndAreaAdjustedSpeed(
        state,
        attacker.id(),
        input.attacker.stats.speed);
    input.defender.stats.defence = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::Defence,
        input.defender.stats.defence);
    input.defender.stats.speed = effectAndAreaAdjustedSpeed(
        state,
        defender.id(),
        input.defender.stats.speed);
    input.attackerCriticalChancePct = effectAndAreaAdjustedRateAttribute(
        state,
        attacker.id(),
        BattleAttribute::CriticalChance,
        0);
    input.attackerCriticalMultiplierPct = effectAndAreaAdjustedRateAttribute(
        state,
        attacker.id(),
        BattleAttribute::CriticalDamage,
        150);
    input.defenderProjectileReflectChancePct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::ProjectileReflectChance,
        0,
        attacker.id());
    input.defenderSkillReflectPercent = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::SkillReflectPercent,
        0,
        attacker.id());
    input.attackerCooldownExtensionChancePct = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::OutgoingCooldownExtensionChance,
        0,
        defender.id());
    input.attackerCooldownExtensionPct = effectAdjustedAttribute(
        state,
        attacker.id(),
        BattleAttribute::OutgoingCooldownExtensionPercent,
        0,
        defender.id());
    input.defenderCooldownExtensionChancePct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::IncomingCooldownExtensionChance,
        0,
        attacker.id());
    input.defenderCooldownExtensionPct = effectAdjustedAttribute(
        state,
        defender.id(),
        BattleAttribute::IncomingCooldownExtensionPercent,
        0,
        attacker.id());
    input.sharedBleedMaxStacks = sharedBleedMaxStacks(event);
    input.randomDamageVariance = state.random.symmetricInt(10);
    input.knockbackProcs = runtimePolicies.knockbackProcs;
    input.nearbyTrackingProcs = runtimePolicies.nearbyTrackingProcs;

    if (event.skillId >= 0)
    {
        const auto potency = snapshotHitAttackPotency(state, event, attacker.core);
        input.skill = makeHitSkillSnapshot(
            event,
            attacker.core,
            defender.core,
            potency,
            resolveHitAttackPotencyAgainstDefender(
                state,
                potency,
                defender.core,
                ignoreDefensePct));
    }
    else if (event.potencySnapshot)
    {
        input.skill.resolvedBaseDamage = resolveHitAttackPotencyAgainstDefender(
            state, *event.potencySnapshot, defender.core, ignoreDefensePct);
        input.skill.potency = *event.potencySnapshot;
    }
    return input;
}

bool tryResolveDodgeHit(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::vector<BattleLogEvent>& logEvents,
    std::vector<BattleVisualEvent>& visualEvents)
{
    const double roll = state.random.nextPercent();
    const int dodgeChancePct = std::clamp(
        battleSaturatedAdd(effectAdjustedAttribute(
            state,
            event.unitId,
            BattleAttribute::DodgeChance,
            0,
            event.sourceUnitId),
            areaAttributeDelta(state, event.unitId, BattleAttribute::DodgeChance)),
        0,
        kBattleDodgeChanceCapPct);
    if (dodgeChancePct <= 0 || roll >= dodgeChancePct)
    {
        return false;
    }

    auto& defender = state.units.require(event.unitId);
    const int criticalAfterDodge = effectAdjustedAttribute(
        state,
        event.unitId,
        BattleAttribute::CriticalAfterDodge,
        0,
        event.sourceUnitId);
    if (criticalAfterDodge > 0)
    {
        BattleStatusApplyRequest request;
        request.kind = BattleStatusKind::NextAttackCritical;
        request.sourceUnitId = event.unitId;
        request.stack = EffectStackPolicy::Refresh;
        request.bypassStatusShield = true;
        auto statusConfig = state.status.config;
        statusConfig.frame = state.movement.frame;
        auto applied = BattleStatusSystem(statusConfig).apply(
            defender.statusDamageState(),
            request);
        assert(applied.applied);
        defender.writeStatusDamageResult(applied.target);
    }

    logEvents.push_back(dodgeStatusEvent(event.unitId, event.sourceUnitId));
    visualEvents.push_back(CoreDetail::roleEffectEvent(event.unitId, KysChess::EFT_EVADE, CoreDetail::CoreRoleStatusEffectFrames));
    return true;
}

bool consumeNextAttackCritical(BattleRuntimeState& state, BattleFrameContext& frame, int attackerUnitId)
{
    auto& attacker = state.units.require(attackerUnitId);
    if (!attacker.status.effects.has(BattleStatusKind::NextAttackCritical))
    {
        return false;
    }

    auto consumed = BattleStatusSystem({}).consume(
        attacker.statusDamageState(),
        { .kind = BattleStatusKind::NextAttackCritical });
    assert(consumed.consumed);
    attacker.writeStatusDamageResult(consumed.target);
    CoreDetail::appendStatusConsumptionLog(state, frame.logEvents, consumed, state.movement.frame);
    return true;
}

void applyAttackSpawnAttackerDualWieldBlockGain(
    BattleRuntimeState& state,
    const BattleAttackSpawnRequest& request,
    std::vector<BattleLogEvent>& logEvents)
{
    if (request.attackerDualWieldBlockGainChancePct <= 0)
    {
        return;
    }
    assert(request.initial.attackSourceUnitId >= 0);
    assert(!request.initial.skillName.empty());
    assert(request.attackerDualWieldBlockGainChancePct <= 100);

    auto& attacker = state.units.require(request.initial.attackSourceUnitId);
    if (!attacker.core.alive)
    {
        return;
    }
    assert(attacker.damage.dualWieldBlocksRemaining >= 0);
    assert(attacker.damage.dualWieldBlocksRemaining <= DualWieldBlockMaxStacks);
    if (attacker.damage.dualWieldBlocksRemaining == DualWieldBlockMaxStacks)
    {
        return;
    }
    if (!state.random.chance(request.attackerDualWieldBlockGainChancePct))
    {
        return;
    }

    attacker.damage.dualWieldBlocksRemaining += 1;
    CoreDetail::appendStatusEventLog(
        logEvents,
        attacker.core.id,
        attacker.core.id,
        std::format("{}·互搏抵擋+1", request.initial.skillName));
}

void applyAreaAttackSpawnModifiers(
    BattleRuntimeState& state,
    BattleAttackSpawnRequest& request)
{
    if (request.initial.operationType != BattleOperationType::RangedProjectile)
    {
        return;
    }

    const auto modifiers = BattleAreaEffectSystem::collectAreaAttackSpawnModifiers(
        state.areas,
        state.gridTransform,
        state.units,
        request.initial.attackSourceUnitId,
        state.movement.frame);
    if (modifiers.tracking)
    {
        request.initial.track = *modifiers.tracking;
    }
    if (modifiers.speedPct)
    {
        const int scalePct = 100 + *modifiers.speedPct;
        assert(scalePct > 0);
        request.initial.velocity *= static_cast<float>(scalePct) / 100.0f;
        request.initial.totalFrame = std::max(
            1,
            (request.initial.totalFrame * 100 + scalePct - 1) / scalePct);
    }
    if (modifiers.projectilePressurePct)
    {
        const int scalePct = 100 + *modifiers.projectilePressurePct;
        assert(scalePct >= 0);
        request.initial.projectilePressurePct =
            request.initial.projectilePressurePct * scalePct / 100;
    }
}

bool consumeTypedAttackSuppression(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event,
    std::span<const EffectCommand> commands)
{
    assert(event.provenance.valid());
    if (state.attacks.contactsSuppressed(event.attackId))
    {
        return true;
    }

    const auto liveContribution = [&](const EffectCommand& command)
        -> BattleStatusContribution*
    {
        assert(command.metadata.statusContribution);
        const auto& context = *command.metadata.statusContribution;
        auto& holder = state.units.require(context.holderUnitId);
        auto live = std::ranges::find(
            holder.status.effects.statuses,
            context.appliedSequence,
            &BattleStatusContribution::appliedSequence);
        if (live == holder.status.effects.statuses.end()
            || live->kind != context.kind
            || live->stacks <= 0
            || live->stacks != context.quantity)
            return nullptr;
        return &*live;
    };

    const auto consume = [&](const EffectCommand& command,
        const CoreDetail::StatusConsumptionLogOverride& logOverride = {})
    {
        assert(command.metadata.statusContribution);
        const auto& context = *command.metadata.statusContribution;
        auto& holder = state.units.require(context.holderUnitId);
        auto consumed = BattleStatusSystem({}).consume(
            holder.statusDamageState(),
            {
                .kind = context.kind,
                .stacks = 1,
                .filter = {
                    .holderUnitId = context.holderUnitId,
                    .appliedSequence = context.appliedSequence,
                },
            });
        assert(consumed.consumed);
        holder.writeStatusDamageResult(consumed.target);
        CoreDetail::appendStatusConsumptionLog(
            state, frame.logEvents, consumed, state.movement.frame, logOverride);
    };

    bool suppressed{};
    for (const auto& command : commands)
    {
        const auto* suppress = std::get_if<SuppressCurrentCastContactsEffectCommand>(
            &command.value);
        if (!suppress || !liveContribution(command)) continue;

        if (!suppressed)
        {
            state.attacks.suppressContactsForCast(event.provenance.cast.castId);
            suppressed = true;
        }
        if (suppress->originalTargetShield > 0)
        {
            int shieldTargetUnitId = OptionalPreferredTargetUnitId;
            const auto cast = state.effectIntegration.casts.find(
                event.provenance.cast.castId);
            if (cast != state.effectIntegration.casts.end())
                shieldTargetUnitId = cast->second.originalTargetUnitId;
            if (shieldTargetUnitId < 0)
                shieldTargetUnitId = event.preferredTargetUnitId;
            if (shieldTargetUnitId < 0)
                shieldTargetUnitId = event.unitId;

            ChangeResourceAction grantShield;
            grantShield.resource = BattleResource::Shield;
            grantShield.kind = ResourceChangeKind::Grant;
            grantShield.amount.flat = suppress->originalTargetShield;
            auto grantShieldMetadata = command.metadata;
            grantShieldMetadata.targetUnitId = shieldTargetUnitId;
            const EffectCommand grantShieldCommand{
                std::move(grantShieldMetadata),
                prepareChangeResource(std::move(grantShield),
                    suppress->originalTargetShield),
                command.execution,
            };
            CoreDetail::reduceEffectCommand(
                state,
                frame,
                frame.currentFrameDamage(),
                grantShieldCommand);
        }
        // 刺目攔下的是持有者自己的出招，紀錄以持有者視角呈現才讀得懂。
        consume(command, CoreDetail::StatusConsumptionLogOverride{
            .sourceUnitId = command.metadata.statusContribution->holderUnitId,
            .targetUnitId = event.unitId,
            .actionPrefix = "出招落空，",
        });
    }
    if (suppressed) return true;

    for (const auto& command : commands)
    {
        if (!std::holds_alternative<MakeIncomingAttackMissEffectCommand>(
                command.value)
            || !liveContribution(command))
            continue;
        consume(command);
        return true;
    }
    return false;
}

int currentHitIgnoreDefensePct(
    const BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::span<const BattleEffectReductionEntry> reductions)
{
    int result{};
    for (const auto& reduction : reductions)
    {
        const auto* routed = std::get_if<
            BattleRoutedEffectCommand<ModifyDamageEffectCommand>>(&reduction.value);
        const auto* damage = routed ? &routed->command : nullptr;
        if (!damage
            || damage->durationFrames != 0
            || damage->stack != EffectStackPolicy::Independent
            || damage->perspective != DamageModifierPerspective::Outgoing
            || damage->stage != DamageModifierStage::BeforeDefense
            || (damage->channel != DamageChannel::All
                && damage->channel != CoreDetail::effectDamageChannel(event.damageKind))
            || damage->operation != DamageModifierOperation::IgnoreDefensePercent)
        {
            continue;
        }
        result = battleSaturatedAdd(result, damage->amount);
    }
    for (const auto& modifier : BattleEffectCommandSystem::queryDamageModifiers(
             state,
             {
                 .unitId = event.sourceUnitId,
                 .eventSourceUnitId = event.unitId,
                 .perspective = DamageModifierPerspective::Outgoing,
                 .channel = CoreDetail::effectDamageChannel(event.damageKind),
                 .stage = DamageModifierStage::BeforeDefense,
                 .frame = state.movement.frame,
             }))
    {
        if (modifier.operation == DamageModifierOperation::IgnoreDefensePercent)
        {
            result = battleSaturatedAdd(
                result,
                battleSaturatedMultiply(modifier.amount, modifier.stackCount));
        }
    }
    return std::clamp(result, 0, 100);
}

void appendHitDamageModifier(
    BattleHitDamageModifierPhases& phases,
    DamageModifierPerspective perspective,
    DamageModifierStage stage,
    BattleHitDamageModifier modifier)
{
    if (perspective == DamageModifierPerspective::Outgoing)
    {
        switch (stage)
        {
        case DamageModifierStage::BeforeDefense:
            phases.outgoingBeforeCritical.push_back(modifier);
            return;
        case DamageModifierStage::AfterDefense:
            phases.outgoingAfterCritical.push_back(modifier);
            return;
        case DamageModifierStage::Final:
            phases.outgoingFinal.push_back(modifier);
            return;
        }
    }

    switch (stage)
    {
    case DamageModifierStage::BeforeDefense:
        phases.incomingBase.push_back(modifier);
        return;
    case DamageModifierStage::AfterDefense:
        phases.incomingAfterBase.push_back(modifier);
        return;
    case DamageModifierStage::Final:
        phases.incomingFinal.push_back(modifier);
        return;
    }
    assert(false);
}

void collectHitDamageModifiers(
    const BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event,
    std::span<const BattleEffectReductionEntry> reductions,
    BattleHitResolutionInput& input)
{
    const DamageChannel channel = CoreDetail::effectDamageChannel(event.damageKind);
    for (const auto& reduction : reductions)
    {
        const auto* routed = std::get_if<
            BattleRoutedEffectCommand<ModifyDamageEffectCommand>>(&reduction.value);
        const auto* modifier = routed ? &routed->command : nullptr;
        if (!modifier
            || modifier->durationFrames != 0
            || modifier->stack != EffectStackPolicy::Independent
            || (modifier->channel != DamageChannel::All
                && modifier->channel != channel))
        {
            continue;
        }
        appendHitDamageModifier(
            input.damageModifiers,
            modifier->perspective,
            modifier->stage,
            { modifier->operation, modifier->amount });
        CoreDetail::appendHitDamageModifierLog(state, frame.logEvents,
            reduction.metadata, *modifier, state.movement.frame);
    }

    const std::array perspectives{
        DamageModifierPerspective::Outgoing,
        DamageModifierPerspective::Incoming,
    };
    const std::array stages{
        DamageModifierStage::BeforeDefense,
        DamageModifierStage::AfterDefense,
        DamageModifierStage::Final,
    };
    for (DamageModifierPerspective perspective : perspectives)
    {
        for (DamageModifierStage stage : stages)
        {
            const int unitId = perspective == DamageModifierPerspective::Outgoing
                ? event.sourceUnitId
                : event.unitId;
            const int eventSourceUnitId = perspective == DamageModifierPerspective::Outgoing
                ? event.unitId
                : event.sourceUnitId;
            for (const auto& modifier : BattleEffectCommandSystem::queryDamageModifiers(
                     state,
                     {
                         .unitId = unitId,
                         .eventSourceUnitId = eventSourceUnitId,
                         .perspective = perspective,
                         .channel = channel,
                         .stage = stage,
                         .frame = state.movement.frame,
                     }))
            {
                appendHitDamageModifier(
                    input.damageModifiers,
                    perspective,
                    stage,
                    { modifier.operation, modifier.amount, modifier.stackCount });
            }
        }
    }

    const auto attackerStatus = BattleStatusSystem({}).persistentModifiers(
        state.units.require(event.sourceUnitId).status.effects);
    if (event.skillId >= 0 && attackerStatus.skillDamagePct != 0)
    {
        input.damageModifiers.outgoingBeforeCritical.push_back({
            DamageModifierOperation::PercentAdd,
            attackerStatus.skillDamagePct,
        });
    }

    const auto defenderStatus = BattleStatusSystem({}).persistentModifiers(
        state.units.require(event.unitId).status.effects);
    const int damageReductionPct = battleSaturatedAdd(
        effectAdjustedAttribute(state, event.unitId, BattleAttribute::DamageReduction,
            0, event.sourceUnitId),
        defenderStatus.damageReductionPct);
    if (damageReductionPct > 0)
    {
        input.damageModifiers.incomingBase.push_back({
            DamageModifierOperation::PercentAdd,
            -damageReductionPct,
        });
    }
    if (defenderStatus.damageTakenPct != 0)
    {
        input.damageModifiers.incomingFinal.push_back({
            DamageModifierOperation::PercentAdd,
            defenderStatus.damageTakenPct,
        });
    }
}

BattleEffectOwnedEvent makeHitEffectEvent(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    EffectEvent effectEvent)
{
    assert(effectEvent == EffectEvent::MainProjectileBeforeDamage
        || effectEvent == EffectEvent::HitBeforeDamage);
    assert(event.provenance.valid());
    HitEventData payload;
    payload.provenance = event.provenance;
    payload.targetUnitId = event.unitId;
    payload.originalTargetUnitId = event.unitId;
    const auto cast = state.effectIntegration.casts.find(
        event.provenance.cast.castId);
    if (cast != state.effectIntegration.casts.end())
    {
        payload.originalTargetUnitId = cast->second.originalTargetUnitId;
    }
    payload.contactPosition = event.position;
    payload.acceptedHit = true;
    payload.preDefenseDamage = std::max(0, event.scriptedDamage);
    payload.damageKind = event.damageKind;

    return BattleEffectEventBridge().makeEvent(
        state,
        CoreDetail::nextEffectEventHeader(state, event.sourceUnitId),
        effectEvent,
        std::move(payload));
}

struct AttackSpawnedEffectDispatch
{
    BattleEffectDispatchResult result;
    bool canExecuteInvincibleTarget{};
};

AttackSpawnedEffectDispatch dispatchAttackSpawnedEffects(
    BattleRuntimeState& state,
    const BattleAttackEvent& event)
{
    assert(event.provenance.valid());
    AttackEventData payload;
    payload.provenance = event.provenance;
    payload.originalTargetUnitId = event.unitId;
    const auto cast = state.effectIntegration.casts.find(
        event.provenance.cast.castId);
    if (cast != state.effectIntegration.casts.end())
    {
        payload.originalTargetUnitId = cast->second.originalTargetUnitId;
    }
    payload.spawnPosition = event.position;
    payload.velocity = event.velocity;
    payload.skillId = event.skillId;
    payload.baseDamage = std::max(0, event.scriptedDamage);
    payload.damageKind = event.damageKind;
    auto owned = BattleEffectEventBridge().makeEvent(
        state,
        CoreDetail::nextEffectEventHeader(state, event.sourceUnitId),
        EffectEvent::AttackSpawned,
        std::move(payload));
    AttackSpawnedEffectDispatch result;
    result.canExecuteInvincibleTarget = BattleEffectSystem().hasInvincibilityPiercingExecuteRule(
        state.effectRules,
        owned.context());
    result.result = BattleEffectEventBridge().dispatch(state, owned);
    return result;
}

std::optional<BattleCastInput> makeEffectAttackCastInput(
    BattleRuntimeState& state,
    const BattleAttackEvent& event,
    std::pmr::memory_resource* frameMemoryResource)
{
    assert(event.provenance.valid());
    const auto cast = state.effectIntegration.casts.find(event.provenance.cast.castId);
    if (cast == state.effectIntegration.casts.end())
    {
        return std::nullopt;
    }

    BattlePendingCastAction pending;
    pending.targetUnitId = cast->second.originalTargetUnitId;
    pending.operationType = event.operationType;
    pending.skillPlan = CoreDetail::makePendingCastSkillPlan(cast->second.skill);
    pending.effectCast.provenance = event.provenance.cast;
    return CoreDetail::tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
}

void applyAttackSpawnedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event)
{
    if (event.type != BattleAttackEventType::AttackSpawned)
    {
        return;
    }
    auto spawned = dispatchAttackSpawnedEffects(state, event);
    const auto liveAttack = std::ranges::find(
        state.attacks.attacks,
        event.attackId,
        &BattleAttackInstance::id);
    assert(liveAttack != state.attacks.attacks.end());
    liveAttack->state.executeCanHitInvincible =
        liveAttack->state.executeCanHitInvincible
        || spawned.canExecuteInvincibleTarget;

    auto& dispatched = spawned.result;
    if (dispatched.commands.empty())
    {
        return;
    }

    auto effectCastInput = makeEffectAttackCastInput(
        state,
        event,
        frame.frameMemoryResource());
    if (effectCastInput)
    {
        BattleAttackSpawnRequest prototype(liveAttack->state);
        prototype.provenance = {
            .cast = event.provenance.cast,
            .propagation = event.provenance.propagation,
            .origin = event.provenance.origin,
            .parentAttackId = event.provenance.parentAttackId,
            .attackOrdinal = event.provenance.attackOrdinal,
            .rootAttack = event.provenance.rootAttack,
            .mainProjectile = event.provenance.mainProjectile,
            .sharedHitGroupId = event.provenance.sharedHitGroupId,
        };
        std::vector<BattleAttackSpawnRequest> requests{ std::move(prototype) };
        BattleEffectAttackApplyState effectAttackState{
            .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
            .sourceAttackProvenance = event.provenance,
        };
        auto applied = BattleEffectAttackCastSystem().applyAttackCommands(
            *effectCastInput,
            requests,
            dispatched.commands,
            effectAttackState);
        CoreDetail::applyEffectAttackDirectives(requests, applied);
        state.effectIntegration.nextSharedHitGroupId =
            effectAttackState.nextSharedHitGroupId;

        for (auto& request : requests)
        {
            if (request.provenance.valid())
            {
                // This is the already-live prototype retained by addToBaseAttack.
                assert(!request.castWork.valid());
                continue;
            }
            request.provenance.parentAttackId = request.provenance.parentAttackId
                .value_or(event.provenance.attackId);
            request.provenance.rootAttack = false;
            CoreDetail::reserveEffectAttack(state.castLifecycle, event.provenance.cast, request);
            frame.currentFrameAttacks().push_back(std::move(request));
        }
    }


    frame.queueEffectCommands(std::move(dispatched.commands));
    CoreDetail::reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
}

void appendHitSettlementEvents(
    BattleFrameContext& frame,
    BattleHitSettlementResult& settlement)
{
    frame.attackEvents.insert(
        frame.attackEvents.end(),
        std::make_move_iterator(settlement.events.begin()),
        std::make_move_iterator(settlement.events.end()));
}

void queueReflectedReturnProjectile(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& incoming,
    const BattleProjectileReflectionRequest& reflection,
    BattleProjectilePropertiesSnapshot properties)
{
    assert(incoming.provenance.valid());
    assert(reflection.incomingAttackId == incoming.attackId);
    assert(reflection.reflectorUnitId == incoming.unitId);
    assert(reflection.originalAttackerUnitId == incoming.sourceUnitId);

    auto payload = std::move(properties.payload);
    payload.attackSourceUnitId = reflection.reflectorUnitId;
    payload.preferredTargetUnitId = reflection.originalAttackerUnitId;
    payload.position = reflection.contactPosition;
    payload.reflectionLineage = BattleAttackReflectionLineageKind::ReflectedReturn;
    assert((payload.payloadClass.kind() == BattleProjectilePayloadKind::Combat)
        == reflection.potency.has_value());
    payload.potencySnapshot = reflection.potency;

    double speed = payload.velocity.norm();
    if (speed <= state.attacks.minimumVectorNorm)
    {
        speed = state.attacks.defaultProjectileSpeed;
    }
    const auto& target = state.units.requireCore(reflection.originalAttackerUnitId);
    payload.velocity = normalizedTo(
        target.motion.position - payload.position,
        speed,
        state.attacks.minimumVectorNorm);
    if (payload.velocity.norm() <= state.attacks.minimumVectorNorm)
    {
        payload.velocity = { static_cast<float>(speed), 0.0f, 0.0f };
    }

    const auto child = state.castLifecycle.beginChildCast(
        incoming.provenance.cast.castId,
        {
            .sourceUnitId = reflection.reflectorUnitId,
            .magicId = -1,
            .ultimate = false,
            .origin = CastOriginKind::Reflection,
            .propagation = CastPropagationPolicy::SourceHitRulesOnly,
        });
    const auto reserved = state.castLifecycle.reserveAttack(
        child.provenance.castId,
        {
            .parentAttackId = incoming.provenance.attackId,
            .origin = BattleAttackOriginKind::Reflection,
            .rootAttack = false,
            .mainProjectile = false,
            .propagation = CastPropagationPolicy::SourceHitRulesOnly,
        });

    BattleAttackSpawnRequest request(std::move(payload));
    request.provenance = reserved.provenance;
    request.castWork = reserved.work;
    request.initialFrame = 0;
    request.acceleration = properties.acceleration;
    request.spiralMotion = properties.spiralMotion;
    request.spiralCenter = properties.spiralCenter;
    request.spiralRadius = properties.spiralRadius;
    request.spiralRadiusGrowth = properties.spiralRadiusGrowth;
    request.spiralAngle = properties.spiralAngle;
    request.spiralAngularVelocity = properties.spiralAngularVelocity;
    state.castLifecycle.completeWork(child.commitBarrier);

    frame.queueCommand(BattleProjectileSpawnCommand{
        .request = std::move(request),
        .reason = "彈道反射回程",
    });
}

BattleHitSettlementResult settleTypedHit(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event,
    bool accepted,
    BattleHitContinuation continuation)
{
    auto settlement = state.attacks.settleHit(
        {
            .attackId = event.attackId,
            .targetUnitId = event.unitId,
            .accepted = accepted,
            .continuation = continuation,
        },
        state.units,
        state.castLifecycle);
    appendHitSettlementEvents(frame, settlement);
    return settlement;
}

void resolveTypedHitEvent(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    const BattleAttackEvent& event)
{
    if (event.type != BattleAttackEventType::Hit)
    {
        return;
    }
    assert(event.provenance.valid());
    const auto hitEvent = makeHitEffectEvent(
        state,
        event,
        EffectEvent::HitBeforeDamage);
    auto suppressors = BattleEffectEventBridge().dispatchActiveStatusBehaviors(
        state,
        hitEvent,
        StatusBehaviorDispatchFilter::OutgoingCastSuppressorsOnly);
    if (consumeTypedAttackSuppression(state, frame, event, suppressors.commands))
    {
        settleTypedHit(
            state,
            frame,
            event,
            false,
            BattleHitContinuation::Normal);
        return;
    }
    auto incomingMiss = BattleEffectEventBridge().dispatchActiveStatusBehaviors(
        state,
        hitEvent,
        StatusBehaviorDispatchFilter::IncomingAttackMissOnly);
    if (consumeTypedAttackSuppression(state, frame, event, incomingMiss.commands))
    {
        settleTypedHit(
            state,
            frame,
            event,
            false,
            BattleHitContinuation::Normal);
        return;
    }
    if (event.scriptedDamage <= 0
        && effectAdjustedAttribute(state, event.sourceUnitId, BattleAttribute::GuaranteedHit, 0, event.unitId) <= 0
        && tryResolveDodgeHit(state, event, frame.logEvents, frame.visualEvents))
    {
        settleTypedHit(
            state,
            frame,
            event,
            false,
            BattleHitContinuation::Normal);
        return;
    }
    const bool forceCritical = event.scriptedDamage <= 0
        && consumeNextAttackCritical(state, frame, event.sourceUnitId);
    state.castLifecycle.recordHit(event.provenance, event.unitId);

    constexpr std::uint64_t HitReductionReceiptId = 1;
    BattleEffectCommandReduction hitEffectReduction;
    const auto reduceDispatched = [&](BattleEffectDispatchResult dispatched)
    {


        frame.queueEffectCommands(std::move(dispatched.commands), HitReductionReceiptId);
        CoreDetail::reduceEffectCommandBatches(
            state,
            frame,
            frame.currentFrameDamage(),
            HitReductionReceiptId,
            &hitEffectReduction);
    };

    RuntimeMainHitPolicies mainHitPolicies;
    if (event.provenance.mainProjectile)
    {
        const auto owned = makeHitEffectEvent(
            state,
            event,
            EffectEvent::MainProjectileBeforeDamage);
        const auto exactMatches = BattleEffectSystem().queryExactRuntimeRules(
            state.effectRules,
            owned.context(),
            state.random);
        mainHitPolicies = runtimeMainHitPolicies(exactMatches);
        reduceDispatched(BattleEffectEventBridge().dispatch(state, owned));
    }
    auto hitDispatched = BattleEffectEventBridge().dispatch(state, hitEvent);
    reduceDispatched(std::move(hitDispatched));

    const int ignoreDefensePct = currentHitIgnoreDefensePct(
        state,
        event,
        hitEffectReduction.entries);
    auto input = makeHitResolutionInput(
        state,
        event,
        ignoreDefensePct,
        mainHitPolicies);
    input.forceCritical = forceCritical;
    collectHitDamageModifiers(state, frame, event, hitEffectReduction.entries, input);
    auto result = BattleHitResolver().resolve(input, state.random);
    const auto continuation = result.reflection
        ? BattleHitContinuation::Reflected
        : BattleHitContinuation::Normal;
    auto settlement = settleTypedHit(
        state,
        frame,
        event,
        true,
        continuation);
    if (result.reflection)
    {
        assert(settlement.reflectedProjectile);
        queueReflectedReturnProjectile(
            state,
            frame,
            event,
            *result.reflection,
            std::move(*settlement.reflectedProjectile));
    }
    for (const auto& activated : result.activatedRuntimeRules)
    {
        state.effectRules.recordRuntimeRuleActivation(
            activated.binding,
            activated.ruleId,
            state.movement.frame);
    }
    auto followUps = expandBattleProjectileFollowUpCommands(
        result.commands,
        state.projectileFollowUps,
        state.units);
    CoreDetail::reserveTrackedProjectileFollowUps(state, followUps);
    result.commands = std::move(followUps.commands);
    result.visualEvents.insert(
        result.visualEvents.end(),
        followUps.visualEvents.begin(),
        followUps.visualEvents.end());
    frame.mutableCommandsForReducer().insert(
        frame.mutableCommandsForReducer().end(),
        result.commands.begin(),
        result.commands.end());
    frame.logEvents.insert(
        frame.logEvents.end(),
        result.logEvents.begin(),
        result.logEvents.end());
    frame.visualEvents.insert(
        frame.visualEvents.end(),
        result.visualEvents.begin(),
        result.visualEvents.end());
}

}  // namespace

namespace CoreDetail
{

void advanceAttacksAndResolveHits(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    auto& attackEvents = frame.attackEvents;
    auto& logEvents = frame.logEvents;

    state.attacks.frame = state.movement.frame;
    // AttackSpawned rules can add zero-delay descendants. Drain until stable so
    // every attack created this frame joins the same tick; explicit delays still queue.
    while (true)
    {
        auto attackSpawns = frame.drainCurrentFrameAttacks();
        if (attackSpawns.empty())
        {
            break;
        }
        attackEvents.reserve(
            attackEvents.size()
            + attackSpawns.size()
            + state.attacks.attacks.size() * 2);
        for (auto& request : attackSpawns)
        {
            if (!attackSpawnDelayElapsed(request))
            {
                state.nextFrame.queueAttack(std::move(request));
                continue;
            }
            applyAreaAttackSpawnModifiers(state, request);
            applyAttackSpawnAttackerDualWieldBlockGain(state, request, logEvents);
            assert(request.provenance.valid());
            assert(request.castWork.valid());
            attackEvents.push_back(state.attacks.spawn(
                std::move(request),
                state.castLifecycle));
            applyAttackSpawnedEffects(state, frame, attackEvents.back());
        }
        state.nextFrame.recycleAttacks(std::move(attackSpawns));
    }
    state.attacks.tick(state.units, state.castLifecycle, attackEvents);
    for (std::size_t i = 0; i < attackEvents.size(); ++i)
    {
        const auto event = attackEvents[i];
        resolveTypedHitEvent(state, frame, event);
    }
    state.attacks.appendProjectileCancelEvents(state.units, attackEvents);
    applyProjectileCancelDamageResults(state, attackEvents);
    appendProjectileCancellationLogEvents(state.attacks, attackEvents, logEvents, false);
    reduceCommandsAfterAttackHits(state, frame);
}

}  // namespace CoreDetail

}  // namespace KysChess::Battle
