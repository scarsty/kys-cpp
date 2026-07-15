#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

namespace
{

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
}

std::vector<std::filesystem::path> projectSources(const std::filesystem::path& project)
{
    const auto xml = readText(project);
    const std::regex pattern(R"re(<ClCompile Include="([^"]+\.cpp)")re");
    std::vector<std::filesystem::path> result;
    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), pattern);
         it != std::sregex_iterator();
         ++it)
    {
        auto relative = std::filesystem::path((*it)[1].str());
        if (relative.string().contains('*'))
        {
            for (const auto& entry : std::filesystem::directory_iterator(project.parent_path() / relative.parent_path()))
            {
                if (entry.path().extension() == ".cpp")
                {
                    result.push_back(std::filesystem::weakly_canonical(entry.path()));
                }
            }
        }
        else
        {
            result.push_back(std::filesystem::weakly_canonical(project.parent_path() / relative));
        }
    }
    return result;
}

std::vector<std::filesystem::path> projectFiles(const std::filesystem::path& project)
{
    auto result = projectSources(project);
    const auto sources = result;
    for (auto source : sources)
    {
        source.replace_extension(".h");
        if (std::filesystem::exists(source)) result.push_back(std::move(source));
    }
    if (project.filename() == "kys_battle_core.vcxproj")
    {
        for (const auto& entry : std::filesystem::directory_iterator(project.parent_path() / "battle"))
        {
            if (entry.path().extension() == ".h") result.push_back(entry.path());
        }
    }
    else if (project.filename() == "kys_chess_core.vcxproj")
    {
        for (const auto* name : {"PreparedChessBattle.h", "ChessReplayTypes.h", "ChessSessionTypes.h"})
        {
            result.push_back(project.parent_path() / name);
        }
    }
    return result;
}

bool hasForbiddenActiveInclude(const std::filesystem::path& source)
{
    static const std::vector<std::string> forbidden{
        "Engine", "Audio", "Font", "RunNode", "SystemSettings", "TextureManager", "BattleScene",
        "imgui", "SDL_mixer", "SDL3_mixer",
    };
    std::istringstream lines(readText(source));
    std::string line;
    int headlessExcludedDepth = 0;
    while (std::getline(lines, line))
    {
        if (line.contains("#ifndef KYS_HEADLESS_CORE"))
        {
            ++headlessExcludedDepth;
            continue;
        }
        if (headlessExcludedDepth > 0 && line.starts_with("#if"))
        {
            ++headlessExcludedDepth;
            continue;
        }
        if (headlessExcludedDepth > 0 && line.starts_with("#endif"))
        {
            --headlessExcludedDepth;
            continue;
        }
        if (headlessExcludedDepth > 0 || !line.contains("#include"))
        {
            continue;
        }
        for (const auto& name : forbidden)
        {
            if (line.contains(name)) return true;
        }
    }
    return false;
}

bool containsAny(
    const std::filesystem::path& source,
    const std::vector<std::string>& forbidden)
{
    const auto text = readText(source);
    return std::ranges::any_of(forbidden, [&](const std::string& token) {
        return text.contains(token);
    });
}

std::size_t countOccurrences(std::string_view text, std::string_view token)
{
    std::size_t count{};
    for (std::size_t position = 0;
         (position = text.find(token, position)) != std::string_view::npos;
         position += token.size())
    {
        ++count;
    }
    return count;
}

std::set<std::string> gameplayRuleCalls(const std::filesystem::path& source)
{
    const auto text = readText(source);
    const std::regex pattern(
        R"((Chess(?:ManagementRules|BattlePlanner|ProgressionRules|RewardRules)::[A-Za-z0-9_]+))");
    std::set<std::string> result;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator();
         ++it)
    {
        result.insert((*it)[1].str());
    }
    return result;
}

}

TEST_CASE("headless core projects have strict FP and no active presentation dependencies", "[chess][dependency]")
{
    const auto root = std::filesystem::current_path();
    const auto battleProject = root / "src" / "kys_battle_core.vcxproj";
    const auto chessProject = root / "src" / "kys_chess_core.vcxproj";
    CHECK(readText(battleProject).contains("<FloatingPointModel>Strict</FloatingPointModel>"));
    CHECK(readText(chessProject).contains("<FloatingPointModel>Strict</FloatingPointModel>"));
    for (const auto& project : {battleProject, chessProject})
    {
        for (const auto& source : projectFiles(project))
        {
            CAPTURE(source.string());
            CHECK_FALSE(hasForbiddenActiveInclude(source));
        }
    }
}

TEST_CASE("headless core has no global game state path or nondeterministic entropy", "[chess][dependency][source-scan]")
{
    const auto root = std::filesystem::current_path();
    const std::vector<std::string> forbidden{
        "GameState::get(",
        "GameUtil::PATH(",
        "std::random_device",
    };
    for (const auto& project : {
             root / "src" / "kys_battle_core.vcxproj",
             root / "src" / "kys_chess_core.vcxproj",
         })
    {
        for (const auto& source : projectFiles(project))
        {
            CAPTURE(source.string());
            CHECK_FALSE(containsAny(source, forbidden));
        }
    }
}

TEST_CASE("graphical adapters do not call gameplay mutation owners", "[chess][dependency][gui][source-scan]")
{
    const auto root = std::filesystem::current_path() / "src";
    const std::set<std::string> allowedQueries{
        "ChessManagementRules::experienceForNextLevel",
        "ChessManagementRules::maximumBanCount",
        "ChessManagementRules::pieceValue",
    };
    for (const auto* name : {
             "BattleSceneHades.cpp",
             "ChessApplicationSessionHost.cpp",
             "ChessGuiSessionAdapter.cpp",
             "ChessModHook.cpp",
             "Console.cpp",
             "Event.cpp",
             "MainScene.cpp",
             "SubScene.cpp",
         })
    {
        const auto source = root / name;
        CAPTURE(source.string());
        CHECK_FALSE(containsAny(
            source,
            std::vector<std::string>{
                "const_cast<ChessGameSession",
                "const_cast<ChessSessionState",
            }));
        for (const auto& call : gameplayRuleCalls(source))
        {
            CAPTURE(call);
            CHECK(allowedQueries.contains(call));
        }
    }
}

TEST_CASE("shared chess read models remain frontend neutral",
          "[chess][dependency][read-model][source-scan]")
{
    const auto root = std::filesystem::current_path() / "src";
    const std::vector<std::string> forbidden{
        "ChessJson",
        "ChessGui",
        "<glaze/",
        "\"Engine.h\"",
        "\"Audio.h\"",
        "\"Font.h\"",
        "\"RunNode.h\"",
    };
    for (const auto* name : {
             "ChessActionOffers.h",
             "ChessActionOffers.cpp",
             "ChessCatalogQueries.h",
             "ChessCatalogQueries.cpp",
             "ChessGameQueries.h",
             "ChessGameQueries.cpp",
             "ChessPreparedBattleAnalysis.h",
             "ChessPreparedBattleAnalysis.cpp",
             "ChessBattleAnalysis.h",
             "ChessBattleAnalysis.cpp",
         })
    {
        const auto source = root / name;
        CAPTURE(source.string());
        CHECK_FALSE(containsAny(source, forbidden));
    }
}

TEST_CASE("JSON protocol is routing-only and raw state access stays infrastructure-scoped",
          "[chess][dependency][json][source-scan]")
{
    const auto root = std::filesystem::current_path() / "src";
    const auto protocol = readText(root / "ChessJsonProtocol.cpp");
    CHECK(protocol.contains("#include \"ChessJsonCodec.h\""));
    CHECK_FALSE(containsAny(
        root / "ChessJsonProtocol.cpp",
        {
            "ChessCatalogQueries",
            "ChessGameQueries",
            "ChessPreparedBattleAnalysis",
            "ChessBattleAnalysis",
            "queryChess",
            "analyzeChess",
            "BattleReportEvent",
            "UnitCombatAggregate",
            "struct RoleStatsDto",
            "struct ObservationDto",
        }));
    CHECK(countOccurrences(protocol, "state()") == 1);
    CHECK(protocol.contains("const auto before = session_->state();"));
    CHECK(countOccurrences(protocol, "content()") == 1);
    CHECK(protocol.contains("session_->content().gameVersion()"));

    const auto codec = readText(root / "ChessJsonCodec.cpp");
    CHECK(codec.contains("queryChessShop("));
    CHECK(codec.contains("queryChessInstance("));
    CHECK(codec.contains("queryChessBans("));
    CHECK(codec.contains("analyzePreparedChessBattle("));
    CHECK(codec.contains("analyzeChessBattleResult("));
    CHECK_FALSE(codec.contains("UnitCombatAggregate"));
    CHECK_FALSE(codec.contains("battleEventText"));

    const auto dtos = readText(root / "ChessJsonDtos.h");
    CHECK_FALSE(dtos.contains("ChessSessionState"));
    CHECK_FALSE(dtos.contains("ChessGameContent"));
    CHECK_FALSE(dtos.contains("Engine"));
    CHECK_FALSE(dtos.contains("Audio"));
}

TEST_CASE("retired auto-chess rule paths remain deleted", "[chess][dependency][legacy][source-scan]")
{
    const auto root = std::filesystem::current_path();
    for (const auto* name : {
             "BattleRoleManager.cpp",
             "BattleSceneSetupBuilder.cpp",
             "Chess.cpp",
             "ChessBattleFlow.cpp",
             "ChessChallengeFlow.cpp",
             "ChessEconomy.cpp",
             "ChessManager.cpp",
             "ChessProgress.cpp",
             "ChessRandom.cpp",
             "ChessRewardFlow.cpp",
             "ChessRoster.cpp",
             "ChessShop.cpp",
             "ChessShopFlow.cpp",
             "DynamicChessMap.cpp",
             "GameState.cpp",
         })
    {
        CAPTURE(name);
        CHECK_FALSE(std::filesystem::exists(root / "src" / name));
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "src"))
    {
        if (entry.path().extension() != ".cpp" && entry.path().extension() != ".h")
        {
            continue;
        }
        CAPTURE(entry.path().string());
        CHECK_FALSE(readText(entry.path()).contains("GameState::get("));
    }
}

TEST_CASE("tests consume core libraries instead of compiling duplicate rule sources", "[chess][dependency]")
{
    const auto root = std::filesystem::current_path();
    const auto testProject = root / "tests" / "kys_tests.vcxproj";
    const auto project = readText(testProject);
    CHECK(project.contains("kys_battle_core.vcxproj"));
    CHECK(project.contains("kys_chess_core.vcxproj"));
    const auto testSources = projectSources(testProject);
    const std::set<std::filesystem::path> compiledByTests(
        testSources.begin(),
        testSources.end());
    for (const auto& coreProject : {
             root / "src" / "kys_battle_core.vcxproj",
             root / "src" / "kys_chess_core.vcxproj",
         })
    {
        for (const auto& source : projectSources(coreProject))
        {
            CAPTURE(source.string());
            CHECK_FALSE(compiledByTests.contains(source));
        }
    }
}

TEST_CASE("GUI consumes core libraries instead of compiling duplicate rule sources", "[chess][dependency][gui]")
{
    const auto root = std::filesystem::current_path();
    const auto guiProject = root / "src" / "kys.vcxproj";
    const auto project = readText(guiProject);
    CHECK(project.contains("kys_battle_core.vcxproj"));
    CHECK(project.contains("kys_chess_core.vcxproj"));
    const auto guiSources = projectSources(guiProject);
    const std::set<std::filesystem::path> compiledByGui(
        guiSources.begin(),
        guiSources.end());
    for (const auto& coreProject : {
             root / "src" / "kys_battle_core.vcxproj",
             root / "src" / "kys_chess_core.vcxproj",
         })
    {
        for (const auto& source : projectSources(coreProject))
        {
            CAPTURE(source.string());
            CHECK_FALSE(compiledByGui.contains(source));
        }
    }
}

TEST_CASE("CLI project links only headless project boundaries", "[chess][dependency][cli]")
{
    const auto project = readText(std::filesystem::current_path() / "src" / "kys_chess_cli.vcxproj");
    CHECK(project.contains("kys_battle_core.vcxproj"));
    CHECK(project.contains("kys_chess_core.vcxproj"));
    CHECK_FALSE(project.contains("SDL"));
    CHECK_FALSE(project.contains("imgui"));
    CHECK_FALSE(project.contains("Engine"));
    CHECK_FALSE(project.contains("Audio"));
}

TEST_CASE("solution and default build wire every headless target", "[chess][dependency][build]")
{
    const auto root = std::filesystem::current_path();
    const auto solution = readText(root / "kys.sln");
    const auto buildScript = readText(root / ".github" / "build-command.ps1");
    const auto presets = readText(root / "CMakePresets.json");

    for (const auto* project : {
             "kys_battle_core",
             "kys_chess_core",
             "kys_chess_cli",
         })
    {
        CAPTURE(project);
        CHECK(solution.contains(project));
    }
    CHECK(buildScript.contains("'kys_chess_cli'"));
    CHECK(presets.contains("windows-msvc-x64-headless-debug"));
    CHECK(presets.contains("\"KYS_BUILD_GAME\": \"OFF\""));
    CHECK(presets.contains("\"BUILD_TESTING\": \"OFF\""));
}
