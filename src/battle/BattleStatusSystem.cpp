#include "BattleStatusSystem.h"

#include "../ChessBattleEffects.h"
#include "BattleCore.h"

#include <algorithm>

namespace KysChess::Battle
{

namespace
{

struct LegacyStatusTickTarget
{
    BattleStatusUnitState& status;

    int id() const { return status.id; }
    bool alive() const { return status.alive; }
    int hp() const { return status.hp; }
    int maxHp() const { return status.maxHp; }
    int& attack() { return status.attack; }
    int& invincible() { return status.invincible; }
    void syncMpBlocked() {}
};

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
    void syncMpBlocked() { unit.mpBlocked = status.effects.mpBlockTimer > 0; }
};

template <typename Target>
void tickTempAttackBuffs(Target& target, BattleStatusTickResult& result)
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

template <typename Target>
void tickPoison(
    const BattleStatusSystemConfig& config,
    Target& target,
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

template <typename Target>
void tickBleed(
    const BattleStatusSystemConfig& config,
    Target& target,
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

template <typename Target>
void tickSimpleTimers(Target& target)
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
    target.syncMpBlocked();

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

template <typename Target>
void tickPeriodicInvincibility(Target& target, BattleStatusTickResult& result)
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

template <typename Target>
BattleStatusTickResult tickStatusTarget(const BattleStatusSystemConfig& config, Target target)
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

BattleStatusTickResult BattleStatusSystem::tick(BattleStatusUnitState& unit) const
{
    return tickStatusTarget(config_, LegacyStatusTickTarget{ unit });
}

BattleStatusTickResult BattleStatusSystem::tick(std::vector<BattleStatusUnitState>& units) const
{
    BattleStatusTickResult result;
    for (auto& unit : units)
    {
        auto unitResult = tick(unit);
        result.events.insert(result.events.end(), unitResult.events.begin(), unitResult.events.end());
    }
    return result;
}

BattleStatusTickResult BattleStatusSystem::tick(BattleUnitStore& units, BattleStatusRuntimeUnit& status) const
{
    auto& unit = units.requireUnit(status.id);
    return tickStatusTarget(config_, RuntimeStatusTickTarget{ unit, status });
}

BattleStatusTickResult BattleStatusSystem::tick(BattleUnitStore& units, std::vector<BattleStatusRuntimeUnit>& statuses) const
{
    BattleStatusTickResult result;
    for (auto& status : statuses)
    {
        auto unitResult = tick(units, status);
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
    status.effects.poisonTimer = state.poisonTimer;
    status.effects.poisonTickPct = state.poisonTickDmg;
    status.effects.poisonSourceId = state.poisonSourceId;
    status.effects.bleedStacks = state.bleedStacks;
    status.effects.bleedTimer = state.bleedTimer;
    status.effects.bleedSourceId = state.bleedSourceId;
    status.effects.freezeReductionPct = state.freezeReductionPct;
    status.effects.shieldFreezeResPct = state.shieldFreezeResPct;
    status.effects.controlImmunityFrames = state.controlImmunityFrames;
    status.effects.mpBlockTimer = state.mpBlockTimer;
    status.effects.damageImmunityAfterFrames = state.damageImmunityAfterFrames;
    status.effects.damageImmunityDuration = state.damageImmunityDuration;
    status.effects.damageImmunityTimer = state.damageImmunityTimer;

    for (const auto& buff : state.tempAttackBuffs)
    {
        status.effects.tempAttackBuffs.push_back({ buff.attackBonus, buff.remainingFrames });
    }
    for (const auto& debuff : state.dmgReduceDebuffs)
    {
        status.effects.damageReduceDebuffs.push_back({ debuff.remainingFrames, debuff.pct });
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
