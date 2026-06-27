#pragma once

#include "ChessBalance.h"

#include <string>

namespace KysChess
{

std::string buildShopWeightString(const BalanceConfig& config, int level);
std::string buildNextShopWeightString(const BalanceConfig& config, int currentLevel);

}    // namespace KysChess
