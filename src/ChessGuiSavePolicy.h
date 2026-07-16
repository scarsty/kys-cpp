#pragma once

#include "ChessGameSession.h"

#include <utility>

namespace KysChess
{

constexpr bool isChessGuiSavePhaseContinuable(ChessSessionPhase phase)
{
    switch (phase)
    {
    case ChessSessionPhase::Management:
        return true;
    case ChessSessionPhase::BattlePreparation:
    case ChessSessionPhase::BattleResolution:
    case ChessSessionPhase::RewardChoice:
    case ChessSessionPhase::Complete:
        return false;
    }
    std::unreachable();
}

inline bool canPersistChessGuiState(const ChessGameSession& session)
{
    return session.isStableDecisionBoundary()
        && isChessGuiSavePhaseContinuable(session.state().phase);
}

}
