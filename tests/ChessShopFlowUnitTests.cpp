#include "ChessEquipment.h"
#include "ChessShopFlow.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <type_traits>
#include <utility>

using namespace KysChess;

TEST_CASE("buy exp reports whether a purchase happened", "[chess][shop]")
{
    CHECK(std::is_same_v<decltype(std::declval<ChessShopFlow&>().buyExp()), bool>);
}

TEST_CASE("equipment max tier includes all lower tiers", "[chess][equipment]")
{
    const std::vector<EquipmentDef> equipments{
        {101, 1, 0},
        {202, 2, 1},
        {303, 3, 0},
        {404, 4, 1},
        {505, 5, 0},
    };

    const auto filtered = filterEquipmentByMaxTier(equipments, 4);

    std::vector<int> itemIds;
    for (const auto* equipment : filtered)
    {
        itemIds.push_back(equipment->itemId);
    }
    CHECK(itemIds == std::vector<int>{101, 202, 303, 404});
}
