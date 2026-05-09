#pragma once

#include "BattlePresentation.h"
#include "../Point.h"

#include <optional>
#include <vector>

namespace KysChess::Battle
{

enum class BattleRescuePullMode
{
    Protect,
    Execute,
};

struct BattleRescueUnitSnapshot
{
    int id = -1;
    int team = 0;
    bool alive = true;
    bool isSummonedClone = false;
    int hp = 0;
    int maxHp = 0;
    int invincible = 0;
    bool forcePullProtect = false;
    int forcePullProtectRemaining = 0;
    bool forcePullExecute = false;
    int forcePullExecuteRemaining = 0;
    Point cell;
};

struct BattleRescueCellSnapshot
{
    int x = 0;
    int y = 0;
    bool walkable = false;
    bool occupied = false;
    int occupantUnitId = -1;
};

struct BattleRescueTeleportDelta
{
    int unitId{};
    int pullerUnitId{};
    Point destinationCell;
};

struct BattleRescueCounterDelta
{
    int unitId = -1;
    int protectRemainingDelta{};
    int executeRemainingDelta{};
};

struct BattleRescueHealDelta
{
    int targetUnitId = -1;
    int amount{};
};

struct BattleRescueInvincibilityDelta
{
    int targetUnitId = -1;
    int frames{};
};

struct BattleRescueBasicCounterAttackCommand
{
    int attackerUnitId{};
    int targetUnitId{};
};

struct BattleRescueRepositionInput
{
    BattleRescuePullMode mode = BattleRescuePullMode::Protect;
    int pulledUnitId = -1;
    int pullerTeam = 0;
    std::vector<BattleRescueUnitSnapshot> units;
    std::vector<BattleRescueCellSnapshot> cells;
};

struct BattleRescueRepositionResult
{
    std::optional<BattleRescueTeleportDelta> teleport;
    BattleRescueCounterDelta counterDelta;
    BattleRescueHealDelta heal;
    BattleRescueInvincibilityDelta invincibility;
    std::optional<BattleRescueBasicCounterAttackCommand> basicCounterAttack;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

class BattleRescueRepositionSystem
{
public:
    BattleRescueRepositionResult resolve(const BattleRescueRepositionInput& input) const;
};

}  // namespace KysChess::Battle
