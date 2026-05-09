#include "BattleStatsView.h"

#include <cassert>
#include <utility>

void BattleTracker::recordDamage(
    const BattleUnitIdentity* attacker,
    const BattleUnitIdentity* defender,
    int damage,
    const std::string& skillName,
    int frame,
    const std::string& detailText)
{
    if (battleResult_ != -1) return;
    if (attacker)
    {
        auto& stats = stats_[attacker->battleId];
        stats.damageDealt += damage;
        if (stats.firstDamageFrame == 0)
        {
            stats.firstDamageFrame = frame;
        }
        stats.lastActiveFrame = frame;
        if (!skillName.empty())
        {
            stats.damagePerSkill[skillName] += damage;
        }
    }
    if (defender)
    {
        stats_[defender->battleId].damageTaken += damage;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Damage;
    event.frame = frame;
    event.value = damage;
    if (attacker)
    {
        event.sourceId = attacker->battleId;
        event.sourceTeam = attacker->team;
        event.sourceName = attacker->name;
    }
    if (defender)
    {
        event.targetId = defender->battleId;
        event.targetTeam = defender->team;
        event.targetName = defender->name;
    }
    event.skillName = skillName;
    event.detailText = detailText;
    events_.push_back(std::move(event));
}

void BattleTracker::recordHeal(
    const BattleUnitIdentity* source,
    const BattleUnitIdentity* target,
    int amount,
    const std::string& reason,
    int frame)
{
    if (battleResult_ != -1) return;
    if (amount <= 0)
    {
        return;
    }

    if (source)
    {
        stats_[source->battleId].lastActiveFrame = frame;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.frame = frame;
    event.value = amount;
    event.detailText = reason;
    if (source)
    {
        event.sourceId = source->battleId;
        event.sourceTeam = source->team;
        event.sourceName = source->name;
    }
    if (target)
    {
        event.targetId = target->battleId;
        event.targetTeam = target->team;
        event.targetName = target->name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordStatus(
    const BattleUnitIdentity* source,
    const BattleUnitIdentity* target,
    const std::string& text,
    int frame)
{
    if (battleResult_ != -1) return;
    if (text.empty())
    {
        return;
    }

    if (source)
    {
        stats_[source->battleId].lastActiveFrame = frame;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.frame = frame;
    event.detailText = text;
    if (source)
    {
        event.sourceId = source->battleId;
        event.sourceTeam = source->team;
        event.sourceName = source->name;
    }
    if (target)
    {
        event.targetId = target->battleId;
        event.targetTeam = target->team;
        event.targetName = target->name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordKill(const BattleUnitIdentity* killer, const BattleUnitIdentity* victim, int frame)
{
    if (battleResult_ != -1) return;
    if (killer)
    {
        stats_[killer->battleId].kills++;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Kill;
    event.frame = frame;
    if (killer)
    {
        event.sourceId = killer->battleId;
        event.sourceTeam = killer->team;
        event.sourceName = killer->name;
    }
    if (victim)
    {
        event.targetId = victim->battleId;
        event.targetTeam = victim->team;
        event.targetName = victim->name;
    }
    events_.push_back(std::move(event));
}

void BattleTracker::recordDeath(const BattleUnitIdentity* unit, int frame)
{
    if (battleResult_ != -1) return;
    if (unit)
    {
        stats_[unit->battleId].lastActiveFrame = frame;
    }
    if (!unit)
    {
        return;
    }

    BattleLogEvent event;
    event.type = BattleLogEventType::Death;
    event.frame = frame;
    event.targetId = unit->battleId;
    event.targetTeam = unit->team;
    event.targetName = unit->name;
    events_.push_back(std::move(event));
}

void BattleTracker::recordProjectileCancel(int unitId, int damage)
{
    assert(unitId >= 0);
    assert(damage >= 0);
    cancelDamageByUnit_[unitId] += damage;
}

int BattleTracker::cancelDamageForUnit(int unitId) const
{
    const auto it = cancelDamageByUnit_.find(unitId);
    return it == cancelDamageByUnit_.end() ? 0 : it->second;
}

void BattleTracker::recordBattleEnd(int frame, int battleResult)
{
    battleEndFrame_ = frame;
    battleResult_ = battleResult;

    BattleLogEvent event;
    event.type = BattleLogEventType::BattleEnd;
    event.frame = frame;
    event.value = battleResult;
    events_.push_back(std::move(event));
}
