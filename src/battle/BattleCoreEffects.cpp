#include "BattleCoreDetail.h"
#include "../ChessBattleEffectSemantics.h"
#include "../ChessEftIds.h"
#include "BattleAreaEffectSystem.h"
#include "BattleEffectAttackCastSystem.h"
#include "BattleEffectEventBridge.h"
#include "BattleFrameContext.h"
#include "BattleLogSegments.h"
#include "BattlePresentationVisuals.h"
#include "BattleProjectileEvents.h"
#include "BattleResourceRules.h"
#include "BattleRuntimeEffects.h"
#include "BattleStatusSystem.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <limits>
#include <map>
#include <memory_resource>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>



namespace KysChess::Battle
{

namespace
{

bool isMpBlocked(BattleRuntimeState& state, int unitId)
{
    return state.units.require(unitId).mpBlocked();
}

template<class... Visitors>
struct Overloaded : Visitors...
{
    using Visitors::operator()...;
};

struct BattleCommandSinks
{
    std::vector<BattleAttackSpawnRequest>& attackSpawns;
    std::vector<BattlePendingDamageIntent>& pendingDamage;
    std::vector<BattleGameplayEvent>& gameplayEvents;
    std::vector<BattleLogEvent>& logEvents;
    std::vector<BattleVisualEvent>& visualEvents;
};

struct EnemyTopDebuffTotals
{
    int attack{};
    int defence{};
    int sourceTeam = -1;
};

int adjustedRuntimeMpRestore(BattleRuntimeState& state, int unitId, int amount)
{
    return adjustedMpRestore(
        isMpBlocked(state, unitId),
        CoreDetail::mpRecoveryBonusPct(state, unitId),
        amount);
}

BattleVisualEvent semanticCueEvent(const BattleSemanticCueRequest& cue)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.targetUnitId = cue.targetUnitId;
    event.durationFrames = 15;
    switch (cue.family)
    {
    case BattleSemanticCueFamily::Positive:
        event.visualPath = BattleCuePositiveVisualPath;
        event.color = { 255, 204, 96, 220 };
        break;
    case BattleSemanticCueFamily::Protection:
        event.visualPath = BattleCuePositiveVisualPath;
        event.color = { 112, 224, 255, 210 };
        break;
    case BattleSemanticCueFamily::Poison:
        event.visualPath = BattleCueNegativeVisualPath;
        event.color = { 136, 220, 96, 170 };
        break;
    case BattleSemanticCueFamily::Bleed:
        event.visualPath = BattleCueBleedVisualPath;
        event.color = { 255, 94, 86, 220 };
        break;
    case BattleSemanticCueFamily::Control:
        event.visualPath = BattleCueControlVisualPath;
        event.color = { 104, 160, 255, 190 };
        break;
    case BattleSemanticCueFamily::Curse:
        event.visualPath = BattleCueNegativeVisualPath;
        event.color = { 190, 112, 255, 170 };
        break;
    case BattleSemanticCueFamily::Cleanse:
        event.visualPath = BattleCueCleanseVisualPath;
        event.color = { 184, 255, 246, 205 };
        break;
    }
    return event;
}

void appendMpResourceEffectLogEvents(
    std::vector<BattleLogEvent>& logEvents,
    const EffectCommandMetadata& metadata,
    const ChangeResourceEffectCommand& command,
    const BattleResourceEffectResult& result)
{
    if (command.action.resource != BattleResource::Mp
        || (command.action.kind != ResourceChangeKind::Drain
            && command.action.kind != ResourceChangeKind::Transfer))
    {
        return;
    }

    int removed{};
    for (const auto& delta : result.deltas)
    {
        if (delta.unitId == metadata.targetUnitId && delta.after < delta.before)
        {
            removed += delta.before - delta.after;
        }
    }
    if (removed > 0)
    {
        CoreDetail::appendStatusEventLog(
            logEvents,
            metadata.binding.ownerUnitId,
            metadata.targetUnitId,
            command.action.kind == ResourceChangeKind::Drain ? "吸取內力" : "轉移內力",
            BattleStatusSemanticId::MagicPointsDrained,
            BattleResourceSemanticId::MagicPoints,
            removed);
    }

    for (const auto& delta : result.deltas)
    {
        const int restored = delta.after - delta.before;
        if (restored <= 0)
        {
            continue;
        }
        CoreDetail::appendHealEventLog(
            logEvents,
            metadata.binding.ownerUnitId,
            delta.unitId,
            restored,
            command.action.kind == ResourceChangeKind::Drain ? "吸取內力" : "轉移內力",
            BattleResourceSemanticId::MagicPoints);
    }
}

bool applyFrameMpRestore(
    BattleRuntimeState& state,
    int unitId,
    int amount,
    const std::string& reason,
    std::vector<BattleLogEvent>& logEvents)
{
    auto& unit = state.units.requireCore(unitId);

    const int restored = std::min(amount, std::max(0, unit.vitals.maxMp - unit.vitals.mp));
    if (restored <= 0)
    {
        return true;
    }

    unit.vitals.mp += restored;
    CoreDetail::appendStatusEventLog(logEvents, unitId, unitId, reason);
    return true;
}

BattleCommandSinks currentFrameSinks(BattleFrameContext& frame)
{
    return {
        frame.currentFrameAttacks(),
        frame.currentFrameDamage(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterAttackHitSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        frame.currentFrameDamage(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

BattleCommandSinks afterDamageLifecycleSinks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    return {
        state.nextFrame.mutableAttacksForReducer(),
        state.nextFrame.mutableDamageForReducer(),
        frame.gameplayEvents,
        frame.logEvents,
        frame.visualEvents,
    };
}

bool reduceFrameGameplayCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    BattleGameplayCommand& command,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleFrameVector<BattleGameplayCommand>& pending,
    BattleCommandSinks sinks)
{
    if (const auto* hp = std::get_if<BattleHpDamageCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *hp);
    }
    if (const auto* mp = std::get_if<BattleMpDamageCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *mp);
    }
    if (const auto* sideEffect = std::get_if<BattleAcceptedHitSideEffectCommand>(&command))
    {
        return CoreDetail::tryAppendFrameDamageTransaction(state, sinks.pendingDamage, *sideEffect);
    }
    if (auto* projectile = std::get_if<BattleProjectileSpawnCommand>(&command))
    {
        assert(projectile->request.provenance.valid());
        assert(projectile->request.castWork.valid());
        sinks.attackSpawns.push_back(std::move(projectile->request));
        return true;
    }
    if (std::holds_alternative<BattleNearbyTrackingProjectilesCommand>(command))
    {
        auto followUps = expandBattleProjectileFollowUpCommands(
            std::span(&command, 1),
            state.projectileFollowUps,
            state.units);
        CoreDetail::reserveTrackedProjectileFollowUps(state, followUps);
        pending.insert(
            pending.end(),
            std::make_move_iterator(followUps.commands.begin()),
            std::make_move_iterator(followUps.commands.end()));
        sinks.visualEvents.insert(
            sinks.visualEvents.end(),
            std::make_move_iterator(followUps.visualEvents.begin()),
            std::make_move_iterator(followUps.visualEvents.end()));
        return true;
    }
    if (const auto* autoUltimate = std::get_if<BattleAutoUltimateCommand>(&command))
    {
        return CoreDetail::tryCommitAutoUltimate(
            state,
            frame,
            autoUltimate->unitId,
            autoUltimate->consumeMp,
            autoUltimate->announce,
            pending.get_allocator().resource(),
            attackSoundIds,
            sinks.attackSpawns,
            sinks.gameplayEvents,
            sinks.logEvents,
            sinks.visualEvents);
    }
    if (const auto* knockback = std::get_if<BattleKnockbackCommand>(&command))
    {
        CoreDetail::applyKnockbackImpulse(state, *knockback);
        return true;
    }
    if (const auto* rumble = std::get_if<BattleRumbleCommand>(&command))
    {
        rumbles.push_back({
            rumble->lowFrequency,
            rumble->highFrequency,
            rumble->durationMs,
        });
        return true;
    }
    assert(false);
    return false;
}

void reduceFrameGameplayCommandsImpl(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    BattleFrameVector<BattleGameplayCommand>& commands,
    std::vector<int>& attackSoundIds,
    std::vector<BattleFrameRumbleEvent>& rumbles,
    BattleCommandSinks sinks)
{
    BattleFrameVector<BattleGameplayCommand> pending = std::move(commands);
    BattleFrameVector<BattleGameplayCommand> unreduced(commands.get_allocator().resource());
    for (std::size_t i = 0; i < pending.size(); ++i)
    {
        if (!reduceFrameGameplayCommand(
            state,
            frame,
            pending[i],
            attackSoundIds,
            rumbles,
            pending,
            sinks))
        {
            unreduced.push_back(std::move(pending[i]));
        }
    }
    commands = std::move(unreduced);
}

void appendStateMachineOutput(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const BattleEffectReductionEntry& entry,
    const StateMachineEffectCommand& command,
    const BattleEffectCommandContext& context)
{
    std::visit(Overloaded{
        [&](const ChangeStateValueAction&)
        {
            // The dispatcher owns rule state so later rules in the same event
            // observe the committed value deterministically.
        },
        [&](const TransferStateValueAction&)
        {
            // The dispatcher atomically moves both state slots while emitting
            // the command, so later rules observe the transferred value.
        },
        [&](const RecordMaximumDamageAction&)
        {
            // The dispatcher owns the recorded maximum so selection and command
            // generation observe one deterministic value.
        },
        [&](const ConsumeRecordedMaximumAction& action)
        {
            if (command.outputValue <= 0)
            {
                return;
            }
            assert(command.outputValue <= std::numeric_limits<int>::max());
            const int amount = static_cast<int>(command.outputValue);
            if (action.destination == StateValueDestination::ShieldAmount)
            {
                assert(false);
                return;
            }

            BattleDamageRequest request;
            request.attackerUnitId = entry.metadata.binding.ownerUnitId;
            request.defenderUnitId = entry.metadata.targetUnitId;
            request.baseDamage = amount;
            request.damageKind = BattleDamageKind::Pure;
            const auto provenance = context.attack.value_or(
                BattleAttackProvenance{});
            CoreDetail::appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                provenance,
                EffectRuleDamageOrigin{
                    entry.metadata.ruleId,
                    entry.metadata.binding,
                },
                CoreDetail::reserveEffectDamageDescendantWork(state, context, provenance));
        },
        [&](const StartDamageAbsorptionAction&)
        {
            // Persistent absorption lifetime is consumed at the damage boundary;
            // the state slot was reset by the dispatcher.
        },
        [&](const SettleDamageAbsorptionAction& action)
        {
            if (command.outputValue <= 0)
            {
                return;
            }
            assert(command.outputValue <= std::numeric_limits<int>::max());
            for (int targetUnitId : command.selectedSourceUnitIds)
            {
                BattleDamageRequest request;
                request.attackerUnitId = entry.metadata.binding.ownerUnitId;
                request.defenderUnitId = targetUnitId;
                request.baseDamage = static_cast<int>(command.outputValue);
                request.damageKind = action.damageKind;
                CoreDetail::appendFramePendingDamage(
                    state,
                    pendingDamage,
                    std::move(request),
                    std::nullopt,
                    false,
                    false,
                    {},
                    EffectRuleDamageOrigin{
                        entry.metadata.ruleId,
                        entry.metadata.binding,
                    },
                    CoreDetail::reserveEffectDamageDescendantWork(state, context, {}));
            }
        },
        [&](const BorrowEffectRulesAction&)
        {
            // Borrowed source bindings are applied while constructing the child
            // attack; no current-frame resource mutation belongs here.
        },
        [&](const CopyAttackDefinitionAction&)
        {
            // The cast coordinator already scheduled the tracked copied child cast
            // before this command reaches the mutation reducer.
        },
        [&](const SettleRemainingStatusDamageAction& action)
        {
            assert(action.status == BattleStatusKind::Poison);
            auto& target = state.units.require(entry.metadata.targetUnitId);
            const auto* poison = target.status.effects.find(BattleStatusKind::Poison);
            int settlementDamage{};
            if (poison)
            {
                assert(poison->remainingFrames > 0);
                assert(poison->potency > 0);
                assert(state.status.config.poisonDamageIntervalFrames > 0);
                settlementDamage = projectRemainingPoisonDamage({
                    .firstFutureFrame = state.movement.frame,
                    .remainingFrames = poison->remainingFrames,
                    .remainingStacks = poison->stacks,
                    .intervalFrames = state.status.config.poisonDamageIntervalFrames,
                    .currentHp = target.core.vitals.hp,
                    .damagePct = poison->potency,
                });
            }
            if (settlementDamage <= 0)
            {
                return;
            }
            BattleDamageRequest request;
            request.attackerUnitId = entry.metadata.binding.ownerUnitId;
            request.defenderUnitId = entry.metadata.targetUnitId;
            request.baseDamage = settlementDamage;
            request.damageKind = BattleDamageKind::Poison;
            request.preResolvedDamage = true;
            request.preResolvedModifierPolicy =
                BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
            CoreDetail::appendFramePendingDamage(
                state,
                pendingDamage,
                std::move(request),
                std::nullopt,
                false,
                false,
                {},
                EffectStatusDamageOrigin{
                    action.status,
                    entry.metadata.binding.ownerUnitId,
                },
                CoreDetail::reserveEffectDamageDescendantWork(state, context, {}));
        },
        [&](const GenerateClonesAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
        [&](const PreventDeathAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
        [&](const ConfigureRescueRepositionAction&)
        {
            // Consumed by BattleStartInitializer before the runtime is built.
        },
    }, command.action);
}

bool sameEffectRuleCommandSequence(
    const EffectCommandMetadata& lhs,
    const EffectCommandMetadata& rhs)
{
    return lhs.binding.kind == rhs.binding.kind
        && lhs.binding.sourceId == rhs.binding.sourceId
        && lhs.binding.ownerUnitId == rhs.binding.ownerUnitId
        && lhs.binding.sourceTeam == rhs.binding.sourceTeam
        && lhs.binding.runtimeInstanceId == rhs.binding.runtimeInstanceId
        && lhs.ruleId == rhs.ruleId
        && lhs.event == rhs.event
        && lhs.ruleOrder == rhs.ruleOrder
        && lhs.eventSourceUnitId == rhs.eventSourceUnitId;
}

bool modifierApplicationShouldCue(
    BattleModifierApplyOutcome outcome,
    int stackCount)
{
    switch (outcome)
    {
    case BattleModifierApplyOutcome::Applied:
    case BattleModifierApplyOutcome::Replaced:
        return true;
    case BattleModifierApplyOutcome::StackChanged:
        return stackCount == 1;
    case BattleModifierApplyOutcome::Refreshed:
    case BattleModifierApplyOutcome::KeptStronger:
    case BattleModifierApplyOutcome::BlockedByStatusShield:
        return false;
    }
    assert(false);
    return false;
}

bool isProtectionAttribute(BattleAttribute attribute)
{
    switch (attribute)
    {
    case BattleAttribute::Defence:
    case BattleAttribute::DodgeChance:
    case BattleAttribute::BlockChance:
    case BattleAttribute::DamageReduction:
    case BattleAttribute::StaggerResistance:
    case BattleAttribute::ProjectileReflectChance:
    case BattleAttribute::SkillReflectPercent:
    case BattleAttribute::CounterUltimateBlockChance:
        return true;
    default:
        return false;
    }
}

BattleSemanticCueFamily statusCueFamily(BattleStatusKind status)
{
    switch (status)
    {
    case BattleStatusKind::Poison:
    case BattleStatusKind::ColdPoison:
        return BattleSemanticCueFamily::Poison;
    case BattleStatusKind::Bleed:
        return BattleSemanticCueFamily::Bleed;
    case BattleStatusKind::Stun:
    case BattleStatusKind::MpBlocked:
    case BattleStatusKind::Blinded:
        return BattleSemanticCueFamily::Control;
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
        return BattleSemanticCueFamily::Curse;
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::Shadowless:
        return BattleSemanticCueFamily::Protection;
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::NextAttackCritical:
        return BattleSemanticCueFamily::Positive;
    }
    assert(false);
    return BattleSemanticCueFamily::Curse;
}

void queueSemanticCue(
    BattleFrameContext& frame,
    const EffectCommandMetadata& metadata,
    int targetUnitId,
    BattleSemanticCueFamily family)
{
    if (metadata.event != EffectEvent::BattleInitialized)
    {
        frame.queueSemanticCue(targetUnitId, family);
    }
}

void queueStatusApplyCue(
    BattleFrameContext& frame,
    const EffectCommandMetadata& metadata,
    BattleStatusKind kind,
    int requestedStacks,
    const BattleStatusApplyResult& result)
{
    switch (result.outcome)
    {
    case BattleStatusApplyOutcome::BlockedByStatusShield:
    case BattleStatusApplyOutcome::BlockedByStaggerShield:
    case BattleStatusApplyOutcome::BlockedByControlImmunity:
        queueSemanticCue(
            frame,
            metadata,
            metadata.targetUnitId,
            BattleSemanticCueFamily::Protection);
        return;
    case BattleStatusApplyOutcome::Applied:
    case BattleStatusApplyOutcome::Replaced:
        break;
    case BattleStatusApplyOutcome::StackChanged:
        // Cue only the primary application; later stack or frame growth stays silent.
        if (!result.applied || result.value > requestedStacks)
        {
            return;
        }
        break;
    case BattleStatusApplyOutcome::Refreshed:
    case BattleStatusApplyOutcome::KeptStronger:
    case BattleStatusApplyOutcome::TargetDead:
        return;
    }
    queueSemanticCue(frame, metadata, metadata.targetUnitId, statusCueFamily(kind));
}

void reduceEffectCommand(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    const EffectCommand& command,
    const BattleEffectCommandContext& context)
{
    auto reduction = BattleEffectCommandSystem().reduce(
        state,
        command,
        context);
    assert(reduction.entries.size() == 1);
    const auto& entry = reduction.entries.front();
    if (const auto* attribute = std::get_if<BattleAttributeEffectResult>(&entry.value))
    {
        if (attribute->outcome == BattleModifierApplyOutcome::BlockedByStatusShield)
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
        else if (modifierApplicationShouldCue(
                     attribute->outcome,
                     attribute->modifier.stackCount))
        {
            const auto family = attribute->modifier.negative
                ? BattleSemanticCueFamily::Curse
                : (isProtectionAttribute(attribute->modifier.attribute)
                    ? BattleSemanticCueFamily::Protection
                    : BattleSemanticCueFamily::Positive);
            queueSemanticCue(frame, entry.metadata, entry.metadata.targetUnitId, family);
        }
    }
    else if (const auto* modifier = std::get_if<BattleDamageModifierEffectResult>(&entry.value))
    {
        if (modifier->outcome == BattleModifierApplyOutcome::BlockedByStatusShield)
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
        else if (modifierApplicationShouldCue(
                     modifier->outcome,
                     modifier->modifier.stackCount))
        {
            const auto family = modifier->modifier.negative
                ? BattleSemanticCueFamily::Curse
                : (modifier->modifier.perspective == DamageModifierPerspective::Incoming
                    ? BattleSemanticCueFamily::Protection
                    : BattleSemanticCueFamily::Positive);
            queueSemanticCue(frame, entry.metadata, entry.metadata.targetUnitId, family);
        }
    }
    else if (const auto* absorption = std::get_if<BattleDamageAbsorptionEffectResult>(&entry.value))
    {
        if (absorption->outcome == BattleModifierApplyOutcome::BlockedByStatusShield
            || modifierApplicationShouldCue(absorption->outcome, 1))
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Protection);
        }
    }
    else if (const auto* damage = std::get_if<BattleEffectDamageRequestOutput>(&entry.value))
    {
        CoreDetail::appendEffectDamageOutput(state, frame, pendingDamage, *damage, context);
    }
    else if (const auto* move = std::get_if<
                 BattleRoutedEffectCommand<ForceMoveEffectCommand>>(&entry.value))
    {
        const auto& source = state.units.requireCore(entry.metadata.binding.ownerUnitId);
        const auto& target = state.units.requireCore(entry.metadata.targetUnitId);
        auto direction = target.motion.position - source.motion.position;
        if (move->command.action.direction == ForceMoveDirection::TowardSource)
        {
            direction *= -1.0f;
        }
        if (direction.norm() <= 0.01)
        {
            direction = { 1, 0, 0 };
        }
        frame.queueCommand(BattleKnockbackCommand{
            .targetUnitId = target.id,
            .direction = direction,
            .distance = move->command.action.distancePixels > 0
                ? static_cast<double>(move->command.action.distancePixels)
                : move->command.action.distanceTiles * state.gridTransform.tileWidth,
            .lockFrames = move->command.action.lockFrames,
            .semanticDirection = move->command.action.direction,
            .collision = move->command.action.collision,
            .blocked = move->command.action.blocked,
        });
    }
    else if (const auto* cast = std::get_if<
                 BattleRoutedEffectCommand<ModifyCastEffectCommand>>(&entry.value))
    {
        const auto& request = cast->command.action.autoUltimate;
        if (request)
        {
            frame.queueCommand(BattleAutoUltimateCommand{
                entry.metadata.targetUnitId,
                request->consumeMp,
                request->announce,
            });
        }
    }
    else if (const auto* stateMachine = std::get_if<
                 BattleRoutedEffectCommand<StateMachineEffectCommand>>(&entry.value))
    {
        appendStateMachineOutput(
            state,
            pendingDamage,
            entry,
            stateMachine->command,
            context);
    }
    else if (const auto* heal = std::get_if<BattleResourceEffectResult>(&entry.value))
    {
        const auto* resource = std::get_if<ChangeResourceEffectCommand>(&command.value);
        assert(resource);
        appendMpResourceEffectLogEvents(
            frame.logEvents,
            entry.metadata,
            *resource,
            *heal);
        if (heal->heal && heal->heal->appliedAmount > 0)
        {
            CoreDetail::appendHealEventLog(
                frame.logEvents,
                heal->heal->request.sourceUnitId,
                heal->heal->request.targetUnitId,
                heal->heal->appliedAmount,
                "效果治療");
            frame.visualEvents.push_back(CoreDetail::roleEffectEvent(
                heal->heal->request.targetUnitId,
                KysChess::EFT_HEAL,
                CoreDetail::CoreRoleStatusEffectFrames));
        }
        if (resource->action.resource == BattleResource::Shield
            || resource->action.resource == BattleResource::StatusShield
            || resource->action.resource == BattleResource::StaggerShield
            || resource->action.resource == BattleResource::ControlImmunityFrames
            || resource->action.resource == BattleResource::InvincibilityFrames)
        {
            for (const auto& delta : heal->deltas)
            {
                if (delta.after > delta.before)
                {
                    queueSemanticCue(
                        frame,
                        entry.metadata,
                        delta.unitId,
                        BattleSemanticCueFamily::Protection);
                }
            }
        }
    }
    else if (const auto* status = std::get_if<BattleStatusApplyEffectResult>(&entry.value))
    {
        const auto* apply = std::get_if<ApplyStatusEffectCommand>(&command.value);
        assert(apply);
        CoreDetail::appendPoisonEffectLogEvents(
            frame.logEvents,
            entry.metadata,
            *apply,
            *status);
        queueStatusApplyCue(
            frame,
            entry.metadata,
            apply->action.status,
            lowerStatusQuantity(apply->action).stacks,
            status->status);
    }
    else if (const auto* consume = std::get_if<BattleStatusConsumeEffectResult>(&entry.value))
    {
        const auto* commandConsume = std::get_if<ConsumeStatusEffectCommand>(&command.value);
        assert(commandConsume);
        if (consume->depletedStatus && commandConsume->whenDepleted)
        {
            queueStatusApplyCue(
                frame,
                entry.metadata,
                commandConsume->whenDepleted->action.status,
                lowerStatusQuantity(commandConsume->whenDepleted->action).stacks,
                *consume->depletedStatus);
        }
    }
    else if (const auto* remove = std::get_if<BattleStatusRemoveEffectResult>(&entry.value))
    {
        if (remove->status.removedCount > 0
            || remove->status.currentActionStaggerCleared
            || !remove->removedAttributeModifiers.empty()
            || !remove->removedDamageModifiers.empty())
        {
            queueSemanticCue(
                frame,
                entry.metadata,
                entry.metadata.targetUnitId,
                BattleSemanticCueFamily::Cleanse);
        }
    }
    else if (const auto* deferredHp = std::get_if<BattleDeferredHpResourceOutput>(&entry.value))
    {
        BattleDamageRequest request;
        request.attackerUnitId = entry.metadata.binding.ownerUnitId;
        request.defenderUnitId = entry.metadata.targetUnitId;
        request.baseDamage = deferredHp->command.amount;
        request.damageKind = BattleDamageKind::Effect;
        CoreDetail::appendFramePendingDamage(
            state,
            pendingDamage,
            std::move(request),
            std::nullopt,
            false,
            false,
            {},
            EffectRuleDamageOrigin{
                entry.metadata.ruleId,
                entry.metadata.binding,
            },
            CoreDetail::reserveEffectDamageDescendantWork(state, context, {}));
    }
}

void registerEffectDamageContinuation(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::size_t firstDamageIndex,
    std::vector<EffectCommand> commands,
    BattleEffectCommandContext context)
{
    assert(firstDamageIndex < pendingDamage.size());
    assert(!commands.empty());
    const auto transactionCount = pendingDamage.size() - firstDamageIndex;
    assert(transactionCount <= static_cast<std::size_t>(std::numeric_limits<int>::max()));

    const std::uint64_t continuationId =
        state.effectIntegration.nextDamageContinuationId++;
    const auto [continuation, inserted] =
        state.effectIntegration.damageContinuations.emplace(
            continuationId,
            BattleEffectDamageContinuationRuntime{
                .remainingDamageTransactions = static_cast<int>(transactionCount),
                .commandBatch = {
                    .commands = std::move(commands),
                    .context = std::move(context),
                },
            });
    assert(inserted);
    (void)continuation;
    for (std::size_t index = firstDamageIndex; index < pendingDamage.size(); ++index)
    {
        assert(pendingDamage[index].effectCommandContinuationId == 0);
        pendingDamage[index].effectCommandContinuationId = continuationId;
    }
}

bool isEnemyTopDebuffSource(
    const BattleRuntimeState& state,
    const EffectSourceBinding& binding)
{
    if (binding.kind != EffectSourceKind::Combo)
    {
        return false;
    }
    const auto source = state.effectSourceNames.find({ binding.kind, binding.sourceId });
    return source != state.effectSourceNames.end() && source->second == "陰險";
}

void appendEnemyTopDebuffReportEvents(
    BattleRuntimeState& state,
    int frame,
    std::vector<BattleLogEvent>& logEvents)
{
    std::map<int, EnemyTopDebuffTotals> current;
    for (const auto& modifier : state.effectCommands.attributeModifiers)
    {
        if (!isEnemyTopDebuffSource(state, modifier.binding)
            || (modifier.expiresFrameExclusive && frame >= *modifier.expiresFrameExclusive)
            || !state.units.requireCore(modifier.targetUnitId).alive)
        {
            continue;
        }
        assert(modifier.operation == AttributeOperation::FlatAdd);
        assert(modifier.amount <= 0);
        auto& total = current[modifier.targetUnitId];
        if (total.sourceTeam < 0)
        {
            total.sourceTeam = modifier.binding.sourceTeam;
        }
        else
        {
            assert(total.sourceTeam == modifier.binding.sourceTeam);
        }
        const int contribution = modifier.amount * modifier.stackCount;
        if (modifier.attribute == BattleAttribute::Attack)
        {
            total.attack += contribution;
        }
        else if (modifier.attribute == BattleAttribute::Defence)
        {
            total.defence += contribution;
        }
    }

    std::set<int> targetUnitIds;
    for (const auto& [targetUnitId, _] : current)
    {
        targetUnitIds.insert(targetUnitId);
    }
    for (const auto& [targetUnitId, _] : state.effectIntegration.reportedEnemyTopDebuffs)
    {
        targetUnitIds.insert(targetUnitId);
    }

    std::map<int, BattleEnemyTopDebuffReportState> nextReported;
    for (int targetUnitId : targetUnitIds)
    {
        const auto active = current.find(targetUnitId);
        const auto previous = state.effectIntegration.reportedEnemyTopDebuffs.find(targetUnitId);
        const int previousValue = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? 0
            : previous->second.value;
        int newValue{};
        int sourceTeam = previous == state.effectIntegration.reportedEnemyTopDebuffs.end()
            ? -1
            : previous->second.sourceTeam;
        if (active != current.end())
        {
            assert(active->second.attack == active->second.defence);
            newValue = active->second.attack;
            sourceTeam = active->second.sourceTeam;
            nextReported.emplace(targetUnitId, BattleEnemyTopDebuffReportState{
                .value = newValue,
                .sourceTeam = sourceTeam,
            });
        }
        if (newValue == previousValue)
        {
            continue;
        }
        if (!state.units.requireCore(targetUnitId).alive && newValue == 0)
        {
            continue;
        }

        BattleLogEvent event;
        event.type = BattleLogEventType::Status;
        event.frame = frame;
        event.targetUnitId = targetUnitId;
        event.amount = newValue - previousValue;
        event.previousAmount = previousValue;
        event.newAmount = newValue;
        event.statusId = BattleStatusSemanticId::EnemyTopDebuff;
        event.semanticSourceTeam = sourceTeam;
        event.semanticSourceKind = "combo";
        event.semanticSourceName = "陰險";
        event.segments = battleLogText(
            std::format("陰險：攻防{:+}", event.amount),
            BattleLogTextTone::Negative);
        logEvents.push_back(std::move(event));
    }
    state.effectIntegration.reportedEnemyTopDebuffs = std::move(nextReported);
}

void queueFreeChildCast(
    BattleRuntimeState& state,
    const BattleCastLifecycleEvent& lifecycleEvent,
    const BattleEffectCastRuntimeContext& parentContext,
    const BattleEffectFreeAdditionalCast& freeCast,
    std::span<const EffectCommand> commands,
    std::pmr::memory_resource* frameMemoryResource)
{
    const auto child = state.castLifecycle.beginChildCast(
        lifecycleEvent.provenance.castId,
        {
            .sourceUnitId = lifecycleEvent.provenance.sourceUnitId,
            .magicId = parentContext.skill.id,
            .ultimate = lifecycleEvent.provenance.ultimate,
            .origin = CastOriginKind::FreeRepeat,
            .propagation = freeCast.propagation,
        });
    BattlePendingCastAction pending;
    pending.targetUnitId = freeCast.targetUnitId >= 0
        ? freeCast.targetUnitId
        : parentContext.originalTargetUnitId;
    pending.operationType = parentContext.operationType;
    pending.skillPlan = CoreDetail::makePendingCastSkillPlan(parentContext.skill);
    pending.effectCast = child;
    auto input = CoreDetail::tryMakeRuntimeCastInputForPendingCast(
        state,
        pending,
        frameMemoryResource);
    if (!input)
    {
        state.castLifecycle.cancelPlannedCast(child, state.movement.frame);
        return;
    }

    std::vector<EffectCommand> childCommands;
    for (const auto& command : commands)
    {
        const auto& candidate = command.metadata;
        const auto& source = freeCast.metadata;
        if (candidate.binding.kind == source.binding.kind
            && candidate.binding.sourceId == source.binding.sourceId
            && candidate.binding.ownerUnitId == source.binding.ownerUnitId
            && candidate.binding.sourceTeam == source.binding.sourceTeam
            && candidate.binding.runtimeInstanceId == source.binding.runtimeInstanceId
            && candidate.ruleId == source.ruleId
            && candidate.event == source.event
            && candidate.ruleOrder == source.ruleOrder
            && candidate.targetOrder == source.targetOrder
            && candidate.targetUnitId == source.targetUnitId)
        {
            childCommands.push_back(command);
        }
    }
    const auto preparation = BattleEffectAttackCastSystem().prepareCast(
        *input,
        child.provenance.ultimate,
        childCommands);
    const auto& childSkill = CoreDetail::selectedCastSkill(*input, child.provenance.ultimate);
    const bool forcedRanged = childSkill.forceRanged
        && (childSkill.attackAreaType == 0 || childSkill.attackAreaType == 3);
    pending.operationType = forcedRanged
        ? BattleOperationType::RangedProjectile
        : parentContext.operationType;
    auto cast = BattleCastPlanner().commitSelectedCast(
        *input,
        childSkill,
        child.provenance.ultimate,
        pending.operationType);
    BattleEffectAttackApplyState effectAttackState{
        .nextSharedHitGroupId = state.effectIntegration.nextSharedHitGroupId,
    };
    const auto preparedAttackEffects = BattleEffectAttackCastSystem().applyPreparedCast(
        *input,
        cast,
        preparation,
        effectAttackState);
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, preparedAttackEffects);
    const auto childAttackEffects = BattleEffectAttackCastSystem().applyAttackCommands(
        *input,
        cast,
        childCommands,
        effectAttackState);
    CoreDetail::applyEffectAttackDirectives(cast.attackSpawnRequests, childAttackEffects);
    state.effectIntegration.nextSharedHitGroupId = effectAttackState.nextSharedHitGroupId;
    for (auto& request : cast.attackSpawnRequests)
    {
        request.provenance.propagation = freeCast.propagation;
        request.provenance.origin = BattleAttackOriginKind::CastDerived;
    }
    CoreDetail::reserveEffectRootCastAttacks(
        state.castLifecycle,
        child,
        cast.attackSpawnRequests);
    state.effectIntegration.casts[child.provenance.castId] = {
        .originalTargetUnitId = cast.decision.targetUnitId,
        .resourcesBeforeCast = CoreDetail::snapshotEffectResourcesBeforeCast(state),
        .skill = childSkill,
        .operationType = pending.operationType,
    };
    for (auto& request : cast.attackSpawnRequests)
    {
        state.nextFrame.queueAttack(std::move(request));
    }
    state.castLifecycle.completeWork(child.commitBarrier);
}

std::optional<BattleAreaVisualStyle> areaVisualStyle(const BattleAreaEffect& area)
{
    if (area.source.kind != EffectSourceKind::Magic)
    {
        return std::nullopt;
    }
    return battleAreaVisualStyleForMagicId(area.source.sourceId);
}



}  // namespace

namespace CoreDetail
{



void reduceCommandsBeforeMovement(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsBeforeAttacks(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        currentFrameSinks(frame));
}

void reduceCommandsAfterAttackHits(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterAttackHitSinks(state, frame));
}

void reduceCommandsAfterDamageLifecycle(BattleRuntimeState& state, BattleFrameContext& frame)
{
    reduceFrameGameplayCommandsImpl(
        state,
        frame,
        frame.mutableCommandsForReducer(),
        frame.attackSoundIds,
        frame.rumbles,
        afterDamageLifecycleSinks(state, frame));
}

void completeCastCommitBarriers(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    for (CastWorkToken barrier : frame.drainCastCommitBarriers())
    {
        state.castLifecycle.completeWork(barrier);
    }
}

void dispatchReadyCastLifecycleEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame)
{
    for (;;)
    {
        auto events = state.castLifecycle.drainReadyEvents(state.movement.frame);
        if (events.empty())
        {
            break;
        }
        for (const auto& event : events)
        {
            if (event.type == BattleCastLifecycleEventType::CastSettled)
            {
                state.attacks.releaseCastContactSuppression(
                    event.provenance.castId);
            }
            const auto contextIt = state.effectIntegration.casts.find(
                event.provenance.castId);
            if (contextIt == state.effectIntegration.casts.end())
            {
                assert(event.provenance.propagation
                        == CastPropagationPolicy::NoEffectRules
                    || event.provenance.propagation
                        == CastPropagationPolicy::SourceHitRulesOnly);
                continue;
            }
            const auto& castContext = contextIt->second;
            auto dispatched = BattleEffectEventBridge().dispatchCastLifecycleEvent(
                state,
                nextEffectEventHeader(state, event.provenance.sourceUnitId),
                event,
                {
                    .originalTargetUnitId = castContext.originalTargetUnitId,
                    .resourcesBeforeCast = castContext.resourcesBeforeCast,
                });

            if (event.type == BattleCastLifecycleEventType::CastContinuation)
            {
                BattleCastInput preparationInput(frame.frameMemoryResource());
                preparationInput.config = state.action.castConfig;
                preparationInput.geometry = state.action.castGeometry;
                preparationInput.unit.id = event.provenance.sourceUnitId;
                preparationInput.unit.mp = state.units.requireCore(
                    event.provenance.sourceUnitId).vitals.mp;
                preparationInput.unit.maxMp = state.units.requireCore(
                    event.provenance.sourceUnitId).vitals.maxMp;
                preparationInput.normalSkill = castContext.skill;
                preparationInput.ultimateSkill = castContext.skill;
                const auto preparation = BattleEffectAttackCastSystem().prepareCast(
                    preparationInput,
                    dispatched.commands);
                for (const auto& freeCast : preparation.freeAdditionalCasts)
                {
                    queueFreeChildCast(
                        state,
                        event,
                        castContext,
                        freeCast,
                        dispatched.commands,
                        frame.frameMemoryResource());
                }
            }

            frame.queueEffectCommands(
                std::move(dispatched.commands),
                {
                    .frame = state.movement.frame,
                    .cast = event.provenance,
                    .retainCastUntilDamageDescendants = event.type
                        != BattleCastLifecycleEventType::CastSettled,
                    .areaTargetTeamDomain = state.units.requireCore(
                        event.provenance.sourceUnitId).team,
                });
            reduceEffectCommandBatches(
                state,
                frame,
                state.nextFrame.mutableDamageForReducer());

            if (event.type == BattleCastLifecycleEventType::CastSettled)
            {
                state.effectIntegration.casts.erase(event.provenance.castId);
                std::erase_if(
                    state.effectIntegration.appliedPerCastDamage,
                    [&](const BattleEffectPerCastDamageKey& key)
                    {
                        return key.castId == event.provenance.castId;
                    });
            }
        }
    }
}

void emitPresentationFrame(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto& result = frame.result;
    auto& gameplayEvents = frame.gameplayEvents;
    auto& logEvents = frame.logEvents;
    auto& visualEvents = frame.visualEvents;

    for (const auto& cue : frame.drainSemanticCues())
    {
        visualEvents.push_back(semanticCueEvent(cue));
    }

    BattlePresentationFrame presentationFrame;
    presentationFrame.frame = state.movement.frame;
    const auto resolveEventFrame = [snapshotFrame = state.movement.frame](auto& event)
    {
        assert(event.frame == BattlePresentationCurrentFrame || event.frame >= 0);
        if (event.frame == BattlePresentationCurrentFrame)
        {
            event.frame = snapshotFrame;
        }
    };

    for (auto& event : gameplayEvents)
    {
        resolveEventFrame(event);
    }
    for (auto& event : visualEvents)
    {
        resolveEventFrame(event);
    }
    for (auto& event : logEvents)
    {
        resolveEventFrame(event);
    }
    presentationFrame.gameplayEvents = std::move(gameplayEvents);
    presentationFrame.visualEvents = std::move(visualEvents);
    presentationFrame.logEvents = std::move(logEvents);
    presentationFrame.areas.reserve(state.areas.areas.size());
    for (const auto& area : state.areas.areas)
    {
        const auto style = areaVisualStyle(area);
        if (!style || !BattleAreaEffectSystem::activeAt(area, state.movement.frame))
        {
            continue;
        }
        presentationFrame.areas.push_back({
            .areaId = area.id.value,
            .sourceUnitId = area.source.ownerUnitId,
            .sourceTeam = area.sourceTeam,
            .center = BattleAreaEffectSystem::center(area, state.units),
            .radiusTiles = area.geometry.radiusTiles,
            .tileWidth = state.gridTransform.tileWidth,
            .style = *style,
            .createdFrame = area.createdFrame,
            .expiresFrameExclusive = area.expiresFrameExclusive,
        });
    }
    presentationFrame.gameplayEvents.reserve(
        presentationFrame.gameplayEvents.size() + frame.attackEvents.size());
    presentationFrame.visualEvents.reserve(
        presentationFrame.visualEvents.size() + frame.attackEvents.size() * 3);

    const auto appendGameplayEvent = [&](BattleGameplayEvent event)
    {
        resolveEventFrame(event);
        presentationFrame.gameplayEvents.push_back(std::move(event));
    };
    for (const auto& event : frame.attackEvents)
    {
        appendGameplayEvent(toGameplayEvent(event, state.attacks));
        appendVisualEvents(event, state.attacks, state.movement.frame, presentationFrame.visualEvents);
    }
    presentationFrame.attackSoundIds = std::move(frame.attackSoundIds);
    presentationFrame.rumbles = std::move(frame.rumbles);
    presentationFrame.blinkSoundCount = frame.blinkSoundCount;
    result = std::move(presentationFrame);
}

void applyRuntimeUnitMpDelta(BattleRuntimeState& state, BattleRuntimeUnit& unit, int mpDelta)
{
    if (mpDelta > 0)
    {
        unit.vitals.mp += adjustedRuntimeMpRestore(state, unit.id, mpDelta);
    }
    else if (mpDelta < 0)
    {
        unit.vitals.mp += mpDelta;
    }
    unit.vitals.mp = std::clamp(unit.vitals.mp, 0, unit.vitals.maxMp);
}

void reduceEffectCommandBatches(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
{
    while (true)
    {
        auto queued = std::exchange(
            state.effectIntegration.queuedCommandBatches,
            {});
        for (auto& batch : queued)
        {
            frame.queueEffectCommands(
                std::move(batch.commands),
                std::move(batch.context));
        }

        auto batches = frame.drainEffectCommandBatches();
        if (batches.empty())
        {
            return;
        }
        for (auto& batch : batches)
        {
            std::size_t ruleBegin{};
            while (ruleBegin < batch.commands.size())
            {
                std::size_t ruleEnd = ruleBegin + 1;
                while (ruleEnd < batch.commands.size()
                       && sameEffectRuleCommandSequence(
                           batch.commands[ruleBegin].metadata,
                           batch.commands[ruleEnd].metadata))
                {
                    ++ruleEnd;
                }

                std::size_t actionBegin = ruleBegin;
                while (actionBegin < ruleEnd)
                {
                    const std::uint32_t actionOrder =
                        batch.commands[actionBegin].metadata.actionOrder;
                    std::size_t actionEnd = actionBegin + 1;
                    while (actionEnd < ruleEnd
                           && batch.commands[actionEnd].metadata.actionOrder == actionOrder)
                    {
                        ++actionEnd;
                    }

                    const std::size_t firstDamageIndex = pendingDamage.size();
                    for (std::size_t index = actionBegin; index < actionEnd; ++index)
                    {
                        reduceEffectCommand(
                            state,
                            frame,
                            pendingDamage,
                            batch.commands[index],
                            batch.context);
                    }
                    if (pendingDamage.size() > firstDamageIndex
                        && actionEnd < ruleEnd)
                    {
                        std::vector<EffectCommand> continuationCommands;
                        continuationCommands.reserve(ruleEnd - actionEnd);
                        for (std::size_t index = actionEnd; index < ruleEnd; ++index)
                        {
                            continuationCommands.push_back(
                                std::move(batch.commands[index]));
                        }
                        registerEffectDamageContinuation(
                            state,
                            pendingDamage,
                            firstDamageIndex,
                            std::move(continuationCommands),
                            batch.context);
                        break;
                    }
                    actionBegin = actionEnd;
                }
                ruleBegin = ruleEnd;
            }
        }
    }
}

std::vector<BattleFrameEffectCommandBatch> dispatchFrameAdvancedEffects(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    int upcomingFrame)
{
    assert(upcomingFrame == state.movement.frame + 1);
    std::vector<BattleFrameEffectCommandBatch> deferredAutoUltimateBatches;
    const BattleEffectRuntimeSnapshot snapshot(state);
    const auto readView = snapshot.readView();
    for (const auto& owner : snapshot.units())
    {
        if (!owner.alive)
        {
            continue;
        }

        EffectEventContext event;
        event.event = EffectEvent::FrameAdvanced;
        event.header.frame = upcomingFrame;
        event.header.eventOrdinal = state.effectIntegration.nextEventOrdinal++;
        event.header.binding = {
            .kind = EffectSourceKind::Combo,
            .sourceId = -1,
            .ownerUnitId = owner.id,
            .sourceTeam = owner.team,
        };
        event.header.owner = &owner;
        event.header.battle = readView;
        event.payload = FrameTickEventData{
            .deltaFrames = 1,
            .periodOrdinal = static_cast<std::uint64_t>(upcomingFrame),
        };
        auto dispatched = BattleEffectSystem().dispatch(
            state.effectRules,
            event,
            state.random);
        std::vector<EffectCommand> earlyCommands;
        std::vector<EffectCommand> deferredCommands;
        earlyCommands.reserve(dispatched.commands.size());
        deferredCommands.reserve(dispatched.commands.size());
        for (auto& command : dispatched.commands)
        {
            const auto* modifyCast = std::get_if<ModifyCastEffectCommand>(
                &command.value);
            auto& destination = modifyCast && modifyCast->action.autoUltimate
                ? deferredCommands
                : earlyCommands;
            destination.push_back(std::move(command));
        }
        BattleEffectCommandContext commandContext{
            .frame = upcomingFrame,
            .effectPosition = owner.position,
            .areaTargetTeamDomain = owner.team,
        };
        frame.queueEffectCommands(
            std::move(earlyCommands),
            commandContext);
        if (!deferredCommands.empty())
        {
            deferredAutoUltimateBatches.push_back({
                std::move(deferredCommands),
                std::move(commandContext),
            });
        }
    }
    reduceEffectCommandBatches(state, frame, frame.currentFrameDamage());
    appendEnemyTopDebuffReportEvents(state, upcomingFrame, frame.logEvents);
    return deferredAutoUltimateBatches;
}

void applyLateFrameMpRestores(BattleRuntimeState& state, BattleFrameContext& frame)
{
    auto restores = frame.drainLateMpRestores();
    for (const auto& restore : restores)
    {
        applyFrameMpRestore(
            state,
            restore.unitId,
            restore.amount,
            restore.reason,
            frame.logEvents);
    }
}

CastPropagationPolicy derivedAttackPropagation(
    const BattleAttackProvenance& sourceAttack)
{
    assert(sourceAttack.valid());
    return sourceAttack.propagation == CastPropagationPolicy::SourceRules
        ? CastPropagationPolicy::SourceHitRulesOnly
        : sourceAttack.propagation;
}



BattleRuntimeUnitsAdvanceResult advanceRuntimeUnits(BattleRuntimeState& state)
{
    BattleRuntimeUnitsAdvanceResult result;
    result.cooldownFinishedEffects.reserve(state.units.size());
    for (auto& unitRecord : state.units.live())
    {
        auto& unit = unitRecord.core;
        assert(unit.id >= 0);
        auto tick = unitRecord.advanceFrameTick({
            .frame = state.movement.frame,
            .mpRegenIntervalFrames = 3,
            .physicalPowerRegenIntervalFrames = 3,
            .mpRecoveryBonusPct = mpRecoveryBonusPct(state, unit.id),
        });
        if (tick.skillFinished)
        {
            if (unitRecord.isSkillCooldownUltimate())
            {
                const auto* actionPlan = unitRecord.actionPlan();
                assert(actionPlan && actionPlan->ultimateSkill.id >= 0);
                BattleEffectEventHeaderInput header{
                    .frame = state.movement.frame + 1,
                    .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
                    .ownerUnitId = unit.id,
                };
                auto dispatched = BattleEffectEventBridge().dispatch(
                    state,
                    std::move(header),
                    EffectEvent::UltimateCooldownFinished,
                    UltimateCooldownFinishedEventData{
                        .magicId = actionPlan->ultimateSkill.id,
                    });
                result.cooldownFinishedEffects.push_back({
                    .commands = std::move(dispatched.commands),
                    .context = {
                        .frame = state.movement.frame + 1,
                        .effectPosition = unit.motion.position,
                        .areaTargetTeamDomain = unit.team,
                    },
                });
            }
            unitRecord.clearSkillCooldownSource();
        }
    }
    return result;
}

EffectResourcesBeforeCastSnapshot snapshotEffectResourcesBeforeCast(
    const BattleRuntimeState& state)
{
    std::vector<EffectUnitResourceBeforeCast> result;
    result.reserve(state.units.size());
    for (const auto& record : state.units.all())
    {
        result.push_back({
            .unitId = record.id(),
            .mp = record.core.vitals.mp,
            .maxMp = record.core.vitals.maxMp,
        });
    }
    std::ranges::sort(result, {}, &EffectUnitResourceBeforeCast::unitId);
    return EffectResourcesBeforeCastSnapshot(std::move(result));
}

BattleEffectEventHeaderInput nextEffectEventHeader(
    BattleRuntimeState& state,
    int ownerUnitId,
    EffectFormulaInputs formulaInputs)
{
    assert(ownerUnitId >= 0);
    state.units.requireCore(ownerUnitId);
    return {
        .frame = state.movement.frame,
        .eventOrdinal = state.effectIntegration.nextEventOrdinal++,
        .ownerUnitId = ownerUnitId,
        .formulaInputs = std::move(formulaInputs),
    };
}

BattleCastProvenance plannedEffectCastProvenance(
    int sourceUnitId,
    int magicId,
    bool ultimate)
{
    return {
        .sourceUnitId = sourceUnitId,
        .magicId = magicId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        .propagation = CastPropagationPolicy::SourceRules,
    };
}

BattleEffectDispatchResult dispatchCastPlannedEffects(
    BattleRuntimeState& state,
    const BattleCastInput& input,
    bool ultimate,
    const EffectResourcesBeforeCastSnapshot& resourcesBeforeCast,
    const BattleCastProvenance& provenance)
{
    const auto& skill = ultimate ? input.ultimateSkill : input.normalSkill;
    if (skill.id < 0)
    {
        return {};
    }

    assert(provenance.valid());
    assert(provenance.sourceUnitId == input.unit.id);
    assert(provenance.magicId == skill.id);
    assert(provenance.ultimate == ultimate);
    auto payload = makeCastPlanEventData(
        input,
        ultimate,
        resourcesBeforeCast,
        &provenance);
    return BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, input.unit.id),
        EffectEvent::CastPlanned,
        std::move(payload));
}

void applyEffectAttackDirectives(
    std::span<BattleAttackSpawnRequest> requests,
    const BattleEffectAttackApplyResult& result)
{
    for (const auto& damage : result.damage)
    {
        assert(damage.requestIndex < requests.size());
        if (damage.damageKind)
        {
            requests[damage.requestIndex].initial.damageKind = *damage.damageKind;
        }
    }
    for (const auto& lifecycle : result.lifecycle)
    {
        assert(lifecycle.requestIndex < requests.size());
        auto& request = requests[lifecycle.requestIndex];
        request.provenance.propagation = lifecycle.propagation;
        request.provenance.origin = lifecycle.origin;
        request.provenance.parentAttackId = lifecycle.parentAttackId;
        request.provenance.rootAttack = lifecycle.rootAttack;
    }
}

BattleCastStart beginEffectRootCast(
    BattleCastLifecycle& lifecycle,
    int sourceUnitId,
    int magicId,
    bool ultimate)
{
    return lifecycle.beginRootCast({
        .sourceUnitId = sourceUnitId,
        .magicId = magicId,
        .ultimate = ultimate,
        .origin = ultimate ? CastOriginKind::Ultimate : CastOriginKind::Normal,
        .propagation = CastPropagationPolicy::SourceRules,
    });
}

void cancelEffectRootCast(
    BattleRuntimeState& state,
    const BattleCastStart& start)
{
    assert(start.provenance.valid());
    BattleEffectEventBridge().releaseCastScopedRules(
        state,
        start.provenance.castId);
    state.castLifecycle.cancelPlannedCast(start, state.movement.frame);
}

void reserveEffectRootCastAttacks(
    BattleCastLifecycle& lifecycle,
    const BattleCastStart& start,
    std::span<BattleAttackSpawnRequest> requests)
{
    assert(start.provenance.valid());
    for (std::size_t index = 0; index < requests.size(); ++index)
    {
        auto& request = requests[index];
        assert(!request.provenance.valid());
        assert(!request.castWork.valid());

        BattleAttackReservationRequest reservationRequest;
        reservationRequest.parentAttackId = request.provenance.parentAttackId;
        reservationRequest.origin = request.provenance.origin;
        reservationRequest.rootAttack = request.provenance.rootAttack;
        reservationRequest.mainProjectile = request.provenance.mainProjectile;
        reservationRequest.sharedHitGroupId = request.provenance.sharedHitGroupId;
        reservationRequest.propagation = request.provenance.propagation;
        const auto reservation = lifecycle.reserveAttack(
            start.provenance.castId,
            reservationRequest);
        request.provenance = reservation.provenance;
        request.castWork = reservation.work;
    }
}

std::vector<BattleEffectDispatchResult> dispatchCastCommittedEffects(
    BattleRuntimeState& state,
    const BattlePendingCastAction& pending,
    const BattleCastResult& cast)
{
    const auto& provenance = pending.effectCast.provenance;
    assert(provenance.valid());
    assert(provenance.sourceUnitId == cast.decision.unitId);
    assert(provenance.ultimate == cast.decision.ultimate);
    CastCommitEventData payload;
    payload.provenance = provenance;
    payload.targetUnitId = cast.decision.targetUnitId;
    const auto resourcesBeforeCast = pending.effectResourcesBeforeCast.values();
    const auto resource = std::ranges::find(
        resourcesBeforeCast,
        provenance.sourceUnitId,
        &EffectUnitResourceBeforeCast::unitId);
    payload.mpBefore = resource != resourcesBeforeCast.end()
        ? resource->mp
        : state.units.requireCore(provenance.sourceUnitId).vitals.mp;
    payload.mpPaid = std::max(0, -cast.mpDelta);
    payload.rangeMode = pending.effectPreparation.rangeMode.value_or(CastRangeMode::Preserve);
    payload.attackPattern = cast.attackPattern;
    payload.resourcesBeforeCast = pending.effectResourcesBeforeCast;
    std::vector<BattleEffectDispatchResult> result;
    result.push_back(BattleEffectEventBridge().dispatch(
        state,
        nextEffectEventHeader(state, provenance.sourceUnitId),
        EffectEvent::AttackCommitted,
        payload));
    if (cast.decision.ultimate)
    {
        result.push_back(BattleEffectEventBridge().dispatch(
            state,
            nextEffectEventHeader(state, provenance.sourceUnitId),
            EffectEvent::UltimateCommitted,
            std::move(payload)));
    }
    return result;
}

void completeEffectDamageContinuation(
    BattleRuntimeState& state,
    BattleFrameContext& frame,
    std::vector<BattlePendingDamageIntent>& pendingDamage,
    std::uint64_t continuationId)
{
    if (continuationId == 0)
    {
        return;
    }

    const auto continuation =
        state.effectIntegration.damageContinuations.find(continuationId);
    assert(continuation != state.effectIntegration.damageContinuations.end());
    assert(continuation->second.remainingDamageTransactions > 0);
    if (--continuation->second.remainingDamageTransactions > 0)
    {
        return;
    }

    auto commandBatch = std::move(continuation->second.commandBatch);
    state.effectIntegration.damageContinuations.erase(continuation);
    frame.queueEffectCommands(
        std::move(commandBatch.commands),
        std::move(commandBatch.context));
    reduceEffectCommandBatches(state, frame, pendingDamage);
}

std::vector<BattleStatusEvent> advanceStatus(
    BattleRuntimeState& state,
    std::vector<BattlePendingDamageIntent>& pendingDamage)
{
    state.status.config.frame = state.movement.frame;
    auto statusTick = BattleStatusSystem(state.status.config).tick(state.units);
    for (const auto& event : statusTick.events)
    {
        if (event.type != BattleStatusEventType::PoisonDamage
            && event.type != BattleStatusEventType::BleedDamage)
        {
            continue;
        }

        state.units.require(event.sourceUnitId);
        state.units.require(event.unitId);

        BattleDamageRequest request;
        request.attackerUnitId = event.sourceUnitId;
        request.defenderUnitId = event.unitId;
        request.baseDamage = event.value;
        request.damageKind = event.type == BattleStatusEventType::PoisonDamage
            ? BattleDamageKind::Poison
            : BattleDamageKind::Bleed;
        request.preResolvedDamage = true;
        request.preResolvedModifierPolicy =
            BattlePreResolvedModifierPolicy::DefenderTypedStatuses;
        BattleDamagePresentationInput presentation;
        presentation.segments = battleLogText(event.reason, BattleLogTextTone::SkillName);
        applyStatusTickDamagePresentation(state, event.type, event.unitId, presentation);
        appendFramePendingDamage(
            state,
            pendingDamage,
            std::move(request),
            std::move(presentation),
            false,
            false,
            {},
            EffectStatusDamageOrigin{
                event.type == BattleStatusEventType::PoisonDamage
                    ? BattleStatusKind::Poison
                    : BattleStatusKind::Bleed,
                event.sourceUnitId,
            });
    }
    return std::move(statusTick.events);
}

int mpRecoveryBonusPct(BattleRuntimeState& state, int unitId)
{
    return effectAdjustedAttribute(
        state,
        unitId,
        BattleAttribute::MpRecoveryBonus,
        0);
}

}  // namespace CoreDetail

}  // namespace KysChess::Battle
