#include "ChessSessionCheckpoint.h"

#include "ChessGameSession.h"
#include "ChessReplayJson.h"

#include <glaze/json.hpp>

#include <cassert>
#include <stdexcept>

namespace KysChess
{
namespace CheckpointDetail
{

struct CheckpointDto
{
    std::string game_version;
    std::string replay_jsonl;
    ChessSessionState state;
    ChessRunRandomState random;
    std::string snapshot_hash;
    std::uint64_t save_revision{};
    std::string label;
};

}

namespace
{

using CheckpointDetail::CheckpointDto;

}

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
    result.snapshotHash = canonicalChessStateHash(result.state, session.random());
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

std::string ChessSessionCheckpoint::serializeJson() const
{
    CheckpointDto dto;
    dto.game_version = gameVersion;
    dto.replay_jsonl = serializeChessReplayJsonl(replay);
    dto.state = state;
    dto.random = random;
    dto.snapshot_hash = chessSha256Hex(snapshotHash);
    dto.save_revision = saveRevision;
    dto.label = label;
    const auto result = glz::write_json(dto);
    return result ? result.value() : std::string{};
}

std::optional<ChessSessionCheckpoint> ChessSessionCheckpoint::parseJson(
    std::string_view json,
    ChessCheckpointError& error)
{
    CheckpointDto dto;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    if (glz::read<options>(dto, json))
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }
    ChessReplayJsonError replayError;
    auto replay = parseChessReplayJsonl(dto.replay_jsonl, replayError);
    ChessSha256 hash{};
    try
    {
        hash = chessSha256FromHex(dto.snapshot_hash);
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
    result.gameVersion = std::move(dto.game_version);
    result.replay = std::move(*replay);
    result.state = std::move(dto.state);
    result.random = dto.random;
    result.snapshotHash = hash;
    result.saveRevision = dto.save_revision;
    result.label = std::move(dto.label);
    error = ChessCheckpointError::None;
    return result;
}

}
