#include "ChessShopRoll.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <unordered_set>
#include <vector>

using namespace KysChess;

TEST_CASE("shop roll candidates prefer roles not seen in the previous roll", "[chess][shop]")
{
    const std::vector<int> tierRoles{10, 11, 12};
    const std::set<int> banned{12};
    const std::unordered_set<int> rejected{10};

    const auto candidates = buildShopCandidateBuckets(tierRoles, 1, banned, rejected);

    CHECK(candidates.preferred == std::vector<int>{11});
    CHECK(candidates.previousRollBlocked == std::vector<int>{10});
    CHECK(candidates.selectable() == std::vector<int>{11});
}

TEST_CASE("shop roll candidates block previous unselected roles from the next roll", "[chess][shop]")
{
    const std::vector<int> tierRoles{10};
    const std::set<int> banned;
    const std::unordered_set<int> rejected{10};

    const auto candidates = buildShopCandidateBuckets(tierRoles, 1, banned, rejected);

    CHECK(candidates.preferred.empty());
    CHECK(candidates.previousRollBlocked == std::vector<int>{10});
    CHECK(candidates.selectable().empty());
    CHECK_FALSE(tierHasShopCandidate(tierRoles, 1, banned, rejected));
}

TEST_CASE("shop roll candidates allow the same role to stay available within one roll", "[chess][shop]")
{
    const std::vector<int> tierRoles{10};
    const std::set<int> banned;
    const std::unordered_set<int> rejected;

    CHECK(tierHasShopCandidate(tierRoles, 1, banned, rejected));

    const auto firstSlotCandidates = buildShopCandidateBuckets(tierRoles, 1, banned, rejected);
    const auto secondSlotCandidates = buildShopCandidateBuckets(tierRoles, 1, banned, rejected);

    REQUIRE(firstSlotCandidates.selectable() == std::vector<int>{10});
    CHECK(secondSlotCandidates.selectable() == std::vector<int>{10});
}
