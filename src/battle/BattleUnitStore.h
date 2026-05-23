#pragma once

#include "BattleOperation.h"
#include "BattleTypes.h"
#include "BattleUnitValues.h"

#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleDamageUnitState;

struct BattleGridTransform
{
    double tileWidth = 0.0;
    int coordCount = 0;

    Point toGrid(Pointf position) const;
};

struct BattleRuntimeUnit
{
    int id = -1;
    int presentationSourceUnitId = -1;
    int realRoleId = -1;
    std::string name;
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
    int blockFirstHitsRemaining = 0;
    int frozen = 0;
    int frozenMax = 0;
    bool mpBlocked = false;
    int mpRecoveryBonusPct = 0;
    std::map<int, int> actPropertiesByMagicType;
    Point grid;
    bool canAttack = true;
    double reach = 0.0;
    CombatStyle style = CombatStyle::Melee;
    int star = 1;
    int cost = 0;
};

struct BattleUnitStore
{
    BattleGridTransform gridTransform;
    std::vector<BattleRuntimeUnit> units;

    BattleRuntimeUnit* findUnit(int unitId);
    const BattleRuntimeUnit* findUnit(int unitId) const;
    BattleRuntimeUnit& requireUnit(int unitId);
    const BattleRuntimeUnit& requireUnit(int unitId) const;
    void writeDamageUnit(const BattleDamageUnitState& source);
    void setPosition(int unitId, Pointf position);
    void setMotion(int unitId, Pointf position, Pointf velocity, Pointf acceleration);
};

constexpr double BattleRuntimeMoveSpeedDivisor = 22.0;

int findNearestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId);
int findFarthestEnemyUnitId(const BattleUnitStore& units, int sourceUnitId);
BattleUnitState makeBattleMovementPlanUnit(const BattleRuntimeUnit& unit, double moveSpeedDivisor);

}  // namespace KysChess::Battle
