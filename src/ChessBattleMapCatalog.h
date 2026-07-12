#pragma once

#include "ChessGameContent.h"
#include "Point.h"

#include <string_view>

namespace KysChess
{

struct ChessBattleMapCatalogEntry
{
    int battleId{};
    int enemyCapacity{};
    std::vector<Point> teammatePositions;
    std::vector<Point> allyClonePositions;
    std::string_view name;
};

class ChessBattleMapCatalog
{
public:
    static const std::vector<ChessBattleMapCatalogEntry>& entries();
    static const ChessBattleMapCatalogEntry* find(int battleId);
    static std::vector<int> fittingMapIds(
        const ChessGameContent& content,
        int allyCount,
        int enemyCount);
    static std::string_view displayName(int battleId);
};

}
