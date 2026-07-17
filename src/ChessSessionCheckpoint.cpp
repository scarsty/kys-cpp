#include "ChessSessionCheckpoint.h"

#include "ChessGameSession.h"
#include <glaze/json.hpp>

#include <cassert>
#include <stdexcept>

namespace KysChess
{
ChessSessionCheckpoint ChessSessionCheckpoint::capture(
    const ChessGameSession& session,
    std::uint64_t revision,
    std::string checkpointLabel)
{
    assert(session.isStableDecisionBoundary());
    auto replay = session.exportReplay();
    assert(replay);
    ChessSessionCheckpoint result;
    result.gameVersion = session.content().gameVersion();
    result.replay = std::move(*replay);
    result.state = session.state();
    result.random = session.random().state();
    result.snapshotHash = chessStateHash(result.state, session.random());
    result.saveRevision = revision;
    result.label = std::move(checkpointLabel);
    return result;
}

ChessCheckpointError ChessSessionCheckpoint::restore(ChessGameSession& session) const
{
    if (!session.isStableDecisionBoundary())
    {
        return ChessCheckpointError::UnstableBoundary;
    }
    if (gameVersion != session.content_->gameVersion())
    {
        return ChessCheckpointError::IncompatibleGameVersion;
    }
    if (state.phase == ChessSessionPhase::BattleResolution
        || state.difficulty != session.content_->difficulty())
    {
        return ChessCheckpointError::UnrepresentableSnapshot;
    }
    session.state_ = state;
    session.random_.restore(random);
    session.journal_ = ChessReplayJournal(replay);
    session.discardPendingTransition();
    session.lastBattleRuntime_.reset();
    session.lastBattleResult_.reset();
    session.lastBattlePrepared_.reset();
    return ChessCheckpointError::None;
}

ChessSessionCheckpointData ChessSessionCheckpoint::toData() const
{
    ChessSessionCheckpointData data;
    data.game_version = gameVersion;
    data.replay = chessReplayData(replay);
    data.state = state;
    data.random = random;
    data.snapshot_hash = chessSha256Hex(snapshotHash);
    data.save_revision = saveRevision;
    data.label = label;
    return data;
}

std::optional<ChessSessionCheckpoint> ChessSessionCheckpoint::fromData(
    const ChessSessionCheckpointData& data,
    ChessCheckpointError& error)
{
    ChessReplayJsonError replayError;
    auto replay = parseChessReplayData(data.replay, replayError);
    ChessSha256 hash{};
    try
    {
        hash = chessSha256FromHex(data.snapshot_hash);
    }
    catch (const std::invalid_argument&)
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }
    if (!replay)
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }
    ChessSessionCheckpoint result;
    result.gameVersion = data.game_version;
    result.replay = std::move(*replay);
    result.state = data.state;
    result.random = data.random;
    result.snapshotHash = hash;
    result.saveRevision = data.save_revision;
    result.label = data.label;
    error = ChessCheckpointError::None;
    return result;
}

std::string ChessSessionCheckpoint::serializeJson() const
{
    const auto result = glz::write_json(toData());
    return result ? result.value() : std::string{};
}

std::optional<ChessSessionCheckpoint> ChessSessionCheckpoint::parseJson(
    std::string_view json,
    ChessCheckpointError& error)
{
    ChessSessionCheckpointData data;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    if (glz::read<options>(data, json))
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }
    return fromData(data, error);
}

}
