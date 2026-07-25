#pragma once

#include "../Point.h"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace KysChess::Battle
{

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
