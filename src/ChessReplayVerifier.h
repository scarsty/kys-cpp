#pragma once

#include "ChessGameSession.h"

namespace KysChess
{

enum class ChessReplayMismatch
{
    None,
    Header,
    Sequence,
    Phase,
    PreState,
    IllegalAction,
    Event,
    Rng,
    PostState,
    Chain,
    Footer,
};

struct ChessReplayVerificationResult
{
    bool valid = false;
    ChessReplayMismatch mismatch = ChessReplayMismatch::None;
    std::uint64_t sequence{};
    std::string message;
};

class ChessReplayVerifier
{
public:
    static ChessReplayVerificationResult verify(
        std::shared_ptr<const ChessGameContent> content,
        const ChessReplay& replay);
};

}
