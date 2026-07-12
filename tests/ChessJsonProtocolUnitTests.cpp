#include "ChessGameSessionTestHelpers.h"
#include "ChessJsonProtocol.h"
#include "ChessReplayJson.h"

#include <catch2/catch_test_macros.hpp>
#include <glaze/json.hpp>

#include <algorithm>
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
    CHECK_FALSE(observed.result->str.contains("strategist"));
    SessionObservationView sessionObservation;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    REQUIRE_FALSE(glz::read<options>(sessionObservation, observed.result->str));
    for (const auto* operation : {"inspect_save", "load_game", "export_save", "import_save"})
    {
        CAPTURE(operation);
        CHECK(std::ranges::contains(sessionObservation.operations, std::string(operation)));
    }
    CHECK(sessionObservation.load_consequence.contains("替換目前遊戲狀態"));
    CHECK(sessionObservation.load_consequence.contains("亂數"));
    CHECK(sessionObservation.load_consequence.contains("完整重播紀錄"));
    CHECK(sessionObservation.load_consequence.contains("捨棄"));
    CHECK(sessionObservation.load_consequence.contains("保留存檔目錄"));
    CHECK(protocol.session()->observe().stateHash == stateHash);
}

TEST_CASE("JSON protocol act matches direct session execution", "[chess][protocol][determinism]")
{
    auto content = managementContent();
    ChessJsonProtocol protocol(content);
    REQUIRE(parseResponse(protocol.handleLine(
        R"({"id":1,"method":"new","params":{"difficulty":"normal","seed":"0x0000000000000007"}})")).ok);
    ChessGameSession direct(content, 7);

    const auto response = parseResponse(protocol.handleLine(
        R"({"id":2,"method":"act","params":{"action":{"type":"set_shop_locked","locked":true}}})"));
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
    CHECK(actionResult.next_observation.shop_locked);
    REQUIRE(protocol.session());
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
