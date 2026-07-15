#pragma once

#include "ChessActionOffers.h"
#include "ChessGameContent.h"
#include "ChessSessionTypes.h"

namespace KysChess
{

class ChessObservationText
{
public:
    static std::string format(
        const ChessGameplayObservation& observation,
        const ChessGameContent& content,
        const std::vector<ChessActionOffer>& legalActions);
};

}
