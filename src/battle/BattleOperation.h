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

inline int battleOperationIndex(BattleOperationType operation)
{
    assert(isBattleOperation(operation));
    return static_cast<int>(operation);
}

inline int toLegacyOperationType(BattleOperationType operation)
{
    return static_cast<int>(operation);
}

inline BattleOperationType battleOperationFromLegacy(int operationType)
{
    switch (operationType)
    {
    case 0:
        return BattleOperationType::Melee;
    case 1:
        return BattleOperationType::TrackingProjectile;
    case 2:
        return BattleOperationType::RangedProjectile;
    case 3:
        return BattleOperationType::Dash;
    default:
        return BattleOperationType::None;
    }
}

}  // namespace KysChess::Battle
