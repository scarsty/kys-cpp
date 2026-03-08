#include "ChessRandom.h"

#include <random>

namespace KysChess
{

ChessRandom::ChessRandom()
{
    std::random_device rd;
    shopSeed_ = rd();
    enemySeed_ = rd();
    shopCallCount_ = 0;
    enemyCallCount_ = 0;
    shopRand_.set_seed(shopSeed_);
    enemyRand_.set_seed(enemySeed_);
}

ChessRandom::ChessRandom(const GameDataStore& store)
{
    shopSeed_ = store.shopSeed;
    shopCallCount_ = store.shopCallCount;
    enemySeed_ = store.enemySeed;
    enemyCallCount_ = store.enemyCallCount;
    restore();
}

void ChessRandom::exportTo(GameDataStore& store) const
{
    store.shopSeed = shopSeed_;
    store.shopCallCount = shopCallCount_;
    store.enemySeed = enemySeed_;
    store.enemyCallCount = enemyCallCount_;
}

void ChessRandom::restore()
{
    shopRand_.set_seed(shopSeed_);
    shopRand_.get_generator().discard(shopCallCount_);
    enemyRand_.set_seed(enemySeed_);
    enemyRand_.get_generator().discard(enemyCallCount_);
}

int ChessRandom::shopRandInt(int n)
{
    shopCallCount_++;
    return shopRand_.rand_int(n);
}

int ChessRandom::enemyRandInt(int n)
{
    enemyCallCount_++;
    return enemyRand_.rand_int(n);
}

}