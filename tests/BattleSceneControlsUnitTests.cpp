#include "BattleSceneControls.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneControls keeps a stable two-row top-right layout", "[battle_controls]")
{
    const auto running = makeBattleControlLayout(1280, false);
    CHECK(running.hitTest({1236, 20}) == BattleControlId::Pause);
    CHECK(running.hitTest({1164, 20}) == BattleControlId::None);
    CHECK(running.hitTest({1092, 20}) == BattleControlId::Speed);
    CHECK(running.hitTest({1092, 92}) == BattleControlId::PaperView);
    CHECK(running.hitTest({1164, 92}) == BattleControlId::CameraMode);
    CHECK(running.hitTest({1236, 92}) == BattleControlId::Surrender);
    CHECK_FALSE(running.rect(BattleControlId::Log).has_value());

    const auto paused = makeBattleControlLayout(1280, true);
    CHECK(paused.hitTest({1164, 20}) == BattleControlId::Log);
    for (const auto id : {BattleControlId::Pause, BattleControlId::Speed,
                         BattleControlId::PaperView, BattleControlId::CameraMode, BattleControlId::Surrender})
    {
        REQUIRE(running.rect(id));
        REQUIRE(paused.rect(id));
        CHECK(running.rect(id)->x == paused.rect(id)->x);
        CHECK(running.rect(id)->y == paused.rect(id)->y);
    }
}

TEST_CASE("BattleControlCapture activates only a matching down and up", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280, false);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 9, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1092, 20}, layout) == BattleControlId::None);

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::Pause);
}

TEST_CASE("BattleControlCapture never turns a battlefield drag into a control activation", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280, false);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 3, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {500, 400}, layout) == BattleControlId::None);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::None);
}

TEST_CASE("surrender requires its own matching press and release", "[battle_controls][surrender]")
{
    const auto layout = makeBattleControlLayout(1280, false);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Mouse, 0, SDL_BUTTON_LEFT};
    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1236, 92}, layout) == BattleControlId::None);
    CHECK(capture.onDown(pointer, {1236, 92}, layout) == BattleControlId::Surrender);
    capture.cancel(pointer);
    CHECK(capture.onUp(pointer, {1236, 92}, layout) == BattleControlId::None);
    CHECK(capture.onDown(pointer, {1236, 92}, layout) == BattleControlId::Surrender);
    CHECK(capture.onUp(pointer, {1236, 92}, layout) == BattleControlId::Surrender);
}
