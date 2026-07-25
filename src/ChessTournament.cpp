#include "ChessTournament.h"

#include "BattleSetupFactory.h"
#include "ChessRunRandom.h"
#include "ChessRuntimeConstants.h"
#include "ChessStandaloneBattle.h"
#include "HeadlessBattleRunner.h"

#include <glaze/json.hpp>

#include <algorithm>
#include <cassert>
#include <climits>
#include <format>
#include <ranges>
#include <utility>

namespace KysChess
{
namespace TournamentDetail
{

struct TournamentJsonSeed
{
    int sequence_index{};
    std::uint32_t battle_seed{};
};

struct TournamentJsonSubmission
{
    int index{};
    std::string name;
    std::string source;
    std::string payload_sha256;
};

struct TournamentJsonBattle
{
    int pairing_index{};
    int seed_sequence_index{};
    std::uint32_t battle_seed{};
    int leg{};
    int team_zero_entrant_index{};
    int team_one_entrant_index{};
    std::string outcome;
    std::string engine_outcome;
    int end_frame{};
    std::string digest_sha256;
};

struct TournamentJsonPairing
{
    int first_entrant_index{};
    int second_entrant_index{};
    int first_half_points{};
    int second_half_points{};
};

struct TournamentJsonStanding
{
    int rank{};
    int entrant_index{};
    std::string name;
    int half_points{};
    std::string points;
    int battle_wins{};
    int battle_draws{};
    int battle_losses{};
};

struct TournamentJsonResult
{
    int format_version = 2;
    std::string format = "full_round_robin_mirrored";
    std::string entrant_order = "utf8_filename_ascending";
    std::string side_assignment = "leg_0_first_entrant_is_team_zero; leg_1_swaps_sides";
    std::string tournament_seed;
    int battle_seed_count{};
    std::vector<TournamentJsonSubmission> submissions;
    std::vector<TournamentJsonSeed> battle_seeds;
    std::vector<TournamentJsonPairing> pairings;
    std::vector<TournamentJsonBattle> battles;
    std::vector<TournamentJsonStanding> standings;
    std::vector<int> champion_entrant_indices;
};

struct TournamentJsonWriteOptions : glz::opts
{
    std::uint8_t indentation_width = 2;
};

constexpr auto kTournamentJsonWriteOptions = []
{
    TournamentJsonWriteOptions options;
    options.prettify = true;
    return options;
}();

std::string tournamentBattleOutcomeText(ChessTournamentBattleOutcome outcome)
{
    switch (outcome)
    {
    case ChessTournamentBattleOutcome::TeamZeroVictory: return "team_zero_victory";
    case ChessTournamentBattleOutcome::TeamOneVictory: return "team_one_victory";
    case ChessTournamentBattleOutcome::Draw: return "draw";
    }
    std::unreachable();
}

std::string engineOutcomeText(Battle::BattleOutcome outcome)
{
    switch (outcome)
    {
    case Battle::BattleOutcome::InProgress: return "in_progress";
    case Battle::BattleOutcome::PlayerVictory: return "team_zero_victory";
    case Battle::BattleOutcome::PlayerDefeat: return "team_one_victory";
    case Battle::BattleOutcome::Timeout: return "timeout";
    }
    std::unreachable();
}

void addBattleScore(
    ChessTournamentPairingResult& pairing,
    const ChessTournamentBattleRecord& battle)
{
    const bool firstIsTeamZero = pairing.firstEntrantIndex == battle.teamZeroEntrantIndex;
    assert(firstIsTeamZero
        || pairing.firstEntrantIndex == battle.teamOneEntrantIndex);
    switch (battle.outcome)
    {
    case ChessTournamentBattleOutcome::TeamZeroVictory:
        (firstIsTeamZero ? pairing.firstHalfPoints : pairing.secondHalfPoints) += 2;
        break;
    case ChessTournamentBattleOutcome::TeamOneVictory:
        (firstIsTeamZero ? pairing.secondHalfPoints : pairing.firstHalfPoints) += 2;
        break;
    case ChessTournamentBattleOutcome::Draw:
        ++pairing.firstHalfPoints;
        ++pairing.secondHalfPoints;
        break;
    }
}

std::optional<ChessTournamentBattleRecord> executeBattle(
    std::shared_ptr<const ChessGameContent> content,
    const std::vector<ChessTournamentSubmission>& submissions,
    int firstEntrantIndex,
    int secondEntrantIndex,
    std::uint64_t tournamentSeed,
    int pairingIndex,
    int seedSequenceIndex,
    std::uint32_t battleSeed,
    int leg,
    std::string& error)
{
    assert(content);
    assert(firstEntrantIndex >= 0);
    assert(secondEntrantIndex >= 0);
    assert(firstEntrantIndex < static_cast<int>(submissions.size()));
    assert(secondEntrantIndex < static_cast<int>(submissions.size()));
    assert(leg == 0 || leg == 1);

    const int teamZeroEntrantIndex = leg == 0 ? firstEntrantIndex : secondEntrantIndex;
    const int teamOneEntrantIndex = leg == 0 ? secondEntrantIndex : firstEntrantIndex;

    ChessStandaloneBattleRequest request;
    request.stableBattleId = std::format(
        "tournament:{}:{}",
        seedSequenceIndex,
        leg);
    request.rootSeed = tournamentSeed;
    request.mapId = ChessPvpMapLayout::BattleId;
    request.battleSeed = battleSeed;
    request.layout = PreparedChessBattleLayout::PvpArena;
    request.teams[0] = chessStandaloneBattleTeam(
        submissions[teamZeroEntrantIndex].composition);
    request.teams[1] = chessStandaloneBattleTeam(
        submissions[teamOneEntrantIndex].composition);

    auto build = ChessStandaloneBattle::prepare(content, request, error);
    if (!build)
    {
        error = std::format(
            "無法建立 {} 與 {} 的賽事戰鬥：{}",
            submissions[firstEntrantIndex].name,
            submissions[secondEntrantIndex].name,
            error);
        return std::nullopt;
    }
    auto input = BattleSetupFactory::build(
        build->preparedBattle,
        *build->content,
        kChessBattleFrameLimit);
    const auto result = HeadlessBattleRunner::run(std::move(input));

    ChessTournamentBattleRecord record;
    record.pairingIndex = pairingIndex;
    record.seedSequenceIndex = seedSequenceIndex;
    record.battleSeed = battleSeed;
    record.leg = leg;
    record.teamZeroEntrantIndex = teamZeroEntrantIndex;
    record.teamOneEntrantIndex = teamOneEntrantIndex;
    record.outcome = chessTournamentBattleOutcome(result.summary);
    record.engineOutcome = result.summary.outcome;
    record.endFrame = result.summary.endFrame;
    record.digest = result.digest;
    return record;
}

std::optional<ChessTournamentRoundRobinResult> runRoundRobin(
    std::shared_ptr<const ChessGameContent> content,
    const std::vector<ChessTournamentSubmission>& submissions,
    const ChessTournamentOptions& options,
    std::size_t& completedBattleCount,
    const ChessTournamentProgressCallback& progress,
    std::string& error)
{
    ChessTournamentRoundRobinResult result;
    result.battleSeeds = ChessTournamentRunner::deriveBattleSeeds(
        options.seed,
        options.battleSeedCount);

    int pairingIndex = 0;
    for (int first = 0; first < static_cast<int>(submissions.size()); ++first)
    {
        for (int second = first + 1; second < static_cast<int>(submissions.size()); ++second)
        {
            ChessTournamentPairingResult pairing;
            pairing.firstEntrantIndex = first;
            pairing.secondEntrantIndex = second;
            for (int seedIndex = 0; seedIndex < static_cast<int>(result.battleSeeds.size()); ++seedIndex)
            {
                for (int leg = 0; leg < 2; ++leg)
                {
                    auto battle = executeBattle(
                        content,
                        submissions,
                        pairing.firstEntrantIndex,
                        pairing.secondEntrantIndex,
                        options.seed,
                        pairingIndex,
                        seedIndex,
                        result.battleSeeds[seedIndex],
                        leg,
                        error);
                    if (!battle)
                    {
                        return std::nullopt;
                    }
                    addBattleScore(pairing, *battle);
                    result.battles.push_back(std::move(*battle));
                    ++completedBattleCount;
                    if (progress)
                    {
                        progress(completedBattleCount, result.battles.back());
                    }
                }
            }
            result.pairings.push_back(std::move(pairing));
            ++pairingIndex;
        }
    }
    return result;
}

std::vector<ChessTournamentStanding> tournamentStandings(
    int entrantCount,
    const ChessTournamentRoundRobinResult& roundRobin)
{
    std::vector<ChessTournamentStanding> result(entrantCount);
    for (int index = 0; index < entrantCount; ++index)
    {
        result[index].entrantIndex = index;
    }
    for (const auto& pairing : roundRobin.pairings)
    {
        result[pairing.firstEntrantIndex].halfPoints += pairing.firstHalfPoints;
        result[pairing.secondEntrantIndex].halfPoints += pairing.secondHalfPoints;
    }
    for (const auto& battle : roundRobin.battles)
    {
        auto& teamZero = result[battle.teamZeroEntrantIndex];
        auto& teamOne = result[battle.teamOneEntrantIndex];
        switch (battle.outcome)
        {
        case ChessTournamentBattleOutcome::TeamZeroVictory:
            ++teamZero.battleWins;
            ++teamOne.battleLosses;
            break;
        case ChessTournamentBattleOutcome::TeamOneVictory:
            ++teamOne.battleWins;
            ++teamZero.battleLosses;
            break;
        case ChessTournamentBattleOutcome::Draw:
            ++teamZero.battleDraws;
            ++teamOne.battleDraws;
            break;
        }
    }

    std::ranges::sort(result, [](const auto& lhs, const auto& rhs)
        {
            if (lhs.halfPoints != rhs.halfPoints)
            {
                return lhs.halfPoints > rhs.halfPoints;
            }
            return lhs.entrantIndex < rhs.entrantIndex;
        });

    int rank = 1;
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        if (index > 0
            && result[index].halfPoints != result[index - 1].halfPoints)
        {
            rank = static_cast<int>(index) + 1;
        }
        result[index].rank = rank;
    }
    return result;
}

TournamentJsonResult tournamentJsonResult(const ChessTournamentResult& source)
{
    TournamentJsonResult result;
    result.tournament_seed = std::format("0x{:016x}", source.options.seed);
    result.battle_seed_count = source.options.battleSeedCount;
    result.champion_entrant_indices = source.championEntrantIndices;

    for (int index = 0; index < static_cast<int>(source.submissions.size()); ++index)
    {
        const auto& submission = source.submissions[index];
        result.submissions.push_back({
            index,
            submission.name,
            submission.source,
            chessSha256Hex(submission.payloadHash),
        });
    }
    for (int index = 0; index < static_cast<int>(source.roundRobin.battleSeeds.size()); ++index)
    {
        result.battle_seeds.push_back({index, source.roundRobin.battleSeeds[index]});
    }
    for (const auto& sourcePairing : source.roundRobin.pairings)
    {
        result.pairings.push_back({
            sourcePairing.firstEntrantIndex,
            sourcePairing.secondEntrantIndex,
            sourcePairing.firstHalfPoints,
            sourcePairing.secondHalfPoints,
        });
    }
    for (const auto& sourceBattle : source.roundRobin.battles)
    {
        result.battles.push_back({
            sourceBattle.pairingIndex,
            sourceBattle.seedSequenceIndex,
            sourceBattle.battleSeed,
            sourceBattle.leg,
            sourceBattle.teamZeroEntrantIndex,
            sourceBattle.teamOneEntrantIndex,
            tournamentBattleOutcomeText(sourceBattle.outcome),
            engineOutcomeText(sourceBattle.engineOutcome),
            sourceBattle.endFrame,
            chessSha256Hex(sourceBattle.digest),
        });
    }
    for (const auto& sourceStanding : source.standings)
    {
        result.standings.push_back({
            sourceStanding.rank,
            sourceStanding.entrantIndex,
            source.submissions[sourceStanding.entrantIndex].name,
            sourceStanding.halfPoints,
            chessTournamentHalfPointsText(sourceStanding.halfPoints),
            sourceStanding.battleWins,
            sourceStanding.battleDraws,
            sourceStanding.battleLosses,
        });
    }
    return result;
}

} // namespace TournamentDetail

using namespace TournamentDetail;

ChessTournamentBattleOutcome chessTournamentBattleOutcome(
    const BattleSummary& summary)
{
    if (summary.outcome == Battle::BattleOutcome::PlayerVictory)
    {
        return ChessTournamentBattleOutcome::TeamZeroVictory;
    }
    assert(summary.outcome == Battle::BattleOutcome::PlayerDefeat
        || summary.outcome == Battle::BattleOutcome::Timeout);
    return ChessTournamentBattleOutcome::TeamOneVictory;
}

std::string chessTournamentHalfPointsText(int halfPoints)
{
    assert(halfPoints >= 0);
    return halfPoints % 2 == 0 ? std::to_string(halfPoints / 2) : std::format("{}.5", halfPoints / 2);
}

std::string serializeChessTournamentResultJson(
    const ChessTournamentResult& result)
{
    const auto json = glz::write<kTournamentJsonWriteOptions>(
        tournamentJsonResult(result));
    return json ? json.value() : std::string{};
}

std::vector<std::uint32_t> ChessTournamentRunner::deriveBattleSeeds(
    std::uint64_t tournamentSeed,
    int count,
    int firstSeedSequenceIndex)
{
    assert(count > 0);
    assert(firstSeedSequenceIndex >= 0);
    ChessRunRandom random(tournamentSeed);
    for (int index = 0; index < firstSeedSequenceIndex; ++index)
    {
        random.nextBounded(
            ChessRngStream::BattleSeed,
            static_cast<std::uint64_t>(UINT_MAX) + 1);
    }
    std::vector<std::uint32_t> result;
    result.reserve(count);
    for (int index = 0; index < count; ++index)
    {
        result.push_back(static_cast<std::uint32_t>(random.nextBounded(
            ChessRngStream::BattleSeed,
            static_cast<std::uint64_t>(UINT_MAX) + 1)));
    }
    return result;
}

std::optional<ChessTournamentResult> ChessTournamentRunner::run(
    std::shared_ptr<const ChessGameContent> content,
    std::vector<ChessTournamentSubmission> submissions,
    ChessTournamentOptions options,
    std::string& error,
    ChessTournamentProgressCallback progress)
{
    error.clear();
    if (!content)
    {
        error = "沒有可用的自走棋規則內容";
        return std::nullopt;
    }
    if (content->difficulty() != Difficulty::Hard)
    {
        error = "賽事只接受困難模式規則內容";
        return std::nullopt;
    }
    if (submissions.size() < 2)
    {
        error = "賽事至少需要兩份存檔";
        return std::nullopt;
    }
    if (options.battleSeedCount <= 0)
    {
        error = "每組配對的戰鬥種子數量必須大於零";
        return std::nullopt;
    }
    for (const auto& submission : submissions)
    {
        if (submission.composition.pieces.empty())
        {
            error = std::format("參賽者 {} 沒有出戰棋子", submission.name);
            return std::nullopt;
        }
    }

    ChessTournamentResult result;
    result.options = options;
    result.submissions = std::move(submissions);
    std::size_t completedBattleCount{};
    auto roundRobin = runRoundRobin(
        content,
        result.submissions,
        options,
        completedBattleCount,
        progress,
        error);
    if (!roundRobin)
    {
        return std::nullopt;
    }
    result.roundRobin = std::move(*roundRobin);
    result.standings = tournamentStandings(
        static_cast<int>(result.submissions.size()),
        result.roundRobin);

    const int bestScore = result.standings.front().halfPoints;
    for (const auto& standing : result.standings)
    {
        if (standing.halfPoints == bestScore)
        {
            result.championEntrantIndices.push_back(standing.entrantIndex);
        }
    }
    return result;
}

std::optional<ChessTournamentBattleRecord> ChessTournamentRunner::runBattle(
    std::shared_ptr<const ChessGameContent> content,
    const ChessTournamentSubmission& first,
    const ChessTournamentSubmission& second,
    std::uint64_t tournamentSeed,
    int seedSequenceIndex,
    int leg,
    std::string& error)
{
    error.clear();
    if (!content)
    {
        error = "沒有可用的自走棋規則內容";
        return std::nullopt;
    }
    if (seedSequenceIndex < 0)
    {
        error = "戰鬥種子序號不可小於零";
        return std::nullopt;
    }
    if (leg != 0 && leg != 1)
    {
        error = "鏡像場次必須是 0 或 1";
        return std::nullopt;
    }
    const std::vector submissions{first, second};
    const auto seed = deriveBattleSeeds(
        tournamentSeed,
        1,
        seedSequenceIndex).front();
    return executeBattle(
        std::move(content),
        submissions,
        0,
        1,
        tournamentSeed,
        0,
        seedSequenceIndex,
        seed,
        leg,
        error);
}

} // namespace KysChess
