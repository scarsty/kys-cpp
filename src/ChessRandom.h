#pragma once

#include "GameDataStore.h"
#include "Random.h"

namespace KysChess
{

class ChessRandom
{
public:
    ChessRandom();
    explicit ChessRandom(const GameDataStore& store);

    void exportTo(GameDataStore& store) const;
    void restore();

    int shopRandInt(int n);
    int enemyRandInt(int n);

    unsigned int getEnemyCallCount() const { return enemyCallCount_; }
    void setEnemyCallCount(unsigned int count) { enemyCallCount_ = count; }

private:
    unsigned int shopSeed_ = 0;
    unsigned int shopCallCount_ = 0;
    unsigned int enemySeed_ = 0;
    unsigned int enemyCallCount_ = 0;
    Random<> shopRand_;
    Random<> enemyRand_;
};

}