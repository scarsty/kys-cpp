#include "BattleSceneControls.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("BattleSceneControls keeps a stable two-row top-right layout", "[battle_controls]")
{
    const auto running = makeBattleControlLayout(1280);
    CHECK(running.hitTest({1236, 20}) == BattleControlId::Pause);
    CHECK(running.hitTest({1164, 20}) == BattleControlId::Log);
    CHECK(running.hitTest({1092, 20}) == BattleControlId::Speed);
    CHECK(running.hitTest({1092, 92}) == BattleControlId::PaperView);
    CHECK(running.hitTest({1164, 92}) == BattleControlId::CameraMode);
    CHECK(running.hitTest({1236, 92}) == BattleControlId::Surrender);
    CHECK(running.rect(BattleControlId::Log).has_value());


}

TEST_CASE("BattleControlCapture activates only a matching down and up", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 9, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1092, 20}, layout) == BattleControlId::None);

    CHECK(capture.onDown(pointer, {1236, 20}, layout) == BattleControlId::Pause);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::Pause);
}

TEST_CASE("BattleControlCapture never turns a battlefield drag into a control activation", "[battle_controls]")
{
    const auto layout = makeBattleControlLayout(1280);
    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 3, SDL_BUTTON_LEFT};

    CHECK(capture.onDown(pointer, {500, 400}, layout) == BattleControlId::None);
    CHECK(capture.onUp(pointer, {1236, 20}, layout) == BattleControlId::None);
}

TEST_CASE("surrender requires its own matching press and release", "[battle_controls][surrender]")
{
    const auto layout = makeBattleControlLayout(1280);
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

TEST_CASE("Battle inspector grows upward while preserving bottom and summary positions", "[battle_controls][battle_inspector]")
{
    const auto empty = makeBattleInspectorLayout({}, 0);
    const auto small = makeBattleInspectorLayout(std::array{78}, 0);
    const auto larger = makeBattleInspectorLayout(std::array{78, 98, 118}, 0);
    CHECK(empty.effects.y > small.effects.y);
    CHECK(small.effects.y > larger.effects.y);
    for (const auto& layout : {empty, small, larger})
    {
        CHECK(layout.effects.y + layout.effects.h == 704);
        CHECK(layout.effects.x + layout.effects.w + 12 == BattleInspectorLayout::summary.x);
        REQUIRE(layout.pages.size() == 1);
    }
}

TEST_CASE("Battle inspector pagination keeps complete variable-height cards and clamps expired pages", "[battle_controls][battle_inspector]")
{
    const std::array heights{78, 98, 118, 158, 78, 98, 118, 78, 98, 138, 118, 98, 78};
    const auto layout = makeBattleInspectorLayout(heights, 99);
    REQUIRE(layout.pages.size() > 1);
    CHECK(layout.page == int(layout.pages.size()) - 1);
    CHECK(layout.effects.y == 16);
    CHECK(layout.effects.y + layout.effects.h == 704);
    int nextRow = 0;
    for (const auto& page : layout.pages)
    {
        CHECK(page.firstRow == nextRow);
        REQUIRE(page.rowCount > 0);
        CHECK(page.height <= BattleInspectorLayout::effectCapacity);
        nextRow += page.rowCount;
    }
    CHECK(nextRow == heights.size());
    CHECK(makeBattleInspectorLayout(std::array{78}, layout.page).page == 0);
    CHECK(makeBattleInspectorLayout({}, layout.page).page == 0);
    const auto exact = makeBattleInspectorLayout(std::array{BattleInspectorLayout::effectCapacity}, 0);
    CHECK(exact.pages.size() == 1);
    CHECK(exact.pages.front().rowCount == 1);
}

TEST_CASE("Battle inspector panels capture input without swallowing their controls or the board", "[battle_controls][battle_inspector]")
{
    auto controls = makeBattleControlLayout(1280);
    const auto inspector = makeBattleInspectorLayout(std::array{300, 300, 300}, 0);
    addBattleInspectorControls(controls, inspector);
    CHECK(controls.hitTest({50, 200}) == BattleControlId::InspectorEffects);
    CHECK(controls.hitTest({700, 600}) == BattleControlId::InspectorBody);
    CHECK(controls.hitTest({1236, 548}) == BattleControlId::None);
    CHECK(controls.hitTest({124, 674}) == BattleControlId::InspectorPrevious);
    CHECK(controls.hitTest({260, 674}) == BattleControlId::InspectorNext);
    CHECK(controls.hitTest({500, 400}) == BattleControlId::None);
    CHECK(controls.hitTest({296, 600}) == BattleControlId::None);
    CHECK(controls.hitTest({1236, 20}) == BattleControlId::Pause);

    BattleControlCapture capture;
    const PointerIdentity pointer{PointerSource::Touch, 7, SDL_BUTTON_LEFT};
    CHECK(capture.onDown(pointer, {50, 200}, controls) == BattleControlId::InspectorEffects);
    CHECK(capture.onUp(pointer, {1236, 20}, controls) == BattleControlId::None);
}
