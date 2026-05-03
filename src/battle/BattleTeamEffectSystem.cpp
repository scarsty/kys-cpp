#include "BattleTeamEffectSystem.h"

#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

BattleTeamEffectUnit& BattleTeamEffectSystem::unitById(BattleTeamEffectWorld& world, int unitId) const
{
    auto it = std::find_if(world.units.begin(), world.units.end(), [&](const BattleTeamEffectUnit& unit)
        {
            return unit.id == unitId;
        });
    assert(it != world.units.end());
    assert(it->alive);
    return *it;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applySelfHeal(BattleTeamEffectWorld& world,
                                                                         int sourceUnitId,
                                                                         int pctHeal) const
{
    assert(pctHeal > 0);

    auto& source = unitById(world, sourceUnitId);
    int amount = source.maxHp * pctHeal / 100;
    assert(amount > 0);

    int before = source.hp;
    source.hp = std::min(source.maxHp, source.hp + amount);
    if (source.hp <= before)
    {
        return {};
    }
    return { { BattleTeamEffectEventType::Heal, sourceUnitId, source.id, source.hp - before, before, source.hp } };
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamHeal(BattleTeamEffectWorld& world,
                                                                         int sourceUnitId,
                                                                         int flatHeal,
                                                                         int pctHeal) const
{
    assert(flatHeal > 0 || pctHeal > 0);

    auto& source = unitById(world, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unit : world.units)
    {
        if (!unit.alive || unit.team != source.team)
        {
            continue;
        }

        int amount = flatHeal + unit.maxHp * pctHeal / 100;
        assert(amount > 0);
        int before = unit.hp;
        unit.hp = std::min(unit.maxHp, unit.hp + amount);
        if (unit.hp > before)
        {
            events.push_back({ BattleTeamEffectEventType::Heal, sourceUnitId, unit.id, unit.hp - before, before, unit.hp });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamMp(BattleTeamEffectWorld& world,
                                                                       int sourceUnitId,
                                                                       int amount) const
{
    assert(amount > 0);

    auto& source = unitById(world, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unit : world.units)
    {
        if (!unit.alive || unit.team != source.team)
        {
            continue;
        }

        int restore = adjustedMpRestore(unit.mpBlocked, unit.mpRecoveryBonusPct, amount);
        int before = unit.mp;
        unit.mp = std::min(unit.maxMp, unit.mp + restore);
        if (unit.mp > before)
        {
            events.push_back({ BattleTeamEffectEventType::MpRestore, sourceUnitId, unit.id, unit.mp - before, before, unit.mp });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamShield(BattleTeamEffectWorld& world,
                                                                           int sourceUnitId,
                                                                           int amount,
                                                                           bool refreshOnly) const
{
    assert(amount > 0);

    auto& source = unitById(world, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unit : world.units)
    {
        if (!unit.alive || unit.team != source.team)
        {
            continue;
        }

        int before = unit.shield;
        unit.shield = refreshOnly ? std::max(unit.shield, amount) : unit.shield + amount;
        if (unit.shield > before)
        {
            events.push_back({ BattleTeamEffectEventType::ShieldGain, sourceUnitId, unit.id, unit.shield - before, before, unit.shield });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyHealAura(BattleTeamEffectWorld& world,
                                                                         int sourceUnitId,
                                                                         int flatHeal,
                                                                         int pctHeal,
                                                                         double radius,
                                                                         int healedCooldownReducePct) const
{
    assert(flatHeal > 0 || pctHeal > 0);
    assert(radius > 0.0);
    assert(healedCooldownReducePct >= 0);

    auto& source = unitById(world, sourceUnitId);
    double radiusSquared = radius * radius;
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unit : world.units)
    {
        if (!unit.alive || unit.id == source.id || unit.team != source.team)
        {
            continue;
        }

        double dx = unit.x - source.x;
        double dy = unit.y - source.y;
        if (dx * dx + dy * dy > radiusSquared)
        {
            continue;
        }

        int amount = flatHeal + unit.maxHp * pctHeal / 100;
        assert(amount > 0);
        int hpBefore = unit.hp;
        unit.hp = std::min(unit.maxHp, unit.hp + amount);
        if (unit.hp > hpBefore)
        {
            events.push_back({ BattleTeamEffectEventType::Heal, sourceUnitId, unit.id, unit.hp - hpBefore, hpBefore, unit.hp });
        }

        if (healedCooldownReducePct > 0 && unit.cooldown > 0)
        {
            int cooldownBefore = unit.cooldown;
            unit.cooldown = static_cast<int>(unit.cooldown * (1.0 - healedCooldownReducePct / 100.0));
            unit.cooldown = std::max(0, unit.cooldown);
            if (unit.cooldown < cooldownBefore)
            {
                events.push_back({
                    BattleTeamEffectEventType::CooldownReduced,
                    sourceUnitId,
                    unit.id,
                    cooldownBefore - unit.cooldown,
                    cooldownBefore,
                    unit.cooldown,
                });
            }
        }
    }
    return events;
}

}  // namespace KysChess::Battle
