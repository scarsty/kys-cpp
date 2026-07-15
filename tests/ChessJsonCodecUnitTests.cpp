#include "ChessGameSessionTestHelpers.h"
#include "ChessJsonCodec.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;
using namespace KysChess::ProtocolDetail;
using namespace KysChess::Test;

namespace
{

ChessAction shopLockAction()
{
    ChessAction action;
    action.type = ChessActionType::SetShopLocked;
    action.value = true;
    return action;
}

}  // namespace

TEST_CASE("JSON codec keeps summary compact and full action projections distinct",
          "[chess][json-codec][projection]")
{
    const auto content = managementContent();

    ChessGameSession summarySession(content, 71);
    const auto summaryBefore = summarySession.state();
    const auto summaryResult = summarySession.submitAndDrain(shopLockAction());
    REQUIRE(summaryResult.accepted);
    const auto summary = writeJson(summaryActionResultDto(
        summarySession,
        summaryBefore,
        summaryResult,
        ChessActionType::SetShopLocked));
    CHECK(summary.contains("\"accepted\":true"));
    CHECK(summary.contains("\"events\":[\"shop_lock_changed\"]"));
    CHECK(summary.contains("\"state_hash\""));
    CHECK_FALSE(summary.contains("\"next_observation\""));
    CHECK_FALSE(summary.contains("\"pre_state_hash\""));

    ChessGameSession compactSession(content, 72);
    const auto compactResult = compactSession.submitAndDrain(shopLockAction());
    REQUIRE(compactResult.accepted);
    const auto compact = writeJson(actionResultDto(
        compactSession,
        compactResult,
        ChessActionType::SetShopLocked,
        ActionResponseDetail::Compact));
    CHECK(compact.contains("\"next_observation\""));
    CHECK(compact.contains("\"detail\":\"compact\""));
    CHECK_FALSE(compact.contains("\"pre_state_hash\""));
    CHECK_FALSE(compact.contains("\"relevant_roles\""));

    ChessGameSession fullSession(content, 73);
    const auto fullResult = fullSession.submitAndDrain(shopLockAction());
    REQUIRE(fullResult.accepted);
    const auto full = writeJson(actionResultDto(
        fullSession,
        fullResult,
        ChessActionType::SetShopLocked,
        ActionResponseDetail::Full));
    CHECK(full.contains("\"detail\":\"full\""));
    CHECK(full.contains("\"pre_state_hash\""));
    CHECK(full.contains("\"post_state_hash\""));
    CHECK(full.contains("\"event_hash\""));
    CHECK(full.contains("\"rng_digest\""));
    CHECK(full.contains("\"chain_hash\""));
    CHECK(full.contains("\"relevant_roles\""));
}

TEST_CASE("JSON codec battle projection omits full-only data in compact detail",
          "[chess][json-codec][projection][battle]")
{
    ChessGameSession session(configuredMapChoiceContent(), 74);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);
    ChessAction deployment;
    deployment.type = ChessActionType::SetDeployment;
    deployment.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deployment).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    ChessAction map;
    map.type = ChessActionType::ChooseMap;
    map.mapId = session.state().preparedBattle->mapCandidates.front();
    REQUIRE(session.submitAndDrain(map).accepted);
    ChessAction start;
    start.type = ChessActionType::StartBattle;
    REQUIRE(session.submitAndDrain(start).accepted);

    const auto compactDto =
        inspectLastBattleDto(session, ObservationDetail::Compact);
    const auto fullDto =
        inspectLastBattleDto(session, ObservationDetail::Full);
    REQUIRE(compactDto);
    REQUIRE(fullDto);
    const auto compact = writeJson(*compactDto);
    const auto full = writeJson(*fullDto);

    CHECK(compact.contains("\"detail\":\"compact\""));
    CHECK(compact.contains("\"unit_stats\""));
    CHECK(compact.contains("\"summary\""));
    CHECK_FALSE(compact.contains("\"initial_board\""));
    CHECK_FALSE(compact.contains("\"effect_activations\""));
    CHECK(full.contains("\"detail\":\"full\""));
    CHECK(full.contains("\"initial_board\""));
    CHECK(full.contains("\"effect_activations\""));
    CHECK(full.contains("\"initial_combat_stats\""));
}

TEST_CASE("JSON codec reads equipment and reward meaning from typed event payloads",
          "[chess][json-codec][semantic-events]")
{
    const auto content = managementContent();
    const ChessSemanticEvent equipment{
        ChessSemanticEventType::EquipmentAssigned,
        ChessEquipmentAssignedEventDetail{11, 100, 2, 1, 22},
    };
    const auto equipmentJson = writeJson(semanticEventDto(*content, equipment));
    CHECK(equipmentJson.contains("\"type\":\"equipment_assigned\""));
    CHECK(equipmentJson.contains("\"primary_id\":11"));
    CHECK(equipmentJson.contains("\"secondary_id\":2"));
    CHECK(equipmentJson.contains("\"value\":100"));

    const ChessSemanticEvent reward{
        ChessSemanticEventType::RewardChosen,
        ChessRewardChosenEventDetail{
            "equipment:102",
            ChessRewardKind::Equipment,
            102,
            3,
        },
    };
    const auto rewardJson = writeJson(semanticEventDto(*content, reward));
    CHECK(rewardJson.contains("\"type\":\"reward_chosen\""));
    CHECK(rewardJson.contains("\"primary_id\":0"));
    CHECK(rewardJson.contains("\"secondary_id\":0"));
    CHECK(rewardJson.contains("\"value\":0"));
    CHECK(rewardJson.contains("\"stable_id\":\"equipment:102\""));
}
