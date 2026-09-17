#include "ChessDiagnostics.h"
#include "ChessContentLoader.h"
#include "ChessBattleMapCatalog.h"
#include "ChessGameContent.h"
#include "ChessNeigong.h"
#include "GameVersion.h"
#include "yaml-cpp/yaml.h"

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

    EffectRule effectRule;
    effectRule.id = {1};
    effectRule.event = EffectEvent::BattleInitialized;
    ModifyAttributeAction modifier;
    modifier.attribute = BattleAttribute::Attack;
    modifier.amount.flat = 10;
    modifier.operation = AttributeOperation::FlatAdd;
    effectRule.actions.push_back({modifier});
    ChessMagicEffectDefinition effect;
    effect.magicId = magic.ID;
    effect.name = "顯示名稱";
    effect.purpose = "作者備註";
    effect.rules.push_back(std::move(effectRule));
    data.magicEffects.push_back(std::move(effect));
    return data;
}

// 測試自有天賦目錄樣本：欄位結構對齊正式設定，數值皆為測試所有。
constexpr std::string_view talentsFixtureText = R"(棋手天賦:
  神兵:
    說明: 測試神兵說明
    可使用神兵商店: true
  晚成:
    說明: 測試晚成說明
    勝場成長受加成比例: 10
  賭徒:
    說明: 測試賭徒說明
    開局額外禁棋:
      次數: 2
      最低費用: 1
      最高費用: 2
    賭運:
      累積截止關卡: 20
      目標最低費用: 1
      目標最高費用: 3
      每次增加層數: 1
      每層觸發機率百分點: 10
      層數上限: 5
      觸發後生命: 30
      無敵幀數: 60
  中堅:
    說明: 測試中堅說明
    目標費用: 4
    額外星級加成:
      每顆開場內力: 12
      計算上限: 6
    刷新保證:
      觸發星級: 2
      每次數量: 2
)";

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
        std::ofstream output(path_ / "chess_talents.yaml", std::ios::binary);
        output << talentsFixtureText;
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

TEST_CASE("star skill selection ignores empty slots even when magic zero exists", "[chess][content]")
{
    auto data = syntheticContentData(Difficulty::Normal);
    ChessMagicDefinition emptySlotMagic;
    emptySlotMagic.ID = 0;
    emptySlotMagic.Name = "普通攻擊";
    data.magics.emplace(0, emptySlotMagic);
    const ChessGameContent content(std::move(data));
    ChessRoleDefinition role;

    for (int star = 1; star <= 3; ++star)
    {
        CHECK(chessRoleMagicsForStar(content, role, star).empty());
        const int slot = RoleSave::getMagicSlotStart(star);
        role.MagicID[slot] = 5;
        role.MagicPower[slot] = 800;
        const auto magics = chessRoleMagicsForStar(content, role, star);
        REQUIRE(magics.size() == 1);
        CHECK(magics.front().first->ID == 5);
        CHECK(magics.front().second == 800);
    }
}

TEST_CASE("immutable content carries the exact game version", "[chess][content][version]")
{
    const ChessGameContent content(syntheticContentData(Difficulty::Normal), "1.2.3");
    CHECK(content.gameVersion() == "1.2.3");
}

TEST_CASE("content fingerprint follows semantic rules but not the release label", "[chess][content][fingerprint]")
{
    auto changedData = syntheticContentData(Difficulty::Normal);
    changedData.balance.initialMoney++;
    auto renamedEffectData = syntheticContentData(Difficulty::Normal);
    renamedEffectData.magicEffects.front().name = "另一個顯示名稱";
    renamedEffectData.magicEffects.front().purpose = "另一段作者備註";
    auto changedEffectData = syntheticContentData(Difficulty::Normal);
    auto& changedModifier = std::get<ModifyAttributeAction>(
        changedEffectData.magicEffects.front().rules.front().actions.front().value);
    changedModifier.amount.flat++;
    auto unfilteredStatusData = syntheticContentData(Difficulty::Normal);
    auto& unfilteredStatusNumber = std::get<ModifyAttributeAction>(
        unfilteredStatusData.magicEffects.front().rules.front().actions.front().value)
        .amount;
    unfilteredStatusNumber.base = EffectNumberBase::SourceStatusQuantity;
    unfilteredStatusNumber.status = BattleStatusKind::TrueQi;
    unfilteredStatusNumber.percent = 100;
    unfilteredStatusNumber.flat = 0;
    auto ownerFilteredStatusData = unfilteredStatusData;
    std::get<ModifyAttributeAction>(
        ownerFilteredStatusData.magicEffects.front().rules.front().actions.front().value)
        .amount.statusSource = StatusSourceMatch::EffectOwner;
    const ChessGameContent first(syntheticContentData(Difficulty::Normal), "1.2.3");
    const ChessGameContent relabeled(syntheticContentData(Difficulty::Normal), "2.0.0");
    const ChessGameContent changed(std::move(changedData), "1.2.3");
    const ChessGameContent renamedEffect(std::move(renamedEffectData), "1.2.3");
    const ChessGameContent changedEffect(std::move(changedEffectData), "1.2.3");
    const ChessGameContent unfilteredStatus(
        std::move(unfilteredStatusData), "1.2.3");
    const ChessGameContent ownerFilteredStatus(
        std::move(ownerFilteredStatusData), "1.2.3");

    CHECK(first.contentFingerprint() == relabeled.contentFingerprint());
    CHECK(first.contentFingerprint() == renamedEffect.contentFingerprint());
    CHECK(first.contentFingerprint() != changed.contentFingerprint());
    CHECK(first.contentFingerprint() != changedEffect.contentFingerprint());
    CHECK(unfilteredStatus.contentFingerprint()
        != ownerFilteredStatus.contentFingerprint());
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

TEST_CASE("content roots discover repository and packaged layouts from the executable", "[chess][content][path]")
{
    TemporaryConfigDirectory files;
    files.write("repository/work/game-dev/save/game.db", "");
    files.write("repository/work/game-dev/cc/STPhrases.txt", "");
    files.write("repository/config/chess_challenge.yaml", "");
    const auto repositoryExecutable = files.write("repository/x64/Release/kys_chess_cli.exe", "");

    const auto repositoryRoots = discoverChessContentRoots(repositoryExecutable);
    CHECK(repositoryRoots.dataRoot == std::filesystem::weakly_canonical(
        files.path() / "repository/work/game-dev"));
    CHECK(repositoryRoots.configRoot == std::filesystem::weakly_canonical(
        files.path() / "repository/config"));

    files.write("package/game/save/game.db", "");
    files.write("package/game/cc/STPhrases.txt", "");
    files.write("package/game/config/chess_challenge.yaml", "");
    files.write("config/chess_challenge.yaml", "");
    const auto packagedExecutable = files.write("package/bin/kys_chess_cli.exe", "");

    const auto packagedRoots = discoverChessContentRoots(packagedExecutable);
    CHECK(packagedRoots.dataRoot == std::filesystem::weakly_canonical(
        files.path() / "package/game"));
    CHECK(packagedRoots.configRoot == std::filesystem::weakly_canonical(
        files.path() / "package/game/config"));
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
    const auto balance = files.write("balance.yaml", "棋手天賦: {預設: 神兵, 可選: [神兵]}\n玩家裝備獎勵: {基本: [], 天賦額外: {}}\n");
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
    const auto balance = files.write("balance.yaml", "棋手天賦: {預設: 神兵, 可選: [神兵]}\n玩家裝備獎勵: {基本: [], 天賦額外: {}}\n");
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
    const auto balance = files.write("balance.yaml", "棋手天賦: {預設: 神兵, 可選: [神兵]}\n玩家裝備獎勵: {基本: [], 天賦額外: {}}\n");
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
        "選擇數量: 1\n"
        "層級分配:\n"
        "  - 層級: 1\n"
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

TEST_CASE("talent configuration rejects malformed identities ranges and legacy keys", "[chess][talent][content]")
{
    // 測試自有平衡樣本：欄位結構對齊正式設定，數值皆為本測試所有。
    constexpr std::string_view balanceFixture = R"(棋手天賦:
  預設: 神兵
  可選: [神兵]
玩家裝備獎勵:
  基本:
    - 關卡: 3
      最高層級: 2
      選項數量: 2
      追加選項費用: 4
      最低層級: 1
    - 關卡: 7
      最高層級: 3
      選項數量: 2
      追加選項費用: 4
      最低層級: 1
神兵商店:
  通關後: 5
  價格: 30
)";
    TemporaryConfigDirectory files;
    auto balance = YAML::Load(std::string(balanceFixture));
    auto talents = YAML::Load(std::string(talentsFixtureText));
    SECTION("unknown talent") { balance["棋手天賦"]["預設"] = "未知"; }
    SECTION("duplicate choice") { balance["棋手天賦"]["可選"].push_back("神兵"); }
    SECTION("empty choice") { balance["棋手天賦"]["可選"] = YAML::Node(YAML::NodeType::Sequence); }
    SECTION("default outside choices") { balance["棋手天賦"]["可選"] = YAML::Load("[晚成]"); }
    SECTION("missing catalog entry") { talents["棋手天賦"].remove("賭徒"); }
    SECTION("unknown catalog field") { talents["棋手天賦"]["神兵"]["未知"] = 1; }
    SECTION("unknown catalog root") { talents["未知"] = 1; }
    SECTION("growth percentage") { talents["棋手天賦"]["晚成"]["勝場成長受加成比例"] = 101; }
    SECTION("stack cap") { talents["棋手天賦"]["賭徒"]["賭運"]["層數上限"] = 0; }
    SECTION("ban range") { talents["棋手天賦"]["賭徒"]["開局額外禁棋"]["最低費用"] = 3; }
    SECTION("zero guarantee count") { talents["棋手天賦"]["中堅"]["刷新保證"]["每次數量"] = 0; }
    SECTION("equipment range") { balance["玩家裝備獎勵"]["基本"][0]["最低層級"] = 4; }
    SECTION("missing equipment minimum") { balance["玩家裝備獎勵"]["基本"][0].remove("最低層級"); }
    SECTION("duplicate reward round") { balance["玩家裝備獎勵"]["基本"][1]["關卡"] = 3; }
    SECTION("negative shop price") { balance["神兵商店"]["價格"] = -30; }
    SECTION("legacy simplified key") { balance["玩家装备奖励"] = balance["玩家裝備獎勵"]; balance.remove("玩家裝備獎勵"); }
    const auto path = files.write("chess_balance_hard.yaml", YAML::Dump(balance));
    files.write("chess_talents.yaml", YAML::Dump(talents));
    const auto challenge = files.write("chess_challenge.yaml", "遠征挑戰: []\n");
    BalanceConfig parsed;
    ChessDiagnosticCollector diagnostics;
    CHECK_FALSE(loadBalanceConfig(
        path.string(),
        challenge.string(),
        {},
        diagnostics.sink(),
        parsed));
}
