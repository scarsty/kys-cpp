#include "ChessGameQueries.h"
#include "ChessGameSessionTestHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("management inspection queries are pure",
          "[chess][game-queries][purity]")
{
    ChessGameSession session(managementContent(), 51);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    const auto stateBefore = session.state();
    const auto randomBefore = session.random().state();
    const auto chainBefore = session.journal().chainHash();
    const auto decisionCountBefore = session.journal().decisions().size();
    const auto hashBefore = session.observe().stateHash;
    const auto odds = queryChessShopOdds(session.state(), session.content(), session.state().level);
    const auto slot = queryChessShopSlot(session.state(), session.content(), 2);
    const auto shop = queryChessShop(session.state(), session.content());
    const auto piece = queryChessInstance(
        session.state(),
        session.content(),
        session.state().roster.begin()->first);
    const auto bans = queryChessBans(session.state(), session.content(), session.legalActions());

    CHECK(odds.tiers.front().probability == 1.0);
    CHECK(slot.ownedCopies == 2);
    CHECK(slot.projectedResult == ChessProjectedPurchaseResult::Merge);
    CHECK(slot.projectedResultStar == 2);
    CHECK(shop.slots.size() == session.state().shop.size());
    CHECK(piece.sameStarCopies == 2);
    CHECK(piece.copiesRequiredForNextStar == 1);
    CHECK(bans.currentBanCount == 0);

    CHECK(session.state() == stateBefore);
    CHECK(session.random().state() == randomBefore);
    CHECK(session.journal().chainHash() == chainBefore);
    CHECK(session.journal().decisions().size() == decisionCountBefore);
    CHECK(session.observe().stateHash == hashBefore);
}
