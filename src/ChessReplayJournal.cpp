#include "ChessReplayJournal.h"

namespace KysChess
{

ChessSha256 chessStateHash(
    const ChessSessionState& state,
    const ChessRunRandom& random)
{
    const auto randomState = random.state();
    return chessBeveSha256("KYS_CHESS_STATE", state, randomState);
}

ChessSha256 chessEventHash(const std::vector<ChessSemanticEvent>& events)
{
    return chessBeveSha256("KYS_CHESS_EVENTS", events);
}

ChessSha256 chessRngDigest(const ChessRunRandom& random)
{
    const auto state = random.state();
    return chessBeveSha256("KYS_CHESS_RNG", state);
}

ChessReplayJournal::ChessReplayJournal(ChessReplayHeader header)
    : header_(std::move(header)),
      evidenceHash_(chessBeveEvidenceHash("KYS_CHESS_REPLAY_HEADER", header_))
{
}

ChessReplayJournal::ChessReplayJournal(const ChessReplay& replay)
    : header_(replay.header),
      decisions_(replay.decisions),
      evidenceHash_(replay.footer.terminalEvidenceHash)
{
}

const ChessReplayDecisionRecord& ChessReplayJournal::append(
    ChessSessionPhase phase,
    const ChessAction& action,
    const ChessSha256& preStateHash,
    const ChessSha256& postStateHash,
    const ChessSha256& eventHash,
    const ChessSha256& rngDigest)
{
    ChessReplayDecisionRecord record;
    record.action = action;
    record.evidenceHash = chessBeveEvidenceHash(
        "KYS_CHESS_REPLAY_EVIDENCE",
        evidenceHash_,
        static_cast<std::uint64_t>(decisions_.size() + 1),
        phase,
        action,
        preStateHash,
        postStateHash,
        eventHash,
        rngDigest);
    evidenceHash_ = record.evidenceHash;
    decisions_.push_back(std::move(record));
    return decisions_.back();
}

ChessReplay ChessReplayJournal::exportReplay(
    const ChessSessionState& state,
    const ChessSha256& finalStateHash) const
{
    ChessReplay replay;
    replay.header = header_;
    replay.decisions = decisions_;
    replay.footer.complete = state.phase == ChessSessionPhase::Complete;
    replay.footer.terminalEvidenceHash = evidenceHash_;
    replay.footer.finalStateHash = finalStateHash;
    replay.footer.fightReached = state.fight;
    return replay;
}

}
