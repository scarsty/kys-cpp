#include "BattleReport.h"

#include <cassert>
#include <utility>

int BattleReport::cancelDamageForUnit(int unitId) const
{
    const auto it = cancelDamageByUnit_.find(unitId);
    return it == cancelDamageByUnit_.end() ? 0 : it->second;
}

void BattleReportBuilder::recordDamage(
    const BattleUnitIdentity* attacker,
    const BattleUnitIdentity* defender,
    int damage,
    const std::string& skillName,
    int frame,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments)
{
    if (attacker)
    {
        auto& stats = report_.stats_[attacker->battleId];
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
        report_.stats_[defender->battleId].damageTaken += damage;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Damage;
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
    event.segments = std::move(segments);
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordHeal(
    const BattleUnitIdentity* source,
    const BattleUnitIdentity* target,
    int amount,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments,
    int frame)
{
    if (amount <= 0)
    {
        return;
    }

    if (source)
    {
        report_.stats_[source->battleId].lastActiveFrame = frame;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Heal;
    event.frame = frame;
    event.value = amount;
    event.segments = std::move(segments);
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
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordStatus(
    const BattleUnitIdentity* source,
    const BattleUnitIdentity* target,
    KysChess::Battle::BattleLogCategory category,
    KysChess::Battle::BattleLogPerspective perspective,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments,
    int frame)
{
    if (segments.empty())
    {
        return;
    }

    if (source)
    {
        report_.stats_[source->battleId].lastActiveFrame = frame;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Status;
    event.frame = frame;
    event.category = category;
    event.perspective = perspective;
    event.segments = std::move(segments);
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
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordKill(const BattleUnitIdentity* killer, const BattleUnitIdentity* victim, int frame)
{
    if (killer)
    {
        report_.stats_[killer->battleId].kills++;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Kill;
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
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordDeath(const BattleUnitIdentity* unit, int frame)
{
    if (unit)
    {
        report_.stats_[unit->battleId].lastActiveFrame = frame;
    }
    if (!unit)
    {
        return;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Death;
    event.frame = frame;
    event.targetId = unit->battleId;
    event.targetTeam = unit->team;
    event.targetName = unit->name;
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordProjectileCancel(int unitId, int damage)
{
    assert(unitId >= 0);
    assert(damage >= 0);
    report_.cancelDamageByUnit_[unitId] += damage;
}

void BattleReportBuilder::recordBattleEnd(int frame, int battleResult)
{
    report_.battleEndFrame_ = frame;
    report_.battleResult_ = battleResult;

    BattleReportEvent event;
    event.type = BattleReportEventType::BattleEnd;
    event.frame = frame;
    event.value = battleResult;
    report_.events_.push_back(std::move(event));
}
