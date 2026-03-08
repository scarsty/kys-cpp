#pragma once

#include "ChessContext.h"

namespace KysChess
{

class ChessManager;

IChessGameState& legacyChessGameState();
IChessCatalog& legacyChessCatalog();
IChessFeedback& legacyChessFeedback();
ChessContext makeLegacyChessContext();

}