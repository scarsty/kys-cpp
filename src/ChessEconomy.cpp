#include "ChessEconomy.h"

#include <algorithm>

namespace KysChess
{

ChessEconomy::ChessEconomy(const GameDataStore& store)
{
    money_ = store.money;
    exp_ = store.exp;
    level_ = store.level;
}

void ChessEconomy::exportTo(GameDataStore& store) const
{
    store.money = money_;
    store.exp = exp_;
    store.level = level_;
}

bool ChessEconomy::spend(int amount)
{
    if (money_ < amount)
    {
        return false;
    }
    money_ -= amount;
    return true;
}

void ChessEconomy::increaseExp(int amount)
{
    auto& cfg = ChessBalance::config();
    exp_ += amount;
    if (level_ < cfg.maxLevel && level_ < static_cast<int>(cfg.expTable.size()) && exp_ >= cfg.expTable[level_])
    {
        exp_ -= cfg.expTable[level_];
        level_ += 1;
    }
}

int ChessEconomy::getExpForNextLevel() const
{
    auto& cfg = ChessBalance::config();
    if (level_ < cfg.maxLevel && level_ < static_cast<int>(cfg.expTable.size()))
    {
        return cfg.expTable[level_];
    }
    return exp_;
}

int ChessEconomy::getMaxDeploy() const
{
    return std::max(level_ + 1, ChessBalance::config().minBattleSize);
}

}