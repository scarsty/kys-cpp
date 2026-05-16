#include "BattleHitResolver.h"

#include "../ChessEftIds.h"
#include "BattleComboTriggerSystem.h"
#include "BattleCore.h"
#include "BattleStatusFormat.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>

namespace KysChess::Battle
{
namespace
{

constexpr int RoleStatusEffectFrames = 45;
constexpr double FollowUpPi = 3.14159265358979323846;

double pointMagnitude(const Pointf& point)
{
    return std::sqrt(
        static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

std::string formatStatusFrames(const char* label, int frames)
{
    if (frames <= 0)
    {
        return label;
    }
    return std::format("{}（{}幀）", label, frames);
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
    return formatStatusFrames(label, frames);
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

std::string formatExecuteStatus(int thresholdPct)
{
    if (thresholdPct <= 0)
    {
        return "觸發處決";
    }
    return std::format("觸發處決（斬殺線{}%）", thresholdPct);
}

BattleDamageUnitState makeDamageUnit(const BattleHitUnitSnapshot& unit, const RoleComboState* combo)
{
    BattleDamageUnitState damageUnit;
    damageUnit.id = unit.id;
    damageUnit.alive = unit.alive;
    damageUnit.vitals = unit.vitals;
    damageUnit.attack = unit.stats.attack;
    damageUnit.invincible = unit.invincible;
    if (combo)
    {
        damageUnit.mpBlocked = combo->mpBlockTimer > 0;
        damageUnit.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
    }
    return damageUnit;
}

BattleResourceUnitState makeResourceUnit(const BattleHitUnitSnapshot& unit, const RoleComboState* combo)
{
    BattleResourceUnitState resource;
    resource.id = unit.id;
    resource.alive = unit.alive;
    resource.vitals = unit.vitals;
    if (combo)
    {
        resource.mpBlocked = combo->mpBlockTimer > 0;
        resource.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
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
    event.text = std::move(text);
    return event;
}

BattleLogEvent healEvent(int sourceUnitId, int targetUnitId, int amount, std::string reason)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.text = std::move(reason);
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
        if (const auto* currentHp = std::get_if<BattleCurrentHpBlastCommand>(&command))
        {
            const auto& source = units.requireUnit(currentHp->sourceUnitId);
            for (const auto& unit : units.units)
            {
                if (!unit.alive || unit.team == source.team)
                {
                    continue;
                }
                expansion.commands.push_back(BattleHpDamageCommand{
                    currentHp->sourceUnitId,
                    unit.id,
                    std::max(1, unit.vitals.hp * currentHp->damagePct / 100),
                    false,
                    false,
                    false,
                    0,
                    "",
                    currentHp->reason,
                    false,
                });
            }
            continue;
        }
        if (const auto* spiral = std::get_if<BattleSpiralBleedProjectileCommand>(&command))
        {
            const auto& source = units.requireUnit(spiral->sourceUnitId);
            const auto sourcePosition = source.motion.position;
            const int sharedHitGroupId = context.nextSharedHitGroupId++;
            const int projectileCount = std::max(1, spiral->projectileCount);
            const double projectileSpeed = spiral->projectileSpeed > 0.0
                ? spiral->projectileSpeed
                : context.projectileSpeed;
            for (int i = 0; i < projectileCount; ++i)
            {
                BattleAttackSpawnRequest request;
                request.initial.attackerUnitId = spiral->sourceUnitId;
                request.initial.operationType = BattleOperationType::RangedProjectile;
                request.initial.visualEffectId = 48;
                request.initial.position = sourcePosition;
                request.initial.totalFrame = 35;
                request.initial.scriptedBleedStacks = spiral->bleedStacks;
                request.initial.sharedHitGroupId = sharedHitGroupId;
                request.initial.ignoreProjectileCancel = true;
                request.initial.through = true;
                request.spiralMotion = true;
                request.spiralCenter = sourcePosition;
                request.spiralRadius = 0.0f;
                request.spiralRadiusGrowth = static_cast<float>(projectileSpeed * 0.9);
                request.spiralAngle = static_cast<float>(2.0 * FollowUpPi * i / projectileCount);
                request.spiralAngularVelocity = 0.42f;
                expansion.commands.push_back(BattleProjectileSpawnCommand{ std::move(request), "螺旋流血彈" });
            }
            continue;
        }
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
        if (const auto* extra = std::get_if<BattleHitExtraProjectilesCommand>(&command))
        {
            for (int i = 0; i < extra->extraCount; ++i)
            {
                BattleAttackSpawnRequest request;
                request.initial.attackerUnitId = extra->prototype.sourceUnitId;
                request.initial.skillId = extra->prototype.skillId;
                request.initial.preferredTargetUnitId = extra->targetUnitId;
                request.initial.totalFrame = extra->prototype.totalFrame;
                request.initial.track = extra->prototype.track
                    || extra->prototype.operationType == BattleOperationType::Melee
                    || extra->prototype.operationType == BattleOperationType::Dash;
                request.initial.through = extra->prototype.through;
                request.initial.ultimate = extra->prototype.ultimate;
                request.initial.ignoreProjectileCancel = extra->prototype.skillId < 0;
                request.initial.sharedHitGroupId = extra->prototype.sharedHitGroupId;
                request.initial.visualEffectId = extra->prototype.visualEffectId;
                request.initial.operationType = extra->prototype.operationType;
                request.initial.scriptedDamage = extra->prototype.scriptedDamage;
                request.initial.scriptedStunFrames = extra->prototype.scriptedStunFrames;
                request.initial.scriptedBleedStacks = extra->prototype.scriptedBleedStacks;
                request.initial.strengthMultiplier = extra->prototype.strengthMultiplier;
                request.initial.suppressNearbyTrackingProjectileProc = extra->prototype.suppressNearbyTrackingProjectileProc;
                request.initial.mainProjectile = false;
                request.initial.position = extra->prototype.position;
                auto prototypeVelocity = extra->prototype.velocity;
                const double prototypeSpeed = prototypeVelocity.norm();
                request.initial.velocity = normalizedFollowUpVelocity(
                    extra->prototype.position,
                    units.requireUnit(extra->targetUnitId).motion.position,
                    prototypeSpeed > 0.01 ? prototypeSpeed : context.projectileSpeed);
                expansion.commands.push_back(BattleProjectileSpawnCommand{ std::move(request), "命中追加彈" });
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
                    formatStatusFrames("眩暈", input.attackEvent.scriptedStunFrames)));
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
                0,
                "",
                "特效傷害",
            });
            result.finalHpDamage = input.attackEvent.scriptedDamage;
        }
        if (input.attacker.id >= 0)
        {
            result.commands.push_back(BattleLastAttackerCommand{ input.defender.id, input.attacker.id });
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
    if (usingSkill && input.attacker.vitals.maxMp > 0 && attackerCombo.mpRatioDmgBoostPct > 0)
    {
        const double mpRatio = static_cast<double>(input.attacker.vitals.mp) / input.attacker.vitals.maxMp;
        const double boostPct = mpRatio * attackerCombo.mpRatioDmgBoostPct;
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
        makeDamageUnit(input.defender, nullptr),
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

    if (attackerCombo.mpOnHit > 0 || attackerCombo.hpOnHit > 0 || attackerCombo.mpDrain > 0)
    {
        BattleDamageRequest request;
        request.mpOnHit = attackerCombo.mpOnHit;
        request.hpOnHit = attackerCombo.hpOnHit;
        request.mpDrain = attackerCombo.mpDrain;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));

        auto resources = BattleDamageSystem().applyOnHitResources({
            makeResourceUnit(input.attacker, &attackerCombo),
            makeResourceUnit(input.defender, &input.defenderCombo),
            attackerCombo.mpOnHit,
            attackerCombo.hpOnHit,
            attackerCombo.mpDrain,
        });
        if (resources.hpHealed > 0)
        {
            result.visualEvents.push_back(roleEffectEvent(input.attacker.id, KysChess::EFT_HEAL, RoleStatusEffectFrames));
            result.logEvents.push_back(healEvent(input.attacker.id, input.attacker.id, resources.hpHealed, "命中回血"));
        }
    }

    if (attackerCombo.poisonDOTPct > 0)
    {
        BattleDamageRequest request;
        request.poisonPct = attackerCombo.poisonDOTPct;
        request.poisonDurationFrames = attackerCombo.poisonDuration;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.logEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusPercentFrames("中毒", attackerCombo.poisonDOTPct, attackerCombo.poisonDuration)));
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
            formatStatusFrames("眩暈", legacyStunFrames)));
    }

    if (offensiveControlEffectsAllowed)
    {
        auto hitStunCommands = BattleComboTriggerSystem().collectStunCommands(
            attackerCombo,
            { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
            random);
        for (const auto& stunCommand : hitStunCommands)
        {
            BattleDamageRequest request;
            request.frozenFrames = stunCommand.frames;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusFrames("眩暈", stunCommand.frames)));
            BattleComboTriggerSystem().recordActivation(
                attackerCombo,
                static_cast<size_t>(stunCommand.effectIndex));
        }
    }

    if (attackerCombo.knockbackChancePct > 0 && random.chance(attackerCombo.knockbackChancePct))
    {
        auto procVelocity = input.defender.motion.position - input.attacker.motion.position;
        procVelocity.normTo(5.0f);
        result.commands.push_back(BattleKnockbackCommand{
            input.defender.id,
            procVelocity,
            0.0,
        });
    }

    int offensiveCooldownExtendPct = attackerCombo.offensiveCharmChancePct > 0
        && attackerCombo.charmCDRAmountPct > 0
        && random.chance(attackerCombo.offensiveCharmChancePct)
        ? attackerCombo.charmCDRAmountPct
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
        makeDamageUnit(input.defender, &defenderCombo),
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
    lateAttackerModifier.poisonDamageAmpPct = attackerCombo.poisonDmgAmpPct;
    BattleDamageModifierState lateDefenderModifier;
    lateDefenderModifier.poisonTimer = defenderCombo.poisonTimer;
    lateDefenderModifier.maxHitPctMaxHp = defenderCombo.maxHitPctCurrentHP;
    auto lateDamage = BattleDamageSystem().applyModifiers({
        result.shapedHpDamage,
        false,
        true,
        lateAttackerModifier,
        lateDefenderModifier,
        makeDamageUnit(input.defender, &defenderCombo),
    });
    result.shapedHpDamage = lateDamage.damage;
    if (lateDamage.maxHitCapped)
    {
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            std::format("單次承傷封頂{}%最大生命", lateDamage.maxHitPct)));
    }

    int defensiveCooldownExtendPct = defenderCombo.charmCDRChancePct > 0
        && defenderCombo.charmCDRAmountPct > 0
        && random.chance(defenderCombo.charmCDRChancePct)
        ? defenderCombo.charmCDRAmountPct
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

    auto beingHitStunCommands = BattleComboTriggerSystem().collectStunCommands(
        defenderCombo,
        { BattleComboTriggerHook::DamageTaken, input.defender.id, input.attacker.id },
        random);
    for (const auto& stunCommand : beingHitStunCommands)
    {
        BattleDamageRequest request;
        request.frozenFrames = stunCommand.frames;
        result.commands.push_back(acceptedHitCommand(input.defender.id, input.attacker.id, request));
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            formatStatusFrames("反制並眩暈對手", stunCommand.frames)));
        BattleComboTriggerSystem().recordActivation(
            defenderCombo,
            static_cast<size_t>(stunCommand.effectIndex));
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
        result.logEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "彈反了遠程攻擊"));
    }

    if (!result.reflected)
    {
        auto executeResult = BattleComboTriggerSystem().resolveExecuteCombo(
            attackerCombo,
            {
                input.attacker.id,
                input.defender.id,
                input.defender.vitals.hp - input.pendingDefenderHpDamage,
                input.defender.vitals.maxHp,
                result.shapedHpDamage,
                usingHpDamage,
            },
            random);
        if (executeResult.executed)
        {
            result.executed = true;
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatExecuteStatus(executeResult.thresholdPct)));
        }
    }

    auto blockCommands = BattleComboTriggerSystem().collectDefenderBlockCommands(
        defenderCombo,
        { result.executed, result.reflected },
        random);
    for (auto blockCommand : blockCommands)
    {
        result.shapedHpDamage = 0;
        switch (blockCommand)
        {
        case BattleDefenderBlockCommand::CounterUltimateBlock:
            result.visualEvents.push_back(roleEffectEvent(input.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
            result.logEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "格擋後釋放絕招"));
            result.commands.push_back(BattleAutoUltimateCommand{ input.defender.id, false });
            break;
        case BattleDefenderBlockCommand::Block:
            result.visualEvents.push_back(roleEffectEvent(input.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
            result.logEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "格擋了本次攻擊"));
            break;
        default:
            assert(false);
        }
    }

    std::string damageDetail;
    if (result.critical)
    {
        damageDetail = appendDetail(std::move(damageDetail), "暴擊");
    }
    if (result.executed)
    {
        damageDetail = appendDetail(std::move(damageDetail), "處決");
    }
    if (result.reflected)
    {
        damageDetail = appendDetail(std::move(damageDetail), "彈反");
    }
    if (auto label = projectileSourceLabel(input.attackEvent); !label.empty())
    {
        damageDetail = appendDetail(std::move(damageDetail), label);
    }

    if (!result.reflected && usingSkill && defenderCombo.skillReflectPct > 0)
    {
        int reflectedDamage = static_cast<int>(result.shapedHpDamage * defenderCombo.skillReflectPct / 100.0);
        if (reflectedDamage > 0)
        {
            result.commands.push_back(BattleHpDamageCommand{
                input.defender.id,
                input.attacker.id,
                reflectedDamage,
                false,
                false,
                false,
                0,
                "",
                "技能反彈",
                false,
            });
        }
    }

    if (!result.reflected)
    {
        BattleBleedProc bleedProc;
        bleedProc.applies = result.shapedHpDamage > 0
            && attackerCombo.bleedChancePct > 0
            && random.chance(attackerCombo.bleedChancePct);
        if (bleedProc.applies)
        {
            bleedProc.stacks = 1;
            bleedProc.maxStacks = attackerCombo.bleedMaxStacks;
        }
        if (bleedProc.applies)
        {
            BattleDamageRequest request;
            request.bleedStacks = bleedProc.stacks;
            request.bleedMaxStacks = bleedProc.maxStacks;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        }

        BattleDamageReduceDebuffProc damageReduceDebuff;
        damageReduceDebuff.applies = result.shapedHpDamage > 0
            && attackerCombo.dmgReduceDebuffChancePct > 0
            && attackerCombo.dmgReduceDebuffDurationFrames > 0
            && random.chance(attackerCombo.dmgReduceDebuffChancePct);
        if (damageReduceDebuff.applies)
        {
            damageReduceDebuff.pct = attackerCombo.dmgReduceDebuffPct;
            damageReduceDebuff.durationFrames = attackerCombo.dmgReduceDebuffDurationFrames;
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
                formatStatusFrames("封內", mpBlock.effect.value)));
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
        if (result.executed)
        {
            damage = std::max(damage, input.defender.vitals.hp);
        }
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
                !result.reflected && result.executed,
                !result.reflected ? impactFrozenFrames : 0,
                input.skill.name,
                damageDetail,
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
            });
            result.finalMpDamage = damage;
        }
    }

    result.commands.push_back(BattleLastAttackerCommand{ input.defender.id, input.attacker.id });
    if (result.reflected)
    {
        result.commands.push_back(BattleLastAttackerCommand{ input.attacker.id, input.defender.id });
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
