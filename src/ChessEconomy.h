#pragma once

#include "ChessBalance.h"
#include "GameDataStore.h"

namespace KysChess
{

class ChessEconomy
{
public:
    ChessEconomy() = default;
    explicit ChessEconomy(const GameDataStore& store);

    void exportTo(GameDataStore& store) const;

    bool spend(int amount);
    void make(int amount) { money_ += amount; }
    int getMoney() const { return money_; }
    void setMoney(int value) { money_ = value; }

    void increaseExp(int amount);
    int getExp() const { return exp_; }
    int getExpForNextLevel() const;
    int getLevel() const { return level_; }
    void setExp(int value) { exp_ = value; }
    void setLevel(int value) { level_ = value; }
    int getMaxDeploy() const;

private:
    int money_ = 0;
    int exp_ = 0;
    int level_ = 0;
};

}