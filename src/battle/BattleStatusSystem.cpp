#include "BattleStatusSystem.h"

#include <algorithm>

namespace KysChess::Battle
{

BattleStatusSystem::BattleStatusSystem(BattleStatusSystemConfig config)
    : config_(config)
{
}

BattleStatusTickResult BattleStatusSystem::tick(BattleStatusUnitState& unit) const
{
    BattleStatusTickResult result;
    if (!unit.alive)
    {
        return result;
    }

    tickTempAttackBuffs(unit, result);
    tickPoison(unit, result);
    tickBleed(unit, result);
    tickSimpleTimers(unit);
    tickPeriodicInvincibility(unit, result);
    return result;
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

void BattleStatusSystem::tickTempAttackBuffs(BattleStatusUnitState& unit, BattleStatusTickResult& result) const
{
    for (auto it = unit.tempAttackBuffs.begin(); it != unit.tempAttackBuffs.end();)
    {
        if (it->remainingFrames > 0)
        {
            --it->remainingFrames;
        }

        if (it->remainingFrames <= 0)
        {
            unit.attack -= it->attackBonus;
            result.events.push_back({
                BattleStatusEventType::TempAttackExpired,
                unit.id,
                unit.id,
                it->attackBonus,
                "臨時攻擊到期",
            });
            it = unit.tempAttackBuffs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BattleStatusSystem::tickPoison(BattleStatusUnitState& unit, BattleStatusTickResult& result) const
{
    if (unit.poisonTimer <= 0)
    {
        return;
    }

    --unit.poisonTimer;
    if (config_.poisonDamageIntervalFrames > 0 && config_.frame % config_.poisonDamageIntervalFrames == 0)
    {
        int damage = std::max(1, unit.hp * unit.poisonTickPct / 100);
        result.events.push_back({
            BattleStatusEventType::PoisonDamage,
            unit.id,
            unit.poisonSourceId,
            damage,
            "中毒",
        });
    }

    if (unit.poisonTimer <= 0)
    {
        unit.poisonSourceId = -1;
    }
}

void BattleStatusSystem::tickBleed(BattleStatusUnitState& unit, BattleStatusTickResult& result) const
{
    if (unit.bleedStacks <= 0)
    {
        unit.bleedTimer = 0;
        unit.bleedSourceId = -1;
        return;
    }

    if (unit.bleedTimer > 0)
    {
        --unit.bleedTimer;
    }

    if (unit.bleedTimer <= 0)
    {
        int damage = std::max(1, unit.maxHp * unit.bleedStacks / 100);
        result.events.push_back({
            BattleStatusEventType::BleedDamage,
            unit.id,
            unit.bleedSourceId,
            damage,
            "流血",
        });
        unit.bleedTimer = config_.bleedDamageIntervalFrames;
    }
}

void BattleStatusSystem::tickSimpleTimers(BattleStatusUnitState& unit) const
{
    if (unit.mpBlockTimer > 0)
    {
        --unit.mpBlockTimer;
    }

    for (auto it = unit.damageReduceDebuffs.begin(); it != unit.damageReduceDebuffs.end();)
    {
        if (--it->remainingFrames <= 0)
        {
            it = unit.damageReduceDebuffs.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void BattleStatusSystem::tickPeriodicInvincibility(BattleStatusUnitState& unit, BattleStatusTickResult& result) const
{
    if (unit.damageImmunityAfterFrames <= 0)
    {
        return;
    }

    if (unit.damageImmunityTimer > 0)
    {
        --unit.damageImmunityTimer;
    }

    if (unit.damageImmunityTimer <= 0)
    {
        int before = unit.invincible;
        unit.invincible = std::max(unit.invincible, unit.damageImmunityDuration);
        unit.damageImmunityTimer = unit.damageImmunityAfterFrames;
        if (unit.invincible > before)
        {
            result.events.push_back({
                BattleStatusEventType::InvincibilityGranted,
                unit.id,
                unit.id,
                unit.damageImmunityDuration,
                "周期免傷",
            });
        }
    }
}

}  // namespace KysChess::Battle
