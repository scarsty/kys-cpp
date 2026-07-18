#pragma once

#include "ChessReplayTypes.h"
#include "ChessRunRandom.h"

namespace KysChess
{

ChessSha256 chessStateHash(const ChessSessionState& state, const ChessRunRandom& random);
ChessSha256 chessStateHash(const ChessSessionState& state, const ChessRunRandomState& random);
ChessSha256 chessEventHash(const std::vector<ChessSemanticEvent>& events);
ChessSha256 chessRngDigest(const ChessRunRandom& random);

class ChessReplayJournal
{
public:
    explicit ChessReplayJournal(ChessReplayHeader header);
    explicit ChessReplayJournal(const ChessReplay& replay);

    const ChessReplayHeader& header() const { return header_; }
    const std::vector<ChessReplayDecisionRecord>& decisions() const { return decisions_; }
    const ChessEvidenceHash& evidenceHash() const { return evidenceHash_; }

    const ChessReplayDecisionRecord& append(
        ChessSessionPhase phase,
        const ChessAction& action,
        const ChessSha256& preStateHash,
        const ChessSha256& postStateHash,
        const ChessSha256& eventHash,
        const ChessSha256& rngDigest);

    ChessReplay exportReplay(const ChessSessionState& state, const ChessSha256& finalStateHash) const;

private:
    ChessReplayHeader header_;
    std::vector<ChessReplayDecisionRecord> decisions_;
    ChessEvidenceHash evidenceHash_{};
};

}
