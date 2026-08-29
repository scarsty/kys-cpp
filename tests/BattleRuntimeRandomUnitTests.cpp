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

TEST_CASE("BattleRuntimeRandom_ReplaysFromSeed", "[battle][random]")
{
    BattleRuntimeRandom first(1234u);
    const int firstA = first.nextInt(1000);
    const int firstB = first.nextInt(1000);
    const double firstC = first.nextPercent();

    BattleRuntimeRandom second(1234u);
    CHECK(second.nextInt(1000) == firstA);
    CHECK(second.nextInt(1000) == firstB);
    CHECK(second.nextPercent() == firstC);

    BattleRuntimeRandom different(1235u);
    CHECK(different.nextInt(1000) != firstA);
}

TEST_CASE("BattleRuntimeRandom_ChanceHandlesGuaranteedOutcomes", "[battle][random]")
{
    BattleRuntimeRandom random(99u);

    CHECK_FALSE(random.chance(0));
    CHECK(random.chance(100));

    BattleRuntimeRandom first(99u);
    BattleRuntimeRandom second(99u);
    CHECK(first.chance(50) == second.chance(50));
}
