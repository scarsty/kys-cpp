#include "ChessCliController.h"
#include "ChessContentLoader.h"
#include "ChessReplayArchive.h"
#include "ChessReplayJson.h"
#include "ChessReplayVerifier.h"

#include <Windows.h>

#include <array>
#include <filesystem>
#include <charconv>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>

namespace
{

std::filesystem::path executablePath()
{
    std::wstring buffer(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path(buffer);
}

struct Arguments
{
    std::string command = "play";
    std::filesystem::path replayPath;
    std::filesystem::path dataRoot;
    std::filesystem::path configRoot;
    KysChess::Difficulty difficulty = KysChess::Difficulty::Normal;
    std::uint64_t seed = 1;
    KysChess::ChessCliOutputMode mode = KysChess::ChessCliOutputMode::Human;
    bool jsonl = false;
};

struct DefaultContentRoots
{
    std::filesystem::path dataRoot;
    std::filesystem::path configRoot;
};

bool isDataRoot(const std::filesystem::path& path)
{
    return std::filesystem::is_regular_file(path / "save" / "game.db")
        && std::filesystem::is_regular_file(path / "cc" / "STPhrases.txt");
}

DefaultContentRoots defaultContentRoots(const std::filesystem::path& executable)
{
    auto directory = std::filesystem::weakly_canonical(executable).parent_path();
    for (auto ancestor = directory; !ancestor.empty(); ancestor = ancestor.parent_path())
    {
        const std::array candidates{
            ancestor,
            ancestor / "game-dev",
            ancestor / "work" / "game-dev",
        };
        for (const auto& dataRoot : candidates)
        {
            if (!isDataRoot(dataRoot))
            {
                continue;
            }
            const auto repositoryConfig = ancestor / "config";
            const auto packagedConfig = dataRoot / "config";
            return {
                std::filesystem::weakly_canonical(dataRoot),
                std::filesystem::is_regular_file(repositoryConfig / "chess_challenge.yaml")
                    ? std::filesystem::weakly_canonical(repositoryConfig)
                    : std::filesystem::weakly_canonical(packagedConfig),
            };
        }
        if (ancestor == ancestor.root_path())
        {
            break;
        }
    }
    return {
        directory / "game-dev",
        directory / "game-dev" / "config",
    };
}

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

Arguments parseArguments(int argc, char** argv)
{
    Arguments result;
    const auto defaults = defaultContentRoots(executablePath());
    result.dataRoot = defaults.dataRoot;
    result.configRoot = defaults.configRoot;

    int index = 1;
    if (index < argc && argv[index][0] != '-')
    {
        result.command = argv[index++];
        if (result.command == "verify" && index < argc)
        {
            result.replayPath = argv[index++];
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

}

int main(int argc, char** argv)
{
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
