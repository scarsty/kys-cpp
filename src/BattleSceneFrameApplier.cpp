#include "BattleSceneFrameApplier.h"

#include <algorithm>
#include <cassert>
#include <utility>

BattleSceneFrameApplier::BattleSceneFrameApplier(Bindings bindings)
    : bindings_(bindings)
{
}

void BattleSceneFrameApplier::recordLog(
    const KysChess::Battle::BattleLogEvent& event) const
{
    using KysChess::Battle::BattleLogEventType;
    switch (event.type)
    {
    case BattleLogEventType::Damage:
        bindings_.report.recordDamage(
            resolveIdentity(event.sourceUnitId),
            resolveIdentity(event.targetUnitId),
            event.amount,
            event.skillName,
            event.frame,
            event.segments);
        break;
    case BattleLogEventType::Heal:
        bindings_.report.recordHeal(
            resolveIdentity(event.sourceUnitId),
            resolveIdentity(event.targetUnitId),
            event.amount,
            event.segments,
            event.frame);
        break;
    case BattleLogEventType::Status:
        if (event.category == KysChess::Battle::BattleLogCategory::ProjectileCancel)
        {
            bindings_.report.recordProjectileCancel(event.sourceUnitId, event.amount);
            bindings_.report.recordProjectileCancel(event.targetUnitId, event.secondaryAmount);
        }
        bindings_.report.recordStatus(
            resolveIdentity(event.sourceUnitId),
            resolveIdentity(event.targetUnitId),
            event.category,
            event.perspective,
            event.segments,
            event.frame);
        break;
    case BattleLogEventType::UnitDied:
    {
        const auto* killer = resolveIdentity(event.sourceUnitId);
        const auto* victim = resolveIdentity(event.targetUnitId);
        bindings_.report.recordKill(killer, victim, event.frame);
        bindings_.report.recordDeath(victim, event.frame);
        break;
    }
    case BattleLogEventType::BattleEnded:
        bindings_.report.recordBattleEnd(event.frame, event.amount);
        break;
    }
}

const BattleUnitIdentity* BattleSceneFrameApplier::resolveIdentity(int unitId) const
{
    if (unitId < 0)
    {
        return nullptr;
    }
    return &bindings_.units.requirePresentation(unitId).identity;
}

std::optional<BattleSceneFrameApplier::UnitView> BattleSceneFrameApplier::resolveUnitView(int unitId) const
{
    if (unitId < 0)
    {
        return std::nullopt;
    }
    const auto& unit = bindings_.units.requireRuntimeUnit(unitId);
    return UnitView{
        unit.motion.position,
        unit.team,
        unit.vitals.maxHp,
    };
}

int BattleSceneFrameApplier::resolveVisualTeam(int unitId) const
{
    auto unit = resolveUnitView(unitId);
    return unit ? unit->team : -1;
}

BattleAttackEffect* BattleSceneFrameApplier::findProjectile(int projectileAttackId) const
{
    auto it = std::find_if(bindings_.attackEffects.begin(), bindings_.attackEffects.end(), [&](const auto& effect)
        {
            return effect.VisualAttackId == projectileAttackId;
        });
    return it == bindings_.attackEffects.end() ? nullptr : &*it;
}

BattleAttackEffect& BattleSceneFrameApplier::createProjectile(
    const KysChess::Battle::BattleVisualEvent& event) const
{
    assert(event.effectId >= 0);

    BattleAttackEffect effect;
    effect.VisualAttackId = event.effectId;
    effect.VisualOnly = 1;
    effect.Frame = 0;
    bindings_.attackEffects.push_back(std::move(effect));
    return bindings_.attackEffects.back();
}

void BattleSceneFrameApplier::cancelProjectile(
    const KysChess::Battle::BattleVisualEvent& event) const
{
    if (auto* effect = findProjectile(event.effectId))
    {
        BattleSceneFrameApplierDetail::finishProjectile(*effect, 5);
    }
    const int relatedAttackId = BattleSceneFrameApplierDetail::projectileRelatedAttackIdFor(event);
    if (relatedAttackId >= 0)
    {
        if (auto* other = findProjectile(relatedAttackId))
        {
            BattleSceneFrameApplierDetail::finishProjectile(*other, 5);
        }
    }
}
