#pragma once

#include "ChessGameContent.h"
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessObservationText
{
public:
    static std::string formatLegalActions(
        const std::vector<ChessLegalActionDescriptor>& legalActions);
    static std::string format(
        const ChessGameplayObservation& observation,
        const ChessGameContent& content,
        const std::vector<ChessLegalActionDescriptor>& legalActions);
};

}
