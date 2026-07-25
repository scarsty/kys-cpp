#include "ChessSessionCheckpoint.h"

#include "ChessGameSession.h"
#include "ChessManagementRules.h"
#include <glaze/json.hpp>

#include <cassert>
#include <stdexcept>
#include <utility>

namespace KysChess
{
std::string_view chessCheckpointErrorDescription(ChessCheckpointError error)
{
    switch (error)
    {
    case ChessCheckpointError::None: return {};
    case ChessCheckpointError::Malformed: return "檢查點格式不完整";
    case ChessCheckpointError::IncompatibleGameVersion: return "遊戲版本不相容";
    case ChessCheckpointError::UnrepresentableSnapshot: return "快照狀態無法還原";
    case ChessCheckpointError::UnstableBoundary: return "不在穩定決策邊界";
    }
    std::unreachable();
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
    if (!chessCheckpointVersionCompatible(
            *this,
            session.content_->gameVersion()))
    {
        return ChessCheckpointError::IncompatibleGameVersion;
    }
    if (state.phase == ChessSessionPhase::BattleResolution
        || state.difficulty != session.content_->difficulty()
        || !ChessManagementRules::formationIsValid(state, state.formationSlots))
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
    result.replay = std::move(*replay);
    result.state = data.state;
    result.random = data.random;
    result.snapshotHash = hash;
    result.saveRevision = data.save_revision;
    result.label = data.label;
    if (!ChessManagementRules::formationIsValid(
            result.state,
            result.state.formationSlots))
    {
        error = ChessCheckpointError::UnrepresentableSnapshot;
        return std::nullopt;
    }
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

std::optional<ParsedChessSavePayload> parseChessSavePayload(
    std::string_view payload,
    ChessCheckpointError& error)
{
    ChessCheckpointError directError;
    if (auto checkpoint = ChessSessionCheckpoint::parseJson(payload, directError))
    {
        error = ChessCheckpointError::None;
        return ParsedChessSavePayload{std::move(*checkpoint), std::nullopt};
    }
    if (directError == ChessCheckpointError::UnrepresentableSnapshot)
    {
        error = directError;
        return std::nullopt;
    }

    ChessSaveSlotData slot;
    constexpr auto options = glz::opts{.error_on_unknown_keys = false};
    if (glz::read<options>(slot, payload))
    {
        error = ChessCheckpointError::Malformed;
        return std::nullopt;
    }

    auto checkpoint = ChessSessionCheckpoint::fromData(
        slot.checkpoint,
        error);
    if (!checkpoint)
    {
        return std::nullopt;
    }
    return ParsedChessSavePayload{std::move(*checkpoint), slot.scene};
}

}
