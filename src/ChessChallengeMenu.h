#pragma once

#include "ChessBalance.h"
#include "ChessMenuHelpers.h"
#include "ChessProgress.h"

namespace KysChess
{

constexpr int kChallengeMenuItemsPerPage = 9;

struct ChallengeMenuSetup
{
    IndexedMenuData data;
    IndexedMenuConfig config;
};

ChallengeMenuSetup buildChallengeMenuSetup(
    const std::vector<BalanceConfig::ChallengeDef>& challenges,
    const ChessProgress& progress,
    PanelAnchor menuAnchor);

}    // namespace KysChess
