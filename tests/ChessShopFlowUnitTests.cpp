#include "ChessShopFlow.h"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>
#include <utility>

using namespace KysChess;

TEST_CASE("buy exp reports whether a purchase happened", "[chess][shop]")
{
    CHECK(std::is_same_v<decltype(std::declval<ChessShopFlow&>().buyExp()), bool>);
}
