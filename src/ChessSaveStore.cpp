#include "ChessSaveStore.h"

#include "ChessGameSession.h"

#include <algorithm>

namespace KysChess
{
namespace
{

bool sameDecision(
    const ChessReplayDecisionRecord& first,
    const ChessReplayDecisionRecord& second)
{
    return first.sequence == second.sequence
        && first.phase == second.phase
        && canonicalChessAction(first.action) == canonicalChessAction(second.action)
        && first.preStateHash == second.preStateHash
        && first.postStateHash == second.postStateHash
        && first.eventHash == second.eventHash
        && first.rngDigest == second.rngDigest
        && first.previousChainHash == second.previousChainHash
        && first.chainHash == second.chainHash;
}

std::size_t commonJournalPrefix(
    const ChessReplayJournal& current,
    const ChessReplay& restored)
{
    if (canonicalChessReplayHeader(current.header())
        != canonicalChessReplayHeader(restored.header))
    {
        return 0;
    }
    const auto count = std::min(current.decisions().size(), restored.decisions.size());
    std::size_t common{};
    while (common < count
        && sameDecision(current.decisions()[common], restored.decisions[common]))
    {
        ++common;
    }
    return common;
}

}

ChessCheckpointError ChessSaveStore::save(
    std::string slotId,
    const ChessGameSession& session,
    std::string label)
{
    if (!session.isStableDecisionBoundary())
    {
        return ChessCheckpointError::UnstableBoundary;
    }
    slots_.insert_or_assign(
        std::move(slotId),
        ChessSessionCheckpoint::capture(session, nextRevision_++, std::move(label)));
    return ChessCheckpointError::None;
}

ChessCheckpointError ChessSaveStore::load(
    const std::string& slotId,
    ChessGameSession& session,
    ChessTimelineReplacement& replacement) const
{
    const auto found = slots_.find(slotId);
    if (found == slots_.end())
    {
        return ChessCheckpointError::Malformed;
    }
    const auto previous = session.journal().decisions().size();
    const auto restored = found->second.replay.decisions.size();
    const auto common = commonJournalPrefix(session.journal(), found->second.replay);
    const auto error = found->second.restore(session);
    if (error != ChessCheckpointError::None)
    {
        return error;
    }
    replacement.loadedSlot = slotId;
    replacement.previousSequence = previous;
    replacement.restoredSequence = restored;
    replacement.discardedActiveActions = previous - common;
    replacement.currentStateHash = session.observe().stateHash;
    return ChessCheckpointError::None;
}

const ChessSessionCheckpoint* ChessSaveStore::inspect(const std::string& slotId) const
{
    const auto found = slots_.find(slotId);
    return found == slots_.end() ? nullptr : &found->second;
}

std::vector<ChessSaveSlotSummary> ChessSaveStore::list(const ChessGameSession& session) const
{
    std::vector<ChessSaveSlotSummary> result;
    for (const auto& [slotId, checkpoint] : slots_)
    {
        result.push_back({
            slotId,
            true,
            checkpoint.saveRevision,
            checkpoint.label,
            checkpoint.state.fight,
            checkpoint.state.level,
            checkpoint.state.money,
            static_cast<int>(checkpoint.state.roster.size()),
            checkpoint.replay.decisions.size(),
            checkpoint.snapshotHash,
            checkpoint.gameVersion == session.content().gameVersion()
                && checkpoint.state.difficulty == session.content().difficulty()
                && checkpoint.state.phase != ChessSessionPhase::BattleResolution,
        });
    }
    return result;
}

std::optional<std::string> ChessSaveStore::exportSave(const std::string& slotId) const
{
    const auto* checkpoint = inspect(slotId);
    return checkpoint ? std::optional(checkpoint->serializeJson()) : std::nullopt;
}

ChessCheckpointError ChessSaveStore::importSave(
    std::string slotId,
    std::string_view payload,
    std::string_view gameVersion)
{
    ChessCheckpointError error;
    auto checkpoint = ChessSessionCheckpoint::parseJson(payload, error);
    if (!checkpoint)
    {
        return error;
    }
    if (checkpoint->gameVersion != gameVersion)
    {
        return ChessCheckpointError::IncompatibleGameVersion;
    }
    nextRevision_ = std::max(nextRevision_, checkpoint->saveRevision + 1);
    slots_.insert_or_assign(std::move(slotId), std::move(*checkpoint));
    return ChessCheckpointError::None;
}

}
