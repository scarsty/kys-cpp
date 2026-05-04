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

std::string formatStatusRange(const char* label, int current, int maxValue, const char* unit)
{
    if (current <= 0 || maxValue <= 0)
    {
        return label;
    }
    return std::format("{}（{}/{}{}）", label, current, maxValue, unit);
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
    damageUnit.hp = unit.hp;
    damageUnit.maxHp = unit.maxHp;
    damageUnit.mp = unit.mp;
    damageUnit.maxMp = unit.maxMp;
    damageUnit.attack = unit.attack;
    damageUnit.invincible = unit.invincible;
    if (combo)
    {
        damageUnit.shield = combo->shield;
        damageUnit.blockFirstHitsRemaining = combo->blockFirstHitsRemaining;
        damageUnit.deathPrevention = combo->deathPrevention;
        damageUnit.deathPreventionUsed = combo->deathPreventionUsed;
        damageUnit.deathPreventionFrames = combo->deathPreventionFrames;
        damageUnit.killHealPct = combo->killHealPct;
        damageUnit.killInvincFrames = combo->killInvincFrames;
        damageUnit.bloodlustAttackPerKill = combo->bloodlustATKPerKill;
        damageUnit.mpBlocked = combo->mpBlockTimer > 0;
        damageUnit.mpRecoveryBonusPct = combo->mpRecoveryBonusPct;
        damageUnit.hurtInvincFrames = combo->hurtInvincFrames;
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

BattlePresentationEvent floatingTextEvent(int targetUnitId,
                                          std::string text,
                                          BattlePresentationColor color,
                                          int textSize)
{
    BattlePresentationEvent event;
    event.type = BattlePresentationEventType::FloatingText;
    event.targetUnitId = targetUnitId;
    event.text = std::move(text);
    event.color = color;
    event.textSize = textSize;
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

void writeDefenseState(RoleComboState& combo, const BattleDamageUnitState& unit)
{
    combo.shield = unit.shield;
    combo.blockFirstHitsRemaining = unit.blockFirstHitsRemaining;
    combo.deathPreventionUsed = unit.deathPreventionUsed;
}

std::string appendDetail(std::string detail, const std::string& text)
{
    if (text.empty())
    {
        return detail;
    }
    if (!detail.empty())
    {
        detail += "、";
    }
    detail += text;
    return detail;
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

    auto defenderCombo = input.defenderCombo;
    const bool rangedProjectile = usingHiddenWeapon || input.attackEvent.operationType == BattleOperationType::RangedProjectile;
    const bool usingHpDamage = usingHiddenWeapon || input.skill.hurtType == 0;

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
        { result.shapedHpDamage, input.defender.hp, input.defender.maxHp, defenderCombo.lastAliveFlag, input.attacker.id });
    result.shapedHpDamage = defenderDamage.damage;
    for (const auto& damageEvent : defenderDamage.events)
    {
        switch (damageEvent.type)
        {
        case BattleDefenderHitDamageEventType::DamageAdaptationStack:
            result.presentationEvents.push_back(statusEvent(
                input.defender.id,
                input.attacker.id,
                formatStackingEffectStatus("同敵減傷", damageEvent.value, damageEvent.value2)));
            break;
        case BattleDefenderHitDamageEventType::DodgeAdaptationStack:
            result.presentationEvents.push_back(statusEvent(
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
        result.presentationEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            std::format("單次承傷封頂{}%最大生命", lateDamage.maxHitPct)));
    }

    int defensiveCooldownExtendPct = BattleComboTriggerSystem().resolveDefensiveCooldownExtendPct(
        defenderCombo,
        [&]() { return rolls.next(); });
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
            result.presentationEvents.push_back(statusEvent(
                input.defender.id,
                input.attacker.id,
                formatCooldownIncreaseStatus(defensiveCooldownExtendPct, cooldown.before, cooldown.after)));
        }
    }

    auto beingHitStunCommands = BattleComboTriggerSystem().collectStunCommands(
        defenderCombo,
        { BattleComboTriggerHook::DamageTaken, input.defender.id, input.attacker.id },
        [&]() { return rolls.next(); });
    for (const auto& stunCommand : beingHitStunCommands)
    {
        BattleDamageRequest request;
        request.frozenFrames = stunCommand.frames;
        result.commands.push_back(acceptedHitCommand(input.defender.id, input.attacker.id, request));
        result.presentationEvents.push_back(statusEvent(
            input.defender.id,
            input.attacker.id,
            formatStatusFrames("反制並眩暈對手", stunCommand.frames)));
        BattleComboTriggerSystem().recordActivation(
            defenderCombo,
            static_cast<size_t>(stunCommand.effectIndex));
    }

    result.reflected = BattleComboTriggerSystem().resolveProjectileReflect(
        defenderCombo,
        rangedProjectile,
        [&]() { return rolls.next(); });
    if (result.reflected)
    {
        result.presentationEvents.push_back(floatingTextEvent(
            input.defender.id,
            "彈反",
            { 180, 150, 255, 255 },
            24));
        result.presentationEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "彈反了遠程攻擊"));
    }

    if (!result.reflected)
    {
        auto executeResult = BattleComboTriggerSystem().resolveExecuteCombo(
            attackerCombo,
            {
                input.attacker.id,
                input.defender.id,
                input.defender.hp - input.pendingDefenderHpDamage,
                input.defender.maxHp,
                result.shapedHpDamage,
                usingHpDamage,
            },
            [&]() { return rolls.next(); });
        if (executeResult.executed)
        {
            result.executed = true;
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatExecuteStatus(executeResult.thresholdPct)));
        }
    }

    auto blockCommands = BattleComboTriggerSystem().collectDefenderBlockCommands(
        defenderCombo,
        { result.executed, result.reflected },
        [&]() { return rolls.next(); });
    for (auto blockCommand : blockCommands)
    {
        result.shapedHpDamage = 0;
        switch (blockCommand)
        {
        case BattleDefenderBlockCommand::CounterUltimateBlock:
            result.presentationEvents.push_back(roleEffectEvent(input.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
            result.presentationEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "格擋後釋放絕招"));
            result.commands.push_back(BattleAutoUltimateCommand{ input.defender.id, false });
            break;
        case BattleDefenderBlockCommand::Block:
            result.presentationEvents.push_back(roleEffectEvent(input.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
            result.presentationEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "格擋了本次攻擊"));
            break;
        default:
            assert(false);
        }
    }

    auto defense = BattleDamageSystem().resolveDefense({
        result.shapedHpDamage,
        result.executed,
        result.reflected,
        input.defender.invincible > 0,
        makeDamageUnit(input.defender, &defenderCombo),
    });
    result.shapedHpDamage = defense.damage;
    writeDefenseState(defenderCombo, defense.defender);
    if (defense.blockedByFirstHit)
    {
        result.presentationEvents.push_back(roleEffectEvent(input.defender.id, KysChess::EFT_BLOCK, RoleStatusEffectFrames));
        result.presentationEvents.push_back(statusEvent(input.defender.id, input.attacker.id, "格擋了首轮傷害"));
    }
    std::string damageDetail;
    if (usingHiddenWeapon)
    {
        damageDetail = appendDetail(std::move(damageDetail), "暗器");
    }
    if (result.critical)
    {
        damageDetail = appendDetail(std::move(damageDetail), "暴擊");
    }
    if (defense.shieldAbsorbed > 0)
    {
        damageDetail = appendDetail(std::move(damageDetail), std::format("護盾吸收 {}", defense.shieldAbsorbed));
    }
    if (result.executed)
    {
        damageDetail = appendDetail(std::move(damageDetail), "處決");
    }
    if (result.reflected)
    {
        damageDetail = appendDetail(std::move(damageDetail), "彈反");
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
                "",
                "技能反彈",
            });
        }
    }

    if (!result.reflected)
    {
        auto bleedProc = BattleComboTriggerSystem().resolveBleedProc(
            attackerCombo,
            result.shapedHpDamage > 0,
            [&]() { return rolls.next(); });
        if (bleedProc.applies)
        {
            BattleDamageRequest request;
            request.bleedStacks = bleedProc.stacks;
            request.bleedMaxStacks = bleedProc.maxStacks;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusRange("流血", bleedProc.stacks, std::max(1, bleedProc.maxStacks), "層")));
        }

        auto damageReduceDebuff = BattleComboTriggerSystem().resolveDamageReduceDebuffProc(
            attackerCombo,
            result.shapedHpDamage > 0,
            [&]() { return rolls.next(); });
        if (damageReduceDebuff.applies)
        {
            BattleDamageRequest request;
            request.damageReduceDebuffDurationFrames = damageReduceDebuff.durationFrames;
            request.damageReduceDebuffPct = damageReduceDebuff.pct;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusPercentFrames("傷害降低", damageReduceDebuff.pct, damageReduceDebuff.durationFrames)));
        }

        auto mpBlockEvents = BattleComboTriggerSystem().collectTriggerEvents(
            attackerCombo,
            { BattleComboTriggerHook::DamageDealt, input.attacker.id, input.defender.id },
            { KysChess::EffectType::MPBlock },
            [&]() { return rolls.next(); });
        for (const auto& mpBlock : mpBlockEvents)
        {
            BattleDamageRequest request;
            request.mpBlockFrames = mpBlock.effect.value;
            result.commands.push_back(acceptedHitCommand(input.attacker.id, input.defender.id, request));
            result.presentationEvents.push_back(statusEvent(
                input.attacker.id,
                input.defender.id,
                formatStatusFrames("封內", mpBlock.effect.value)));
        }
    }

    if (usingHpDamage && result.shapedHpDamage > 0)
    {
        int damage = static_cast<int>(result.shapedHpDamage) + input.randomDamageVariance;
        damage = std::max(0, damage);
        if (result.executed)
        {
            damage = std::max(damage, input.defender.hp);
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
                input.skill.name,
                damageDetail,
            });
            result.finalHpDamage = damage;
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
