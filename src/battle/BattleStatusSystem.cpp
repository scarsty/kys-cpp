#include "BattleStatusSystem.h"

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
    int hp() const { return unit.hp; }
    int maxHp() const { return unit.maxHp; }
    int& attack() { return unit.attack; }
    int& invincible() { return unit.invincible; }
    void syncMpBlocked() { unit.mpBlocked = status.mpBlockTimer > 0; }
};

template <typename Target>
void tickTempAttackBuffs(Target& target, BattleStatusTickResult& result)
{
    auto& status = target.status;
    for (auto it = status.tempAttackBuffs.begin(); it != status.tempAttackBuffs.end();)
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
            it = status.tempAttackBuffs.erase(it);
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
    auto& status = target.status;
    if (status.poisonTimer <= 0)
    {
        return;
    }

    --status.poisonTimer;
    if (config.poisonDamageIntervalFrames > 0 && config.frame % config.poisonDamageIntervalFrames == 0)
    {
        int damage = std::max(1, target.hp() * status.poisonTickPct / 100);
        result.events.push_back({
            BattleStatusEventType::PoisonDamage,
            target.id(),
            status.poisonSourceId,
            damage,
            "中毒",
        });
    }

    if (status.poisonTimer <= 0)
    {
        status.poisonSourceId = -1;
    }
}

template <typename Target>
void tickBleed(
    const BattleStatusSystemConfig& config,
    Target& target,
    BattleStatusTickResult& result)
{
    auto& status = target.status;
    if (status.bleedStacks <= 0)
    {
        status.bleedTimer = 0;
        status.bleedSourceId = -1;
        return;
    }

    if (status.bleedTimer > 0)
    {
        --status.bleedTimer;
    }

    if (status.bleedTimer <= 0)
    {
        int damage = std::max(1, target.maxHp() * status.bleedStacks / 100);
        result.events.push_back({
            BattleStatusEventType::BleedDamage,
            target.id(),
            status.bleedSourceId,
            damage,
            "流血",
        });
        status.bleedTimer = config.bleedDamageIntervalFrames;
    }
}

template <typename Target>
void tickSimpleTimers(Target& target)
{
    auto& status = target.status;
    if (status.mpBlockTimer > 0)
    {
        --status.mpBlockTimer;
    }
    target.syncMpBlocked();

    for (auto it = status.damageReduceDebuffs.begin(); it != status.damageReduceDebuffs.end();)
    {
        if (--it->remainingFrames <= 0)
        {
            it = status.damageReduceDebuffs.erase(it);
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
    auto& status = target.status;
    if (status.damageImmunityAfterFrames <= 0)
    {
        return;
    }

    if (status.damageImmunityTimer > 0)
    {
        --status.damageImmunityTimer;
    }

    if (status.damageImmunityTimer <= 0)
    {
        int before = target.invincible();
        target.invincible() = std::max(target.invincible(), status.damageImmunityDuration);
        status.damageImmunityTimer = status.damageImmunityAfterFrames;
        if (target.invincible() > before)
        {
            result.events.push_back({
                BattleStatusEventType::InvincibilityGranted,
                target.id(),
                target.id(),
                status.damageImmunityDuration,
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

BattleStatusUnitState makeBattleStatusUnitState(const BattleStatusRuntimeUnit& status, const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState frame;
    frame.id = status.id;
    frame.alive = unit.alive;
    frame.hp = unit.hp;
    frame.maxHp = unit.maxHp;
    frame.attack = unit.attack;
    frame.invincible = unit.invincible;
    frame.poisonTimer = status.poisonTimer;
    frame.poisonTickPct = status.poisonTickPct;
    frame.poisonSourceId = status.poisonSourceId;
    frame.bleedStacks = status.bleedStacks;
    frame.bleedTimer = status.bleedTimer;
    frame.bleedSourceId = status.bleedSourceId;
    frame.frozenTimer = status.frozenTimer;
    frame.frozenMaxTimer = status.frozenMaxTimer;
    frame.freezeReductionPct = status.freezeReductionPct;
    frame.shieldFreezeResPct = status.shieldFreezeResPct;
    frame.controlImmunityFrames = status.controlImmunityFrames;
    frame.mpBlockTimer = status.mpBlockTimer;
    frame.damageImmunityAfterFrames = status.damageImmunityAfterFrames;
    frame.damageImmunityDuration = status.damageImmunityDuration;
    frame.damageImmunityTimer = status.damageImmunityTimer;
    frame.tempAttackBuffs = status.tempAttackBuffs;
    frame.damageReduceDebuffs = status.damageReduceDebuffs;
    return frame;
}

void writeBattleStatusRuntimeUnit(BattleStatusRuntimeUnit& status, const BattleStatusUnitState& unit)
{
    status.poisonTimer = unit.poisonTimer;
    status.poisonTickPct = unit.poisonTickPct;
    status.poisonSourceId = unit.poisonSourceId;
    status.bleedStacks = unit.bleedStacks;
    status.bleedTimer = unit.bleedTimer;
    status.bleedSourceId = unit.bleedSourceId;
    status.frozenTimer = unit.frozenTimer;
    status.frozenMaxTimer = unit.frozenMaxTimer;
    status.freezeReductionPct = unit.freezeReductionPct;
    status.shieldFreezeResPct = unit.shieldFreezeResPct;
    status.controlImmunityFrames = unit.controlImmunityFrames;
    status.mpBlockTimer = unit.mpBlockTimer;
    status.damageImmunityAfterFrames = unit.damageImmunityAfterFrames;
    status.damageImmunityDuration = unit.damageImmunityDuration;
    status.damageImmunityTimer = unit.damageImmunityTimer;
    status.tempAttackBuffs = unit.tempAttackBuffs;
    status.damageReduceDebuffs = unit.damageReduceDebuffs;
}

}  // namespace KysChess::Battle
