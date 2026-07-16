#include "ChessGuiSessionAdapter.h"
#include "ChessGuiSavePolicy.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessJsonProtocol.h"
#include "ChessReplayJson.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace KysChess;
using namespace KysChess::Test;

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

TEST_CASE("GUI persistence only replaces a save from the overworld management phase", "[chess][gui][save]")
{
    CHECK(isChessGuiSavePhaseContinuable(ChessSessionPhase::Management));
    CHECK_FALSE(isChessGuiSavePhaseContinuable(ChessSessionPhase::BattlePreparation));
    CHECK_FALSE(isChessGuiSavePhaseContinuable(ChessSessionPhase::BattleResolution));
    CHECK_FALSE(isChessGuiSavePhaseContinuable(ChessSessionPhase::RewardChoice));
    CHECK_FALSE(isChessGuiSavePhaseContinuable(ChessSessionPhase::Complete));

    const auto content = managementContent();
    ChessGameSession session(content, 321);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);

    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    CHECK(canPersistChessGuiState(session));

    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    CHECK(session.state().phase == ChessSessionPhase::BattlePreparation);
    CHECK_FALSE(canPersistChessGuiState(session));

    ChessAction start;
    start.type = ChessActionType::StartBattle;
    const auto started = session.beginAction(start);
    REQUIRE(started.accepted);
    REQUIRE(started.transitionPending);
    CHECK(session.state().phase == ChessSessionPhase::BattleResolution);
    CHECK_FALSE(canPersistChessGuiState(session));
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
