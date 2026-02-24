#pragma once

#include "ChessBattleEffects.h"
#include <map>
#include <string>
#include <vector>

namespace KysChess
{

struct NeigongDef
{
    int magicId;
    int itemId;
    int tier;
    std::string name;
    std::vector<ComboEffect> effects;
};

struct NeigongConfig
{
    int rerollCost = 4;
    int choiceCount = 3;
    std::map<int, std::vector<int>> tiersByBoss;
};

class ChessNeigong
{
public:
    static const NeigongConfig& config();
    static const std::vector<NeigongDef>& getPool();

private:
    static inline NeigongConfig config_;
    static inline std::vector<NeigongDef> pool_;
    static inline bool configLoaded_ = false;
};

}    // namespace KysChess
