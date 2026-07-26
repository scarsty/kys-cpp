#include "battle/BattleMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

using namespace KysChess::Battle;

TEST_CASE("deterministic battle sin and cos preserve cardinal directions", "[battle][math][determinism]")
{
    CHECK(deterministicSinCos(0.0).sine == 0.0);
    CHECK(deterministicSinCos(0.0).cosine == 1.0);
    CHECK(deterministicSinCos(BattlePi / 2.0).sine == 1.0);
    CHECK(deterministicSinCos(BattlePi / 2.0).cosine == 0.0);
    CHECK(deterministicSinCos(-BattlePi / 2.0).sine == -1.0);
    CHECK(deterministicSinCos(-BattlePi / 2.0).cosine == 0.0);
    CHECK(deterministicSinCos(BattlePi).sine == 0.0);
    CHECK(deterministicSinCos(BattlePi).cosine == -1.0);
}

TEST_CASE("deterministic battle trig remains close to mathematical references", "[battle][math][determinism]")
{
    constexpr std::array<double, 8> angles{
        -7.25,
        -BattlePi * 0.75,
        -0.125,
        BattlePi / 6.0,
        BattlePi / 4.0,
        1.25,
        BattlePi * 0.875,
        9.5,
    };
    for (const double angle : angles)
    {
        const auto [sine, cosine] = deterministicSinCos(angle);
        CAPTURE(angle);
        CHECK(sine == Catch::Approx(std::sin(angle)).margin(3.0e-13));
        CHECK(cosine == Catch::Approx(std::cos(angle)).margin(3.0e-13));
    }

    CHECK(deterministicAtan2(1.0, 2.0) == Catch::Approx(std::atan2(1.0, 2.0)).margin(3.0e-13));
    CHECK(deterministicAtan2(2.0, -1.0) == Catch::Approx(std::atan2(2.0, -1.0)).margin(3.0e-13));
    CHECK(deterministicAtan2(-2.5, -3.75) == Catch::Approx(std::atan2(-2.5, -3.75)).margin(3.0e-13));
}

TEST_CASE("deterministic battle trig has stable representative bit patterns", "[battle][math][determinism]")
{
    const auto [sine, cosine] = deterministicSinCos(1.25);
    CHECK(std::bit_cast<std::uint64_t>(sine) == 0x3fee5e14fe1142e0ULL);
    CHECK(std::bit_cast<std::uint64_t>(cosine) == 0x3fd42e3dd88bd480ULL);
    CHECK(std::bit_cast<std::uint64_t>(deterministicAtan2(-2.5, -3.75)) == 0xc0046dc09ec293eeULL);
}

TEST_CASE("deterministic battle angle helpers handle wrapping and rotation", "[battle][math][determinism]")
{
    CHECK(deterministicAtan2(1.0, 0.0) == BattlePi / 2.0);
    CHECK(deterministicAtan2(-1.0, 0.0) == -BattlePi / 2.0);
    CHECK(deterministicAtan2(0.0, -1.0) == BattlePi);
    CHECK(deterministicAngleDelta(BattlePi - 0.1, -BattlePi + 0.1)
          == Catch::Approx(0.2).margin(3.0e-13));

    const auto rotated = rotateBattlePoint({3.0f, 4.0f, 5.0f}, BattlePi / 2.0);
    CHECK(rotated.x == -4.0f);
    CHECK(rotated.y == 3.0f);
    CHECK(rotated.z == 5.0f);

    const auto direction = battleDirection(-BattlePi / 2.0);
    CHECK(direction.x == 0.0f);
    CHECK(direction.y == -1.0f);
    CHECK(direction.z == 0.0f);
}

TEST_CASE("fixed battle geometry keeps contact boundaries exact", "[battle][math][determinism]")
{
    CHECK(battlePointSegmentWithinRadius(
        {5.0f, 2.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f},
        2.0));
    CHECK_FALSE(battlePointSegmentWithinRadius(
        {5.0f, 2.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {10.0f, 0.0f, 0.0f},
        2.0,
        false));
    CHECK(battleSegmentsWithinRadius(
        {0.0f, 0.0f, 0.0f},
        {10.0f, 10.0f, 0.0f},
        {0.0f, 10.0f, 0.0f},
        {10.0f, 0.0f, 0.0f},
        0.0));
}

TEST_CASE("fixed battle facing assigns exact diagonal boundaries consistently", "[battle][math][determinism]")
{
    const Pointf facing{1.0f, 0.0f, 0.0f};
    CHECK(classifyBattleFacing({1.0f, 0.0f, 0.0f}, facing) == BattleFacingArc::Front);
    CHECK(classifyBattleFacing({1.0f, 1.0f, 0.0f}, facing) == BattleFacingArc::Side);
    CHECK(classifyBattleFacing({-1.0f, 1.0f, 0.0f}, facing) == BattleFacingArc::Back);
    CHECK(classifyBattleFacing({}, facing) == BattleFacingArc::Front);
    CHECK(classifyBattleFacing({0.0001f, 0.0f, 0.0f}, facing) == BattleFacingArc::Front);
}

TEST_CASE("fixed projectile travel frames use exact ceiling", "[battle][math][determinism]")
{
    CHECK(battleTravelFrames2d({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 3.0) == 4);
    CHECK(battleTravelFrames2d({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, 2.0) == 5);
    CHECK(battleTravelFrames2d({4.0f, 7.0f, 0.0f}, {4.0f, 7.0f, 0.0f}, 2.0) == 0);
}
