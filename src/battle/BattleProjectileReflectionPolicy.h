#pragma once

#include "BattleAttackDelivery.h"

namespace KysChess::Battle
{

class BattleProjectileReflectionPolicy
{
public:
    bool allows(const BattleProjectileReflectionDescriptor& descriptor) const;
};

}  // namespace KysChess::Battle
