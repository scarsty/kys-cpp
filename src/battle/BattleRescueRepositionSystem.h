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
    int unitId = -1;
    int pullerUnitId = -1;
    Point destinationCell;
};

struct BattleRescueCounterDelta
{
    int unitId = -1;
    int protectRemainingDelta = 0;
    int executeRemainingDelta = 0;
};

struct BattleRescueHealDelta
{
    int targetUnitId = -1;
    int amount = 0;
};

struct BattleRescueInvincibilityDelta
{
    int targetUnitId = -1;
    int frames = 0;
};

struct BattleRescueBasicCounterAttackCommand
{
    int attackerUnitId = -1;
    int targetUnitId = -1;
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
    std::vector<BattlePresentationEvent> presentationEvents;
};

class BattleRescueRepositionSystem
{
public:
    BattleRescueRepositionResult resolve(const BattleRescueRepositionInput& input) const;
};

}  // namespace KysChess::Battle
