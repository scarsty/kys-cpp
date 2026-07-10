#include "BattleScenePaperCameraTouch.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
TouchSample sample(
    TouchPhase phase,
    int finger,
    float x,
    float y,
    bool inside = true,
    TouchCancelScope cancelScope = TouchCancelScope::Contact)
{
    TouchSample value;
    value.phase = phase;
    value.key = {SDL_TouchID{1}, static_cast<SDL_FingerID>(finger)};
    value.uiPosition = {x, y};
    value.insidePresent = inside;
    value.cancelScope = cancelScope;
    return value;
}
}

TEST_CASE("Paper camera touch establishes a baseline and crosses the pan threshold once", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    CHECK(touch.process(sample(TouchPhase::Down, 1, 400, 300), layout).empty());
    CHECK(touch.process(sample(TouchPhase::Motion, 1, 406, 306), layout).empty());

    const auto activated = touch.process(sample(TouchPhase::Motion, 1, 412, 306), layout);
    REQUIRE(activated.size() == 2);
    CHECK(activated[0].type == PaperCameraTouchActionType::PanActivated);
    CHECK(activated[1].type == PaperCameraTouchActionType::Pan);
    CHECK(activated[1].delta.x == 12.0f);
    CHECK(activated[1].delta.y == 6.0f);

    const auto continued = touch.process(sample(TouchPhase::Motion, 1, 415, 310), layout);
    REQUIRE(continued.size() == 1);
    CHECK(continued[0].delta.x == 3.0f);
    CHECK(continued[0].delta.y == 4.0f);
}

TEST_CASE("Paper camera touch rebaselines one-to-two-to-one transitions", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    touch.process(sample(TouchPhase::Down, 1, 300, 300), layout);
    CHECK(touch.process(sample(TouchPhase::Down, 2, 500, 300), layout).empty());

    const auto pair = touch.process(sample(TouchPhase::Motion, 1, 280, 280), layout);
    REQUIRE(pair.size() == 1);
    CHECK(pair[0].type == PaperCameraTouchActionType::Pair);

    CHECK(touch.process(sample(TouchPhase::Up, 2, 500, 300), layout).empty());
    CHECK(touch.process(sample(TouchPhase::Motion, 1, 285, 285), layout).empty());
}

TEST_CASE("Paper camera touch ignores outside starts and requires matching control release", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    touch.process(sample(TouchPhase::Down, 1, 500, 300, false), layout);
    CHECK(touch.process(sample(TouchPhase::Motion, 1, 800, 600), layout).empty());

    touch.process(sample(TouchPhase::Down, 2, 1236, 20), layout);
    CHECK(touch.process(sample(TouchPhase::Up, 2, 1092, 20), layout).empty());

    touch.process(sample(TouchPhase::Down, 3, 1236, 20), layout);
    const auto activated = touch.process(sample(TouchPhase::Up, 3, 1236, 20), layout);
    REQUIRE(activated.size() == 1);
    CHECK(activated[0].type == PaperCameraTouchActionType::ControlActivated);
    CHECK(activated[0].control == BattleControlId::Pause);
}

TEST_CASE("Paper camera touch suspends for more than two fingers and stream cancel clears all state", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);
    touch.process(sample(TouchPhase::Down, 1, 300, 300), layout);
    touch.process(sample(TouchPhase::Down, 2, 500, 300), layout);
    touch.process(sample(TouchPhase::Down, 3, 700, 300), layout);

    CHECK(touch.process(sample(TouchPhase::Motion, 1, 250, 250), layout).empty());
    CHECK(touch.process(sample(TouchPhase::Canceled, 2, 500, 300, true, TouchCancelScope::All), layout).empty());
    CHECK(touch.activeContactCount() == 0);

    touch.process(sample(TouchPhase::Down, 4, 400, 400), layout);
    CHECK(touch.process(sample(TouchPhase::Motion, 4, 405, 405), layout).empty());
}

TEST_CASE("Paper camera touch retires only the canceled browser contact", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    touch.process(sample(TouchPhase::Down, 1, 300, 300), layout);
    touch.process(sample(TouchPhase::Down, 2, 500, 300), layout);
    CHECK(touch.process(sample(TouchPhase::Canceled, 2, 500, 300), layout).empty());
    CHECK(touch.activeContactCount() == 1);

    CHECK(touch.process(sample(TouchPhase::Motion, 1, 305, 300), layout).empty());
    const auto resumed = touch.process(sample(TouchPhase::Motion, 1, 312, 300), layout);
    REQUIRE(resumed.size() == 2);
    CHECK(resumed[0].type == PaperCameraTouchActionType::PanActivated);
    CHECK(resumed[1].type == PaperCameraTouchActionType::Pan);
    CHECK(resumed[1].delta.x == 12.0f);
}

TEST_CASE("Paper camera touch arms only the first control until all control contacts end", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    touch.process(sample(TouchPhase::Down, 1, 1236, 20), layout);
    touch.process(sample(TouchPhase::Down, 2, 1092, 20), layout);

    const auto first = touch.process(sample(TouchPhase::Up, 1, 1236, 20), layout);
    REQUIRE(first.size() == 1);
    CHECK(first[0].control == BattleControlId::Pause);
    CHECK(touch.process(sample(TouchPhase::Up, 2, 1092, 20), layout).empty());
}

TEST_CASE("Paper camera touch rebaselines camera contacts when the final control contact ends", "[paper_camera_touch]")
{
    BattleScenePaperCameraTouch touch;
    const auto layout = makeBattleControlLayout(1280, false, true);

    touch.process(sample(TouchPhase::Down, 1, 400, 300), layout);
    touch.process(sample(TouchPhase::Down, 2, 1236, 20), layout);
    CHECK(touch.process(sample(TouchPhase::Motion, 1, 450, 300), layout).empty());
    touch.process(sample(TouchPhase::Up, 2, 1236, 20), layout);
    CHECK(touch.process(sample(TouchPhase::Motion, 1, 455, 300), layout).empty());
}
