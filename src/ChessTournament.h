#pragma once

#include "BattleSummaryBuilder.h"
#include "ChessPvp.h"
#include "ChessReplayHash.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace KysChess
{

inline constexpr int kDefaultChessTournamentBattleSeedCount = 3;

struct ChessTournamentSubmission
{
    std::string name;
    std::string source;
    ChessSha256 payloadHash{};
    ChessPvpComposition composition;

    auto operator<=>(const ChessTournamentSubmission&) const = default;
};

struct ChessTournamentOptions
{
    std::uint64_t seed = 1;
    int battleSeedCount = kDefaultChessTournamentBattleSeedCount;

    auto operator<=>(const ChessTournamentOptions&) const = default;
};

enum class ChessTournamentBattleOutcome
{
    TeamZeroVictory,
    TeamOneVictory,
    Draw,
};

struct ChessTournamentBattleRecord
{
    int pairingIndex{};
    int seedSequenceIndex{};
    std::uint32_t battleSeed{};
    int leg{};
    int teamZeroEntrantIndex = -1;
    int teamOneEntrantIndex = -1;
    ChessTournamentBattleOutcome outcome{};
    Battle::BattleOutcome engineOutcome = Battle::BattleOutcome::InProgress;
    int endFrame{};
    ChessSha256 digest{};

    auto operator<=>(const ChessTournamentBattleRecord&) const = default;
};

struct ChessTournamentPairingResult
{
    int firstEntrantIndex = -1;
    int secondEntrantIndex = -1;
    int firstHalfPoints{};
    int secondHalfPoints{};

    auto operator<=>(const ChessTournamentPairingResult&) const = default;
};

struct ChessTournamentRoundRobinResult
{
    std::vector<std::uint32_t> battleSeeds;
    std::vector<ChessTournamentPairingResult> pairings;
    std::vector<ChessTournamentBattleRecord> battles;

    auto operator<=>(const ChessTournamentRoundRobinResult&) const = default;
};

struct ChessTournamentStanding
{
    int rank{};
    int entrantIndex = -1;
    int halfPoints{};
    int battleWins{};
    int battleDraws{};
    int battleLosses{};

    auto operator<=>(const ChessTournamentStanding&) const = default;
};

struct ChessTournamentResult
{
    ChessTournamentOptions options;
    std::vector<ChessTournamentSubmission> submissions;
    ChessTournamentRoundRobinResult roundRobin;
    std::vector<ChessTournamentStanding> standings;
    std::vector<int> championEntrantIndices;

    auto operator<=>(const ChessTournamentResult&) const = default;
};

using ChessTournamentProgressCallback = std::function<void(
    std::size_t completedBattleCount,
    const ChessTournamentBattleRecord& battle)>;

ChessTournamentBattleOutcome chessTournamentBattleOutcome(
    const BattleSummary& summary);
std::string chessTournamentHalfPointsText(int halfPoints);
std::string serializeChessTournamentResultJson(
    const ChessTournamentResult& result);

class ChessTournamentRunner
{
public:
    static std::vector<std::uint32_t> deriveBattleSeeds(
        std::uint64_t tournamentSeed,
        int count,
        int firstSeedSequenceIndex = 0);

    static std::optional<ChessTournamentResult> run(
        std::shared_ptr<const ChessGameContent> content,
        std::vector<ChessTournamentSubmission> submissions,
        ChessTournamentOptions options,
        std::string& error,
        ChessTournamentProgressCallback progress = {});

    static std::optional<ChessTournamentBattleRecord> runBattle(
        std::shared_ptr<const ChessGameContent> content,
        const ChessTournamentSubmission& first,
        const ChessTournamentSubmission& second,
        std::uint64_t tournamentSeed,
        int seedSequenceIndex,
        int leg,
        std::string& error);
};

} // namespace KysChess
