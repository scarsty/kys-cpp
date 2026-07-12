#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <compare>

namespace KysChess
{

enum class ChessRngStream : std::uint64_t
{
    Shop = 1,
    Reward = 2,
    EnemyLineup = 3,
    EnemyEquipment = 4,
    MapSelection = 5,
    BattleSeed = 6,
    EnemyReroll = 7,
};

struct ChessRngStreamState
{
    std::array<std::uint64_t, 4> words{};
    std::uint64_t rawDrawCount = 0;

    auto operator<=>(const ChessRngStreamState&) const = default;
};

struct ChessRunRandomCheckpoint
{
    std::uint64_t enemyPlanKey = 0;
    std::array<ChessRngStreamState, 4> preparationStreams{};

    auto operator<=>(const ChessRunRandomCheckpoint&) const = default;
};

struct ChessRunRandomState
{
    std::uint64_t rootSeed{};
    std::uint64_t enemyPlanKey{};
    std::array<ChessRngStreamState, 7> streams{};

    auto operator<=>(const ChessRunRandomState&) const = default;
};

class ChessRunRandom
{
public:
    explicit ChessRunRandom(std::uint64_t rootSeed);

    std::uint64_t rootSeed() const { return rootSeed_; }
    std::uint64_t enemyPlanKey() const { return enemyPlanKey_; }
    const ChessRngStreamState& streamState(ChessRngStream stream) const;

    std::uint64_t nextRaw(ChessRngStream stream);
    std::uint64_t nextBounded(ChessRngStream stream, std::uint64_t upperBound);
    int nextInt(ChessRngStream stream, int upperBound);

    void rerollEnemySeed();
    ChessRunRandomCheckpoint checkpointPreparation() const;
    ChessRunRandomState state() const;
    void restore(const ChessRunRandomState& state);
    void restorePreparation(const ChessRunRandomCheckpoint& checkpoint);

private:
    static std::size_t indexOf(ChessRngStream stream);
    ChessRngStreamState makeInitialState(ChessRngStream stream) const;
    void resetStream(ChessRngStream stream);

    std::uint64_t rootSeed_{};
    std::uint64_t enemyPlanKey_ = 0;
    std::array<ChessRngStreamState, 7> streams_{};
};

}
