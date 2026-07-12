#pragma once

#include "ChessReplayTypes.h"
#include "ChessRunRandom.h"

namespace KysChess
{

std::vector<std::uint8_t> canonicalChessAction(const ChessAction& action);
std::vector<std::uint8_t> canonicalChessState(const ChessSessionState& state, const ChessRunRandom& random);
ChessSha256 canonicalChessStateHash(const ChessSessionState& state, const ChessRunRandom& random);
ChessSha256 canonicalChessEventHash(const std::vector<ChessSemanticEvent>& events);
ChessSha256 canonicalChessRngDigest(const ChessRunRandom& random);
std::vector<std::uint8_t> canonicalChessReplayHeader(const ChessReplayHeader& header);

class ChessReplayJournal
{
public:
    explicit ChessReplayJournal(ChessReplayHeader header);
    explicit ChessReplayJournal(const ChessReplay& replay);

    const ChessReplayHeader& header() const { return header_; }
    const std::vector<ChessReplayDecisionRecord>& decisions() const { return decisions_; }
    const ChessSha256& chainHash() const { return chainHash_; }

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
    ChessSha256 chainHash_{};
};

}
