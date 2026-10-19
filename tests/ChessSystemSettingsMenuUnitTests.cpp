#include "ChessSystemSettingsMenu.h"

#include <catch2/catch_test_macros.hpp>
#include <glaze/json.hpp>

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
    CHECK(on.g > on.r);
    CHECK(on.g > on.b);
    CHECK(on.a == 255);

    const Color off = settingsToggleValueColor(false);
    CHECK(off.r > off.g);
    CHECK(off.r > off.b);
    CHECK(off.a == 255);
}

TEST_CASE("ChessSystemSettingsMenu_FooterActionRowIsPinnedToPanelBottom", "[chess][settings]")
{
    CHECK(settingsFooterActionY(100, 610, 44, 48) == 618);
    CHECK(settingsFooterActionY(30, 660, 44, 48) == 598);
}

TEST_CASE("ChessSystemSettingsMenu_FullscreenRowFitsAboveFooter", "[chess][settings]")
{
    for (const int panelH : {610, 660})
    {
        constexpr int settingRows = 9;
        constexpr int rowGap = 8;
        constexpr int bottomPadding = 48;
        constexpr int dividerGap = 24;
        const int rowH = settingsRowHeight(panelH, settingRows, rowGap, bottomPadding, dividerGap);
        const int finalRowBottom = 82 + (settingRows - 1) * (rowH + rowGap) + rowH;
        const int footerY = settingsFooterActionY(0, panelH, rowH, bottomPadding);

        CHECK(rowH >= 36);
        CHECK(rowH <= 44);
        CHECK(finalRowBottom <= footerY - dividerGap);
    }
}

TEST_CASE("SystemSettings_FullscreenPreferenceDefaultsOffAndRoundTrips", "[chess][settings]")
{
    SystemSettingsData settings;
    CHECK_FALSE(settings.borderlessFullscreen);

    for (const bool fullscreen : {true, false})
    {
        settings.borderlessFullscreen = fullscreen;
        settings.musicVolume = 37;
        std::string payload;
        REQUIRE_FALSE(glz::write_json(settings, payload));

        SystemSettingsData restored;
        REQUIRE_FALSE(glz::read_json(restored, payload));
        CHECK(restored.borderlessFullscreen == fullscreen);
        CHECK(restored.musicVolume == 37);
    }
}

TEST_CASE("preparation guide and movement preferences persist without changing other settings", "[chess][settings]")
{
    SystemSettingsData settings;
    CHECK_FALSE(settings.preparationGuideSeen);
    CHECK_FALSE(settings.preparationSingleClick);
    settings.preparationGuideSeen = true;
    settings.preparationSingleClick = true;
    settings.musicVolume = 37;
    std::string payload;
    REQUIRE_FALSE(glz::write_json(settings, payload));
    SystemSettingsData restored;
    REQUIRE_FALSE(glz::read_json(restored, payload));
    CHECK(restored.preparationGuideSeen);
    CHECK(restored.preparationSingleClick);
    CHECK(restored.musicVolume == 37);
}

TEST_CASE("FullscreenHotkey_ConsumesBothEdgesAndIgnoresModifiedKeys", "[ui][fullscreen]")
{
    EngineEvent event{};
    event.type = EVENT_KEY_DOWN;
    event.key.key = SDLK_F10;
    CHECK(Engine::isFullscreenHotkey(event) == Engine::supportsDesktopFullscreen());

    event.key.repeat = true;
    CHECK(Engine::isFullscreenHotkey(event) == Engine::supportsDesktopFullscreen());
    event.type = EVENT_KEY_UP;
    CHECK(Engine::isFullscreenHotkey(event) == Engine::supportsDesktopFullscreen());

    for (const auto mod : {SDL_KMOD_SHIFT, SDL_KMOD_CTRL, SDL_KMOD_ALT, SDL_KMOD_GUI})
    {
        event.key.mod = mod;
        CHECK_FALSE(Engine::isFullscreenHotkey(event));
    }
    event.key.mod = SDL_KMOD_CAPS | SDL_KMOD_NUM;
    CHECK(Engine::isFullscreenHotkey(event) == Engine::supportsDesktopFullscreen());
    event.key.mod = 0;
    event.key.key = SDLK_F1;
    CHECK_FALSE(Engine::isFullscreenHotkey(event));
    event.key.key = SDLK_F10;
    event.type = EVENT_FIRST;
    CHECK_FALSE(Engine::isFullscreenHotkey(event));
}

TEST_CASE("Formation defaults on and an explicit saved preference is retained", "[chess][settings]")
{
    SystemSettingsData settings;
    CHECK(settings.positionSwapEnabled);
    CHECK_FALSE(glz::read_json(settings, R"({"positionSwapEnabled":false})"));
    CHECK_FALSE(settings.positionSwapEnabled);
}
