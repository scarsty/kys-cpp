#include "ChessRunRandom.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>

using namespace KysChess;

TEST_CASE("run RNG matches the version-one xoshiro golden vectors", "[chess][determinism][rng]")
{
    ChessRunRandom random(0x3039ULL);
    constexpr std::array<std::uint64_t, 5> expected{
        0x1f56f5249474b9ceULL,
        0x9f5f782de9ec157bULL,
        0x6351dce75457538dULL,
        0xec8bdb12166c6036ULL,
        0xaeb1bd87f0b50214ULL,
    };

    for (const auto value : expected)
    {
        CHECK(random.nextRaw(ChessRngStream::Shop) == value);
    }
    CHECK(random.streamState(ChessRngStream::Shop).rawDrawCount == expected.size());
}

TEST_CASE("run RNG bounded sampling counts rejected raw draws", "[chess][determinism][rng]")
{
    ChessRunRandom random(0x3039ULL);

    CHECK(random.nextBounded(ChessRngStream::Shop, 0x8000000000000001ULL) == 0x1f5f782de9ec157aULL);
    CHECK(random.streamState(ChessRngStream::Shop).rawDrawCount == 2);
}

TEST_CASE("run RNG streams are isolated", "[chess][determinism][rng]")
{
    ChessRunRandom interleaved(0x3039ULL);
    ChessRunRandom shopOnly(0x3039ULL);

    CHECK(interleaved.nextRaw(ChessRngStream::Shop) == shopOnly.nextRaw(ChessRngStream::Shop));
    for (int i = 0; i < 20; ++i)
    {
        interleaved.nextRaw(ChessRngStream::Reward);
        interleaved.nextRaw(ChessRngStream::EnemyLineup);
    }
    CHECK(interleaved.nextRaw(ChessRngStream::Shop) == shopOnly.nextRaw(ChessRngStream::Shop));
    CHECK(interleaved.streamState(ChessRngStream::Shop).rawDrawCount == 2);
}

TEST_CASE("enemy reroll rekeys only preparation streams", "[chess][determinism][rng]")
{
    ChessRunRandom random(0x3039ULL);
    random.nextRaw(ChessRngStream::Shop);
    random.nextRaw(ChessRngStream::Reward);
    const auto shopState = random.streamState(ChessRngStream::Shop);
    const auto rewardState = random.streamState(ChessRngStream::Reward);

    random.rerollEnemySeed();

    CHECK(random.enemyPlanKey() == 0x055e29722abb6cf0ULL);
    CHECK(random.streamState(ChessRngStream::Shop) == shopState);
    CHECK(random.streamState(ChessRngStream::Reward) == rewardState);
    CHECK(random.streamState(ChessRngStream::EnemyReroll).rawDrawCount == 1);
    CHECK(random.streamState(ChessRngStream::EnemyLineup).rawDrawCount == 0);
    CHECK(random.nextRaw(ChessRngStream::EnemyLineup) == 0xb6fe155d9f2b5cb1ULL);
}

TEST_CASE("preparation checkpoint restores complete stream state", "[chess][determinism][rng]")
{
    ChessRunRandom random(77);
    random.nextRaw(ChessRngStream::EnemyLineup);
    random.nextRaw(ChessRngStream::EnemyEquipment);
    const auto checkpoint = random.checkpointPreparation();

    const auto expectedLineup = random.nextRaw(ChessRngStream::EnemyLineup);
    const auto expectedEquipment = random.nextRaw(ChessRngStream::EnemyEquipment);
    random.nextRaw(ChessRngStream::MapSelection);
    random.nextRaw(ChessRngStream::BattleSeed);

    random.restorePreparation(checkpoint);

    CHECK(random.nextRaw(ChessRngStream::EnemyLineup) == expectedLineup);
    CHECK(random.nextRaw(ChessRngStream::EnemyEquipment) == expectedEquipment);
    CHECK(random.streamState(ChessRngStream::MapSelection).rawDrawCount == 0);
    CHECK(random.streamState(ChessRngStream::BattleSeed).rawDrawCount == 0);
}

TEST_CASE("run RNG state round trip restores counters and values", "[chess][determinism][rng]")
{
    ChessRunRandom original(0);
    original.nextRaw(ChessRngStream::Shop);
    original.nextRaw(ChessRngStream::EnemyEquipment);
    original.rerollEnemySeed();
    original.nextRaw(ChessRngStream::BattleSeed);

    ChessRunRandom restored(0);
    restored.restore(original.state());

    CHECK(restored.rootSeed() == 0);
    CHECK(restored.enemyPlanKey() == original.enemyPlanKey());
    for (std::uint64_t tag = 1; tag <= 7; ++tag)
    {
        const auto stream = static_cast<ChessRngStream>(tag);
        CHECK(restored.streamState(stream) == original.streamState(stream));
        CHECK(restored.nextRaw(stream) == original.nextRaw(stream));
    }
}
