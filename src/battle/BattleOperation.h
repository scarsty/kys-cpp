#pragma once

#include <cassert>

namespace KysChess::Battle
{

enum class BattleOperationType
{
    None = -1,
    Melee = 0,
    TrackingProjectile = 1,
    RangedProjectile = 2,
    Dash = 3,
};

inline bool isBattleOperation(BattleOperationType operation)
{
    return operation == BattleOperationType::Melee
        || operation == BattleOperationType::TrackingProjectile
        || operation == BattleOperationType::RangedProjectile
        || operation == BattleOperationType::Dash;
}

inline int battleOperationDamagePct(BattleOperationType operation)
{
    switch (operation)
    {
    case BattleOperationType::TrackingProjectile:
        return 150;
    default:
        return 100;
    }
}

inline int battleOperationIndex(BattleOperationType operation)
{
    assert(isBattleOperation(operation));
    return static_cast<int>(operation);
}

inline int toPresentationOperationKind(BattleOperationType operation)
{
    return static_cast<int>(operation);
}

}  // namespace KysChess::Battle
