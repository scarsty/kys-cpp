#include "ChessGameSession.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessReplayVerifier.h"
#include "ChessRuntimeConstants.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <format>
#include <iterator>
#include <set>

using namespace KysChess;
using namespace KysChess::Test;

TEST_CASE("session timeout policy is frozen at 36000 frames", "[chess][session][determinism]")
{
    ChessSessionOptions requested;
    requested.battleFrameLimit = 1;
    ChessGameSession session(managementContent(), 123, requested);

    CHECK(session.state().options.battleFrameLimit == kChessBattleFrameLimit);
    CHECK(session.journal().header().options.battleFrameLimit == kChessBattleFrameLimit);
}

TEST_CASE("seeded sessions expose identical pure observations", "[chess][session][determinism]")
{
    ChessGameSession first(managementContent(), 12345);
    ChessGameSession second(managementContent(), 12345);

    const auto beforeCounter = first.random().streamState(ChessRngStream::Shop).rawDrawCount;
    const auto firstObservation = first.observe();
    const auto legal = first.legalActions();

    CHECK(firstObservation.shop == second.observe().shop);
    CHECK(firstObservation.stateHash == second.observe().stateHash);
    CHECK_FALSE(legal.empty());
    CHECK(first.random().streamState(ChessRngStream::Shop).rawDrawCount == beforeCounter);
    CHECK(first.journal().decisions().empty());
}

TEST_CASE("accepted management actions journal from sequence one", "[chess][session][journal]")
{
    ChessGameSession session(managementContent(), 7);
    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;

    const auto result = session.submitAndDrain(lock);

    REQUIRE(result.accepted);
    CHECK(result.replaySequence == 1);
    CHECK(session.observe().shopLocked);
    REQUIRE(session.journal().decisions().size() == 1);
    CHECK(session.journal().decisions()[0].preStateHash == result.preStateHash);
    CHECK(session.journal().decisions()[0].postStateHash == result.postStateHash);
    const auto replay = session.exportReplay();
    REQUIRE(replay);
    CHECK(replay->footer.finalStateHash == result.postStateHash);
}

TEST_CASE("rejected actions mutate no state RNG or journal", "[chess][session][management]")
{
    ChessGameSession session(managementContent(0), 9);
    ChessAction refresh;
    refresh.type = ChessActionType::RefreshShop;
    const auto before = session.observe();
    const auto beforeCounter = session.random().streamState(ChessRngStream::Shop).rawDrawCount;

    const auto result = session.submitAndDrain(refresh);

    CHECK_FALSE(result.accepted);
    CHECK(result.error == ChessRuleErrorCode::InsufficientGold);
    CHECK(session.observe().stateHash == before.stateHash);
    CHECK(session.random().streamState(ChessRngStream::Shop).rawDrawCount == beforeCounter);
    CHECK(session.journal().decisions().empty());
}

TEST_CASE("legal actions omit unaffordable concrete management actions", "[chess][session][legal]")
{
    ChessGameSession session(managementContent(0), 10);
    const auto legal = session.legalActions();

    CHECK_FALSE(std::ranges::contains(legal, ChessActionType::RefreshShop, &ChessLegalActionDescriptor::type));
    CHECK_FALSE(std::ranges::contains(legal, ChessActionType::BuyShopSlot, &ChessLegalActionDescriptor::type));
    CHECK_FALSE(std::ranges::contains(legal, ChessActionType::BuyExp, &ChessLegalActionDescriptor::type));
    CHECK_FALSE(std::ranges::contains(legal, ChessActionType::RerollEnemySeed, &ChessLegalActionDescriptor::type));
    CHECK(std::ranges::contains(legal, ChessActionType::SetShopLocked, &ChessLegalActionDescriptor::type));
}

TEST_CASE("unjournaled money cheat remains playable but fails explicit verification",
          "[chess][session][cheat][replay]")
{
    const auto content = managementContent(0);
    ChessGameSession session(content, 10);
    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    REQUIRE(session.submitAndDrain(lock).accepted);
    const auto baselineReplay = session.exportReplay();
    REQUIRE(baselineReplay);
    CHECK(ChessReplayVerifier::verify(content, *baselineReplay).valid);
    CHECK_FALSE(std::ranges::contains(
        session.legalActions(),
        ChessActionType::RefreshShop,
        &ChessLegalActionDescriptor::type));
    const auto randomBeforeCheat = session.random().state();
    const auto chainBeforeCheat = session.journal().chainHash();
    REQUIRE(session.journal().decisions().size() == 1);
    const auto firstRecordChain = session.journal().decisions().front().chainHash;

    session.grantUnjournaledCheatMoney(100);

    CHECK(session.state().money == 100);
    CHECK(session.random().state() == randomBeforeCheat);
    REQUIRE(session.journal().decisions().size() == 1);
    CHECK(session.journal().chainHash() == chainBeforeCheat);
    CHECK(session.journal().decisions().front().chainHash == firstRecordChain);
    CHECK(std::ranges::contains(
        session.legalActions(),
        ChessActionType::RefreshShop,
        &ChessLegalActionDescriptor::type));

    const auto immediateReplay = session.exportReplay();
    REQUIRE(immediateReplay);
    const auto immediateVerification = ChessReplayVerifier::verify(content, *immediateReplay);
    CHECK_FALSE(immediateVerification.valid);
    CHECK(immediateVerification.mismatch == ChessReplayMismatch::Footer);
    CHECK(immediateVerification.sequence == 1);

    ChessAction refresh;
    refresh.type = ChessActionType::RefreshShop;
    const auto refreshResult = session.submitAndDrain(refresh);
    REQUIRE(refreshResult.accepted);
    CHECK(session.state().money == 98);
    const auto continuedReplay = session.exportReplay();
    REQUIRE(continuedReplay);
    const auto continuedVerification = ChessReplayVerifier::verify(content, *continuedReplay);
    CHECK_FALSE(continuedVerification.valid);
    CHECK(continuedVerification.mismatch == ChessReplayMismatch::PreState);
    CHECK(continuedVerification.sequence == 2);
}

TEST_CASE("enemy plan reroll spends the configured cost transactionally", "[chess][session][reroll][config]")
{
    ChessGameSession poor(managementContent(2), 21);
    ChessAction reroll;
    reroll.type = ChessActionType::RerollEnemySeed;
    const auto poorHash = poor.observe().stateHash;
    const auto rejected = poor.submitAndDrain(reroll);
    CHECK_FALSE(rejected.accepted);
    CHECK(rejected.error == ChessRuleErrorCode::InsufficientGold);
    CHECK(poor.observe().stateHash == poorHash);

    ChessGameSession funded(managementContent(10), 21);
    const auto oldKey = funded.random().enemyPlanKey();
    const auto accepted = funded.submitAndDrain(reroll);
    REQUIRE(accepted.accepted);
    CHECK(funded.state().money == 7);
    CHECK(funded.random().enemyPlanKey() != oldKey);
    REQUIRE(accepted.events.size() == 1);
    CHECK(accepted.events.front().type == ChessSemanticEventType::EnemyPlanRerolled);
    CHECK(accepted.events.front().value == 3);
}

TEST_CASE("shop purchases merge deterministically and stale slots reject", "[chess][session][management]")
{
    ChessGameSession session(managementContent(), 11);

    const auto first = session.submitAndDrain(buySlot(0));
    REQUIRE(first.accepted);
    REQUIRE(first.events.size() == 1);
    CHECK(first.events[0].type == ChessSemanticEventType::ChessPurchased);
    CHECK(first.events[0].secondaryId == 10);
    CHECK(first.events[0].value == 1);
    const auto stale = session.submitAndDrain(buySlot(0));
    CHECK_FALSE(stale.accepted);
    CHECK(stale.error == ChessRuleErrorCode::EmptyShopSlot);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    const auto third = session.submitAndDrain(buySlot(2));

    REQUIRE(third.accepted);
    REQUIRE(third.events.size() == 2);
    CHECK(third.events[0].type == ChessSemanticEventType::ChessPurchased);
    CHECK(third.events[0].secondaryId == 10);
    CHECK(third.events[0].value == 1);
    CHECK(third.events[1].type == ChessSemanticEventType::ChessMerged);
    CHECK(third.events[1].secondaryId == 10);
    CHECK(third.events[1].value == 2);
    REQUIRE(session.state().roster.size() == 1);
    CHECK(session.state().roster.begin()->second.star == 2);
    CHECK(session.journal().decisions().size() == 3);
}

TEST_CASE("configured map decision rejects overrides after selection", "[chess][session][map][config]")
{
    ChessGameSession session(configuredMapChoiceContent(), 12);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    REQUIRE(session.state().roster.size() == 1);

    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);

    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    REQUIRE(session.state().preparedBattle);
    CHECK(session.state().preparedBattle->chosenMapId == -1);
    CHECK(std::ranges::contains(
        session.legalActions(),
        ChessActionType::ChooseMap,
        &ChessLegalActionDescriptor::type));

    ChessAction choose;
    choose.type = ChessActionType::ChooseMap;
    choose.mapId = session.state().preparedBattle->mapCandidates.front();
    REQUIRE(session.submitAndDrain(choose).accepted);
    CHECK_FALSE(std::ranges::contains(
        session.legalActions(),
        ChessActionType::ChooseMap,
        &ChessLegalActionDescriptor::type));
    const auto selectedState = session.observe().stateHash;
    choose.mapId = session.state().preparedBattle->mapCandidates.back();

    const auto override = session.submitAndDrain(choose);

    CHECK_FALSE(override.accepted);
    CHECK(override.error == ChessRuleErrorCode::InvalidMap);
    CHECK(session.observe().stateHash == selectedState);
}

TEST_CASE("inactive configured map effect never exposes a choice", "[chess][session][map][config]")
{
    ChessGameSession session(configuredMapChoiceContent(), 14);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    REQUIRE(session.state().preparedBattle);
    CHECK(std::ranges::contains(
        session.state().preparedBattle->mapCandidates,
        session.state().preparedBattle->chosenMapId));
    CHECK_FALSE(std::ranges::contains(
        session.legalActions(),
        ChessActionType::ChooseMap,
        &ChessLegalActionDescriptor::type));

    ChessAction choose;
    choose.type = ChessActionType::ChooseMap;
    choose.mapId = session.state().preparedBattle->mapCandidates.front();
    const auto result = session.submitAndDrain(choose);

    CHECK_FALSE(result.accepted);
    CHECK(result.error == ChessRuleErrorCode::InvalidMap);
}

TEST_CASE("deployment validation is atomic", "[chess][session][management]")
{
    ChessGameSession session(managementContent(), 13);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    const auto ids = std::vector<int>{
        session.state().roster.begin()->first,
        std::next(session.state().roster.begin())->first,
    };
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = ids;
    REQUIRE(session.submitAndDrain(deploy).accepted);

    ChessAction invalid = deploy;
    invalid.chessInstanceIds = {ids.front(), 999};
    const auto before = session.observe().stateHash;
    const auto rejected = session.submitAndDrain(invalid);

    CHECK_FALSE(rejected.accepted);
    CHECK(rejected.error == ChessRuleErrorCode::UnknownChessInstance);
    CHECK(session.observe().stateHash == before);
    CHECK(std::ranges::all_of(session.state().roster, [](const auto& entry) { return entry.second.deployed; }));
}

TEST_CASE("one deployed piece can prepare campaign and challenge battles", "[chess][session][battle][challenge]")
{
    const auto content = singlePieceChallengeContent();

    ChessGameSession campaign(content, 31);
    REQUIRE(campaign.submitAndDrain(buySlot(0)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {campaign.state().roster.begin()->first};
    REQUIRE(campaign.submitAndDrain(deploy).accepted);
    CHECK(std::ranges::contains(
        campaign.legalActions(),
        ChessActionType::PrepareBattle,
        &ChessLegalActionDescriptor::type));
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    CHECK(campaign.submitAndDrain(prepare).accepted);

    ChessGameSession expedition(content, 31);
    REQUIRE(expedition.submitAndDrain(buySlot(0)).accepted);
    deploy.chessInstanceIds = {expedition.state().roster.begin()->first};
    REQUIRE(expedition.submitAndDrain(deploy).accepted);
    const auto challengeActions = expedition.legalActions();
    const auto challengeAction = std::ranges::find(
        challengeActions,
        ChessActionType::StartChallenge,
        &ChessLegalActionDescriptor::type);
    REQUIRE(challengeAction != challengeActions.end());
    CHECK(challengeAction->candidateStableIds == std::vector<std::string>{"單騎遠征"});
    ChessAction startChallenge;
    startChallenge.type = ChessActionType::StartChallenge;
    startChallenge.challengeName = "單騎遠征";
    CHECK(expedition.submitAndDrain(startChallenge).accepted);
}

TEST_CASE("post-clear management keeps challenges legal until finish run", "[chess][session][challenge][complete]")
{
    const auto content = singlePieceChallengeContent(0);
    ChessGameSession session(content, 32);
    REQUIRE(session.state().campaignComplete);
    REQUIRE(session.state().phase == ChessSessionPhase::Management);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);

    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);

    const auto legal = session.legalActions();
    CHECK_FALSE(std::ranges::contains(
        legal,
        ChessActionType::PrepareBattle,
        &ChessLegalActionDescriptor::type));
    const auto challenge = std::ranges::find(
        legal,
        ChessActionType::StartChallenge,
        &ChessLegalActionDescriptor::type);
    REQUIRE(challenge != legal.end());
    CHECK(challenge->candidateStableIds == std::vector<std::string>{"單騎遠征"});
    CHECK(std::ranges::contains(
        legal,
        ChessActionType::FinishRun,
        &ChessLegalActionDescriptor::type));

    ChessAction startChallenge;
    startChallenge.type = ChessActionType::StartChallenge;
    startChallenge.challengeName = "單騎遠征";
    REQUIRE(session.submitAndDrain(startChallenge).accepted);
    CHECK(session.state().phase == ChessSessionPhase::BattlePreparation);

    ChessGameSession finished(content, 32);
    ChessAction finish;
    finish.type = ChessActionType::FinishRun;
    REQUIRE(finished.submitAndDrain(finish).accepted);
    CHECK(finished.state().phase == ChessSessionPhase::Complete);
    CHECK(finished.legalActions().empty());
    const auto rejected = finished.submitAndDrain(startChallenge);
    CHECK_FALSE(rejected.accepted);
    CHECK(rejected.error == ChessRuleErrorCode::WrongPhase);
}

TEST_CASE("piece merging preserves wins and the legacy best-equipment owner order", "[chess][management][merge]")
{
    auto makeContent = [] {
        ChessGameContentData data;
        ChessRoleDefinition role;
        role.ID = 10;
        role.Name = "合成棋子";
        role.Cost = 1;
        data.roles.emplace(role.ID, role);
        data.equipment = {
            {100, 1, 0},
            {200, 3, 0},
            {201, 3, 0},
        };
        return ChessGameContent(std::move(data));
    };

    SECTION("higher equipment tier and maximum wins survive")
    {
        auto content = makeContent();
        ChessSessionState state;
        state.nextChessInstanceId = 3;
        state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true, 11, -1, 2});
        state.roster.emplace(2, ChessSessionPiece{2, 10, 1, false, 22, -1, 7});
        state.equipmentInventory.emplace(11, ChessEquipmentInstance{11, 100, 1});
        state.equipmentInventory.emplace(22, ChessEquipmentInstance{22, 200, 2});
        std::vector<ChessSemanticEvent> events;

        ChessManagementRules::grantPiece(state, content, 10, events);

        REQUIRE(state.roster.size() == 1);
        const auto& upgraded = state.roster.begin()->second;
        CHECK(upgraded.star == 2);
        CHECK(upgraded.deployed);
        CHECK(upgraded.fightsWon == 7);
        CHECK(upgraded.weaponInstanceId == 22);
        CHECK(state.equipmentInventory.at(11).assignedChessInstanceId == -1);
        CHECK(state.equipmentInventory.at(22).assignedChessInstanceId == upgraded.instanceId);
    }

    SECTION("equal tiers keep the first consumed piece owner")
    {
        auto content = makeContent();
        ChessSessionState state;
        state.nextChessInstanceId = 3;
        state.roster.emplace(1, ChessSessionPiece{1, 10, 1, false, 22});
        state.roster.emplace(2, ChessSessionPiece{2, 10, 1, false, 11});
        state.equipmentInventory.emplace(11, ChessEquipmentInstance{11, 201, 2});
        state.equipmentInventory.emplace(22, ChessEquipmentInstance{22, 200, 1});
        std::vector<ChessSemanticEvent> events;

        ChessManagementRules::grantPiece(state, content, 10, events);

        REQUIRE(state.roster.size() == 1);
        CHECK(state.roster.begin()->second.weaponInstanceId == 22);
    }
}

TEST_CASE("shop refresh preserves prior-roll blocking and tier-five repeat behavior", "[chess][management][shop]")
{
    auto contentForTier = [](int tier) {
        ChessGameContentData data;
        data.balance.shopSlotCount = 1;
        for (auto& row : data.balance.shopWeights)
        {
            row = {0, 0, 0, 0, 0};
            row[tier - 1] = 100;
        }
        ChessRoleDefinition role;
        role.ID = tier * 10;
        role.Name = std::format("{}費棋子", tier);
        role.Cost = tier;
        data.roles.emplace(role.ID, role);
        data.poolRoleIds = {role.ID};
        return ChessGameContent(std::move(data));
    };

    SECTION("tiers one through four do not recycle an exhausted prior roll")
    {
        auto content = contentForTier(1);
        ChessSessionState state;
        state.shop = {{10, 1}};
        ChessRunRandom random(1);
        ChessManagementRules::refreshShop(state, content, random);
        CHECK(state.shop.empty());
        CHECK(state.rejectedRoleIds == std::set<int>{10});
    }

    SECTION("tier five ignores prior-roll rejection")
    {
        auto content = contentForTier(5);
        ChessSessionState state;
        state.shop = {{50, 5}};
        ChessRunRandom random(1);
        ChessManagementRules::refreshShop(state, content, random);
        REQUIRE(state.shop.size() == 1);
        CHECK(state.shop.front().roleId == 50);
    }
}

TEST_CASE("legendary shop disabled state and exact tier are config driven", "[chess][management][equipment][config]")
{
    ChessGameContentData data;
    data.balance.legendaryShop.unlockFight = 0;
    data.balance.legendaryShop.price = 4;
    data.equipment = {{400, 4, 0}, {500, 5, 0}};
    ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.money = 10;
    ChessAction buy;
    buy.type = ChessActionType::BuyLegendaryEquipment;
    buy.itemId = 400;
    CHECK(ChessManagementRules::validate(state, content, buy) == ChessRuleErrorCode::LegendaryShopLocked);

    auto enabledData = ChessGameContentData{};
    enabledData.balance.legendaryShop.unlockFight = 1;
    enabledData.balance.legendaryShop.price = 4;
    enabledData.equipment = {{400, 4, 0}, {500, 5, 0}};
    ChessGameContent enabled(std::move(enabledData));
    state.fight = 1;
    buy.itemId = 500;
    CHECK(ChessManagementRules::validate(state, enabled, buy) == ChessRuleErrorCode::InvalidEquipment);
    buy.itemId = 400;
    CHECK(ChessManagementRules::validate(state, enabled, buy) == ChessRuleErrorCode::None);
}

TEST_CASE("experience purchase and selling use configured economy", "[chess][session][management]")
{
    ChessGameSession session(managementContent(), 15);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    const int pieceId = session.state().roster.begin()->first;

    ChessAction buyExp;
    buyExp.type = ChessActionType::BuyExp;
    REQUIRE(session.submitAndDrain(buyExp).accepted);
    CHECK(session.state().level == 1);
    CHECK(session.state().experience == 1);

    ChessAction sell;
    sell.type = ChessActionType::SellChess;
    sell.chessInstanceId = pieceId;
    REQUIRE(session.submitAndDrain(sell).accepted);
    CHECK(session.state().roster.empty());
    CHECK(session.state().money == 95);
}
