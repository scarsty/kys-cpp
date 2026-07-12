#include "BattlefieldData.h"

#include <cmath>

namespace KysChess
{

BattlefieldData::BattlefieldData(const ChessBattlefieldDefinition* definition)
{
    if (definition)
    {
        layers_ = definition->layers;
    }
}

bool BattlefieldData::contains(int x, int y) const
{
    return x >= 0 && x < CoordinateCount && y >= 0 && y < CoordinateCount;
}

std::int16_t BattlefieldData::layerValue(int layer, int x, int y) const
{
    if (layers_.empty())
    {
        return 0;
    }
    const auto index = static_cast<std::size_t>(
        layer * CoordinateCount * CoordinateCount + x + CoordinateCount * y);
    return layers_[index];
}

bool BattlefieldData::isBuilding(int x, int y) const
{
    return layerValue(1, x, y) > 0;
}

bool BattlefieldData::isWater(int x, int y) const
{
    const int tile = layerValue(0, x, y) / 2;
    return (tile >= 179 && tile <= 181)
        || tile == 261
        || tile == 511
        || (tile >= 662 && tile <= 665)
        || tile == 674;
}

bool BattlefieldData::canWalk(int x, int y) const
{
    return contains(x, y) && !isBuilding(x, y) && !isWater(x, y);
}

Pointf BattlefieldData::worldPosition(int x, int y) const
{
    return {
        static_cast<float>((-y + x + CoordinateCount) * TileWidth),
        static_cast<float>((y + x) * TileWidth),
        0.0f,
    };
}

Point BattlefieldData::gridPosition(Pointf position) const
{
    const double x = position.x / TileWidth - CoordinateCount;
    const double y = position.y / TileWidth;
    return {
        static_cast<int>(std::round((x + y) / 2.0)),
        static_cast<int>(std::round((-x + y) / 2.0)),
    };
}

}
