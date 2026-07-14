#include "ChessDiagnostics.h"
#include "ChessContentLoader.h"
#include "ChessBattleMapCatalog.h"
#include "ChessGameContent.h"
#include "ChessNeigong.h"
#include "GameVersion.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <format>
#include <string>
#include <string_view>
#include <vector>

using namespace KysChess;

TEST_CASE("difficulty display keeps the selected menu labels", "[chess][content][presentation]")
{
    CHECK(std::string_view(ChessBalance::difficultyDisplayNameTraditional(Difficulty::Easy)) == "簡單");
    CHECK(std::string_view(ChessBalance::difficultyDisplayNameTraditional(Difficulty::Normal)) == "標準");
    CHECK(std::string_view(ChessBalance::difficultyDisplayNameTraditional(Difficulty::Hard)) == "困難");
}

namespace
{

ChessGameContentData syntheticContentData(Difficulty difficulty)
{
    ChessGameContentData data;
    data.difficulty = difficulty;
    data.balance.initialMoney = difficulty == Difficulty::Easy ? 20 : 12;

    ChessRoleDefinition first;
    first.ID = 20;
    first.Name = "角色乙";
    first.Cost = 2;
    first.MaxHP = 500;
    first.Attack = 40;
    data.roles.emplace(first.ID, first);

    ChessRoleDefinition second;
    second.ID = 10;
    second.Name = "角色甲";
    second.Cost = 1;
    second.MaxHP = 400;
    second.Attack = 30;
    data.roles.emplace(second.ID, second);

    ChessMagicDefinition magic;
    magic.ID = 5;
    magic.Name = "測試武功";
    magic.MagicType = 1;
    magic.AttackAreaType = 3;
    data.magics.emplace(magic.ID, magic);

    data.items.emplace(9, ChessItemDefinition{9, -1, 0, 1, 0, 3, 0, 0, 0, 0, 0, 0, 0, "測試裝備"});

    ComboDef secondCombo;
    secondCombo.id = 2;
    secondCombo.name = "羈絆乙";
    secondCombo.memberRoleIds = {20};
    secondCombo.thresholds.push_back({1, "啟動", {}});
    data.combos.push_back(secondCombo);

    ComboDef firstCombo;
    firstCombo.id = 1;
    firstCombo.name = "羈絆甲";
    firstCombo.memberRoleIds = {10};
    firstCombo.thresholds.push_back({1, "啟動", {}});
    data.combos.push_back(firstCombo);

    EquipmentDef laterEquipment;
    laterEquipment.itemId = 9;
    laterEquipment.tier = 1;
    laterEquipment.equipType = 0;
    data.equipment.push_back(laterEquipment);

    EquipmentDef earlierEquipment;
    earlierEquipment.itemId = 3;
    earlierEquipment.tier = 1;
    earlierEquipment.equipType = 1;
    data.equipment.push_back(earlierEquipment);
    return data;
}

class TemporaryConfigDirectory
{
public:
    TemporaryConfigDirectory()
        : path_(std::filesystem::temp_directory_path()
            / std::format(
                "kys-chess-config-{}",
                std::chrono::steady_clock::now().time_since_epoch().count()))
    {
        std::filesystem::create_directories(path_);
    }

    ~TemporaryConfigDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path write(std::string_view name, std::string_view content) const
    {
        const auto path = path_ / name;
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << content;
        return path;
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}

TEST_CASE("immutable content keeps independent difficulty snapshots", "[chess][content]")
{
    const ChessGameContent easy(syntheticContentData(Difficulty::Easy));
    const ChessGameContent hard(syntheticContentData(Difficulty::Hard));

    CHECK(easy.difficulty() == Difficulty::Easy);
    CHECK(hard.difficulty() == Difficulty::Hard);
    CHECK(easy.balance().initialMoney == 20);
    CHECK(hard.balance().initialMoney == 12);
    CHECK(easy.role(10)->Name == "角色甲");
    CHECK(hard.role(20)->Name == "角色乙");
    CHECK(easy.gameVersion() == "dev");
    CHECK(hard.gameVersion() == "dev");
}

TEST_CASE("immutable content carries the exact game version", "[chess][content][version]")
{
    const ChessGameContent content(syntheticContentData(Difficulty::Normal), "1.2.3");
    CHECK(content.gameVersion() == "1.2.3");
}

TEST_CASE("game version loader reads release configuration", "[chess][content][version]")
{
    TemporaryConfigDirectory files;
    files.write(
        "config/release.ini",
        "[其他]\n"
        "version=ignored\n"
        "[release]\n"
        "version = 1.2.3 \n");

    CHECK(loadGameVersion(files.path()) == "1.2.3");
}

TEST_CASE("game version loader uses development version when release configuration is absent", "[chess][content][version]")
{
    TemporaryConfigDirectory files;
    CHECK(loadGameVersion(files.path()) == "dev");
}

TEST_CASE("battle map catalog exposes usable formation capacities", "[chess][content][map]")
{
    REQUIRE_FALSE(ChessBattleMapCatalog::entries().empty());
    CHECK(ChessBattleMapCatalog::entries().front().enemyCapacity > 0);
    CHECK_FALSE(ChessBattleMapCatalog::entries().front().teammatePositions.empty());
    CHECK_FALSE(ChessBattleMapCatalog::entries().front().allyClonePositions.empty());
}

TEST_CASE("challenge configuration rejects duplicate challenge names", "[chess][content][config]")
{
    TemporaryConfigDirectory files;
    const auto balance = files.write("balance.yaml", "{}\n");
    const auto challenges = files.write(
        "challenge.yaml",
        "遠征挑戰:\n"
        "  - 名稱: 重複名稱\n"
        "    敵人: []\n"
        "  - 名稱: 重複名稱\n"
        "    敵人: []\n");
    ChessDiagnosticCollector diagnostics;
    BalanceConfig result;

    CHECK_FALSE(loadBalanceConfig(
        balance.generic_string(),
        challenges.generic_string(),
        [](std::string_view text) { return std::string(text); },
        diagnostics.sink(),
        result));
    CHECK(diagnostics.hasErrors());
}

TEST_CASE("challenge configuration rejects duplicate reward meanings in one choice", "[chess][content][config]")
{
    TemporaryConfigDirectory files;
    const auto balance = files.write("balance.yaml", "{}\n");
    const auto challenges = files.write(
        "challenge.yaml",
        "遠征挑戰:\n"
        "  - 名稱: 獎勵測試\n"
        "    敵人: []\n"
        "    獎勵:\n"
        "      - 類型: 獲取金幣\n"
        "        數值: 1\n"
        "      - 類型: 獲取金幣\n"
        "        數值: 1\n");
    ChessDiagnosticCollector diagnostics;
    BalanceConfig result;

    CHECK_FALSE(loadBalanceConfig(
        balance.generic_string(),
        challenges.generic_string(),
        [](std::string_view text) { return std::string(text); },
        diagnostics.sink(),
        result));
    CHECK(diagnostics.hasErrors());
}

TEST_CASE("challenge configuration reads Traditional Chinese star and equipment fields",
          "[chess][content][challenge]")
{
    TemporaryConfigDirectory files;
    const auto balance = files.write("balance.yaml", "{}\n");
    const auto challenges = files.write(
        "challenge.yaml",
        "遠征挑戰:\n"
        "  - 名稱: 權威資料\n"
        "    敵人:\n"
        "      - 角色ID: 10\n"
        "        星級: 3\n"
        "        武器: 100\n"
        "        防具: 200\n"
        "    獎勵: []\n");
    ChessDiagnosticCollector diagnostics;
    BalanceConfig result;

    REQUIRE(loadBalanceConfig(
        balance.generic_string(),
        challenges.generic_string(),
        [](std::string_view text) { return std::string(text); },
        diagnostics.sink(),
        result));

    REQUIRE(result.challenges.size() == 1);
    REQUIRE(result.challenges.front().enemies.size() == 1);
    CHECK(result.challenges.front().enemies.front().star == 3);
    CHECK(result.challenges.front().enemies.front().weaponId == 100);
    CHECK(result.challenges.front().enemies.front().armorId == 200);
}

TEST_CASE("pool configuration rejects duplicate role identifiers", "[chess][content][config]")
{
    TemporaryConfigDirectory files;
    const auto pool = files.write("pool.yaml", "角色: [10, 20, 10]\n");
    ChessDiagnosticCollector diagnostics;
    std::vector<int> roleIds;

    CHECK_FALSE(loadChessPoolRoleIds(pool, roleIds, diagnostics.sink()));
    CHECK(diagnostics.hasErrors());
}

TEST_CASE("content lookups use stable numeric identifiers", "[chess][content]")
{
    const ChessGameContent content(syntheticContentData(Difficulty::Normal));

    REQUIRE(content.role(10));
    CHECK(content.role(10)->Cost == 1);
    CHECK(content.role(999) == nullptr);
    REQUIRE(content.magic(5));
    CHECK(content.magic(5)->Name == "測試武功");
    REQUIRE(content.item(9));
    CHECK(content.item(9)->name == "測試裝備");
}

TEST_CASE("internal skill configuration can override legacy names with Traditional Chinese",
          "[chess][content][neigong]")
{
    TemporaryConfigDirectory files;
    const auto config = files.write(
        "chess_neigong.yaml",
        "选择数量: 1\n"
        "层级分配:\n"
        "  - 层级: 1\n"
        "    武功: [93]\n"
        "名稱:\n"
        "  93: 聖火神功\n"
        "效果:\n"
        "  93: []\n");
    Item item;
    item.ID = 1;
    item.ItemType = 2;
    item.MagicID = 93;
    Magic magic;
    magic.ID = 93;
    magic.Name = "舊版名稱";
    std::vector<Item*> items{&item};
    ChessDiagnosticCollector diagnostics;
    NeigongConfig resultConfig;
    std::vector<NeigongDef> pool;

    REQUIRE(loadChessNeigong(
        config.generic_string(),
        items,
        [&](int magicId) -> const Magic* { return magicId == magic.ID ? &magic : nullptr; },
        diagnostics.sink(),
        resultConfig,
        pool));

    REQUIRE(pool.size() == 1);
    CHECK(pool.front().name == "聖火神功");
}

TEST_CASE("diagnostics are collected without writing protocol output", "[chess][content]")
{
    ChessDiagnosticCollector collector;
    const auto sink = collector.sink();
    emitChessDiagnostic(sink, ChessDiagnosticSeverity::Warning, "測試來源", "測試警告");
    emitChessDiagnostic(sink, ChessDiagnosticSeverity::Error, "測試來源", "測試錯誤");

    REQUIRE(collector.diagnostics().size() == 2);
    CHECK(collector.diagnostics()[0].source == "測試來源");
    CHECK(collector.hasErrors());
}
