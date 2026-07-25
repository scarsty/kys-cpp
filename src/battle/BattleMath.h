#pragma once

#include "../Point.h"

#include <cassert>
#include <cmath>

namespace KysChess::Battle
{

inline constexpr double BattlePi = 0x1.921fb54442d18p+1;

struct BattleSinCos
{
    double sine{};
    double cosine{};
};

BattleSinCos deterministicSinCos(double radians);
double deterministicAtan2(double y, double x);
double deterministicAngleDelta(double lhs, double rhs);
Pointf rotateBattlePoint(Pointf point, double radians);
Pointf battleDirection(double radians);

inline double pointDistance(const Pointf& lhs, const Pointf& rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

inline double pointDistanceSquared(const Pointf& lhs, const Pointf& rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return dx * dx + dy * dy;
}

inline Pointf normalizedTo(Pointf point, double length, double minimumNorm)
{
    assert(minimumNorm > 0.0);
    const double current = point.norm();
    if (current <= minimumNorm)
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
