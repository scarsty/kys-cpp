#include "BattleSceneUnitStore.h"

#include "BattleReport.h"
#include "BattleSceneRenderMath.h"
#include "Find.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

namespace
{
BattleSceneUnitPresentationState makeInitializedScenePresentationState(
    const KysChess::Battle::BattleRuntimeUnit& runtimeUnit,
    const KysChess::Battle::BattleSetupUnitInput& setup)
{
    BattleSceneUnitPresentationState unit;
    unit.identity = {
        runtimeUnit.id,
        runtimeUnit.realRoleId,
        runtimeUnit.team,
        setup.headId,
        setup.name,
    };
    unit.unitId = runtimeUnit.id;
    unit.sourceUnitId = runtimeUnit.presentationSourceUnitId;
    unit.faceTowards = setup.faceTowards;
    unit.headId = setup.headId;
    unit.fightFrames = setup.fightFrames;
    const bool isClone = runtimeUnit.presentationSourceUnitId != runtimeUnit.id;
    unit.chessInstanceId = isClone ? -1 : setup.chessInstanceId;
    unit.weaponId = isClone ? -1 : setup.weaponId;
    unit.armorId = isClone ? -1 : setup.armorId;
    unit.skillNames = setup.skillNames;
    return unit;
}

}  // namespace

void BattleSceneUnitStore::initialize(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession,
    std::vector<BattleSceneUnitPresentationState> presentationStates)
{
    runtime_session_ = &runtimeSession;
    presentation_ = std::move(presentationStates);
}

void BattleSceneUnitStore::initializeFromRuntimeCreation(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession,
    const KysChess::Battle::BattleRuntimeSessionCreationInput& input)
{
    std::vector<BattleSceneUnitPresentationState> presentation;
    presentation.reserve(runtimeSession.runtimeUnits().size());
    for (const auto& runtimeUnit : runtimeSession.runtimeUnits())
    {
        const auto& setupUnit = KysChess::requireDenseBy(
            input.units,
            runtimeUnit.presentationSourceUnitId,
            &KysChess::Battle::BattleSetupUnitInput::unitId);
        presentation.push_back(makeInitializedScenePresentationState(runtimeUnit, setupUnit));
    }
    initialize(runtimeSession, std::move(presentation));
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

int BattleSceneUnitStore::aliveUnitsOnTeam(int team) const
{
    assert(runtime_session_);
    return static_cast<int>(std::ranges::count_if(
        runtime_session_->runtimeUnits(),
        [team](const KysChess::Battle::BattleRuntimeUnit& unit)
        {
            return unit.team == team && unit.alive;
        }));
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
        const auto& presentation = requirePresentation(source.id);
        BattlePostBattleUnitSummary unit;
        unit.identity = presentation.identity;
        unit.star = source.star;
        unit.chessInstanceId = presentation.chessInstanceId;
        unit.hp = source.vitals.maxHp;
        unit.maxHp = source.vitals.maxHp;
        unit.attack = source.stats.attack;
        unit.defence = source.stats.defence;
        unit.speed = source.stats.speed;
        unit.weaponId = presentation.weaponId;
        unit.armorId = presentation.armorId;
        unit.skillNames = presentation.skillNames;
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
