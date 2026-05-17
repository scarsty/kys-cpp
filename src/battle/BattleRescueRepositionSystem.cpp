#include "BattleRescueRepositionSystem.h"

#include "BattleLogSegments.h"
#include "../Find.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
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

bool legalCell(const BattleRescueRepositionInput& input, const BattleRescueUnitSnapshot& pulled, Point cell)
{
    if (cell.x == pulled.cell.x && cell.y == pulled.cell.y)
    {
        return false;
    }

    auto it = std::ranges::find_if(input.cells, [&](const BattleRescueCellSnapshot& snapshot)
        {
            return snapshot.x == cell.x && snapshot.y == cell.y;
        });
    if (it == std::ranges::end(input.cells))
    {
        return false;
    }
    if (!it->walkable)
    {
        return false;
    }
    return !it->occupied || it->occupantUnitId == pulled.id;
}

const BattleRescueCellSnapshot& requireCell(const BattleRescueRepositionInput& input, Point cell)
{
    auto it = std::ranges::find_if(input.cells, [&](const BattleRescueCellSnapshot& snapshot)
        {
            return snapshot.x == cell.x && snapshot.y == cell.y;
        });
    assert(it != std::ranges::end(input.cells));
    return *it;
}

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
    const BattleRescueRepositionInput& input,
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
            if (legalCell(input, pulled, cell))
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
    const BattleRescueRepositionInput& input,
    const BattleRescueUnitSnapshot& pulled,
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

    Point allyCenter;
    Point enemyCenter;
    int allyCount = 0;
    int enemyCount = 0;
    for (const auto& unit : input.units)
    {
        if (!unit.alive || unit.id == pulled.id)
        {
            continue;
        }
        if (unit.team == input.pullerTeam)
        {
            allyCenter.x += unit.cell.x;
            allyCenter.y += unit.cell.y;
            ++allyCount;
        }
        else
        {
            enemyCenter.x += unit.cell.x;
            enemyCenter.y += unit.cell.y;
            ++enemyCount;
        }
    }
    if (allyCount > 0)
    {
        allyCenter.x /= allyCount;
        allyCenter.y /= allyCount;
    }
    if (enemyCount > 0)
    {
        enemyCenter.x /= enemyCount;
        enemyCenter.y /= enemyCount;
    }
    const Point backrow{ allyCenter.x - enemyCenter.x, allyCenter.y - enemyCenter.y };
    const double backrowLength = std::sqrt(static_cast<double>(backrow.x * backrow.x + backrow.y * backrow.y));

    for (const auto& unit : input.units)
    {
        if (!unit.alive || unit.id == pulled.id)
        {
            continue;
        }
        const int distance = distanceCells(cell, unit.cell);
        if (unit.team == input.pullerTeam)
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
    if (backrowLength > 0.0001)
    {
        const Point candidateFromAlly{ cell.x - allyCenter.x, cell.y - allyCenter.y };
        choice.backrowScore = (candidateFromAlly.x * backrow.x + candidateFromAlly.y * backrow.y) / backrowLength;
    }
    return choice;
}

ProtectionChoice resolveProtectionChoice(
    const BattleRescueRepositionInput& input,
    const BattleRescueUnitSnapshot& pulled,
    const std::vector<const BattleRescueUnitSnapshot*>& candidates)
{
    ProtectionChoice best;
    for (const auto* puller : candidates)
    {
        ProtectionChoice bestForPuller;
        for (const auto& cellSnapshot : input.cells)
        {
            Point cell{ cellSnapshot.x, cellSnapshot.y };
            if (distanceCells(cell, puller->cell) > ProtectSearchRadius)
            {
                continue;
            }
            if (!legalCell(input, pulled, cell))
            {
                continue;
            }
            auto choice = protectionChoiceForCell(input, pulled, *puller, cell);
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
            auto cells = executeCellsForPuller(input, pulled, *puller);
            if (cells.empty())
            {
                continue;
            }
            commitExecuteResult(result, pulled, *puller, requireCell(input, cells.front()));
            return result;
        }
        return result;
    }

    auto choice = resolveProtectionChoice(input, pulled, candidates);
    if (choice.valid)
    {
        commitProtectionResult(result, pulled, *choice.puller, requireCell(input, choice.cell));
    }
    return result;
}

}  // namespace KysChess::Battle
