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

int BattleReport::projectilePotentialDamageCancelledForUnit(int unitId) const
{
    const auto it = projectilePotentialDamageCancelledByUnit_.find(unitId);
    return it == projectilePotentialDamageCancelledByUnit_.end() ? 0 : it->second;
}

int BattleReport::projectileCancellationCountForUnit(int unitId) const
{
    const auto it = projectileCancellationCountByUnit_.find(unitId);
    return it == projectileCancellationCountByUnit_.end() ? 0 : it->second;
}

void BattleReportBuilder::recordDamage(
    const KysChess::Battle::BattleRuntimeUnit* attacker,
    const KysChess::Battle::BattleRuntimeUnit* defender,
    int damage,
    const std::string& skillName,
    int frame,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments,
    int skillId)
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
        if (skillId >= 0)
        {
            stats.damagePerSkillId[skillId] += damage;
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
    event.skillId = skillId;
    event.resourceId = KysChess::Battle::BattleResourceSemanticId::HitPoints;
    event.segments = std::move(segments);
    report_.events_.push_back(std::move(event));
}

void BattleReportBuilder::recordHeal(
    const KysChess::Battle::BattleRuntimeUnit* source,
    const KysChess::Battle::BattleRuntimeUnit* target,
    int amount,
    std::vector<KysChess::Battle::BattleLogTextSegment> segments,
    int frame,
    KysChess::Battle::BattleResourceSemanticId resourceId)
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
    event.resourceId = resourceId;
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
    int frame,
    KysChess::Battle::BattleStatusSemanticId statusId,
    KysChess::Battle::BattleResourceSemanticId resourceId,
    int value,
    int secondaryValue,
    int previousValue,
    int newValue,
    int effectId,
    int secondaryEffectId,
    int semanticSourceTeam,
    std::string semanticSourceKind,
    std::string semanticSourceName,
    std::string skillName,
    int skillId)
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
    event.statusId = statusId;
    event.resourceId = resourceId;
    event.value = value;
    event.secondaryValue = secondaryValue;
    event.previousValue = previousValue;
    event.newValue = newValue;
    event.effectId = effectId;
    event.secondaryEffectId = secondaryEffectId;
    event.sourceKind = std::move(semanticSourceKind);
    event.skillName = std::move(skillName);
    event.skillId = skillId;
    event.segments = std::move(segments);
    if (source)
    {
        assignSource(event, *source);
    }
    else if (semanticSourceTeam >= 0)
    {
        event.sourceTeam = semanticSourceTeam;
    }
    if (!semanticSourceName.empty())
    {
        event.sourceName = std::move(semanticSourceName);
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
    report_.projectilePotentialDamageCancelledByUnit_[unitId] += damage;
    ++report_.projectileCancellationCountByUnit_[unitId];
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
