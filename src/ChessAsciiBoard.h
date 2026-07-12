#pragma once

#include "ChessGameContent.h"
#include "PreparedChessBattle.h"

namespace KysChess
{

class ChessAsciiBoard
{
public:
    static std::string render(
        const PreparedChessBattle& prepared,
        const ChessGameContent& content);
};

}
