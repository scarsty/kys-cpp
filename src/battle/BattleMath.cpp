#include "BattleMath.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

namespace KysChess::Battle
{

namespace
{

using AngleUnits = std::int64_t;

constexpr int AngleBits = 48;
constexpr AngleUnits FullTurn = AngleUnits{1} << AngleBits;
constexpr AngleUnits HalfTurn = FullTurn / 2;
constexpr AngleUnits QuarterTurn = FullTurn / 4;
constexpr AngleUnits FixedScale = AngleUnits{1} << 48;
constexpr AngleUnits CordicGain = 170926505739102;
constexpr double RadiansToAngleUnits = 0x1.45f306dc9c883p+45;
constexpr double AngleUnitsToRadians = 0x1.921fb54442d18p-46;

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

}  // namespace KysChess::Battle
