#pragma once

#include "BattleTypes.h"

namespace KysChess::Battle
{

struct BattleCastGeometry;
enum class BattleOperationType;

double battleAttackReach(
    BattleOperationType operation,
    int selectDistance,
    const BattleMovementConfig& movement,
    const BattleCastGeometry& cast,
    double meleeAttackHitRadius,
    double heavyAttackReach);

class BattleGeometry
{
public:
    explicit BattleGeometry(BattleMovementGeometry geometry);

    const BattleMovementGeometry& source() const { return source_; }
    const BattleMovementConfig& movementConfig() const { return movementConfig_; }

private:
    BattleMovementGeometry source_;
    BattleMovementConfig movementConfig_;
};

}  // namespace KysChess::Battle
