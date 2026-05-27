#include "BattleStatusSystem.h"

#include "../ChessBattleEffects.h"
#include "BattleRuntimeUnits.h"
#include "BattleUnitStore.h"

#include <algorithm>

namespace KysChess::Battle
{

namespace
{

struct RuntimeStatusTickTarget
{
    BattleRuntimeUnit& unit;
    BattleStatusRuntimeUnit& status;

    int id() const { return status.id; }
    bool alive() const { return unit.alive; }
    int hp() const { return unit.vitals.hp; }
    int maxHp() const { return unit.vitals.maxHp; }
    int& attack() { return unit.stats.attack; }
    int& invincible() { return unit.invincible; }
};

void tickTempAttackBuffs(RuntimeStatusTickTarget& target, BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    for (auto it = effects.tempAttackBuffs.begin(); it != effects.tempAttackBuffs.end();)
    {
        if (it->remainingFrames > 0)
        {
            --it->remainingFrames;
        }

        if (it->remainingFrames <= 0)
        {
            target.attack() -= it->attackBonus;
            result.events.push_back({
                BattleStatusEventType::TempAttackExpired,
                target.id(),
                target.id(),
                it->attackBonus,
                "臨時攻擊到期",
            });
            it = effects.tempAttackBuffs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void tickPoison(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    if (effects.poisonTimer <= 0)
    {
        return;
    }

    --effects.poisonTimer;
    if (config.poisonDamageIntervalFrames > 0 && config.frame % config.poisonDamageIntervalFrames == 0)
    {
        int damage = std::max(1, target.hp() * effects.poisonTickPct / 100);
        result.events.push_back({
            BattleStatusEventType::PoisonDamage,
            target.id(),
            effects.poisonSourceId,
            damage,
            "中毒",
        });
    }

    if (effects.poisonTimer <= 0)
    {
        effects.poisonSourceId = -1;
    }
}

void tickBleed(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    if (effects.bleedStacks <= 0)
    {
        effects.bleedTimer = 0;
        effects.bleedSourceId = -1;
        return;
    }

    if (effects.bleedTimer > 0)
    {
        --effects.bleedTimer;
    }

    if (effects.bleedTimer <= 0)
    {
        int damage = std::max(1, target.maxHp() * effects.bleedStacks / 100);
        result.events.push_back({
            BattleStatusEventType::BleedDamage,
            target.id(),
            effects.bleedSourceId,
            damage,
            "流血",
        });
        effects.bleedTimer = config.bleedDamageIntervalFrames;
    }
}

void tickSimpleTimers(RuntimeStatusTickTarget& target)
{
    auto& effects = target.status.effects;
    if (target.invincible() > 0)
    {
        --target.invincible();
    }
    if (effects.mpBlockTimer > 0)
    {
        --effects.mpBlockTimer;
    }

    for (auto it = effects.damageReduceDebuffs.begin(); it != effects.damageReduceDebuffs.end();)
    {
        if (--it->remainingFrames <= 0)
        {
            it = effects.damageReduceDebuffs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void tickPeriodicInvincibility(RuntimeStatusTickTarget& target, BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    if (effects.damageImmunityAfterFrames <= 0)
    {
        return;
    }

    if (effects.damageImmunityTimer > 0)
    {
        --effects.damageImmunityTimer;
    }

    if (effects.damageImmunityTimer <= 0)
    {
        int before = target.invincible();
        target.invincible() = std::max(target.invincible(), effects.damageImmunityDuration);
        effects.damageImmunityTimer = effects.damageImmunityAfterFrames;
        if (target.invincible() > before)
        {
            result.events.push_back({
                BattleStatusEventType::InvincibilityGranted,
                target.id(),
                target.id(),
                effects.damageImmunityDuration,
                "周期免傷",
            });
        }
    }
}

BattleStatusTickResult tickStatusTarget(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget target)
{
    BattleStatusTickResult result;
    if (!target.alive())
    {
        return result;
    }

    tickTempAttackBuffs(target, result);
    tickPoison(config, target, result);
    tickBleed(config, target, result);
    tickSimpleTimers(target);
    tickPeriodicInvincibility(target, result);
    return result;
}

}  // namespace

BattleStatusSystem::BattleStatusSystem(BattleStatusSystemConfig config)
    : config_(config)
{
}

BattleStatusTickResult BattleStatusSystem::tick(BattleUnitStore& units, BattleRuntimeUnitRecord& record) const
{
    auto& unit = units.requireUnit(record.id());
    return tickStatusTarget(config_, RuntimeStatusTickTarget{ unit, record.status });
}

BattleStatusTickResult BattleStatusSystem::tick(BattleUnitStore& units, BattleRuntimeUnitRecords& records) const
{
    BattleStatusTickResult result;
    for (auto& record : records.all())
    {
        auto unitResult = tick(units, record);
        result.events.insert(result.events.end(), unitResult.events.begin(), unitResult.events.end());
    }
    return result;
}

BattleStatusRuntimeUnit makeBattleStatusRuntimeUnit(const BattleStatusUnitState& unit)
{
    BattleStatusRuntimeUnit status;
    status.id = unit.id;
    writeBattleStatusRuntimeUnit(status, unit);
    return status;
}

BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit, const KysChess::RoleComboState& state)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    status.attack = unit.stats.attack;
    status.invincible = unit.invincible;
    status.effects.freezeReductionPct = sumAlwaysEffectValue(state, EffectType::FreezeReductionPct);
    status.effects.shieldFreezeResPct = sumAlwaysEffectValue(state, EffectType::ShieldFreezeRes);
    status.effects.controlImmunityFrames = sumAlwaysEffectValue(state, EffectType::ControlImmunityFrames);
    if (const auto* immunity = maxAlwaysEffectByValue(state, EffectType::DamageImmunityAfterFrames))
    {
        status.effects.damageImmunityAfterFrames = immunity->value;
        status.effects.damageImmunityDuration = immunity->value2;
    }
    if (status.effects.damageImmunityAfterFrames > 0)
    {
        status.effects.damageImmunityTimer = status.effects.damageImmunityAfterFrames;
    }
    return status;
}

BattleStatusUnitState makeBattleStatusUnitState(const BattleStatusRuntimeUnit& status, const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState frame;
    frame.id = status.id;
    frame.alive = unit.alive;
    frame.hp = unit.vitals.hp;
    frame.maxHp = unit.vitals.maxHp;
    frame.attack = unit.stats.attack;
    frame.invincible = unit.invincible;
    frame.effects = status.effects;
    return frame;
}

void writeBattleStatusRuntimeUnit(BattleStatusRuntimeUnit& status, const BattleStatusUnitState& unit)
{
    status.effects = unit.effects;
}

}  // namespace KysChess::Battle
