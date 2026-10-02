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

TEST_CASE("JSON codec equipment projection selects description style without rendering identity data",
          "[chess][json-codec][projection][equipment]")
{
    const auto content = syntheticContent();
    REQUIRE(content);

    bool foundStyleDifference{};
    for (const auto& definition : content->equipment())
    {
        const auto compact = writeJson(equipmentInfoDto(
            *content,
            definition.itemId,
            EquipmentProjection::Detailed,
            EffectDescriptionStyle::Compact));
        const auto full = writeJson(equipmentInfoDto(
            *content,
            definition.itemId,
            EquipmentProjection::Detailed,
            EffectDescriptionStyle::Full));
        if (compact != full)
        {
            foundStyleDifference = true;
            break;
        }
    }
    CHECK(foundStyleDifference);

    const int itemId = content->equipment().front().itemId;
    const auto compactIdentity = writeJson(equipmentInfoDto(
        *content,
        itemId,
        EquipmentProjection::Identity,
        EffectDescriptionStyle::Compact));
    const auto fullIdentity = writeJson(equipmentInfoDto(
        *content,
        itemId,
        EquipmentProjection::Identity,
        EffectDescriptionStyle::Full));
    CHECK(compactIdentity == fullIdentity);
    CHECK_FALSE(compactIdentity.contains("base_stat_effects"));
    CHECK_FALSE(compactIdentity.contains("special_effects"));
    CHECK_FALSE(compactIdentity.contains("character_bonuses"));
}

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
    CHECK_FALSE(summary.contains("\"evidence_hash\""));

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
    CHECK_FALSE(compact.contains("\"evidence_hash\""));
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
    CHECK(full.contains("\"evidence_hash\""));
    CHECK(full.contains("\"relevant_roles\""));
}

TEST_CASE("JSON codec battle projections keep summary and compact reports bounded",
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

    const auto summaryDto =
        inspectLastBattleDto(session, BattleReportDetail::Summary);
    const auto compactDto =
        inspectLastBattleDto(session, BattleReportDetail::Compact);
    const auto fullDto =
        inspectLastBattleDto(session, BattleReportDetail::Full);
    REQUIRE(summaryDto);
    REQUIRE(compactDto);
    REQUIRE(fullDto);
    const auto summary = writeJson(*summaryDto);
    const auto compact = writeJson(*compactDto);
    const auto full = writeJson(*fullDto);

    CHECK(summary.contains("\"detail\":\"summary\""));
    CHECK(summary.contains("\"unit_stats\""));
    CHECK(summary.contains("\"death_order\""));
    CHECK_FALSE(summary.contains("\"key_events\""));
    CHECK_FALSE(summary.contains("\"initial_board\""));
    CHECK_FALSE(summary.contains("\"important_effects\""));
    CHECK_FALSE(summary.contains("\"effect_activations\""));
    CHECK(compact.contains("\"detail\":\"compact\""));
    CHECK(compact.contains("\"unit_stats\""));
    CHECK(compact.contains("\"summary\""));
    CHECK(compact.contains("\"initial_board\""));
    CHECK_FALSE(compact.contains("\"important_effects\""));
    CHECK_FALSE(compact.contains("\"skill_damage\""));
    CHECK_FALSE(compact.contains("\"board\""));
    CHECK_FALSE(compact.contains("\"effect_activations\""));
    CHECK(full.contains("\"detail\":\"full\""));
    CHECK(full.contains("\"initial_board\""));
    CHECK(full.contains("\"effect_activations\""));
    CHECK(full.contains("\"initial_combat_stats\""));

    InspectLastBattleParams focused{};
    focused.sections = std::vector<std::string>{"unit_stats", "death_order"};
    focused.unit_ids = {1};
    focused.metrics = std::vector<std::string>{"damage_dealt", "invulnerability_triggers"};
    REQUIRE(validBattleReportParams(focused));
    const auto focusedReport = inspectLastBattleDto(session, BattleReportDetail::Summary, focused);
    REQUIRE(focusedReport);
    REQUIRE(focusedReport->unit_stats);
    REQUIRE(focusedReport->unit_stats->size() == 1);
    CHECK(focusedReport->unit_stats->front().str.contains("\"unit_id\":1"));
    CHECK(focusedReport->unit_stats->front().str.contains("\"invulnerability_triggers\""));
    CHECK_FALSE(focusedReport->unit_stats->front().str.contains("\"damage_taken\""));
    CHECK_FALSE(focusedReport->survivors);
    CHECK_FALSE(focusedReport->initial_board);
    REQUIRE(focusedReport->death_order);
    for (const auto& death : *focusedReport->death_order) CHECK(death.unit_id == 1);
    CHECK(writeJson(*focusedReport).size() < summary.size());
    focused.metrics = std::vector<std::string>{"not_a_metric"};
    CHECK_FALSE(validBattleReportParams(focused));
    focused.metrics.reset();
    focused.sections = std::vector<std::string>{"not_a_section"};
    CHECK_FALSE(validBattleReportParams(focused));

    InspectLastBattleParams effects{};
    effects.sections = std::vector<std::string>{"effect_activations"};
    effects.effect_types = {"ability_cast"};
    const auto effectReport = inspectLastBattleDto(session, BattleReportDetail::Summary, effects);
    REQUIRE(effectReport);
    REQUIRE(effectReport->effect_activations);
    REQUIRE_FALSE(effectReport->effect_activations->empty());
    for (const auto& event : *effectReport->effect_activations) CHECK(event.type == "ability_cast");
    CHECK_FALSE(effectReport->unit_stats);

    BattleEventsParams pageParams;
    pageParams.limit = 1;
    const auto compactPage = inspectLastBattleEventsDto(
        session,
        pageParams,
        BattleEventDetail::Compact);
    REQUIRE(compactPage);
    REQUIRE(compactPage->total_matching_count > 0);
    REQUIRE(compactPage->events.size() == 1);
    CHECK_FALSE(compactPage->events.front().description);
    CHECK(compactPage->next_cursor == 1);

    const auto fullPage = inspectLastBattleEventsDto(
        session,
        pageParams,
        BattleEventDetail::Full);
    REQUIRE(fullPage);
    REQUIRE(fullPage->events.size() == 1);
    CHECK(fullPage->events.front().description);

    REQUIRE(fullDto->effect_activations);
    const auto filterableEvent = std::ranges::find_if(
        *fullDto->effect_activations,
        [](const BattleEffectActivationDto& event) {
            return event.source_unit_id || event.target_unit_id;
        });
    REQUIRE(filterableEvent != fullDto->effect_activations->end());
    const int unitId = filterableEvent->source_unit_id
        ? *filterableEvent->source_unit_id
        : *filterableEvent->target_unit_id;
    BattleEventsParams unitParams;
    unitParams.unit_ids = {unitId};
    const auto unitFiltered = inspectLastBattleEventsDto(
        session,
        unitParams,
        BattleEventDetail::Compact);
    REQUIRE(unitFiltered);
    REQUIRE(unitFiltered->total_matching_count > 0);
    for (const auto& event : unitFiltered->events)
    {
        CHECK((event.source_unit_id == unitId || event.target_unit_id == unitId));
    }

    const int frame = fullDto->effect_activations->front().frame;
    BattleEventsParams frameParams;
    frameParams.frame_range = BattleEventsParams::FrameRange{frame, frame};
    const auto frameFiltered = inspectLastBattleEventsDto(
        session,
        frameParams,
        BattleEventDetail::Compact);
    REQUIRE(frameFiltered);
    REQUIRE(frameFiltered->total_matching_count > 0);
    for (const auto& event : frameFiltered->events)
    {
        CHECK(event.frame == frame);
    }

    BattleEventsParams filteredParams;
    filteredParams.effect_types = {"不存在的效果"};
    const auto filtered = inspectLastBattleEventsDto(
        session,
        filteredParams,
        BattleEventDetail::Compact);
    REQUIRE(filtered);
    CHECK(filtered->total_matching_count == 0);
    CHECK(filtered->events.empty());
}

TEST_CASE("JSON codec preserves the stable semantic event wire projection",
          "[chess][json-codec][events]")
{
    const auto content = managementContent();
    const ChessSemanticEvent equipment{
        ChessSemanticEventType::EquipmentAssigned,
        11,
        2,
        100,
    };
    const auto equipmentJson = writeJson(semanticEventDto(*content, equipment));
    CHECK(equipmentJson.contains("\"type\":\"equipment_assigned\""));
    CHECK(equipmentJson.contains("\"primary_id\":11"));
    CHECK(equipmentJson.contains("\"secondary_id\":2"));
    CHECK(equipmentJson.contains("\"value\":100"));

    const ChessSemanticEvent reward{
        ChessSemanticEventType::RewardChosen,
        {},
        {},
        {},
        "equipment:102",
    };
    const auto rewardJson = writeJson(semanticEventDto(*content, reward));
    CHECK(rewardJson.contains("\"type\":\"reward_chosen\""));
    CHECK(rewardJson.contains("\"primary_id\":0"));
    CHECK(rewardJson.contains("\"secondary_id\":0"));
    CHECK(rewardJson.contains("\"value\":0"));
    CHECK(rewardJson.contains("\"stable_id\":\"equipment:102\""));
}
