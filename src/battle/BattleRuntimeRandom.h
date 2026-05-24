#pragma once

#include <random>

namespace KysChess::Battle
{

class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(unsigned int seed = 1);

    unsigned int seed() const;
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::mt19937 rand_;
};

}  // namespace KysChess::Battle
