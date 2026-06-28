#include "Button.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("button selection overlay does not replace semantic outline", "[ui][button]")
{
    ButtonVisualLayers layers;

    layers.setCustomOutline({255, 215, 0, 255});
    layers.setSelectionOverlay({245, 245, 230, 235}, {255, 255, 255, 32}, 2);

    CHECK(layers.hasCustomOutline());
    CHECK(layers.hasSelectionOverlay());
    CHECK(layers.keepsTextColorDuringSelection());

    layers.clearSelectionOverlay();

    CHECK(layers.hasCustomOutline());
    CHECK_FALSE(layers.hasSelectionOverlay());
    CHECK_FALSE(layers.keepsTextColorDuringSelection());
}
