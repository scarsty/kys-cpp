#include "BattleRescueRepositionSystem.h"

#include "BattleLogSegments.h"
#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <queue>
#include <string>
#include <utility>

namespace KysChess::Battle
{

namespace
{

constexpr int ProtectSearchRadius = 10;
constexpr int SupportRadius = 4;
constexpr int ProtectionHealPctMaxHp = 10;
constexpr int ProtectionInvincibilityFrames = 10;
constexpr BattlePresentationColor RescueTextColor{ 160, 220, 255, 255 };
constexpr int RescueTextSize = 24;

int distanceCells(Point lhs, Point rhs)
{
    return std::abs(lhs.x - rhs.x) + std::abs(lhs.y - rhs.y);
}

constexpr int NeighborOffsets[4][2] = {
    { 1, 0 },
    { -1, 0 },
    { 0, 1 },
    { 0, -1 },
};

struct BattleRescueSearchSpace
{
    BattleRescueSearchSpace(
        const BattleRescueRepositionInput& input,
        const BattleRescueUnitSnapshot& pulled)
    {
        if (!input.cells.empty())
        {
            buildCellIndex(input.cells);
        }
        buildConnectedCells(input, pulled);
    }

    const BattleRescueCellSnapshot* findCell(Point cell) const
    {
        const std::size_t index = findCellIndex(cell);
        return index == MissingCellIndex ? nullptr : cellsByIndex[index];
    }

    const BattleRescueCellSnapshot& requireCell(Point cell) const
    {
        const std::size_t index = findCellIndex(cell);
        assert(index != MissingCellIndex);
        return *cellsByIndex[index];
    }

    bool legalDestination(const BattleRescueUnitSnapshot& pulled, Point cell) const
    {
        if (cell.x == pulled.cell.x && cell.y == pulled.cell.y)
        {
            return false;
        }

        const std::size_t index = findCellIndex(cell);
        if (index == MissingCellIndex)
        {
            return false;
        }
        const auto& snapshot = *cellsByIndex[index];
        if (!snapshot.walkable || connectedCells[index] == 0)
        {
            return false;
        }
        return !snapshot.occupied || snapshot.occupantUnitId == pulled.id;
    }

private:
    void buildCellIndex(const std::vector<BattleRescueCellSnapshot>& cells)
    {
        minX = cells.front().x;
        minY = cells.front().y;
        int maxX = minX;
        int maxY = minY;
        for (const auto& cell : cells)
        {
            minX = std::min(minX, cell.x);
            minY = std::min(minY, cell.y);
            maxX = std::max(maxX, cell.x);
            maxY = std::max(maxY, cell.y);
        }

        width = maxX - minX + 1;
        height = maxY - minY + 1;
        cellsByIndex.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), nullptr);
        connectedCells.assign(cellsByIndex.size(), 0);

        for (const auto& cell : cells)
        {
            const std::size_t index = cellIndex({ cell.x, cell.y });
            assert(cellsByIndex[index] == nullptr);
            cellsByIndex[index] = &cell;
        }
    }

    std::size_t cellIndex(Point cell) const
    {
        assert(width > 0);
        assert(height > 0);
        assert(cell.x >= minX);
        assert(cell.y >= minY);
        assert(cell.x < minX + width);
        assert(cell.y < minY + height);
        return static_cast<std::size_t>(cell.x - minX) * static_cast<std::size_t>(height)
            + static_cast<std::size_t>(cell.y - minY);
    }

    std::size_t findCellIndex(Point cell) const
    {
        if (width <= 0 || height <= 0)
        {
            return MissingCellIndex;
        }
        if (cell.x < minX || cell.y < minY || cell.x >= minX + width || cell.y >= minY + height)
        {
            return MissingCellIndex;
        }
        const std::size_t index = cellIndex(cell);
        return cellsByIndex[index] == nullptr ? MissingCellIndex : index;
    }

    bool walkable(std::size_t index) const
    {
        assert(index != MissingCellIndex);
        assert(cellsByIndex[index]);
        return cellsByIndex[index]->walkable;
    }

    void addConnectedSeed(std::queue<Point>& frontier, Point cell)
    {
        const std::size_t index = findCellIndex(cell);
        if (index == MissingCellIndex || !walkable(index))
        {
            return;
        }
        if (connectedCells[index] == 0)
        {
            connectedCells[index] = 1;
            frontier.push(cell);
        }
    }

    void buildConnectedCells(
        const BattleRescueRepositionInput& input,
        const BattleRescueUnitSnapshot& pulled)
    {
        std::queue<Point> frontier;
        for (const auto& unit : input.units)
        {
            if (unit.id == pulled.id || !unit.alive || unit.isSummonedClone)
            {
                continue;
            }
            addConnectedSeed(frontier, unit.cell);
        }

        while (!frontier.empty())
        {
            const Point current = frontier.front();
            frontier.pop();

            for (const auto& [dx, dy] : NeighborOffsets)
            {
                Point next{ current.x + dx, current.y + dy };
                const std::size_t index = findCellIndex(next);
                if (index == MissingCellIndex || connectedCells[index] != 0 || !walkable(index))
                {
                    continue;
                }
                connectedCells[index] = 1;
                frontier.push(next);
            }
        }
    }

    static constexpr std::size_t MissingCellIndex = std::numeric_limits<std::size_t>::max();

    int minX{};
    int minY{};
    int width{};
    int height{};
    std::vector<const BattleRescueCellSnapshot*> cellsByIndex;
    std::vector<std::uint8_t> connectedCells;
};

std::vector<const BattleRescueUnitSnapshot*> pullCandidates(const BattleRescueRepositionInput& input)
{
    std::vector<const BattleRescueUnitSnapshot*> candidates;
    for (const auto& unit : input.units)
    {
        if (!unit.alive || unit.team != input.pullerTeam || unit.isSummonedClone)
        {
            continue;
        }
        if (input.mode == BattleRescuePullMode::Execute)
        {
            if (!unit.forcePullExecute || unit.forcePullExecuteRemaining <= 0)
            {
                continue;
            }
        }
        else if (!unit.forcePullProtect || unit.forcePullProtectRemaining <= 0)
        {
            continue;
        }
        candidates.push_back(&unit);
    }
    return candidates;
}

std::vector<Point> executeCellsForPuller(
    const BattleRescueSearchSpace& searchSpace,
    const BattleRescueUnitSnapshot& pulled,
    const BattleRescueUnitSnapshot& puller)
{
    std::vector<Point> cells;
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            Point cell{ puller.cell.x + dx, puller.cell.y + dy };
            if (searchSpace.legalDestination(pulled, cell))
            {
                cells.push_back(cell);
            }
        }
    }
    std::sort(cells.begin(), cells.end(), [&](Point lhs, Point rhs)
        {
            const int lhsDistance = distanceCells(lhs, pulled.cell);
            const int rhsDistance = distanceCells(rhs, pulled.cell);
            if (lhsDistance != rhsDistance)
            {
                return lhsDistance < rhsDistance;
            }
            if (lhs.x != rhs.x)
            {
                return lhs.x < rhs.x;
            }
            return lhs.y < rhs.y;
        });
    return cells;
}

std::vector<Point> protectionCellsForPuller(
    const BattleRescueSearchSpace& searchSpace,
    const BattleRescueUnitSnapshot& pulled,
    const BattleRescueUnitSnapshot& puller)
{
    std::vector<Point> cells;
    cells.reserve(2 * ProtectSearchRadius * ProtectSearchRadius + 2 * ProtectSearchRadius + 1);
    for (int dx = -ProtectSearchRadius; dx <= ProtectSearchRadius; ++dx)
    {
        const int remainingDistance = ProtectSearchRadius - std::abs(dx);
        for (int dy = -remainingDistance; dy <= remainingDistance; ++dy)
        {
            Point cell{ puller.cell.x + dx, puller.cell.y + dy };
            if (searchSpace.legalDestination(pulled, cell))
            {
                cells.push_back(cell);
            }
        }
    }
    return cells;
}

struct ProtectionContext
{
    int pullerTeam{};
    std::vector<const BattleRescueUnitSnapshot*> units;
    Point allyCenter{};
    Point enemyCenter{};
    Point backrow{};
    double backrowLength{};
};

ProtectionContext makeProtectionContext(
    const BattleRescueRepositionInput& input,
    const BattleRescueUnitSnapshot& pulled)
{
    ProtectionContext context;
    context.pullerTeam = input.pullerTeam;

    Point allySum;
    Point enemySum;
    int allyCount = 0;
    int enemyCount = 0;
    context.units.reserve(input.units.size());
    for (const auto& unit : input.units)
    {
        if (!unit.alive || unit.id == pulled.id)
        {
            continue;
        }
        context.units.push_back(&unit);
        if (unit.team == input.pullerTeam)
        {
            allySum.x += unit.cell.x;
            allySum.y += unit.cell.y;
            ++allyCount;
        }
        else
        {
            enemySum.x += unit.cell.x;
            enemySum.y += unit.cell.y;
            ++enemyCount;
        }
    }
    if (allyCount > 0)
    {
        context.allyCenter.x = allySum.x / allyCount;
        context.allyCenter.y = allySum.y / allyCount;
    }
    if (enemyCount > 0)
    {
        context.enemyCenter.x = enemySum.x / enemyCount;
        context.enemyCenter.y = enemySum.y / enemyCount;
    }
    context.backrow = { context.allyCenter.x - context.enemyCenter.x, context.allyCenter.y - context.enemyCenter.y };
    context.backrowLength = std::sqrt(static_cast<double>(
        context.backrow.x * context.backrow.x + context.backrow.y * context.backrow.y));
    return context;
}

struct ProtectionChoice
{
    bool valid = false;
    const BattleRescueUnitSnapshot* puller = nullptr;
    Point cell;
    int enemyThreat = std::numeric_limits<int>::max();
    int enemyMinDistance = std::numeric_limits<int>::min();
    int allySupport = std::numeric_limits<int>::min();
    double backrowScore = std::numeric_limits<double>::lowest();
    int pullerDistance = std::numeric_limits<int>::max();
};

bool betterProtectionChoice(const ProtectionChoice& lhs, const ProtectionChoice& rhs)
{
    if (!rhs.valid)
    {
        return lhs.valid;
    }
    if (!lhs.valid)
    {
        return false;
    }
    if (lhs.enemyThreat != rhs.enemyThreat)
    {
        return lhs.enemyThreat < rhs.enemyThreat;
    }
    if (lhs.enemyMinDistance != rhs.enemyMinDistance)
    {
        return lhs.enemyMinDistance > rhs.enemyMinDistance;
    }
    if (lhs.allySupport != rhs.allySupport)
    {
        return lhs.allySupport > rhs.allySupport;
    }
    if (std::abs(lhs.backrowScore - rhs.backrowScore) > 0.000001)
    {
        return lhs.backrowScore > rhs.backrowScore;
    }
    if (lhs.pullerDistance != rhs.pullerDistance)
    {
        return lhs.pullerDistance < rhs.pullerDistance;
    }
    if (lhs.cell.x != rhs.cell.x)
    {
        return lhs.cell.x < rhs.cell.x;
    }
    return lhs.cell.y < rhs.cell.y;
}

ProtectionChoice protectionChoiceForCell(
    const ProtectionContext& context,
    const BattleRescueUnitSnapshot& puller,
    Point cell)
{
    ProtectionChoice choice;
    choice.valid = true;
    choice.puller = &puller;
    choice.cell = cell;
    choice.enemyThreat = 0;
    choice.enemyMinDistance = std::numeric_limits<int>::max();
    choice.allySupport = 0;
    choice.pullerDistance = distanceCells(cell, puller.cell);

    for (const auto* unit : context.units)
    {
        const int distance = distanceCells(cell, unit->cell);
        if (unit->team == context.pullerTeam)
        {
            if (distance <= SupportRadius)
            {
                ++choice.allySupport;
            }
        }
        else
        {
            choice.enemyMinDistance = std::min(choice.enemyMinDistance, distance);
            if (distance <= SupportRadius)
            {
                ++choice.enemyThreat;
            }
        }
    }
    if (choice.enemyMinDistance == std::numeric_limits<int>::max())
    {
        choice.enemyMinDistance = 1000000;
    }
    if (context.backrowLength > 0.0001)
    {
        const Point candidateFromAlly{ cell.x - context.allyCenter.x, cell.y - context.allyCenter.y };
        choice.backrowScore = (candidateFromAlly.x * context.backrow.x + candidateFromAlly.y * context.backrow.y)
            / context.backrowLength;
    }
    return choice;
}

ProtectionChoice resolveProtectionChoice(
    const BattleRescueRepositionInput& input,
    const BattleRescueSearchSpace& searchSpace,
    const BattleRescueUnitSnapshot& pulled,
    const std::vector<const BattleRescueUnitSnapshot*>& candidates)
{
    const auto context = makeProtectionContext(input, pulled);
    ProtectionChoice best;
    for (const auto* puller : candidates)
    {
        ProtectionChoice bestForPuller;
        for (const Point cell : protectionCellsForPuller(searchSpace, pulled, *puller))
        {
            auto choice = protectionChoiceForCell(context, *puller, cell);
            if (betterProtectionChoice(choice, bestForPuller))
            {
                bestForPuller = choice;
            }
        }
        if (betterProtectionChoice(bestForPuller, best))
        {
            best = bestForPuller;
        }
    }
    return best;
}

BattleVisualEvent floatingTextEvent(int targetUnitId)
{
    BattleVisualEvent event;
    event.type = BattleVisualEventType::FloatingText;
    event.targetUnitId = targetUnitId;
    event.text = "挪移";
    event.color = RescueTextColor;
    event.textSize = RescueTextSize;
    return event;
}

BattleLogEvent statusEvent(int sourceUnitId, int targetUnitId, std::string text)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Status;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.segments = battleLogText(std::move(text), BattleLogTextTone::SkillName);
    return event;
}

BattleLogEvent healEvent(int sourceUnitId, int targetUnitId, int amount)
{
    BattleLogEvent event;
    event.type = BattleLogEventType::Heal;
    event.sourceUnitId = sourceUnitId;
    event.targetUnitId = targetUnitId;
    event.amount = amount;
    event.segments = battleLogText("保護挪移", BattleLogTextTone::SkillName);
    return event;
}

void commitProtectionResult(
    BattleRescueRepositionResult& result,
    const BattleRescueUnitSnapshot& pulled,
    const BattleRescueUnitSnapshot& puller,
    const BattleRescueCellSnapshot& destination)
{
    result.teleport = BattleRescueTeleportDelta{ pulled.id, puller.id, { destination.x, destination.y }, destination.position };
    result.counterDelta = { puller.id, -1, 0 };
    const int requestedHeal = std::max(1, pulled.maxHp * ProtectionHealPctMaxHp / 100);
    result.heal = { pulled.id, std::max(0, std::min(pulled.maxHp, pulled.hp + requestedHeal) - pulled.hp) };
    result.invincibility = { pulled.id, ProtectionInvincibilityFrames };
    if (result.heal.amount > 0)
    {
        result.logEvents.push_back(healEvent(puller.id, pulled.id, result.heal.amount));
    }
    result.logEvents.push_back(statusEvent(puller.id, pulled.id, "保護挪移"));
    result.logEvents.push_back(statusEvent(puller.id, pulled.id, "短暫無敵（10幀）"));
    result.visualEvents.push_back(floatingTextEvent(pulled.id));
}

void commitExecuteResult(
    BattleRescueRepositionResult& result,
    const BattleRescueUnitSnapshot& pulled,
    const BattleRescueUnitSnapshot& puller,
    const BattleRescueCellSnapshot& destination)
{
    result.teleport = BattleRescueTeleportDelta{ pulled.id, puller.id, { destination.x, destination.y }, destination.position };
    result.counterDelta = { puller.id, 0, -1 };
    result.basicCounterAttack = BattleRescueBasicCounterAttackCommand{ puller.id, pulled.id };
    result.logEvents.push_back(statusEvent(puller.id, pulled.id, "處決挪移"));
    result.visualEvents.push_back(floatingTextEvent(pulled.id));
}

}  // namespace

BattleRescueRepositionResult BattleRescueRepositionSystem::resolve(
    const BattleRescueRepositionInput& input) const
{
    const auto& pulled = requireById(input.units, input.pulledUnitId);
    if (!pulled.alive)
    {
        return {};
    }

    auto candidates = pullCandidates(input);
    BattleRescueSearchSpace searchSpace(input, pulled);
    std::sort(candidates.begin(), candidates.end(), [&](const auto* lhs, const auto* rhs)
        {
            const int lhsDistance = distanceCells(lhs->cell, pulled.cell);
            const int rhsDistance = distanceCells(rhs->cell, pulled.cell);
            if (lhsDistance != rhsDistance)
            {
                return lhsDistance < rhsDistance;
            }
            return lhs->id < rhs->id;
        });

    BattleRescueRepositionResult result;
    if (input.mode == BattleRescuePullMode::Execute)
    {
        for (const auto* puller : candidates)
        {
            auto cells = executeCellsForPuller(searchSpace, pulled, *puller);
            if (cells.empty())
            {
                continue;
            }
            commitExecuteResult(result, pulled, *puller, searchSpace.requireCell(cells.front()));
            return result;
        }
        return result;
    }

    auto choice = resolveProtectionChoice(input, searchSpace, pulled, candidates);
    if (choice.valid)
    {
        commitProtectionResult(result, pulled, *choice.puller, searchSpace.requireCell(choice.cell));
    }
    return result;
}

}  // namespace KysChess::Battle
