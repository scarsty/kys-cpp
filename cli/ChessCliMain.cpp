#include "ChessCliController.h"
#include "ChessContentLoader.h"
#include "ChessPvp.h"
#include "ChessReplayArchive.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"
#include "ChessTournament.h"
#include "Utf8Path.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <ranges>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{

struct Arguments
{
    std::string command = "play";
    std::filesystem::path replayPath;
    std::vector<std::filesystem::path> tournamentPaths;
    std::filesystem::path outputPath;
    std::filesystem::path jsonOutputPath;
    std::filesystem::path dataRoot;
    std::filesystem::path configRoot;
    KysChess::Difficulty difficulty = KysChess::Difficulty::Normal;
    std::uint64_t seed = 1;
    int battleSeedCount = KysChess::kDefaultChessTournamentBattleSeedCount;
    int seedSequenceIndex{};
    int leg{};
    KysChess::ChessCliOutputMode mode = KysChess::ChessCliOutputMode::Human;
    bool jsonl = false;
};

std::optional<std::uint64_t> parseSeed(std::string_view text)
{
    std::uint64_t value{};
    int base = 10;
    if (text.starts_with("0x"))
    {
        text.remove_prefix(2);
        base = 16;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, base);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size()
        ? std::optional(value)
        : std::nullopt;
}

std::optional<int> parseNonnegativeInt(std::string_view text)
{
    int value{};
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    return result.ec == std::errc{}
        && result.ptr == text.data() + text.size()
        && value >= 0
        ? std::optional(value)
        : std::nullopt;
}

Arguments parseArguments(int argc, char** argv)
{
    Arguments result;
    const auto defaults = KysChess::discoverChessContentRoots(KysChess::currentExecutablePath());
    result.dataRoot = defaults.dataRoot;
    result.configRoot = defaults.configRoot;

    int index = 1;
    if (index < argc && argv[index][0] != '-')
    {
        result.command = argv[index++];
        if ((result.command == "verify" || result.command == "verify-pvp")
            && index < argc)
        {
            result.replayPath = argv[index++];
        }
        else if (result.command == "tournament"
            && index < argc
            && argv[index][0] != '-')
        {
            result.tournamentPaths.push_back(argv[index++]);
        }
        else if (result.command == "tournament-battle"
            && index + 1 < argc
            && argv[index][0] != '-'
            && argv[index + 1][0] != '-')
        {
            result.tournamentPaths.push_back(argv[index++]);
            result.tournamentPaths.push_back(argv[index++]);
        }
    }
    while (index < argc)
    {
        const std::string_view option = argv[index++];
        if (option == "--jsonl") result.jsonl = true;
        else if (option == "--compact") result.mode = KysChess::ChessCliOutputMode::Compact;
        else if (option == "--json") result.mode = KysChess::ChessCliOutputMode::Json;
        else if (option == "--trace") result.mode = KysChess::ChessCliOutputMode::Trace;
        else if (option == "--data-root" && index < argc) result.dataRoot = argv[index++];
        else if (option == "--config-root" && index < argc) result.configRoot = argv[index++];
        else if (option == "--output" && index < argc) result.outputPath = argv[index++];
        else if (option == "--json-output" && index < argc) result.jsonOutputPath = argv[index++];
        else if (option == "--difficulty" && index < argc)
        {
            const std::string_view value = argv[index++];
            result.difficulty = value == "easy"
                ? KysChess::Difficulty::Easy
                : value == "hard" ? KysChess::Difficulty::Hard : KysChess::Difficulty::Normal;
        }
        else if (option == "--seed" && index < argc)
        {
            if (const auto seed = parseSeed(argv[index++]))
            {
                result.seed = *seed;
            }
        }
        else if (option == "--battle-seeds" && index < argc)
        {
            result.battleSeedCount = parseNonnegativeInt(argv[index++]).value_or(0);
        }
        else if (option == "--seed-index" && index < argc)
        {
            result.seedSequenceIndex = parseNonnegativeInt(argv[index++]).value_or(-1);
        }
        else if (option == "--leg" && index < argc)
        {
            result.leg = parseNonnegativeInt(argv[index++]).value_or(-1);
        }
    }
    return result;
}

std::optional<std::string> readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), {});
}

bool writeText(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    return output.good();
}

std::vector<std::filesystem::path> tournamentSavePaths(
    const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::is_directory(directory))
    {
        return result;
    }
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        auto extension = entry.path().extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (extension == ".json")
        {
            result.push_back(entry.path());
        }
    }
    std::ranges::sort(result, {}, [](const auto& path) {
        return path.generic_u8string();
    });
    return result;
}

std::optional<KysChess::ChessTournamentSubmission> loadTournamentSubmission(
    const std::shared_ptr<const KysChess::ChessGameContent>& content,
    const std::filesystem::path& path,
    std::string& error)
{
    const auto payload = readText(path);
    if (!payload)
    {
        error = std::format("無法讀取參賽存檔 {}", KysChess::pathToUtf8(path));
        return std::nullopt;
    }
    const auto verification = KysChess::ChessPvpSaveVerifier::verify(
        content,
        *payload);
    if (!verification.valid)
    {
        error = std::format(
            "參賽存檔 {} 驗證失敗（序號 {}）：{}",
            KysChess::pathToUtf8(path),
            verification.sequence,
            verification.message);
        return std::nullopt;
    }

    KysChess::ChessTournamentSubmission result;
    result.name = KysChess::pathToUtf8(path.stem());
    result.source = KysChess::pathToUtf8(path.filename());
    result.payloadHash = KysChess::chessSha256(*payload);
    result.composition = verification.composition;
    return result;
}

std::string tournamentBattleOutcomeText(
    const KysChess::ChessTournamentBattleRecord& battle,
    const std::vector<KysChess::ChessTournamentSubmission>& submissions)
{
    using KysChess::ChessTournamentBattleOutcome;
    switch (battle.outcome)
    {
    case ChessTournamentBattleOutcome::TeamZeroVictory:
        return submissions[battle.teamZeroEntrantIndex].name + " 勝";
    case ChessTournamentBattleOutcome::TeamOneVictory:
        return submissions[battle.teamOneEntrantIndex].name + " 勝";
    case ChessTournamentBattleOutcome::Draw:
        return "和局";
    }
    std::unreachable();
}

std::string_view tournamentEntrantBattleOutcome(
    const KysChess::ChessTournamentBattleRecord& battle,
    int entrantIndex)
{
    using KysChess::ChessTournamentBattleOutcome;
    assert(entrantIndex == battle.teamZeroEntrantIndex
        || entrantIndex == battle.teamOneEntrantIndex);
    if (battle.outcome == ChessTournamentBattleOutcome::Draw)
    {
        return "D";
    }
    const int winnerIndex = battle.outcome == ChessTournamentBattleOutcome::TeamZeroVictory
        ? battle.teamZeroEntrantIndex
        : battle.teamOneEntrantIndex;
    return entrantIndex == winnerIndex ? "W" : "L";
}

std::string markdownTableText(std::string_view source)
{
    std::string result;
    result.reserve(source.size());
    for (const char value : source)
    {
        if (value == '|')
        {
            result += "\\|";
        }
        else if (value == '\r' || value == '\n')
        {
            result += ' ';
        }
        else
        {
            result += value;
        }
    }
    return result;
}

std::string tournamentMatrixCell(
    const KysChess::ChessTournamentResult& result,
    int rowEntrantIndex,
    int columnEntrantIndex)
{
    using namespace KysChess;
    assert(rowEntrantIndex != columnEntrantIndex);
    const auto pairing = std::ranges::find_if(
        result.roundRobin.pairings,
        [=](const auto& candidate) {
            return candidate.firstEntrantIndex == std::min(rowEntrantIndex, columnEntrantIndex)
                && candidate.secondEntrantIndex == std::max(rowEntrantIndex, columnEntrantIndex);
        });
    assert(pairing != result.roundRobin.pairings.end());
    const int pairingIndex = static_cast<int>(std::distance(
        result.roundRobin.pairings.begin(),
        pairing));

    std::string seedResults;
    for (int seedIndex = 0; seedIndex < static_cast<int>(result.roundRobin.battleSeeds.size()); ++seedIndex)
    {
        const ChessTournamentBattleRecord* rowAsTeamZero = nullptr;
        const ChessTournamentBattleRecord* rowAsTeamOne = nullptr;
        for (const auto& battle : result.roundRobin.battles)
        {
            if (battle.pairingIndex != pairingIndex
                || battle.seedSequenceIndex != seedIndex)
            {
                continue;
            }
            if (battle.teamZeroEntrantIndex == rowEntrantIndex)
            {
                rowAsTeamZero = &battle;
            }
            if (battle.teamOneEntrantIndex == rowEntrantIndex)
            {
                rowAsTeamOne = &battle;
            }
        }
        assert(rowAsTeamZero);
        assert(rowAsTeamOne);
        if (!seedResults.empty())
        {
            seedResults += ", ";
        }
        seedResults += std::format(
            "{}/{}",
            tournamentEntrantBattleOutcome(*rowAsTeamZero, rowEntrantIndex),
            tournamentEntrantBattleOutcome(*rowAsTeamOne, rowEntrantIndex));
    }

    const int rowHalfPoints = pairing->firstEntrantIndex == rowEntrantIndex
        ? pairing->firstHalfPoints
        : pairing->secondHalfPoints;
    const int columnHalfPoints = pairing->firstEntrantIndex == columnEntrantIndex
        ? pairing->firstHalfPoints
        : pairing->secondHalfPoints;
    return std::format(
        "**{}–{}** · [{}]",
        chessTournamentHalfPointsText(rowHalfPoints),
        chessTournamentHalfPointsText(columnHalfPoints),
        seedResults);
}

void printTournamentMarkdownResult(
    const KysChess::ChessTournamentResult& result,
    std::ostream& output)
{
    using namespace KysChess;
    output << "# 賽事結果\n\n";
    output << std::format("- 賽事種子：`0x{:016x}`\n", result.options.seed);
    output << "- 賽制：完整循環賽；最高總分相同者並列冠軍，不另行加賽。\n";
    output << "- 參賽順序：依 UTF-8 檔名升冪排列並指派索引。\n";
    output << "- 隊伍分配：場次 0 由配對第一位擔任隊伍 0；場次 1 交換雙方。\n\n";

    output << "## 參賽陣容\n\n";
    output << "| 索引 | 陣容 | 檔名 |\n";
    output << "|---:|---|---|\n";
    for (int index = 0; index < static_cast<int>(result.submissions.size()); ++index)
    {
        const auto& submission = result.submissions[index];
        output << std::format(
            "| {} | {} | {} |\n",
            index,
            markdownTableText(submission.name),
            markdownTableText(submission.source));
    }

    output << "\n## 共用戰鬥種子\n\n";
    output << "| 種子序號 | 戰鬥種子 |\n";
    output << "|---:|---:|\n";
    for (int index = 0; index < static_cast<int>(result.roundRobin.battleSeeds.size()); ++index)
    {
        output << std::format("| S{} | {} |\n", index, result.roundRobin.battleSeeds[index]);
    }

    output << "\n## 陣容對戰矩陣\n\n";
    output << "每格皆從左側列陣容的角度顯示，格式為 `總分 · [S0, S1, S2]`。"
           << "例如 `3–3 · [L/W, L/W, W/L]` 中，每個結果的第一個字母是該陣容擔任隊伍 0 時的結果，"
           << "第二個字母是擔任隊伍 1 時的結果；`W`、`L`、`D` 分別代表勝、敗、和。\n\n";
    output << "| 陣容 \\ 對手 |";
    for (const auto& submission : result.submissions)
    {
        output << ' ' << markdownTableText(submission.name) << " |";
    }
    output << "\n|---|";
    for (std::size_t index = 0; index < result.submissions.size(); ++index)
    {
        output << "---|";
    }
    output << '\n';
    for (int row = 0; row < static_cast<int>(result.submissions.size()); ++row)
    {
        output << "| " << markdownTableText(result.submissions[row].name) << " |";
        for (int column = 0; column < static_cast<int>(result.submissions.size()); ++column)
        {
            output << ' ';
            if (row == column)
            {
                output << "—";
            }
            else
            {
                output << tournamentMatrixCell(result, row, column);
            }
            output << " |";
        }
        output << '\n';
    }

    output << "\n## 逐場戰鬥紀錄\n\n";
    int battleIndex = 0;
    for (const auto& battle : result.roundRobin.battles)
    {
        output << std::format(
            "- `#{:03}` 配對 {} · S{}=`{}` · 場次 {} · 隊伍 0：{} · 隊伍 1：{} · **{}**\n",
            battleIndex + 1,
            battle.pairingIndex,
            battle.seedSequenceIndex,
            battle.battleSeed,
            battle.leg,
            markdownTableText(result.submissions[battle.teamZeroEntrantIndex].name),
            markdownTableText(result.submissions[battle.teamOneEntrantIndex].name),
            tournamentBattleOutcomeText(battle, result.submissions));
        ++battleIndex;
    }

    output << "\n## 最終排名\n\n";
    output << "| 名次 | 索引 | 陣容 | 總分 | 勝 | 和 | 敗 |\n";
    output << "|---:|---:|---|---:|---:|---:|---:|\n";
    for (const auto& standing : result.standings)
    {
        output << std::format(
            "| {} | {} | {} | {} | {} | {} | {} |\n",
            standing.rank,
            standing.entrantIndex,
            markdownTableText(result.submissions[standing.entrantIndex].name),
            chessTournamentHalfPointsText(standing.halfPoints),
            standing.battleWins,
            standing.battleDraws,
            standing.battleLosses);
    }
    output << "\n## "
           << (result.championEntrantIndices.size() > 1 ? "並列冠軍" : "冠軍")
           << "\n\n";
    for (const int entrantIndex : result.championEntrantIndices)
    {
        output << "- " << result.submissions[entrantIndex].name << '\n';
    }
    output << "\n## 單場重現\n\n";
    output << "請依配對順序傳入 `<第一存檔> <第二存檔>`，再指定相同種子序號與場次。\n\n";
    output << "```powershell\n";
    output << "kys_chess_cli tournament-battle <第一存檔> <第二存檔> --seed <賽事種子> --seed-index <序號> --leg <0|1>\n";
    output << "```\n";
}

}

int main(int argc, char** argv)
{
#ifdef _WIN32
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
#endif
    using namespace KysChess;
    const auto arguments = parseArguments(argc, argv);
    std::map<Difficulty, std::shared_ptr<const ChessGameContent>> cache;
    const auto provider = [&](Difficulty difficulty) -> std::shared_ptr<const ChessGameContent> {
        if (const auto found = cache.find(difficulty); found != cache.end())
        {
            return found->second;
        }
        ChessContentLoadOptions options;
        options.dataRoot = arguments.dataRoot;
        options.configRoot = arguments.configRoot;
        options.difficulty = difficulty;
        options.diagnostics = [](const ChessDiagnostic& diagnostic) {
            std::cerr << "[" << diagnostic.source << "] " << diagnostic.message << '\n';
        };
        auto loaded = ChessContentLoader::load(options);
        if (!loaded)
        {
            return {};
        }
        auto content = std::make_shared<const ChessGameContent>(std::move(*loaded));
        cache.emplace(difficulty, content);
        return content;
    };

    if (arguments.command == "verify")
    {
        ChessReplayArchiveError archiveError;
        auto jsonl = arguments.replayPath.extension() == ".kysreplay"
            ? ChessReplayArchive::readReplayJsonl(arguments.replayPath, archiveError)
            : readText(arguments.replayPath);
        if (!jsonl)
        {
            std::cerr << "無法讀取重播檔案\n";
            return 2;
        }
        ChessReplayJsonError parseError;
        const auto replay = parseChessReplayJsonl(*jsonl, parseError);
        if (!replay)
        {
            std::cerr << parseError.message << '\n';
            return 2;
        }
        const auto difficulty = replay->header.difficulty == "easy"
            ? Difficulty::Easy
            : replay->header.difficulty == "hard" ? Difficulty::Hard : Difficulty::Normal;
        const auto content = provider(difficulty);
        if (!content)
        {
            return 2;
        }
        const auto verification = ChessReplayVerifier::verify(content, *replay);
        if (!verification.valid)
        {
            std::cerr << "序號 " << verification.sequence << "：" << verification.message << '\n';
            return 1;
        }
        std::cout << "重播驗證成功\n";
        return 0;
    }

    if (arguments.command == "verify-pvp")
    {
        const auto payload = readText(arguments.replayPath);
        if (!payload)
        {
            std::cerr << "無法讀取離線對戰存檔\n";
            return 2;
        }
        const auto content = provider(Difficulty::Hard);
        if (!content)
        {
            return 2;
        }
        ChessPvpSaveVerifier verifier(content, *payload);
        while (!verifier.finished())
        {
            verifier.step(1, std::numeric_limits<int>::max());
        }
        const auto verification = verifier.takeResult();
        if (!verification.valid)
        {
            std::cerr << "序號 " << verification.sequence << "：" << verification.message << '\n';
            return 1;
        }
        std::cout << "離線對戰存檔驗證成功\n";
        return 0;
    }

    if (arguments.command == "tournament"
        || arguments.command == "tournament-battle")
    {
        const auto content = provider(Difficulty::Hard);
        if (!content)
        {
            return 2;
        }
        if (arguments.command == "tournament-battle")
        {
            if (arguments.tournamentPaths.size() != 2)
            {
                std::cerr << "用法：kys_chess_cli tournament-battle <存檔甲> <存檔乙> --seed N --seed-index N --leg 0|1\n";
                return 2;
            }
            std::string error;
            auto first = loadTournamentSubmission(
                content,
                arguments.tournamentPaths[0],
                error);
            if (!first)
            {
                std::cerr << error << '\n';
                return 2;
            }
            auto second = loadTournamentSubmission(
                content,
                arguments.tournamentPaths[1],
                error);
            if (!second)
            {
                std::cerr << error << '\n';
                return 2;
            }
            const auto battle = ChessTournamentRunner::runBattle(
                content,
                *first,
                *second,
                arguments.seed,
                arguments.seedSequenceIndex,
                arguments.leg,
                error);
            if (!battle)
            {
                std::cerr << error << '\n';
                return 1;
            }
            if (arguments.mode == ChessCliOutputMode::Json)
            {
                std::string_view outcome;
                switch (battle->outcome)
                {
                case ChessTournamentBattleOutcome::TeamZeroVictory:
                    outcome = battle->teamZeroEntrantIndex == 0 ? "first_victory" : "second_victory";
                    break;
                case ChessTournamentBattleOutcome::TeamOneVictory:
                    outcome = battle->teamOneEntrantIndex == 0 ? "first_victory" : "second_victory";
                    break;
                case ChessTournamentBattleOutcome::Draw:
                    outcome = "draw";
                    break;
                }
                std::cout << std::format(
                    "{{\"tournament_seed\":\"0x{:016x}\",\"seed_sequence_index\":{},\"battle_seed\":{},\"leg\":{},\"outcome\":\"{}\",\"end_frame\":{},\"digest_sha256\":\"{}\"}}\n",
                    arguments.seed,
                    battle->seedSequenceIndex,
                    battle->battleSeed,
                    battle->leg,
                    outcome,
                    battle->endFrame,
                    chessSha256Hex(battle->digest));
            }
            else
            {
                const std::vector submissions{*first, *second};
                std::cout << std::format(
                    "種子序號 [{}]，戰鬥種子 {}，場次 {}：{}（隊伍 0） vs {}（隊伍 1） -> {}，{} 幀，{}\n",
                    battle->seedSequenceIndex,
                    battle->battleSeed,
                    battle->leg,
                    submissions[battle->teamZeroEntrantIndex].name,
                    submissions[battle->teamOneEntrantIndex].name,
                    tournamentBattleOutcomeText(*battle, submissions),
                    battle->endFrame,
                    chessSha256Hex(battle->digest));
            }
            return 0;
        }

        if (arguments.tournamentPaths.size() != 1)
        {
            std::cerr << "用法：kys_chess_cli tournament <存檔目錄> --seed N [--battle-seeds N] [--output 報告.md] [--json-output 結果.json] [--json]\n";
            return 2;
        }
        auto savePaths = tournamentSavePaths(arguments.tournamentPaths.front());
        const auto saveDirectory = std::filesystem::absolute(
            arguments.tournamentPaths.front()).lexically_normal();
        const std::array outputPaths{
            arguments.outputPath,
            arguments.jsonOutputPath,
        };
        if (!arguments.outputPath.empty()
            && !arguments.jsonOutputPath.empty()
            && std::filesystem::absolute(arguments.outputPath).lexically_normal()
                == std::filesystem::absolute(arguments.jsonOutputPath).lexically_normal())
        {
            std::cerr << "Markdown 報告與 JSON 結果必須使用不同的輸出路徑\n";
            return 2;
        }
        for (const auto& outputPath : outputPaths)
        {
            if (outputPath.empty())
            {
                continue;
            }
            const auto outputDirectory = std::filesystem::absolute(
                outputPath).lexically_normal().parent_path();
            if (saveDirectory == outputDirectory)
            {
                std::cerr << "賽事結果不可寫入參賽存檔目錄，避免下次執行誤當成參賽存檔\n";
                return 2;
            }
        }
        if (savePaths.size() < 2)
        {
            std::cerr << "參賽目錄至少需要兩個 .json 存檔\n";
            return 2;
        }
        std::vector<ChessTournamentSubmission> submissions;
        submissions.reserve(savePaths.size());
        for (const auto& path : savePaths)
        {
            std::string error;
            auto submission = loadTournamentSubmission(content, path, error);
            if (!submission)
            {
                std::cerr << error << '\n';
                return 2;
            }
            std::cerr << "已驗證：" << submission->name << '\n';
            submissions.push_back(std::move(*submission));
        }

        ChessTournamentOptions options;
        options.seed = arguments.seed;
        options.battleSeedCount = arguments.battleSeedCount;
        std::string error;
        const auto result = ChessTournamentRunner::run(
            content,
            std::move(submissions),
            options,
            error,
            [](std::size_t completed, const ChessTournamentBattleRecord&) {
                std::cerr << '\r' << "已完成 " << completed << " 場戰鬥" << std::flush;
            });
        std::cerr << '\n';
        if (!result)
        {
            std::cerr << error << '\n';
            return 1;
        }
        std::ostringstream reportOutput;
        printTournamentMarkdownResult(*result, reportOutput);
        const auto report = std::move(reportOutput).str();
        const auto json = serializeChessTournamentResultJson(*result);
        if (json.empty())
        {
            std::cerr << "無法序列化賽事結果\n";
            return 1;
        }
        if (!arguments.outputPath.empty()
            && !writeText(arguments.outputPath, report))
        {
            std::cerr << "無法寫出 Markdown 賽事報告 " << pathToUtf8(arguments.outputPath) << '\n';
            return 2;
        }
        if (!arguments.jsonOutputPath.empty()
            && !writeText(arguments.jsonOutputPath, json))
        {
            std::cerr << "無法寫出 JSON 賽事結果 " << pathToUtf8(arguments.jsonOutputPath) << '\n';
            return 2;
        }
        if (arguments.mode == ChessCliOutputMode::Json)
        {
            std::cout << json << '\n';
        }
        else
        {
            std::cout << report;
            if (!arguments.outputPath.empty())
            {
                std::cout << "Markdown 報告已寫入：" << pathToUtf8(arguments.outputPath) << '\n';
            }
            if (!arguments.jsonOutputPath.empty())
            {
                std::cout << "JSON 結果已寫入：" << pathToUtf8(arguments.jsonOutputPath) << '\n';
            }
        }
        return 0;
    }

    ChessCliController controller(provider);
    if (arguments.jsonl)
    {
        return controller.runJsonl(std::cin, std::cout);
    }
    std::cout << controller.newSession(arguments.difficulty, arguments.seed, arguments.mode);
    if (arguments.command == "new")
    {
        return 0;
    }
    return controller.runInteractive(std::cin, std::cout, arguments.mode);
}
