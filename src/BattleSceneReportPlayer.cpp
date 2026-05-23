#include "BattleSceneReportPlayer.h"

#include <cassert>

namespace
{
const BattleUnitIdentity* resolveIdentity(const BattleSceneReportPlayer::Bindings& bindings, int unitId)
{
    assert(bindings.units);
    if (unitId < 0)
    {
        return nullptr;
    }
    return &bindings.units->requirePresentation(unitId).identity;
}
}  // namespace

void BattleSceneReportPlayer::playLogs(
    const std::vector<KysChess::Battle::BattleLogEvent>& logEvents,
    const Bindings& bindings) const
{
    assert(bindings.report);
    assert(bindings.units);

    for (const auto& event : logEvents)
    {
        recordLog(event, bindings);
    }
}

void BattleSceneReportPlayer::recordLog(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    using KysChess::Battle::BattleLogEventType;
    switch (event.type)
    {
    case BattleLogEventType::Damage:
        recordDamage(event, bindings);
        break;
    case BattleLogEventType::Heal:
        recordHeal(event, bindings);
        break;
    case BattleLogEventType::Status:
        recordStatus(event, bindings);
        break;
    case BattleLogEventType::UnitDied:
        recordUnitDied(event, bindings);
        break;
    case BattleLogEventType::BattleEnded:
        recordBattleEnded(event, bindings);
        break;
    }
}

void BattleSceneReportPlayer::recordDamage(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.report->recordDamage(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.amount,
        event.skillName,
        event.frame,
        event.segments);
}

void BattleSceneReportPlayer::recordHeal(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.report->recordHeal(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.amount,
        event.segments,
        event.frame);
}

void BattleSceneReportPlayer::recordStatus(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    if (event.category == KysChess::Battle::BattleLogCategory::ProjectileCancel)
    {
        bindings.report->recordProjectileCancel(event.sourceUnitId, event.amount);
        bindings.report->recordProjectileCancel(event.targetUnitId, event.secondaryAmount);
    }
    bindings.report->recordStatus(
        resolveIdentity(bindings, event.sourceUnitId),
        resolveIdentity(bindings, event.targetUnitId),
        event.category,
        event.perspective,
        event.segments,
        event.frame);
}

void BattleSceneReportPlayer::recordUnitDied(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    const auto* killer = resolveIdentity(bindings, event.sourceUnitId);
    const auto* victim = resolveIdentity(bindings, event.targetUnitId);
    bindings.report->recordKill(killer, victim, event.frame);
    bindings.report->recordDeath(victim, event.frame);
}

void BattleSceneReportPlayer::recordBattleEnded(
    const KysChess::Battle::BattleLogEvent& event,
    const Bindings& bindings) const
{
    bindings.report->recordBattleEnd(event.frame, event.amount);
}
