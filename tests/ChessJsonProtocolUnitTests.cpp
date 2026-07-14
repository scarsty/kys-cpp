#include "ChessGameSessionTestHelpers.h"
#include "ChessJsonProtocol.h"
#include "ChessReplayJson.h"
#include "ChessRewardRules.h"
#include "ChessSessionCheckpoint.h"

#include <catch2/catch_test_macros.hpp>
#include <glaze/json.hpp>

#include <algorithm>
#include <format>
#include <ranges>
#include <vector>

using namespace KysChess;
using namespace KysChess::Test;

namespace TestProtocolDetail
{

struct ResponseView
{
    glz::raw_json id;
    bool ok{};
    std::optional<glz::raw_json> result;
    std::optional<std::string> error_code;
    std::optional<std::string> error_message;
};

struct ExportView { std::string replay_jsonl; };
struct VerifyRequest
{
    glz::raw_json id = "4";
    std::string method = "verify_replay";
    struct Params { std::string replay_jsonl; } params;
};
struct VerifyView { bool valid{}; };
struct ActionEventView { std::string type; int value{}; };
struct ActionObservationView { std::string phase; bool shop_locked{}; };
struct ActionResultView
{
    bool accepted{};
    std::vector<ActionEventView> events;
    ActionObservationView next_observation;
};
struct TimelineView { std::uint64_t discarded_active_actions{}; std::uint64_t restored_sequence{}; };
struct SavePayloadView { std::string payload; };
struct SessionObservationView
{
    std::vector<std::string> operations;
    std::string load_consequence;
};
struct ImportSaveParams { std::string slot; std::string payload; };
struct ImportSaveRequest { glz::raw_json id = "8"; std::string method = "import_save"; ImportSaveParams params; };

ResponseView parseResponse(const std::string& json)
{
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    ResponseView response;
    REQUIRE_FALSE(glz::read<options>(response, json));
    return response;
}

int substringCount(std::string_view text, std::string_view needle)
{
    int count{};
    for (auto position = text.find(needle);
         position != std::string_view::npos;
         position = text.find(needle, position + needle.size()))
    {
        ++count;
    }
    return count;
}

}

using namespace TestProtocolDetail;

TEST_CASE("JSON protocol preserves request identifiers and session state", "[chess][protocol]")
{
    ChessJsonProtocol protocol(managementContent());
    const auto created = parseResponse(protocol.handleLine(
        R"({"id":"request-1","method":"new","params":{"difficulty":"normal","seed":"0x0000000000003039"}})"));

    REQUIRE(created.ok);
    CHECK(created.id.str == R"("request-1")");
    REQUIRE(protocol.session());
    const auto stateHash = protocol.session()->observe().stateHash;

    const auto observed = parseResponse(protocol.handleLine(R"({"id":42,"method":"observe","params":{}})"));
    CHECK(observed.ok);
    CHECK(observed.id.str == "42");
    REQUIRE(observed.result);
    CHECK(observed.result->str.contains("\"free_shop_refresh_available\""));
    CHECK(observed.result->str.contains("\"free_shop_refresh_granted_fight\""));
    CHECK(observed.result->str.contains("\"interest_gold\""));
    CHECK(observed.result->str.contains("\"maximum_interest_gold\""));
    CHECK(observed.result->str.contains("\"total_campaign_rounds\":28"));
    CHECK(observed.result->str.contains("\"boss_interval\":4"));
    CHECK(observed.result->str.contains("\"forced_bans_enabled\":false"));
    CHECK(observed.result->str.contains("\"projected_victory_income\""));
    CHECK(observed.result->str.contains("\"current_ban_count\""));
    CHECK(observed.result->str.contains("\"maximum_ban_count\""));
    CHECK(observed.result->str.contains("\"remaining_ban_capacity\""));
    CHECK(observed.result->str.contains("禁棋只影響之後生成或刷新的商店"));
    CHECK(observed.result->str.contains("\"relevant_roles\""));
    CHECK(observed.result->str.contains("\"name\""));
    CHECK(observed.result->str.contains("\"base_stats\""));
    CHECK(observed.result->str.contains("\"abilities\""));
    CHECK(observed.result->str.contains("\"combos\":[]"));
    CHECK_FALSE(observed.result->str.contains("strategist"));
    SessionObservationView sessionObservation;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(sessionObservation, observed.result->str));
    for (const auto* operation : {
             "inspect_shop_slot",
             "inspect_shop",
             "get_shop_odds",
             "inspect_chess_instance",
             "inspect_bans",
             "inspect_save",
             "load_game",
             "export_save",
             "import_save",
         })
    {
        CAPTURE(operation);
        CHECK(std::ranges::contains(sessionObservation.operations, std::string(operation)));
    }
    CHECK_FALSE(std::ranges::contains(sessionObservation.operations, std::string("verify_replay")));
    CHECK(sessionObservation.load_consequence.contains("替換目前遊戲狀態"));
    CHECK(sessionObservation.load_consequence.contains("亂數"));
    CHECK(sessionObservation.load_consequence.contains("完整重播紀錄"));
    CHECK(sessionObservation.load_consequence.contains("捨棄"));
    CHECK(sessionObservation.load_consequence.contains("保留存檔目錄"));
    CHECK(protocol.session()->observe().stateHash == stateHash);
}

TEST_CASE("JSON protocol inspects authoritative challenge stars and equipment",
          "[chess][protocol][challenge][actual-config]")
{
    const auto content = actualContent();
    REQUIRE(content);
    ChessJsonProtocol protocol(content);
    const auto created = parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x000000000000000e"}})"));
    REQUIRE(created.ok);
    REQUIRE(created.result);
    CHECK(created.result->str.contains("\"total_campaign_rounds\":28"));
    CHECK(created.result->str.contains("\"forced_bans_enabled\":true"));

    const auto inspected = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"inspect_challenge","params":{"challenge_name":"倚天屠龍"}})"));

    REQUIRE(inspected.ok);
    REQUIRE(inspected.result);
    CHECK(inspected.result->str.contains("\"enemy_count\":12"));
    CHECK(substringCount(inspected.result->str, "\"star\":3") == 12);
    CHECK(substringCount(inspected.result->str, "\"weapon\":{") == 12);
    CHECK(substringCount(inspected.result->str, "\"armor\":{") == 12);
    CHECK(inspected.result->str.contains("\"name\":\"屠龍刀\""));
    CHECK(inspected.result->str.contains("\"name\":\"倚天劍\""));

    const auto easyContent = actualContent(Difficulty::Easy);
    REQUIRE(easyContent);
    ChessJsonProtocol easyProtocol(easyContent);
    const auto easy = parseResponse(easyProtocol.handleLine(
        R"({"id":3,"method":"new","params":{"difficulty":"easy","seed":"0x000000000000000e"}})"));
    REQUIRE(easy.ok);
    REQUIRE(easy.result);
    CHECK(easy.result->str.contains("\"total_campaign_rounds\":28"));
    CHECK(easy.result->str.contains("\"forced_bans_enabled\":false"));
}

TEST_CASE("JSON protocol compact prepared battle omits unavailable unit metadata",
          "[chess][protocol][compact][prepared]")
{
    ChessJsonProtocol protocol(configuredMapChoiceContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x000000000000000f"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":0}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":1}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":2}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":5,"method":"act","params":{"action":{"type":"set_deployment","chess_instance_ids":[4]}}})")).ok);
    const auto prepared = parseResponse(protocol.handleLine(
        R"({"id":6,"method":"act","params":{"detail":"compact","action":{"type":"prepare_battle"}}})"));

    REQUIRE(prepared.ok);
    REQUIRE(prepared.result);
    CHECK(prepared.result->str.contains("\"metadata_scope\":\"omitted_compact\""));
    CHECK_FALSE(prepared.result->str.contains("\"preview_stats\""));
    CHECK_FALSE(prepared.result->str.contains("\"stats_note\""));
    CHECK_FALSE(prepared.result->str.contains("\"abilities\""));
}

TEST_CASE("JSON protocol compact observation stays lean for a developed roster",
          "[chess][protocol][compact][size]")
{
    const auto content = actualContent();
    REQUIRE(content);
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000017"}})")).ok);
    auto* session = const_cast<ChessGameSession*>(protocol.session());
    REQUIRE(session);
    auto checkpoint = ChessSessionCheckpoint::capture(*session, 1);
    checkpoint.state.roster.clear();
    int instanceId = 1;
    for (const int roleId : content->poolRoleIds() | std::views::take(9))
    {
        checkpoint.state.roster.emplace(
            instanceId,
            ChessSessionPiece{instanceId, roleId, 3, true});
        ++instanceId;
    }
    checkpoint.state.nextChessInstanceId = instanceId;
    REQUIRE(checkpoint.restore(*session) == ChessCheckpointError::None);

    const auto compact = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"observe","params":{"detail":"compact"}})"));
    const auto full = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"observe","params":{"detail":"full"}})"));

    REQUIRE(compact.ok);
    REQUIRE(compact.result);
    REQUIRE(full.ok);
    REQUIRE(full.result);
    CHECK(compact.result->str.size() < 12000);
    CHECK(compact.result->str.size() * 3 < full.result->str.size());
    for (const auto* omitted : {
             "\"current_stats\"",
             "\"count_explanation\"",
             "\"contributions\"",
             "\"thresholds\"",
             "\"relevant_roles\"",
             "\"base_stats\"",
             "\"abilities\"",
         })
    {
        CAPTURE(omitted);
        CHECK_FALSE(compact.result->str.contains(omitted));
    }
    CHECK(compact.result->str.contains("\"legal_action_types\""));
}

TEST_CASE("JSON protocol emits semantic merge and enemy-reroll events", "[chess][protocol][events]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x000000000000000b"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":0}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":1}}})")).ok);
    const auto merged = parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"detail":"full","action":{"type":"buy_shop_slot","slot":2}}})"));
    REQUIRE(merged.ok);
    REQUIRE(merged.result);
    CHECK(merged.result->str.contains("\"type\":\"chess_merged\""));
    CHECK(merged.result->str.contains("\"role_id\":10"));
    CHECK(merged.result->str.contains("\"role_name\":\"測試棋子\""));
    CHECK(merged.result->str.contains("\"consumed_instance_ids\":[1,2,3]"));
    CHECK(merged.result->str.contains("\"new_instance_id\":4"));
    CHECK(merged.result->str.contains("\"result_star\":2"));

    const auto rerolled = parseResponse(protocol.handleLine(
        R"({"id":5,"method":"act","params":{"detail":"full","action":{"type":"reroll_enemy_seed"}}})"));
    REQUIRE(rerolled.ok);
    REQUIRE(rerolled.result);
    CHECK(rerolled.result->str.contains("\"type\":\"enemy_plan_rerolled\""));
    CHECK(rerolled.result->str.contains("\"cost\":3"));
    CHECK(rerolled.result->str.contains("\"previous_enemy_plan_key\":\"0x"));
    CHECK(rerolled.result->str.contains("\"new_enemy_plan_key\":\"0x"));
    CHECK(rerolled.result->str.contains("重新生成敵方陣容"));
}

TEST_CASE("JSON protocol keeps compact forced-ban and rejected-action responses focused",
          "[chess][protocol][compact][reward]")
{
    const auto content = managementContent();
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x000000000000000d"}})")).ok);
    auto* session = const_cast<ChessGameSession*>(protocol.session());
    REQUIRE(session);
    auto checkpoint = ChessSessionCheckpoint::capture(*session, 1);
    std::vector<ChessSemanticEvent> setupEvents;
    ChessRewardRules::enqueueForcedBan(checkpoint.state, *content, 1, 1, setupEvents);
    REQUIRE(checkpoint.restore(*session) == ChessCheckpointError::None);

    const auto compact = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"observe","params":{"detail":"compact"}})"));
    REQUIRE(compact.ok);
    REQUIRE(compact.result);
    const auto pendingStart = compact.result->str.find("\"pending_reward\":{");
    REQUIRE(pendingStart != std::string::npos);
    const auto pendingJson = compact.result->str.substr(pendingStart);
    CHECK(pendingJson.contains("\"kind\":\"禁棋\""));
    CHECK(pendingJson.contains("\"option_count\":1"));
    CHECK(pendingJson.contains("\"option_count_description\""));
    CHECK(pendingJson.contains("\"maximum_selections\":1"));
    CHECK(pendingJson.contains("\"remaining_selections\":1"));
    CHECK(pendingJson.contains("\"selection_optional\":true"));
    CHECK(pendingJson.contains("強制先完成禁棋決策"));
    CHECK(pendingJson.contains("\"eligible_tiers\":[1]"));
    CHECK_FALSE(pendingJson.contains("\"options\""));

    const auto rejected = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"detail":"compact","action":{"type":"buy_shop_slot","slot":0}}})"));
    REQUIRE(rejected.ok);
    REQUIRE(rejected.result);
    CHECK(rejected.result->str.contains("\"accepted\":false"));
    CHECK(rejected.result->str.contains("\"error_code\":\"wrong_phase\""));
    CHECK(rejected.result->str.contains("\"phase\":\"RewardChoice\""));
    CHECK(rejected.result->str.contains("\"legal_action_types\":[\"add_ban\",\"skip_forced_bans\"]"));
    CHECK(rejected.result->str.contains("\"state_hash\""));
    CHECK_FALSE(rejected.result->str.contains("\"shop\""));
    CHECK_FALSE(rejected.result->str.contains("\"pending_reward\""));
    CHECK_FALSE(rejected.result->str.contains("\"replay_sequence\""));

    const auto fullRejected = parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"detail":"full","action":{"type":"buy_shop_slot","slot":0}}})"));
    REQUIRE(fullRejected.ok);
    REQUIRE(fullRejected.result);
    CHECK(fullRejected.result->str.contains("\"pending_reward\""));
    CHECK(fullRejected.result->str.contains("\"options\""));
}

TEST_CASE("JSON protocol distinguishes omitted equipment metadata and assigned candidates",
          "[chess][protocol][equipment]")
{
    auto data = ChessGameContentData{};
    data.difficulty = Difficulty::Normal;
    data.balance.initialMoney = 10;
    data.balance.shopSlotCount = 1;
    for (auto& row : data.balance.shopWeights) row = {100, 0, 0, 0, 0};
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "裝備測試棋子";
    role.Cost = 1;
    data.roles.emplace(role.ID, role);
    data.poolRoleIds = {role.ID};
    data.items.emplace(100, ChessItemDefinition{100, -1, 0, 1, 0, 10, 0, 0, 0, 0, 0, 0, 0, "已裝備之劍"});
    data.items.emplace(200, ChessItemDefinition{200, -1, 0, 1, 0, 20, 0, 0, 0, 0, 0, 0, 0, "未分配之劍"});
    EquipmentDef assignedEquipment{100, 1, 0};
    assignedEquipment.actAsComboNames = {"裝備羈絆"};
    data.equipment = {assignedEquipment, {200, 2, 0}};
    ComboDef combo;
    combo.id = 0;
    combo.name = "裝備羈絆";
    combo.memberRoleIds = {10};
    combo.thresholds.push_back({2, "雙人", {}});
    data.combos.push_back(std::move(combo));
    auto content = std::make_shared<const ChessGameContent>(std::move(data));
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x000000000000000c"}})")).ok);
    auto* session = const_cast<ChessGameSession*>(protocol.session());
    REQUIRE(session);
    auto checkpoint = ChessSessionCheckpoint::capture(*session, 1);
    checkpoint.state.roster.clear();
    checkpoint.state.roster.emplace(1, ChessSessionPiece{1, 10, 1, true, 1});
    checkpoint.state.equipmentInventory.clear();
    checkpoint.state.equipmentInventory.emplace(1, ChessEquipmentInstance{1, 100, 1});
    checkpoint.state.equipmentInventory.emplace(2, ChessEquipmentInstance{2, 200, -1});
    checkpoint.state.nextChessInstanceId = 2;
    checkpoint.state.nextEquipmentInstanceId = 3;
    REQUIRE(checkpoint.restore(*session) == ChessCheckpointError::None);

    const auto compact = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"observe","params":{"detail":"compact"}})"));
    REQUIRE(compact.ok);
    REQUIRE(compact.result);
    CHECK(compact.result->str.contains("\"name\":\"已裝備之劍\""));
    CHECK_FALSE(compact.result->str.contains("\"equipment_metadata_scope\""));
    CHECK_FALSE(compact.result->str.contains("\"current_stats\""));
    CHECK_FALSE(compact.result->str.contains("\"current_stats_note\""));
    CHECK_FALSE(compact.result->str.contains("\"base_stat_effects\""));
    CHECK_FALSE(compact.result->str.contains("\"special_effects\""));

    const auto legal = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"legal_actions","params":{}})"));
    REQUIRE(legal.ok);
    REQUIRE(legal.result);
    const auto equipStart = legal.result->str.find("\"type\":\"equip\"");
    REQUIRE(equipStart != std::string::npos);
    const auto equipJson = legal.result->str.substr(equipStart);
    const auto unassignedPosition = equipJson.find("\"id\":2");
    const auto assignedPosition = equipJson.find("\"id\":1");
    REQUIRE(unassignedPosition != std::string::npos);
    REQUIRE(assignedPosition != std::string::npos);
    CHECK(unassignedPosition < assignedPosition);
    CHECK(equipJson.contains("\"assigned_chess_instance_id\":-1"));
    CHECK(equipJson.contains("\"assigned_chess_instance_id\":1"));
    CHECK(equipJson.contains("\"assigned_to\":\"裝備測試棋子（棋子實例 1）\""));
    CHECK(equipJson.contains("\"example\":{\"type\":\"equip\",\"equipment_instance_id\":2"));

    auto assignedOnly = ChessSessionCheckpoint::capture(*session, 2);
    assignedOnly.state.roster.emplace(2, ChessSessionPiece{2, 10, 1});
    assignedOnly.state.equipmentInventory.erase(2);
    assignedOnly.state.nextChessInstanceId = 3;
    REQUIRE(assignedOnly.restore(*session) == ChessCheckpointError::None);
    const auto reassignmentLegal = parseResponse(protocol.handleLine(
        R"({"id":4,"method":"legal_actions","params":{}})"));
    REQUIRE(reassignmentLegal.ok);
    REQUIRE(reassignmentLegal.result);
    const auto reassignmentStart = reassignmentLegal.result->str.find("\"type\":\"equip\"");
    REQUIRE(reassignmentStart != std::string::npos);
    const auto reassignmentJson = reassignmentLegal.result->str.substr(reassignmentStart);
    CHECK(reassignmentJson.contains(
        "\"example\":{\"type\":\"equip\",\"equipment_instance_id\":1,\"target_chess_instance_id\":2}"));
    CHECK(reassignmentJson.contains(
        "\"example_note\":\"範例會把目前已裝備的項目移給另一名棋子\""));

    const auto equipmentInfo = parseResponse(protocol.handleLine(
        R"({"id":5,"method":"inspect_equipment","params":{"item_id":100}})"));
    REQUIRE(equipmentInfo.ok);
    REQUIRE(equipmentInfo.result);
    CHECK(equipmentInfo.result->str.contains("\"combo_counting_note\""));

    const auto comboInfo = parseResponse(protocol.handleLine(
        R"({"id":6,"method":"inspect_combo","params":{"combo_name":"裝備羈絆"}})"));
    REQUIRE(comboInfo.ok);
    REQUIRE(comboInfo.result);
    CHECK(comboInfo.result->str.contains("\"physical_count\":1"));
    CHECK(comboInfo.result->str.contains("\"effective_count\":1"));
    CHECK(comboInfo.result->str.contains("\"natural_member\":true"));
    CHECK(comboInfo.result->str.contains("\"equipment_sources\":[{\"id\":100,\"name\":\"已裝備之劍\"}]"));
    CHECK(comboInfo.result->str.contains("不重複加點"));
}

TEST_CASE("JSON protocol groups large equipment choices without filtering mechanics",
          "[chess][protocol][reward][equipment]")
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "獎勵棋子";
    role.Cost = 1;
    data.roles.emplace(role.ID, role);
    data.poolRoleIds = {role.ID};
    for (int index = 0; index < 13; ++index)
    {
        const int itemId = 100 + index;
        const int tier = index < 7 ? 1 : 2;
        const int type = index % 2;
        data.items.emplace(itemId, ChessItemDefinition{
            itemId,
            -1,
            type,
            1,
            0,
            index + 1,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            std::format("獎勵裝備{}", index),
        });
        data.equipment.push_back({itemId, tier, type});
    }
    auto content = std::make_shared<const ChessGameContent>(std::move(data));
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000010"}})")).ok);
    auto* session = const_cast<ChessGameSession*>(protocol.session());
    REQUIRE(session);
    auto checkpoint = ChessSessionCheckpoint::capture(*session, 1);
    ChessPendingReward pending;
    pending.id = "大量裝備";
    pending.kind = ChessRewardKind::Equipment;
    for (const auto& equipment : content->equipment())
    {
        pending.options.push_back({
            std::format("equipment:{}", equipment.itemId),
            ChessRewardKind::Equipment,
            equipment.itemId,
        });
    }
    checkpoint.state.pendingRewards = {std::move(pending)};
    checkpoint.state.phase = ChessSessionPhase::RewardChoice;
    REQUIRE(checkpoint.restore(*session) == ChessCheckpointError::None);

    const auto compact = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"observe","params":{"detail":"compact"}})"));
    REQUIRE(compact.ok);
    REQUIRE(compact.result);
    const auto pendingStart = compact.result->str.find("\"pending_reward\":{");
    REQUIRE(pendingStart != std::string::npos);
    const auto pendingJson = compact.result->str.substr(pendingStart);
    CHECK(pendingJson.contains("\"option_count\":13"));
    CHECK(pendingJson.contains("\"option_groups\""));
    CHECK_FALSE(pendingJson.contains("\"options\""));

    const auto legal = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"legal_actions","params":{}})"));
    REQUIRE(legal.ok);
    REQUIRE(legal.result);
    CHECK(substringCount(legal.result->str, "\"value\":\"equipment:") == 13);
    CHECK(legal.result->str.contains("\"group\":\"1階武器\""));
    CHECK(legal.result->str.contains("\"group\":\"2階防具\""));
}

TEST_CASE("JSON protocol publishes action schemas and explains malformed payloads", "[chess][protocol][actions]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000007"}})")).ok);

    const auto legal = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"legal_actions","params":{}})"));
    REQUIRE(legal.ok);
    REQUIRE(legal.result);
    CHECK(legal.result->str.contains("\"action_schema\""));
    CHECK(legal.result->str.contains("\"chess_instance_ids\":\"整數陣列\""));
    CHECK(legal.result->str.contains("\"example\""));

    const auto invalid = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"action":{"type":"set_deployment","ids":[1]}}})"));
    CHECK_FALSE(invalid.ok);
    CHECK(invalid.error_code == "invalid_action");
    REQUIRE(invalid.error_message);
    CHECK(invalid.error_message->contains("chess_instance_ids"));
    CHECK(invalid.error_message->contains("範例"));
}

TEST_CASE("JSON protocol summary actions return changes without embedding observation metadata",
          "[chess][protocol][actions][summary]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000015"}})")).ok);

    const auto summary = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"refresh_shop"}}})"));

    REQUIRE(summary.ok);
    REQUIRE(summary.result);
    CHECK(summary.result->str.size() < 2000);
    CHECK(summary.result->str.contains("\"accepted\":true"));
    CHECK(summary.result->str.contains("\"events\":[\"shop_refreshed\"]"));
    CHECK(summary.result->str.contains("\"money\":-2"));
    CHECK(summary.result->str.contains("\"shop_changed\":true"));
    CHECK(summary.result->str.contains("\"phase\":\"Management\""));
    CHECK(summary.result->str.contains("\"state_hash\""));
    CHECK_FALSE(summary.result->str.contains("\"next_observation\""));
    CHECK_FALSE(summary.result->str.contains("\"relevant_roles\""));
    CHECK_FALSE(summary.result->str.contains("\"current_stats\""));
    CHECK_FALSE(summary.result->str.contains("\"contributions\""));
    CHECK_FALSE(summary.result->str.contains("\"replay_sequence\""));
}

TEST_CASE("JSON protocol battle preparation and start summaries stay bounded",
          "[chess][protocol][actions][summary][battle]")
{
    ChessJsonProtocol protocol(configuredMapChoiceContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000018"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":0}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":1}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":2}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":5,"method":"act","params":{"action":{"type":"set_deployment","chess_instance_ids":[4]}}})")).ok);

    const auto prepared = parseResponse(protocol.handleLine(
        R"({"id":6,"method":"act","params":{"action":{"type":"prepare_battle"}}})"));
    REQUIRE(prepared.ok);
    REQUIRE(prepared.result);
    CHECK(prepared.result->str.size() < 3000);
    CHECK(prepared.result->str.contains("\"events\":[\"battle_prepared\"]"));
    CHECK_FALSE(prepared.result->str.contains("\"next_observation\""));
    CHECK_FALSE(prepared.result->str.contains("\"prepared_battle\""));

    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":7,"method":"act","params":{"action":{"type":"choose_map","map_id":7}}})")).ok);
    const auto started = parseResponse(protocol.handleLine(
        R"({"id":8,"method":"act","params":{"action":{"type":"start_battle"}}})"));
    REQUIRE(started.ok);
    REQUIRE(started.result);
    CHECK(started.result->str.size() < 4000);
    CHECK(started.result->str.contains("\"battle\":{"));
    CHECK(started.result->str.contains("\"outcome\""));
    CHECK_FALSE(started.result->str.contains("\"next_observation\""));
    CHECK_FALSE(started.result->str.contains("\"unit_stats\""));
    CHECK_FALSE(started.result->str.contains("\"effect_activations\""));
    CHECK_FALSE(started.result->str.contains("\"initial_board\""));
}

TEST_CASE("JSON protocol focused shop instance odds and ban inspections provide decision context",
          "[chess][protocol][inspect][management]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000016"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":0}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":1}}})")).ok);

    const auto slot = parseResponse(protocol.handleLine(
        R"({"id":4,"method":"inspect_shop_slot","params":{"slot":2}})"));
    REQUIRE(slot.ok);
    REQUIRE(slot.result);
    CHECK(slot.result->str.contains("\"owned_copies\":2"));
    CHECK(slot.result->str.contains("\"purchase_result\":\"merge_to_2_star\""));
    CHECK(slot.result->str.contains("\"level_tier_probability\":1"));
    CHECK(slot.result->str.contains("\"gold_cost\":1"));

    const auto shop = parseResponse(protocol.handleLine(
        R"({"id":5,"method":"inspect_shop","params":{}})"));
    REQUIRE(shop.ok);
    REQUIRE(shop.result);
    CHECK(shop.result->str.contains("\"slots\""));
    CHECK(shop.result->str.contains("\"odds\""));

    const auto odds = parseResponse(protocol.handleLine(
        R"({"id":6,"method":"get_shop_odds","params":{}})"));
    REQUIRE(odds.ok);
    REQUIRE(odds.result);
    CHECK(odds.result->str.contains("\"tier\":1"));
    CHECK(odds.result->str.contains("\"probability\":1"));
    CHECK(odds.result->str.contains("\"available_role_count\":1"));

    const auto instance = parseResponse(protocol.handleLine(
        R"({"id":7,"method":"inspect_chess_instance","params":{"chess_instance_id":1}})"));
    REQUIRE(instance.ok);
    REQUIRE(instance.result);
    CHECK(instance.result->str.contains("\"current_stats\""));
    CHECK(instance.result->str.contains("\"same_star_copies\":2"));
    CHECK(instance.result->str.contains("\"copies_required_for_next_star\":1"));

    const auto bans = parseResponse(protocol.handleLine(
        R"({"id":8,"method":"inspect_bans","params":{}})"));
    REQUIRE(bans.ok);
    REQUIRE(bans.result);
    CHECK(bans.result->str.contains("\"current_ban_count\""));
    CHECK(bans.result->str.contains("\"remaining_ban_capacity\""));
    CHECK(bans.result->str.contains("禁棋只影響之後生成或刷新的商店"));
}

TEST_CASE("JSON protocol publishes economic previews and keeps verification hashes in full detail",
          "[chess][protocol][actions][economy]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000013"}})")).ok);

    const auto legal = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"legal_actions","params":{}})"));
    REQUIRE(legal.ok);
    REQUIRE(legal.result);
    const auto actionJson = [&](std::string_view type) {
        const auto marker = std::format("\"type\":\"{}\"", type);
        const auto start = legal.result->str.find(marker);
        REQUIRE(start != std::string::npos);
        const auto next = legal.result->str.find("},{\"type\":", start + marker.size());
        return legal.result->str.substr(start, next - start);
    };
    const auto refresh = actionJson("refresh_shop");
    CHECK(refresh.contains("\"gold_cost\":2"));
    CHECK(refresh.contains("\"projected_gold_after\":98"));
    CHECK(refresh.contains("\"affected_option_count\":5"));
    const auto buyExp = actionJson("buy_exp");
    CHECK(buyExp.contains("\"gold_cost\":5"));
    CHECK(buyExp.contains("\"experience_gained\":5"));
    CHECK(buyExp.contains("\"projected_experience_after\":1"));
    CHECK(buyExp.contains("\"projected_level_after\":1"));
    const auto enemyReroll = actionJson("reroll_enemy_seed");
    CHECK(enemyReroll.contains("\"gold_cost\":3"));
    CHECK(enemyReroll.contains("\"projected_gold_after\":97"));

    const auto compact = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"act","params":{"detail":"compact","action":{"type":"set_shop_locked","locked":true}}})"));
    REQUIRE(compact.ok);
    REQUIRE(compact.result);
    CHECK(compact.result->str.contains("\"state_hash\""));
    CHECK_FALSE(compact.result->str.contains("\"last_battle_digest\""));
    CHECK_FALSE(compact.result->str.contains("\"pre_state_hash\""));
    CHECK_FALSE(compact.result->str.contains("\"post_state_hash\""));
    CHECK_FALSE(compact.result->str.contains("\"event_hash\""));
    CHECK_FALSE(compact.result->str.contains("\"rng_digest\""));
    CHECK_FALSE(compact.result->str.contains("\"chain_hash\""));

    const auto full = parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"detail":"full","action":{"type":"set_shop_locked","locked":false}}})"));
    REQUIRE(full.ok);
    REQUIRE(full.result);
    CHECK(full.result->str.contains("\"last_battle_digest\""));
    CHECK(full.result->str.contains("\"pre_state_hash\""));
    CHECK(full.result->str.contains("\"post_state_hash\""));
    CHECK(full.result->str.contains("\"event_hash\""));
    CHECK(full.result->str.contains("\"rng_digest\""));
    CHECK(full.result->str.contains("\"chain_hash\""));
}

TEST_CASE("JSON protocol previews reward reroll and legendary equipment costs",
          "[chess][protocol][actions][economy][actual-config]")
{
    const auto content = actualContent();
    REQUIRE(content);
    REQUIRE(content->balance().legendaryShop.unlockFight > 0);
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000014"}})")).ok);
    auto* session = const_cast<ChessGameSession*>(protocol.session());
    REQUIRE(session);

    auto checkpoint = ChessSessionCheckpoint::capture(*session, 1);
    checkpoint.state.money = 100;
    checkpoint.state.fight = content->balance().legendaryShop.unlockFight;
    REQUIRE(checkpoint.restore(*session) == ChessCheckpointError::None);
    const auto legendaryLegal = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"legal_actions","params":{}})"));
    REQUIRE(legendaryLegal.ok);
    REQUIRE(legendaryLegal.result);
    const auto legendaryStart = legendaryLegal.result->str.find("\"type\":\"buy_legendary_equipment\"");
    REQUIRE(legendaryStart != std::string::npos);
    const auto legendary = legendaryLegal.result->str.substr(legendaryStart);
    CHECK(legendary.contains(std::format(
        "\"gold_cost\":{}",
        content->balance().legendaryShop.price)));
    CHECK(legendary.contains(std::format(
        "\"projected_gold_after\":{}",
        100 - content->balance().legendaryShop.price)));
    CHECK(legendary.contains(std::format(
        "價格 {} 金幣",
        content->balance().legendaryShop.price)));

    auto rewardCheckpoint = ChessSessionCheckpoint::capture(*session, 2);
    rewardCheckpoint.state.phase = ChessSessionPhase::RewardChoice;
    rewardCheckpoint.state.pendingRewards.clear();
    ChessPendingReward pending;
    pending.id = "經濟預覽獎勵";
    pending.kind = ChessRewardKind::Equipment;
    pending.rerollCost = 7;
    pending.parameter = 4;
    pending.choiceCount = 2;
    for (const auto& equipment : content->equipment() | std::views::take(2))
    {
        pending.options.push_back({
            std::format("equipment:{}", equipment.itemId),
            ChessRewardKind::Equipment,
            equipment.itemId,
        });
    }
    REQUIRE(pending.options.size() == 2);
    rewardCheckpoint.state.pendingRewards.push_back(std::move(pending));
    REQUIRE(rewardCheckpoint.restore(*session) == ChessCheckpointError::None);
    const auto rewardLegal = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"legal_actions","params":{}})"));
    REQUIRE(rewardLegal.ok);
    REQUIRE(rewardLegal.result);
    const auto rerollStart = rewardLegal.result->str.find("\"type\":\"reroll_reward\"");
    REQUIRE(rerollStart != std::string::npos);
    const auto reroll = rewardLegal.result->str.substr(rerollStart);
    CHECK(reroll.contains("\"gold_cost\":7"));
    CHECK(reroll.contains("\"projected_gold_after\":93"));
    CHECK(reroll.contains("\"affected_option_count\":2"));
}

TEST_CASE("JSON protocol act matches direct session execution", "[chess][protocol][determinism]")
{
    auto content = managementContent();
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000007"}})")).ok);
    ChessGameSession direct(content, 7);

    const auto response = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"detail":"compact","action":{"type":"set_shop_locked","locked":true}}})"));
    ChessAction action;
    action.type = ChessActionType::SetShopLocked;
    action.value = true;
    const auto expected = direct.submitAndDrain(action);

    REQUIRE(response.ok);
    REQUIRE(response.result);
    ActionResultView actionResult;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(actionResult, response.result->str));
    CHECK(actionResult.accepted);
    REQUIRE(actionResult.events.size() == 1);
    CHECK(actionResult.events.front().type == "shop_lock_changed");
    CHECK(actionResult.events.front().value == 1);
    CHECK(actionResult.next_observation.phase == "Management");
    REQUIRE(protocol.session());
    CHECK(protocol.session()->state().shopLocked);
    CHECK(protocol.session()->observe().stateHash == expected.postStateHash);
    CHECK(protocol.session()->journal().chainHash() == expected.chainHash);
}

TEST_CASE("JSON protocol exports and verifies its active replay", "[chess][protocol][replay]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000009"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"buy_shop_slot","slot":0}}})")).ok);

    const auto exported = parseResponse(protocol.handleLine(R"({"id":3,"method":"export_replay","params":{}})"));
    REQUIRE(exported.ok);
    REQUIRE(exported.result);
    ExportView exportView;
    REQUIRE_FALSE(glz::read_json(exportView, exported.result->str));

    const auto request = glz::write_json(VerifyRequest{{"4"}, "verify_replay", {exportView.replay_jsonl}});
    REQUIRE(request);
    const auto verified = parseResponse(protocol.handleLine(request.value()));
    REQUIRE(verified.ok);
    REQUIRE(verified.result);
    VerifyView verifyView;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(verifyView, verified.result->str));
    CHECK(verifyView.valid);
}

TEST_CASE("JSON protocol verifies a replay without an active session", "[chess][protocol][replay]")
{
    const auto content = managementContent();
    ChessGameSession source(content, 10);
    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    REQUIRE(source.submitAndDrain(lock).accepted);
    const auto replay = source.exportReplay();
    REQUIRE(replay);
    std::optional<Difficulty> requestedDifficulty;
    ChessJsonProtocol protocol([&](Difficulty difficulty) {
        requestedDifficulty = difficulty;
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
    const auto request = glz::write_json(VerifyRequest{
        {"4"},
        "verify_replay",
        {serializeChessReplayJsonl(*replay)},
    });
    REQUIRE(request);

    const auto verified = parseResponse(protocol.handleLine(request.value()));

    REQUIRE(verified.ok);
    REQUIRE(verified.result);
    VerifyView view;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(view, verified.result->str));
    CHECK(view.valid);
    CHECK(requestedDifficulty == Difficulty::Normal);
    CHECK_FALSE(protocol.session());
}

TEST_CASE("JSON protocol reports malformed, missing-session, and unknown methods", "[chess][protocol]")
{
    ChessJsonProtocol protocol(managementContent());
    CHECK(parseResponse(protocol.handleLine("not json")).error_code == "malformed_request");
    CHECK(parseResponse(protocol.handleLine(R"({"id":1,"method":"observe","params":{}})")).error_code == "no_session");
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000001"}})")).ok);
    CHECK(parseResponse(protocol.handleLine(R"({"id":3,"method":"unknown","params":{}})")).error_code == "unknown_method");
}

TEST_CASE("JSON protocol exposes visible timeline replacement save operations", "[chess][protocol][save]")
{
    ChessJsonProtocol protocol(managementContent());
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000011"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"set_shop_locked","locked":true}}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":3,"method":"save_game","params":{"slot":"1","label":"鎖定"}})")).ok);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":4,"method":"act","params":{"action":{"type":"set_shop_locked","locked":false}}})")).ok);

    const auto loaded = parseResponse(protocol.handleLine(
        R"({"id":5,"method":"load_game","params":{"slot":"1"}})"));
    REQUIRE(loaded.ok);
    REQUIRE(loaded.result);
    TimelineView timeline;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(timeline, loaded.result->str));
    CHECK(timeline.discarded_active_actions == 1);
    CHECK(timeline.restored_sequence == 1);
    REQUIRE(protocol.session());
    CHECK(protocol.session()->state().shopLocked);
}

TEST_CASE("JSON protocol exports and imports a portable save without activation", "[chess][protocol][save]")
{
    const auto content = managementContent();
    int contentLoads{};
    ChessJsonProtocol protocol([&](Difficulty difficulty) {
        ++contentLoads;
        return difficulty == Difficulty::Normal ? content : nullptr;
    });
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000012"}})")).ok);
    CHECK(contentLoads == 1);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":2,"method":"save_game","params":{"slot":"source","label":"原始"}})")).ok);
    const auto exported = parseResponse(protocol.handleLine(
        R"({"id":3,"method":"export_save","params":{"slot":"source"}})"));
    REQUIRE(exported.ok);
    REQUIRE(exported.result);
    SavePayloadView payload;
    REQUIRE_FALSE(glz::read_json(payload, exported.result->str));
    const auto request = glz::write_json(ImportSaveRequest{{"8"}, "import_save", {"copy", payload.payload}});
    REQUIRE(request);
    CHECK(parseResponse(protocol.handleLine(request.value())).ok);
    CHECK(contentLoads == 1);
    CHECK(parseResponse(protocol.handleLine(
        R"({"id":9,"method":"inspect_save","params":{"slot":"copy"}})")).ok);
    REQUIRE(protocol.session());
    CHECK(protocol.session()->journal().decisions().empty());

    ChessCheckpointError checkpointError;
    auto incompatible = ChessSessionCheckpoint::parseJson(payload.payload, checkpointError);
    REQUIRE(incompatible);
    incompatible->gameVersion = "另一個遊戲版本";
    const auto incompatibleRequest = glz::write_json(ImportSaveRequest{
        {"10"},
        "import_save",
        {"incompatible", incompatible->serializeJson()},
    });
    REQUIRE(incompatibleRequest);
    const auto incompatibleResponse = parseResponse(protocol.handleLine(incompatibleRequest.value()));
    CHECK_FALSE(incompatibleResponse.ok);
    CHECK(incompatibleResponse.error_code == "save_game_version_mismatch");
    CHECK(contentLoads == 1);
}
