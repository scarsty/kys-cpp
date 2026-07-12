#pragma once

#include "ChessGameContent.h"
#include "Point.h"

#include <cstdint>
#include <vector>

namespace KysChess
{

class BattlefieldData
{
public:
    static constexpr int CoordinateCount = 64;
    static constexpr float TileWidth = 36.0f;

    explicit BattlefieldData(const ChessBattlefieldDefinition* definition = nullptr);

    bool contains(int x, int y) const;
    bool isBuilding(int x, int y) const;
    bool isWater(int x, int y) const;
    bool canWalk(int x, int y) const;
    Pointf worldPosition(int x, int y) const;
    Point gridPosition(Pointf position) const;

private:
    std::int16_t layerValue(int layer, int x, int y) const;

    std::vector<std::int16_t> layers_;
};

}
