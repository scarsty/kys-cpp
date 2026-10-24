#include "ChessDiagnostics.h"
#include "ChessContentLoader.h"
#include "ChessBattleMapCatalog.h"
#include "ChessGameContent.h"
#include "ChessNeigong.h"
#include "ChessPreparedBattleAnalysis.h"
#include "BattleSetupFactory.h"
#include "battle/BattleRuntimeRandom.h"
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
    突破勝場: 10
    提醒剩餘戰鬥: 15
    各費開場狀態護盾: [80, 40, 20, 0, 0]
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
      每顆強化次數: 2
      強化傷害百分比: 37
      計算上限: 6
    刷新保證:
      觸發星級: 2
      每次數量: 2
  合縱:
    說明: 測試合縱說明
    開局羈絆加點: 1
    高階門檻:
      最低門檻: 7
      降低人數: 2
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

TEST_CASE("challenge NPC roles use configured stats and a single skill at every star", "[chess][content][npc]")
{
    TemporaryConfigDirectory files;
    auto data = syntheticContentData(Difficulty::Normal);
    data.roles.at(10).HeadID = 73;
    const auto path = files.write("challenge.yaml", R"(NPC角色:
  - 角色ID: 60
    名稱: 測試劍魔
    稱號: 測試稱號
    外觀角色ID: 10
    費用: 5
    生命: 777
    內力: 100
    內力性質: 1
    攻擊: 150
    防禦: 120
    輕功: 95
    御劍: 80
    武功: 5
    威力: 900
遠征挑戰: []
)");
    ChessDiagnosticCollector diagnostics;
    REQUIRE(loadChessNpcRoles(path, data,
        [](std::string_view text) { return std::string(text); }, diagnostics.sink()));
    CHECK_FALSE(diagnostics.hasErrors());
    const ChessGameContent content(std::move(data));
    const auto* role = content.role(60);
    REQUIRE(role);
    CHECK(role->HeadID == 73);
    CHECK(role->MaxHP == 777);
    CHECK(role->Cost == 5);
    CHECK(role->Attack == 150);
    CHECK(role->Defence == 120);
    CHECK(role->Speed == 95);
    CHECK_FALSE(std::ranges::contains(content.poolRoleIds(), role->ID));
    for (int star = 1; star <= 3; ++star)
    {
        const auto magics = chessRoleMagicsForStar(content, *role, star);
        REQUIRE(magics.size() == 1);
        CHECK(magics.front().first->ID == 5);
        CHECK(magics.front().second == 900);
    }

    auto invalid = YAML::LoadFile(path.string());
    auto invalidData = syntheticContentData(Difficulty::Normal);
    SECTION("cannot join a recruitable pool") { invalidData.poolRoleIds.push_back(60); }
    SECTION("unknown appearance") { invalid["NPC角色"][0]["外觀角色ID"] = 999; }
    SECTION("unknown martial art") { invalid["NPC角色"][0]["武功"] = 999; }
    SECTION("duplicate role") { invalid["NPC角色"].push_back(YAML::Clone(invalid["NPC角色"][0])); }
    SECTION("invalid tier") { invalid["NPC角色"][0]["費用"] = 6; }
    const auto invalidPath = files.write("invalid.yaml", YAML::Dump(invalid));
    CHECK_FALSE(loadChessNpcRoles(invalidPath, invalidData,
        [](std::string_view text) { return std::string(text); }, diagnostics.sink()));
    CHECK(diagnostics.hasErrors());
}

TEST_CASE("hidden combo keeps both shield resources and activates effects on normal attacks and ultimates", "[chess][content][combo][npc]")
{
    TemporaryConfigDirectory files;
    const auto path = files.write("combos.yaml", R"(羈絆:
  - 名稱: 隱藏測試
    圖示: 9
    NPC專用: true
    成員: [20]
    閾值:
      - 人數: 1
        名稱: 測試門檻
        效果:
          - 類型: 開場狀態護盾
            護盾點數: 78
          - 類型: 開場僵直護盾
            護盾點數: 102
          - 類型: 貫穿清除敵彈
            清除半徑百分比: 250
          - 類型: 出招疊加增傷減傷
            每次層數: 1
            層數上限: 4
            每層增傷百分比: 7
            每層承傷百分比: -3
)");
    ChessDiagnosticCollector diagnostics;
    auto combos = loadChessCombos(path.string(),
        [](std::string_view text) { return std::string(text); }, diagnostics.sink());
    REQUIRE(combos.size() == 1);
    CHECK(combos.front().iconId == 9);
    CHECK(combos.front().isNpcOnly);
    const auto& threshold = combos.front().thresholds.front();
    CHECK(threshold.effects.size() == 4);
    REQUIRE(threshold.rules.size() == 4);
    const auto dispatch = [&](bool ultimate)
    {
        const EffectSourceBinding binding{
            .kind = EffectSourceKind::Combo, .sourceId = 0, .ownerUnitId = 1, .sourceTeam = 1};
        Battle::BattleEffectRuleStore store;
        for (const auto& rule : threshold.rules) store.append(binding, rule);
        const std::vector<Battle::EffectUnitSnapshot> units{
            {.id = 1, .team = 1, .hp = 500, .maxHp = 500},
            {.id = 2, .team = 0, .hp = 500, .maxHp = 500}};
        const Battle::EffectEventData event{
            .event = EffectEvent::AttackCommitted,
            .header = {.binding = binding, .owner = &units.front(), .battle = Battle::BattleEffectReadView(units)},
            .payload = Battle::CastCommitEventData{
                .provenance = {.sourceUnitId = 1, .magicId = 5, .ultimate = ultimate},
                .targetUnitId = 2}};
        Battle::BattleRuntimeRandom random(1);
        return Battle::BattleEffectSystem{}.dispatch(store, event, random);
    };
    CHECK(dispatch(false).commands.size() == 2);
    CHECK(dispatch(true).commands.size() == 2);
    auto data = syntheticContentData(Difficulty::Normal);
    data.roles.at(20).MagicID[0] = 5;
    data.roles.at(20).MagicPower[0] = 100;
    data.combos = std::move(combos);
    const ChessGameContent content(std::move(data));
    PreparedChessBattle prepared;
    prepared.kind = PreparedChessBattleKind::Standalone;
    prepared.units = {{.unitId = 1, .roleId = 20, .team = 1}};
    auto input = BattleSetupFactory::build(prepared, content, 100);
    auto creation = Battle::BattleRuntimeSession::createInitialized(std::move(input));
    const auto& runtime = creation.session.runtime().units.require(1);
    CHECK(runtime.status.effects.statusShield == 78);
    CHECK(runtime.status.effects.staggerShield == 102);
    const auto analysis = analyzePreparedChessBattle(prepared, content, 100);
    REQUIRE(analysis.enemySynergies.size() == 1);
    CHECK(analysis.enemySynergies.front().name == "隱藏測試");
    CHECK(analysis.enemySynergies.front().active);
}

TEST_CASE("combo configuration requires explicit nonnegative icon IDs", "[chess][content][combo][config]")
{
    TemporaryConfigDirectory files;
    auto document = YAML::Load(R"(羈絆:
  - 名稱: 圖示測試
    圖示: 17
    成員: [20]
    閾值:
      - 人數: 1
        名稱: 測試門檻
        效果: []
)");
    bool valid{};
    int expected{};
    auto combo = document["羈絆"][0];
    SECTION("configured ID is independent of combo position") { valid = true; expected = 17; }
    SECTION("zero is a valid asset ID") { combo["圖示"] = 0; valid = true; }
    SECTION("missing icon") { combo.remove("圖示"); }
    SECTION("negative icon") { combo["圖示"] = -1; }
    SECTION("text icon") { combo["圖示"] = "蓮花"; }
    SECTION("fractional icon") { combo["圖示"] = 1.5; }
    SECTION("null icon") { combo["圖示"] = YAML::Null; }
    SECTION("list icon") { combo["圖示"] = YAML::Load("[1, 2]"); }
    const auto path = files.write("icons.yaml", YAML::Dump(document));
    ChessDiagnosticCollector diagnostics;
    const auto combos = loadChessCombos(path.string(),
        [](std::string_view text) { return std::string(text); }, diagnostics.sink());
    CHECK(diagnostics.hasErrors() == !valid);
    if (valid)
    {
        REQUIRE(combos.size() == 1);
        CHECK(combos.front().id == 0);
        CHECK(combos.front().iconId == expected);
    }
    else CHECK(combos.empty());
}

TEST_CASE("synthetic content fingerprint includes the current gameplay schema", "[chess][content][fingerprint]")
{
    // 固定測試自有樣本的指紋，玩法結構變更時明確更新。
    const ChessGameContent content(syntheticContentData(Difficulty::Normal), "0.2.20");
    CHECK(chessSha256Hex(content.contentFingerprint())
        == "d97a46dc6e0a79a3b30dc3c884f685d930452c2d479b739ed4ab73ed14e0a8e3");
}

TEST_CASE("0.2.20 only adds a forward compatibility exception for 0.2.19", "[chess][content][version]")
{
    CHECK(chessGameVersionsCompatible("0.2.19", "0.2.20"));
    CHECK_FALSE(chessGameVersionsCompatible("0.2.20", "0.2.19"));
    CHECK_FALSE(chessGameVersionsCompatible("0.2.18", "0.2.20"));
    CHECK_FALSE(chessGameVersionsCompatible("0.2.17", "0.2.20"));
    CHECK_FALSE(chessGameVersionsCompatible("0.2.19", "0.2.21"));
}

TEST_CASE("cosmetic combo icons do not change the gameplay content fingerprint", "[chess][content][combo][fingerprint]")
{
    auto data = syntheticContentData(Difficulty::Normal);
    data.combos.front().iconId = 37;
    const ChessGameContent ordinary(syntheticContentData(Difficulty::Normal));
    CHECK(ordinary.contentFingerprint() == ChessGameContent(std::move(data)).contentFingerprint());
}

TEST_CASE("NPC visibility participates in the immutable content fingerprint", "[chess][content][npc][fingerprint]")
{
    auto comboData = syntheticContentData(Difficulty::Normal);
    comboData.combos.front().isNpcOnly = true;
    const ChessGameContent ordinary(syntheticContentData(Difficulty::Normal));
    CHECK(ordinary.contentFingerprint() != ChessGameContent(std::move(comboData)).contentFingerprint());
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

TEST_CASE("talent configuration reads the strengthening parameters for each pool", "[chess][content][talent][backbone]")
{
    TemporaryConfigDirectory files;
    const auto balance = files.write("balance.yaml", "棋手天賦: {預設: 中堅, 可選: [中堅]}\n玩家裝備獎勵: {基本: [], 天賦額外: {}}\n");
    const auto challenge = files.write("challenge.yaml", "遠征挑戰: []\n");
    ChessDiagnosticCollector diagnostics;
    BalanceConfig result;
    REQUIRE(loadBalanceConfig(balance.generic_string(), challenge.generic_string(),
        [](std::string_view text) { return std::string(text); }, diagnostics.sink(), result));

    const auto& talent = result.talent(ChessTalentId::Backbone);
    CHECK(talent.strengtheningChargesPerExtraStar == 2);
    CHECK(talent.strengtheningDamagePercent == 37);
    CHECK(talent.extraStarCap == 6);
    const auto& alliance = result.talent(ChessTalentId::Alliance);
    CHECK(alliance.selectedComboBonus == 1);
    CHECK(alliance.reducedThresholdMinimum == 7);
    CHECK(alliance.thresholdReduction == 2);
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

TEST_CASE("late bloomer configuration loads training milestones and tier shields", "[chess][content][talent]")
{
    TemporaryConfigDirectory files;
    auto talents = YAML::Load(std::string(talentsFixtureText));
    talents["棋手天賦"]["晚成"]["突破勝場"] = 31;
    talents["棋手天賦"]["晚成"]["各費開場狀態護盾"] = YAML::Load("[801, 401, 201, 0, 0]");
    files.write("chess_talents.yaml", YAML::Dump(talents));
    const auto balance = files.write("balance.yaml", "棋手天賦: {預設: 晚成, 可選: [晚成]}\n玩家裝備獎勵: {基本: [], 天賦額外: {}}\n");
    const auto challenge = files.write("challenge.yaml", "遠征挑戰: []\n");
    ChessDiagnosticCollector diagnostics;
    BalanceConfig result;
    REQUIRE(loadBalanceConfig(balance.generic_string(), challenge.generic_string(),
        [](std::string_view text) { return std::string(text); }, diagnostics.sink(), result));
    CHECK(result.talent(ChessTalentId::LateBloomer).evolutionWins == 31);
    CHECK(result.talent(ChessTalentId::LateBloomer).evolutionStatusShields == std::array{801, 401, 201, 0, 0});
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
    SECTION("evolution wins") { talents["棋手天賦"]["晚成"]["突破勝場"] = 0; }
    SECTION("evolution shields need five tiers") { talents["棋手天賦"]["晚成"]["各費開場狀態護盾"] = YAML::Load("[80, 40, 20]"); }
    SECTION("evolution shields cannot be negative") { talents["棋手天賦"]["晚成"]["各費開場狀態護盾"][0] = -1; }
    SECTION("negative synergy bonus") { talents["棋手天賦"]["合縱"]["開局羈絆加點"] = -1; }
    SECTION("synergy bonus cannot activate an empty pair") { talents["棋手天賦"]["合縱"]["開局羈絆加點"] = 2; }
    SECTION("invalid reduced threshold") { talents["棋手天賦"]["合縱"]["高階門檻"]["最低門檻"] = 1; }
    SECTION("reduction must leave a positive threshold") { talents["棋手天賦"]["合縱"]["高階門檻"]["降低人數"] = 7; }
    SECTION("stack cap") { talents["棋手天賦"]["賭徒"]["賭運"]["層數上限"] = 0; }
    SECTION("ban range") { talents["棋手天賦"]["賭徒"]["開局額外禁棋"]["最低費用"] = 3; }
    SECTION("zero guarantee count") { talents["棋手天賦"]["中堅"]["刷新保證"]["每次數量"] = 0; }
    SECTION("negative strengthening count") { talents["棋手天賦"]["中堅"]["額外星級加成"]["每顆強化次數"] = -1; }
    SECTION("missing strengthening count") { talents["棋手天賦"]["中堅"]["額外星級加成"].remove("每顆強化次數"); }
    SECTION("zero strengthening percentage") { talents["棋手天賦"]["中堅"]["額外星級加成"]["強化傷害百分比"] = 0; }
    SECTION("excessive strengthening percentage") { talents["棋手天賦"]["中堅"]["額外星級加成"]["強化傷害百分比"] = 101; }
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
