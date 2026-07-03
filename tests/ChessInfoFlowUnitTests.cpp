#include "ChessInfoFlow.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

int testDisplayWidth(const std::string& text)
{
    int width = 0;
    for (size_t index = 0; index < text.size();)
    {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80)
        {
            ++width;
            ++index;
            continue;
        }

        const int charLength = lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4);
        width += charLength >= 3 ? 2 : 1;
        index += charLength;
    }
    return width;
}

}    // namespace

TEST_CASE("combo catalog labels align mixed-width names and progress counts", "[chess][combo]")
{
    const auto labels = buildAlignedComboCatalogLabels(
        {
            {"拳師", "3/5 ✓"},
            {"江南七怪", "2+1/5 ✓"},
            {"獨行", "獨/1 ✓"},
        },
        testDisplayWidth);

    REQUIRE(labels.size() == 3);
    const int width = testDisplayWidth(labels.front());
    CHECK(testDisplayWidth(labels[1]) == width);
    CHECK(testDisplayWidth(labels[2]) == width);
}
