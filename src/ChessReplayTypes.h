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
};

struct ChessReplayDecisionRecord
{
    std::uint64_t sequence{};
    ChessSessionPhase phase{};
    ChessAction action;
    ChessSha256 preStateHash{};
    ChessSha256 postStateHash{};
    ChessSha256 eventHash{};
    ChessSha256 rngDigest{};
    ChessSha256 previousChainHash{};
    ChessSha256 chainHash{};
};

struct ChessReplayFooter
{
    bool complete = false;
    ChessSha256 terminalChainHash{};
    ChessSha256 finalStateHash{};
    int fightReached{};
};

struct ChessReplay
{
    ChessReplayHeader header;
    std::vector<ChessReplayDecisionRecord> decisions;
    ChessReplayFooter footer;
};

}
