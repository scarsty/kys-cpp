#include "BattleSceneUnitStore.h"

#include "BattleReport.h"
#include "BattleSceneRenderMath.h"

#include <cassert>
#include <utility>

namespace
{
BattleSceneUnitPresentationState makeInitializedScenePresentationState(
    const KysChess::Battle::BattleRuntimeUnit& runtimeUnit)
{
    BattleSceneUnitPresentationState unit;
    unit.unitId = runtimeUnit.id;
    return unit;
}

}  // namespace

void BattleSceneUnitStore::initialize(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession)
{
    std::vector<BattleSceneUnitPresentationState> presentation;
    presentation.reserve(runtimeSession.runtimeUnits().size());
    for (const auto& runtimeUnit : runtimeSession.runtimeUnits())
    {
        presentation.push_back(makeInitializedScenePresentationState(runtimeUnit));
    }
    runtime_session_ = &runtimeSession;
    presentation_ = std::move(presentation);
}

const BattleSceneUnitPresentationState& BattleSceneUnitStore::requirePresentation(int unitId) const
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < presentation_.size());
    assert(presentation_[unitId].unitId == unitId);
    return presentation_[unitId];
}

BattleSceneUnitPresentationState& BattleSceneUnitStore::requirePresentation(int unitId)
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < presentation_.size());
    assert(presentation_[unitId].unitId == unitId);
    return presentation_[unitId];
}

const KysChess::Battle::BattleRuntimeUnit& BattleSceneUnitStore::requireRuntimeUnit(int unitId) const
{
    assert(runtime_session_);
    return runtime_session_->requireRuntimeUnit(unitId);
}

std::span<const KysChess::Battle::BattleRuntimeUnit> BattleSceneUnitStore::runtimeUnits() const
{
    assert(runtime_session_);
    return runtime_session_->runtimeUnits();
}

std::vector<int> BattleSceneUnitStore::allyUnitIds() const
{
    assert(runtime_session_);
    std::vector<int> ids;
    for (const auto& unit : runtime_session_->runtimeUnits())
    {
        if (unit.alive && unit.team == 0)
        {
            ids.push_back(unit.id);
        }
    }
    return ids;
}

void BattleSceneUnitStore::setUnitShake(int unitId, int shake)
{
    requirePresentation(unitId).shake = shake;
}

void BattleSceneUnitStore::decreaseTransientPresentationState()
{
    for (auto& state : presentation_)
    {
        BattleSceneRenderMath::decreaseToZero(state.shake);
        BattleSceneRenderMath::decreaseToZero(state.attention);
    }
}

Pointf BattleSceneUnitStore::facingTowardNearestEnemy(int unitId) const
{
    assert(runtime_session_);
    const auto& source = runtime_session_->requireRuntimeUnit(unitId);
    const KysChess::Battle::BattleRuntimeUnit* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const auto& candidate : runtime_session_->runtimeUnits())
    {
        if (!candidate.alive || candidate.team == source.team)
        {
            continue;
        }
        auto delta = candidate.motion.position - source.motion.position;
        const auto distance = delta.norm();
        if (!nearest || distance < nearestDistance)
        {
            nearest = &candidate;
            nearestDistance = distance;
        }
    }
    if (!nearest)
    {
        return source.motion.facing;
    }

    auto facing = nearest->motion.position - source.motion.position;
    facing.z = 0;
    facing.normTo(1.0f);
    return facing;
}

BattlePostBattleSummary BattleSceneUnitStore::makePostBattleSummary(
    const BattleReport& report,
    int battleResult) const
{
    assert(runtime_session_);
    BattlePostBattleSummary summary;
    summary.battleResult = battleResult;

    auto append = [this, &report](const KysChess::Battle::BattleRuntimeUnit& source, std::vector<BattlePostBattleUnitSummary>& target)
    {
        BattlePostBattleUnitSummary unit;
        unit.identity = source.identity();
        unit.star = source.star;
        unit.chessInstanceId = source.chessInstanceId;
        unit.hp = source.vitals.maxHp;
        unit.maxHp = source.vitals.maxHp;
        unit.attack = source.stats.attack;
        unit.defence = source.stats.defence;
        unit.speed = source.stats.speed;
        unit.weaponId = source.weaponId;
        unit.armorId = source.armorId;
        unit.skillNames = source.skillNames;
        unit.hpRemaining = source.vitals.hp;
        unit.maxHpRemaining = source.vitals.maxHp;
        unit.dead = !source.alive;
        unit.cancelDmg = report.cancelDamageForUnit(source.id);
        target.push_back(std::move(unit));
    };

    for (const auto& unit : runtime_session_->runtimeUnits())
    {
        if (unit.team == 0)
        {
            append(unit, summary.allies);
        }
        else if (unit.team == 1)
        {
            append(unit, summary.enemies);
        }
    }
    return summary;
}
