#pragma once

#include "../Point.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace KysChess::Battle
{

constexpr int battleSaturatedInt(std::int64_t value)
{
    return static_cast<int>(std::clamp<std::int64_t>(
        value,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max()));
}

constexpr int battleSaturatedAdd(int lhs, int rhs)
{
    return battleSaturatedInt(
        static_cast<std::int64_t>(lhs) + static_cast<std::int64_t>(rhs));
}

constexpr int battleSaturatedMultiply(int lhs, int rhs)
{
    return battleSaturatedInt(
        static_cast<std::int64_t>(lhs) * static_cast<std::int64_t>(rhs));
}

constexpr std::int64_t battleSaturatedAdd64(std::int64_t lhs, std::int64_t rhs)
{
    if (rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs)
    {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)
    {
        return std::numeric_limits<std::int64_t>::min();
    }
    return lhs + rhs;
}

inline constexpr double BattlePi = 0x1.921fb54442d18p+1;

struct BattleSinCos
{
    double sine{};
    double cosine{};
};

enum class BattleFacingArc
{
    Front,
    Side,
    Back,
};

BattleSinCos deterministicSinCos(double radians);
double deterministicAtan2(double y, double x);
double deterministicAngleDelta(double lhs, double rhs);
Pointf rotateBattlePoint(Pointf point, double radians);
Pointf battleDirection(double radians);
Point battleIsometricGridPosition(Pointf position, int coordCount, double tileWidth);
Point battleCartesianGridPosition(Pointf position, Pointf origin, double tileWidth);
int battleTravelFrames2d(Pointf start, Pointf end, double speed);
int battleTravelFrames3d(Pointf start, Pointf end, double speed);
std::uint64_t battleDistanceSquared2d(Pointf lhs, Pointf rhs);
inline double battleDistance2d(Pointf a, Pointf b)
{
    return EuclidDis(a.x - b.x, a.y - b.y);
}

std::uint64_t battleDistanceSquared3d(Pointf lhs, Pointf rhs);
BattleFacingArc classifyBattleFacing(Pointf attackVector, Pointf defenderFacing);
bool battlePointSegmentWithinRadius(
    Pointf point,
    Pointf segmentStart,
    Pointf segmentEnd,
    double radius,
    bool inclusive = true);
bool battleSegmentsWithinRadius(
    Pointf lhsStart,
    Pointf lhsEnd,
    Pointf rhsStart,
    Pointf rhsEnd,
    double radius,
    bool inclusive = true);

inline double pointDistance(const Pointf& lhs, const Pointf& rhs)
{
    const double dx = static_cast<double>(lhs.x) - rhs.x;
    const double dy = static_cast<double>(lhs.y) - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
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
