#include "ChessMenuFormatting.h"
#include "ChessGameSession.h"
#include "ChessTalentUi.h"
#include "ChessUiCommon.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

int testDisplayWidth(const std::string& text)
{
    int width = 0;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto lead = static_cast<unsigned char>(text[index]);
        if (lead < 0x80)
        {
            ++width;
            ++index;
            continue;
        }
        const int length = lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4);
        width += 2;
        index += length;
    }
    return width;
}

int displayColumnBefore(const std::string& text, const std::string& marker)
{
    return testDisplayWidth(text.substr(0, text.find(marker)));
}

const ChessGameGuideSection& guideSection(
    const std::vector<ChessGameGuideSection>& sections,
    const std::string& title)
{
    const auto found = std::ranges::find(sections, title, &ChessGameGuideSection::title);
    REQUIRE(found != sections.end());
    return *found;
}

}

TEST_CASE("save rows align legacy timestamps and full-width separators", "[chess][menu-formatting][save]")
{
    CHECK(displayTextWidth("·") == 2);
    CHECK(displayTextWidth("賭徒 · 第2關") == 13);
    std::vector<std::string> rows{
        "進度01  2026-09-10 23:48:11  賭徒 · 第2關",
        "進度02  2026-07-27 00:19:32",
        "進度03  -------------------",
        "自動檔  2026-09-11 00:46:42  大器晚成 · 第12關",
    };
    const auto original = rows;
    alignDisplayTextRows(rows);
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        CHECK(rows[index].starts_with(original[index]));
        CHECK(displayTextWidth(rows[index]) == displayTextWidth(original.back()));
        CHECK(displayColumnBefore(rows[index], "  ") == 6);
    }
}

TEST_CASE("panel text fitting chooses the largest complete readable layout",
          "[chess][menu-formatting][panel-text]")
{
    const std::vector<PanelTextSourceRow> rows{
        { .text = "第一段文字需要換行", .spacingAfter = 2 },
        { .text = "第二段", .fontSizeDelta = 2, .indentUnits = 2,
          .spacingBefore = 3, .spacingAfter = 1 },
    };
    const auto fitted = fitPanelText(rows, 80, 68, 20, 12);
    CHECK(fitted.baseFontSize == 16);
    CHECK(fitted.height <= 68);
    REQUIRE_FALSE(fitted.lines.empty());
    CHECK(fitted.lines.front().sourceRow == 0);
    CHECK(fitted.lines.back().sourceRow == 1);
    for (const auto& line : fitted.lines)
    {
        CHECK(line.y + line.fontSize <= fitted.height);
        CHECK(line.indentPixels
            + displayTextWidth(line.text) * line.fontSize / 2 <= 80);
    }

    const auto tooLarge = layoutPanelText(rows, 80, fitted.baseFontSize + 1);
    CHECK(tooLarge.height > 68);
}

TEST_CASE("card prose wrapping preserves punctuation and avoids orphaned introductions",
          "[chess][menu-formatting][panel-text][player-card]")
{
    const std::string text = "觸發時，其原攻擊目標獲得每星100護盾。";
    for (int width : {18, 24, 30, 40})
    {
        const auto lines = wrapDisplayText(text, width, true, DisplayTextWrapping::Prose);
        REQUIRE_FALSE(lines.empty());
        CHECK(lines.front() != "觸發時，");
        std::string joined;
        for (const auto& line : lines)
        {
            CHECK(displayTextWidth(line) <= width);
            CHECK_FALSE(line.starts_with("，"));
            CHECK_FALSE(line.starts_with("。"));
            joined += line;
        }
        CHECK(joined == text);
        CHECK(std::ranges::any_of(lines, [](const auto& line) { return line.contains("100"); }));
    }
    CHECK(wrapDisplayText("天地玄黃。", 8, true, DisplayTextWrapping::Prose)
        == std::vector<std::string>{"天地玄", "黃。"});
    const std::vector<PanelTextSourceRow> rows{
        {.text = text, .wrapping = DisplayTextWrapping::Prose},
    };
    const auto layout = layoutPanelText(rows, 168, 14);
    CHECK(layout.lines.front().text == wrapDisplayText(text, 24, true, DisplayTextWrapping::Prose).front());
}

TEST_CASE("talent presentation exposes an equipment reward table and descriptive mechanics",
          "[chess][menu-formatting][talent]")
{
    BalanceConfig balance;
    balance.talents.clear();
    balance.playerEquipmentRewards = {{11, 2, 2, 4, 1}, {3, 2, 2, 4, 1}};
    balance.talentEquipmentRewards[ChessTalentId::DivineArms] = {
        {15, 4, 6, 6, 2},
        {7, 3, 4, 6, 2},
    };
    balance.legendaryShop = {9, 27};

    ChessTalentDefinition divineArms;
    divineArms.description = "神兵說明來自設定";
    divineArms.legendaryShop = true;
    balance.talents.emplace(ChessTalentId::DivineArms, divineArms);

    ChessTalentDefinition lateBloomer;
    lateBloomer.description = "晚成說明來自設定";
    lateBloomer.amplifiedGrowthPercent = 73;
    balance.talents.emplace(ChessTalentId::LateBloomer, lateBloomer);

    ChessTalentDefinition gambler;
    gambler.description = "賭徒說明來自設定";
    gambler.openingBans = 8;
    gambler.banMinTier = 1;
    gambler.banMaxTier = 4;
    gambler.luckLastFight = 19;
    gambler.luckMinTier = 2;
    gambler.luckMaxTier = 5;
    gambler.luckPerRefresh = 3;
    gambler.luckChancePerStack = 11;
    gambler.luckStackCap = 7;
    gambler.luckSurvivalHp = 2;
    gambler.luckInvincibleFrames = 121;
    balance.talents.emplace(ChessTalentId::Gambler, gambler);

    ChessTalentDefinition backbone;
    backbone.description = "中堅說明來自設定";
    backbone.targetTier = 4;
    backbone.mpPerExtraStar = 12;
    backbone.extraStarCap = 6;
    backbone.guaranteeStar = 2;
    backbone.guaranteeCount = 2;
    balance.talents.emplace(ChessTalentId::Backbone, backbone);

    const auto fact = [](const ChessTalentPresentation& presentation,
                         const std::string& category,
                         const std::string& label) -> const ChessTalentFactRow& {
        const auto found = std::ranges::find_if(presentation.facts, [&](const auto& row) {
            return row.category == category && row.label == label;
        });
        REQUIRE(found != presentation.facts.end());
        return *found;
    };

    const auto divinePresentation = buildChessTalentPresentation(balance, ChessTalentId::DivineArms);
    CHECK(divinePresentation.description.contains("神兵說明來自設定"));
    CHECK(divinePresentation.description.contains("\n神兵商店：第9關後開放，每件27金。"));
    REQUIRE(divinePresentation.facts.size() == 5);
    CHECK(divinePresentation.facts[0].category == "基本");
    CHECK(divinePresentation.facts[0].label == "第3關");
    CHECK(divinePresentation.facts[0].value == "1～2階｜2選項");
    CHECK(divinePresentation.facts[0].kind == ChessTalentFactKind::Equipment);
    CHECK(divinePresentation.facts[1].category == "天賦額外");
    CHECK(divinePresentation.facts[1].label == "第7關");
    CHECK(divinePresentation.facts[1].value == "2～3階｜4選項");
    CHECK(divinePresentation.facts[1].kind == ChessTalentFactKind::TalentEquipment);
    CHECK(divinePresentation.facts[2].category == "神兵商店");
    CHECK(divinePresentation.facts[2].label == "第9關後");
    CHECK(divinePresentation.facts[2].value == "每件27金");
    CHECK(divinePresentation.facts[2].kind == ChessTalentFactKind::Shop);
    CHECK(divinePresentation.facts[3].label == "第11關");
    CHECK(divinePresentation.facts[4].label == "第15關");
    CHECK_FALSE(divinePresentation.description.contains("追加4金"));
    CHECK_FALSE(divinePresentation.description.contains("神兵商店：不可用"));
    CHECK(chessTalentPresentationUsesEquipmentTable(divinePresentation));

    const auto latePresentation = buildChessTalentPresentation(balance, ChessTalentId::LateBloomer);
    CHECK(latePresentation.description.contains("晚成說明來自設定"));
    REQUIRE(latePresentation.facts.size() == 5);
    CHECK(fact(latePresentation, "一般基準", "加成").value
        == "0%（勝場成長不受星級倍率放大）");
    CHECK(fact(latePresentation, "晚成", "加成").value
        == "73%（勝場成長會併入星級倍率）");
    CHECK(fact(latePresentation, "每多1星", "星級倍率").value
        == "生命 +80%、攻擊 +80%、防禦 +50%、武功 +50%、輕功 +25%");
    CHECK(chessTalentPresentationUsesEquipmentTable(latePresentation) == false);
    CHECK_FALSE(latePresentation.description.contains("神兵商店"));

    const auto gamblerPresentation = buildChessTalentPresentation(balance, ChessTalentId::Gambler);
    REQUIRE(gamblerPresentation.facts.size() == 4);
    CHECK(fact(gamblerPresentation, "開局", "額外禁棋").value == "8枚（1～4費）");
    CHECK(fact(gamblerPresentation, "賭運", "取得方式").value
        == "第1～19關付費刷新；隨機選2～5費未滿層棋子 +3層，優先上場棋子");
    CHECK(fact(gamblerPresentation, "賭運", "觸發機率").value == "每層 +11%，最多7層（77%）");
    CHECK(fact(gamblerPresentation, "致命傷害", "成功效果").value == "保留2生命，無敵121幀");
    CHECK_FALSE(chessTalentPresentationUsesEquipmentTable(gamblerPresentation));

    const auto backbonePresentation = buildChessTalentPresentation(balance, ChessTalentId::Backbone);
    REQUIRE(backbonePresentation.facts.size() == 4);
    CHECK(fact(backbonePresentation, "開場內力", "適用對象").value
        == "4費棋子；只計算非4費友軍的額外星級");
    CHECK(fact(backbonePresentation, "開場內力", "計算方式").value
        == "非4費友軍每多1星 +12，最多計6星，最高 +72");
    CHECK(fact(backbonePresentation, "定向增援", "觸發時機").value
        == "4費棋子由1星升至2星時，商店保證2枚同名棋子");
    CHECK(fact(backbonePresentation, "定向增援", "升至3星").value
        == "升至3星後取消尚未使用的保證棋子");
    CHECK_FALSE(chessTalentPresentationUsesEquipmentTable(backbonePresentation));
    CHECK(chessTalentDescription(balance, ChessTalentId::Backbone).contains("每多1星 +12"));
}
TEST_CASE("chess menu labels align stars and prices by measured display width", "[chess][menu-formatting]")
{
    const auto labels = buildAlignedChessMenuLabels(
        {
            {"", "楊過", "★", "$1"},
            {"[戰]", "黃蓉", "★★", "$12"},
            {"", "獨孤求敗", "★★★", "$123"},
        },
        testDisplayWidth);

    REQUIRE(labels.size() == 3);
    CHECK(displayColumnBefore(labels[0], "★") == displayColumnBefore(labels[1], "★"));
    CHECK(displayColumnBefore(labels[1], "★") == displayColumnBefore(labels[2], "★"));
    CHECK(testDisplayWidth(labels[0]) == testDisplayWidth(labels[1]));
    CHECK(testDisplayWidth(labels[1]) == testDisplayWidth(labels[2]));
}

TEST_CASE("menu action and equipment rows share the same measured columns", "[chess][menu-formatting]")
{
    const auto labels = buildAlignedChessMenuLabels(
        {
            {"", "刷新商店", "", "$2"},
            {"", "[已鎖定] 點擊解鎖", "", ""},
            {"", "[傳說] 倚天劍", "[已裝]", "$40"},
        },
        testDisplayWidth);

    REQUIRE(labels.size() == 3);
    const int width = testDisplayWidth(labels.front());
    CHECK(testDisplayWidth(labels[1]) == width);
    CHECK(testDisplayWidth(labels[2]) == width);
}

TEST_CASE("equipment inventory alignment does not reserve unused global columns",
          "[chess][menu-formatting][equipment]")
{
    const auto labels = buildContentSizedAlignedChessMenuLabels(
        {
            {"[初階] ", "射雕弓", " [已裝]", ""},
            {"[初階] ", "越女劍", "", ""},
        },
        testDisplayWidth);

    REQUIRE(labels.size() == 2);
    CHECK(testDisplayWidth(labels[0]) == testDisplayWidth(labels[1]));
    CHECK(testDisplayWidth(labels[0]) < ChessScreenLayout::kDefaultMenuItemUnits);
    CHECK(labels[0].contains(" [已裝]"));
}

TEST_CASE("equipment reward alignment does not reserve unused global columns",
          "[chess][menu-formatting][equipment][reward]")
{
    const std::vector<ChessMenuColumnRow> rows{
        {"[中階] ", "綠波香露刀", "", ""},
        {"[高階] ", "大燕傳國玉璽", "", ""},
        {"", "刷新", "", "$4"},
    };

    const auto labels = buildAlignedChessRewardMenuLabels(
        ChessRewardKind::Equipment,
        rows,
        testDisplayWidth);

    REQUIRE(labels.size() == 3);
    const int width = testDisplayWidth(labels.front());
    CHECK(testDisplayWidth(labels[1]) == width);
    CHECK(testDisplayWidth(labels[2]) == width);
    CHECK(width < ChessScreenLayout::kDefaultMenuItemUnits);
}

TEST_CASE("legacy browse menu typography and reward overrides stay explicit", "[chess][menu-formatting][legacy]")
{
    CHECK((kChessBrowseMenuPresentation == ChessMenuPresentation{36, 10}));
    CHECK((kChessCompactMenuPresentation == ChessMenuPresentation{32, 12}));
    CHECK((kChessChallengeMenuPresentation == ChessMenuPresentation{36, 9}));
    CHECK((kChessPositionSwapMenuPresentation == ChessMenuPresentation{36, 2}));

    ChessPendingReward campaignEquipment;
    campaignEquipment.kind = ChessRewardKind::Equipment;
    CHECK((chessRewardMenuPresentation(campaignEquipment, 4) == ChessMenuPresentation{36, 4}));

    auto challengeEquipment = campaignEquipment;
    challengeEquipment.challengeName = "遠征";
    CHECK((chessRewardMenuPresentation(challengeEquipment, 7) == ChessMenuPresentation{36, 10}));

    ChessPendingReward campaignNeigong;
    campaignNeigong.kind = ChessRewardKind::InternalSkill;
    CHECK((chessRewardMenuPresentation(campaignNeigong, 5) == ChessMenuPresentation{36, 5}));

    auto challengeNeigong = campaignNeigong;
    challengeNeigong.challengeName = "遠征";
    CHECK((chessRewardMenuPresentation(challengeNeigong, 7) == ChessMenuPresentation{36, 16}));

    ChessPendingReward challengeReward;
    challengeReward.kind = ChessRewardKind::ChallengeReward;
    challengeReward.challengeName = "遠征";
    CHECK((chessRewardMenuPresentation(challengeReward, 3) == ChessMenuPresentation{36, 3}));

    ChessPendingReward pieceReward;
    pieceReward.kind = ChessRewardKind::Piece;
    CHECK((chessRewardMenuPresentation(pieceReward, 20) == ChessMenuPresentation{32, 12}));

    ChessPendingReward starReward;
    starReward.kind = ChessRewardKind::StarUpgrade;
    CHECK((chessRewardMenuPresentation(starReward, 20) == ChessMenuPresentation{32, 12}));
}

TEST_CASE("menu state prefixes retain their visible legacy separators", "[chess][menu-formatting][legacy]")
{
    CHECK(chessMenuPrefixWithSeparator("").empty());
    CHECK(chessMenuPrefixWithSeparator("[初階]") == "[初階] ");
    CHECK(chessMenuPrefixWithSeparator("[已通關] ") == "[已通關] ");
    CHECK(chessEquipmentAssignmentColumn(false).empty());
    CHECK(chessEquipmentAssignmentColumn(true) == " [已裝]");

    const auto labels = buildAlignedChessMenuLabels(
        {
            {chessMenuPrefixWithSeparator("[初階]"), "倚天劍", chessEquipmentAssignmentColumn(true), ""},
            {chessMenuPrefixWithSeparator("[✓]"), "楊過", "", ""},
            {chessMenuPrefixWithSeparator("[已通關]"), "聚賢莊內", "", ""},
        },
        testDisplayWidth);

    CHECK(labels[0].find("[初階] 倚天劍") != std::string::npos);
    CHECK(labels[0].find(" [已裝]") != std::string::npos);
    CHECK(labels[1].find("[✓] 楊過") != std::string::npos);
    CHECK(labels[2].find("[已通關] 聚賢莊內") != std::string::npos);
}

TEST_CASE("combo catalog aligns mixed-width names and progress counts", "[chess][menu-formatting]")
{
    const auto labels = buildAlignedComboCatalogLabels(
        {
            {"拳師", "3/5 ✓"},
            {"江南七怪", "2+1/5 ✓"},
            {"獨行", "獨/1 ✓"},
        },
        testDisplayWidth);

    REQUIRE(labels.size() == 3);
    const int width = testDisplayWidth(labels.front());
    CHECK(testDisplayWidth(labels[1]) == width);
    CHECK(testDisplayWidth(labels[2]) == width);
}

TEST_CASE("panel text wrapping reserves indentation inside the right boundary", "[chess][ui][wrapping]")
{
    constexpr int fontSize = 22;
    constexpr int pixelWidth = 260;
    constexpr int indent = 22;
    const std::vector<std::string> descriptions{
        "15%擊退130距離並鎖定5幀",
        "絕招觸發：全隊回105HP",
    };

    const int unindentedUnits = displayTextUnitsForPixelWidth(fontSize, pixelWidth);
    const int indentedUnits = displayTextUnitsForPixelWidth(fontSize, pixelWidth, indent);
    CHECK(unindentedUnits == 23);
    CHECK(indentedUnits == 21);
    CHECK(testDisplayWidth(descriptions[0]) <= unindentedUnits);
    CHECK(testDisplayWidth(descriptions[0]) > indentedUnits);
    CHECK(testDisplayWidth(descriptions[1]) <= indentedUnits);
    CHECK(indent + fontSize * indentedUnits / 2 <= pixelWidth);
}

TEST_CASE("equipment inventory keeps owned rows first then tier and item order", "[chess][menu-formatting]")
{
    std::vector<ChessEquipmentMenuOrder> rows{
        {false, 1, 10},
        {true, 3, 30},
        {true, 1, 20},
        {true, 1, 10},
        {false, 1, 5},
    };

    std::stable_sort(rows.begin(), rows.end(), chessEquipmentMenuOrderLess);

    CHECK((rows == std::vector<ChessEquipmentMenuOrder>{
        {true, 1, 10},
        {true, 1, 20},
        {true, 3, 30},
        {false, 1, 5},
        {false, 1, 10},
    }));
}

TEST_CASE("game guide derives economy equipment and reward details from content", "[chess][menu-formatting]")
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Normal;
    data.balance.totalFights = 17;
    data.balance.bossInterval = 3;
    data.balance.battleExp = 6;
    data.balance.bossBattleExp = 11;
    data.balance.shopSlotCount = 4;
    data.balance.tierPrices = {2, 4, 7, 11, 16};
    data.balance.starCostMult = 4;
    data.balance.initialMoney = 23;
    data.balance.refreshCost = 5;
    data.balance.buyExpCost = 7;
    data.balance.buyExpAmount = 9;
    data.balance.interestPercent = 15;
    data.balance.interestMax = 6;
    data.balance.bossRewardBonus = 8;
    data.balance.maxLevel = 8;
    data.balance.benchSize = 12;
    data.balance.minBattleSize = 3;
    data.balance.fightWinGrowthHP = 12;
    data.balance.fightWinGrowthAtk = 1;
    data.balance.fightWinGrowthDef = 1;
    data.balance.fightWinGrowthWeapon = 0.5;
    data.balance.fightWinGrowthSpeed = 0.25;
    data.balance.playerEquipmentRewards = {{9, 2, 1, 0}, {4, 4, 2, 3}};
    data.balance.legendaryShop = {13, 37};
    data.balance.challenges.resize(3);
    data.neigongConfig.choiceCount = 5;
    data.neigongConfig.additionalOptionCost = 6;
    const ChessGameContent content(std::move(data));

    const auto sections = buildChessGameGuideSections(content);

    REQUIRE(sections.size() == 6);
    const auto& basic = guideSection(sections, "基本流程");
    CHECK(basic.lines[1].text == "· 戰鬥全自動進行，共17回；每3回有一位強敵");
    CHECK(basic.lines[2].text == "· 勝利得6經驗，強敵勝利得11經驗");
    const auto& pieces = guideSection(sections, "棋子與升星");
    CHECK(pieces.lines[1].text == "· 1費2金、2費4金、3費7金、4費11金、5費16金；星級價格按4倍計");
    CHECK(pieces.lines[3].text == "· 主線勝利或首次通關遠征時，所有出戰棋子勝場+1；合成取三枚中的最高勝場，不會相加");
    CHECK(pieces.lines[4].text == "· 每個勝場提供生命+12、攻擊+1、防禦+1、各項兵器熟練度+0.5、輕功+0.25；小數按總勝場累計後取整");
    CHECK(pieces.lines[4].tone == ChessGameGuideLineTone::Information);
    const auto& economy = guideSection(sections, "經濟與等級");
    CHECK(economy.lines[0].text == "· 初始23金，刷新商店5金，購買經驗7金換9點");
    CHECK(economy.lines[1].text == "· 存款按15%生利息，上限6金；強敵獎勵另加8金");
    CHECK(economy.lines[2].text == "· 最高8級，背包12格，最低3人出戰");
    const auto& equipment = guideSection(sections, "裝備與內功");
    CHECK(equipment.lines[1].text == "· 固定裝備獎勵共2次，最早第4回，最高4階");
    CHECK(equipment.lines[2].text == "· 神兵商店第13回後開放，每件37金");
    CHECK(equipment.lines[3].text == "· 強敵戰後提供5本免費與額外候選；選擇額外候選加收6金");
    CHECK(guideSection(sections, "遠征挑戰").lines[0].text == "· 遠征挑戰共3關，不佔主線回合，勝後獎勵各領一次");
}

TEST_CASE("game guide inserts the configured ban mode with legacy emphasis", "[chess][menu-formatting]")
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Hard;
    data.balance.shopSlotCount = 6;
    data.balance.maxLevel = 10;
    data.balance.banBaseCount = 2;
    data.balance.banCountPerLevel = 1;
    data.balance.banUnlocks = {{8, 1, 3}, {3, 2, 2}};
    const ChessGameContent content(std::move(data));

    const auto sections = buildChessGameGuideSections(content);

    REQUIRE(sections.size() == 7);
    REQUIRE(sections[3].title == "困難棋局");
    REQUIRE(sections[3].lines.size() == 3);
    CHECK(sections[3].lines[0].text == "· 本難度商店每回合6格可選，最高10級");
    CHECK(sections[3].lines[0].tone == ChessGameGuideLineTone::Information);
    CHECK(sections[3].lines[1].text == "· 開局可禁2名棋子；每升1級，再增1名禁位");
    CHECK(sections[3].lines[1].tone == ChessGameGuideLineTone::Highlight);
    CHECK(sections[3].lines[2].text == "· 額外禁棋共2段，最早第3回後解鎖，最高可禁3費");
    CHECK(sections[3].lines[2].tone == ChessGameGuideLineTone::Standard);
    const auto& equipment = guideSection(sections, "裝備與內功");
    CHECK(equipment.lines[1].text == "· 本難度沒有固定裝備獎勵，主要從遠征挑戰與商店取得裝備");
    CHECK(equipment.lines[2].text == "· 神兵商店未開放，可從獎勵與挑戰取得裝備");
    CHECK(guideSection(sections, "遠征挑戰").lines[0].text == "· 目前沒有遠征挑戰配置");
}

TEST_CASE("shop emphasis ignores a lone completed three-star", "[chess][menu-formatting][shop]")
{
    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{1, 10, 3});

    CHECK(chessShopRowEmphasis(state, 10) == ChessShopRowEmphasis::None);
    const auto none = chessShopRowOutlineStyle(chessShopRowEmphasis(state, 10));
    CHECK(none.color.a == 0);
    CHECK_FALSE(none.animated);
    CHECK(none.thickness == 1);

    state.roster.emplace(2, ChessSessionPiece{2, 10, 1});
    CHECK(chessShopRowEmphasis(state, 10) == ChessShopRowEmphasis::Owned);
    const auto owned = chessShopRowOutlineStyle(chessShopRowEmphasis(state, 10));
    CHECK(owned.color.r == 100);
    CHECK(owned.color.g == 200);
    CHECK(owned.color.b == 255);
    CHECK(owned.color.a == 220);
    CHECK(owned.animated);
    CHECK(owned.thickness == 1);

    state.roster.emplace(3, ChessSessionPiece{3, 10, 1});
    CHECK(chessShopRowEmphasis(state, 10) == ChessShopRowEmphasis::Merge);
    const auto merge = chessShopRowOutlineStyle(chessShopRowEmphasis(state, 10));
    CHECK(merge.color.r == 255);
    CHECK(merge.color.g == 215);
    CHECK(merge.color.b == 0);
    CHECK(merge.color.a == 255);
    CHECK(merge.animated);
    CHECK(merge.thickness == 3);
}

TEST_CASE("buy experience preview uses config-driven deployment capacity", "[chess][menu-formatting][experience]")
{
    ChessGameContentData data;
    data.balance.maxLevel = 9;
    data.balance.minBattleSize = 4;
    const ChessGameContent content(std::move(data));
    ChessSessionState state;

    state.level = 2;
    const auto plateau = buildChessBuyExpDeploymentPreview(state, content);
    CHECK(plateau.current == 4);
    CHECK_FALSE(plateau.next);

    state.level = 3;
    state.experience = 7;
    state.money = 23;
    const auto increase = buildChessBuyExpDeploymentPreview(state, content);
    CHECK(increase.current == 4);
    REQUIRE(increase.next);
    CHECK(*increase.next == 5);

    const auto text = buildChessBuyExpPreviewText(state, content);
    CHECK(text.header == "等級 4    經驗 7/14    金幣 $23");
    CHECK(text.costLine == "花費 $5  獲得 5 經驗");
    CHECK(text.deploymentLine == "出戰人數: 4");
    CHECK(text.deploymentNext == "  → 5");
    CHECK(text.currentWeights == "  1費50% 2費35% 3費15% 4費0% 5費0%");
    CHECK(text.nextWeights == "  1費40% 2費35% 3費23% 4費2% 5費0%");
    CHECK(formatChessShopWeightLine(content.balance(), 10) == "已滿級");
}

TEST_CASE("chess pool browse order preserves config order within each tier", "[chess][menu-formatting][pool]")
{
    ChessGameContentData data;
    data.poolRoleIds = {30, 11, 20, 10, 40};
    for (const auto [roleId, tier] : std::vector<std::pair<int, int>>{
             {30, 2}, {11, 1}, {20, 2}, {10, 1}, {40, 3}})
    {
        ChessRoleDefinition role;
        role.ID = roleId;
        role.Cost = tier;
        data.roles.emplace(roleId, role);
    }
    const ChessGameContent content(std::move(data));

    CHECK(buildChessPoolBrowseOrder(content) == std::vector<int>{11, 10, 30, 20, 40});
}

TEST_CASE("position swap preview follows the highlighted row", "[chess][menu-formatting][position-swap]")
{
    CHECK_FALSE(chessPositionSwapPreviewEnabled(false, -1));
    CHECK(chessPositionSwapPreviewEnabled(true, -1));
    CHECK_FALSE(chessPositionSwapPreviewEnabled(true, 0));
    CHECK(chessPositionSwapPreviewEnabled(false, 1));
}

TEST_CASE("ban management rows retain banned roles and legacy status", "[chess][menu-formatting][ban]")
{
    ChessGameContentData data;
    for (const auto [roleId, tier] : std::vector<std::pair<int, int>>{{30, 3}, {11, 2}, {20, 1}, {10, 2}})
    {
        ChessRoleDefinition role;
        role.ID = roleId;
        role.Cost = tier;
        data.roles.emplace(roleId, role);
    }
    const ChessGameContent content(std::move(data));
    ChessGameplayObservation observation;
    observation.seenRoles = {30, 11, 20, 10};
    observation.bans = {11, 30};

    const auto rows = buildChessBanManagementRows(observation, content);

    REQUIRE(rows.size() == 4);
    CHECK(rows[0].roleId == 20);
    CHECK_FALSE(rows[0].banned);
    CHECK(rows[1].roleId == 10);
    CHECK_FALSE(rows[1].banned);
    CHECK(rows[2].roleId == 11);
    CHECK(rows[2].banned);
    CHECK(rows[3].roleId == 30);
    CHECK(rows[3].banned);

    const auto available = chessBanManagementRowOutlineStyle(false);
    CHECK(available.color.a == 0);
    CHECK_FALSE(available.animated);
    CHECK(available.thickness == 1);

    const auto banned = chessBanManagementRowOutlineStyle(true);
    CHECK(banned.color.r == 255);
    CHECK(banned.color.g == 80);
    CHECK(banned.color.b == 80);
    CHECK(banned.color.a == 255);
    CHECK_FALSE(banned.animated);
    CHECK(banned.thickness == 2);

    observation.bans = observation.seenRoles;
    const auto fullRows = buildChessBanManagementRows(observation, content);
    REQUIRE(fullRows.size() == observation.seenRoles.size());
    CHECK(std::ranges::all_of(fullRows, &ChessBanManagementRow::banned));
}

TEST_CASE("challenge rewards retain configured limits and specific equipment names", "[chess][menu-formatting][challenge]")
{
    ChessGameContentData data;
    ChessItemDefinition item;
    item.id = 99;
    item.name = "倚天劍";
    data.items.emplace(item.id, item);
    ChessItemDefinition armor;
    armor.id = 100;
    armor.name = "金絲甲";
    data.items.emplace(armor.id, armor);
    ChessRoleDefinition enemyRole;
    enemyRole.ID = 7;
    enemyRole.Name = "金輪法王";
    enemyRole.Cost = 4;
    data.roles.emplace(enemyRole.ID, enemyRole);
    const ChessGameContent content(std::move(data));
    using Type = BalanceConfig::ChallengeRewardType;

    CHECK(chessChallengeRewardDescription(content, {Type::Gold, 23}) == "獲取23金幣");
    CHECK(chessChallengeRewardDescription(content, {Type::GetPiece, 4}) == "獲取棋子(最高4費)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetNeigong, 3}) == "獲取內功(最高3階)");
    CHECK(chessChallengeRewardDescription(content, {Type::StarUp1to2, 2}) == "升星★→★★(最高2費)");
    CHECK(chessChallengeRewardDescription(content, {Type::StarUp2to3, 5}) == "升星★★→★★★(最高5費)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetEquipment, 4}) == "獲取裝備(最高4階)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetSpecificEquipment, 99}) == "獲取指定裝備: 倚天劍");

    CHECK(std::string(chessChallengeCompletionLabel(false)).empty());
    CHECK(std::string(chessChallengeCompletionLabel(true)) == "[已通關]");
    const auto incompleteColor = chessChallengeMenuRowColor(false);
    CHECK(incompleteColor.r == 255);
    CHECK(incompleteColor.g == 200);
    CHECK(incompleteColor.b == 100);
    CHECK(incompleteColor.a == 255);
    const auto completedColor = chessChallengeMenuRowColor(true);
    CHECK(completedColor.r == 120);
    CHECK(completedColor.g == 120);
    CHECK(completedColor.b == 120);
    CHECK(completedColor.a == 255);
    CHECK(chessChallengeEnemyDescription(content, {7, 2}) == "  金輪法王 ★★ (4費)");
    CHECK(chessChallengeEnemyDescription(content, {7, 2, 99, 100})
        == "  金輪法王 ★★ (4費)　倚天劍／金絲甲");
}

TEST_CASE("challenge browsing is driven by configured rows rather than current legality", "[chess][menu-formatting][challenge]")
{
    ChessGameContentData data;
    data.balance.challenges.resize(3);
    data.balance.challenges[0].name = "第一場";
    data.balance.challenges[1].name = "第二場";
    data.balance.challenges[2].name = "第三場";
    const auto content = std::make_shared<const ChessGameContent>(std::move(data));
    const ChessGameSession session(content, 1);

    CHECK(std::ranges::none_of(session.legalActions(), [](const ChessLegalActionDescriptor& action) {
        return action.type == ChessActionType::StartChallenge;
    }));
    CHECK((chessChallengeBrowseNames(*content) == std::vector<std::string>{"第一場", "第二場", "第三場"}));
}

TEST_CASE("star reward title derives one shared source and target pair from session state", "[chess][menu-formatting][reward]")
{
    ChessSessionState state;
    state.roster.emplace(41, ChessSessionPiece{41, 7, 2});
    state.roster.emplace(42, ChessSessionPiece{42, 8, 2});

    ChessPendingReward pending;
    pending.kind = ChessRewardKind::StarUpgrade;
    pending.options = {
        {"star:41", ChessRewardKind::StarUpgrade, 41, 3},
        {"star:42", ChessRewardKind::StarUpgrade, 42, 3},
    };

    CHECK(chessStarUpgradeRewardTitle(state, pending) == "選擇升星 2★→3★");
}

TEST_CASE("reward option cost column marks only paid rerolled choices", "[chess][menu-formatting][reward]")
{
    ChessRewardOption original{"equipment:1", ChessRewardKind::Equipment, 1};
    ChessRewardOption rerolled{"equipment:2", ChessRewardKind::Equipment, 2};
    rerolled.goldCost = 4;

    CHECK(chessRewardOptionCostColumn(original).empty());
    CHECK(chessRewardOptionCostColumn(rerolled) == "+$4");
}

TEST_CASE("victory combo bonus uses the configured source combo name", "[chess][menu-formatting][combo][config]")
{
    ChessGameContentData data;
    ComboDef combo;
    combo.id = 17;
    combo.name = "配置經濟羈絆";
    data.combos.push_back(std::move(combo));
    const ChessGameContent content(std::move(data));
    ChessSemanticEvent gold{ChessSemanticEventType::GoldAwarded, 3, 2, 11, "combo:17"};

    CHECK(chessVictoryComboBonusText(content, gold, 6) == "(配置經濟羈絆+$6)");

    gold.stableId.clear();
    CHECK(chessVictoryComboBonusText(content, gold, 6) == "(羈絆+$6)");
}

TEST_CASE("piece and star rewards provide full role preview inputs", "[chess][menu-formatting][reward]")
{
    ChessSessionState state;
    state.roster.emplace(42, ChessSessionPiece{42, 9, 2});

    const auto piece = chessRewardRolePreview(
        state,
        {"piece:7", ChessRewardKind::Piece, 7});
    REQUIRE(piece);
    const ChessRewardRolePreview expectedPiece{7, 1, -1};
    CHECK(*piece == expectedPiece);

    const auto upgrade = chessRewardRolePreview(
        state,
        {"star:42", ChessRewardKind::StarUpgrade, 42, 3});
    REQUIRE(upgrade);
    const ChessRewardRolePreview expectedUpgrade{9, 3, 42};
    CHECK(*upgrade == expectedUpgrade);

    CHECK_FALSE(chessRewardRolePreview(
        state,
        {"equipment:99", ChessRewardKind::Equipment, 99}));
}

TEST_CASE("management role preview starts with zero current MP", "[chess][menu-formatting][role-preview]")
{
    CHECK(formatChessRolePreviewMp(999) == "0/999");
}

TEST_CASE("synergy group gaps do not change wrapped effect line spacing",
          "[chess][menu-formatting][panel-text]")
{
    const std::vector<std::vector<PanelTextSourceRow>> blocks{
        {{.text = "紅顏", .spacingAfter = 2},
         {.text = "受擊時有30%機率，使攻擊者的出招冷卻延長50%", .spacingAfter = 2}},
        {{.text = "明教", .spacingAfter = 2},
         {.text = "自身死亡時，其他羈絆成員回復各自最大生命的8%，最低1點", .spacingAfter = 2}},
    };
    const auto normal = fitPanelTextBlocks(blocks, 200, 1000, 18, 18);
    const auto spaced = fitPanelTextBlocks(blocks, 200, 1000, 18, 18, 8);
    REQUIRE(normal);
    REQUIRE(spaced);
    REQUIRE(spaced->columns.size() == 1);
    const auto& before = normal->columns.front().layout;
    const auto& after = spaced->columns.front().layout;
    REQUIRE(after.lines.size() == before.lines.size());
    CHECK(after.height == before.height + 8);
    int wrappedContinuations{};
    for (std::size_t i = 0; i < after.lines.size(); ++i)
    {
        CHECK(after.lines[i].text == before.lines[i].text);
        CHECK(after.lines[i].y == before.lines[i].y + (after.lines[i].sourceRow >= 2 ? 8 : 0));
        if (i > 0 && after.lines[i].sourceRow == after.lines[i - 1].sourceRow)
        {
            ++wrappedContinuations;
            CHECK(after.lines[i].y - after.lines[i - 1].y == 20);
        }
    }
    CHECK(wrappedContinuations >= 2);
    const auto columns = fitPanelTextBlocks(blocks, 400, 100, 18, 18, 8);
    REQUIRE(columns);
    REQUIRE(columns->columns.size() == 2);
    for (const auto& column : columns->columns)
    {
        CHECK(column.layout.lines.front().y == 0);
        for (std::size_t i = 1; i < column.layout.lines.size(); ++i)
        {
            const auto& line = column.layout.lines[i];
            const auto& previous = column.layout.lines[i - 1];
            if (line.sourceRow == previous.sourceRow)
                CHECK(line.y - previous.y == 20);
        }
    }
}
