#include "BattleMovement.h"

#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <ranges>
#include <utility>
#include <vector>

namespace KysChess::Battle
{
namespace
{

constexpr double kPi = 3.14159265358979323846;

Pointf rotated(Pointf value, double angle)
{
    value.rotate(angle);
    return value;
}

double distance2d(Pointf a, Pointf b)
{
    return EuclidDis(a.x - b.x, a.y - b.y);
}

Pointf unitVector(Pointf value)
{
    if (value.norm() > 0.01f)
    {
        value.normTo(1.0f);
    }
    return value;
}

double dot2d(Pointf lhs, Pointf rhs)
{
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

int deterministicSide(int unitId, int slot)
{
    return ((unitId * 37 + slot * 17) & 1) == 0 ? 1 : -1;
}

const BattleUnitState* nearestEnemy(const BattleMovementFrameInput& world, const BattleUnitState& unit)
{
    const BattleUnitState* best = nullptr;
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& other : world.units)
    {
        if (!other.alive || other.team == unit.team)
        {
            continue;
        }
        double d = distance2d(unit.position, other.position);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = &other;
        }
    }
    return best;
}

std::vector<int> movementOrder(const BattleMovementFrameInput& world)
{
    std::vector<int> ids;
    for (const auto& unit : world.units)
    {
        if (unit.alive)
        {
            ids.push_back(unit.id);
        }
    }

    std::sort(ids.begin(), ids.end(), [&](int lhsId, int rhsId)
        {
            const auto& lhs = requireById(world.units, lhsId);
            const auto& rhs = requireById(world.units, rhsId);
            const auto* lhsTarget = nearestEnemy(world, lhs);
            const auto* rhsTarget = nearestEnemy(world, rhs);
            double lhsDistance = lhsTarget ? distance2d(lhs.position, lhsTarget->position) : std::numeric_limits<double>::max();
            double rhsDistance = rhsTarget ? distance2d(rhs.position, rhsTarget->position) : std::numeric_limits<double>::max();
            bool lhsAttackReady = lhsTarget && lhs.canAttack && lhsDistance <= lhs.reach;
            bool rhsAttackReady = rhsTarget && rhs.canAttack && rhsDistance <= rhs.reach;
            if (lhsAttackReady != rhsAttackReady)
            {
                return lhsAttackReady;
            }
            if (lhsDistance != rhsDistance)
            {
                return lhsDistance < rhsDistance;
            }
            return lhsId < rhsId;
        });
    return ids;
}

std::vector<Pointf> candidateDirections(const BattleMovementFrameInput& world,
                                        const BattleUnitState& unit,
                                        const BattleUnitState& target,
                                        Pointf desired)
{
    std::vector<Pointf> result;
    auto direct = desired - unit.position;
    if (direct.norm() <= 0.01f)
    {
        direct = target.position - unit.position;
    }
    direct = unitVector(direct);
    result.push_back(direct);

    int side = deterministicSide(unit.id, unit.assignedSlot);
    double smallTurn = kPi / 6.0;
    double largeTurn = kPi / 3.0;
    result.push_back(rotated(direct, smallTurn * side));
    result.push_back(rotated(direct, -smallTurn * side));
    result.push_back(rotated(direct, largeTurn * side));
    result.push_back(rotated(direct, -largeTurn * side));
    return result;
}

int terrainGridCoordCount(const BattleMovementFrameInput& world)
{
    const auto cellCount = static_cast<int>(world.terrainCells.size());
    const int coordCount = static_cast<int>(std::round(std::sqrt(static_cast<double>(cellCount))));
    return coordCount > 0 && coordCount * coordCount == cellCount ? coordCount : 0;
}

std::size_t terrainGridIndex(int coordCount, int x, int y)
{
    assert(coordCount > 0);
    assert(x >= 0 && x < coordCount);
    assert(y >= 0 && y < coordCount);
    return static_cast<std::size_t>(x * coordCount + y);
}

std::optional<int> nearestWalkableTerrainCell(const BattleMovementFrameInput& world, Pointf position)
{
    std::optional<int> best;
    double bestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(world.terrainCells.size()); ++i)
    {
        const auto& cell = world.terrainCells[static_cast<std::size_t>(i)];
        if (!cell.walkable)
        {
            continue;
        }
        const double distance = distance2d(position, cell.position);
        if (!best || distance < bestDistance)
        {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

std::optional<Pointf> nextTerrainPathWaypoint(const BattleMovementFrameInput& world,
                                              Pointf from,
                                              Pointf to)
{
    const int coordCount = terrainGridCoordCount(world);
    if (coordCount <= 0)
    {
        return std::nullopt;
    }

    const auto start = nearestWalkableTerrainCell(world, from);
    const auto goal = nearestWalkableTerrainCell(world, to);
    if (!start || !goal || *start == *goal)
    {
        return std::nullopt;
    }

    const int cellCount = coordCount * coordCount;
    std::vector<double> cost(static_cast<std::size_t>(cellCount), std::numeric_limits<double>::max());
    std::vector<int> previous(static_cast<std::size_t>(cellCount), -1);
    using QueueItem = std::pair<double, int>;
    std::priority_queue<QueueItem, std::vector<QueueItem>, std::greater<>> frontier;

    cost[static_cast<std::size_t>(*start)] = 0.0;
    frontier.push({ 0.0, *start });

    const auto goalPosition = world.terrainCells[static_cast<std::size_t>(*goal)].position;
    constexpr int neighborOffsets[4][2] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 },
    };

    while (!frontier.empty())
    {
        const auto [_, current] = frontier.top();
        frontier.pop();
        if (current == *goal)
        {
            break;
        }

        const int x = current / coordCount;
        const int y = current % coordCount;
        for (const auto& [dx, dy] : neighborOffsets)
        {
            const int nx = x + dx;
            const int ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= coordCount || ny >= coordCount)
            {
                continue;
            }
            const int next = static_cast<int>(terrainGridIndex(coordCount, nx, ny));
            const auto& nextCell = world.terrainCells[static_cast<std::size_t>(next)];
            if (!nextCell.walkable)
            {
                continue;
            }

            const double nextCost = cost[static_cast<std::size_t>(current)]
                + distance2d(
                    world.terrainCells[static_cast<std::size_t>(current)].position,
                    nextCell.position);
            if (nextCost >= cost[static_cast<std::size_t>(next)])
            {
                continue;
            }

            cost[static_cast<std::size_t>(next)] = nextCost;
            previous[static_cast<std::size_t>(next)] = current;
            frontier.push({ nextCost + distance2d(nextCell.position, goalPosition), next });
        }
    }

    if (previous[static_cast<std::size_t>(*goal)] < 0)
    {
        return std::nullopt;
    }

    int waypoint = *goal;
    while (previous[static_cast<std::size_t>(waypoint)] >= 0
        && previous[static_cast<std::size_t>(waypoint)] != *start)
    {
        waypoint = previous[static_cast<std::size_t>(waypoint)];
    }
    return world.terrainCells[static_cast<std::size_t>(waypoint)].position;
}

std::optional<Pointf> terrainPathDirection(const BattleMovementFrameInput& world,
                                           const BattleUnitState& unit,
                                           Pointf desired)
{
    auto waypoint = nextTerrainPathWaypoint(world, unit.position, desired);
    if (!waypoint)
    {
        return std::nullopt;
    }
    auto direction = *waypoint - unit.position;
    if (direction.norm() <= 0.01f)
    {
        return std::nullopt;
    }
    return unitVector(direction);
}

Pointf combatSlotPosition(const BattleMovementFrameInput& world,
                          const BattleUnitState& unit,
                          const BattleUnitState& target,
                          int slot)
{
    auto away = unit.position - target.position;
    if (away.norm() <= 0.01f)
    {
        double angle = (unit.id * 37 + slot * 53) * kPi / 180.0;
        away = Pointf{ static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0f };
    }
    away = unitVector(away);

    double radius = unit.style == CombatStyle::Ranged
        ? std::clamp(unit.reach - world.config.engagementDeadband, world.config.meleeAttackReach, world.config.maxRangedReach)
        : std::max(world.config.engagementArriveDistance, world.config.meleeAttackReach - world.config.engagementDeadband);
    double angleStep = kPi / 4.0;
    int side = deterministicSide(unit.id, slot);
    double angle = angleStep * ((slot + 1) / 2) * side;
    return target.position + rotated(away, angle) * radius;
}

bool reservationConflicts(const BattleMovementFrameInput& world,
                          int unitId,
                          Pointf nextPosition,
                          const std::map<int, Pointf>& reservations)
{
    for (const auto& [reservedBy, pos] : reservations)
    {
        if (reservedBy == unitId)
        {
            continue;
        }
        if (distance2d(nextPosition, pos) < world.config.bodyRadius)
        {
            return true;
        }
    }
    return false;
}

bool softReservationConflicts(const BattleMovementFrameInput& world,
                              int unitId,
                              Pointf nextPosition)
{
    for (const auto& [reservedBy, reservation] : world.movementReservations)
    {
        if (reservedBy == unitId || reservation.unitId == unitId)
        {
            continue;
        }
        const double radius = reservation.radius > 0.0
            ? reservation.radius
            : world.config.bodyRadius;
        if (distance2d(nextPosition, reservation.position) < radius)
        {
            return true;
        }
    }
    return false;
}

double horizontalSpeed(Pointf velocity)
{
    return EuclidDis(velocity.x, velocity.y);
}

void pruneMovementReservations(BattleMovementFrameInput& world)
{
    for (auto it = world.movementReservations.begin(); it != world.movementReservations.end();)
    {
        const auto* unit = tryFindById(world.units, it->first);
        if (!unit || !unit->alive || it->second.expiresFrame <= world.frame)
        {
            it = world.movementReservations.erase(it);
            continue;
        }
        ++it;
    }
}

void clearMovementReservation(BattleMovementFrameInput& world, int unitId)
{
    world.movementReservations.erase(unitId);
}

void reserveMovementDestination(BattleMovementFrameInput& world,
                                const BattleUnitState& unit,
                                Pointf destination)
{
    if (world.config.reservationHorizonFrames <= 0 || battleMovementTaXueUnstable(unit))
    {
        clearMovementReservation(world, unit.id);
        return;
    }

    world.movementReservations[unit.id] = BattleMovementReservation{
        unit.id,
        destination,
        world.config.bodyRadius,
        world.frame + world.config.reservationHorizonFrames + 1,
    };
}

void reserveSameFrameDestination(const BattleUnitState& unit,
                                 Pointf destination,
                                 std::map<int, Pointf>& reservations)
{
    if (battleMovementTaXueUnstable(unit))
    {
        reservations.erase(unit.id);
        return;
    }
    reservations[unit.id] = destination;
}

bool terrainAllows(const BattleMovementFrameInput& world, Pointf position)
{
    if (world.terrainCells.empty())
    {
        return true;
    }

    const BattleTerrainCell* nearest = nullptr;
    double nearestDistance = std::numeric_limits<double>::max();
    for (const auto& cell : world.terrainCells)
    {
        const double distance = distance2d(position, cell.position);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = &cell;
        }
    }
    assert(nearest);
    const double maximumDistance = std::max(1.0, world.config.tileWidth * 1.5);
    if (nearestDistance > maximumDistance)
    {
        return false;
    }
    return nearest->walkable;
}

MoveProbe probeMoveInWorld(const BattleMovementFrameInput& world,
                           const BattleUnitState& unit,
                           Pointf nextPosition,
                           bool ignoreUnits,
                           const std::map<int, Pointf>& reservations,
                           bool ignoreReservations = false,
                           bool allowSoftReservations = true)
{
    if (!terrainAllows(world, nextPosition))
    {
        return { false, MoveBlockReason::Wall, -1 };
    }
    if (!ignoreReservations && reservationConflicts(world, unit.id, nextPosition, reservations))
    {
        return { false, MoveBlockReason::Reservation, -1 };
    }
    if (!ignoreReservations && !allowSoftReservations && softReservationConflicts(world, unit.id, nextPosition))
    {
        return { false, MoveBlockReason::Reservation, -1 };
    }
    if (!ignoreUnits)
    {
        for (const auto& other : world.units)
        {
            if (!other.alive || other.id == unit.id)
            {
                continue;
            }
            if (distance2d(nextPosition, other.position) >= world.config.bodyRadius)
            {
                continue;
            }
            return {
                false,
                other.team == unit.team ? MoveBlockReason::Ally : MoveBlockReason::Enemy,
                other.id,
            };
        }
    }
    return { true, MoveBlockReason::None, -1 };
}

bool terrainSegmentClear(const BattleMovementFrameInput& world, Pointf from, Pointf to)
{
    if (world.terrainCells.empty())
    {
        return true;
    }

    auto delta = to - from;
    double length = delta.norm();
    if (length <= 0.01)
    {
        return terrainAllows(world, to);
    }

    int steps = std::max(1, static_cast<int>(std::ceil(length / std::max(1.0, world.config.engagementDeadband))));
    for (int i = 1; i <= steps; ++i)
    {
        double t = static_cast<double>(i) / steps;
        auto probe = from + delta * t;
        if (!terrainAllows(world, probe))
        {
            return false;
        }
    }
    return true;
}

Point movementPhysicsCell(const BattleMovementPhysicsCollisionWorld& world, Pointf position)
{
    assert(world.tileWidth > 0.0);
    assert(world.coordCount > 0);
    double x = position.x - world.coordCount * world.tileWidth;
    Point cell;
    cell.x = static_cast<int>(std::round((x / world.tileWidth + position.y / world.tileWidth) / 2.0));
    cell.y = static_cast<int>(std::round((-x / world.tileWidth + position.y / world.tileWidth) / 2.0));
    return cell;
}

std::optional<MovementDecision> chooseDash(const BattleMovementFrameInput& world,
                                           const BattleUnitState& unit,
                                           const BattleUnitState& target,
                                           Pointf direction,
                                           const std::map<int, Pointf>& reservations,
                                           bool ignoreReservations,
                                           bool allowSoftReservations);

std::optional<MovementDecision> chooseDashByDistance(const BattleMovementFrameInput& world,
                                                     const BattleUnitState& unit,
                                                     const BattleUnitState& target,
                                                     Pointf direction,
                                                     double minDistance,
                                                     double maxDistance,
                                                     const std::map<int, Pointf>& reservations,
                                                     bool ignoreReservations,
                                                     bool allowSoftReservations)
{
    if (unit.dashCooldownRemaining > 0 || direction.norm() <= 0.01f)
    {
        return std::nullopt;
    }
    direction = unitVector(direction);

    if (maxDistance <= world.config.engagementDeadband)
    {
        return std::nullopt;
    }
    minDistance = std::min(minDistance, maxDistance);

    for (double dashDistance = maxDistance; dashDistance >= minDistance; dashDistance -= world.config.engagementDeadband)
    {
        auto landing = unit.position + direction * dashDistance;
        if (!terrainSegmentClear(world, unit.position, landing))
        {
            continue;
        }
        auto probe = probeMoveInWorld(
            world,
            unit,
            landing,
            true,
            reservations,
            ignoreReservations,
            allowSoftReservations);
        if (probe.canMove)
        {
            MovementDecision decision;
            decision.unitId = unit.id;
            decision.action = MovementAction::Dash;
            decision.targetId = target.id;
            decision.velocity = direction * (dashDistance / std::max(1, world.config.dashFrames));
            decision.destination = landing;
            decision.dashDistance = dashDistance;
            decision.slot = unit.assignedSlot;
            return decision;
        }
    }
    return std::nullopt;
}

double normalDashDistanceFor(const BattleMovementFrameInput& world, const BattleUnitState& unit)
{
    return std::max(
        unit.speed * world.config.dashFrames,
        unit.speed * world.config.dashFrames * world.config.movementDashDistanceMultiplier);
}

double minimumDashDistanceFor(const BattleMovementFrameInput& world, const BattleUnitState& unit)
{
    return unit.taXue
        ? std::max(world.config.tileWidth * 2.0, unit.speed * world.config.dashFrames)
        : unit.speed * world.config.dashFrames;
}

double allowedDashDistanceFor(const BattleMovementFrameInput& world, const BattleUnitState& unit)
{
    const double normalDashDistance = normalDashDistanceFor(world, unit);
    return unit.taXue
        ? world.config.maxDashDistance
        : std::min(world.config.maxDashDistance, normalDashDistance);
}

std::optional<MovementDecision> chooseDash(const BattleMovementFrameInput& world,
                                           const BattleUnitState& unit,
                                           const BattleUnitState& target,
                                           Pointf direction,
                                           const std::map<int, Pointf>& reservations,
                                           bool ignoreReservations,
                                           bool allowSoftReservations)
{

    double targetDistance = distance2d(unit.position, target.position);
    double usefulGap = unit.style == CombatStyle::Melee
        ? targetDistance - world.config.meleeAttackReach + world.config.engagementDeadband
        : targetDistance - unit.reach + world.config.engagementDeadband;
    double minDistance = minimumDashDistanceFor(world, unit);
    double allowedDashDistance = allowedDashDistanceFor(world, unit);
    double maxDistance = std::min(allowedDashDistance, usefulGap);
    return chooseDashByDistance(
        world,
        unit,
        target,
        direction,
        minDistance,
        maxDistance,
        reservations,
        ignoreReservations,
        allowSoftReservations);
}

std::optional<MovementDecision> chooseRetreatDash(const BattleMovementFrameInput& world,
                                                  const BattleUnitState& unit,
                                                  const BattleUnitState& target,
                                                  Pointf direction,
                                                  const std::map<int, Pointf>& reservations,
                                                  bool ignoreReservations,
                                                  bool allowSoftReservations)
{
    return chooseDashByDistance(
        world,
        unit,
        target,
        direction,
        minimumDashDistanceFor(world, unit),
        allowedDashDistanceFor(world, unit),
        reservations,
        ignoreReservations,
        allowSoftReservations);
}

std::optional<MovementDecision> chooseDashWithSoftFallback(const BattleMovementFrameInput& world,
                                                           const BattleUnitState& unit,
                                                           const BattleUnitState& target,
                                                           Pointf direction,
                                                           const std::map<int, Pointf>& reservations,
                                                           bool ignoreReservations)
{
    if (ignoreReservations)
    {
        return chooseDash(world, unit, target, direction, reservations, true, true);
    }
    if (auto dash = chooseDash(world, unit, target, direction, reservations, false, false))
    {
        return dash;
    }
    return chooseDash(world, unit, target, direction, reservations, false, true);
}

std::optional<MovementDecision> chooseRetreatDashWithSoftFallback(const BattleMovementFrameInput& world,
                                                                  const BattleUnitState& unit,
                                                                  const BattleUnitState& target,
                                                                  Pointf direction,
                                                                  const std::map<int, Pointf>& reservations,
                                                                  bool ignoreReservations)
{
    if (ignoreReservations)
    {
        return chooseRetreatDash(world, unit, target, direction, reservations, true, true);
    }
    if (auto dash = chooseRetreatDash(world, unit, target, direction, reservations, false, false))
    {
        return dash;
    }
    return chooseRetreatDash(world, unit, target, direction, reservations, false, true);
}

Pointf meleeChaosDesiredPosition(const BattleMovementFrameInput& world,
                                 const BattleUnitState& unit,
                                 const BattleUnitState& target)
{
    auto away = unit.position - target.position;
    if (away.norm() <= 0.01f)
    {
        away = { 1.0f, 0.0f, 0.0f };
    }
    away = unitVector(away);
    Pointf side{ -away.y, away.x, 0 };
    side = side * static_cast<float>(deterministicSide(unit.id, unit.assignedSlot));
    return unit.position
        + side * static_cast<float>(world.config.tileWidth * 2.0)
        + away * static_cast<float>(world.config.tileWidth * 1.5);
}

Pointf rangedPeelDashDirection(const BattleMovementFrameInput& world,
                               const BattleUnitState& unit,
                               const BattleUnitState& target)
{
    auto away = unit.position - target.position;
    if (away.norm() <= 0.01f)
    {
        away = { static_cast<float>(deterministicSide(unit.id, unit.assignedSlot + world.frame)), 0.0f, 0.0f };
    }
    away = unitVector(away);
    Pointf side{ -away.y, away.x, 0 };
    side = side * static_cast<float>(0.35 * deterministicSide(unit.id, world.frame + unit.assignedSlot));
    return unitVector(away + side);
}

void recordEvent(std::vector<BattleEvent>& events,
                 BattleEventType type,
                 const BattleUnitState& unit,
                 const MovementDecision& decision)
{
    BattleEvent event;
    event.type = type;
    event.unitId = unit.id;
    event.targetId = decision.targetId;
    event.blockerId = decision.blockerId;
    event.slot = decision.slot;
    event.from = unit.position;
    event.to = decision.destination;
    event.value = decision.dashDistance;
    events.push_back(event);
}

}  // namespace

std::size_t movementPhysicsCellIndex(const BattleMovementPhysicsCollisionWorld& world, int x, int y)
{
    assert(world.coordCount > 0);
    assert(x >= 0);
    assert(y >= 0);
    assert(x < world.coordCount);
    assert(y < world.coordCount);
    return static_cast<std::size_t>(y * world.coordCount + x);
}

bool movementPhysicsCellWalkable(const BattleMovementPhysicsCollisionWorld& world, Point cell)
{
    if (cell.x < 0 || cell.y < 0 || cell.x >= world.coordCount || cell.y >= world.coordCount)
    {
        return false;
    }
    const auto index = movementPhysicsCellIndex(world, cell.x, cell.y);
    assert(index < world.walkableByCell.size());
    return world.walkableByCell[index] != 0;
}

bool battleMovementTaXueUnstable(const BattleUnitState& unit)
{
    constexpr double StopThreshold = 0.1;
    return unit.taXue
        && (unit.dashFramesRemaining > 0
            || unit.movementDashSpreadFramesRemaining > 0
            || unit.postDashRetreatFramesRemaining > 0
            || unit.postDashChaosFramesRemaining > 0
            || horizontalSpeed(unit.velocity) > StopThreshold);
}

bool canMoveInPhysicsSnapshot(
    const BattleMovementPhysicsCollisionWorld& world,
    int unitId,
    Pointf currentPosition,
    Pointf nextPosition,
    int separationDistance,
    bool ignoreUnitCollision)
{
    if (currentPosition.z > 1.0f)
    {
        return true;
    }

    if (!ignoreUnitCollision)
    {
        const double separation = separationDistance == -1
            ? world.defaultSeparationDistance
            : static_cast<double>(separationDistance);
        for (const auto& unit : world.units)
        {
            if (!unit.alive || unit.id == unitId)
            {
                continue;
            }
            const double nextDistance = distance2d(nextPosition, unit.position);
            if (nextDistance < separation)
            {
                const double currentDistance = distance2d(currentPosition, unit.position);
                if (currentDistance >= nextDistance)
                {
                    return false;
                }
            }
        }
    }

    return movementPhysicsCellWalkable(world, movementPhysicsCell(world, nextPosition));
}

BattleMovementPlanner::BattleMovementPlanner(BattleMovementFrameInput& world)
    : world_(world)
{
}

BattleMovementPhysicsState BattleMovementPhysicsSystem::advance(const BattleMovementPhysicsInput& input) const
{
    assert(input.collisionWorld);
    assert(input.unitId >= 0);

    auto canMove = [&](Pointf position, int separationDistance)
    {
        return canMoveInPhysicsSnapshot(
            *input.collisionWorld,
            input.unitId,
            input.currentPosition,
            position,
            separationDistance,
            input.ignoreUnitCollision);
    };

    auto state = input.state;
    const bool movementDashActive = state.movementDashFrames > 0;
    const bool movementDashEnding = state.movementDashFrames == 1;
    const bool postDashRetreatActive = state.postDashRetreatFrames > 0;
    if (postDashRetreatActive && !input.actionDashActive && !movementDashActive)
    {
        state.velocity = state.postDashRetreatVelocity;
    }
    const int separationDistance = input.actionDashActive || movementDashActive || postDashRetreatActive ? 1 : -1;
    auto nextPosition = state.position + state.velocity;

    if (canMove(nextPosition, separationDistance))
    {
        state.position = nextPosition;
    }
    else
    {
        bool canSlide = false;
        auto xOnly = state.position;
        xOnly.x = nextPosition.x;
        if (canMove(xOnly, separationDistance))
        {
            state.position = xOnly;
            state.velocity.y = 0;
            canSlide = true;
        }
        else
        {
            auto yOnly = state.position;
            yOnly.y = nextPosition.y;
            if (canMove(yOnly, separationDistance))
            {
                state.position = yOnly;
                state.velocity.x = 0;
                canSlide = true;
            }
        }

        if (!canSlide)
        {
            state.velocity = { 0, 0, 0 };
            state.movementDashFrames = 0;
            state.postDashRetreatFrames = 0;
        }
    }

    if (state.movementDashFrames > 0)
    {
        --state.movementDashFrames;
    }
    if (!movementDashActive && state.postDashRetreatFrames > 0)
    {
        --state.postDashRetreatFrames;
    }
    if (movementDashEnding)
    {
        state.movementDashSpreadFrames = input.config.postDashSpreadFrames;
    }
    else if (!movementDashActive && state.movementDashSpreadFrames > 0)
    {
        --state.movementDashSpreadFrames;
    }
    if (state.movementDashCooldown > 0)
    {
        --state.movementDashCooldown;
    }
    if (state.position.z < 0)
    {
        state.position.z = 0;
    }
    if (state.position.z == 0 && state.velocity.norm() != 0)
    {
        auto friction = -state.velocity;
        friction.normTo(input.config.friction);
        state.acceleration = { friction.x, friction.y, input.config.gravity };
    }
    else
    {
        state.acceleration = { 0, 0, input.config.gravity };
    }
    state.velocity += state.acceleration;
    if (state.position.z == 0)
    {
        state.velocity.z = 0;
    }
    if (state.velocity.norm() < 0.1)
    {
        state.velocity.x = 0;
        state.velocity.y = 0;
    }
    return state;
}

MoveProbe BattleMovementPlanner::probeMove(const BattleUnitState& unit,
                                           Pointf nextPosition,
                                           bool ignoreUnits,
                                           const std::map<int, Pointf>& reservations) const
{
    return probeMoveInWorld(world_, unit, nextPosition, ignoreUnits, reservations);
}

BattleTickResult BattleMovementPlanner::tick()
{
    assert(world_.config.tileWidth > 0.0);
    assert(world_.config.reservationHorizonFrames >= 0);
    assert(world_.config.dashFrames > 0);
    assert(world_.config.dashCooldownFrames >= 0);
    assert(world_.config.slotSwitchCooldownFrames >= 0);
    assert(world_.config.bodyRadius > 0.0);
    assert(world_.config.engagementDeadband > 0.0);
    assert(world_.config.engagementArriveDistance > 0.0);
    assert(world_.config.meleeAttackReach > 0.0);
    assert(world_.config.meleeLocalTargetRadius > 0.0);
    assert(world_.config.maxDashDistance > 0.0);
    assert(world_.config.maxRangedReach > 0.0);
    assert(world_.config.movementDashDistanceMultiplier > 0.0);

    BattleTickResult result;
    result.frame = world_.frame;
    pruneMovementReservations(world_);
    std::map<int, Pointf> reservations;

    for (int unitId : movementOrder(world_))
    {
        auto* unit = &requireById(world_.units, unitId);
        if (!unit->alive)
        {
            clearMovementReservation(world_, unit->id);
            continue;
        }

        MovementDecision decision;
        decision.unitId = unit->id;
        decision.slot = unit->assignedSlot;

        if (unit->postDashRetreatFramesRemaining > 0)
        {
            clearMovementReservation(world_, unit->id);
            decision.destination = unit->position;
            result.decisions[unit->id] = decision;
            continue;
        }

        if (unit->slotSwitchCooldownRemaining > 0)
        {
            unit->slotSwitchCooldownRemaining--;
        }

        if (unit->dashFramesRemaining > 0)
        {
            auto next = unit->position + unit->velocity;
            auto probe = probeMoveInWorld(world_, *unit, next, true, reservations, true, true);
            if (probe.canMove)
            {
                unit->position = next;
                decision.action = MovementAction::Dash;
                decision.velocity = unit->velocity;
                decision.destination = unit->position;
                recordEvent(result.events, BattleEventType::Movement, *unit, decision);
            }
            clearMovementReservation(world_, unit->id);
            unit->dashFramesRemaining--;
            if (unit->dashFramesRemaining <= 0)
            {
                recordEvent(result.events, BattleEventType::DashEnd, *unit, decision);
            }
            result.decisions[unit->id] = decision;
            continue;
        }

        if (unit->dashCooldownRemaining > 0)
        {
            unit->dashCooldownRemaining--;
        }

        const auto* target = nearestEnemy(world_, *unit);
        if (!target)
        {
            clearMovementReservation(world_, unit->id);
            result.decisions[unit->id] = decision;
            continue;
        }

        if (unit->targetId != target->id)
        {
            unit->targetId = target->id;
        }
        decision.targetId = target->id;
        double targetDistance = distance2d(unit->position, target->position);
        const bool meleeChaosActive = unit->style == CombatStyle::Melee
            && unit->postDashChaosFramesRemaining > 0;
        if (unit->taXue
            && unit->style == CombatStyle::Ranged
            && unit->canAttack
            && unit->dashCooldownRemaining <= 0
            && unit->reach > 0.0
            && targetDistance <= unit->reach * 0.65)
        {
            auto dash = chooseRetreatDashWithSoftFallback(
                world_,
                *unit,
                *target,
                rangedPeelDashDirection(world_, *unit, *target),
                reservations,
                true);
            if (dash)
            {
                decision = *dash;
                unit->velocity = decision.velocity;
                unit->dashFramesRemaining = world_.config.dashFrames;
                unit->dashCooldownRemaining = world_.config.dashCooldownFrames;
                reserveSameFrameDestination(*unit, decision.destination, reservations);
                reserveMovementDestination(world_, *unit, decision.destination);
                recordEvent(result.events, BattleEventType::DashStart, *unit, decision);
                result.decisions[unit->id] = decision;
                continue;
            }
        }

        if (unit->taXue
            && meleeChaosActive
            && unit->dashCooldownRemaining <= 0)
        {
            auto dash = chooseRetreatDashWithSoftFallback(
                world_,
                *unit,
                *target,
                meleeChaosDesiredPosition(world_, *unit, *target) - unit->position,
                reservations,
                true);
            if (dash)
            {
                decision = *dash;
                unit->velocity = decision.velocity;
                unit->dashFramesRemaining = world_.config.dashFrames;
                unit->dashCooldownRemaining = world_.config.dashCooldownFrames;
                unit->postDashChaosFramesRemaining = 0;
                reserveSameFrameDestination(*unit, decision.destination, reservations);
                reserveMovementDestination(world_, *unit, decision.destination);
                recordEvent(result.events, BattleEventType::DashStart, *unit, decision);
                result.decisions[unit->id] = decision;
                continue;
            }
        }

        bool canAttackFromHere = unit->canAttack && targetDistance <= unit->reach;
        bool rangedCanHold = unit->style == CombatStyle::Ranged && targetDistance <= unit->reach;
        bool meleeReady = unit->style == CombatStyle::Melee && targetDistance <= world_.config.meleeAttackReach;
        if (meleeChaosActive)
        {
            --unit->postDashChaosFramesRemaining;
        }
        if (!meleeChaosActive && (canAttackFromHere || rangedCanHold || meleeReady))
        {
            decision.action = MovementAction::AttackReady;
            decision.destination = unit->position;
            unit->velocity = { 0, 0, 0 };
            clearMovementReservation(world_, unit->id);
            recordEvent(result.events, BattleEventType::AttackReady, *unit, decision);
            result.decisions[unit->id] = decision;
            continue;
        }

        Pointf desired = combatSlotPosition(world_, *unit, *target, unit->assignedSlot);
        if (meleeChaosActive)
        {
            desired = meleeChaosDesiredPosition(world_, *unit, *target);
        }
        else if (unit->style == CombatStyle::Melee && targetDistance > world_.config.meleeAttackReach)
        {
            desired = target->position;
        }

        bool shouldPlanDash = unit->dashCooldownRemaining <= 0
            && !meleeChaosActive
            && (unit->taXue
                ? targetDistance > world_.config.meleeAttackReach * 1.1
                : targetDistance > (unit->style == CombatStyle::Melee
                    ? world_.config.meleeLocalTargetRadius
                    : unit->reach + world_.config.tileWidth * 2.0));
        if (shouldPlanDash)
        {
            auto dashDirection = desired - unit->position;
            auto dash = chooseDashWithSoftFallback(
                world_,
                *unit,
                *target,
                dashDirection,
                reservations,
                unit->taXue || battleMovementTaXueUnstable(*unit));
            if (dash)
            {
                decision = *dash;
                unit->velocity = decision.velocity;
                unit->dashFramesRemaining = world_.config.dashFrames;
                unit->dashCooldownRemaining = unit->taXue
                    ? std::max(world_.config.dashFrames, world_.config.dashCooldownFrames / 2)
                    : world_.config.dashCooldownFrames;
                reserveSameFrameDestination(*unit, decision.destination, reservations);
                reserveMovementDestination(world_, *unit, decision.destination);
                recordEvent(result.events, BattleEventType::DashStart, *unit, decision);
                result.decisions[unit->id] = decision;
                continue;
            }
        }

        bool moved = false;
        MoveProbe lastProbe;
        auto directions = candidateDirections(world_, *unit, *target, desired);
        if (!terrainSegmentClear(world_, unit->position, desired))
        {
            if (auto pathDirection = terrainPathDirection(world_, *unit, desired))
            {
                directions.insert(directions.begin(), *pathDirection);
            }
        }
        for (auto direction : directions)
        {
            auto next = unit->position + direction * unit->speed;
            auto probe = probeMoveInWorld(
                world_,
                *unit,
                next,
                battleMovementTaXueUnstable(*unit),
                reservations,
                battleMovementTaXueUnstable(*unit),
                false);
            lastProbe = probe;
            if (!probe.canMove)
            {
                continue;
            }
            unit->position = next;
            unit->velocity = direction * unit->speed;
            reserveSameFrameDestination(*unit, next, reservations);
            reserveMovementDestination(world_, *unit, next);
            decision.action = MovementAction::Move;
            decision.velocity = unit->velocity;
            decision.destination = unit->position;
            recordEvent(result.events, BattleEventType::Movement, *unit, decision);
            moved = true;
            break;
        }
        if (!moved && !battleMovementTaXueUnstable(*unit))
        {
            for (auto direction : directions)
            {
                auto next = unit->position + direction * unit->speed;
                auto probe = probeMoveInWorld(world_, *unit, next, false, reservations, false, true);
                lastProbe = probe;
                if (!probe.canMove)
                {
                    continue;
                }
                unit->position = next;
                unit->velocity = direction * unit->speed;
                reserveSameFrameDestination(*unit, next, reservations);
                reserveMovementDestination(world_, *unit, next);
                decision.action = MovementAction::Move;
                decision.velocity = unit->velocity;
                decision.destination = unit->position;
                recordEvent(result.events, BattleEventType::Movement, *unit, decision);
                moved = true;
                break;
            }
        }

        if (!moved)
        {
            decision.blockReason = lastProbe.reason;
            decision.blockerId = lastProbe.blockerId;
            if (lastProbe.reason == MoveBlockReason::Ally || lastProbe.reason == MoveBlockReason::Reservation)
            {
                if (unit->slotSwitchCooldownRemaining <= 0)
                {
                    int oldSlot = unit->assignedSlot;
                    unit->assignedSlot += deterministicSide(unit->id, unit->assignedSlot);
                    unit->assignedSlot = std::clamp(unit->assignedSlot, -4, 4);
                    unit->slotSwitchCooldownRemaining = world_.config.slotSwitchCooldownFrames;
                    decision.slot = unit->assignedSlot;
                    if (unit->assignedSlot != oldSlot)
                    {
                        recordEvent(result.events, BattleEventType::SlotChanged, *unit, decision);
                    }
                }

                auto dashDirection = desired - unit->position;
                auto dash = chooseDashWithSoftFallback(
                    world_,
                    *unit,
                    *target,
                    dashDirection,
                    reservations,
                    unit->taXue || battleMovementTaXueUnstable(*unit));
                if (dash)
                {
                    decision = *dash;
                    unit->velocity = decision.velocity;
                    unit->dashFramesRemaining = world_.config.dashFrames;
                    unit->dashCooldownRemaining = world_.config.dashCooldownFrames;
                    reserveSameFrameDestination(*unit, decision.destination, reservations);
                    reserveMovementDestination(world_, *unit, decision.destination);
                    recordEvent(result.events, BattleEventType::DashStart, *unit, decision);
                    moved = true;
                }
                else
                {
                    recordEvent(result.events, BattleEventType::BlockedByAlly, *unit, decision);
                }
            }
            else if (lastProbe.reason == MoveBlockReason::Wall)
            {
                recordEvent(result.events, BattleEventType::BlockedByWall, *unit, decision);
            }
        }

        if (!moved && decision.action == MovementAction::Hold)
        {
            unit->velocity = { 0, 0, 0 };
            clearMovementReservation(world_, unit->id);
        }
        result.decisions[unit->id] = decision;
    }

    world_.frame++;
    result.frame = world_.frame;
    result.units = world_.units;
    return result;
}

BattleMovementSimulator::BattleMovementSimulator(BattleMovementFrameInput world)
    : world_(std::move(world))
{
}

MovementRunResult BattleMovementSimulator::run(int frames, unsigned int seed)
{
    world_.seed = seed;
    MovementRunResult run;
    run.world = world_;
    std::map<int, Pointf> lastPositions;
    std::map<int, Pointf> lastDirections;
    std::map<int, int> lastTargets;
    std::map<int, int> lastSlots;
    for (const auto& unit : world_.units)
    {
        lastPositions[unit.id] = unit.position;
        lastDirections[unit.id] = { 0, 0, 0 };
        lastTargets[unit.id] = unit.targetId;
        lastSlots[unit.id] = unit.assignedSlot;
    }

    for (int i = 0; i < frames; ++i)
    {
        auto tickResult = BattleMovementPlanner(world_).tick();
        run.events.insert(run.events.end(), tickResult.events.begin(), tickResult.events.end());
        for (const auto& event : tickResult.events)
        {
            if (event.type == BattleEventType::DashStart)
            {
                auto& stats = run.stats[event.unitId];
                stats.dashCount++;
                stats.lastDashDistance = event.value;
            }
        }
        for (const auto& unit : world_.units)
        {
            auto& stats = run.stats[unit.id];
            auto dit = tickResult.decisions.find(unit.id);
            MovementDecision decision = dit != tickResult.decisions.end() ? dit->second : MovementDecision{ unit.id };
            if (decision.blockReason != MoveBlockReason::None
                || decision.action == MovementAction::Hold)
            {
                if (decision.blockReason != MoveBlockReason::None)
                {
                    stats.consecutiveBlockedFrames++;
                    stats.lastBlockReason = decision.blockReason;
                }
                if (decision.blockReason == MoveBlockReason::Ally
                    || decision.blockReason == MoveBlockReason::Reservation)
                {
                    stats.consecutiveAllyBlockedFrames++;
                    stats.totalAllyBlockedFrames++;
                }
                else
                {
                    stats.consecutiveAllyBlockedFrames = 0;
                }
                if (decision.blockReason == MoveBlockReason::Wall)
                {
                    stats.consecutiveWallBlockedFrames++;
                }
                else
                {
                    stats.consecutiveWallBlockedFrames = 0;
                }
            }
            else
            {
                stats.consecutiveBlockedFrames = 0;
                stats.consecutiveAllyBlockedFrames = 0;
                stats.consecutiveWallBlockedFrames = 0;
                stats.lastBlockReason = MoveBlockReason::None;
            }

            bool madeTacticalProgress = decision.action == MovementAction::Move
                || decision.action == MovementAction::Dash
                || decision.action == MovementAction::AttackReady
                || distance2d(unit.position, lastPositions[unit.id]) >= world_.config.engagementDeadband / 4.0;
            if (!madeTacticalProgress)
            {
                stats.consecutiveNoProgressFrames++;
            }
            else
            {
                stats.consecutiveNoProgressFrames = 0;
            }
            if (lastTargets[unit.id] != unit.targetId)
            {
                stats.targetSwitches++;
                lastTargets[unit.id] = unit.targetId;
            }
            if (lastSlots[unit.id] != unit.assignedSlot)
            {
                stats.slotSwitches++;
                lastSlots[unit.id] = unit.assignedSlot;
            }
            if (decision.action == MovementAction::AttackReady)
            {
                stats.attackReadyFrames++;
            }
            if (distance2d(unit.velocity, { 0, 0, 0 }) > 0.01 && distance2d(lastDirections[unit.id], { 0, 0, 0 }) > 0.01)
            {
                auto currentDir = unitVector(unit.velocity);
                auto lastDir = unitVector(lastDirections[unit.id]);
                if (dot2d(currentDir, lastDir) < -0.5)
                {
                    stats.directionReversalCount++;
                }
            }
            lastDirections[unit.id] = unit.velocity;
            lastPositions[unit.id] = unit.position;
        }
    }
    run.world = world_;
    return run;
}

}  // namespace KysChess::Battle
