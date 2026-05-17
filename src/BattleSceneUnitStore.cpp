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
    const KysChess::Battle::BattleSetupUnitInput& setup,
    const KysChess::Battle::BattleInitializationCloneIntent* clone)
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
    unit.sourceUnitId = clone ? clone->sourceUnitId : runtimeUnit.id;
    unit.faceTowards = setup.faceTowards;
    unit.headId = setup.headId;
    unit.fightFrames = setup.fightFrames;
    unit.chessInstanceId = clone ? -1 : setup.chessInstanceId;
    unit.weaponId = clone ? -1 : setup.weaponId;
    unit.armorId = clone ? -1 : setup.armorId;
    unit.skillNames = setup.skillNames;
    return unit;
}

BattleSceneSetupPlacementState makeInitialSceneSetupPlacement(
    const KysChess::Battle::BattleRuntimeUnit& runtimeUnit,
    const KysChess::Battle::BattleSetupUnitInput& setup,
    const KysChess::Battle::BattleInitializationCloneIntent* clone)
{
    return {
        runtimeUnit.id,
        clone ? clone->gridX : setup.gridX,
        clone ? clone->gridY : setup.gridY,
        setup.faceTowards,
        runtimeUnit.motion.position,
        runtimeUnit.alive,
    };
}
}  // namespace

void BattleSceneUnitStore::initialize(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession,
    std::vector<BattleSceneUnitPresentationState> presentationStates,
    std::vector<BattleSceneSetupPlacementState> setupPlacements)
{
    runtime_session_ = &runtimeSession;
    presentation_ = std::move(presentationStates);
    setup_placements_ = std::move(setupPlacements);
    assert(presentation_.size() == setup_placements_.size());
}

void BattleSceneUnitStore::initializeFromRuntimeCreation(
    const KysChess::Battle::BattleRuntimeSession& runtimeSession,
    const KysChess::Battle::BattleRuntimeSessionCreationInput& input,
    const KysChess::Battle::BattleInitializationResult& initialization)
{
    std::vector<BattleSceneUnitPresentationState> presentation;
    std::vector<BattleSceneSetupPlacementState> setupPlacements;
    presentation.reserve(runtimeSession.runtimeUnits().size());
    setupPlacements.reserve(runtimeSession.runtimeUnits().size());
    for (const auto& runtimeUnit : runtimeSession.runtimeUnits())
    {
        const auto* clone = KysChess::tryFindBy(
            initialization.cloneIntents,
            runtimeUnit.id,
            &KysChess::Battle::BattleInitializationCloneIntent::cloneUnitId);
        const auto& setupUnit = clone
            ? KysChess::requireDenseBy(input.units, clone->sourceUnitId, &KysChess::Battle::BattleSetupUnitInput::unitId)
            : KysChess::requireDenseBy(input.units, runtimeUnit.id, &KysChess::Battle::BattleSetupUnitInput::unitId);
        presentation.push_back(makeInitializedScenePresentationState(runtimeUnit, setupUnit, clone));
        setupPlacements.push_back(makeInitialSceneSetupPlacement(runtimeUnit, setupUnit, clone));
    }
    initialize(runtimeSession, std::move(presentation), std::move(setupPlacements));
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

const BattleSceneSetupPlacementState& BattleSceneUnitStore::requireSetupPlacement(int unitId) const
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < setup_placements_.size());
    assert(setup_placements_[unitId].unitId == unitId);
    return setup_placements_[unitId];
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

void BattleSceneUnitStore::swapSetupUnitPositions(int firstUnitId, int secondUnitId)
{
    auto& first = setup_placements_[firstUnitId];
    auto& second = setup_placements_[secondUnitId];
    assert(first.unitId == firstUnitId);
    assert(second.unitId == secondUnitId);
    std::swap(first.gridX, second.gridX);
    std::swap(first.gridY, second.gridY);
    std::swap(first.position, second.position);
}

KysChess::Battle::BattleSetupPlacementInput BattleSceneUnitStore::makeSetupPlacementInput() const
{
    KysChess::Battle::BattleSetupPlacementInput input;
    input.units.reserve(setup_placements_.size());
    for (const auto& placement : setup_placements_)
    {
        if (!placement.active)
        {
            continue;
        }
        input.units.push_back({
            placement.unitId,
            placement.gridX,
            placement.gridY,
            placement.faceTowards,
        });
    }
    return input;
}

std::vector<KysChess::ChessComboBattleUnitRef> BattleSceneUnitStore::makeComboBattleUnitRefs() const
{
    assert(runtime_session_);
    std::vector<KysChess::ChessComboBattleUnitRef> refs;
    auto units = runtime_session_->runtimeUnits();
    refs.reserve(units.size());
    for (const auto& unit : units)
    {
        refs.push_back({
            unit.id,
            unit.realRoleId,
            unit.team,
            unit.alive,
            unit.cost,
        });
    }
    return refs;
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
