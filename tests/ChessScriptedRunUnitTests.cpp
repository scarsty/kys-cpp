#include "ChessGameSession.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessReplayHash.h"
#include "ChessReplayVerifier.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace KysChess;

namespace
{

ChessAction action(ChessActionType type)
{
    ChessAction result;
    result.type = type;
    return result;
}

std::shared_ptr<const ChessGameContent> scriptedContent()
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 20;
    data.balance.shopSlotCount = 3;
    data.balance.minBattleSize = 1;
    data.balance.totalFights = 1;
    data.balance.rewardBase = 0;
    data.balance.rewardGrowth = 0;
    for (auto& weights : data.balance.shopWeights)
    {
        weights = {100, 0, 0, 0, 0};
    }
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "完整流程棋子";
    role.Cost = 1;
    role.MaxHP = 800;
    role.Attack = 100;
    role.Defence = 20;
    role.Speed = 100;
    role.Fist = 100;
    for (int index = 0; index < 6; ++index)
    {
        role.MagicID[index] = 1;
        role.MagicPower[index] = 500;
    }
    data.roles.emplace(role.ID, role);
    ChessMagicDefinition magic;
    magic.ID = 1;
    magic.Name = "完整流程武學";
    magic.MagicType = 0;
    magic.HurtType = 0;
    magic.AttackAreaType = 0;
    magic.SelectDistance = 4;
    data.magics.emplace(magic.ID, magic);
    data.poolRoleIds.push_back(role.ID);
    return std::make_shared<const ChessGameContent>(std::move(data));
}

}

TEST_CASE("synthetic scripted run completes through a real headless battle", "[chess][scripted][smoke][replay]")
{
    const auto content = scriptedContent();
    ChessGameSession session(content, 0x51a7);
    for (int slot = 0; slot < 3; ++slot)
    {
        ChessAction buy = action(ChessActionType::BuyShopSlot);
        buy.shopSlot = slot;
        REQUIRE(session.submitAndDrain(buy).accepted);
    }
    REQUIRE(session.state().roster.size() == 1);
    const auto pieceId = session.state().roster.begin()->first;
    CHECK(session.state().roster.begin()->second.star == 2);

    ChessAction deploy = action(ChessActionType::SetDeployment);
    deploy.chessInstanceIds = {pieceId};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    REQUIRE(session.submitAndDrain(action(ChessActionType::PrepareBattle)).accepted);
    REQUIRE(session.submitAndDrain(action(ChessActionType::StartBattle)).accepted);

    CHECK(session.state().lastBattleOutcome == Battle::BattleOutcome::PlayerVictory);
    CHECK(session.state().campaignComplete);
    CHECK(session.state().fight == 1);
    REQUIRE(session.submitAndDrain(action(ChessActionType::FinishRun)).accepted);
    CHECK(session.state().phase == ChessSessionPhase::Complete);
    const auto replay = session.exportReplay();
    REQUIRE(replay);
    CHECK(replay->footer.complete);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
}

TEST_CASE("actual configuration gameplay smoke has stable roster equipment and hash", "[chess][scripted][smoke][actual-config]")
{
    const auto content = Test::actualContent();
    REQUIRE(content);
    ChessGameSession session(content, 0x5eed);

    for (const int slot : {0, 2})
    {
        ChessAction buy = action(ChessActionType::BuyShopSlot);
        buy.shopSlot = slot;
        REQUIRE(session.submitAndDrain(buy).accepted);
    }
    ChessAction deploy = action(ChessActionType::SetDeployment);
    deploy.chessInstanceIds = {1, 2};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    REQUIRE(session.submitAndDrain(action(ChessActionType::PrepareBattle)).accepted);
    REQUIRE(session.state().preparedBattle);
    REQUIRE(std::ranges::contains(
        session.state().preparedBattle->mapCandidates,
        session.state().preparedBattle->chosenMapId));
    REQUIRE(session.submitAndDrain(action(ChessActionType::StartBattle)).accepted);

    REQUIRE(session.state().roster.size() == 2);
    CHECK(session.state().roster.at(1).roleId == 4);
    CHECK(session.state().roster.at(2).roleId == 160);
    CHECK(session.state().equipmentInventory.empty());
    CHECK(session.state().lastBattleOutcome == Battle::BattleOutcome::PlayerVictory);
    CHECK(session.state().lastBattleEndFrame == 740);
    CHECK(session.state().fight == 1);
    CHECK(chessSha256Hex(session.observe().stateHash) == "f9ad8b516317c1526632ee23f82ec5c36cc50183890e3f6d2792e735ab3d1a6d");
    const auto replay = session.exportReplay();
    REQUIRE(replay);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
}
