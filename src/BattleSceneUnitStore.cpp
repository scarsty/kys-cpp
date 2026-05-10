#include "BattleSceneUnitStore.h"

#include "BattleStatsView.h"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <utility>

void BattleSceneUnitStore::initialize(std::vector<BattleSceneUnit> units)
{
    units_ = std::move(units);
    for (std::size_t index = 0; index < units_.size(); ++index)
    {
        assert(units_[index].unitId == static_cast<int>(index));
    }
}

BattleSceneUnit& BattleSceneUnitStore::requireUnit(int unitId)
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < units_.size());
    assert(units_[unitId].unitId == unitId);
    return units_[unitId];
}

const BattleSceneUnit& BattleSceneUnitStore::requireUnit(int unitId) const
{
    assert(unitId >= 0);
    assert(static_cast<std::size_t>(unitId) < units_.size());
    assert(units_[unitId].unitId == unitId);
    return units_[unitId];
}

void BattleSceneUnitStore::swapSetupUnitPositions(
    int firstUnitId,
    int secondUnitId,
    PositionForCell positionForCell)
{
    auto& first = requireUnit(firstUnitId);
    auto& second = requireUnit(secondUnitId);
    std::swap(first.gridX, second.gridX);
    std::swap(first.gridY, second.gridY);
    first.motion.position = positionForCell(first.gridX, first.gridY);
    second.motion.position = positionForCell(second.gridX, second.gridY);
}

KysChess::Battle::BattleSetupPlacementInput BattleSceneUnitStore::makeSetupPlacementInput() const
{
    KysChess::Battle::BattleSetupPlacementInput input;
    input.units.reserve(units_.size());
    for (const auto& unit : units_)
    {
        if (!unit.active)
        {
            continue;
        }
        input.units.push_back({
            unit.unitId,
            unit.gridX,
            unit.gridY,
            unit.faceTowards,
        });
    }
    return input;
}

std::vector<KysChess::ChessComboBattleUnitRef> BattleSceneUnitStore::makeComboBattleUnitRefs() const
{
    std::vector<KysChess::ChessComboBattleUnitRef> refs;
    refs.reserve(units_.size());
    for (const auto& unit : units_)
    {
        refs.push_back({
            unit.unitId,
            unit.identity.realRoleId,
            unit.identity.team,
            unit.alive,
            unit.cost,
        });
    }
    return refs;
}

Pointf BattleSceneUnitStore::facingTowardNearestEnemy(int unitId) const
{
    const auto& source = requireUnit(unitId);
    const BattleSceneUnit* nearest = nullptr;
    float nearestDistance = 0.0f;
    for (const auto& candidate : units_)
    {
        if (!candidate.alive || candidate.identity.team == source.identity.team)
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

int BattleSceneUnitStore::aliveUnitsOnTeam(int team) const
{
    return static_cast<int>(std::ranges::count_if(
        units_,
        [team](const BattleSceneUnit& unit)
        {
            return unit.active && unit.identity.team == team && unit.alive;
        }));
}

std::vector<int> BattleSceneUnitStore::allyUnitIds() const
{
    std::vector<int> ids;
    for (const auto& unit : units_)
    {
        if (unit.active && unit.alive && unit.identity.team == 0)
        {
            ids.push_back(unit.unitId);
        }
    }
    return ids;
}

BattlePostBattleSummary BattleSceneUnitStore::makePostBattleSummary(
    const BattleTracker& tracker,
    int battleResult) const
{
    BattlePostBattleSummary summary;
    summary.battleResult = battleResult;

    auto append = [&tracker](const BattleSceneUnit& source, std::vector<BattlePostBattleUnitSummary>& target)
    {
        BattlePostBattleUnitSummary unit;
        unit.identity = source.identity;
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
        unit.cancelDmg = tracker.cancelDamageForUnit(source.unitId);
        target.push_back(std::move(unit));
    };

    for (const auto& unit : units_)
    {
        if (unit.identity.team == 0)
        {
            append(unit, summary.allies);
        }
        else if (unit.identity.team == 1)
        {
            append(unit, summary.enemies);
        }
    }
    return summary;
}
