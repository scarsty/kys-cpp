#include "ChessGuiSessionAdapter.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessJsonProtocol.h"
#include "ChessReplayJson.h"
#include "ChessSessionCheckpoint.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

void checkEquivalentSubmission(
    ChessGuiSessionAdapter& gui,
    ChessGameSession& guiSession,
    ChessGameSession& directSession,
    const ChessAction& action)
{
    const auto guiResult = gui.submitAction(action);
    const auto directResult = directSession.submitAndDrain(action);

    CAPTURE(chessActionTypeId(action.type), guiResult.description, directResult.description);
    REQUIRE(guiResult.accepted == directResult.accepted);
    REQUIRE(guiResult.accepted);
    CHECK(guiResult.error == directResult.error);
    CHECK(guiResult.transitionPending == directResult.transitionPending);
    CHECK(guiResult.replaySequence == directResult.replaySequence);
    CHECK(guiResult.preStateHash == directResult.preStateHash);
    CHECK(guiResult.postStateHash == directResult.postStateHash);
    CHECK(guiResult.eventHash == directResult.eventHash);
    CHECK(guiResult.rngDigest == directResult.rngDigest);
    CHECK(guiResult.chainHash == directResult.chainHash);
    REQUIRE(guiResult.events.size() == directResult.events.size());
    for (std::size_t index = 0; index < guiResult.events.size(); ++index)
    {
        CHECK(guiResult.events[index].type == directResult.events[index].type);
        const auto guiFields = chessSemanticEventStableFields(guiResult.events[index]);
        const auto directFields = chessSemanticEventStableFields(directResult.events[index]);
        CHECK(guiFields.primaryId == directFields.primaryId);
        CHECK(guiFields.secondaryId == directFields.secondaryId);
        CHECK(guiFields.value == directFields.value);
        CHECK(guiFields.stableId == directFields.stableId);
    }
    CHECK(guiSession.observe().stateHash == directSession.observe().stateHash);
    CHECK(guiSession.random().state() == directSession.random().state());
    CHECK(guiSession.journal().chainHash() == directSession.journal().chainHash());
}

void restorePair(
    ChessGameSession& first,
    ChessGameSession& second,
    ChessSessionCheckpoint checkpoint)
{
    REQUIRE(checkpoint.restore(first) == ChessCheckpointError::None);
    REQUIRE(checkpoint.restore(second) == ChessCheckpointError::None);
}

std::shared_ptr<const ChessGameContent> workflowManagementContent()
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 100;
    data.balance.shopSlotCount = 2;
    data.balance.benchSize = 10;
    data.balance.minBattleSize = 1;
    data.balance.banBaseCount = 2;
    data.balance.expTable = {4, 6};
    data.balance.maxLevel = 2;
    for (auto& weights : data.balance.shopWeights)
    {
        weights = {100, 0, 0, 0, 0};
    }

    ChessRoleDefinition first;
    first.ID = 10;
    first.Name = "甲";
    first.Cost = 1;
    first.MaxHP = 100;
    data.roles.emplace(first.ID, first);
    ChessRoleDefinition second = first;
    second.ID = 20;
    second.Name = "乙";
    data.roles.emplace(second.ID, second);
    data.poolRoleIds = {10, 20};

    data.items.emplace(100, ChessItemDefinition{
        100, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "甲劍"});
    data.items.emplace(101, ChessItemDefinition{
        101, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "乙劍"});
    data.items.emplace(102, ChessItemDefinition{
        102, -1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, "獎勵劍"});
    data.equipment = {
        {100, 1, 0, {}, {}},
        {101, 1, 0, {}, {}},
        {102, 1, 0, {}, {}},
    };
    return std::make_shared<const ChessGameContent>(std::move(data));
}

}  // namespace

TEST_CASE("GUI adapter and direct session calls produce identical hashes", "[chess][gui][parity][determinism]")
{
    const auto content = managementContent();
    ChessGameSession guiSession(content, 321);
    ChessGameSession directSession(content, 321);
    ChessGuiSessionAdapter adapter(guiSession);

    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    const auto gui = adapter.submitAction(lock);
    const auto direct = directSession.submitAndDrain(lock);

    REQUIRE(gui.accepted);
    REQUIRE(direct.accepted);
    CHECK(gui.postStateHash == direct.postStateHash);
    CHECK(gui.eventHash == direct.eventHash);
    CHECK(gui.rngDigest == direct.rngDigest);
    CHECK(gui.chainHash == direct.chainHash);
}

TEST_CASE("GUI adapter and JSONL playthrough export identical replays", "[chess][gui][cli][parity][determinism]")
{
    const auto content = configuredMapChoiceContent();
    ChessGameSession guiSession(content, 0x1234);
    ChessGuiSessionAdapter gui(guiSession);
    ChessJsonProtocol jsonl(content);
    const auto created = jsonl.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000001234"}})");
    REQUIRE(created.contains(R"("ok":true)"));
    REQUIRE(jsonl.session());

    int requestId = 2;
    auto submitBoth = [&](const ChessAction& action) {
        const auto guiResult = gui.submitAction(action);
        const auto request = std::string{"{\"id\":"}
            + std::to_string(requestId++)
            + ",\"method\":\"act\",\"params\":{\"action\":"
            + serializeChessActionJson(action)
            + "}}";
        const auto response = jsonl.handleLine(request);

        CAPTURE(chessActionTypeId(action.type), response);
        REQUIRE(guiResult.accepted);
        REQUIRE(response.contains(R"("ok":true)"));
        REQUIRE(response.contains(R"("accepted":true)"));
        REQUIRE(jsonl.session());
        CHECK(jsonl.session()->observe().stateHash == guiSession.observe().stateHash);
        CHECK(jsonl.session()->journal().chainHash() == guiSession.journal().chainHash());
    };

    submitBoth(buySlot(0));
    submitBoth(buySlot(1));
    submitBoth(buySlot(2));

    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {guiSession.state().roster.begin()->first};
    submitBoth(deploy);

    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    submitBoth(prepare);
    REQUIRE(guiSession.state().preparedBattle);
    REQUIRE_FALSE(guiSession.state().preparedBattle->mapCandidates.empty());

    ChessAction chooseMap;
    chooseMap.type = ChessActionType::ChooseMap;
    chooseMap.mapId = guiSession.state().preparedBattle->mapCandidates.front();
    submitBoth(chooseMap);

    ChessAction startBattle;
    startBattle.type = ChessActionType::StartBattle;
    submitBoth(startBattle);

    const auto guiReplay = guiSession.exportReplay();
    const auto jsonlReplay = jsonl.session()->exportReplay();
    REQUIRE(guiReplay);
    REQUIRE(jsonlReplay);
    CHECK(serializeChessReplayJsonl(*guiReplay) == serializeChessReplayJsonl(*jsonlReplay));
}

TEST_CASE("GUI management action sequence preserves session hashes and events",
          "[chess][gui][parity][workflow]")
{
    const auto content = managementContent();
    ChessGameSession guiSession(content, 0x2101);
    ChessGameSession directSession(content, 0x2101);
    ChessGuiSessionAdapter gui(guiSession);

    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    checkEquivalentSubmission(gui, guiSession, directSession, lock);
    lock.value = false;
    checkEquivalentSubmission(gui, guiSession, directSession, lock);

    checkEquivalentSubmission(gui, guiSession, directSession, buySlot(0));

    ChessAction deployment;
    deployment.type = ChessActionType::SetDeployment;
    deployment.chessInstanceIds = {guiSession.state().roster.begin()->first};
    checkEquivalentSubmission(gui, guiSession, directSession, deployment);

    ChessAction experience;
    experience.type = ChessActionType::BuyExp;
    checkEquivalentSubmission(gui, guiSession, directSession, experience);

    ChessAction sell;
    sell.type = ChessActionType::SellChess;
    sell.chessInstanceId = guiSession.state().roster.begin()->first;
    checkEquivalentSubmission(gui, guiSession, directSession, sell);

    ChessAction refresh;
    refresh.type = ChessActionType::RefreshShop;
    checkEquivalentSubmission(gui, guiSession, directSession, refresh);
}

TEST_CASE("GUI ban equipment and reward choices preserve session hashes and events",
          "[chess][gui][parity][workflow]")
{
    const auto content = workflowManagementContent();
    ChessGameSession guiSession(content, 0x2102);
    ChessGameSession directSession(content, 0x2102);
    auto checkpoint = ChessSessionCheckpoint::capture(guiSession, 1);
    checkpoint.state.phase = ChessSessionPhase::Management;
    checkpoint.state.money = 100;
    checkpoint.state.roster = {
        {1, ChessSessionPiece{1, 10, 1, true, 11}},
        {2, ChessSessionPiece{2, 20, 1, false, 22}},
    };
    checkpoint.state.nextChessInstanceId = 3;
    checkpoint.state.equipmentInventory = {
        {11, ChessEquipmentInstance{11, 100, 1}},
        {22, ChessEquipmentInstance{22, 101, 2}},
    };
    checkpoint.state.nextEquipmentInstanceId = 23;
    checkpoint.state.seenRoleIds = {10, 20};
    restorePair(guiSession, directSession, checkpoint);
    ChessGuiSessionAdapter gui(guiSession);

    ChessAction ban;
    ban.type = ChessActionType::AddBan;
    ban.roleId = 20;
    checkEquivalentSubmission(gui, guiSession, directSession, ban);

    ChessAction equip;
    equip.type = ChessActionType::Equip;
    equip.equipmentInstanceId = 11;
    equip.targetChessInstanceId = 2;
    checkEquivalentSubmission(gui, guiSession, directSession, equip);

    checkpoint = ChessSessionCheckpoint::capture(guiSession, 2);
    checkpoint.state.phase = ChessSessionPhase::RewardChoice;
    checkpoint.state.pendingRewards.clear();
    ChessPendingReward pending;
    pending.id = "equipment:campaign:4";
    pending.kind = ChessRewardKind::Equipment;
    pending.options.push_back({
        "equipment:102",
        ChessRewardKind::Equipment,
        102,
    });
    checkpoint.state.pendingRewards.push_back(std::move(pending));
    restorePair(guiSession, directSession, checkpoint);

    ChessAction reward;
    reward.type = ChessActionType::ChooseReward;
    reward.rewardId = "equipment:102";
    checkEquivalentSubmission(gui, guiSession, directSession, reward);
}

TEST_CASE("GUI challenge selection preserves the prepared battle boundary",
          "[chess][gui][parity][workflow][challenge]")
{
    const auto content = singlePieceChallengeContent();
    ChessGameSession guiSession(content, 0x2103);
    ChessGameSession directSession(content, 0x2103);
    ChessGuiSessionAdapter gui(guiSession);

    checkEquivalentSubmission(gui, guiSession, directSession, buySlot(0));
    ChessAction deployment;
    deployment.type = ChessActionType::SetDeployment;
    deployment.chessInstanceIds = {guiSession.state().roster.begin()->first};
    checkEquivalentSubmission(gui, guiSession, directSession, deployment);

    ChessAction challenge;
    challenge.type = ChessActionType::StartChallenge;
    challenge.challengeName = "單騎遠征";
    checkEquivalentSubmission(gui, guiSession, directSession, challenge);
    CHECK(guiSession.state().phase == ChessSessionPhase::BattlePreparation);
}

TEST_CASE("GUI map swap and battle start sequence preserves deterministic outcomes",
          "[chess][gui][parity][workflow][battle]")
{
    const auto content = configuredMapChoiceContent();
    ChessGameSession guiSession(content, 0x2104);
    ChessGameSession directSession(content, 0x2104);
    ChessGuiSessionAdapter gui(guiSession);

    checkEquivalentSubmission(gui, guiSession, directSession, buySlot(0));
    ChessAction deployment;
    deployment.type = ChessActionType::SetDeployment;
    deployment.chessInstanceIds = {guiSession.state().roster.begin()->first};
    checkEquivalentSubmission(gui, guiSession, directSession, deployment);

    auto checkpoint = ChessSessionCheckpoint::capture(guiSession, 1);
    checkpoint.state.roster.begin()->second.star = 2;
    checkpoint.state.roster.emplace(2, ChessSessionPiece{2, 30, 1, true});
    checkpoint.state.nextChessInstanceId = 3;
    restorePair(guiSession, directSession, checkpoint);

    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    checkEquivalentSubmission(gui, guiSession, directSession, prepare);
    REQUIRE(guiSession.state().preparedBattle);
    REQUIRE(guiSession.state().preparedBattle->mapCandidates.size() == 2);

    ChessAction map;
    map.type = ChessActionType::ChooseMap;
    map.mapId = guiSession.state().preparedBattle->mapCandidates.front();
    checkEquivalentSubmission(gui, guiSession, directSession, map);
    REQUIRE(guiSession.state().preparedBattle->units.size() >= 2);

    ChessAction swap;
    swap.type = ChessActionType::SwapPositions;
    swap.chessInstanceId = guiSession.state().preparedBattle->units[0].unitId;
    swap.targetChessInstanceId = guiSession.state().preparedBattle->units[1].unitId;
    checkEquivalentSubmission(gui, guiSession, directSession, swap);

    ChessAction start;
    start.type = ChessActionType::StartBattle;
    checkEquivalentSubmission(gui, guiSession, directSession, start);
    REQUIRE(guiSession.lastBattleResult());
    REQUIRE(directSession.lastBattleResult());
    CHECK(guiSession.lastBattleResult()->digest == directSession.lastBattleResult()->digest);
}
