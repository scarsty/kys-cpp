#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Chess.h"
#include "ChessBalance.h"
#include "Engine.h"

namespace KysChess
{

struct ChessMenuEntry {
    Chess chess;
    std::string prefix;
};

struct IndexedMenuData {
    std::vector<std::string> labels;
    std::vector<Color> colors;
};

struct ChessMenuData : IndexedMenuData {
    std::vector<Chess> previewData;
};

class ChessSelectorPresenter
{
public:
    int getDisplayWidth(const std::string& text) const;
    std::pair<std::string, Color> formatChessName(Role* role,
        int tier,
        std::optional<int> starOpt,
        std::optional<int> countOpt,
        const std::string& prefix = "") const;
    ChessMenuData buildChessMenuData(const std::vector<ChessMenuEntry>& entries) const;
    std::string challengeRewardDesc(const BalanceConfig::ChallengeReward& reward) const;
    std::string buildEquippedBy(const std::map<ChessInstanceID, Chess>& collection, int itemId) const;
};

}