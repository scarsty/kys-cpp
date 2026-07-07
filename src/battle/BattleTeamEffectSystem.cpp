#include "BattleTeamEffectSystem.h"

#include "../Find.h"
#include "BattleRuntimeUnits.h"
#include "BattleResourceRules.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

BattleRuntimeUnit& BattleTeamEffectSystem::unitById(BattleRuntimeUnits& units, int unitId) const
{
    auto& unit = units.requireCore(unitId);
    assert(unit.alive);
    return unit;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applySelfHeal(BattleRuntimeUnits& units,
                                                                         int sourceUnitId,
                                                                         int pctHeal) const
{
    assert(pctHeal > 0);

    auto& source = unitById(units, sourceUnitId);
    int amount = source.vitals.maxHp * pctHeal / 100;
    assert(amount > 0);

    int before = source.vitals.hp;
    source.vitals.hp = std::min(source.vitals.maxHp, source.vitals.hp + amount);
    if (source.vitals.hp <= before)
    {
        return {};
    }
    return { { BattleTeamEffectEventType::Heal,
               sourceUnitId,
               source.id,
               source.vitals.hp - before,
               before,
               source.vitals.hp } };
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamHeal(BattleRuntimeUnits& units,
                                                                         int sourceUnitId,
                                                                         int flatHeal,
                                                                         int pctHeal) const
{
    assert(flatHeal > 0 || pctHeal > 0);

    auto& source = unitById(units, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.team != source.team)
        {
            continue;
        }

        int amount = flatHeal + unit.vitals.maxHp * pctHeal / 100;
        assert(amount > 0);
        int before = unit.vitals.hp;
        unit.vitals.hp = std::min(unit.vitals.maxHp, unit.vitals.hp + amount);
        if (unit.vitals.hp > before)
        {
            events.push_back({ BattleTeamEffectEventType::Heal,
                               sourceUnitId,
                               unit.id,
                               unit.vitals.hp - before,
                               before,
                               unit.vitals.hp });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyLowestAllyHeal(
    BattleRuntimeUnits& units,
    int sourceUnitId,
    int flatHeal,
    int pctHeal) const
{
    assert(flatHeal > 0 || pctHeal > 0);

    auto& source = unitById(units, sourceUnitId);
    BattleRuntimeUnit* target = nullptr;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.team != source.team || unit.vitals.hp >= unit.vitals.maxHp)
        {
            continue;
        }
        if (!target)
        {
            target = &unit;
            continue;
        }
        const long long lhs = static_cast<long long>(unit.vitals.hp) * target->vitals.maxHp;
        const long long rhs = static_cast<long long>(target->vitals.hp) * unit.vitals.maxHp;
        if (lhs < rhs || (lhs == rhs && unit.id < target->id))
        {
            target = &unit;
        }
    }
    if (!target)
    {
        return {};
    }

    const int amount = flatHeal + target->vitals.maxHp * pctHeal / 100;
    assert(amount > 0);
    const int before = target->vitals.hp;
    target->vitals.hp = std::min(target->vitals.maxHp, target->vitals.hp + amount);
    if (target->vitals.hp <= before)
    {
        return {};
    }
    return { { BattleTeamEffectEventType::Heal,
               sourceUnitId,
               target->id,
               target->vitals.hp - before,
               before,
               target->vitals.hp } };
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamMp(BattleRuntimeUnits& units,
                                                                       int sourceUnitId,
                                                                       int amount) const
{
    assert(amount > 0);

    auto& source = unitById(units, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.team != source.team)
        {
            continue;
        }

        const auto& record = unitRecord;
        const int recoveryBonus = record.sumAlways(EffectType::MPRecoveryBonus);
        const auto& status = record.status;
        int restore = adjustedMpRestore(status.effects.mpBlockTimer > 0, recoveryBonus, amount);
        int before = unit.vitals.mp;
        unit.vitals.mp = std::min(unit.vitals.maxMp, unit.vitals.mp + restore);
        if (unit.vitals.mp > before)
        {
            events.push_back({ BattleTeamEffectEventType::MpRestore,
                               sourceUnitId,
                               unit.id,
                               unit.vitals.mp - before,
                               before,
                               unit.vitals.mp });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyEnemyMpDamageAll(BattleRuntimeUnits& units,
                                                                                 int sourceUnitId,
                                                                                 int amount) const
{
    assert(amount > 0);

    auto& source = unitById(units, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.team == source.team)
        {
            continue;
        }

        int before = unit.vitals.mp;
        unit.vitals.mp = std::max(0, unit.vitals.mp - amount);
        if (unit.vitals.mp < before)
        {
            events.push_back({ BattleTeamEffectEventType::MpDamage,
                               sourceUnitId,
                               unit.id,
                               before - unit.vitals.mp,
                               before,
                               unit.vitals.mp });
        }
    }
    return events;
}

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyTeamShield(BattleRuntimeUnits& units,
                                                                           int sourceUnitId,
                                                                           int amount,
                                                                           bool refreshOnly) const
{
    assert(amount > 0);

    auto& source = unitById(units, sourceUnitId);
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.team != source.team)
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

std::vector<BattleTeamEffectEvent> BattleTeamEffectSystem::applyHealAura(BattleRuntimeUnits& units,
                                                                         int sourceUnitId,
                                                                         int flatHeal,
                                                                         int pctHeal,
                                                                         double radius,
                                                                         int healedCooldownReducePct) const
{
    assert(flatHeal > 0 || pctHeal > 0);
    assert(radius > 0.0);
    assert(healedCooldownReducePct >= 0);

    auto& source = unitById(units, sourceUnitId);
    double radiusSquared = radius * radius;
    std::vector<BattleTeamEffectEvent> events;
    for (auto& unitRecord : units.live())
    {
        auto& unit = unitRecord.core;
        if (unit.id == source.id || unit.team != source.team)
        {
            continue;
        }

        double dx = unit.motion.position.x - source.motion.position.x;
        double dy = unit.motion.position.y - source.motion.position.y;
        if (dx * dx + dy * dy > radiusSquared)
        {
            continue;
        }

        int amount = flatHeal + unit.vitals.maxHp * pctHeal / 100;
        assert(amount > 0);
        int hpBefore = unit.vitals.hp;
        unit.vitals.hp = std::min(unit.vitals.maxHp, unit.vitals.hp + amount);
        if (unit.vitals.hp > hpBefore)
        {
            events.push_back({ BattleTeamEffectEventType::Heal,
                               sourceUnitId,
                               unit.id,
                               unit.vitals.hp - hpBefore,
                               hpBefore,
                               unit.vitals.hp });
        }

        if (healedCooldownReducePct > 0 && unit.animation.cooldown > 0)
        {
            int cooldownBefore = unit.animation.cooldown;
            unit.animation.cooldown = static_cast<int>(
                unit.animation.cooldown * (1.0 - healedCooldownReducePct / 100.0));
            unit.animation.cooldown = std::max(0, unit.animation.cooldown);
            if (unit.animation.cooldown < cooldownBefore)
            {
                events.push_back({
                    BattleTeamEffectEventType::CooldownReduced,
                    sourceUnitId,
                    unit.id,
                    cooldownBefore - unit.animation.cooldown,
                    cooldownBefore,
                    unit.animation.cooldown,
                });
            }
        }
    }
    return events;
}

}  // namespace KysChess::Battle
