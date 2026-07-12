#pragma once

#include <random>
#include <cstdint>

namespace KysChess::Battle
{

class BattleRuntimeRandom
{
public:
    explicit BattleRuntimeRandom(unsigned int seed = 1);

    unsigned int seed() const;
    std::uint64_t rawDrawCount() const;
    void restore(std::uint64_t rawDrawCount);
    double nextPercent();
    int nextInt(int upperBound);
    bool chance(int chancePct);
    int symmetricInt(int exclusiveBound);

private:
    unsigned int seed_ = 1;
    std::uint64_t rawDrawCount_ = 0;
    std::mt19937 rand_;
};

}  // namespace KysChess::Battle
