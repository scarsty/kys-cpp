#include "ChessSaveStore.h"
#include "ChessSaveFile.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessReplayVerifier.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>

using namespace KysChess;
using namespace KysChess::Test;

namespace
{

class TemporarySaveDirectory
{
public:
    TemporarySaveDirectory()
        : path_(std::filesystem::temp_directory_path()
            / ("kys-chess-save-store-"
                + std::to_string(std::chrono::steady_clock::now()
                    .time_since_epoch().count())))
    {
        std::filesystem::create_directories(path_);
    }

    ~TemporarySaveDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

ChessAction lockAction(bool value)
{
    ChessAction action;
    action.type = ChessActionType::SetShopLocked;
    action.value = value;
    return action;
}

}

TEST_CASE("self-contained checkpoint JSON directly restores its snapshot and full journal", "[chess][checkpoint][save]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 55);
    REQUIRE(session.submitAndDrain(lockAction(true)).accepted);
    const auto beforeCounter = session.random().streamState(ChessRngStream::Shop).rawDrawCount;

    const auto checkpoint = ChessSessionCheckpoint::capture(session, 4, "第一戰前");
    const auto payload = checkpoint.serializeJson();
    CHECK(payload.contains("\"replay\":{"));
    CHECK(payload.contains("\"evidence_hash\""));
    CHECK_FALSE(payload.contains("replay_jsonl"));
    CHECK_FALSE(payload.contains("pre_state_hash"));
    CHECK_FALSE(payload.contains("chain_hash"));
    const auto gameVersionPosition = payload.find("\"game_version\"");
    REQUIRE(gameVersionPosition != std::string::npos);
    CHECK(payload.find("\"game_version\"", gameVersionPosition + 1) == std::string::npos);
    ChessCheckpointError error;
    const auto parsed = ChessSessionCheckpoint::parseJson(payload, error);

    REQUIRE(parsed);
    CHECK(error == ChessCheckpointError::None);
    CHECK(parsed->state == session.state());
    CHECK(parsed->random == session.random().state());
    CHECK(parsed->gameVersion() == content->gameVersion());
    CHECK(session.random().streamState(ChessRngStream::Shop).rawDrawCount == beforeCounter);

    ChessGameSession restored(content, 999);
    REQUIRE(parsed->restore(restored) == ChessCheckpointError::None);
    CHECK(restored.state() == session.state());
    CHECK(restored.random().state() == session.random().state());
    REQUIRE(restored.journal().decisions().size() == session.journal().decisions().size());
    CHECK(restored.journal().decisions().front().evidenceHash
        == session.journal().decisions().front().evidenceHash);

    ChessSaveSlotData slot;
    slot.scene = {53, 21, 54, 1};
    slot.checkpoint = checkpoint.toData();
    const auto slotJson = serializeChessSaveSlotJson(slot);
    REQUIRE(slotJson);
    CHECK(slotJson->contains("\"scene\""));
    CHECK(slotJson->contains("\"checkpoint\""));
    CHECK_FALSE(slotJson->contains("gameData"));
    CHECK_FALSE(slotJson->contains("chessSessionCheckpoint"));
}

TEST_CASE("direct restore accepts dev versions and rejects incompatible release snapshots", "[chess][checkpoint][save]")
{
    const auto content = managementContent(100, Difficulty::Normal, "1.4.0");
    ChessGameSession session(content, 77);
    const auto originalState = session.state();

    auto incompatible = ChessSessionCheckpoint::capture(session, 1);
    incompatible.replay.header.gameVersion = "另一個遊戲版本";
    CHECK(incompatible.restore(session) == ChessCheckpointError::IncompatibleGameVersion);
    CHECK(session.state() == originalState);

    auto incompatibleContent = ChessSessionCheckpoint::capture(session, 2);
    incompatibleContent.replay.header.contentFingerprint[0] ^= 1;
    CHECK(incompatibleContent.restore(session) == ChessCheckpointError::IncompatibleContent);
    CHECK(session.state() == originalState);

    auto devSave = ChessSessionCheckpoint::capture(session, 2);
    devSave.replay.header.gameVersion = "dev";
    CHECK(devSave.restore(session) == ChessCheckpointError::None);

    const auto devContent = managementContent();
    ChessGameSession devSession(devContent, 78);
    auto releaseSave = ChessSessionCheckpoint::capture(session, 3);
    releaseSave.replay.header.gameVersion = "1.3.0";
    CHECK(releaseSave.restore(devSession) == ChessCheckpointError::None);

    auto transition = ChessSessionCheckpoint::capture(session, 4);
    transition.state.phase = ChessSessionPhase::BattleResolution;
    CHECK(transition.restore(session) == ChessCheckpointError::UnrepresentableSnapshot);
    CHECK(session.state() == originalState);

    auto wrongDifficulty = ChessSessionCheckpoint::capture(session, 5);
    wrongDifficulty.state.difficulty = Difficulty::Easy;
    CHECK(wrongDifficulty.restore(session) == ChessCheckpointError::UnrepresentableSnapshot);
    CHECK(session.state() == originalState);
}

TEST_CASE("snapshot cheats load immediately and explicit replay audit finds first divergence", "[chess][checkpoint][save][replay]")
{
    const auto content = managementContent();
    ChessGameSession source(content, 77);
    REQUIRE(source.submitAndDrain(lockAction(true)).accepted);
    const auto originalPrefix = source.journal().decisions();

    auto checkpoint = ChessSessionCheckpoint::capture(source, 1);
    checkpoint.state.money += 100;
    ChessSaveStore store;
    REQUIRE(store.importSave(
        "cheat",
        checkpoint.serializeJson(),
        content->gameVersion()) == ChessCheckpointError::None);
    ChessGameSession restored(content, 999);
    ChessTimelineReplacement replacement;
    REQUIRE(store.load("cheat", restored, replacement) == ChessCheckpointError::None);
    CHECK(restored.state().money == source.state().money + 100);
    REQUIRE(restored.journal().decisions().size() == originalPrefix.size());
    CHECK(restored.journal().decisions().front().evidenceHash == originalPrefix.front().evidenceHash);

    REQUIRE(restored.submitAndDrain(lockAction(false)).accepted);
    const auto replay = restored.exportReplay();
    REQUIRE(replay);
    REQUIRE(replay->decisions.size() == originalPrefix.size() + 1);
    CHECK(replay->decisions.front().evidenceHash == originalPrefix.front().evidenceHash);

    const auto verification = ChessReplayVerifier::verify(content, *replay);
    CHECK_FALSE(verification.valid);
    CHECK(verification.mismatch == ChessReplayMismatch::Evidence);
    CHECK(verification.sequence == 2);
}

TEST_CASE("structurally valid journal corruption can load without implicit verification", "[chess][checkpoint][save][replay]")
{
    const auto content = managementContent();
    ChessGameSession source(content, 78);
    REQUIRE(source.submitAndDrain(lockAction(true)).accepted);
    auto checkpoint = ChessSessionCheckpoint::capture(source, 1);
    checkpoint.replay.decisions.front().evidenceHash.front() ^= 0xff;

    ChessSaveStore store;
    REQUIRE(store.importSave(
        "corrupted",
        checkpoint.serializeJson(),
        content->gameVersion()) == ChessCheckpointError::None);
    ChessGameSession restored(content, 999);
    ChessTimelineReplacement replacement;
    REQUIRE(store.load("corrupted", restored, replacement) == ChessCheckpointError::None);
    CHECK(restored.state() == source.state());
    REQUIRE(restored.journal().decisions().size() == 1);

    const auto replay = restored.exportReplay();
    REQUIRE(replay);
    const auto verification = ChessReplayVerifier::verify(content, *replay);
    CHECK_FALSE(verification.valid);
    CHECK(verification.mismatch == ChessReplayMismatch::Evidence);
    CHECK(verification.sequence == 1);
}

TEST_CASE("nonstandard replay runtime options load normally and fail only explicit verification", "[chess][checkpoint][save][replay]")
{
    const auto content = managementContent();
    ChessGameSession source(content, 79);
    REQUIRE(source.submitAndDrain(lockAction(true)).accepted);
    auto checkpoint = ChessSessionCheckpoint::capture(source, 1);
    checkpoint.replay.header.options.battleFrameLimit = 1;

    ChessSaveStore store;
    REQUIRE(store.importSave(
        "nonstandard-runtime",
        checkpoint.serializeJson(),
        content->gameVersion()) == ChessCheckpointError::None);
    ChessGameSession restored(content, 999);
    ChessTimelineReplacement replacement;
    REQUIRE(store.load("nonstandard-runtime", restored, replacement) == ChessCheckpointError::None);
    CHECK(restored.state() == source.state());
    CHECK(restored.journal().header().options.battleFrameLimit == 1);
    REQUIRE(restored.journal().decisions().size() == source.journal().decisions().size());

    const auto replay = restored.exportReplay();
    REQUIRE(replay);
    const auto verification = ChessReplayVerifier::verify(content, *replay);
    CHECK_FALSE(verification.valid);
    CHECK(verification.mismatch == ChessReplayMismatch::Header);
    CHECK(verification.sequence == 0);
}

TEST_CASE("loading replaces the active suffix while preserving save slots", "[chess][checkpoint][save][replay]")
{
    const auto content = managementContent();
    ChessGameSession session(content, 99);
    ChessSaveStore store;
    REQUIRE(session.submitAndDrain(lockAction(true)).accepted);
    REQUIRE(store.save("1", session, "鎖定商店") == ChessCheckpointError::None);
    REQUIRE(session.submitAndDrain(lockAction(false)).accepted);
    REQUIRE(session.journal().decisions().size() == 2);

    ChessTimelineReplacement replacement;
    REQUIRE(store.load("1", session, replacement) == ChessCheckpointError::None);

    CHECK(replacement.previousSequence == 2);
    CHECK(replacement.restoredSequence == 1);
    CHECK(replacement.discardedActiveActions == 1);
    CHECK(session.state().shopLocked);
    REQUIRE(session.journal().decisions().size() == 1);
    REQUIRE(store.list().size() == 1);
    REQUIRE(session.submitAndDrain(lockAction(false)).accepted);
    const auto replay = session.exportReplay();
    REQUIRE(replay);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
}

TEST_CASE("shared native autosave file survives process reconstruction", "[chess][checkpoint][save][durable]")
{
    const auto content = managementContent();
    TemporarySaveDirectory directory;
    const auto autosaveFile = directory.path() / "autosave.json";
    {
        ChessGameSession session(content, 404);
        REQUIRE(session.submitAndDrain(lockAction(true)).accepted);
        ChessSaveStore store;
        REQUIRE(store.save("autosave", session, "自動存檔") == ChessCheckpointError::None);
        REQUIRE(writeChessCheckpointFile(autosaveFile, *store.inspect("autosave")));
    }

    auto checkpoint = readChessCheckpointFile(autosaveFile);
    REQUIRE(checkpoint);
    ChessSaveStore restoredStore;
    restoredStore.restoreSave("autosave", std::move(*checkpoint));
    const auto summaries = restoredStore.list();
    REQUIRE(summaries.size() == 1);
    CHECK(summaries.front().slotId == "autosave");
    CHECK(summaries.front().label == "自動存檔");

    ChessGameSession restored(content, 999);
    ChessTimelineReplacement replacement;
    REQUIRE(restoredStore.load("autosave", restored, replacement) == ChessCheckpointError::None);
    CHECK(restored.state().shopLocked);
    REQUIRE(restoredStore.save("autosave", restored, "覆寫") == ChessCheckpointError::None);
    CHECK(restoredStore.inspect("autosave")->saveRevision > summaries.front().revision);
    REQUIRE(writeChessCheckpointFile(
        autosaveFile,
        *restoredStore.inspect("autosave")));
    CHECK(std::ranges::distance(std::filesystem::directory_iterator(directory.path())) == 1);
}

TEST_CASE("portable save import does not activate the checkpoint", "[chess][checkpoint][save]")
{
    const auto content = managementContent();
    ChessGameSession sourceSession(content, 101);
    ChessSaveStore source;
    REQUIRE(source.save("source", sourceSession) == ChessCheckpointError::None);
    const auto payload = source.exportSave("source");
    REQUIRE(payload);
    ChessSaveStore target;
    ChessGameSession resumed(content, 202);
    REQUIRE(resumed.submitAndDrain(lockAction(true)).accepted);

    ChessCheckpointError checkpointError;
    auto releaseCheckpoint = ChessSessionCheckpoint::parseJson(*payload, checkpointError);
    REQUIRE(releaseCheckpoint);
    releaseCheckpoint->replay.header.gameVersion = "1.4.0";
    const auto releasePayload = releaseCheckpoint->serializeJson();
    CHECK(target.importSave("wrong-version", releasePayload, "1.5.0")
        == ChessCheckpointError::IncompatibleGameVersion);
    CHECK_FALSE(target.inspect("wrong-version"));

    REQUIRE(target.importSave("dev-version", *payload, "另一個遊戲版本")
        == ChessCheckpointError::None);
    CHECK(target.inspect("dev-version"));
    REQUIRE(target.importSave("copy", *payload, content->gameVersion()) == ChessCheckpointError::None);
    CHECK(target.inspect("copy"));
    CHECK(resumed.journal().decisions().size() == 1);

    ChessTimelineReplacement replacement;
    REQUIRE(target.load("copy", resumed, replacement) == ChessCheckpointError::None);
    CHECK(replacement.discardedActiveActions == 1);
    CHECK(resumed.random().rootSeed() == 101);
    CHECK(resumed.journal().decisions().empty());

    ChessAction reroll;
    reroll.type = ChessActionType::RerollEnemySeed;
    REQUIRE(resumed.submitAndDrain(reroll).accepted);
    REQUIRE(target.save("continued", resumed) == ChessCheckpointError::None);
    const auto replay = resumed.exportReplay();
    REQUIRE(replay);
    CHECK(ChessReplayVerifier::verify(content, *replay).valid);
    REQUIRE(target.inspect("continued"));
    CHECK(target.inspect("continued")->gameVersion() == content->gameVersion());
}

TEST_CASE("timeline replacement counts a divergent equal-length branch", "[chess][checkpoint][save][replay]")
{
    const auto content = managementContent();
    ChessGameSession savedBranch(content, 303);
    REQUIRE(savedBranch.submitAndDrain(lockAction(true)).accepted);
    ChessSaveStore source;
    REQUIRE(source.save("branch", savedBranch) == ChessCheckpointError::None);
    const auto payload = source.exportSave("branch");
    REQUIRE(payload);

    ChessGameSession activeBranch(content, 303);
    REQUIRE(activeBranch.submitAndDrain(lockAction(false)).accepted);
    ChessSaveStore target;
    REQUIRE(target.importSave("branch", *payload, content->gameVersion()) == ChessCheckpointError::None);
    ChessTimelineReplacement replacement;

    REQUIRE(target.load("branch", activeBranch, replacement) == ChessCheckpointError::None);
    CHECK(replacement.previousSequence == 1);
    CHECK(replacement.restoredSequence == 1);
    CHECK(replacement.discardedActiveActions == 1);
    CHECK(activeBranch.state().shopLocked);
}

TEST_CASE("map decision point and selected map persist in checkpoint replay", "[chess][checkpoint][save][map]")
{
    const auto content = configuredMapChoiceContent();
    ChessGameSession session(content, 103);
    REQUIRE(session.submitAndDrain(buySlot(0)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(1)).accepted);
    REQUIRE(session.submitAndDrain(buySlot(2)).accepted);

    ChessAction deploy;
    deploy.type = ChessActionType::SetDeployment;
    deploy.chessInstanceIds = {session.state().roster.begin()->first};
    REQUIRE(session.submitAndDrain(deploy).accepted);
    ChessAction prepare;
    prepare.type = ChessActionType::PrepareBattle;
    REQUIRE(session.submitAndDrain(prepare).accepted);
    REQUIRE(session.state().preparedBattle);
    CHECK(session.state().preparedBattle->chosenMapId == -1);
    CHECK(session.state().preparedBattle->mapCandidates == std::vector<int>{7, 8});

    const auto pendingPayload = ChessSessionCheckpoint::capture(session, 1, "待選戰場").serializeJson();
    ChessCheckpointError error;
    const auto pending = ChessSessionCheckpoint::parseJson(pendingPayload, error);
    REQUIRE(pending);
    CHECK(error == ChessCheckpointError::None);
    REQUIRE(pending->state.preparedBattle);
    CHECK(pending->state.preparedBattle->chosenMapId == -1);
    CHECK(pending->state.preparedBattle->mapCandidates == std::vector<int>{7, 8});

    ChessAction choose;
    choose.type = ChessActionType::ChooseMap;
    choose.mapId = 8;
    REQUIRE(session.submitAndDrain(choose).accepted);
    const auto selectedPayload = ChessSessionCheckpoint::capture(session, 2, "已選戰場").serializeJson();
    const auto selected = ChessSessionCheckpoint::parseJson(selectedPayload, error);
    REQUIRE(selected);
    CHECK(error == ChessCheckpointError::None);
    REQUIRE(selected->state.preparedBattle);
    CHECK(selected->state.preparedBattle->chosenMapId == 8);
    REQUIRE_FALSE(selected->replay.decisions.empty());
    CHECK(selected->replay.decisions.back().action.type == ChessActionType::ChooseMap);
    CHECK(selected->replay.decisions.back().action.mapId == 8);
}
