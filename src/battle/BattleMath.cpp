#include "BattleMath.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <compare>
#include <cstdint>
#include <limits>

namespace KysChess::Battle
{

namespace
{

using AngleUnits = std::int64_t;

struct FixedPoint3
{
    std::int64_t x{};
    std::int64_t y{};
    std::int64_t z{};
};

struct WideUnsigned
{
    std::uint64_t high{};
    std::uint64_t low{};

    auto operator<=>(const WideUnsigned&) const = default;
};

constexpr int AngleBits = 48;
constexpr AngleUnits FullTurn = AngleUnits{1} << AngleBits;
constexpr AngleUnits HalfTurn = FullTurn / 2;
constexpr AngleUnits QuarterTurn = FullTurn / 4;
constexpr AngleUnits FixedScale = AngleUnits{1} << 48;
constexpr AngleUnits CordicGain = 170926505739102;
constexpr double RadiansToAngleUnits = 0x1.45f306dc9c883p+45;
constexpr double AngleUnitsToRadians = 0x1.921fb54442d18p-46;
constexpr std::int64_t GeometryScale = std::int64_t{1} << 10;

constexpr std::array<AngleUnits, 47> CordicAngles{
    35184372088832, 20770547670515, 10974586953444, 5570871696862, 2796246208089, 1399486241028,
    699913886760, 349978300884, 174991820497, 87496244017, 43748163730, 21874087080,
    10937044192, 5468522177, 2734261099, 1367130551, 683565276, 341782638,
    170891319, 85445659, 42722830, 21361415, 10680707, 5340354,
    2670177, 1335088, 667544, 333772, 166886, 83443,
    41722, 20861, 10430, 5215, 2608, 1304,
    652, 326, 163, 81, 41, 20,
    10, 5, 3, 1, 1,
};

AngleUnits roundedInteger(double value)
{
    assert(std::isfinite(value));
    assert(value < static_cast<double>(std::numeric_limits<AngleUnits>::max()));
    assert(value > static_cast<double>(std::numeric_limits<AngleUnits>::min()));
    return static_cast<AngleUnits>(value + (value >= 0.0 ? 0.5 : -0.5));
}

AngleUnits normalizedAngleUnits(double radians)
{
    AngleUnits units = roundedInteger(radians * RadiansToAngleUnits) % FullTurn;
    if (units >= HalfTurn)
    {
        units -= FullTurn;
    }
    else if (units < -HalfTurn)
    {
        units += FullTurn;
    }
    return units;
}

AngleUnits floorDivideByPowerOfTwo(AngleUnits value, int shift)
{
    if (shift == 0)
    {
        return value;
    }
    if (value >= 0)
    {
        return value >> shift;
    }
    return -1 - ((-1 - value) >> shift);
}

double fixedUnitValue(AngleUnits value)
{
    return std::clamp(static_cast<double>(value) / FixedScale, -1.0, 1.0);
}

double radiansFromAngleUnits(AngleUnits units)
{
    return static_cast<double>(units) * AngleUnitsToRadians;
}

std::int64_t fixedGeometryValue(double value)
{
    assert(std::isfinite(value));
    const double scaled = value * static_cast<double>(GeometryScale);
    assert(scaled < static_cast<double>(std::numeric_limits<std::int64_t>::max()));
    assert(scaled > static_cast<double>(std::numeric_limits<std::int64_t>::min()));
    return static_cast<std::int64_t>(scaled + (scaled >= 0.0 ? 0.5 : -0.5));
}

FixedPoint3 fixedPoint(Pointf point)
{
    return {
        fixedGeometryValue(point.x),
        fixedGeometryValue(point.y),
        fixedGeometryValue(point.z),
    };
}

std::uint64_t unsignedMagnitude(std::int64_t value)
{
    return value >= 0
        ? static_cast<std::uint64_t>(value)
        : static_cast<std::uint64_t>(-(value + 1)) + 1;
}

WideUnsigned wideMultiply(std::uint64_t lhs, std::uint64_t rhs)
{
    const std::uint64_t lhsLow = static_cast<std::uint32_t>(lhs);
    const std::uint64_t lhsHigh = lhs >> 32;
    const std::uint64_t rhsLow = static_cast<std::uint32_t>(rhs);
    const std::uint64_t rhsHigh = rhs >> 32;

    const std::uint64_t lowProduct = lhsLow * rhsLow;
    const std::uint64_t crossOne = lhsLow * rhsHigh;
    const std::uint64_t crossTwo = lhsHigh * rhsLow;
    const std::uint64_t highProduct = lhsHigh * rhsHigh;
    const std::uint64_t middle = (lowProduct >> 32)
        + static_cast<std::uint32_t>(crossOne)
        + static_cast<std::uint32_t>(crossTwo);
    return {
        highProduct + (crossOne >> 32) + (crossTwo >> 32) + (middle >> 32),
        (middle << 32) | static_cast<std::uint32_t>(lowProduct),
    };
}

bool productWithin(
    std::uint64_t lhs,
    std::uint64_t rhs,
    std::uint64_t limitLhs,
    std::uint64_t limitRhs,
    bool inclusive)
{
    const auto value = wideMultiply(lhs, rhs);
    const auto limit = wideMultiply(limitLhs, limitRhs);
    return inclusive ? value <= limit : value < limit;
}

std::int64_t cross2d(FixedPoint3 origin, FixedPoint3 lhs, FixedPoint3 rhs)
{
    const std::int64_t lhsX = lhs.x - origin.x;
    const std::int64_t lhsY = lhs.y - origin.y;
    const std::int64_t rhsX = rhs.x - origin.x;
    const std::int64_t rhsY = rhs.y - origin.y;
    return lhsX * rhsY - lhsY * rhsX;
}

std::uint64_t distanceSquared2d(FixedPoint3 lhs, FixedPoint3 rhs)
{
    const std::int64_t dx = lhs.x - rhs.x;
    const std::int64_t dy = lhs.y - rhs.y;
    return static_cast<std::uint64_t>(dx * dx + dy * dy);
}

std::uint64_t distanceSquared3d(FixedPoint3 lhs, FixedPoint3 rhs)
{
    const std::int64_t dx = lhs.x - rhs.x;
    const std::int64_t dy = lhs.y - rhs.y;
    const std::int64_t dz = lhs.z - rhs.z;
    return static_cast<std::uint64_t>(dx * dx + dy * dy + dz * dz);
}

int fixedTravelFrames(std::uint64_t distanceSquared, double speed)
{
    assert(speed > 0.0);
    if (distanceSquared == 0)
    {
        return 0;
    }

    const std::int64_t fixedSpeedValue = fixedGeometryValue(speed);
    assert(fixedSpeedValue > 0);
    const auto fixedSpeed = static_cast<std::uint64_t>(fixedSpeedValue);
    std::uint64_t upper = 1;
    while (!productWithin(distanceSquared, 1, upper * fixedSpeed, upper * fixedSpeed, true))
    {
        assert(upper <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()) / 2);
        upper *= 2;
    }

    std::uint64_t lower = 0;
    while (lower + 1 < upper)
    {
        const std::uint64_t middle = lower + (upper - lower) / 2;
        const std::uint64_t travel = middle * fixedSpeed;
        if (productWithin(distanceSquared, 1, travel, travel, true))
        {
            upper = middle;
        }
        else
        {
            lower = middle;
        }
    }
    assert(upper <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(upper);
}

bool rangesOverlap(std::int64_t lhsStart, std::int64_t lhsEnd, std::int64_t rhsStart, std::int64_t rhsEnd)
{
    if (lhsStart > lhsEnd)
    {
        std::swap(lhsStart, lhsEnd);
    }
    if (rhsStart > rhsEnd)
    {
        std::swap(rhsStart, rhsEnd);
    }
    return std::max(lhsStart, rhsStart) <= std::min(lhsEnd, rhsEnd);
}

bool fixedSegmentsIntersect(
    FixedPoint3 lhsStart,
    FixedPoint3 lhsEnd,
    FixedPoint3 rhsStart,
    FixedPoint3 rhsEnd)
{
    const std::int64_t lhsCrossStart = cross2d(lhsStart, lhsEnd, rhsStart);
    const std::int64_t lhsCrossEnd = cross2d(lhsStart, lhsEnd, rhsEnd);
    const std::int64_t rhsCrossStart = cross2d(rhsStart, rhsEnd, lhsStart);
    const std::int64_t rhsCrossEnd = cross2d(rhsStart, rhsEnd, lhsEnd);
    if (lhsCrossStart == 0 && lhsCrossEnd == 0 && rhsCrossStart == 0 && rhsCrossEnd == 0)
    {
        return rangesOverlap(lhsStart.x, lhsEnd.x, rhsStart.x, rhsEnd.x)
            && rangesOverlap(lhsStart.y, lhsEnd.y, rhsStart.y, rhsEnd.y);
    }
    return ((lhsCrossStart <= 0 && lhsCrossEnd >= 0)
            || (lhsCrossStart >= 0 && lhsCrossEnd <= 0))
        && ((rhsCrossStart <= 0 && rhsCrossEnd >= 0)
            || (rhsCrossStart >= 0 && rhsCrossEnd <= 0));
}

bool fixedPointSegmentWithinRadius(
    FixedPoint3 point,
    FixedPoint3 segmentStart,
    FixedPoint3 segmentEnd,
    std::uint64_t radiusSquared,
    bool inclusive)
{
    const std::int64_t dx = segmentEnd.x - segmentStart.x;
    const std::int64_t dy = segmentEnd.y - segmentStart.y;
    const std::uint64_t lengthSquared = static_cast<std::uint64_t>(dx * dx + dy * dy);
    if (lengthSquared == 0)
    {
        const auto distanceSquared = distanceSquared2d(point, segmentEnd);
        return inclusive ? distanceSquared <= radiusSquared : distanceSquared < radiusSquared;
    }

    const std::int64_t px = point.x - segmentStart.x;
    const std::int64_t py = point.y - segmentStart.y;
    const std::int64_t projection = px * dx + py * dy;
    if (projection <= 0)
    {
        const auto distanceSquared = distanceSquared2d(point, segmentStart);
        return inclusive ? distanceSquared <= radiusSquared : distanceSquared < radiusSquared;
    }
    if (static_cast<std::uint64_t>(projection) >= lengthSquared)
    {
        const auto distanceSquared = distanceSquared2d(point, segmentEnd);
        return inclusive ? distanceSquared <= radiusSquared : distanceSquared < radiusSquared;
    }

    const std::uint64_t cross = unsignedMagnitude(px * dy - py * dx);
    return productWithin(cross, cross, radiusSquared, lengthSquared, inclusive);
}

int roundedQuotient(std::int64_t numerator, std::int64_t denominator)
{
    assert(denominator > 0);
    const auto magnitude = unsignedMagnitude(numerator);
    const auto rounded = (magnitude + static_cast<std::uint64_t>(denominator) / 2)
        / static_cast<std::uint64_t>(denominator);
    assert(rounded <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()));
    return numerator >= 0 ? static_cast<int>(rounded) : -static_cast<int>(rounded);
}

}  // namespace

BattleSinCos deterministicSinCos(double radians)
{
    AngleUnits angle = normalizedAngleUnits(radians);
    if (angle == 0)
    {
        return {0.0, 1.0};
    }
    if (angle == QuarterTurn)
    {
        return {1.0, 0.0};
    }
    if (angle == -QuarterTurn)
    {
        return {-1.0, 0.0};
    }
    if (angle == -HalfTurn)
    {
        return {0.0, -1.0};
    }

    AngleUnits sign = 1;
    if (angle > QuarterTurn)
    {
        angle -= HalfTurn;
        sign = -1;
    }
    else if (angle < -QuarterTurn)
    {
        angle += HalfTurn;
        sign = -1;
    }

    AngleUnits x = CordicGain;
    AngleUnits y{};
    for (int index = 0; index < static_cast<int>(CordicAngles.size()); ++index)
    {
        const AngleUnits previousX = x;
        const AngleUnits previousY = y;
        if (angle >= 0)
        {
            x = previousX - floorDivideByPowerOfTwo(previousY, index);
            y = previousY + floorDivideByPowerOfTwo(previousX, index);
            angle -= CordicAngles[index];
        }
        else
        {
            x = previousX + floorDivideByPowerOfTwo(previousY, index);
            y = previousY - floorDivideByPowerOfTwo(previousX, index);
            angle += CordicAngles[index];
        }
    }
    return {fixedUnitValue(sign * y), fixedUnitValue(sign * x)};
}

double deterministicAtan2(double y, double x)
{
    assert(std::isfinite(x));
    assert(std::isfinite(y));
    if (x == 0.0)
    {
        if (y > 0.0)
        {
            return BattlePi / 2.0;
        }
        if (y < 0.0)
        {
            return -BattlePi / 2.0;
        }
        return 0.0;
    }
    if (y == 0.0)
    {
        return x < 0.0 ? BattlePi : 0.0;
    }

    const double magnitude = std::max(std::abs(x), std::abs(y));
    AngleUnits fixedX = roundedInteger(x / magnitude * FixedScale);
    AngleUnits fixedY = roundedInteger(y / magnitude * FixedScale);
    AngleUnits angle{};
    if (fixedX < 0)
    {
        angle = fixedY >= 0 ? HalfTurn : -HalfTurn;
        fixedX = -fixedX;
        fixedY = -fixedY;
    }

    for (int index = 0; index < static_cast<int>(CordicAngles.size()); ++index)
    {
        const AngleUnits previousX = fixedX;
        const AngleUnits previousY = fixedY;
        if (fixedY > 0)
        {
            fixedX = previousX + floorDivideByPowerOfTwo(previousY, index);
            fixedY = previousY - floorDivideByPowerOfTwo(previousX, index);
            angle += CordicAngles[index];
        }
        else if (fixedY < 0)
        {
            fixedX = previousX - floorDivideByPowerOfTwo(previousY, index);
            fixedY = previousY + floorDivideByPowerOfTwo(previousX, index);
            angle -= CordicAngles[index];
        }
        else
        {
            break;
        }
    }
    return radiansFromAngleUnits(angle);
}

double deterministicAngleDelta(double lhs, double rhs)
{
    const AngleUnits delta = normalizedAngleUnits(lhs - rhs);
    return radiansFromAngleUnits(delta >= 0 ? delta : -delta);
}

Pointf rotateBattlePoint(Pointf point, double radians)
{
    const auto [sine, cosine] = deterministicSinCos(radians);
    const double x = point.x;
    const double y = point.y;
    point.x = static_cast<float>(x * cosine - y * sine);
    point.y = static_cast<float>(x * sine + y * cosine);
    return point;
}

Pointf battleDirection(double radians)
{
    const auto [sine, cosine] = deterministicSinCos(radians);
    return {static_cast<float>(cosine), static_cast<float>(sine), 0.0f};
}

Point battleIsometricGridPosition(Pointf position, int coordCount, double tileWidth)
{
    assert(coordCount > 0);
    assert(tileWidth > 0.0);
    const auto fixedPosition = fixedPoint(position);
    const std::int64_t fixedTileWidth = fixedGeometryValue(tileWidth);
    assert(fixedTileWidth > 0);
    const std::int64_t shiftedX = fixedPosition.x - static_cast<std::int64_t>(coordCount) * fixedTileWidth;
    const std::int64_t denominator = fixedTileWidth * 2;
    return {
        roundedQuotient(shiftedX + fixedPosition.y, denominator),
        roundedQuotient(-shiftedX + fixedPosition.y, denominator),
    };
}

Point battleCartesianGridPosition(Pointf position, Pointf origin, double tileWidth)
{
    assert(tileWidth > 0.0);
    const auto fixedPosition = fixedPoint(position);
    const auto fixedOrigin = fixedPoint(origin);
    const std::int64_t fixedTileWidth = fixedGeometryValue(tileWidth);
    assert(fixedTileWidth > 0);
    return {
        roundedQuotient(fixedPosition.x - fixedOrigin.x, fixedTileWidth),
        roundedQuotient(fixedPosition.y - fixedOrigin.y, fixedTileWidth),
    };
}

int battleTravelFrames2d(Pointf start, Pointf end, double speed)
{
    const auto fixedStart = fixedPoint(start);
    const auto fixedEnd = fixedPoint(end);
    return fixedTravelFrames(distanceSquared2d(fixedStart, fixedEnd), speed);
}

int battleTravelFrames3d(Pointf start, Pointf end, double speed)
{
    return fixedTravelFrames(distanceSquared3d(fixedPoint(start), fixedPoint(end)), speed);
}

std::uint64_t battleDistanceSquared2d(Pointf lhs, Pointf rhs)
{
    return distanceSquared2d(fixedPoint(lhs), fixedPoint(rhs));
}

std::uint64_t battleDistanceSquared3d(Pointf lhs, Pointf rhs)
{
    return distanceSquared3d(fixedPoint(lhs), fixedPoint(rhs));
}

BattleFacingArc classifyBattleFacing(Pointf attackVector, Pointf defenderFacing)
{
    const auto attack = fixedPoint(attackVector);
    const auto facing = fixedPoint(defenderFacing);
    const std::int64_t dot = attack.x * facing.x + attack.y * facing.y;
    const std::uint64_t attackNormSquared = static_cast<std::uint64_t>(
        attack.x * attack.x + attack.y * attack.y + attack.z * attack.z);
    const std::uint64_t facingNormSquared = static_cast<std::uint64_t>(
        facing.x * facing.x + facing.y * facing.y + facing.z * facing.z);
    assert(facingNormSquared > 0);
    if (attackNormSquared == 0)
    {
        return BattleFacingArc::Front;
    }

    const std::uint64_t absoluteDot = unsignedMagnitude(dot);
    assert(absoluteDot <= std::numeric_limits<std::uint64_t>::max() / 2);
    const auto diagonalComparison = wideMultiply(absoluteDot, absoluteDot * 2);
    const auto normProduct = wideMultiply(attackNormSquared, facingNormSquared);
    if (dot < 0 && diagonalComparison >= normProduct)
    {
        return BattleFacingArc::Back;
    }
    if (diagonalComparison <= normProduct)
    {
        return BattleFacingArc::Side;
    }
    return BattleFacingArc::Front;
}

bool battlePointSegmentWithinRadius(
    Pointf point,
    Pointf segmentStart,
    Pointf segmentEnd,
    double radius,
    bool inclusive)
{
    assert(radius >= 0.0);
    const auto fixedRadius = unsignedMagnitude(fixedGeometryValue(radius));
    return fixedPointSegmentWithinRadius(
        fixedPoint(point),
        fixedPoint(segmentStart),
        fixedPoint(segmentEnd),
        fixedRadius * fixedRadius,
        inclusive);
}

bool battleSegmentsWithinRadius(
    Pointf lhsStart,
    Pointf lhsEnd,
    Pointf rhsStart,
    Pointf rhsEnd,
    double radius,
    bool inclusive)
{
    assert(radius >= 0.0);
    const auto fixedLhsStart = fixedPoint(lhsStart);
    const auto fixedLhsEnd = fixedPoint(lhsEnd);
    const auto fixedRhsStart = fixedPoint(rhsStart);
    const auto fixedRhsEnd = fixedPoint(rhsEnd);
    if (fixedSegmentsIntersect(fixedLhsStart, fixedLhsEnd, fixedRhsStart, fixedRhsEnd))
    {
        return true;
    }

    const auto fixedRadius = unsignedMagnitude(fixedGeometryValue(radius));
    const std::uint64_t radiusSquared = fixedRadius * fixedRadius;
    return fixedPointSegmentWithinRadius(fixedLhsStart, fixedRhsStart, fixedRhsEnd, radiusSquared, inclusive)
        || fixedPointSegmentWithinRadius(fixedLhsEnd, fixedRhsStart, fixedRhsEnd, radiusSquared, inclusive)
        || fixedPointSegmentWithinRadius(fixedRhsStart, fixedLhsStart, fixedLhsEnd, radiusSquared, inclusive)
        || fixedPointSegmentWithinRadius(fixedRhsEnd, fixedLhsStart, fixedLhsEnd, radiusSquared, inclusive);
}

}  // namespace KysChess::Battle
