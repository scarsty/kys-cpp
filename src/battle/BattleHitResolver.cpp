#include "BattleHitResolver.h"

#include "BattleLogSegments.h"
#include "BattleComboTriggerSystem.h"
#include "BattleRuntimeRandom.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <format>
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
        damageUnit.mpRecoveryBonusPct = combo->sumAlways(EffectType::MPRecoveryBonus);
    }
    if (effects)
    {
        damageUnit.mpBlocked = effects->mpBlockTimer > 0;
    }
    return damageUnit;
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
    request.initial.skillEffectRef = command.prototype.skillEffectRef;
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
    const BattleRuntimeUnits& units)
{
    const auto& source = units.requireCore(sourceUnitId);
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

BattleDamageModifierState makeDamageModifierState(const BattleEffectSources& sources)
{
    BattleDamageModifierState modifier;
    BattleEffectReader reader;
    modifier.flatDamageIncrease = reader.sumAlways(sources, EffectType::FlatDmgIncrease);
    modifier.skillDamagePct = reader.sumAlways(sources, EffectType::SkillDmgPct);
    modifier.poisonDamageAmpPct = reader.sumAlways(sources, EffectType::PoisonDmgAmp);
    modifier.flatDamageReduction = reader.sumAlways(sources, EffectType::FlatDmgReduction);
    modifier.damageReductionPct = reader.sumAlways(sources, EffectType::DmgReductionPct);
    modifier.maxHitPctMaxHp = reader.maxAlways(sources, EffectType::MaxHitPctCurrentHP);
    return modifier;
}

BattleComboTriggerInput damageDealtTriggerInput(const BattleHitResolutionInput& input)
{
    return {
        BattleComboTriggerHook::DamageDealt,
        input.attacker.id,
        input.defender.id,
        input.attackEvent.ultimate,
        input.attackEvent.mainProjectile,
    };
}

int resolveOffensiveCooldownExtendPct(
    const BattleEffectSources& attackerSources,
    const BattleComboTriggerInput& input,
    BattleRuntimeRandom& random)
{
    const BattleEffectState* selectedState = nullptr;
    int selectedChancePct = 0;
    for (const auto& source : orderedBattleEffectSources(attackerSources))
    {
        if (!source.state)
        {
            continue;
        }
        const int chancePct = source.state->maxAlways(EffectType::OffensiveCharm);
        if (chancePct > selectedChancePct)
        {
            selectedChancePct = chancePct;
            selectedState = source.state;
        }
    }
    if (selectedState && selectedChancePct > 0 && passesPercentChance(random, selectedChancePct))
    {
        const auto* charm = selectedState->firstAlways(EffectType::CharmCDRDebuff);
        if (charm && charm->value2 > 0)
        {
            return charm->value2;
        }
    }

    for (const auto& event : BattleEffectReader().matchingTriggerEvents(
             attackerSources,
             input,
             { EffectType::OffensiveCharm }))
    {
        auto source = battleEffectSourceForStore(attackerSources, event.effectRef.store);
        assert(source.state != nullptr);
        if (!source.state->canActivateTriggeredEffect(event.effectRef.localId))
        {
            continue;
        }
        if (!passesPercentChance(random, event.effect.triggerValue)
            || !passesPercentChance(random, event.effect.value))
        {
            continue;
        }

        int cooldownExtendPct = 0;
        for (RoleComboEffectId id : source.state->effectIds(event.effect.trigger, EffectType::CharmCDRDebuff))
        {
            const auto& debuff = source.state->effect(id);
            if (debuff.value == event.effect.value
                && debuff.triggerValue == event.effect.triggerValue
                && debuff.value2 > 0)
            {
                cooldownExtendPct = debuff.value2;
                break;
            }
        }
        if (cooldownExtendPct <= 0)
        {
            continue;
        }

        BattleEffectCommands().recordActivation(attackerSources, event.effectRef);
        return cooldownExtendPct;
    }

    return 0;
}

bool hasExecuteEffect(
    const BattleEffectSources& attackerSources,
    const BattleComboTriggerInput& input)
{
    for (const auto& source : orderedBattleEffectSources(attackerSources))
    {
        if (!source.state)
        {
            continue;
        }
        for (RoleComboEffectId id : source.state->effectIds(Trigger::OnHit, EffectType::Execute))
        {
            if (!input.mainProjectile)
            {
                continue;
            }
            const auto& effect = source.state->effect(id);
            if (effect.triggerValue > 0 && effect.value > 0)
            {
                return true;
            }
        }
    }
    return false;
}

struct BattlePoisonEffectSummary
{
    int pct{};
    int durationFrames{};
};

BattlePoisonEffectSummary resolvePoisonEffectSummary(
    const BattleEffectSources& sources,
    const BattleComboTriggerInput& input,
    BattleRuntimeRandom& random)
{
    BattlePoisonEffectSummary result;
    BattleEffectReader reader;
    result.pct = reader.sumAlways(sources, EffectType::PoisonDOT);
    result.durationFrames = reader.maxAlwaysValue2(sources, EffectType::PoisonDOT) * 30;

    for (const auto& event : reader.collectTriggerEvents(
             sources,
             input,
             { EffectType::PoisonDOT },
             random,
             BattleComboActivationRecording::CallerRecords))
    {
        if (event.effect.value <= 0)
        {
            continue;
        }
        result.pct += event.effect.value;
        result.durationFrames = std::max(result.durationFrames, event.effect.value2 * 30);
        BattleEffectCommands().recordActivation(sources, event.effectRef);
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

BattleBleedEffectSummary resolveBattleBleedEffectSummary(const BattleEffectSources& sources)
{
    BattleBleedEffectSummary result;
    int maxStacks = 0;
    for (const auto& source : orderedBattleEffectSources(sources))
    {
        if (!source.state)
        {
            continue;
        }
        for (RoleComboEffectId id : source.state->effectIds(Trigger::Always, EffectType::BleedChance))
        {
            const auto& effect = source.state->effect(id);
            if (effect.value <= 0)
            {
                continue;
            }
            result.hasBleedChance = true;
            result.chancePct += effect.value;
            maxStacks = std::max(maxStacks, effect.value2 > 0 ? effect.value2 : 5);
        }
    }
    if (result.hasBleedChance)
    {
        result.maxStacks = std::max(1, maxStacks);
    }
    return result;
}

BattleProjectileFollowUpExpansion expandBattleProjectileFollowUpCommands(
    const std::vector<BattleGameplayCommand>& commands,
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
                    makeNearbyFollowUpSpawn(
                        *nearby,
                        units.requireCore(targetId),
                        context),
                    "範圍追蹤彈",
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
            makeAreaFollowUpSpawn(
                followUp.sourceUnitId,
                targetId,
                followUp.damage,
                followUp.stunFrames,
                followUp.effectId,
                context,
                units),
            followUp.reason,
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
    RoleComboState& attackerCombo,
    RoleComboState& defenderCombo,
    BattleRuntimeRandom& random) const
{
    BattleEffectSources attackerSources;
    attackerSources.combo = { { BattleEffectSourceKind::Combo, BattleSkillSlot::None }, &attackerCombo };
    BattleEffectSources defenderSources;
    defenderSources.combo = { { BattleEffectSourceKind::Combo, BattleSkillSlot::None }, &defenderCombo };
    return resolve(input, attackerSources, defenderSources, random);
}

BattleHitResolutionResult BattleHitResolver::resolve(
    const BattleHitResolutionInput& input,
    BattleEffectSources attackerSources,
    BattleEffectSources defenderSources,
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

    assert(attackerSources.combo.state != nullptr);
    assert(defenderSources.combo.state != nullptr);
    auto& attackerCombo = *static_cast<RoleComboState*>(attackerSources.combo.state);
    auto& defenderCombo = *static_cast<RoleComboState*>(defenderSources.combo.state);
    BattleEffectReader effectReader;
    BattleEffectCommands effectCommands;

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

    BattleHitShapeInput shapeInput;
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

    const auto shaped = BattleDamageSystem().shapeHitDamage(shapeInput);
    result.shapedHpDamage = shaped.damage;

    if (shaped.frozenFrames > 0)
    {
        BattleDamageRequest request;
        request.hitstunFrames = shaped.frozenFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
    }

    auto hitVelocity = knockbackDirection(input.attacker, input.defender);
    hitVelocity.normTo(1.0f);
    result.commands.push_back(BattleKnockbackCommand{
        input.defender.id,
        hitVelocity,
        1.0,
        1,
    });

    const int mpRatioDmgBoostPct = effectReader.sumAlways(attackerSources, EffectType::MPRatioDmgBoost);
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
        makeDamageModifierState(attackerSources),
        {},
        makeDamageUnit(input.defender, nullptr, &input.defenderStatusEffects),
    }).damage;

    auto attackerDamage = BattleComboTriggerSystem().shapeAttackerHitDamage(
        attackerCombo,
        { result.shapedHpDamage, input.attacker.vitals.hp, input.attacker.vitals.maxHp, attackerCombo.lastAliveForComboRuntime() },
        random);
    result.shapedHpDamage = attackerDamage.damage;
    for (const auto& damageEvent : attackerDamage.events)
    {
        switch (damageEvent.type)
        {
        case BattleAttackerHitDamageEventType::Crit:
            result.critical = true;
            result.criticalMultiplier = damageEvent.value;
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

    const int mpOnHit = effectReader.sumAlways(attackerSources, EffectType::MPOnHit);
    const int hpOnHit = effectReader.sumAlways(attackerSources, EffectType::HPOnHit);
    const int mpDrain = effectReader.sumAlways(attackerSources, EffectType::MPDrain);
    if (mpOnHit > 0 || hpOnHit > 0 || mpDrain > 0)
    {
        BattleDamageRequest request;
        request.mpOnHit = mpOnHit;
        request.hpOnHit = hpOnHit;
        request.mpDrain = mpDrain;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));

    }

    const auto poison = resolvePoisonEffectSummary(
        attackerSources,
        damageDealtTriggerInput(input),
        random);
    if (poison.pct > 0)
    {
        BattleDamageRequest request;
        request.poisonPct = poison.pct;
        request.poisonDurationFrames = poison.durationFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        auto poisonLog = statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusPercentFrames("中毒", poison.pct, poison.durationFrames));
        poisonLog.statusId = BattleStatusSemanticId::PoisonPayload;
        poisonLog.amount = poison.pct;
        poisonLog.secondaryAmount = poison.durationFrames / 30;
        result.logEvents.push_back(std::move(poisonLog));
    }

    const bool offensiveControlEffectsAllowed = input.attackEvent.mainProjectile;
    int alwaysStunFrames = 0;
    if (offensiveControlEffectsAllowed)
    {
        for (RoleComboEffectId effectId : attackerCombo.effectIds(Trigger::Always, EffectType::Stun))
        {
            const auto& effect = attackerCombo.effect(effectId);
            const int chancePct = effect.triggerValue > 0 ? effect.triggerValue : 100;
            if (chancePct > 0 && random.chance(chancePct))
            {
                alwaysStunFrames = std::max(alwaysStunFrames, effect.value);
            }
        }
    }
    if (alwaysStunFrames > 0)
    {
        BattleDamageRequest request;
        request.stunFrames = alwaysStunFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.logEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            logStatusFrames("眩暈", alwaysStunFrames)));
    }

    if (offensiveControlEffectsAllowed)
    {
        auto hitStunEvents = effectReader.collectTriggerEvents(
            attackerSources,
            damageDealtTriggerInput(input),
            { EffectType::Stun },
            random,
            BattleComboActivationRecording::CallerRecords);
        for (const auto& event : hitStunEvents)
        {
            BattleDamageRequest request;
            request.stunFrames = event.effect.value;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.logEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                logStatusFrames("眩暈", event.effect.value)));
            effectCommands.recordActivation(attackerSources, event.effectRef);
        }
    }

    if (input.attackEvent.mainProjectile)
    {
        for (RoleComboEffectId effectId : attackerCombo.effectIds(Trigger::Always, EffectType::KnockbackChance))
        {
            const auto& effect = attackerCombo.effect(effectId);
            if (effect.value <= 0 || !random.chance(effect.value))
            {
                continue;
            }
            const int frames = std::max(1, effect.duration > 0 ? effect.duration : 3);
            const int distance = effect.value2 > 0 ? effect.value2 : 5;
            auto procDirection = knockbackDirection(input.attacker, input.defender);
            procDirection.normTo(1.0f);
            result.commands.push_back(BattleKnockbackCommand{
                input.defender.id,
                procDirection,
                static_cast<double>(distance),
                frames,
            });
            auto knockbackLog = statusEvent(
                input.attacker.id,
                input.defender.id,
                std::format("擊退（{}距離·{}幀）", distance, frames));
            knockbackLog.statusId = BattleStatusSemanticId::Knockback;
            knockbackLog.amount = distance;
            knockbackLog.secondaryAmount = frames;
            result.logEvents.push_back(std::move(knockbackLog));
        }
    }

    const int offensiveCooldownExtendPct = resolveOffensiveCooldownExtendPct(
        attackerSources,
        damageDealtTriggerInput(input),
        random);
    if (offensiveCooldownExtendPct > 0)
    {
        BattleDamageRequest request;
        request.cooldownExtendPct = offensiveCooldownExtendPct;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));

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
        { result.shapedHpDamage, input.defender.vitals.hp, input.defender.vitals.maxHp, defenderCombo.lastAliveForComboRuntime(), input.attacker.id });
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
    lateAttackerModifier.poisonDamageAmpPct = effectReader.sumAlways(attackerSources, EffectType::PoisonDmgAmp);
    BattleDamageModifierState lateDefenderModifier;
    lateDefenderModifier.poisonTimer = input.defenderStatusEffects.poisonTimer;
    lateDefenderModifier.maxHitPctMaxHp = defenderCombo.maxAlways(EffectType::MaxHitPctCurrentHP);
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

    const auto* defensiveCharm = defenderCombo.firstAlways(EffectType::CharmCDRDebuff);
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
        request.stunFrames = event.effect.value;
        result.commands.push_back(acceptedHitCommand(input.defender.id, input.attacker.id, request));
        result.logEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            logStatusFrames("反制並眩暈對手", event.effect.value)));
        BattleComboTriggerSystem().recordActivation(
            defenderCombo,
            event.effectId);
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
        && hasExecuteEffect(attackerSources, damageDealtTriggerInput(input));
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

    const int skillReflectPct = defenderCombo.maxAlways(EffectType::SkillReflectPct);
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
        const auto bleed = resolveBattleBleedEffectSummary(attackerSources);
        bleedProc.applies = result.shapedHpDamage > 0
            && passesPercentChance(random, bleed.chancePct);
        if (bleedProc.applies)
        {
            bleedProc.stacks = 1;
            bleedProc.maxStacks = bleed.maxStacks;
        }
        if (bleedProc.applies)
        {
            BattleDamageRequest request;
            request.bleedStacks = bleedProc.stacks;
            request.bleedMaxStacks = bleedProc.maxStacks;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        }

        BattleDamageReduceDebuffProc damageReduceDebuff;
        const auto* alwaysDamageReduceDebuff = effectReader.firstAlways(attackerSources, EffectType::DmgReduceDebuff);
        if (result.shapedHpDamage > 0 && alwaysDamageReduceDebuff && alwaysDamageReduceDebuff->value2 > 0)
        {
            damageReduceDebuff.applies = true;
            damageReduceDebuff.pct = alwaysDamageReduceDebuff->value;
            damageReduceDebuff.durationFrames = alwaysDamageReduceDebuff->value2;
        }
        auto damageReduceEvents = effectReader.collectTriggerEvents(
            attackerSources,
            damageDealtTriggerInput(input),
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

        auto mpBlockEvents = effectReader.collectTriggerEvents(
            attackerSources,
            damageDealtTriggerInput(input),
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

        std::vector<BattleEffectTriggerEvent> followUpEvents;
        if (!input.attackEvent.suppressNearbyTrackingProjectileProc)
        {
            followUpEvents = effectReader.collectTriggerEvents(
                attackerSources,
                damageDealtTriggerInput(input),
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
            BattleHpDamageCommand command{
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
            };
            command.criticalMultiplier = result.criticalMultiplier;
            command.skillId = input.skill.id;
            if (!result.reflected)
            {
                command.skillEffectRef = input.attackEvent.skillEffectRef;
            }
            result.commands.push_back(std::move(command));
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
            request.hitstunFrames = !result.reflected ? impactFrozenFrames : 0;
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

    if (!usingHpDamage)
    {
        result.finalHpDamage = 0;
    }
    return result;
}

}  // namespace KysChess::Battle
