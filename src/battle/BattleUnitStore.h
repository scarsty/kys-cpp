#pragma once

#include "../BattleUnitIdentity.h"
#include "BattleOperation.h"
#include "BattleTypes.h"
#include "BattleUnitValues.h"

#include <array>
#include <map>
#include <string>

namespace KysChess::Battle
{

struct BattleDamageUnitState;
class BattleRuntimeUnits;

struct BattleGridTransform
{
    double tileWidth = 0.0;
    int coordCount = 0;

    Point toGrid(Pointf position) const;
};

struct BattleRuntimeUnit
{
    int id = -1;
    int cloneSourceUnitId = -1;
    int realRoleId = -1;
    std::string name;
    int headId = -1;
    std::array<int, 5> fightFrames{};
    std::string skillNames;
    int team = 0;
    bool alive = true;
    BattleUnitVitals vitals;
    BattleUnitStats stats;
    BattleUnitMotion motion;
    BattleUnitAnimationState animation;
    bool haveAction = false;
    BattleOperationType operationType = BattleOperationType::None;
    int operationCount = 0;
    int physicalPower = 0;
    int invincible = 0;
    int shield = 0;
    std::map<int, int> actPropertiesByMagicType;
    Point grid;
    bool canAttack = true;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
    int star = 1;
    int cost = 0;
    int weaponId = -1;
    int armorId = -1;
    int chessInstanceId = -1;

    BattleUnitIdentity identity() const
    {
        return { id, realRoleId, team, headId, name };
    }
};

constexpr double BattleRuntimeMoveSpeedDivisor = 22.0;

int findNearestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId);
int findFarthestEnemyUnitId(const BattleRuntimeUnits& units, int sourceUnitId);
BattleUnitState makeBattleMovementPlanUnit(const BattleRuntimeUnit& unit, double moveSpeedDivisor);

}  // namespace KysChess::Battle
