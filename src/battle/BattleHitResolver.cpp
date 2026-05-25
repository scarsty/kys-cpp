#include "BattleHitResolver.h"

#include "BattleLogSegments.h"
#include "../ChessEftIds.h"
#include "BattleComboTriggerSystem.h"
#include "BattleRuntimeRandom.h"
#include "BattleUnitStore.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>

namespace KysChess::Battle
{
namespace
{

constexpr int RoleStatusEffectFrames = 45;

double pointMagnitude(const Pointf& point)
{
    return std::sqrt(
        static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

std::string formatStatusPercent(const char* label, int pct)
{
    if (pct <= 0)
    {
        return label;
    }
    return std::format("{}（{}%）", label, pct);
}

std::string formatStatusPercentFrames(const char* label, int pct, int frames)
{
    if (pct > 0 && frames > 0)
    {
        return std::format("{}（{}%·{}幀）", label, pct, frames);
    }
    if (pct > 0)
    {
        return formatStatusPercent(label, pct);
    }
    return frames > 0 ? std::format("{}（{}幀）", label, frames) : std::string(label);
}

std::string formatStackingEffectStatus(const char* label, int pctPerStack, int stacks)
{
    if (pctPerStack <= 0 || stacks <= 0)
    {
        return label;
    }
    return std::format("{} +{}%（{}層）", label, pctPerStack * stacks, stacks);
}

std::string formatCooldownIncreaseStatus(int pct, int before, int after)
{
    if (pct <= 0)
    {
        return "冷卻延長";
    }
    if (before > 0 && after > 0)
    {
        return std::format("冷卻延長（+{}%，{}→{}幀）", pct, before, after);
    }
    return std::format("冷卻延長（+{}%）", pct);
}

BattleDamageUnitState makeDamageUnit(
    const BattleHitUnitSnapshot& unit,
    const RoleComboState* combo,
    const BattleStatusEffectState* effects)
{
    BattleDamageUnitState damageUnit;
    damageUnit.id = unit.id;
    damageUnit.alive = unit.alive;
    damageUnit.vitals = unit.vitals;
    damageUnit.attack = unit.stats.attack;
    damageUnit.invincible = unit.invincible;
    if (combo)
    {
        damageUnit.mpRecoveryBonusPct = sumAlwaysEffectValue(*combo, EffectType::MPRecoveryBonus);
    }
    if (effects)
    {
        damageUnit.mpBlocked = effects->mpBlockTimer > 0;
    }
    return damageUnit;
}

BattleResourceUnitState makeResourceUnit(
    const BattleHitUnitSnapshot& unit,
    const RoleComboState* combo,
    const BattleStatusEffectState* effects)
{
    BattleResourceUnitState resource;
    resource.id = unit.id;
    resource.alive = unit.alive;
    resource.vitals = unit.vitals;
    if (combo)
    {
        resource.mpRecoveryBonusPct = sumAlwaysEffectValue(*combo, EffectType::MPRecoveryBonus);
    }
    if (effects)
    {
        resource.mpBlocked = effects->mpBlockTimer > 0;
    }
    return resource;
}

BattleCooldownState makeCooldownState(const BattleHitUnitSnapshot& unit)
{
    BattleCooldownState cooldown;
    cooldown.alive = unit.alive;
    cooldown.cooldown = unit.animation.cooldown;
    cooldown.cooldownMax = unit.animation.cooldownMax;
    cooldown.haveAction = unit.haveAction;
    cooldown.operationType = unit.operationType;
    cooldown.actType = unit.animation.actType;
    return cooldown;
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

BattleLogEvent healEvent(int sourceUnitId, int targetUnitId, int amount, std::string reason)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.segments = battleLogText(std::move(reason), BattleLogTextTone::SkillName);
    return event;
}

BattleVisualEvent roleEffectEvent(int targetUnitId, int effectId, int durationFrames)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::RoleEffect;
    event.targetUnitId = targetUnitId;
    event.effectId = effectId;
    event.visualEffectId = effectId;
    event.durationFrames = durationFrames;
    return event;
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

double followUpDistance(Pointf lhs, Pointf rhs)
{
    return EuclidDis(lhs.x - rhs.x, lhs.y - rhs.y);
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
    const auto targetPosition = target.motion.position;
    const double projectileSpeed = command.projectileSpeed > 0.0
        ? command.projectileSpeed
        : context.projectileSpeed;
    BattleAttackSpawnRequest request;
    request.initial.attackerUnitId = command.prototype.sourceUnitId;
    request.initial.skillId = command.prototype.skillId;
    request.initial.preferredTargetUnitId = target.id;
    request.initial.requirePreferredTarget = true;
    request.initial.track = true;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.visualEffectId = command.prototype.visualEffectId;
    request.initial.ultimate = command.prototype.ultimate;
    request.initial.ignoreProjectileCancel = command.prototype.skillId < 0;
    request.initial.scriptedDamage = command.prototype.scriptedDamage;
    request.initial.scriptedStunFrames = command.prototype.scriptedStunFrames;
    request.initial.scriptedBleedStacks = command.prototype.scriptedBleedStacks;
    request.initial.sharedHitGroupId = command.prototype.sharedHitGroupId;
    request.initial.strengthMultiplier = command.prototype.strengthMultiplier
        * static_cast<float>(std::max(1, command.damagePct) / 100.0);
    request.initial.suppressNearbyTrackingProjectileProc = true;
    request.initial.mainProjectile = false;
    request.initial.position = command.prototype.position;
    request.initial.velocity = normalizedFollowUpVelocity(
        request.initial.position,
        targetPosition,
        projectileSpeed);
    request.initial.totalFrame = std::max(
        context.minimumProjectileFrames,
        static_cast<int>(std::ceil(followUpDistance(targetPosition, request.initial.position)
                                   / std::max(1.0, projectileSpeed)))
            + context.nearbyProjectileFramePadding);
    return request;
}

BattleAttackSpawnRequest makeAreaFollowUpSpawn(
    int sourceUnitId,
    int targetUnitId,
    int damage,
    int stunFrames,
    int visualEffectId,
    const BattleProjectileFollowUpContext& context,
    const BattleUnitStore& units)
{
    const auto& source = units.requireUnit(sourceUnitId);
    const auto& target = units.requireUnit(targetUnitId);
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
    request.initial.attackerUnitId = sourceUnitId;
    request.initial.preferredTargetUnitId = targetUnitId;
    request.initial.scriptedDamage = damage;
    request.initial.scriptedStunFrames = stunFrames;
    request.initial.track = true;
    request.initial.operationType = BattleOperationType::RangedProjectile;
    request.initial.ignoreProjectileCancel = true;
    request.initial.visualEffectId = visualEffectId;
    request.initial.position = sourcePosition + spawnOffset;
    request.initial.velocity = normalizedFollowUpVelocity(
        request.initial.position,
        targetPosition,
        context.projectileSpeed);
    request.initial.totalFrame = std::max(
        15,
        static_cast<int>(std::ceil(followUpDistance(targetPosition, request.initial.position)
                                   / std::max(1.0, context.projectileSpeed)))
            + context.areaProjectileFramePadding);
    return request;
}

BattleAcceptedHitSideEffectCommand acceptedHitCommand(int sourceUnitId,
                                                      int targetUnitId,
                                                      BattleDamageRequest request)
{
    request.attackerUnitId = sourceUnitId;
    request.defenderUnitId = targetUnitId;
    request.acceptedHit = true;
    return { sourceUnitId, targetUnitId, request };
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
    if (event.operationType == BattleOperationType::Dash)
    {
        return "滑步";
    }
    if (event.ultimate && event.track && !event.mainProjectile)
    {
        return "絕招追蹤彈";
    }
    if (event.ultimate && !event.mainProjectile)
    {
        return "絕招追加彈";
    }
    if ((event.track || event.operationType == BattleOperationType::TrackingProjectile) && !event.mainProjectile)
    {
        return "追蹤彈";
    }
    if (event.sharedHitGroupId > 0 && !event.mainProjectile)
    {
        return "連鎖彈";
    }
    return "";
}

bool canApplyOffensiveControlEffects(const BattleAttackEvent& event)
{
    return event.mainProjectile
        || event.operationType != BattleOperationType::RangedProjectile;
}

}  // namespace

BattleProjectileFollowUpExpansion expandBattleProjectileFollowUpCommands(
    const std::vector<BattleGameplayCommand>& commands,
    BattleProjectileFollowUpContext& context,
    const BattleUnitStore& units)
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
                    makeNearbyFollowUpSpawn(
                        *nearby,
                        units.requireUnit(targetId),
                        context),
                    "範圍追蹤彈",
                });
            }
            continue;
        }
        if (const auto* shieldExplosion = std::get_if<BattleShieldExplosionCommand>(&command))
        {
            auto targetIds = targeting.selectAreaImpactTargets(
                units,
                shieldExplosion->sourceUnitId,
                shieldExplosion->areaSize,
                0,
                -1);
            for (int targetId : targetIds)
            {
                expansion.commands.push_back(BattleProjectileSpawnCommand{
                    makeAreaFollowUpSpawn(
                        shieldExplosion->sourceUnitId,
                        targetId,
                        shieldExplosion->damage,
                        0,
                        shieldExplosion->effectId,
                        context,
                        units),
                    shieldExplosion->reason,
                });
            }
            expansion.logEvents.push_back(statusEvent(
                shieldExplosion->sourceUnitId,
                -1,
                std::format("{}（{}傷害）", shieldExplosion->reason, shieldExplosion->damage)));
            continue;
        }
        if (const auto* deathAoe = std::get_if<BattleDeathAoeProjectileCommand>(&command))
        {
            auto targetIds = targeting.selectAreaImpactTargets(
                units,
                deathAoe->sourceUnitId,
                7,
                deathAoe->maxTargets,
                deathAoe->trackedTargetUnitId);
            for (int targetId : targetIds)
            {
                expansion.commands.push_back(BattleProjectileSpawnCommand{
                    makeAreaFollowUpSpawn(
                        deathAoe->sourceUnitId,
                        targetId,
                        deathAoe->damage,
                        deathAoe->stunFrames,
                        KysChess::EFT_DEATH_BLAST,
                        context,
                        units),
                    "殉爆",
                });
            }
            expansion.logEvents.push_back(statusEvent(
                deathAoe->sourceUnitId,
                -1,
                std::format("殉爆{}%（{}幀）", deathAoe->damagePct, deathAoe->stunFrames)));
            continue;
        }
        expansion.commands.push_back(command);
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
    result.attackerCombo = input.attackerCombo;
    result.defenderCombo = input.defenderCombo;

    if (input.attackEvent.type != BattleAttackEventType::Hit)
    {
        return result;
    }

    const bool scriptedImpact = scriptedInput;
    if (scriptedImpact)
    {
        if (input.attackEvent.scriptedStunFrames > 0 || input.attackEvent.scriptedBleedStacks > 0)
        {
            BattleDamageRequest request;
            request.frozenFrames = input.attackEvent.scriptedStunFrames;
            request.bleedStacks = input.attackEvent.scriptedBleedStacks;
            request.bleedMaxStacks = input.attackEvent.scriptedBleedStacks > 0
                ? input.sharedBleedMaxStacks
                : 0;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
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
            result.commands.push_back(BattleHpDamageCommand{
                input.attacker.id,
                input.defender.id,
                input.attackEvent.scriptedDamage,
                false,
                false,
                false,
                false,
                0,
                "",
                battleLogText("特效傷害", BattleLogTextTone::SkillName),
            });
            result.finalHpDamage = input.attackEvent.scriptedDamage;
        }
        return result;
    }

    const bool usingSkill = input.skill.id >= 0;
    const double baseDamage = static_cast<double>(input.skill.resolvedBaseDamage);
    const int impactFrozenFrames = input.attackEvent.mainProjectile
        ? (input.attackEvent.ultimate ? 10 : 5)
        : 0;

    BattleLegacyHitShapeInput shapeInput;
    shapeInput.baseDamage = baseDamage;
    shapeInput.projectileCancelDamage = input.attackEvent.projectileCancelDamage;
    shapeInput.strengthMultiplier = input.attackEvent.strengthMultiplier;
    shapeInput.frame = input.attackEvent.frame;
    shapeInput.totalFrame = input.attackEvent.totalFrame;
    shapeInput.impactPosition = input.attackEvent.position;
    shapeInput.defenderPosition = input.defender.motion.position;
    shapeInput.defenderFacing = input.defender.motion.facing;
    shapeInput.operationType = input.attackEvent.operationType;
    shapeInput.usingSkill = usingSkill;
    shapeInput.attackerActProperty = input.skill.attackerActProperty;
    shapeInput.defenderActProperty = input.skill.defenderActProperty;

    const auto shaped = BattleDamageSystem().shapeLegacyHitDamage(shapeInput);
    result.shapedHpDamage = shaped.damage;

    if (shaped.frozenFrames > 0)
    {
        BattleDamageRequest request;
        request.frozenFrames = shaped.frozenFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
    }

    auto velocityDelta = input.defender.motion.position - input.attacker.motion.position;
    velocityDelta.normTo(static_cast<float>(shaped.knockbackStrength));
    result.commands.push_back(BattleKnockbackCommand{
        input.defender.id,
        velocityDelta,
        shaped.knockbackVelocityCap,
    });

    auto attackerCombo = input.attackerCombo;
    const int mpRatioDmgBoostPct = sumAlwaysEffectValue(attackerCombo, EffectType::MPRatioDmgBoost);
    if (usingSkill && input.attacker.vitals.maxMp > 0 && mpRatioDmgBoostPct > 0)
    {
        const double mpRatio = static_cast<double>(input.attacker.vitals.mp) / input.attacker.vitals.maxMp;
        const double boostPct = mpRatio * mpRatioDmgBoostPct;
        if (boostPct > 0.0)
        {
            result.shapedHpDamage *= 1.0 + boostPct / 100.0;
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                std::format("內力加傷 +{:.1f}%（目前內力 {}%）",
                            boostPct,
                            static_cast<int>(std::round(mpRatio * 100.0)))));
        }
    }

    result.shapedHpDamage = BattleDamageSystem().applyModifiers({
        result.shapedHpDamage,
        usingSkill,
        true,
        makeBattleDamageModifierState(&attackerCombo),
        {},
        makeDamageUnit(input.defender, nullptr, &input.defenderStatusEffects),
    }).damage;

    auto attackerDamage = BattleComboTriggerSystem().shapeAttackerHitDamage(
        attackerCombo,
        { result.shapedHpDamage, input.attacker.vitals.hp, input.attacker.vitals.maxHp, attackerCombo.lastAliveFlag },
        random);
    result.shapedHpDamage = attackerDamage.damage;
    for (const auto& damageEvent : attackerDamage.events)
    {
        switch (damageEvent.type)
        {
        case BattleAttackerHitDamageEventType::Crit:
            result.critical = true;
            break;
        case BattleAttackerHitDamageEventType::RampingStack:
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStackingEffectStatus("連擊增傷", damageEvent.value, damageEvent.value2)));
            break;
        default:
            assert(false);
        }
    }

    const int mpOnHit = sumAlwaysEffectValue(attackerCombo, EffectType::MPOnHit);
    const int hpOnHit = sumAlwaysEffectValue(attackerCombo, EffectType::HPOnHit);
    const int mpDrain = sumAlwaysEffectValue(attackerCombo, EffectType::MPDrain);
    if (mpOnHit > 0 || hpOnHit > 0 || mpDrain > 0)
    {
        BattleDamageRequest request;
        request.mpOnHit = mpOnHit;
        request.hpOnHit = hpOnHit;
        request.mpDrain = mpDrain;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));

        auto resources = BattleDamageSystem().applyOnHitResources({
            makeResourceUnit(input.attacker, &attackerCombo, &input.attackerStatusEffects),
            makeResourceUnit(input.defender, &input.defenderCombo, &input.defenderStatusEffects),
            mpOnHit,
            hpOnHit,
            mpDrain,
        });
        if (resources.hpHealed > 0)
        {
            result.visualEvents.push_back(roleEffectEvent(input.attacker.id, KysChess::EFT_HEAL, RoleStatusEffectFrames));
            result.logEvents.push_back(healEvent(input.attacker.id, input.attacker.id, resources.hpHealed, "命中回血"));
        }
    }

    const int poisonPct = sumAlwaysEffectValue(attackerCombo, EffectType::PoisonDOT);
    const auto* poison = firstAlwaysEffect(attackerCombo, EffectType::PoisonDOT);
    const int poisonDuration = poison ? poison->value2 * 30 : 0;
    if (poisonPct > 0)
    {
        BattleDamageRequest request;
        request.poisonPct = poisonPct;
        request.poisonDurationFrames = poisonDuration;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.logEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusPercentFrames("中毒", poisonPct, poisonDuration)));
    }

    const bool offensiveControlEffectsAllowed = canApplyOffensiveControlEffects(input.attackEvent);
    int legacyStunFrames = offensiveControlEffectsAllowed
        && attackerCombo.stunChancePct > 0
        && random.chance(attackerCombo.stunChancePct)
        ? attackerCombo.stunFrames
        : 0;
    if (legacyStunFrames > 0)
    {
        BattleDamageRequest request;
        request.frozenFrames = legacyStunFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.logEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            logStatusFrames("眩暈", legacyStunFrames)));
    }

    if (offensiveControlEffectsAllowed)
    {
        auto hitStunEvents = BattleComboTriggerSystem().collectTriggerEvents(
            attackerCombo,
            { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
            { EffectType::Stun },
            random,
            BattleComboActivationRecording::CallerRecords);
        for (const auto& event : hitStunEvents)
        {
            BattleDamageRequest request;
            request.frozenFrames = event.effect.value;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                logStatusFrames("眩暈", event.effect.value)));
            BattleComboTriggerSystem().recordActivation(
                attackerCombo,
                static_cast<size_t>(event.effectIndex));
        }
    }

    const int knockbackChancePct = sumAlwaysEffectValue(attackerCombo, EffectType::KnockbackChance);
    if (knockbackChancePct > 0 && random.chance(knockbackChancePct))
    {
        auto procVelocity = input.defender.motion.position - input.attacker.motion.position;
        procVelocity.normTo(5.0f);
        result.commands.push_back(BattleKnockbackCommand{
            input.defender.id,
            procVelocity,
            0.0,
        });
    }

    const int offensiveCharmChancePct = maxAlwaysEffectValue(attackerCombo, EffectType::OffensiveCharm);
    const auto* offensiveCharm = firstAlwaysEffect(attackerCombo, EffectType::CharmCDRDebuff);
    const int offensiveCooldownExtendPct = offensiveCharmChancePct > 0
        && offensiveCharm
        && offensiveCharm->value2 > 0
        && random.chance(offensiveCharmChancePct)
        ? offensiveCharm->value2
        : 0;
    if (offensiveCooldownExtendPct > 0)
    {
        BattleDamageRequest request;
        request.cooldownExtendPct = offensiveCooldownExtendPct;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));

        auto cooldown = BattleDamageSystem().extendActiveCooldown(
            makeCooldownState(input.defender),
            offensiveCooldownExtendPct);
        if (cooldown.increased)
        {
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatCooldownIncreaseStatus(offensiveCooldownExtendPct, cooldown.before, cooldown.after)));
        }
    }

    auto teamHeal = BattleComboTriggerSystem().collectTriggeredTeamHeal(
        attackerCombo,
        { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
        random);
    if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
    {
        result.commands.push_back(BattleTeamHealCommand{
            input.attacker.id,
            teamHeal.flatHeal,
            teamHeal.pctHeal,
            "命中群療",
        });
    }

    auto defenderCombo = input.defenderCombo;
    const bool reflectableProjectile =
        input.attackEvent.operationType == BattleOperationType::RangedProjectile
        || input.attackEvent.operationType == BattleOperationType::TrackingProjectile;
    const bool usingHpDamage = input.skill.hurtType == 0;

    result.shapedHpDamage = BattleDamageSystem().applyModifiers({
        result.shapedHpDamage,
        false,
        false,
        {},
        makeBattleDamageModifierState(&defenderCombo),
        makeDamageUnit(input.defender, &defenderCombo, &input.defenderStatusEffects),
    }).damage;

    auto defenderDamage = BattleComboTriggerSystem().shapeDefenderHitDamage(
        defenderCombo,
        { result.shapedHpDamage, input.defender.vitals.hp, input.defender.vitals.maxHp, defenderCombo.lastAliveFlag, input.attacker.id });
    result.shapedHpDamage = defenderDamage.damage;
    for (const auto& damageEvent : defenderDamage.events)
    {
        switch (damageEvent.type)
        {
        case BattleDefenderHitDamageEventType::DamageAdaptationStack:
            result.logEvents.push_back(statusEvent(
                input.defender.id,
                input.attacker.id,
                formatStackingEffectStatus("同敵減傷", damageEvent.value, damageEvent.value2)));
            break;
        case BattleDefenderHitDamageEventType::DodgeAdaptationStack:
            result.logEvents.push_back(statusEvent(
                input.defender.id,
                input.attacker.id,
                formatStackingEffectStatus("同敵閃避", damageEvent.value, damageEvent.value2)));
            break;
        default:
            assert(false);
        }
    }

    BattleDamageModifierState lateAttackerModifier;
    lateAttackerModifier.poisonDamageAmpPct = sumAlwaysEffectValue(attackerCombo, EffectType::PoisonDmgAmp);
    BattleDamageModifierState lateDefenderModifier;
    lateDefenderModifier.poisonTimer = input.defenderStatusEffects.poisonTimer;
    lateDefenderModifier.maxHitPctMaxHp = maxAlwaysEffectValue(defenderCombo, EffectType::MaxHitPctCurrentHP);
    auto lateDamage = BattleDamageSystem().applyModifiers({
        result.shapedHpDamage,
        false,
        true,
        lateAttackerModifier,
        lateDefenderModifier,
        makeDamageUnit(input.defender, &defenderCombo, &input.defenderStatusEffects),
    });
    result.shapedHpDamage = lateDamage.damage;
    if (lateDamage.maxHitCapped)
    {
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            std::format("單次承傷封頂{}%最大生命", lateDamage.maxHitPct)));
    }

    const auto* defensiveCharm = firstAlwaysEffect(defenderCombo, EffectType::CharmCDRDebuff);
    const int defensiveCooldownExtendPct = defensiveCharm
        && defensiveCharm->value > 0
        && defensiveCharm->value2 > 0
        && random.chance(defensiveCharm->value)
        ? defensiveCharm->value2
        : 0;
    if (defensiveCooldownExtendPct > 0)
    {
        BattleDamageRequest request;
        request.cooldownExtendPct = defensiveCooldownExtendPct;
        result.commands.push_back(acceptedHitCommand(input.defender.id, input.attacker.id, request));

        auto cooldown = BattleDamageSystem().extendActiveCooldown(
            makeCooldownState(input.attacker),
            defensiveCooldownExtendPct);
        if (cooldown.increased)
        {
            result.logEvents.push_back(statusEvent(
                input.defender.id,
                input.attacker.id,
                formatCooldownIncreaseStatus(defensiveCooldownExtendPct, cooldown.before, cooldown.after)));
        }
    }

    auto beingHitStunEvents = BattleComboTriggerSystem().collectTriggerEvents(
        defenderCombo,
        { BattleComboTriggerHook::DamageTaken, input.defender.id, input.attacker.id },
        { EffectType::Stun },
        random,
        BattleComboActivationRecording::CallerRecords);
    for (const auto& event : beingHitStunEvents)
    {
        BattleDamageRequest request;
        request.frozenFrames = event.effect.value;
        result.commands.push_back(acceptedHitCommand(input.defender.id, input.attacker.id, request));
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            logStatusFrames("反制並眩暈對手", event.effect.value)));
        BattleComboTriggerSystem().recordActivation(
            defenderCombo,
            static_cast<size_t>(event.effectIndex));
    }

    result.reflected = BattleComboTriggerSystem().resolveProjectileReflect(
        defenderCombo,
        reflectableProjectile,
        random);
    if (result.reflected)
    {
        result.visualEvents.push_back(floatingTextEvent(
            input.defender.id,
            "彈反",
            { 180, 150, 255, 255 },
            24));
        result.logEvents.push_back(sourceStatusEvent(input.defender.id, input.attacker.id, "彈反了遠程攻擊"));
    }

    const bool canTriggerExecute = !result.reflected
        && usingHpDamage
        && BattleComboTriggerSystem().hasExecuteCombo(attackerCombo, input.attacker.id);
    const bool canTriggerDefenderBlock = !result.reflected;

    std::string damageDetail;
    if (result.critical)
    {
        damageDetail = appendDetail(std::move(damageDetail), "暴擊");
    }
    if (result.reflected)
    {
        damageDetail = appendDetail(std::move(damageDetail), "彈反");
    }
    if (auto label = projectileSourceLabel(input.attackEvent); !label.empty())
    {
        damageDetail = appendDetail(std::move(damageDetail), label);
    }

    const int skillReflectPct = maxAlwaysEffectValue(defenderCombo, EffectType::SkillReflectPct);
    if (!result.reflected && usingSkill && skillReflectPct > 0)
    {
        int reflectedDamage = static_cast<int>(result.shapedHpDamage * skillReflectPct / 100.0);
        if (reflectedDamage > 0)
        {
            result.commands.push_back(BattleHpDamageCommand{
                input.defender.id,
                input.attacker.id,
                reflectedDamage,
                false,
                false,
                false,
                false,
                0,
                "",
                battleLogText("技能反彈", BattleLogTextTone::SkillName),
                false,
            });
        }
    }

    if (!result.reflected)
    {
        BattleBleedProc bleedProc;
        const auto* bleed = firstAlwaysEffect(attackerCombo, EffectType::BleedChance);
        const int bleedChancePct = sumAlwaysEffectValue(attackerCombo, EffectType::BleedChance);
        bleedProc.applies = result.shapedHpDamage > 0
            && bleedChancePct > 0
            && random.chance(bleedChancePct);
        if (bleedProc.applies)
        {
            bleedProc.stacks = 1;
            bleedProc.maxStacks = bleed && bleed->value2 > 0 ? bleed->value2 : 5;
        }
        if (bleedProc.applies)
        {
            BattleDamageRequest request;
            request.bleedStacks = bleedProc.stacks;
            request.bleedMaxStacks = bleedProc.maxStacks;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        }

        BattleDamageReduceDebuffProc damageReduceDebuff;
        const auto* alwaysDamageReduceDebuff = firstAlwaysEffect(attackerCombo, EffectType::DmgReduceDebuff);
        if (result.shapedHpDamage > 0 && alwaysDamageReduceDebuff && alwaysDamageReduceDebuff->value2 > 0)
        {
            damageReduceDebuff.applies = true;
            damageReduceDebuff.pct = alwaysDamageReduceDebuff->value;
            damageReduceDebuff.durationFrames = alwaysDamageReduceDebuff->value2;
        }
        auto damageReduceEvents = BattleComboTriggerSystem().collectTriggerEvents(
            attackerCombo,
            { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
            { EffectType::DmgReduceDebuff },
            random);
        if (!damageReduceDebuff.applies && !damageReduceEvents.empty())
        {
            const auto& effect = damageReduceEvents.front().effect;
            if (effect.value2 > 0)
            {
                damageReduceDebuff.applies = true;
                damageReduceDebuff.pct = effect.value;
                damageReduceDebuff.durationFrames = effect.value2;
            }
        }
        if (damageReduceDebuff.applies)
        {
            BattleDamageRequest request;
            request.damageReduceDebuffDurationFrames = damageReduceDebuff.durationFrames;
            request.damageReduceDebuffPct = damageReduceDebuff.pct;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusPercentFrames("傷害降低", damageReduceDebuff.pct, damageReduceDebuff.durationFrames)));
        }

        auto mpBlockEvents = BattleComboTriggerSystem().collectTriggerEvents(
            attackerCombo,
            { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
            { KysChess::EffectType::MPBlock },
            random);
        for (const auto& mpBlock : mpBlockEvents)
        {
            BattleDamageRequest request;
            request.mpBlockFrames = mpBlock.effect.value;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                logStatusFrames("封內", mpBlock.effect.value)));
        }

        std::vector<BattleComboTriggerEvent> followUpEvents;
        if (!input.attackEvent.suppressNearbyTrackingProjectileProc)
        {
            followUpEvents = BattleComboTriggerSystem().collectTriggerEvents(
                attackerCombo,
                { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
                { KysChess::EffectType::NearbyTrackingProjectiles },
                random);
        }

        const double attackerProjectileSpeed = pointMagnitude(input.attackEvent.velocity) > 0.01
            ? pointMagnitude(input.attackEvent.velocity)
            : 0.0;
        for (const auto& followUp : followUpEvents)
        {
            assert(followUp.effect.value > 0);
            switch (followUp.effect.type)
            {
            case KysChess::EffectType::NearbyTrackingProjectiles:
                result.commands.push_back(BattleNearbyTrackingProjectilesCommand{
                    input.attackEvent,
                    input.defender.id,
                    followUp.effect.value,
                    followUp.effect.value2 > 0 ? followUp.effect.value2 : 40,
                    attackerProjectileSpeed,
                });
                break;
            default:
                assert(false);
            }
        }
    }

    if (usingHpDamage && result.shapedHpDamage > 0)
    {
        int damage = static_cast<int>(result.shapedHpDamage) + input.randomDamageVariance;
        damage = std::max(0, damage);
        if (damage > 0)
        {
            const int sourceUnitId = result.reflected ? input.defender.id : input.attacker.id;
            const int targetUnitId = result.reflected ? input.attacker.id : input.defender.id;
            result.commands.push_back(BattleHpDamageCommand{
                sourceUnitId,
                targetUnitId,
                damage,
                result.critical,
                input.attackEvent.ultimate,
                canTriggerExecute,
                canTriggerDefenderBlock,
                !result.reflected ? impactFrozenFrames : 0,
                input.skill.name,
                battleLogText(damageDetail, BattleLogTextTone::SkillName),
                !result.reflected,
            });
            result.finalHpDamage = damage;
        }
    }
    else if (!usingHpDamage && result.shapedHpDamage > 0)
    {
        int damage = static_cast<int>(result.shapedHpDamage) + input.randomDamageVariance;
        damage = std::max(0, damage);
        if (damage > 0)
        {
            BattleDamageRequest request;
            request.mpDamage = damage;
            request.mpOnHit = static_cast<int>(damage * 0.8);
            request.frozenFrames = !result.reflected ? impactFrozenFrames : 0;
            const int sourceUnitId = result.reflected ? input.defender.id : input.attacker.id;
            const int targetUnitId = result.reflected ? input.attacker.id : input.defender.id;
            result.commands.push_back(BattleMpDamageCommand{
                sourceUnitId,
                targetUnitId,
                request,
                canTriggerDefenderBlock,
            });
            result.finalMpDamage = damage;
        }
    }

    result.attackerCombo = std::move(attackerCombo);
    result.defenderCombo = std::move(defenderCombo);
    if (!usingHpDamage)
    {
        result.finalHpDamage = 0;
    }
    return result;
}

}  // namespace KysChess::Battle
