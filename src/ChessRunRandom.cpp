#include "ChessRunRandom.h"

#include <cassert>

namespace KysChess
{
namespace
{

constexpr std::uint64_t kSplitMixIncrement = 0x9E3779B97F4A7C15ULL;
constexpr std::uint64_t kStreamMultiplier = 0xD2B74407B1CE6E93ULL;

std::uint64_t splitMix64(std::uint64_t& state)
{
    std::uint64_t z = (state += kSplitMixIncrement);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

std::uint64_t rotateLeft(std::uint64_t value, int count)
{
    return (value << count) | (value >> (64 - count));
}

}

ChessRunRandom::ChessRunRandom(std::uint64_t rootSeed)
    : rootSeed_(rootSeed)
{
    for (std::uint64_t tag = 1; tag <= streams_.size(); ++tag)
    {
        resetStream(static_cast<ChessRngStream>(tag));
    }
}

std::size_t ChessRunRandom::indexOf(ChessRngStream stream)
{
    const auto tag = static_cast<std::uint64_t>(stream);
    assert(tag >= 1 && tag <= 7);
    return static_cast<std::size_t>(tag - 1);
}

ChessRngStreamState ChessRunRandom::makeInitialState(ChessRngStream stream) const
{
    const std::uint64_t tag = static_cast<std::uint64_t>(stream);
    const bool usesEnemyPlanKey = tag >= 3 && tag <= 6;
    std::uint64_t splitMixState = rootSeed_ ^ (kStreamMultiplier * tag)
        ^ (usesEnemyPlanKey ? enemyPlanKey_ : 0ULL);

    ChessRngStreamState result;
    for (auto& word : result.words)
    {
        word = splitMix64(splitMixState);
    }
    if (result.words[0] == 0 && result.words[1] == 0 && result.words[2] == 0 && result.words[3] == 0)
    {
        result.words[0] = kSplitMixIncrement;
    }
    return result;
}

void ChessRunRandom::resetStream(ChessRngStream stream)
{
    streams_[indexOf(stream)] = makeInitialState(stream);
}

const ChessRngStreamState& ChessRunRandom::streamState(ChessRngStream stream) const
{
    return streams_[indexOf(stream)];
}

std::uint64_t ChessRunRandom::nextRaw(ChessRngStream stream)
{
    auto& state = streams_[indexOf(stream)];
    const std::uint64_t result = rotateLeft(state.words[1] * 5ULL, 7) * 9ULL;
    const std::uint64_t t = state.words[1] << 17;

    state.words[2] ^= state.words[0];
    state.words[3] ^= state.words[1];
    state.words[1] ^= state.words[2];
    state.words[0] ^= state.words[3];
    state.words[2] ^= t;
    state.words[3] = rotateLeft(state.words[3], 45);
    ++state.rawDrawCount;
    return result;
}

std::uint64_t ChessRunRandom::nextBounded(ChessRngStream stream, std::uint64_t upperBound)
{
    assert(upperBound > 0);
    const std::uint64_t threshold = (0ULL - upperBound) % upperBound;
    for (;;)
    {
        const std::uint64_t value = nextRaw(stream);
        if (value >= threshold)
        {
            return value % upperBound;
        }
    }
}

int ChessRunRandom::nextInt(ChessRngStream stream, int upperBound)
{
    assert(upperBound > 0);
    return static_cast<int>(nextBounded(stream, static_cast<std::uint64_t>(upperBound)));
}

std::uint64_t ChessRunRandom::enemyPlanIdentity() const
{
    std::uint64_t state = rootSeed_
        ^ rotateLeft(enemyPlanKey_ + 0xA0761D6478BD642FULL, 23);
    const auto identity = splitMix64(state);
    return identity != 0 ? identity : 0xE7037ED1A0B428DBULL;
}

void ChessRunRandom::rerollEnemySeed()
{
    enemyPlanKey_ = nextRaw(ChessRngStream::EnemyReroll);
    resetStream(ChessRngStream::EnemyLineup);
    resetStream(ChessRngStream::EnemyEquipment);
    resetStream(ChessRngStream::MapSelection);
    resetStream(ChessRngStream::BattleSeed);
}

ChessRunRandomCheckpoint ChessRunRandom::checkpointPreparation() const
{
    ChessRunRandomCheckpoint checkpoint;
    checkpoint.enemyPlanKey = enemyPlanKey_;
    for (std::uint64_t tag = 3; tag <= 6; ++tag)
    {
        checkpoint.preparationStreams[static_cast<std::size_t>(tag - 3)] =
            streamState(static_cast<ChessRngStream>(tag));
    }
    return checkpoint;
}

ChessRunRandomState ChessRunRandom::state() const
{
    return {rootSeed_, enemyPlanKey_, streams_};
}

void ChessRunRandom::restore(const ChessRunRandomState& state)
{
    rootSeed_ = state.rootSeed;
    enemyPlanKey_ = state.enemyPlanKey;
    streams_ = state.streams;
}

void ChessRunRandom::restorePreparation(const ChessRunRandomCheckpoint& checkpoint)
{
    enemyPlanKey_ = checkpoint.enemyPlanKey;
    for (std::uint64_t tag = 3; tag <= 6; ++tag)
    {
        streams_[static_cast<std::size_t>(tag - 1)] =
            checkpoint.preparationStreams[static_cast<std::size_t>(tag - 3)];
    }
}

}
