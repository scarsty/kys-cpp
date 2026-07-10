#include "PaperCameraControlMath.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using Catch::Approx;

TEST_CASE("PaperCameraControlMath shares the keyboard and touch ground basis", "[paper_camera_math]")
{
    const auto basis = paperCameraGroundBasis(0.0f);
    CHECK(basis.forward.x == Approx(-1.0f));
    CHECK(basis.forward.y == Approx(0.0f));
    CHECK(basis.screenRight.x == Approx(0.0f));
    CHECK(basis.screenRight.y == Approx(1.0f));

    CHECK(paperKeyboardPanVector(0.0f, true, false, false, false).x == Approx(-1.0f));
    CHECK(paperKeyboardPanVector(0.0f, false, false, true, false).y == Approx(1.0f));
}

TEST_CASE("PaperCameraControlMath maps one-finger drag in camera space", "[paper_camera_math]")
{
    const auto delta = paperTouchPanDelta(0.0f, 100.0f, 100.0f, 60.0f, 1000.0f, 10.0f, 20.0f);
    const float worldPerPixel = 2.0f * std::hypot(100.0f, 100.0f)
        * std::tan(std::numbers::pi_v<float> / 6.0f) / 1000.0f;
    CHECK(delta.x == Approx(-20.0f * worldPerPixel));
    CHECK(delta.y == Approx(-10.0f * worldPerPixel));
}

TEST_CASE("PaperCameraControlMath applies multiplicative pinch and two-finger orientation", "[paper_camera_math]")
{
    CHECK(paperTouchPinchDistance(600.0f, 100.0f, 200.0f) == Approx(300.0f));
    CHECK(paperTouchRotationDelta(100.0f, 1000.0f) == Approx(std::numbers::pi_v<float> * 0.1f));
    CHECK(paperTouchHeightDelta(-100.0f, 1000.0f, 80.0f, 520.0f) == Approx(44.0f));
}
