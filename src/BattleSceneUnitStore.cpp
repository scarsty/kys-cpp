#include "BattleSceneUnitStore.h"

#include "BattleReport.h"
#include "BattleSceneRenderMath.h"

#include <cassert>
#include <algorithm>
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

std::vector<int> BattleSceneUnitStore::initialize(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession)
{
    runtime_session_ = &runtimeSession;
    presentation_.clear();
    return synchronizeRuntimeUnits();
}

std::vector<int> BattleSceneUnitStore::synchronizeRuntimeUnits()
{
    assert(runtime_session_);
    std::vector<int> addedUnitIds;
    for (const auto& runtimeUnit : runtime_session_->runtimeUnits())
    {
        assert(runtimeUnit.id >= 0);
        const auto requiredSize = static_cast<std::size_t>(runtimeUnit.id + 1);
        if (presentation_.size() < requiredSize)
        {
            presentation_.resize(requiredSize);
        }
        auto& state = presentation_[static_cast<std::size_t>(runtimeUnit.id)];
        if (state.unitId == -1)
        {
            state = makeInitializedScenePresentationState(runtimeUnit);
            addedUnitIds.push_back(runtimeUnit.id);
        }
        else
        {
            assert(state.unitId == runtimeUnit.id);
        }
    }
    return addedUnitIds;
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

void BattleSceneUnitStore::setFightFrames(int unitId, std::array<int, 5> fightFrames)
{
    requirePresentation(unitId).fightFrames = fightFrames;
}

void BattleSceneUnitStore::decreaseTransientPresentationState()
{
    for (auto& state : presentation_)
    {
        BattleSceneRenderMath::decreaseToZero(state.shake);
        BattleSceneRenderMath::decreaseToZero(state.attention);
    }
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
        unit.cancelDmg = report.projectilePotentialDamageCancelledForUnit(source.id);
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
