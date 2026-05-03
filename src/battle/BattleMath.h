#pragma once

#include "../Point.h"

#include <cmath>

namespace KysChess::Battle
{

inline double pointDistance(const Pointf& lhs, const Pointf& rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline double pointNorm(const Pointf& point)
{
    return std::sqrt(static_cast<double>(point.x) * point.x
        + static_cast<double>(point.y) * point.y
        + static_cast<double>(point.z) * point.z);
}

inline Pointf normalizedTo(Pointf point, double length)
{
    const double current = pointNorm(point);
    if (current <= 0.0001)
    {
        return {};
    }
    point.x = static_cast<float>(point.x * length / current);
    point.y = static_cast<float>(point.y * length / current);
    point.z = static_cast<float>(point.z * length / current);
    return point;
}

inline Pointf scaled(Pointf point, double length)
{
    point.x = static_cast<float>(point.x * length);
    point.y = static_cast<float>(point.y * length);
    point.z = static_cast<float>(point.z * length);
    return point;
}

}  // namespace KysChess::Battle
