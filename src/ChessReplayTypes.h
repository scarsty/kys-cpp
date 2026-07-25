#pragma once

#include "ChessSessionTypes.h"

namespace KysChess
{

struct ChessReplayHeader
{
    std::string gameVersion;
    std::string difficulty;
    std::uint64_t rootSeed{};
    ChessSessionOptions options;
    ChessSha256 contentFingerprint{};

    auto operator<=>(const ChessReplayHeader&) const = default;
};

struct ChessReplayDecisionRecord
{
    ChessAction action;
    ChessEvidenceHash evidenceHash{};

    auto operator<=>(const ChessReplayDecisionRecord&) const = default;
};

struct ChessReplayFooter
{
    bool complete = false;
    ChessEvidenceHash terminalEvidenceHash{};
    ChessSha256 finalStateHash{};
    int fightReached{};

    auto operator<=>(const ChessReplayFooter&) const = default;
};

struct ChessReplay
{
    ChessReplayHeader header;
    std::vector<ChessReplayDecisionRecord> decisions;
    ChessReplayFooter footer;

    auto operator<=>(const ChessReplay&) const = default;
};

}
