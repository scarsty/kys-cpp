#pragma once

#include "../Point.h"

#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

inline int faceTowardsFromGridDelta(int deltaX, int deltaY, int fallback)
{
    if (deltaX == 0 && deltaY == 0)
    {
        return fallback;
    }
    if (std::abs(deltaY) >= std::abs(deltaX))
    {
        return deltaY < 0 ? Towards_RightUp : Towards_LeftDown;
    }
    return deltaX < 0 ? Towards_LeftUp : Towards_RightDown;
}

inline Pointf realTowardsFromFaceTowards(int faceTowards)
{
    switch (faceTowards)
    {
    case Towards_RightDown: return {1, 1, 0};
    case Towards_RightUp: return {1, -1, 0};
    case Towards_LeftDown: return {-1, 1, 0};
    case Towards_LeftUp: return {-1, -1, 0};
    default:
        assert(false);
        return {};
    }
}

}  // namespace KysChess::Battle
