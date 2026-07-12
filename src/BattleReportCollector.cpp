#include "BattleReportCollector.h"

#include <algorithm>

namespace
{

const KysChess::Battle::BattleRuntimeUnit* resolveUnit(
    const KysChess::Battle::BattleRuntimeSession& session,
    int unitId)
{
    return unitId < 0 ? nullptr : &session.requireRuntimeUnit(unitId);
}

}

void BattleReportCollector::consumeInitialization(
    const KysChess::Battle::BattleInitializationResult& initialization,
    const KysChess::Battle::BattleRuntimeSession& session)
{
    for (const auto& event : initialization.logEvents)
    {
        consumeLog(event, session);
    }
}

void BattleReportCollector::consumeFrame(
    const KysChess::Battle::BattlePresentationFrame& frame,
    const KysChess::Battle::BattleRuntimeSession& session)
{
    for (const auto& event : frame.logEvents)
    {
        consumeLog(event, session);
    }
}

void BattleReportCollector::consumeLog(
    const KysChess::Battle::BattleLogEvent& event,
    const KysChess::Battle::BattleRuntimeSession& session)
{
    using KysChess::Battle::BattleLogEventType;
    switch (event.type)
    {
    case BattleLogEventType::Damage:
        builder_.recordDamage(
            resolveUnit(session, event.sourceUnitId),
            resolveUnit(session, event.targetUnitId),
            event.amount,
            event.skillName,
            event.frame,
            event.segments,
            event.skillId);
        return;
    case BattleLogEventType::Heal:
        builder_.recordHeal(
            resolveUnit(session, event.sourceUnitId),
            resolveUnit(session, event.targetUnitId),
            event.amount,
            event.segments,
            event.frame,
            event.resourceId);
        return;
    case BattleLogEventType::Status:
        if (event.category == KysChess::Battle::BattleLogCategory::ProjectileCancel)
        {
            const int cancelledPotentialDamage = std::min(event.amount, event.secondaryAmount);
            builder_.recordProjectileCancel(event.sourceUnitId, cancelledPotentialDamage);
            builder_.recordProjectileCancel(event.targetUnitId, cancelledPotentialDamage);
        }
        builder_.recordStatus(
            resolveUnit(session, event.sourceUnitId),
            resolveUnit(session, event.targetUnitId),
            event.category,
            event.perspective,
            event.segments,
            event.frame,
            event.statusId,
            event.resourceId,
            event.amount,
            event.secondaryAmount,
            event.previousAmount,
            event.newAmount,
            event.effectId,
            event.otherEffectId,
            event.semanticSourceTeam,
            event.semanticSourceKind,
            event.semanticSourceName,
            event.skillName,
            event.skillId);
        return;
    case BattleLogEventType::UnitDied:
        builder_.recordKill(
            resolveUnit(session, event.sourceUnitId),
            resolveUnit(session, event.targetUnitId),
            event.frame);
        builder_.recordDeath(resolveUnit(session, event.targetUnitId), event.frame);
        return;
    case BattleLogEventType::BattleEnded:
        builder_.recordBattleEnd(event.frame, event.amount);
        return;
    }
}
