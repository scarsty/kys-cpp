#include "ChessShopWeightDisplay.h"

#include <format>

namespace KysChess
{

std::string buildShopWeightString(const BalanceConfig& config, int level)
{
    if (level >= static_cast<int>(config.shopWeights.size()))
    {
        return "已滿級";
    }

    const auto& weights = config.shopWeights[level];
    return std::format("1費{}% 2費{}% 3費{}% 4費{}% 5費{}%", weights[0], weights[1], weights[2], weights[3], weights[4]);
}

std::string buildNextShopWeightString(const BalanceConfig& config, int currentLevel)
{
    return buildShopWeightString(config, currentLevel + 1);
}

}    // namespace KysChess
