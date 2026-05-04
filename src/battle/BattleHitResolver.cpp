#include "BattleHitResolver.h"

#include "../ChessEftIds.h"
#include "BattleComboTriggerSystem.h"

#include <cassert>
#include <cmath>
#include <format>

namespace KysChess::Battle
{
namespace
{

constexpr int RoleStatusEffectFrames = 45;

class BattleHitRollStream
{
public:
    explicit BattleHitRollStream(const std::vector<double>& rolls)
        : rolls_(rolls)
    {
    }

    double next()
    {
        assert(index_ < rolls_.size());
        const double roll = rolls_[index_++];
        assert(roll >= 0.0 && roll < 100.0);
        return roll;
    }

private:
    const std::vector<double>& rolls_;
    size_t index_ = 0;
};

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

BattleDamageUnitState makeDamageUnit(const BattleHitUnitSnapshot& unit, const RoleComboState* combo)
{
    BattleDamageUnitState damageUnit;
    damageUnit.id = unit.id;
    damageUnit.alive = unit.alive;
    damageUnit.hp = unit.hp;
    damageUnit.maxHp = unit.maxHp;
    damageUnit.mp = unit.mp;
    damageUnit.maxMp = unit.maxMp;
    damageUnit.attack = unit.attack;
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
    resource.hp = unit.hp;
    resource.maxHp = unit.maxHp;
    resource.mp = unit.mp;
    resource.maxMp = unit.maxMp;
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
    cooldown.cooldown = unit.cooldown;
    cooldown.cooldownMax = unit.cooldownMax;
    cooldown.haveAction = unit.haveAction;
    cooldown.operationType = unit.operationType;
    cooldown.actType = unit.actType;
    return cooldown;
}

BattlePresentationEvent statusEvent(int sourceUnitId, int targetUnitId, std::string text)
{
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::StatusLog;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.text = std::move(text);
    return event;
}

BattlePresentationEvent healEvent(int sourceUnitId, int targetUnitId, int amount, std::string reason)
{
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::HealLog;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.text = std::move(reason);
    return event;
}

BattlePresentationEvent roleEffectEvent(int targetUnitId, int effectId, int durationFrames)
{
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::RoleEffect;
    event.targetUnitId = targetUnitId;
    event.effectId = effectId;
    event.visualEffectId = effectId;
    event.durationFrames = durationFrames;
    return event;
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

}  // namespace

BattleHitResolutionResult BattleHitResolver::resolve(const BattleHitResolutionInput& input) const
{
    assert(input.attacker.id >= 0);
    assert(input.defender.id >= 0);

    BattleHitResolutionResult result;
    result.attackerCombo = input.attackerCombo;
    result.defenderCombo = input.defenderCombo;

    if (input.attackEvent.type != BattleAttackEventType::Hit)
    {
        return result;
    }

    const bool usingHiddenWeapon = input.item.id >= 0;
    const bool usingSkill = !usingHiddenWeapon && input.skill.id >= 0;
    const double baseDamage = usingHiddenWeapon
        ? input.item.resolvedDamage / 5.0
        : static_cast<double>(input.skill.resolvedBaseDamage);

    BattleLegacyHitShapeInput shapeInput;
    shapeInput.baseDamage = baseDamage;
    shapeInput.projectileCancelDamage = input.attackEvent.projectileCancelDamage;
    shapeInput.strengthMultiplier = input.attackEvent.strengthMultiplier;
    shapeInput.frame = input.attackEvent.frame;
    shapeInput.totalFrame = input.attackEvent.totalFrame;
    shapeInput.impactPosition = input.attackEvent.position;
    shapeInput.defenderPosition = input.defender.position;
    shapeInput.defenderFacing = input.defender.facing;
    shapeInput.operationType = usingHiddenWeapon ? BattleOperationType::None : input.attackEvent.operationType;
    shapeInput.usingSkill = usingSkill;
    shapeInput.attackerActProperty = input.skill.attackerActProperty;
    shapeInput.defenderActProperty = input.skill.defenderActProperty;

    const auto shaped = BattleDamageSystem().shapeLegacyHitDamage(shapeInput);
    result.shapedHpDamage = shaped.damage;
    result.finalHpDamage = static_cast<int>(shaped.damage);
    BattleHitRollStream rolls(input.percentRolls);

    if (shaped.frozenFrames > 0)
    {
        BattleDamageRequest request;
        request.frozenFrames = shaped.frozenFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
    }

    auto velocityDelta = input.defender.position - input.attacker.position;
    velocityDelta.normTo(static_cast<float>(shaped.knockbackStrength));
    result.commands.push_back(BattleKnockbackCommand{
        input.defender.id,
        velocityDelta,
        shaped.knockbackVelocityCap,
        shaped.grantsHurtFrame,
    });

    auto attackerCombo = input.attackerCombo;
    if (usingSkill && input.attacker.maxMp > 0 && attackerCombo.mpRatioDmgBoostPct > 0)
    {
        const double mpRatio = static_cast<double>(input.attacker.mp) / input.attacker.maxMp;
        const double boostPct = mpRatio * attackerCombo.mpRatioDmgBoostPct;
        if (boostPct > 0.0)
        {
            result.shapedHpDamage *= 1.0 + boostPct / 100.0;
            result.presentationEvents.push_back(statusEvent(
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
        { result.shapedHpDamage, input.attacker.hp, input.attacker.maxHp, attackerCombo.lastAliveFlag },
        [&]() { return rolls.next(); });
    result.shapedHpDamage = attackerDamage.damage;
    for (const auto& damageEvent : attackerDamage.events)
    {
        switch (damageEvent.type)
        {
        case BattleAttackerHitDamageEventType::Crit:
            result.critical = true;
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusPercent("暴擊", damageEvent.value)));
            break;
        case BattleAttackerHitDamageEventType::RampingStack:
            result.presentationEvents.push_back(statusEvent(
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
            result.presentationEvents.push_back(roleEffectEvent(input.attacker.id, KysChess::EFT_HEAL, RoleStatusEffectFrames));
            result.presentationEvents.push_back(healEvent(input.attacker.id, input.attacker.id, resources.hpHealed, "命中回血"));
        }
    }

    if (attackerCombo.poisonDOTPct > 0)
    {
        BattleDamageRequest request;
        request.poisonPct = attackerCombo.poisonDOTPct;
        request.poisonDurationFrames = attackerCombo.poisonDuration;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.presentationEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusPercentFrames("中毒", attackerCombo.poisonDOTPct, attackerCombo.poisonDuration)));
    }

    int legacyStunFrames = BattleComboTriggerSystem().resolveLegacyStunFrames(
        attackerCombo,
        [&]() { return rolls.next(); });
    if (legacyStunFrames > 0)
    {
        BattleDamageRequest request;
        request.frozenFrames = legacyStunFrames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.presentationEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusFrames("眩暈", legacyStunFrames)));
    }

    auto hitStunCommands = BattleComboTriggerSystem().collectStunCommands(
        attackerCombo,
        { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
        [&]() { return rolls.next(); });
    for (const auto& stunCommand : hitStunCommands)
    {
        BattleDamageRequest request;
        request.frozenFrames = stunCommand.frames;
        result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
        result.presentationEvents.push_back(statusEvent(
            input.attacker.id,
            input.defender.id,
            formatStatusFrames("眩暈", stunCommand.frames)));
        BattleComboTriggerSystem().recordActivation(
            attackerCombo,
            static_cast<size_t>(stunCommand.effectIndex));
    }

    if (BattleComboTriggerSystem().shouldApplyKnockback(attackerCombo, [&]() { return rolls.next(); }))
    {
        auto procVelocity = input.defender.position - input.attacker.position;
        procVelocity.normTo(5.0f);
        result.commands.push_back(BattleKnockbackCommand{
            input.defender.id,
            procVelocity,
            0.0,
            false,
        });
    }

    int offensiveCooldownExtendPct = BattleComboTriggerSystem().resolveOffensiveCooldownExtendPct(
        attackerCombo,
        [&]() { return rolls.next(); });
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
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatCooldownIncreaseStatus(offensiveCooldownExtendPct, cooldown.before, cooldown.after)));
        }
    }

    auto teamHeal = BattleComboTriggerSystem().collectTriggeredTeamHeal(
        attackerCombo,
        { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
        [&]() { return rolls.next(); });
    if (teamHeal.flatHeal > 0 || teamHeal.pctHeal > 0)
    {
        result.commands.push_back(BattleTeamHealCommand{
            input.attacker.id,
            teamHeal.flatHeal,
            teamHeal.pctHeal,
            "命中群療",
        });
    }

    result.attackerCombo = std::move(attackerCombo);
    result.finalHpDamage = static_cast<int>(result.shapedHpDamage);
    return result;
}

}  // namespace KysChess::Battle
