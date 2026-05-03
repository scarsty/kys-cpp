#pragma once

#include <cassert>

namespace KysChess::Battle
{

inline int adjustedMpRestore(bool mpBlocked, int mpRecoveryBonusPct, int amount)
{
    assert(amount >= 0);
    assert(mpRecoveryBonusPct >= 0);

    if (amount == 0 || mpBlocked)
    {
        return 0;
    }

    double adjusted = amount;
    if (mpRecoveryBonusPct > 0)
    {
        adjusted *= 1.0 + mpRecoveryBonusPct / 100.0;
    }
    return static_cast<int>(adjusted);
}

}  // namespace KysChess::Battle
