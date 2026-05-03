#include "BattleDamageSystem.h"

#include <algorithm>
#include <cassert>

namespace KysChess::Battle
{

namespace
{

BattleUnitDelta makeBattleUnitDelta(const BattleDamageUnitState& before, const BattleDamageUnitState& after)
{
    BattleUnitDelta delta;
    delta.unitId = after.id;
    delta.hpDelta = after.hp - before.hp;
    delta.mpDelta = after.mp - before.mp;
    delta.shieldDelta = after.shield - before.shield;
    delta.invincibleDelta = after.invincible - before.invincible;
    delta.attackDelta = after.attack - before.attack;
    delta.aliveChanged = before.alive != after.alive;
    delta.alive = after.alive;
    return delta;
}

void recordBattleDamageEvent(std::vector<BattleDamageEvent>& events,
                             BattleDamageEventType type,
                             int sourceUnitId,
                             int targetUnitId,
                             int value)
{
    events.push_back({ type, BattleDamageStatusType::None, sourceUnitId, targetUnitId, value });
}

void recordBattleStatusEvent(std::vector<BattleDamageEvent>& events,
                             BattleDamageStatusType statusType,
                             int sourceUnitId,
                             int targetUnitId,
                             int value)
{
    events.push_back({ BattleDamageEventType::StatusApplied, statusType, sourceUnitId, targetUnitId, value });
}

BattleResourceUnitState makeBattleResourceUnit(const BattleDamageUnitState& unit)
{
    BattleResourceUnitState resource;
    resource.id = unit.id;
    resource.alive = unit.alive;
    resource.hp = unit.hp;
    resource.maxHp = unit.maxHp;
    resource.mp = unit.mp;
    resource.maxMp = unit.maxMp;
    return resource;
}

void writeBattleResourceUnit(BattleDamageUnitState& unit, const BattleResourceUnitState& resource)
{
    unit.hp = resource.hp;
    unit.mp = resource.mp;
}

}  // namespace

BattleDamageTransactionResult BattleDamageSystem::resolveTransaction(const BattleDamageTransactionInput& input) const
{
    assert(input.request.attackerUnitId == input.attacker.id);
    assert(input.request.defenderUnitId == input.defender.id);
    assert(input.request.baseDamage >= 0.0 && input.request.mpDamage >= 0);

    BattleDamageTransactionResult result;
    result.attacker = input.attacker;
    result.defender = input.defender;
    result.defenderStatus = input.defenderStatus;
    result.defenderCooldown = input.defenderCooldown;
    bool acceptedHit = false;

    if (input.request.baseDamage > 0.0)
    {
        auto modified = applyModifiers({
            input.request.baseDamage,
            input.request.usingSkill,
            input.request.ignoreDefense,
            input.attackerModifiers,
            input.defenderModifiers,
            result.defender,
        });

        result.executed = input.request.canExecute
            && shouldExecute({
                result.defender.hp,
                result.defender.maxHp,
                modified.damage,
                true,
                input.request.executeThresholdPct,
            });
        if (result.executed)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::ExecuteTriggered,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    input.request.executeThresholdPct);
        }

        auto defense = resolveDefense({
            modified.damage,
            result.executed,
            input.request.reflected,
            result.defender.invincible > 0,
            result.defender,
        });
        result.defender = defense.defender;
        result.shieldAbsorbed = defense.shieldAbsorbed;
        result.blockedByInvincible = defense.blockedByInvincible;
        result.blockedByFirstHit = defense.blockedByFirstHit;
        acceptedHit = !defense.blockedByInvincible && !defense.blockedByFirstHit;

        if (defense.shieldAbsorbed > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::ShieldAbsorbed,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    defense.shieldAbsorbed);
        }
        if (defense.blockedByInvincible)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::BlockedByInvincible,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);
        }

        int hpBeforeDamage = result.defender.hp;
        int hpDamage = static_cast<int>(defense.damage);
        if (result.executed)
        {
            hpDamage = std::max(hpDamage, result.defender.hp);
        }

        auto taken = applyDamageTaken(result.defender, hpDamage);
        result.defender = taken.defender;
        result.finalHpDamage = std::max(0, hpBeforeDamage - result.defender.hp);
        result.deathPrevented = taken.deathPrevented;
        result.killed = taken.died;

        if (result.finalHpDamage > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::DamageApplied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    result.finalHpDamage);
        }
        if (taken.deathPrevented)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::DeathPrevented,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    taken.invincibilityGranted);
        }
        if (taken.died)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::UnitDied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    0);

            auto reward = applyKillReward({ result.attacker });
            result.attacker = reward.killer;
            if (reward.healed > 0 || reward.invincibilityGranted > 0 || reward.attackGranted > 0)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::KillRewardApplied,
                                        input.request.attackerUnitId,
                                        input.request.attackerUnitId,
                                        reward.healed);
            }
        }
    }

    if (input.request.mpDamage > 0)
    {
        int mpBefore = result.defender.mp;
        int mpDamage = std::min(input.request.mpDamage, std::max(0, result.defender.mp));
        result.defender.mp -= mpDamage;
        result.finalMpDamage = mpBefore - result.defender.mp;
        if (result.finalMpDamage > 0)
        {
            recordBattleDamageEvent(result.events,
                                    BattleDamageEventType::MpDamageApplied,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    result.finalMpDamage);
        }
    }

    if (acceptedHit)
    {
        if (input.request.mpOnHit > 0 || input.request.hpOnHit > 0 || input.request.mpDrain > 0)
        {
            auto resources = applyOnHitResources({
                makeBattleResourceUnit(result.attacker),
                makeBattleResourceUnit(result.defender),
                input.request.mpOnHit,
                input.request.hpOnHit,
                input.request.mpDrain,
            });
            int attackerHpBefore = result.attacker.hp;
            int attackerMpBefore = result.attacker.mp;
            int defenderMpBefore = result.defender.mp;
            writeBattleResourceUnit(result.attacker, resources.attacker);
            writeBattleResourceUnit(result.defender, resources.target);
            if (result.attacker.hp > attackerHpBefore)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::HpRestored,
                                        input.request.attackerUnitId,
                                        input.request.attackerUnitId,
                                        result.attacker.hp - attackerHpBefore);
            }
            if (result.attacker.mp > attackerMpBefore)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::MpRestored,
                                        input.request.attackerUnitId,
                                        input.request.attackerUnitId,
                                        result.attacker.mp - attackerMpBefore);
            }
            if (defenderMpBefore > result.defender.mp)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::MpDrained,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        defenderMpBefore - result.defender.mp);
            }
        }

        if (input.request.cooldownExtendPct > 0)
        {
            auto cooldown = extendActiveCooldown(result.defenderCooldown, input.request.cooldownExtendPct);
            result.defenderCooldown = cooldown.unit;
            result.cooldownDelta = cooldown.after - cooldown.before;
            if (cooldown.increased)
            {
                recordBattleDamageEvent(result.events,
                                        BattleDamageEventType::CooldownExtended,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        result.cooldownDelta);
            }
        }

        if (input.request.poisonPct > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            auto poison = applyPoisonIfStronger({
                result.defenderStatus,
                input.request.attackerUnitId,
                input.request.poisonPct,
                input.request.poisonDurationFrames,
            });
            result.defenderStatus = poison.target;
            if (poison.applied)
            {
                recordBattleStatusEvent(result.events,
                                        BattleDamageStatusType::Poison,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        poison.value);
            }
        }

        if (input.request.bleedStacks > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            auto bleed = applyBleed(result.defenderStatus,
                                    input.request.attackerUnitId,
                                    input.request.bleedStacks,
                                    input.request.bleedMaxStacks);
            result.defenderStatus = bleed.target;
            if (bleed.applied)
            {
                recordBattleStatusEvent(result.events,
                                        BattleDamageStatusType::Bleed,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        bleed.value);
            }
        }

        if (input.request.damageReduceDebuffDurationFrames > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            auto debuff = applyDamageReduceDebuff(result.defenderStatus,
                                                  input.request.damageReduceDebuffDurationFrames,
                                                  input.request.damageReduceDebuffPct);
            result.defenderStatus = debuff.target;
            if (debuff.applied)
            {
                recordBattleStatusEvent(result.events,
                                        BattleDamageStatusType::DamageReduceDebuff,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        debuff.value);
            }
        }

        if (input.request.frozenFrames > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            result.defenderStatus.frozenTimer += input.request.frozenFrames;
            result.defenderStatus.frozenMaxTimer = result.defenderStatus.frozenTimer;
            recordBattleStatusEvent(result.events,
                                    BattleDamageStatusType::Frozen,
                                    input.request.attackerUnitId,
                                    input.request.defenderUnitId,
                                    input.request.frozenFrames);
        }

        if (input.request.mpBlockFrames > 0)
        {
            assert(input.defenderStatus.id == input.request.defenderUnitId);
            int before = result.defenderStatus.mpBlockTimer;
            result.defenderStatus.mpBlockTimer = std::max(result.defenderStatus.mpBlockTimer,
                                                          input.request.mpBlockFrames);
            if (result.defenderStatus.mpBlockTimer > before)
            {
                recordBattleStatusEvent(result.events,
                                        BattleDamageStatusType::MpBlocked,
                                        input.request.attackerUnitId,
                                        input.request.defenderUnitId,
                                        result.defenderStatus.mpBlockTimer - before);
            }
        }
    }

    result.attackerDelta = makeBattleUnitDelta(input.attacker, result.attacker);
    result.defenderDelta = makeBattleUnitDelta(input.defender, result.defender);
    return result;
}

BattleDamageModifierResult BattleDamageSystem::applyModifiers(const BattleDamageModifierInput& input) const
{
    double damage = input.damage;

    if (input.usingSkill && input.attacker.skillDamagePct > 0)
    {
        damage *= (1.0 + input.attacker.skillDamagePct / 100.0);
    }

    for (const auto& debuff : input.attacker.outgoingDamageReduceDebuffs)
    {
        if (debuff.remainingFrames > 0 && debuff.pct > 0)
        {
            damage *= (1.0 - debuff.pct / 100.0);
        }
    }

    damage += input.attacker.flatDamageIncrease;

    if (!input.ignoreDefense)
    {
        damage -= input.defender.flatDamageReduction;
        if (input.defender.damageReductionPct > 0)
        {
            damage *= (1.0 - input.defender.damageReductionPct / 100.0);
        }
    }

    if (input.defender.poisonTimer > 0 && input.attacker.poisonDamageAmpPct > 0)
    {
        damage *= (1.0 + input.attacker.poisonDamageAmpPct / 100.0);
    }

    BattleDamageModifierResult result;
    result.damage = damage;
    if (input.defender.maxHitPctMaxHp > 0 && result.damage > 0)
    {
        int maxHit = std::max(1, input.defenderUnit.maxHp * input.defender.maxHitPctMaxHp / 100);
        if (result.damage > maxHit)
        {
            result.damage = static_cast<double>(maxHit);
            result.maxHitCapped = true;
            result.maxHitPct = input.defender.maxHitPctMaxHp;
        }
    }
    return result;
}

BattleDamageDefenseResult BattleDamageSystem::resolveDefense(const BattleDamageDefenseInput& input) const
{
    BattleDamageDefenseResult result;
    result.damage = input.damage;
    result.defender = input.defender;

    if (!input.executed && input.defenderWasInvincible)
    {
        result.damage = 0;
        result.blockedByInvincible = true;
        return result;
    }

    if (!input.executed
        && !input.reflected
        && result.damage > 0
        && result.defender.blockFirstHitsRemaining > 0)
    {
        result.damage = 0;
        result.defender.blockFirstHitsRemaining--;
        result.blockedByFirstHit = true;
        return result;
    }

    if (!input.reflected && result.defender.shield > 0 && result.damage > 0)
    {
        int shieldBefore = result.defender.shield;
        int absorbed = std::min(result.defender.shield, static_cast<int>(result.damage));
        result.defender.shield -= absorbed;
        result.damage -= absorbed;
        result.shieldAbsorbed = absorbed;
        result.shieldBroken = shieldBefore > 0 && result.defender.shield == 0;
    }

    return result;
}

BattleDamageTakenResult BattleDamageSystem::applyDamageTaken(BattleDamageUnitState defender, int damage) const
{
    BattleDamageTakenResult result;
    result.defender = defender;
    if (damage <= 0 || !result.defender.alive)
    {
        return result;
    }

    result.defender.hp -= damage;
    if (result.defender.hp > 0 && result.defender.hurtInvincFrames > 0)
    {
        result.defender.invincible += result.defender.hurtInvincFrames;
        result.hurtInvincGranted = true;
        result.invincibilityGranted = result.defender.hurtInvincFrames;
    }

    if (result.defender.hp <= 0)
    {
        if (result.defender.deathPrevention && !result.defender.deathPreventionUsed)
        {
            result.defender.deathPreventionUsed = true;
            result.defender.hp = 1;
            int frames = result.defender.deathPreventionFrames > 0 ? result.defender.deathPreventionFrames : 100;
            result.defender.invincible += frames;
            result.deathPrevented = true;
            result.invincibilityGranted = frames;
        }
        else
        {
            result.defender.hp = 0;
            result.defender.alive = false;
            result.died = true;
        }
    }

    return result;
}

BattleKillRewardResult BattleDamageSystem::applyKillReward(const BattleKillRewardInput& input) const
{
    BattleKillRewardResult result;
    result.killer = input.killer;
    if (!result.killer.alive)
    {
        return result;
    }

    if (result.killer.killHealPct > 0)
    {
        int before = result.killer.hp;
        result.killer.hp = std::min(result.killer.maxHp,
            result.killer.hp + result.killer.maxHp * result.killer.killHealPct / 100);
        result.healed = result.killer.hp - before;
    }

    if (result.killer.killInvincFrames > 0)
    {
        result.killer.invincible = result.killer.killInvincFrames;
        result.invincibilityGranted = result.killer.killInvincFrames;
    }

    if (result.killer.bloodlustAttackPerKill > 0)
    {
        result.killer.attack += result.killer.bloodlustAttackPerKill;
        result.attackGranted = result.killer.bloodlustAttackPerKill;
    }

    return result;
}

BattleCooldownIncreaseResult BattleDamageSystem::extendActiveCooldown(BattleCooldownState unit, int pct) const
{
    BattleCooldownIncreaseResult result;
    result.unit = unit;
    result.before = unit.cooldown;
    result.after = unit.cooldown;

    bool canExtend = unit.alive
        && unit.cooldown > 0
        && unit.haveAction
        && unit.operationType != BattleOperationType::None
        && unit.actType >= 0
        && pct > 0;
    if (!canExtend)
    {
        return result;
    }

    int baseCooldown = std::max(1, unit.cooldownMax);
    int extension = std::max(1, (baseCooldown * pct + 99) / 100);
    int cap = baseCooldown + extension;
    if (unit.cooldown >= cap)
    {
        return result;
    }

    result.unit.cooldown = std::min(cap, unit.cooldown + extension);
    result.after = result.unit.cooldown;
    result.increased = result.after > result.before;
    return result;
}

bool BattleDamageSystem::shouldExecute(const BattleExecuteInput& input) const
{
    if (input.thresholdPct <= 0 || input.maxHp <= 0 || input.pendingDamage <= 0)
    {
        return false;
    }

    int projectedHp = input.projectedHpBeforeDamage;
    if (input.appliesHpDamage)
    {
        projectedHp -= static_cast<int>(input.pendingDamage);
    }
    return projectedHp * 100 < input.maxHp * input.thresholdPct;
}

BattleOnHitResourceResult BattleDamageSystem::applyOnHitResources(const BattleOnHitResourceInput& input) const
{
    assert(input.attacker.id >= 0);
    assert(input.target.id >= 0);
    assert(input.attacker.maxHp >= 0);
    assert(input.attacker.maxMp >= 0);
    assert(input.target.maxHp >= 0);
    assert(input.target.maxMp >= 0);
    assert(input.mpOnHit >= 0);
    assert(input.hpOnHit >= 0);
    assert(input.mpDrain >= 0);

    BattleOnHitResourceResult result;
    result.attacker = input.attacker;
    result.target = input.target;
    if (!result.attacker.alive)
    {
        return result;
    }

    if (input.mpOnHit > 0)
    {
        int before = result.attacker.mp;
        result.attacker.mp = std::min(result.attacker.maxMp, result.attacker.mp + input.mpOnHit);
        result.mpRestored += result.attacker.mp - before;
    }

    if (input.hpOnHit > 0)
    {
        int before = result.attacker.hp;
        result.attacker.hp = std::min(result.attacker.maxHp, result.attacker.hp + input.hpOnHit);
        result.hpHealed = result.attacker.hp - before;
    }

    if (input.mpDrain > 0 && result.target.alive)
    {
        int drained = std::min(input.mpDrain, std::max(0, result.target.mp));
        result.target.mp -= drained;
        int before = result.attacker.mp;
        result.attacker.mp = std::min(result.attacker.maxMp, result.attacker.mp + drained);
        result.mpRestored += result.attacker.mp - before;
        result.mpDrained = drained;
    }

    return result;
}

BattleStatusApplyResult BattleDamageSystem::applyPoisonIfStronger(const BattlePoisonApplyInput& input) const
{
    assert(input.target.id >= 0);
    assert(input.target.maxHp >= 0);
    assert(input.poisonPct > 0);
    assert(input.durationFrames > 0);

    BattleStatusApplyResult result;
    result.target = input.target;
    if (!result.target.alive)
    {
        return result;
    }

    int newDamage = result.target.hp * input.poisonPct / 100;
    int oldDamage = result.target.hp * result.target.poisonTickPct / 100;
    if (newDamage <= oldDamage)
    {
        return result;
    }

    result.target.poisonTimer = input.durationFrames;
    result.target.poisonTickPct = input.poisonPct;
    result.target.poisonSourceId = input.sourceUnitId;
    result.applied = true;
    result.value = input.poisonPct;
    return result;
}

BattleStatusApplyResult BattleDamageSystem::applyBleed(BattleStatusUnitState target,
                                                       int sourceUnitId,
                                                       int stacks,
                                                       int maxStacks) const
{
    assert(target.id >= 0);
    assert(stacks > 0);
    assert(maxStacks > 0);

    BattleStatusApplyResult result;
    result.target = target;
    if (!result.target.alive)
    {
        return result;
    }

    int before = result.target.bleedStacks;
    result.target.bleedStacks = std::min(result.target.bleedStacks + stacks, maxStacks);
    if (result.target.bleedTimer <= 0)
    {
        result.target.bleedTimer = 10;
    }
    result.target.bleedSourceId = sourceUnitId;
    result.applied = result.target.bleedStacks > before;
    result.value = result.target.bleedStacks;
    return result;
}

BattleStatusApplyResult BattleDamageSystem::applyDamageReduceDebuff(BattleStatusUnitState target,
                                                                    int durationFrames,
                                                                    int pct) const
{
    assert(target.id >= 0);
    assert(durationFrames > 0);
    assert(pct > 0);

    BattleStatusApplyResult result;
    result.target = target;
    if (!result.target.alive)
    {
        return result;
    }

    result.target.damageReduceDebuffs.push_back({ durationFrames, pct });
    result.applied = true;
    result.value = pct;
    return result;
}

}  // namespace KysChess::Battle
