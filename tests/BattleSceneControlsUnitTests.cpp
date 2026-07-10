#include "BattleSceneControls.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("BattleSceneControls lays out only visible top-right controls", "[battle_controls]")
{
    const auto running = makeBattleControlLayout(1280, false, true);
    CHECK(running.hitTest({1236, 20}) == BattleControlId::Pause);
    CHECK(running.hitTest({1164, 20}) == BattleControlId::CameraMode);
    CHECK(running.hitTest({1092, 20}) == BattleControlId::PaperView);
    CHECK(running.hitTest({1020, 20}) == BattleControlId::Speed);
    CHECK_FALSE(running.rect(BattleControlId::Log).has_value());

    const auto paused = makeBattleControlLayout(1280, true, true);
    CHECK(paused.hitTest({1164, 20}) == BattleControlId::Log);
    CHECK(paused.hitTest({1092, 20}) == BattleControlId::CameraMode);
}

TEST_CASE("BattleControlCapture activates only a matching down and up", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280, false, true);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 9, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1092, 20}, layout) == BattleControlId::None);

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::Pause);
}

TEST_CASE("BattleControlCapture never turns a battlefield drag into a control activation", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280, false, true);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 3, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {500, 400}, layout) == BattleControlId::None);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::None);
}
