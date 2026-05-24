#include "BattleRuntimeRandom.h"

#include <cassert>
#include <cstdint>

namespace KysChess::Battle
{

BattleRuntimeRandom::BattleRuntimeRandom(unsigned int seed)
    : seed_(seed), rand_(seed)
{
}

unsigned int BattleRuntimeRandom::seed() const
{
    return seed_;
}

double BattleRuntimeRandom::nextPercent()
{
    return static_cast<double>(rand_() % 10000u) / 100.0;
}

int BattleRuntimeRandom::nextInt(int upperBound)
{
    assert(upperBound > 0);
    return static_cast<int>(rand_() % static_cast<std::uint32_t>(upperBound));
}

bool BattleRuntimeRandom::chance(int chancePct)
{
    assert(chancePct >= 0);
    assert(chancePct <= 100);
    if (chancePct <= 0)
    {
        return false;
    }
    if (chancePct >= 100)
    {
        return true;
    }
    return nextPercent() < static_cast<double>(chancePct);
}

int BattleRuntimeRandom::symmetricInt(int exclusiveBound)
{
    assert(exclusiveBound > 0);
    return nextInt(exclusiveBound) - nextInt(exclusiveBound);
}

}  // namespace KysChess::Battle
