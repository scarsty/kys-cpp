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
constexpr double AirborneTerrainClearanceTileFactor = 3.0;
constexpr int CooperativeYieldRequestFrames = 4;

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

Pointf blendUnitDirections(Pointf primary, Pointf secondary, double secondaryWeight)
{
    primary = unitVector(primary);
    secondary = unitVector(secondary);
    if (primary.norm() <= 0.01f)
    {
        return secondary;
    }
    if (secondary.norm() <= 0.01f || dot2d(primary, secondary) <= 0.0)
    {
        return primary;
    }
    return unitVector(primary + secondary * secondaryWeight);
}

int deterministicSide(int unitId, int slot)
{
    return ((unitId * 37 + slot * 17) & 1) == 0 ? 1 : -1;
}

double combatSlotAngle(int slot)
{
    if (slot == 0)
    {
        return 0.0;
    }
    return kPi / 4.0 * std::abs(slot) * (slot > 0 ? 1.0 : -1.0);
}

const BattleUnitState* nearestEnemy(const BattleMovementPlanInput& world, const BattleUnitState& unit)
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

const BattleUnitState* assignedEnemy(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    const auto* assigned = tryFindById(world.units, unit.targetId);
    if (assigned && assigned->alive && assigned->team != unit.team)
    {
        return assigned;
    }
    return nearestEnemy(world, unit);
}

double comfortableMeleeSpacing(const BattleMovementConfig& config)
{
    return config.bodyRadius + config.engagementDeadband;
}

void assignMovementTargets(BattleMovementPlanInput& world)
{
    struct MeleeTargetingEntry
    {
        int unitId{};
        double nearestDistance{};
    };

    std::map<int, int> meleeClaimsByTarget;
    std::vector<MeleeTargetingEntry> unassignedMelee;
    unassignedMelee.reserve(world.units.size());

    for (auto& unit : world.units)
    {
        if (!unit.alive)
        {
            continue;
        }

        const auto* nearest = nearestEnemy(world, unit);
        if (!nearest)
        {
            unit.targetId = -1;
            continue;
        }
        if (unit.style == CombatStyle::Ranged)
        {
            unit.targetId = nearest->id;
            continue;
        }

        if (unit.speed <= 0.0)
        {
            unit.targetId = nearest->id;
            if (distance2d(unit.position, nearest->position) <= world.config.meleeAttackReach)
            {
                ++meleeClaimsByTarget[nearest->id];
            }
            continue;
        }

        const auto* current = tryFindById(world.units, unit.targetId);
        if (current && (!current->alive || current->team == unit.team))
        {
            current = nullptr;
        }
        if (current)
        {
            ++meleeClaimsByTarget[current->id];
            continue;
        }

        unassignedMelee.push_back({ unit.id, distance2d(unit.position, nearest->position) });
    }

    std::ranges::sort(unassignedMelee, [](const MeleeTargetingEntry& lhs, const MeleeTargetingEntry& rhs)
        {
            if (lhs.nearestDistance != rhs.nearestDistance)
            {
                return lhs.nearestDistance < rhs.nearestDistance;
            }
            return lhs.unitId < rhs.unitId;
        });

    const double claimSpacing = comfortableMeleeSpacing(world.config);
    for (const auto& entry : unassignedMelee)
    {
        auto& unit = requireById(world.units, entry.unitId);
        const BattleUnitState* best = nullptr;
        double bestScore = std::numeric_limits<double>::max();
        double bestDistance = std::numeric_limits<double>::max();
        for (const auto& candidate : world.units)
        {
            if (!candidate.alive || candidate.team == unit.team)
            {
                continue;
            }

            const double distance = distance2d(unit.position, candidate.position);
            const double score = distance + meleeClaimsByTarget[candidate.id] * claimSpacing;
            if (!best
                || score < bestScore
                || (score == bestScore && distance < bestDistance)
                || (score == bestScore && distance == bestDistance && candidate.id < best->id))
            {
                best = &candidate;
                bestScore = score;
                bestDistance = distance;
            }
        }
        assert(best);
        unit.targetId = best->id;
        ++meleeClaimsByTarget[best->id];
    }
}

std::vector<int> movementOrder(const BattleMovementPlanInput& world)
{
    struct MovementOrderEntry
    {
        int id = -1;
        bool attackReady = false;
        double distance = std::numeric_limits<double>::max();
    };

    std::vector<MovementOrderEntry> entries;
    entries.reserve(world.units.size());
    for (const auto& unit : world.units)
    {
        if (unit.alive)
        {
            const auto* target = assignedEnemy(world, unit);
            const double distance = target
                ? distance2d(unit.position, target->position)
                : std::numeric_limits<double>::max();
            entries.push_back({
                unit.id,
                target && unit.canAttack && distance <= unit.reach,
                distance,
            });
        }
    }

    std::sort(entries.begin(), entries.end(), [](const MovementOrderEntry& lhs, const MovementOrderEntry& rhs)
        {
            if (lhs.attackReady != rhs.attackReady)
            {
                return lhs.attackReady;
            }
            if (lhs.distance != rhs.distance)
            {
                return lhs.distance < rhs.distance;
            }
            return lhs.id < rhs.id;
        });

    std::vector<int> ids;
    ids.reserve(entries.size());
    for (const auto& entry : entries)
    {
        ids.push_back(entry.id);
    }
    return ids;
}

std::vector<Pointf> candidateDirections(const BattleMovementPlanInput& world,
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

int terrainGridCoordCount(const BattleMovementPlanInput& world)
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

enum class TerrainLookupMode
{
    Empty,
    Isometric,
    Cartesian,
    Scan,
};

struct BattleMovementTerrainLookup
{
    explicit BattleMovementTerrainLookup(const BattleMovementPlanInput& world)
        : world(world),
          coordCount(terrainGridCoordCount(world)),
          tileWidth(world.config.tileWidth)
    {
        if (world.terrainCells.empty())
        {
            mode = TerrainLookupMode::Empty;
            return;
        }
        if (coordCount <= 0 || tileWidth <= 0.0)
        {
            mode = TerrainLookupMode::Scan;
            return;
        }

        cartesianOrigin = world.terrainCells.front().position;
        if (matchesIsometricGrid())
        {
            mode = TerrainLookupMode::Isometric;
        }
        else if (matchesCartesianGrid())
        {
            mode = TerrainLookupMode::Cartesian;
        }
        else
        {
            mode = TerrainLookupMode::Scan;
        }
    }

    bool empty() const
    {
        return mode == TerrainLookupMode::Empty;
    }

    bool allows(Pointf position) const
    {
        if (empty())
        {
            return true;
        }

        if (const auto coords = cellCoordsFor(position))
        {
            const auto& cell = world.terrainCells[terrainGridIndex(coordCount, coords->x, coords->y)];
            const double maximumDistance = std::max(1.0, tileWidth * 1.5);
            if (distance2d(position, cell.position) <= maximumDistance)
            {
                return cell.walkable;
            }
        }

        return scanAllows(position);
    }

    bool segmentClear(Pointf from, Pointf to) const
    {
        if (empty())
        {
            return true;
        }

        auto delta = to - from;
        double length = delta.norm();
        if (length <= 0.01)
        {
            return allows(to);
        }

        int steps = std::max(1, static_cast<int>(std::ceil(length / std::max(1.0, world.config.engagementDeadband))));
        for (int i = 1; i <= steps; ++i)
        {
            double t = static_cast<double>(i) / steps;
            auto probe = from + delta * t;
            if (!allows(probe))
            {
                return false;
            }
        }
        return true;
    }

    bool blockedCellNear(Pointf position, double radius) const
    {
        if (empty())
        {
            return false;
        }

        if (const auto coords = cellCoordsFor(position))
        {
            const int cellRadius = std::max(1, static_cast<int>(std::ceil(radius / std::max(1.0, tileWidth))) + 2);
            const int minX = std::max(0, coords->x - cellRadius);
            const int maxX = std::min(coordCount - 1, coords->x + cellRadius);
            const int minY = std::max(0, coords->y - cellRadius);
            const int maxY = std::min(coordCount - 1, coords->y + cellRadius);
            for (int x = minX; x <= maxX; ++x)
            {
                for (int y = minY; y <= maxY; ++y)
                {
                    const auto& cell = world.terrainCells[terrainGridIndex(coordCount, x, y)];
                    if (!cell.walkable && distance2d(position, cell.position) < radius)
                    {
                        return true;
                    }
                }
            }
            return false;
        }

        for (const auto& cell : world.terrainCells)
        {
            if (!cell.walkable && distance2d(position, cell.position) < radius)
            {
                return true;
            }
        }
        return false;
    }

    const BattleMovementPlanInput& world;
    int coordCount{};
    double tileWidth{};
    TerrainLookupMode mode = TerrainLookupMode::Scan;
    Pointf cartesianOrigin;

private:
    bool closeTo(Pointf lhs, Pointf rhs) const
    {
        return distance2d(lhs, rhs) <= 0.01;
    }

    Pointf isometricPosition(int x, int y) const
    {
        return {
            static_cast<float>((-y + x + coordCount) * tileWidth),
            static_cast<float>((y + x) * tileWidth),
            0.0f,
        };
    }

    Pointf cartesianPosition(int x, int y) const
    {
        return {
            static_cast<float>(cartesianOrigin.x + x * tileWidth),
            static_cast<float>(cartesianOrigin.y + y * tileWidth),
            0.0f,
        };
    }

    bool matchesIsometricGrid() const
    {
        for (int x = 0; x < coordCount; ++x)
        {
            for (int y = 0; y < coordCount; ++y)
            {
                const auto& cell = world.terrainCells[terrainGridIndex(coordCount, x, y)];
                if (!closeTo(cell.position, isometricPosition(x, y)))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool matchesCartesianGrid() const
    {
        for (int x = 0; x < coordCount; ++x)
        {
            for (int y = 0; y < coordCount; ++y)
            {
                const auto& cell = world.terrainCells[terrainGridIndex(coordCount, x, y)];
                if (!closeTo(cell.position, cartesianPosition(x, y)))
                {
                    return false;
                }
            }
        }
        return true;
    }

    std::optional<Point> cellCoordsFor(Pointf position) const
    {
        Point coords;
        switch (mode)
        {
        case TerrainLookupMode::Isometric:
        {
            const double shiftedX = position.x - coordCount * tileWidth;
            coords.x = static_cast<int>(std::round((shiftedX / tileWidth + position.y / tileWidth) / 2.0));
            coords.y = static_cast<int>(std::round((-shiftedX / tileWidth + position.y / tileWidth) / 2.0));
            break;
        }
        case TerrainLookupMode::Cartesian:
            coords.x = static_cast<int>(std::round((position.x - cartesianOrigin.x) / tileWidth));
            coords.y = static_cast<int>(std::round((position.y - cartesianOrigin.y) / tileWidth));
            break;
        default:
            return std::nullopt;
        }

        if (coords.x < 0 || coords.y < 0 || coords.x >= coordCount || coords.y >= coordCount)
        {
            return std::nullopt;
        }
        return coords;
    }

    bool scanAllows(Pointf position) const
    {
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
};

std::optional<int> nearestWalkableTerrainCell(const BattleMovementPlanInput& world, Pointf position)
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

std::optional<Pointf> nextTerrainPathWaypoint(const BattleMovementPlanInput& world,
                                              const BattleMovementTerrainLookup& terrain,
                                              Pointf from,
                                              Pointf to)
{
    const int coordCount = terrain.coordCount;
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

    std::vector<int> path;
    for (int cell = *goal; cell >= 0; cell = previous[static_cast<std::size_t>(cell)])
    {
        path.push_back(cell);
        if (cell == *start)
        {
            break;
        }
    }
    std::ranges::reverse(path);

    if (path.size() >= 2)
    {
        const auto startCellPosition = world.terrainCells[static_cast<std::size_t>(path[0])].position;
        const auto nextCellPosition = world.terrainCells[static_cast<std::size_t>(path[1])].position;
        auto pathSegment = nextCellPosition - startCellPosition;
        pathSegment.z = 0;
        if (pathSegment.norm() > 0.01f)
        {
            return from + unitVector(pathSegment) * static_cast<float>(std::max(1.0, world.config.tileWidth));
        }
    }

    return world.terrainCells[static_cast<std::size_t>(path.back())].position;
}

std::optional<Pointf> terrainPathDirection(const BattleMovementPlanInput& world,
                                           const BattleMovementTerrainLookup& terrain,
                                           const BattleUnitState& unit,
                                           Pointf desired)
{
    auto waypoint = nextTerrainPathWaypoint(world, terrain, unit.position, desired);
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

Pointf combatSlotPosition(const BattleMovementPlanInput& world,
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
    return target.position + rotated(away, combatSlotAngle(slot)) * radius;
}

struct MeleeApproachSlotEntry
{
    int unitId = -1;
    double signedLateral = 0.0;
    double targetDistance = 0.0;
};

int slotForApproachIndex(int index, int count)
{
    assert(index >= 0);
    assert(index < count);
    if (count == 1)
    {
        return 0;
    }

    int slot = count / 2 - index;
    if (count % 2 == 0 && slot <= 0)
    {
        --slot;
    }
    return std::clamp(slot, -4, 4);
}

int defaultMeleeApproachSlot(const BattleMovementPlanInput& world,
                             const BattleUnitState& unit,
                             const BattleUnitState& target)
{
    Pointf approachCentroid;
    std::vector<const BattleUnitState*> contenders;
    for (const auto& other : world.units)
    {
        if (!other.alive
            || other.team != unit.team
            || other.style != CombatStyle::Melee
            || other.speed <= 0.0
            || battleMovementTaXueUnstable(other))
        {
            continue;
        }

        const auto* otherTarget = assignedEnemy(world, other);
        if (!otherTarget || otherTarget->id != target.id)
        {
            continue;
        }

        approachCentroid += other.position;
        contenders.push_back(&other);
    }

    const int approachCount = static_cast<int>(contenders.size());
    assert(approachCount > 0);
    approachCentroid.x /= static_cast<float>(approachCount);
    approachCentroid.y /= static_cast<float>(approachCount);
    approachCentroid.z /= static_cast<float>(approachCount);

    auto approach = target.position - approachCentroid;
    approach.z = 0;
    if (approach.norm() <= 0.01f)
    {
        approach = target.position - unit.position;
        approach.z = 0;
    }
    if (approach.norm() <= 0.01f)
    {
        approach = { 1.0f, 0.0f, 0.0f };
    }
    approach = unitVector(approach);
    const Pointf lateral{ -approach.y, approach.x, 0.0f };

    std::vector<MeleeApproachSlotEntry> entries;
    entries.reserve(static_cast<std::size_t>(approachCount));
    for (const auto* other : contenders)
    {
        entries.push_back({
            other->id,
            dot2d(other->position - target.position, lateral),
            distance2d(other->position, target.position),
        });
    }

    std::ranges::sort(entries, [](const MeleeApproachSlotEntry& lhs, const MeleeApproachSlotEntry& rhs)
        {
            if (lhs.signedLateral != rhs.signedLateral)
            {
                return lhs.signedLateral < rhs.signedLateral;
            }
            if (lhs.targetDistance != rhs.targetDistance)
            {
                return lhs.targetDistance < rhs.targetDistance;
            }
            return lhs.unitId < rhs.unitId;
        });

    const auto entry = std::ranges::find(entries, unit.id, &MeleeApproachSlotEntry::unitId);
    assert(entry != entries.end());
    return slotForApproachIndex(static_cast<int>(std::distance(entries.begin(), entry)), static_cast<int>(entries.size()));
}

int meleeApproachSlot(const BattleMovementPlanInput& world,
                      const BattleUnitState& unit,
                      const BattleUnitState& target)
{
    if (unit.speed <= 0.0)
    {
        return unit.assignedSlot;
    }
    if (battleMovementTaXueUnstable(unit))
    {
        return unit.assignedSlot;
    }
    if (unit.assignedSlot != 0)
    {
        return unit.assignedSlot;
    }
    return defaultMeleeApproachSlot(world, unit, target);
}

bool reservationConflicts(const BattleMovementPlanInput& world,
                          const BattleUnitState& unit,
                          Pointf nextPosition,
                          const std::map<int, Pointf>& reservations)
{
    for (const auto& [reservedBy, pos] : reservations)
    {
        if (reservedBy == unit.id)
        {
            continue;
        }
        if (distance2d(nextPosition, pos) < world.config.bodyRadius)
        {
            const double currentDistance = distance2d(unit.position, pos);
            const double nextDistance = distance2d(nextPosition, pos);
            if (currentDistance < nextDistance)
            {
                continue;
            }
            return true;
        }
    }
    return false;
}

bool softReservationConflicts(const BattleMovementPlanInput& world,
                              const BattleUnitState& unit,
                              Pointf nextPosition)
{
    for (const auto& [reservedBy, reservation] : world.movementReservations)
    {
        if (reservedBy == unit.id || reservation.unitId == unit.id)
        {
            continue;
        }
        const double radius = reservation.radius > 0.0
            ? reservation.radius
            : world.config.bodyRadius;
        if (distance2d(nextPosition, reservation.position) < radius)
        {
            const double currentDistance = distance2d(unit.position, reservation.position);
            const double nextDistance = distance2d(nextPosition, reservation.position);
            if (currentDistance < nextDistance)
            {
                continue;
            }
            return true;
        }
    }
    return false;
}

bool bodyConflicts(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    for (const auto& other : world.units)
    {
        if (!other.alive || other.id == unit.id)
        {
            continue;
        }
        if (distance2d(unit.position, other.position) < world.config.bodyRadius)
        {
            return true;
        }
    }
    return false;
}

std::optional<Pointf> bodySeparationDirection(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    Pointf direction;
    for (const auto& other : world.units)
    {
        if (!other.alive || other.id == unit.id)
        {
            continue;
        }
        const double distance = distance2d(unit.position, other.position);
        if (distance >= world.config.bodyRadius)
        {
            continue;
        }
        auto away = unit.position - other.position;
        if (away.norm() <= 0.01f)
        {
            const double angle = (unit.id * 37 + other.id * 53) * kPi / 180.0;
            away = Pointf{ static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0f };
        }
        away = unitVector(away);
        direction += away * static_cast<float>(world.config.bodyRadius - distance);
    }
    if (direction.norm() <= 0.01f)
    {
        return std::nullopt;
    }
    return unitVector(direction);
}

bool sharesFrontlineTarget(const BattleMovementPlanInput& world,
                           const BattleUnitState& unit,
                           const BattleUnitState& other,
                           const BattleUnitState& target)
{
    if (!other.alive
        || other.id == unit.id
        || other.team != unit.team
        || other.style != CombatStyle::Melee
        || battleMovementTaXueUnstable(other))
    {
        return false;
    }

    const auto* otherTarget = assignedEnemy(world, other);
    if (!otherTarget || otherTarget->id != target.id)
    {
        return false;
    }

    const double frontlineBand = world.config.meleeAttackReach + world.config.engagementDeadband;
    return distance2d(other.position, target.position) <= frontlineBand;
}

double nearestFrontlineAllyDistance(const BattleMovementPlanInput& world,
                                    const BattleUnitState& unit,
                                    const BattleUnitState& target,
                                    Pointf position)
{
    double nearest = std::numeric_limits<double>::max();
    for (const auto& other : world.units)
    {
        if (!sharesFrontlineTarget(world, unit, other, target))
        {
            continue;
        }
        nearest = std::min(nearest, distance2d(position, other.position));
    }
    return nearest;
}

std::optional<Pointf> frontlineSoftSpreadDirection(const BattleMovementPlanInput& world,
                                                   const BattleUnitState& unit,
                                                   const BattleUnitState& target)
{
    if (unit.style != CombatStyle::Melee || battleMovementTaXueUnstable(unit))
    {
        return std::nullopt;
    }

    const double frontlineBand = world.config.meleeAttackReach + world.config.engagementDeadband;
    if (distance2d(unit.position, target.position) > frontlineBand)
    {
        return std::nullopt;
    }

    auto awayFromTarget = unit.position - target.position;
    awayFromTarget.z = 0;
    if (awayFromTarget.norm() <= 0.01f)
    {
        const double angle = (unit.id * 37 + target.id * 53) * kPi / 180.0;
        awayFromTarget = Pointf{ static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0f };
    }
    awayFromTarget = unitVector(awayFromTarget);

    Pointf direction;
    const double comfortableSpacing = comfortableMeleeSpacing(world.config);
    for (const auto& other : world.units)
    {
        if (!sharesFrontlineTarget(world, unit, other, target))
        {
            continue;
        }

        const double distance = distance2d(unit.position, other.position);
        if (distance >= comfortableSpacing)
        {
            continue;
        }

        auto awayFromOther = unit.position - other.position;
        awayFromOther.z = 0;
        if (awayFromOther.norm() <= 0.01f)
        {
            const double angle = (unit.id * 37 + other.id * 53) * kPi / 180.0;
            awayFromOther = Pointf{ static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0f };
        }
        awayFromOther = unitVector(awayFromOther);

        Pointf side{ -awayFromTarget.y, awayFromTarget.x, 0 };
        if (dot2d(side, awayFromOther) < 0.0)
        {
            side = -side;
        }
        direction += unitVector(side) * static_cast<float>(comfortableSpacing - distance);
    }

    if (direction.norm() <= 0.01f)
    {
        return std::nullopt;
    }
    return unitVector(direction);
}

MoveProbe probeMoveInWorld(const BattleMovementPlanInput& world,
                           const BattleMovementTerrainLookup& terrain,
                           const BattleUnitState& unit,
                           Pointf nextPosition,
                           bool ignoreUnits,
                           const std::map<int, Pointf>& reservations,
                           bool ignoreReservations,
                           bool allowSoftReservations);

std::optional<MovementDecision> tryFrontlineSoftSpread(const BattleMovementPlanInput& world,
                                                       const BattleMovementTerrainLookup& terrain,
                                                       const BattleUnitState& unit,
                                                       const BattleUnitState& target,
                                                       const std::map<int, Pointf>& reservations)
{
    const auto direction = frontlineSoftSpreadDirection(world, unit, target);
    if (!direction)
    {
        return std::nullopt;
    }

    const auto next = unit.position + *direction * unit.speed;
    const double frontlineBand = world.config.meleeAttackReach + world.config.engagementDeadband;
    if (distance2d(next, target.position) > frontlineBand)
    {
        return std::nullopt;
    }

    const double currentClearance = nearestFrontlineAllyDistance(world, unit, target, unit.position);
    const double nextClearance = nearestFrontlineAllyDistance(world, unit, target, next);
    if (nextClearance <= currentClearance + 0.1)
    {
        return std::nullopt;
    }

    auto probe = probeMoveInWorld(world, terrain, unit, next, false, reservations, false, false);
    if (!probe.canMove)
    {
        return std::nullopt;
    }

    MovementDecision decision;
    decision.unitId = unit.id;
    decision.action = MovementAction::Move;
    decision.targetId = target.id;
    decision.velocity = *direction * unit.speed;
    decision.destination = next;
    decision.slot = unit.assignedSlot;
    return decision;
}

std::vector<Pointf> blockerDetourDirections(const BattleUnitState& unit,
                                            const BattleUnitState& blocker,
                                            Pointf desired)
{
    auto toBlocker = blocker.position - unit.position;
    toBlocker.z = 0;
    if (toBlocker.norm() <= 0.01f)
    {
        const double angle = (unit.id * 37 + blocker.id * 53) * kPi / 180.0;
        toBlocker = Pointf{ static_cast<float>(std::cos(angle)), static_cast<float>(std::sin(angle)), 0.0f };
    }
    toBlocker = unitVector(toBlocker);

    Pointf side{ -toBlocker.y, toBlocker.x, 0 };
    auto desiredDirection = desired - unit.position;
    desiredDirection.z = 0;
    if (desiredDirection.norm() > 0.01f)
    {
        desiredDirection = unitVector(desiredDirection);
        if (dot2d(side, desiredDirection) < dot2d(-side, desiredDirection))
        {
            side = -side;
        }
    }
    else if (deterministicSide(unit.id, unit.assignedSlot) < 0)
    {
        side = -side;
    }

    const auto away = -toBlocker;
    return {
        side,
        unitVector(side + away * 0.5f),
        -side,
        unitVector(-side + away * 0.5f),
    };
}

std::vector<Pointf> cooperativeYieldDirections(const BattleUnitState& unit,
                                               const BattleMovementYieldRequest& request)
{
    auto awayFromRequester = unit.position - request.requesterPosition;
    awayFromRequester.z = 0;
    if (awayFromRequester.norm() <= 0.01f)
    {
        return {};
    }
    awayFromRequester = unitVector(awayFromRequester);

    Pointf side{ -awayFromRequester.y, awayFromRequester.x, 0 };
    if (deterministicSide(unit.id, request.requesterUnitId) < 0)
    {
        side = -side;
    }

    return {
        side,
        -side,
        unitVector(side + awayFromRequester * 0.35f),
        unitVector(-side + awayFromRequester * 0.35f),
    };
}

double horizontalSpeed(Pointf velocity)
{
    return EuclidDis(velocity.x, velocity.y);
}

void pruneMovementReservations(BattleMovementPlanInput& world)
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

void pruneMovementYieldRequests(BattleMovementPlanInput& world)
{
    for (auto it = world.yieldRequests.begin(); it != world.yieldRequests.end();)
    {
        const auto* unit = tryFindById(world.units, it->first);
        const auto* requester = tryFindById(world.units, it->second.requesterUnitId);
        if (!unit || !unit->alive || !requester || !requester->alive || it->second.expiresFrame <= world.frame)
        {
            it = world.yieldRequests.erase(it);
            continue;
        }
        ++it;
    }
}

void pruneMovementDetourRequests(BattleMovementPlanInput& world)
{
    for (auto it = world.detourRequests.begin(); it != world.detourRequests.end();)
    {
        const auto* unit = tryFindById(world.units, it->first);
        const auto* blocker = tryFindById(world.units, it->second.blockerUnitId);
        if (!unit || !unit->alive || !blocker || !blocker->alive || it->second.expiresFrame <= world.frame)
        {
            it = world.detourRequests.erase(it);
            continue;
        }
        ++it;
    }
}

void clearMovementReservation(BattleMovementPlanInput& world, int unitId)
{
    world.movementReservations.erase(unitId);
}

void reserveMovementDestination(BattleMovementPlanInput& world,
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

void requestMovementYield(std::map<int, BattleMovementYieldRequest>& requests,
                          const BattleMovementPlanInput& world,
                          const BattleUnitState& requester,
                          int blockerId,
                          Pointf desired)
{
    const auto* blocker = tryFindById(world.units, blockerId);
    if (!blocker
        || !blocker->alive
        || blocker->team != requester.team
        || requester.style != CombatStyle::Melee
        || blocker->style != CombatStyle::Melee
        || battleMovementTaXueUnstable(*blocker))
    {
        return;
    }

    auto requesterToBlocker = blocker->position - requester.position;
    requesterToBlocker.z = 0;
    auto requesterToDesired = desired - requester.position;
    requesterToDesired.z = 0;
    if (requesterToBlocker.norm() <= 0.01f || requesterToDesired.norm() <= 0.01f)
    {
        return;
    }
    requesterToBlocker = unitVector(requesterToBlocker);
    requesterToDesired = unitVector(requesterToDesired);
    if (dot2d(requesterToBlocker, requesterToDesired) <= 0.0)
    {
        return;
    }

    requests[blocker->id] = BattleMovementYieldRequest{
        blocker->id,
        requester.id,
        requester.position,
        desired,
        world.frame + CooperativeYieldRequestFrames,
    };
}

void requestMovementDetour(std::map<int, BattleMovementDetourRequest>& requests,
                           const BattleMovementPlanInput& world,
                           const BattleMovementTerrainLookup& terrain,
                           const BattleUnitState& requester,
                           int blockerId,
                           Pointf desired)
{
    if (requests.contains(requester.id))
    {
        return;
    }

    const auto* blocker = tryFindById(world.units, blockerId);
    if (!blocker
        || !blocker->alive
        || blocker->team != requester.team
        || requester.style != CombatStyle::Melee
        || battleMovementTaXueUnstable(requester))
    {
        return;
    }

    if (!terrain.segmentClear(requester.position, desired))
    {
        return;
    }
    if (terrain.blockedCellNear(requester.position, world.config.bodyRadius * 3.0))
    {
        return;
    }

    const auto directions = blockerDetourDirections(requester, *blocker, desired);
    if (directions.empty())
    {
        return;
    }

    Pointf bestDirection = directions.front();
    double bestClearance = -1.0;
    for (auto direction : directions)
    {
        direction = unitVector(direction);
        auto next = requester.position + direction * requester.speed;
        if (!terrain.allows(next))
        {
            continue;
        }
        double clearance = std::numeric_limits<double>::max();
        for (const auto& other : world.units)
        {
            if (!other.alive || other.id == requester.id || other.team != requester.team)
            {
                continue;
            }
            clearance = std::min(clearance, distance2d(next, other.position));
        }
        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            bestDirection = direction;
        }
    }
    if (bestClearance < 0.0)
    {
        return;
    }
    const auto active = world.detourRequests.find(requester.id);
    if (active != world.detourRequests.end()
        && active->second.direction.norm() > 0.01f)
    {
        bestDirection = blendUnitDirections(active->second.direction, bestDirection, 0.25);
    }

    requests[requester.id] = BattleMovementDetourRequest{
        requester.id,
        blocker->id,
        unitVector(bestDirection),
        world.frame + 8,
    };
}

void requestCooperativeMovement(std::map<int, BattleMovementYieldRequest>& yieldRequests,
                                std::map<int, BattleMovementDetourRequest>& detourRequests,
                                const BattleMovementPlanInput& world,
                                const BattleMovementTerrainLookup& terrain,
                                const BattleUnitState& requester,
                                const MoveProbe& probe,
                                Pointf desired)
{
    if (probe.reason != MoveBlockReason::Ally)
    {
        return;
    }

    requestMovementYield(yieldRequests, world, requester, probe.blockerId, desired);
    requestMovementDetour(detourRequests, world, terrain, requester, probe.blockerId, desired);
}

const BattleUnitState* approachCorridorBlocker(const BattleMovementPlanInput& world,
                                               const BattleUnitState& requester,
                                               Pointf desired)
{
    if (requester.style != CombatStyle::Melee)
    {
        return nullptr;
    }

    auto approach = desired - requester.position;
    approach.z = 0;
    if (approach.norm() <= 0.01f)
    {
        return nullptr;
    }
    approach = unitVector(approach);

    const BattleUnitState* blocker = nullptr;
    double bestProjection = std::numeric_limits<double>::max();
    const double maximumProjection = std::max(world.config.bodyRadius * 2.0, world.config.bodyRadius + requester.speed * 6.0);
    const double maximumLateral = world.config.bodyRadius * 0.65;
    for (const auto& other : world.units)
    {
        if (!other.alive
            || other.id == requester.id
            || other.team != requester.team
            || other.style != CombatStyle::Melee
            || battleMovementTaXueUnstable(other))
        {
            continue;
        }

        auto toOther = other.position - requester.position;
        toOther.z = 0;
        const double projection = dot2d(toOther, approach);
        if (projection <= 0.0 || projection > maximumProjection)
        {
            continue;
        }

        auto lateral = toOther - approach * projection;
        lateral.z = 0;
        if (lateral.norm() > maximumLateral)
        {
            continue;
        }

        if (projection < bestProjection)
        {
            bestProjection = projection;
            blocker = &other;
        }
    }
    return blocker;
}

void requestApproachCorridorYield(std::map<int, BattleMovementYieldRequest>& yieldRequests,
                                  const BattleMovementPlanInput& world,
                                  const BattleUnitState& requester,
                                  Pointf desired)
{
    const auto* blocker = approachCorridorBlocker(world, requester, desired);
    if (!blocker)
    {
        return;
    }

    requestMovementYield(yieldRequests, world, requester, blocker->id, desired);
}

void recordMovementDecision(BattleTickResult& result, const BattleUnitState& unit, MovementDecision decision)
{
    decision.unitId = unit.id;
    if (decision.action == MovementAction::Hold)
    {
        decision.velocity = unit.velocity;
        decision.destination = unit.position;
    }
    decision.targetId = unit.targetId;
    decision.slot = unit.assignedSlot;
    decision.slotSwitchCooldownRemaining = unit.slotSwitchCooldownRemaining;
    decision.dashFramesRemaining = unit.dashFramesRemaining;
    decision.dashCooldownRemaining = unit.dashCooldownRemaining;
    decision.movementDashSpreadFramesRemaining = unit.movementDashSpreadFramesRemaining;
    decision.postDashRetreatFramesRemaining = unit.postDashRetreatFramesRemaining;
    decision.postDashChaosFramesRemaining = unit.postDashChaosFramesRemaining;
    decision.knockbackFramesRemaining = unit.knockbackFramesRemaining;
    decision.knockbackControlFramesRemaining = unit.knockbackControlFramesRemaining;
    result.decisions[unit.id] = std::move(decision);
}

MoveProbe probeMoveInWorld(const BattleMovementPlanInput& world,
                           const BattleMovementTerrainLookup& terrain,
                           const BattleUnitState& unit,
                           Pointf nextPosition,
                           bool ignoreUnits,
                           const std::map<int, Pointf>& reservations,
                           bool ignoreReservations = false,
                           bool allowSoftReservations = true)
{
    if (!terrain.allows(nextPosition))
    {
        return { false, MoveBlockReason::Wall, -1 };
    }
    if (!ignoreReservations && reservationConflicts(world, unit, nextPosition, reservations))
    {
        return { false, MoveBlockReason::Reservation, -1 };
    }
    if (!ignoreReservations && !allowSoftReservations && softReservationConflicts(world, unit, nextPosition))
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
            const double currentDistance = distance2d(unit.position, other.position);
            const double nextDistance = distance2d(nextPosition, other.position);
            if (currentDistance < nextDistance)
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

std::optional<MovementDecision> tryCooperativeYield(const BattleMovementPlanInput& world,
                                                    const BattleMovementTerrainLookup& terrain,
                                                    const BattleUnitState& unit,
                                                    const BattleUnitState& target,
                                                    bool canCastFromHere,
                                                    const std::map<int, Pointf>& reservations)
{
    const auto request = world.yieldRequests.find(unit.id);
    if (request == world.yieldRequests.end()
        || unit.style != CombatStyle::Melee
        || canCastFromHere)
    {
        return std::nullopt;
    }

    for (auto direction : cooperativeYieldDirections(unit, request->second))
    {
        auto next = unit.position + direction * unit.speed;
        auto probe = probeMoveInWorld(world, terrain, unit, next, false, reservations, false, false);
        if (!probe.canMove)
        {
            continue;
        }

        MovementDecision decision;
        decision.unitId = unit.id;
        decision.action = MovementAction::Move;
        decision.targetId = target.id;
        decision.velocity = direction * unit.speed;
        decision.destination = next;
        decision.slot = unit.assignedSlot;
        return decision;
    }

    return std::nullopt;
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

std::optional<MovementDecision> chooseDash(const BattleMovementPlanInput& world,
                                           const BattleMovementTerrainLookup& terrain,
                                           const BattleUnitState& unit,
                                           const BattleUnitState& target,
                                           Pointf direction,
                                           const std::map<int, Pointf>& reservations,
                                           bool ignoreReservations,
                                           bool allowSoftReservations);

std::optional<MovementDecision> chooseDashByDistance(const BattleMovementPlanInput& world,
                                                     const BattleMovementTerrainLookup& terrain,
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
        if (!terrain.segmentClear(unit.position, landing))
        {
            continue;
        }
        auto probe = probeMoveInWorld(
            world,
            terrain,
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

double normalDashDistanceFor(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    return std::max(
        unit.speed * world.config.dashFrames,
        unit.speed * world.config.dashFrames * world.config.movementDashDistanceMultiplier);
}

double minimumDashDistanceFor(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    return unit.taXue
        ? std::max(world.config.tileWidth * 2.0, unit.speed * world.config.dashFrames)
        : unit.speed * world.config.dashFrames;
}

double allowedDashDistanceFor(const BattleMovementPlanInput& world, const BattleUnitState& unit)
{
    const double normalDashDistance = normalDashDistanceFor(world, unit);
    return unit.taXue
        ? world.config.maxDashDistance
        : std::min(world.config.maxDashDistance, normalDashDistance);
}

std::optional<MovementDecision> chooseDash(const BattleMovementPlanInput& world,
                                           const BattleMovementTerrainLookup& terrain,
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
        terrain,
        unit,
        target,
        direction,
        minDistance,
        maxDistance,
        reservations,
        ignoreReservations,
        allowSoftReservations);
}

std::optional<MovementDecision> chooseRetreatDash(const BattleMovementPlanInput& world,
                                                  const BattleMovementTerrainLookup& terrain,
                                                  const BattleUnitState& unit,
                                                  const BattleUnitState& target,
                                                  Pointf direction,
                                                  const std::map<int, Pointf>& reservations,
                                                  bool ignoreReservations,
                                                  bool allowSoftReservations)
{
    return chooseDashByDistance(
        world,
        terrain,
        unit,
        target,
        direction,
        minimumDashDistanceFor(world, unit),
        allowedDashDistanceFor(world, unit),
        reservations,
        ignoreReservations,
        allowSoftReservations);
}

std::optional<MovementDecision> chooseDashWithSoftFallback(const BattleMovementPlanInput& world,
                                                           const BattleMovementTerrainLookup& terrain,
                                                           const BattleUnitState& unit,
                                                           const BattleUnitState& target,
                                                           Pointf direction,
                                                           const std::map<int, Pointf>& reservations,
                                                           bool ignoreReservations)
{
    if (ignoreReservations)
    {
        return chooseDash(world, terrain, unit, target, direction, reservations, true, true);
    }
    if (auto dash = chooseDash(world, terrain, unit, target, direction, reservations, false, false))
    {
        return dash;
    }
    return chooseDash(world, terrain, unit, target, direction, reservations, false, true);
}

std::optional<MovementDecision> chooseRetreatDashWithSoftFallback(const BattleMovementPlanInput& world,
                                                                  const BattleMovementTerrainLookup& terrain,
                                                                  const BattleUnitState& unit,
                                                                  const BattleUnitState& target,
                                                                  Pointf direction,
                                                                  const std::map<int, Pointf>& reservations,
                                                                  bool ignoreReservations)
{
    if (ignoreReservations)
    {
        return chooseRetreatDash(world, terrain, unit, target, direction, reservations, true, true);
    }
    if (auto dash = chooseRetreatDash(world, terrain, unit, target, direction, reservations, false, false))
    {
        return dash;
    }
    return chooseRetreatDash(world, terrain, unit, target, direction, reservations, false, true);
}

Pointf meleeChaosDesiredPosition(const BattleMovementPlanInput& world,
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

Pointf rangedPeelDashDirection(const BattleMovementPlanInput& world,
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

bool movementPhysicsSegmentWalkable(
    const BattleMovementPhysicsCollisionWorld& world,
    Pointf currentPosition,
    Pointf nextPosition)
{
    auto delta = nextPosition - currentPosition;
    delta.z = 0;
    const double distance = delta.norm();
    const int steps = std::max(1, static_cast<int>(std::ceil(distance / std::max(1.0, world.tileWidth / 4.0))));
    for (int step = 1; step <= steps; ++step)
    {
        const auto probe = currentPosition + (nextPosition - currentPosition) * (static_cast<double>(step) / steps);
        if (!movementPhysicsCellWalkable(world, movementPhysicsCell(world, probe)))
        {
            return false;
        }
    }
    return true;
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
    if (currentPosition.z <= 1.0f && !ignoreUnitCollision)
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

    if (std::min(currentPosition.z, nextPosition.z) >= world.tileWidth * AirborneTerrainClearanceTileFactor)
    {
        return true;
    }

    return movementPhysicsSegmentWalkable(world, currentPosition, nextPosition);
}

BattleMovementPlanner::BattleMovementPlanner(BattleMovementPlanInput world)
    : world_(std::move(world))
{
}

BattleMovementPhysicsState BattleMovementPhysicsSystem::advance(const BattleMovementPhysicsInput& input) const
{
    assert(input.collisionWorld);
    assert(input.unitId >= 0);

    auto state = input.state;
    auto canMove = [&](Pointf position, int separationDistance)
    {
        return canMoveInPhysicsSnapshot(
            *input.collisionWorld,
            input.unitId,
            state.position,
            position,
            separationDistance,
            input.ignoreUnitCollision);
    };

    const auto startPosition = state.position;
    const auto startVelocity = state.velocity;
    const bool movementDashActive = state.movementDashFrames > 0;
    const bool movementDashEnding = state.movementDashFrames == 1;
    const bool postDashRetreatActive = state.postDashRetreatFrames > 0;
    const bool knockbackActive = state.knockbackFrames > 0;
    if (postDashRetreatActive && !knockbackActive && !input.actionDashActive && !movementDashActive)
    {
        state.velocity = state.postDashRetreatVelocity;
    }
    const int separationDistance = input.actionDashActive || movementDashActive || postDashRetreatActive || knockbackActive ? 1 : -1;
    const auto velocity = state.velocity;
    const double stepDistance = knockbackActive
        ? std::max(1.0, input.collisionWorld->tileWidth / 4.0)
        : std::max(static_cast<double>(velocity.norm()), 1.0);
    const int stepCount = std::max(1, static_cast<int>(std::ceil(velocity.norm() / stepDistance)));
    Pointf appliedVelocity;
    bool blocked = false;
    for (int step = 1; step <= stepCount; ++step)
    {
        auto nextPosition = state.position + velocity * (1.0 / stepCount);
        if (canMove(nextPosition, separationDistance))
        {
            appliedVelocity += nextPosition - state.position;
            state.position = nextPosition;
            continue;
        }

        bool canSlide = false;
        auto xOnly = state.position;
        xOnly.x = nextPosition.x;
        if (!knockbackActive && canMove(xOnly, separationDistance))
        {
            appliedVelocity += xOnly - state.position;
            state.position = xOnly;
            state.velocity.y = 0;
            canSlide = true;
        }
        auto yOnly = state.position;
        yOnly.y = nextPosition.y;
        if (!knockbackActive && !canSlide && canMove(yOnly, separationDistance))
        {
            appliedVelocity += yOnly - state.position;
            state.position = yOnly;
            state.velocity.x = 0;
            canSlide = true;
        }

        if (!canSlide)
        {
            blocked = true;
            break;
        }
    }
    if (blocked)
    {
        state.velocity = { 0, 0, 0 };
        state.movementDashFrames = 0;
        state.postDashRetreatFrames = 0;
        state.knockbackFrames = 0;
        state.knockbackControlFrames = 0;
        state.knockbackVelocity = {};
    }
    else if (knockbackActive)
    {
        state.velocity = appliedVelocity;
    }

    if (state.knockbackFrames > 0)
    {
        --state.knockbackFrames;
    }
    state.knockbackVelocity = state.knockbackFrames > 0 ? state.velocity : Pointf{};
    if (state.knockbackControlFrames > 0)
    {
        --state.knockbackControlFrames;
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
    const BattleMovementTerrainLookup terrain(world_);
    return probeMoveInWorld(world_, terrain, unit, nextPosition, ignoreUnits, reservations);
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
    pruneMovementYieldRequests(world_);
    pruneMovementDetourRequests(world_);
    assignMovementTargets(world_);
    const BattleMovementTerrainLookup terrain(world_);
    std::map<int, Pointf> reservations;
    std::map<int, BattleMovementYieldRequest> pendingYieldRequests;
    std::map<int, BattleMovementDetourRequest> pendingDetourRequests;

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
            recordMovementDecision(result, *unit, decision);
            continue;
        }

        if (unit->knockbackFramesRemaining > 0 || unit->knockbackControlFramesRemaining > 0)
        {
            clearMovementReservation(world_, unit->id);
            decision.destination = unit->position;
            recordMovementDecision(result, *unit, decision);
            continue;
        }

        if (unit->slotSwitchCooldownRemaining > 0)
        {
            unit->slotSwitchCooldownRemaining--;
        }

        if (unit->dashFramesRemaining > 0)
        {
            auto next = unit->position + unit->velocity;
            auto probe = terrain.segmentClear(unit->position, next)
                ? probeMoveInWorld(world_, terrain, *unit, next, true, reservations, true, true)
                : MoveProbe{ false, MoveBlockReason::Wall, -1 };
            if (probe.canMove)
            {
                unit->position = next;
                decision.action = MovementAction::Dash;
                decision.velocity = unit->velocity;
                decision.destination = unit->position;
                recordEvent(result.events, BattleEventType::Movement, *unit, decision);
            }
            clearMovementReservation(world_, unit->id);
            if (probe.canMove)
            {
                unit->dashFramesRemaining--;
            }
            else
            {
                unit->velocity = {};
                unit->dashFramesRemaining = 0;
                decision.blockReason = probe.reason;
                decision.blockerId = probe.blockerId;
                if (probe.reason == MoveBlockReason::Wall)
                {
                    recordEvent(result.events, BattleEventType::BlockedByWall, *unit, decision);
                }
            }
            if (probe.canMove && unit->dashFramesRemaining <= 0)
            {
                recordEvent(result.events, BattleEventType::DashEnd, *unit, decision);
            }
            recordMovementDecision(result, *unit, decision);
            continue;
        }

        if (unit->dashCooldownRemaining > 0)
        {
            unit->dashCooldownRemaining--;
        }

        const auto* target = assignedEnemy(world_, *unit);
        if (!target)
        {
            clearMovementReservation(world_, unit->id);
            recordMovementDecision(result, *unit, decision);
            continue;
        }

        if (unit->targetId != target->id)
        {
            unit->targetId = target->id;
        }
        decision.targetId = target->id;
        double targetDistance = distance2d(unit->position, target->position);
        const bool bodyConflict = bodyConflicts(world_, *unit);
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
                terrain,
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
                recordMovementDecision(result, *unit, decision);
                continue;
            }
        }

        if (unit->taXue
            && meleeChaosActive
            && unit->dashCooldownRemaining <= 0)
        {
            auto dash = chooseRetreatDashWithSoftFallback(
                world_,
                terrain,
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
                recordMovementDecision(result, *unit, decision);
                continue;
            }
        }

        const double meleeCastReach = unit->reach > 0.0 && unit->speed > 0.0
            ? std::min(unit->reach, world_.config.meleeAttackReach)
            : unit->reach > 0.0
                ? unit->reach
                : world_.config.meleeAttackReach;
        bool canCastFromHere = unit->canAttack && targetDistance <= unit->reach;
        if (unit->style == CombatStyle::Melee)
        {
            canCastFromHere = unit->canAttack && targetDistance <= meleeCastReach;
        }
        bool rangedCanHold = unit->style == CombatStyle::Ranged && targetDistance <= unit->reach;
        bool meleeReady = unit->style == CombatStyle::Melee && targetDistance <= world_.config.meleeAttackReach;
        if (meleeChaosActive)
        {
            --unit->postDashChaosFramesRemaining;
        }
        if (!meleeChaosActive && !bodyConflict)
        {
            auto cooperativeYield = tryCooperativeYield(
                world_,
                terrain,
                *unit,
                *target,
                canCastFromHere,
                reservations);
            if (cooperativeYield)
            {
                decision = *cooperativeYield;
                unit->position = decision.destination;
                unit->velocity = decision.velocity;
                reserveSameFrameDestination(*unit, decision.destination, reservations);
                reserveMovementDestination(world_, *unit, decision.destination);
                recordEvent(result.events, BattleEventType::Movement, *unit, decision);
                recordMovementDecision(result, *unit, decision);
                continue;
            }
        }
        if (!meleeChaosActive && !bodyConflict && !canCastFromHere)
        {
            auto frontlineSpread = tryFrontlineSoftSpread(
                world_,
                terrain,
                *unit,
                *target,
                reservations);
            if (frontlineSpread)
            {
                decision = *frontlineSpread;
                unit->position = decision.destination;
                unit->velocity = decision.velocity;
                reserveSameFrameDestination(*unit, decision.destination, reservations);
                reserveMovementDestination(world_, *unit, decision.destination);
                recordEvent(result.events, BattleEventType::Movement, *unit, decision);
                recordMovementDecision(result, *unit, decision);
                continue;
            }
        }
        if (!meleeChaosActive && !bodyConflict && (canCastFromHere || rangedCanHold || meleeReady))
        {
            decision.action = MovementAction::AttackReady;
            decision.destination = unit->position;
            unit->velocity = { 0, 0, 0 };
            clearMovementReservation(world_, unit->id);
            recordEvent(result.events, BattleEventType::AttackReady, *unit, decision);
            recordMovementDecision(result, *unit, decision);
            continue;
        }

        Pointf desired = combatSlotPosition(world_, *unit, *target, unit->assignedSlot);
        if (meleeChaosActive)
        {
            desired = meleeChaosDesiredPosition(world_, *unit, *target);
        }
        else if (unit->style == CombatStyle::Melee && targetDistance > world_.config.meleeAttackReach)
        {
            desired = terrain.segmentClear(unit->position, target->position)
                ? combatSlotPosition(world_, *unit, *target, meleeApproachSlot(world_, *unit, *target))
                : target->position;
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
                terrain,
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
                recordMovementDecision(result, *unit, decision);
                continue;
            }
        }

        bool moved = false;
        MoveProbe lastProbe;
        requestApproachCorridorYield(pendingYieldRequests, world_, *unit, desired);
        auto directions = candidateDirections(world_, *unit, *target, desired);
        const auto detourRequest = world_.detourRequests.find(unit->id);
        if (detourRequest != world_.detourRequests.end()
            && detourRequest->second.direction.norm() > 0.01f)
        {
            const auto detourDirection = blendUnitDirections(
                detourRequest->second.direction,
                unit->velocity,
                0.35);
            directions.insert(directions.begin(), detourDirection);
            if (unit->velocity.norm() > 0.01f)
            {
                directions.insert(directions.begin(), unitVector(unit->velocity));
            }
        }
        if (bodyConflict)
        {
            if (auto separationDirection = bodySeparationDirection(world_, *unit))
            {
                directions.insert(directions.begin(), *separationDirection);
            }
        }
        if (!terrain.segmentClear(unit->position, desired))
        {
            if (auto pathDirection = terrainPathDirection(world_, terrain, *unit, desired))
            {
                directions.insert(directions.begin(), *pathDirection);
            }
        }
        for (auto direction : directions)
        {
            auto next = unit->position + direction * unit->speed;
            auto probe = probeMoveInWorld(
                world_,
                terrain,
                *unit,
                next,
                battleMovementTaXueUnstable(*unit),
                reservations,
                battleMovementTaXueUnstable(*unit),
                false);
            lastProbe = probe;
            if (!probe.canMove)
            {
                requestCooperativeMovement(pendingYieldRequests, pendingDetourRequests, world_, terrain, *unit, probe, desired);
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
                auto probe = probeMoveInWorld(world_, terrain, *unit, next, false, reservations, false, true);
                lastProbe = probe;
                if (!probe.canMove)
                {
                    requestCooperativeMovement(pendingYieldRequests, pendingDetourRequests, world_, terrain, *unit, probe, desired);
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

        if (!moved && (lastProbe.reason == MoveBlockReason::Ally || lastProbe.reason == MoveBlockReason::Reservation))
        {
            const auto* blocker = lastProbe.blockerId >= 0
                ? tryFindById(world_.units, lastProbe.blockerId)
                : nullptr;
            if (blocker)
            {
                for (auto direction : blockerDetourDirections(*unit, *blocker, desired))
                {
                    auto next = unit->position + direction * unit->speed;
                    auto probe = probeMoveInWorld(world_, terrain, *unit, next, false, reservations, false, true);
                    lastProbe = probe;
                    if (!probe.canMove)
                    {
                        requestCooperativeMovement(pendingYieldRequests, pendingDetourRequests, world_, terrain, *unit, probe, desired);
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
                    terrain,
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
        recordMovementDecision(result, *unit, decision);
    }

    for (const auto& [unitId, request] : pendingYieldRequests)
    {
        world_.yieldRequests[unitId] = request;
    }
    for (const auto& [unitId, request] : pendingDetourRequests)
    {
        world_.detourRequests[unitId] = request;
    }

    world_.frame++;
    result.frame = world_.frame;
    result.movementReservations = world_.movementReservations;
    result.yieldRequests = world_.yieldRequests;
    result.detourRequests = world_.detourRequests;
    return result;
}

}  // namespace KysChess::Battle
