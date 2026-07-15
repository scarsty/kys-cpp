#include "ChessGameSessionTestHelpers.h"
#include "ChessRewardRules.h"
#include "ChessSessionCheckpoint.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

const ChessActionOffer& requireOffer(
    const std::vector<ChessActionOffer>& offers,
    ChessActionType type)
{
    const auto found = std::ranges::find(offers, type, &ChessActionOffer::type);
    REQUIRE(found != offers.end());
    return *found;
}

}  // namespace

TEST_CASE("typed action offers preserve management order and authoritative economics",
          "[chess][action-offers][management]")
{
    ChessGameSession session(managementContent(), 44);
    const auto stateBefore = session.state();
    const auto randomBefore = session.random().state();
    const auto chainBefore = session.journal().chainHash();
    const auto offers = session.legalActions();

    std::vector<ChessActionType> types;
    for (const auto& offer : offers) types.push_back(offer.type);
    CHECK(types == std::vector<ChessActionType>{
        ChessActionType::RefreshShop,
        ChessActionType::SetShopLocked,
        ChessActionType::BuyShopSlot,
        ChessActionType::SetDeployment,
        ChessActionType::SetPositionSwapEnabled,
        ChessActionType::RerollEnemySeed,
        ChessActionType::BuyExp,
    });

    const auto& buy = requireOffer(offers, ChessActionType::BuyShopSlot);
    const auto& slots = chessActionOfferDetail<ChessShopSlotSelectionOffer>(buy).candidates;
    REQUIRE(slots.size() == 5);
    CHECK(slots.front().slot == 0);
    CHECK(slots.front().roleId == 10);
    CHECK(slots.front().cost == 1);
    CHECK(slots.front().projectedGoldAfter == 99);
    for (const auto& candidate : slots)
    {
        ChessAction action;
        action.type = ChessActionType::BuyShopSlot;
        action.shopSlot = candidate.slot;
        CHECK(session.validateAction(action) == ChessRuleErrorCode::None);
    }

    const auto& experience = requireOffer(offers, ChessActionType::BuyExp);
    REQUIRE(experience.economicConsequence);
    CHECK(experience.economicConsequence->goldCost == 5);
    CHECK(experience.economicConsequence->projectedGoldAfter == 95);
    CHECK(experience.economicConsequence->experienceGained == 5);
    CHECK(experience.economicConsequence->projectedExperienceAfter == 1);
    CHECK(experience.economicConsequence->projectedLevelAfter == 1);

    CHECK(session.state() == stateBefore);
    CHECK(session.random().state() == randomBefore);
    CHECK(session.journal().chainHash() == chainBefore);
}

TEST_CASE("typed action offers expose field-specific prepared battle candidates",
          "[chess][action-offers][prepared]")
{
    ChessGameSession session(configuredMapChoiceContent(), 45);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);

    const auto offers = session.legalActions();
    const auto& map = requireOffer(offers, ChessActionType::ChooseMap);
    CHECK(map.minimumSelection == 1);
    CHECK(map.maximumSelection == 1);
    const auto& candidates = chessActionOfferDetail<ChessMapSelectionOffer>(map).candidates;
    REQUIRE(candidates.size() == 2);
    for (const auto& candidate : candidates)
    {
        ChessAction action;
        action.type = ChessActionType::ChooseMap;
        action.mapId = candidate.mapId;
        CHECK(session.validateAction(action) == ChessRuleErrorCode::None);
    }
}

TEST_CASE("forced-ban offers expose only validated role candidates",
          "[chess][action-offers][reward]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 46);
    auto checkpoint = ChessSessionCheckpoint::capture(session, 1);
    std::vector<ChessSemanticEvent> events;
    ChessRewardRules::enqueueForcedBan(checkpoint.state, *content, 1, 1, events);
    REQUIRE(checkpoint.restore(session) == ChessCheckpointError::None);

    const auto offers = session.legalActions();
    REQUIRE(offers.size() == 2);
    const auto& ban = requireOffer(offers, ChessActionType::AddBan);
    const auto& roles = chessActionOfferDetail<ChessRoleSelectionOffer>(ban).candidates;
    REQUIRE(roles.size() == 1);
    ChessAction action;
    action.type = ChessActionType::AddBan;
    action.roleId = roles.front().roleId;
    CHECK(session.validateAction(action) == ChessRuleErrorCode::None);
    CHECK(offers.back().type == ChessActionType::SkipForcedBans);
}
