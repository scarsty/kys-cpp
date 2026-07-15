#include "ChessManagementRules.h"
#include "ChessRewardRules.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

ChessGameContent eventContent()
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;

    ChessRoleDefinition first;
    first.ID = 10;
    first.Name = "甲";
    data.roles.emplace(first.ID, first);
    ChessRoleDefinition second = first;
    second.ID = 20;
    second.Name = "乙";
    data.roles.emplace(second.ID, second);

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
    return ChessGameContent(std::move(data));
}

}  // namespace

TEST_CASE("equipment assignment event carries ownership transfer semantics",
          "[chess][semantic-events][equipment]")
{
    const auto content = eventContent();
    ChessSessionState state;
    state.difficulty = Difficulty::Normal;
    state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true, 11});
    state.roster.emplace(2, ChessSessionPiece{2, 20, 1, false, 22});
    state.equipmentInventory.emplace(11, ChessEquipmentInstance{11, 100, 1});
    state.equipmentInventory.emplace(22, ChessEquipmentInstance{22, 101, 2});
    ChessRunRandom random(1);
    ChessAction action;
    action.type = ChessActionType::Equip;
    action.equipmentInstanceId = 11;
    action.targetChessInstanceId = 2;
    std::vector<ChessSemanticEvent> events;

    REQUIRE(ChessManagementRules::validate(state, content, action) == ChessRuleErrorCode::None);
    ChessManagementRules::apply(state, content, random, action, events);

    REQUIRE(events.size() == 1);
    REQUIRE(events.front().type == ChessSemanticEventType::EquipmentAssigned);
    const auto& detail =
        chessSemanticEventDetail<ChessEquipmentAssignedEventDetail>(events.front());
    CHECK(detail.equipmentInstanceId == 11);
    CHECK(detail.itemId == 100);
    CHECK(detail.targetChessInstanceId == 2);
    CHECK(detail.previousChessInstanceId == 1);
    CHECK(detail.displacedEquipmentInstanceId == 22);
    CHECK(state.roster.at(1).weaponInstanceId == -1);
    CHECK(state.roster.at(2).weaponInstanceId == 11);
    CHECK(state.equipmentInventory.at(11).assignedChessInstanceId == 2);
    CHECK(state.equipmentInventory.at(22).assignedChessInstanceId == -1);

    const auto stable = chessSemanticEventStableFields(events.front());
    CHECK(stable.primaryId == 11);
    CHECK(stable.secondaryId == 2);
    CHECK(stable.value == 100);
    CHECK(stable.stableId.empty());
}

TEST_CASE("reward selection event carries the selected typed option",
          "[chess][semantic-events][reward]")
{
    const auto content = eventContent();
    ChessSessionState state;
    state.difficulty = Difficulty::Normal;
    state.phase = ChessSessionPhase::RewardChoice;
    state.nextEquipmentInstanceId = 7;
    ChessPendingReward pending;
    pending.id = "equipment:campaign:4";
    pending.kind = ChessRewardKind::Equipment;
    pending.options.push_back({
        "equipment:102",
        ChessRewardKind::Equipment,
        102,
        3,
    });
    state.pendingRewards.push_back(pending);
    ChessRunRandom random(2);
    ChessAction action;
    action.type = ChessActionType::ChooseReward;
    action.rewardId = "equipment:102";
    std::vector<ChessSemanticEvent> events;

    REQUIRE(ChessRewardRules::validate(state, content, action) == ChessRuleErrorCode::None);
    ChessRewardRules::apply(state, content, random, action, events);

    REQUIRE(events.size() == 2);
    CHECK(events[0].type == ChessSemanticEventType::EquipmentAcquired);
    REQUIRE(events[1].type == ChessSemanticEventType::RewardChosen);
    const auto& detail =
        chessSemanticEventDetail<ChessRewardChosenEventDetail>(events[1]);
    CHECK(detail.rewardId == "equipment:102");
    CHECK(detail.kind == ChessRewardKind::Equipment);
    CHECK(detail.value == 102);
    CHECK(detail.value2 == 3);
    CHECK(state.phase == ChessSessionPhase::Management);
    CHECK(state.pendingRewards.empty());
    CHECK(state.equipmentInventory.at(7).itemId == 102);

    const auto stable = chessSemanticEventStableFields(events[1]);
    CHECK(stable.primaryId == 0);
    CHECK(stable.secondaryId == 0);
    CHECK(stable.value == 0);
    CHECK(stable.stableId == "equipment:102");
}
