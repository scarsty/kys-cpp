#pragma once

#include "BattleTypes.h"

namespace KysChess::Battle
{

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
