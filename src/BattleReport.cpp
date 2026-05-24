#include "BattleReport.h"

#include "battle/BattleUnitStore.h"

#include <cassert>
#include <utility>

namespace
{
void assignSource(BattleReportEvent& event, const KysChess::Battle::BattleRuntimeUnit& unit)
{
    event.sourceId = unit.id;
    event.sourceTeam = unit.team;
    event.sourceName = unit.name;
}

void assignTarget(BattleReportEvent& event, const KysChess::Battle::BattleRuntimeUnit& unit)
{
    event.targetId = unit.id;
    event.targetTeam = unit.team;
    event.targetName = unit.name;
}
}  // namespace

int BattleReport::cancelDamageForUnit(int unitId) const
{
    const auto it = cancelDamageByUnit_.find(unitId);
    return it == cancelDamageByUnit_.end() ? 0 : it->second;
}

void BattleReportBuilder::recordDamage(
    const KysChess::Battle::BattleRuntimeUnit* attacker,
    const KysChess::Battle::BattleRuntimeUnit* defender,
    int damage,
    const std::string& skillName,
    int frame,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments)
{
    if (attacker)
    {
        auto& stats = report_.stats_[attacker->id];
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
        report_.stats_[defender->id].damageTaken += damage;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Damage;
    event.frame = frame;
    event.value = damage;
    if (attacker)
    {
        assignSource(event, *attacker);
    }
    if (defender)
    {
        assignTarget(event, *defender);
    }
    event.skillName = skillName;
    event.segments = std::move(segments);
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordHeal(
    const KysChess::Battle::BattleRuntimeUnit* source,
    const KysChess::Battle::BattleRuntimeUnit* target,
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
        report_.stats_[source->id].lastActiveFrame = frame;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Heal;
    event.frame = frame;
    event.value = amount;
    event.segments = std::move(segments);
    if (source)
    {
        assignSource(event, *source);
    }
    if (target)
    {
        assignTarget(event, *target);
    }
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordStatus(
    const KysChess::Battle::BattleRuntimeUnit* source,
    const KysChess::Battle::BattleRuntimeUnit* target,
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
        report_.stats_[source->id].lastActiveFrame = frame;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Status;
    event.frame = frame;
    event.category = category;
    event.perspective = perspective;
    event.segments = std::move(segments);
    if (source)
    {
        assignSource(event, *source);
    }
    if (target)
    {
        assignTarget(event, *target);
    }
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordKill(
    const KysChess::Battle::BattleRuntimeUnit* killer,
    const KysChess::Battle::BattleRuntimeUnit* victim,
    int frame)
{
    if (killer)
    {
        report_.stats_[killer->id].kills++;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Kill;
    event.frame = frame;
    if (killer)
    {
        assignSource(event, *killer);
    }
    if (victim)
    {
        assignTarget(event, *victim);
    }
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordDeath(const KysChess::Battle::BattleRuntimeUnit* unit, int frame)
{
    if (unit)
    {
        report_.stats_[unit->id].lastActiveFrame = frame;
    }
    if (!unit)
    {
        return;
    }

    BattleReportEvent event;
    event.type = BattleReportEventType::Death;
    event.frame = frame;
    assignTarget(event, *unit);
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
