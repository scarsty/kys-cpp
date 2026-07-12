#include "battle/BattleRuntimeRandom.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess::Battle;

TEST_CASE("battle runtime RNG matches mt19937 modulo version one", "[battle][determinism][rng]")
{
    BattleRuntimeRandom random(1);

    CHECK(random.seed() == 1);
    CHECK(random.nextInt(100) == 45);
    CHECK_FALSE(random.chance(50));
    CHECK(random.symmetricInt(10) == -4);
    CHECK(random.rawDrawCount() == 4);
}

TEST_CASE("battle runtime RNG percentage and boundary chances are fixed", "[battle][determinism][rng]")
{
    BattleRuntimeRandom random(5489);

    CHECK(random.nextPercent() == 16.12);
    CHECK_FALSE(random.chance(0));
    CHECK(random.chance(100));
    CHECK(random.rawDrawCount() == 1);
}

TEST_CASE("battle runtime RNG restores by raw draw counter", "[battle][determinism][rng]")
{
    BattleRuntimeRandom random(1);
    CHECK(random.nextInt(100) == 45);
    const int expected = random.nextInt(10000);

    random.restore(1);

    CHECK(random.nextInt(10000) == expected);
    CHECK(expected == 6139);
    CHECK(random.rawDrawCount() == 2);
}
