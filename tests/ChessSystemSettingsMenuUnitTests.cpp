#include "ChessSystemSettingsMenu.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

TEST_CASE("ChessSystemSettingsMenu_SliderValueFromPointerClampsToTrack", "[chess][settings]")
{
    CHECK(settingsSliderValueFromPointer(80, 100, 200) == 0);
    CHECK(settingsSliderValueFromPointer(100, 100, 200) == 0);
    CHECK(settingsSliderValueFromPointer(150, 100, 200) == 25);
    CHECK(settingsSliderValueFromPointer(200, 100, 200) == 50);
    CHECK(settingsSliderValueFromPointer(300, 100, 200) == 100);
    CHECK(settingsSliderValueFromPointer(340, 100, 200) == 100);
}

TEST_CASE("ChessSystemSettingsMenu_AdjustSliderValueClampsToVolumeRange", "[chess][settings]")
{
    CHECK(adjustSettingsSliderValue(31, -5) == 26);
    CHECK(adjustSettingsSliderValue(98, 5) == 100);
    CHECK(adjustSettingsSliderValue(2, -5) == 0);
}

TEST_CASE("ChessSystemSettingsMenu_ToggleValueColorUsesSemanticOnOffColors", "[chess][settings]")
{
    const Color on = settingsToggleValueColor(true);
    CHECK(on.r == 0);
    CHECK(on.g == 255);
    CHECK(on.b == 100);
    CHECK(on.a == 255);

    const Color off = settingsToggleValueColor(false);
    CHECK(off.r == 255);
    CHECK(off.g == 80);
    CHECK(off.b == 80);
    CHECK(off.a == 255);
}

TEST_CASE("ChessSystemSettingsMenu_FooterActionRowIsPinnedToPanelBottom", "[chess][settings]")
{
    CHECK(settingsFooterActionY(100, 610, 44, 48) == 618);
    CHECK(settingsFooterActionY(30, 660, 44, 48) == 598);
}
