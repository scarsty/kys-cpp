#include "BattleHitResolver.h"

#include "BattleLogSegments.h"
#include "BattleMath.h"
#include "BattleRuntimeRandom.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

namespace KysChess::Battle
{
namespace
{
double pointMagnitude(const Pointf& point)
{
    return std::sqrt(
        static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

BattleLogEvent statusEvent(int sourceUnitId, int targetUnitId, std::string text)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    return event;
}

BattleLogEvent statusEvent(
    int sourceUnitId,
    int targetUnitId,
    std::vector<BattleLogTextSegment> segments,
    BattleLogPerspective perspective = BattleLogPerspective::Targeted)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.perspective = perspective;
    event.segments = std::move(segments);
    return event;
}

BattleLogEvent sourceStatusEvent(int sourceUnitId, int targetUnitId, std::string text)
{
    return statusEvent(
        sourceUnitId,
        targetUnitId,
        battleLogText(std::move(text), BattleLogTextTone::SkillName),
        BattleLogPerspective::SourceOnly);
}

BattleVisualEvent floatingTextEvent(int targetUnitId,
                                          std::string text,
                                          BattlePresentationColor color,
                                          int textSize)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::FloatingText;
    event.targetUnitId = targetUnitId;
    event.text = std::move(text);
    event.color = color;
    event.textSize = textSize;
    return event;
}

Pointf normalizedFollowUpVelocity(Pointf from, Pointf to, double speed)
{
    assert(speed > 0.0);
    auto velocity = to - from;
    if (velocity.norm() <= 0.01)
    {
        velocity = { 1, 0, 0 };
    }
    velocity.normTo(static_cast<float>(speed));
    return velocity;
}

BattleAttackSpawnRequest makeNearbyFollowUpSpawn(
    const BattleNearbyTrackingProjectilesCommand& command,
    const BattleRuntimeUnit& target,
    const BattleProjectileFollowUpContext& context)
{
    assert(command.prototype.provenance.valid());
    const auto targetPosition = target.motion.position;
    const double projectileSpeed = command.projectileSpeed > 0.0
        ? command.projectileSpeed
        : context.projectileSpeed;
    BattleAttackSpawnRequest request;
    request.initial.attackSourceUnitId = command.prototype.sourceUnitId;
    request.initial.skillId = command.prototype.skillId;
    request.initial.skillName = command.prototype.skillName;
    request.initial.skillHurtType = command.prototype.skillHurtType;
    request.initial.skillMagicType = command.prototype.skillMagicType;
    request.initial.skillAttackerActProperty = command.prototype.skillAttackerActProperty;
    request.initial.skillMagicPower = command.prototype.skillMagicPower;
    request.initial.damageKind = command.prototype.damageKind;
    request.initial.preferredTargetUnitId = target.id;
    request.initial.requirePreferredTarget = true;
    request.initial.track = true;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = command.prototype.visualEffectId;
    request.initial.ignoreProjectileCancel = command.prototype.skillId < 0;
    request.initial.scriptedDamage = command.prototype.scriptedDamage;
    request.initial.scriptedStunFrames = command.prototype.scriptedStunFrames;
    request.initial.scriptedBleedStacks = command.prototype.scriptedBleedStacks;
    request.initial.strengthPct = command.prototype.strengthPct
        * std::max(1, command.damagePct)
        / 100;
    request.initial.suppressNearbyTrackingProjectileProc = true;
    request.provenance.mainProjectile = false;
    request.initial.position = command.prototype.position;
    request.initial.velocity = normalizedFollowUpVelocity(
        request.initial.position,
        targetPosition,
        projectileSpeed);
    request.initial.totalFrame = std::max(
        context.minimumProjectileFrames,
        battleTravelFrames2d(
            request.initial.position,
            targetPosition,
            std::max(1.0, projectileSpeed))
            + context.nearbyProjectileFramePadding);
    return request;
}

BattleAttackSpawnRequest makeAreaFollowUpSpawn(
    const BattleAreaProjectileFollowUp& followUp,
    int targetUnitId,
    const BattleProjectileFollowUpContext& context,
    const BattleRuntimeUnits& units)
{
    assert(followUp.cast.valid());
    assert(followUp.expansionWork.valid());
    const auto& source = units.requireCore(followUp.sourceUnitId);
    const auto& target = units.requireCore(targetUnitId);
    auto sourcePosition = source.motion.position;
    auto targetPosition = target.motion.position;
    auto direction = targetPosition - sourcePosition;
    if (direction.norm() <= 0.01)
    {
        direction = { 1, 0, 0 };
    }
    direction.normTo(1);
    auto spawnOffset = direction;
    spawnOffset.normTo(static_cast<float>(context.areaSpawnDistance));

    BattleAttackSpawnRequest request;
    request.initial.attackSourceUnitId = followUp.sourceUnitId;
    request.initial.preferredTargetUnitId = targetUnitId;
    request.initial.scriptedDamage = followUp.damage;
    request.initial.scriptedDamageAppliesModifiers = followUp.appliesDamageModifiers;
    request.initial.scriptedDamageTriggersDefenseEffects = followUp.triggersDefenseEffects;
    request.initial.damageKind = followUp.damageKind;
    request.initial.scriptedStunFrames = followUp.stunFrames;
    request.initial.track = true;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.ignoreProjectileCancel = true;
    request.initial.visualEffectId = followUp.effectId;
    request.initial.position = sourcePosition + spawnOffset;
    request.initial.velocity = normalizedFollowUpVelocity(
        request.initial.position,
        targetPosition,
        context.projectileSpeed);
    request.initial.totalFrame = std::max(
        context.minimumProjectileFrames,
        battleTravelFrames2d(
            request.initial.position,
            targetPosition,
            std::max(1.0, context.projectileSpeed))
            + context.areaProjectileFramePadding);
    return request;
}

BattleAcceptedHitSideEffectCommand acceptedHitCommand(
                                                      BattleAttackProvenance provenance,
                                                      int sourceUnitId,
                                                      int targetUnitId,
                                                      BattleDamageRequest request)
{
    request.attackerUnitId = sourceUnitId;
    request.defenderUnitId = targetUnitId;
    request.acceptedHit = true;
    return {
        .sourceUnitId = sourceUnitId,
        .targetUnitId = targetUnitId,
        .damage = std::move(request),
        .provenance = std::move(provenance),
    };
}

std::string appendDetail(std::string detail, const std::string& text)
{
    if (text.empty())
    {
        return detail;
    }
    if (!detail.empty())
    {
        return std::format("{}、{}", detail, text);
    }
    return text;
}

std::string projectileSourceLabel(const BattleAttackEvent& event)
{
    if (event.castSubrequestKind == BattleAttackCastSubrequestKind::DualWieldFollowUp)
    {
        return "左右互搏";
    }
    if (event.operationType == BattleOperationType::Dash)
    {
        return "滑步";
    }
    if (event.provenance.cast.ultimate
        && event.track
        && !event.provenance.mainProjectile)
    {
        return "絕招追蹤彈";
    }
    if (event.provenance.cast.ultimate && !event.provenance.mainProjectile)
    {
        return "絕招追加彈";
    }
    if ((event.track || event.operationType == BattleOperationType::TrackingProjectile)
        && !event.provenance.mainProjectile)
    {
        return "追蹤彈";
    }
    if (event.provenance.sharedHitGroupId > 0
        && !event.provenance.mainProjectile)
    {
        return "連鎖彈";
    }
    return "";
}

bool passesPercentChance(BattleRuntimeRandom& random, int chancePct)
{
    if (chancePct <= 0)
    {
        return false;
    }
    if (chancePct >= 100)
    {
        return true;
    }
    return random.chance(chancePct);
}

std::int64_t effectiveModifierAmount(const BattleHitDamageModifier& modifier)
{
    return static_cast<std::int64_t>(modifier.amount) * modifier.stackCount;
}

void applyDamageReductionPct(
    BattleFixed& damage,
    int reductionPct,
    int& remainingDamageBasisPoints)
{
    if (reductionPct <= 0 || damage <= BattleFixed{})
    {
        return;
    }
    const int requestedRemaining = remainingDamageBasisPoints
        * std::max(0, 100 - reductionPct) / 100;
    const int cappedRemaining = std::max(
        (100 - FinalDamageReductionCapPct) * 100,
        requestedRemaining);
    damage = damage.scaled(cappedRemaining, remainingDamageBasisPoints);
    remainingDamageBasisPoints = cappedRemaining;
}

BattleFixed applyOutgoingBeforeCriticalModifiers(
    BattleFixed damage,
    std::span<const BattleHitDamageModifier> modifiers,
    int& remainingDamageBasisPoints)
{
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation == DamageModifierOperation::Multiply)
        {
            for (int stack = 0; stack < modifier.stackCount; ++stack)
            {
                damage = damage.scaled(modifier.amount, 100);
            }
        }
    }

    std::int64_t percent{};
    std::int64_t flat{};
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation == DamageModifierOperation::PercentAdd)
        {
            percent += effectiveModifierAmount(modifier);
        }
        else if (modifier.operation == DamageModifierOperation::FlatAdd)
        {
            flat += effectiveModifierAmount(modifier);
        }
    }
    if (percent < 0)
    {
        applyDamageReductionPct(
            damage,
            static_cast<int>(std::min<std::int64_t>(-percent, 100)),
            remainingDamageBasisPoints);
    }
    else if (percent > 0)
    {
        damage = damage.scaled(static_cast<int>(100 + percent), 100);
    }
    damage += BattleFixed::fromInteger(static_cast<int>(std::clamp<std::int64_t>(
        flat,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max())));
    return damage;
}

BattleFixed applyOutgoingAfterCriticalModifiers(
    BattleFixed damage,
    std::span<const BattleHitDamageModifier> modifiers)
{
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation != DamageModifierOperation::Multiply)
        {
            continue;
        }
        for (int stack = 0; stack < modifier.stackCount; ++stack)
        {
            damage = damage.scaled(modifier.amount, 100);
        }
    }
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation == DamageModifierOperation::PercentAdd)
        {
            damage = damage.scaled(
                static_cast<int>(100 + effectiveModifierAmount(modifier)),
                100);
        }
        else if (modifier.operation == DamageModifierOperation::FlatAdd)
        {
            damage += BattleFixed::fromInteger(static_cast<int>(effectiveModifierAmount(modifier)));
        }
    }
    return damage;
}

BattleFixed applyIncomingBaseModifiers(
    BattleFixed damage,
    std::span<const BattleHitDamageModifier> modifiers,
    int& remainingDamageBasisPoints)
{
    std::int64_t flat{};
    std::int64_t percent{};
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation == DamageModifierOperation::FlatAdd)
        {
            flat += effectiveModifierAmount(modifier);
        }
        else if (modifier.operation == DamageModifierOperation::PercentAdd)
        {
            percent += effectiveModifierAmount(modifier);
        }
    }
    damage += BattleFixed::fromInteger(static_cast<int>(std::clamp<std::int64_t>(
        flat,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max())));
    if (percent < 0)
    {
        applyDamageReductionPct(
            damage,
            static_cast<int>(std::min<std::int64_t>(-percent, 100)),
            remainingDamageBasisPoints);
    }
    else if (percent > 0)
    {
        damage = damage.scaled(static_cast<int>(100 + percent), 100);
    }
    return damage;
}

BattleFixed applyIncomingAfterBaseModifiers(
    BattleFixed damage,
    std::span<const BattleHitDamageModifier> modifiers,
    int& remainingDamageBasisPoints)
{
    for (const auto& modifier : modifiers)
    {
        const auto amount = effectiveModifierAmount(modifier);
        switch (modifier.operation)
        {
        case DamageModifierOperation::FlatAdd:
            damage += BattleFixed::fromInteger(static_cast<int>(amount));
            break;
        case DamageModifierOperation::PercentAdd:
            if (amount < 0)
            {
                applyDamageReductionPct(
                    damage,
                    static_cast<int>(std::min<std::int64_t>(-amount, 100)),
                    remainingDamageBasisPoints);
            }
            else if (amount > 0)
            {
                damage = damage.scaled(static_cast<int>(100 + amount), 100);
            }
            break;
        case DamageModifierOperation::Multiply:
            for (int stack = 0; stack < modifier.stackCount; ++stack)
            {
                damage = damage.scaled(modifier.amount, 100);
            }
            break;
        case DamageModifierOperation::IgnoreDefensePercent:
        case DamageModifierOperation::CapSingleHitAtMaxHpPercent:
        case DamageModifierOperation::ExecuteBelowMaxHpPercent:
            break;
        }
    }
    return damage;
}

struct BattleHitFinalDamageResult
{
    BattleFixed damage;
    bool maxHitCapped{};
    int maxHitPct{};
};

BattleHitFinalDamageResult applyFinalDamageModifiers(
    BattleFixed damage,
    std::span<const BattleHitDamageModifier> modifiers,
    int maxHp,
    int& remainingDamageBasisPoints,
    bool applySingleHitCap)
{
    damage = applyIncomingAfterBaseModifiers(
        damage,
        modifiers,
        remainingDamageBasisPoints);

    BattleHitFinalDamageResult result{ .damage = damage };
    if (!applySingleHitCap || damage <= BattleFixed{})
    {
        return result;
    }

    int capPct{};
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation != DamageModifierOperation::CapSingleHitAtMaxHpPercent
            || modifier.amount <= 0)
        {
            continue;
        }
        capPct = capPct == 0 ? modifier.amount : std::min(capPct, modifier.amount);
    }
    if (capPct <= 0)
    {
        return result;
    }

    const int maximumDamage = std::max(1, maxHp * capPct / 100);
    if (result.damage > BattleFixed::fromInteger(maximumDamage))
    {
        result.damage = BattleFixed::fromInteger(maximumDamage);
        result.maxHitCapped = true;
        result.maxHitPct = capPct;
    }
    return result;
}

int executeThresholdPct(std::span<const BattleHitDamageModifier> modifiers)
{
    int result{};
    for (const auto& modifier : modifiers)
    {
        if (modifier.operation == DamageModifierOperation::ExecuteBelowMaxHpPercent)
        {
            result = std::max(result, modifier.amount);
        }
    }
    return result;
}

Pointf knockbackDirection(const BattleHitUnitSnapshot& attacker, const BattleHitUnitSnapshot& defender)
{
    auto direction = defender.motion.position - attacker.motion.position;
    if (direction.norm() > 0.01f)
    {
        return direction;
    }
    direction = defender.motion.facing;
    if (direction.norm() > 0.01f)
    {
        return direction;
    }
    return { 1.0f, 0.0f, 0.0f };
}

}  // namespace

BattleProjectileFollowUpExpansion expandBattleProjectileFollowUpCommands(
    std::span<const BattleGameplayCommand> commands,
    BattleProjectileFollowUpContext& context,
    const BattleRuntimeUnits& units)
{
    assert(context.projectileSpeed > 0.0);
    assert(context.minimumProjectileFrames > 0);

    BattleProjectileFollowUpExpansion expansion;
    BattleProjectileTargetingSystem targeting;
    for (const auto& command : commands)
    {
        if (const auto* nearby = std::get_if<BattleNearbyTrackingProjectilesCommand>(&command))
        {
            auto targetIds = targeting.selectNearbyTargets(
                units,
                nearby->prototype.sourceUnitId,
                nearby->centerTargetUnitId,
                nearby->rangePixels);
            for (int targetId : targetIds)
            {
                expansion.commands.push_back(BattleProjectileSpawnCommand{
                    .request = makeNearbyFollowUpSpawn(
                        *nearby,
                        units.requireCore(targetId),
                        context),
                    .sourceAttack = nearby->prototype.provenance,
                    .reason = "範圍追蹤彈",
                });
            }
            continue;
        }
        expansion.commands.push_back(command);
    }
    return expansion;
}

BattleProjectileFollowUpExpansion expandBattleAreaProjectileFollowUp(
    const BattleAreaProjectileFollowUp& followUp,
    BattleProjectileFollowUpContext& context,
    const BattleRuntimeUnits& units)
{
    assert(context.projectileSpeed > 0.0);
    assert(context.minimumProjectileFrames > 0);
    assert(followUp.areaSize > 0);
    assert(followUp.cast.valid());
    assert(followUp.expansionWork.valid());
    if (followUp.sourceAttack)
    {
        assert(followUp.sourceAttack->valid());
        assert(followUp.sourceAttack->cast.castId == followUp.cast.castId);
    }

    BattleProjectileFollowUpExpansion expansion;
    BattleProjectileTargetingSystem targeting;
    auto targetIds = targeting.selectAreaImpactTargets(
        units,
        followUp.sourceUnitId,
        followUp.areaSize,
        followUp.maxTargets,
        followUp.trackedTargetUnitId);
    for (int targetId : targetIds)
    {
        expansion.commands.push_back(BattleProjectileSpawnCommand{
            .request = makeAreaFollowUpSpawn(
                followUp,
                targetId,
                context,
                units),
            .sourceAttack = followUp.sourceAttack,
            .reason = followUp.reason,
        });
    }
    if (!followUp.logText.empty())
    {
        expansion.logEvents.push_back(statusEvent(
            followUp.sourceUnitId,
            -1,
            followUp.logText));
    }
    return expansion;
}

BattleHitResolutionResult BattleHitResolver::resolve(
    const BattleHitResolutionInput& input,
    BattleRuntimeRandom& random) const
{
    assert(input.defender.id >= 0);
    const bool scriptedInput = input.attackEvent.scriptedDamage > 0
        || input.attackEvent.scriptedStunFrames > 0
        || input.attackEvent.scriptedBleedStacks > 0;
    assert(input.attacker.id >= 0 || scriptedInput);

    BattleHitResolutionResult result;
    result.attackerUnitId = input.attacker.id;
    result.defenderUnitId = input.defender.id;

    if (input.attackEvent.type != BattleAttackEventType::Hit)
    {
        return result;
    }
    assert(input.attackEvent.provenance.valid());

    const bool scriptedImpact = scriptedInput;
    if (scriptedImpact)
    {
        if (input.attackEvent.scriptedStunFrames > 0 || input.attackEvent.scriptedBleedStacks > 0)
        {
            BattleDamageRequest request;
            request.stunFrames = input.attackEvent.scriptedStunFrames;
            request.bleedStacks = input.attackEvent.scriptedBleedStacks;
            request.bleedMaxStacks = input.attackEvent.scriptedBleedStacks > 0
                ? input.sharedBleedMaxStacks
                : 0;
            result.commands.push_back(acceptedHitCommand(
                input.attackEvent.provenance,
                input.attacker.id,
                input.defender.id,
                request));
            if (input.attackEvent.scriptedStunFrames > 0)
            {
                result.logEvents.push_back(statusEvent(
                    input.attacker.id,
                    input.defender.id,
                    logStatusFrames("眩暈", input.attackEvent.scriptedStunFrames)));
            }
            if (input.attackEvent.scriptedBleedStacks > 0)
            {
                result.logEvents.push_back(statusEvent(
                    input.attacker.id,
                    input.defender.id,
                    std::format("螺旋流血彈流血{}層", input.attackEvent.scriptedBleedStacks)));
            }
        }
        if (input.attackEvent.scriptedDamage > 0)
        {
            BattleHpDamageCommand command{
                .sourceUnitId = input.attacker.id,
                .targetUnitId = input.defender.id,
                .damage = input.attackEvent.scriptedDamage,
                .segments = battleLogText(
                    "特效傷害",
                    BattleLogTextTone::SkillName),
                .preResolvedDamage = !input.attackEvent.scriptedDamageAppliesModifiers,
                .triggersDefenseEffects = input.attackEvent.scriptedDamageTriggersDefenseEffects,
            };
            command.provenance = input.attackEvent.provenance;
            command.damageKind = input.attackEvent.damageKind;
            result.commands.push_back(std::move(command));
            result.finalHpDamage = input.attackEvent.scriptedDamage;
        }
        return result;
    }

    const bool usingSkill = input.skill.id >= 0;
    const int impactFrozenFrames = input.attackEvent.provenance.mainProjectile
        ? (input.attackEvent.provenance.cast.ultimate ? 10 : 5)
        : 0;

    BattleHitShapeInput shapeInput;
    shapeInput.baseDamage = input.skill.resolvedBaseDamage;
    shapeInput.projectileCancelDamage = input.attackEvent.projectileCancelDamage;
    shapeInput.strengthPct = input.attackEvent.strengthPct;
    shapeInput.frame = input.attackEvent.frame;
    shapeInput.totalFrame = input.attackEvent.totalFrame;
    shapeInput.impactPosition = input.attackEvent.position;
    shapeInput.defenderPosition = input.defender.motion.position;
    shapeInput.defenderFacing = input.defender.motion.facing;
    shapeInput.operationType = input.attackEvent.operationType;
    shapeInput.usingSkill = usingSkill;
    shapeInput.attackerActProperty = input.skill.attackerActProperty;
    shapeInput.defenderActProperty = input.skill.defenderActProperty;

    const auto shaped = BattleDamageSystem().shapeHitDamage(shapeInput);
    BattleFixed shapedDamage = shaped.damage;

    if (shaped.frozenFrames > 0)
    {
        BattleDamageRequest request;
        request.hitstunFrames = shaped.frozenFrames;
        result.commands.push_back(acceptedHitCommand(
            input.attackEvent.provenance,
            input.attacker.id,
            input.defender.id,
            request));
    }

    auto hitVelocity = knockbackDirection(input.attacker, input.defender);
    hitVelocity.normTo(1.0f);
    result.commands.push_back(BattleKnockbackCommand{
        .targetUnitId = input.defender.id,
        .direction = hitVelocity,
        .distance = 1.0,
        .lockFrames = 1,
    });

    int remainingDamageBasisPoints = 10'000;
    shapedDamage = applyOutgoingBeforeCriticalModifiers(
        shapedDamage,
        input.damageModifiers.outgoingBeforeCritical,
        remainingDamageBasisPoints);

    result.critical = input.forceCritical
        || passesPercentChance(random, input.attackerCriticalChancePct);
    if (result.critical)
    {
        result.criticalMultiplier = std::max(100, input.attackerCriticalMultiplierPct);
        shapedDamage = shapedDamage.scaled(result.criticalMultiplier, 100);
    }

    shapedDamage = applyOutgoingAfterCriticalModifiers(
        shapedDamage,
        input.damageModifiers.outgoingAfterCritical);

    if (input.attackEvent.provenance.mainProjectile)
    {
        for (const auto& proc : input.knockbackProcs)
        {
            if (!random.chance(proc.chancePct))
            {
                continue;
            }
            assert(proc.action.direction == ForceMoveDirection::AwayFromSource);
            assert(proc.action.distancePixels > 0);
            assert(proc.action.lockFrames > 0);
            auto procDirection = knockbackDirection(input.attacker, input.defender);
            procDirection.normTo(1.0f);
            result.commands.push_back(BattleKnockbackCommand{
                .targetUnitId = input.defender.id,
                .direction = procDirection,
                .distance = static_cast<double>(proc.action.distancePixels),
                .lockFrames = proc.action.lockFrames,
                .semanticDirection = proc.action.direction,
                .collision = proc.action.collision,
                .blocked = proc.action.blocked,
            });
            auto knockbackLog = statusEvent(
                input.attacker.id,
                input.defender.id,
                std::format(
                    "擊退（{}距離·{}幀）",
                    proc.action.distancePixels,
                    proc.action.lockFrames));
            knockbackLog.statusId = BattleStatusSemanticId::Knockback;
            knockbackLog.amount = proc.action.distancePixels;
            knockbackLog.secondaryAmount = proc.action.lockFrames;
            result.logEvents.push_back(std::move(knockbackLog));
        }
    }

    const int offensiveCooldownExtendPct = passesPercentChance(
        random,
        input.attackerCooldownExtensionChancePct)
        ? input.attackerCooldownExtensionPct
        : 0;
    if (offensiveCooldownExtendPct > 0)
    {
        BattleDamageRequest request;
        request.cooldownExtendPct = offensiveCooldownExtendPct;
        result.commands.push_back(acceptedHitCommand(
            input.attackEvent.provenance,
            input.attacker.id,
            input.defender.id,
            request));

    }

    const bool reflectableProjectile =
        input.attackEvent.operationType == BattleOperationType::RangedProjectile
        || input.attackEvent.operationType == BattleOperationType::TrackingProjectile;
    const bool usingHpDamage = input.skill.hurtType == 0;

    shapedDamage = applyIncomingBaseModifiers(
        shapedDamage,
        input.damageModifiers.incomingBase,
        remainingDamageBasisPoints);
    shapedDamage = applyIncomingAfterBaseModifiers(
        shapedDamage,
        input.damageModifiers.incomingAfterBase,
        remainingDamageBasisPoints);
    if (shapedDamage > BattleFixed{})
    {
        shapedDamage += BattleFixed::fromInteger(input.randomDamageVariance);
        shapedDamage = std::max(BattleFixed{}, shapedDamage);
    }
    shapedDamage = applyFinalDamageModifiers(
        shapedDamage,
        input.damageModifiers.outgoingFinal,
        input.defender.vitals.maxHp,
        remainingDamageBasisPoints,
        false).damage;
    auto finalDamage = applyFinalDamageModifiers(
        shapedDamage,
        input.damageModifiers.incomingFinal,
        input.defender.vitals.maxHp,
        remainingDamageBasisPoints,
        true);
    shapedDamage = finalDamage.damage;
    result.shapedHpDamage = shapedDamage.toDouble();
    if (finalDamage.maxHitCapped)
    {
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            std::format("單次承傷封頂{}%最大生命", finalDamage.maxHitPct)));
    }

    const int defensiveCooldownExtendPct = passesPercentChance(
        random,
        input.defenderCooldownExtensionChancePct)
        ? input.defenderCooldownExtensionPct
        : 0;
    if (defensiveCooldownExtendPct > 0)
    {
        BattleDamageRequest request;
        request.cooldownExtendPct = defensiveCooldownExtendPct;
        result.commands.push_back(acceptedHitCommand(
            input.attackEvent.provenance,
            input.defender.id,
            input.attacker.id,
            request));

    }

    result.reflected = reflectableProjectile
        && passesPercentChance(random, input.defenderProjectileReflectChancePct);
    if (result.reflected)
    {
        result.visualEvents.push_back(floatingTextEvent(
            input.defender.id,
            "彈反",
            { 180, 150, 255, 255 },
            24));
        result.logEvents.push_back(sourceStatusEvent(input.defender.id, input.attacker.id, "彈反了遠程攻擊"));
    }

    const int typedExecuteThresholdPct = !result.reflected && usingHpDamage
        ? executeThresholdPct(input.damageModifiers.outgoingFinal)
        : 0;
    const bool canTriggerDefenderBlock = !result.reflected;

    std::string damageDetail;
    if (result.critical)
    {
        damageDetail = appendDetail(
            std::move(damageDetail),
            std::format("暴擊 {}", criticalMultiplierLabel(result.criticalMultiplier)));
    }
    if (result.reflected)
    {
        damageDetail = appendDetail(std::move(damageDetail), "彈反");
    }
    if (auto label = projectileSourceLabel(input.attackEvent); !label.empty())
    {
        damageDetail = appendDetail(std::move(damageDetail), label);
    }

    const int skillReflectPct = input.defenderSkillReflectPercent;
    if (!result.reflected && usingSkill && skillReflectPct > 0)
    {
        int reflectedDamage = shapedDamage.scaled(skillReflectPct, 100).toInt();
        if (reflectedDamage > 0)
        {
            result.commands.push_back(BattleHpDamageCommand{
                .sourceUnitId = input.defender.id,
                .targetUnitId = input.attacker.id,
                .damage = reflectedDamage,
                .segments = battleLogText(
                    "技能反彈",
                    BattleLogTextTone::SkillName),
                .triggersDefenseEffects = false,
            });
        }
    }

    if (!result.reflected)
    {
        if (!input.attackEvent.suppressNearbyTrackingProjectileProc)
        {
            const double attackerProjectileSpeed = pointMagnitude(input.attackEvent.velocity) > 0.01
                ? pointMagnitude(input.attackEvent.velocity)
                : 0.0;
            for (const auto& proc : input.nearbyTrackingProcs)
            {
                if (!random.chance(proc.chancePct))
                {
                    continue;
                }
                assert(proc.behavior.rangePixels > 0);
                assert(proc.behavior.damagePct > 0);
                result.commands.push_back(BattleNearbyTrackingProjectilesCommand{
                    input.attackEvent,
                    input.defender.id,
                    proc.behavior.rangePixels,
                    proc.behavior.damagePct,
                    attackerProjectileSpeed,
                });
                result.activatedRuntimeRules.push_back(proc.rule);
            }
        }
    }

    if (usingHpDamage && shapedDamage > BattleFixed{})
    {
        const int damage = shapedDamage.toInt();
        if (damage > 0)
        {
            const int sourceUnitId = result.reflected ? input.defender.id : input.attacker.id;
            const int targetUnitId = result.reflected ? input.attacker.id : input.defender.id;
            BattleHpDamageCommand command{
                .sourceUnitId = sourceUnitId,
                .targetUnitId = targetUnitId,
                .damage = damage,
                .critical = result.critical,
                .executeThresholdPct = typedExecuteThresholdPct,
                .canTriggerDefenderBlock = canTriggerDefenderBlock,
                .frozenFrames = !result.reflected ? impactFrozenFrames : 0,
                .skillName = input.skill.name,
                .segments = battleLogText(
                    damageDetail,
                    BattleLogTextTone::SkillName),
                .triggersDefenseEffects = !result.reflected,
            };
            command.criticalMultiplier = result.criticalMultiplier;
            command.skillId = input.skill.id;
            command.damageKind = result.reflected
                ? BattleDamageKind::Reflected
                : input.attackEvent.damageKind;
            command.combinedDamageReductionBasisPoints = result.reflected
                ? 0
                : 10'000 - remainingDamageBasisPoints;
            if (!result.reflected)
            {
                command.provenance = input.attackEvent.provenance;
            }
            result.commands.push_back(std::move(command));
            result.finalHpDamage = damage;
        }
    }
    else if (!usingHpDamage && shapedDamage > BattleFixed{})
    {
        const int damage = shapedDamage.toInt();
        if (damage > 0)
        {
            BattleDamageRequest request;
            request.mpDamage = damage;
            request.mpOnHit = input.attackEvent.provenance.origin
                    == BattleAttackOriginKind::Echo
                ? 0
                : damage * 80 / 100;
            request.hitstunFrames = !result.reflected ? impactFrozenFrames : 0;
            const int sourceUnitId = result.reflected ? input.defender.id : input.attacker.id;
            const int targetUnitId = result.reflected ? input.attacker.id : input.defender.id;
            result.commands.push_back(BattleMpDamageCommand{
                sourceUnitId,
                targetUnitId,
                request,
                canTriggerDefenderBlock,
                input.attackEvent.provenance,
            });
            result.finalMpDamage = damage;
        }
    }

    if (!usingHpDamage)
    {
        result.finalHpDamage = 0;
    }
    return result;
}

}  // namespace KysChess::Battle
