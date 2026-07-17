#include "ChessGameSessionTestHelpers.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"

#include <catch2/catch_test_macros.hpp>

#include <string_view>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

ChessReplay shortReplay()
{
    ChessGameSession session(managementContent(), 12345);
    ChessAction lock;
    lock.type = ChessActionType::SetShopLocked;
    lock.value = true;
    REQUIRE(session.submitAndDrain(lock).accepted);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    const auto replay = session.exportReplay();
    REQUIRE(replay);
    return *replay;
}

std::string replaceOnce(
    std::string value,
    std::string_view from,
    std::string_view to)
{
    const auto position = value.find(from);
    REQUIRE(position != std::string::npos);
    value.replace(position, from.size(), to);
    return value;
}

}

TEST_CASE("replay JSONL round trip preserves the authoritative records", "[chess][replay][json]")
{
    const auto expected = shortReplay();
    const auto jsonl = serializeChessReplayJsonl(expected);
    ChessReplayJsonError error;
    const auto parsed = parseChessReplayJsonl(jsonl, error);

    REQUIRE(parsed);
    CHECK(parsed->header.rootSeed == 12345);
    CHECK(parsed->header.gameVersion == expected.header.gameVersion);
    REQUIRE(parsed->decisions.size() == expected.decisions.size());
    CHECK(parsed->decisions[1].action.type == ChessActionType::BuyShopSlot);
    CHECK(parsed->decisions[1].action.shopSlot == 0);
    CHECK(parsed->footer.finalStateHash == expected.footer.finalStateHash);
    CHECK(jsonl.find("\"root_seed\":\"0x0000000000003039\"") != std::string::npos);
}

TEST_CASE("fresh session verifier accepts an unmodified replay", "[chess][replay][verify]")
{
    const auto result = ChessReplayVerifier::verify(managementContent(), shortReplay());

    CHECK(result.valid);
    CHECK(result.mismatch == ChessReplayMismatch::None);
    CHECK(result.sequence == 3);
}

TEST_CASE("fresh session verifier identifies altered actions and evidence", "[chess][replay][verify]")
{
    SECTION("altered action")
    {
        auto replay = shortReplay();
        replay.decisions[1].action.shopSlot = 1;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Evidence);
        CHECK(result.sequence == 2);
    }

    SECTION("action absent from legal descriptor set")
    {
        auto replay = shortReplay();
        replay.decisions[0].action.type = ChessActionType::FinishRun;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::IllegalAction);
        CHECK(result.sequence == 1);
    }

    SECTION("altered evidence")
    {
        auto replay = shortReplay();
        replay.decisions[1].evidenceHash[0] ^= 1;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Evidence);
        CHECK(result.sequence == 2);
    }
}

TEST_CASE("fresh session verifier rejects sequence and footer changes", "[chess][replay][verify]")
{
    SECTION("removed decision")
    {
        auto replay = shortReplay();
        replay.decisions.erase(replay.decisions.begin() + 1);
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::IllegalAction);
    }

    SECTION("duplicated decision")
    {
        auto replay = shortReplay();
        replay.decisions.insert(replay.decisions.begin() + 1, replay.decisions.front());
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Evidence);
    }

    SECTION("reordered decisions")
    {
        auto replay = shortReplay();
        std::swap(replay.decisions[0], replay.decisions[1]);
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Evidence);
        CHECK(result.sequence == 1);
    }

    SECTION("altered footer")
    {
        auto replay = shortReplay();
        replay.footer.finalStateHash[0] ^= 1;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Footer);
    }

    SECTION("altered footer completion status")
    {
        auto replay = shortReplay();
        replay.footer.complete = !replay.footer.complete;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Footer);
    }

    SECTION("altered footer fight result")
    {
        auto replay = shortReplay();
        ++replay.footer.fightReached;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK_FALSE(result.valid);
        CHECK(result.mismatch == ChessReplayMismatch::Footer);
    }
}

TEST_CASE("fresh session verifier rejects version and runtime option mismatches", "[chess][replay][verify]")
{
    SECTION("game version")
    {
        auto replay = shortReplay();
        replay.header.gameVersion = "other-version";
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK(result.mismatch == ChessReplayMismatch::Header);
    }


    SECTION("difficulty")
    {
        auto replay = shortReplay();
        replay.header.difficulty = "hard";
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK(result.mismatch == ChessReplayMismatch::Header);
    }

    SECTION("battle frame limit")
    {
        auto replay = shortReplay();
        replay.header.options.battleFrameLimit = 1;
        const auto result = ChessReplayVerifier::verify(managementContent(), replay);
        CHECK(result.mismatch == ChessReplayMismatch::Header);
    }
}

TEST_CASE("replay JSONL parser rejects malformed and truncated streams", "[chess][replay][json]")
{
    ChessReplayJsonError error;
    CHECK_FALSE(parseChessReplayJsonl("not json\n", error));
    CHECK(error.line == 1);

    auto jsonl = serializeChessReplayJsonl(shortReplay());
    const auto footer = jsonl.rfind("{\"record\":\"footer\"");
    REQUIRE(footer != std::string::npos);
    jsonl.resize(footer);
    CHECK_FALSE(parseChessReplayJsonl(jsonl, error));
    CHECK(error.message.find("缺少") != std::string::npos);

    const auto valid = serializeChessReplayJsonl(shortReplay());
    CHECK_FALSE(parseChessReplayJsonl(
        replaceOnce(valid, "\"game_version\":\"dev\"", "\"game_version\":\"\""),
        error));
    const auto nonstandardRuntimeOption = parseChessReplayJsonl(
        replaceOnce(valid, "\"battle_frame_limit\":36000", "\"battle_frame_limit\":1"),
        error);
    REQUIRE(nonstandardRuntimeOption);
    CHECK(nonstandardRuntimeOption->header.options.battleFrameLimit == 1);
}
