#include "ChessShopWeightDisplay.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

TEST_CASE("shop preview shows level 10 weights while player is level 9", "[chess][shop]")
{
    BalanceConfig config;

    CHECK(buildNextShopWeightString(config, 8) == "1費19% 2費25% 3費25% 4費25% 5費6%");
}
