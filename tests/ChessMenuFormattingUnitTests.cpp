#include "ChessMenuFormatting.h"
#include "ChessGameGuide.h"
#include "ChessGameSession.h"
#include "ChessTalentUi.h"
#include "ChessUiCommon.h"

#include <catch2/catch_test_macros.hpp>

using namespace KysChess;

namespace
{

int displayColumnBefore(const std::string& text, const std::string& marker)
{
    return displayTextWidth(text.substr(0, text.find(marker)));
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
    backbone.strengtheningChargesPerExtraStar = 2;
    backbone.strengtheningDamagePercent = 37;
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
    REQUIRE(gamblerPresentation.facts.size() == 7);
    CHECK(fact(gamblerPresentation, "開局", "額外禁棋").value == "8枚（1～4費）");
    CHECK(fact(gamblerPresentation, "賭運", "取得方式").value
        == "第1～19關付費刷新商店時累積；免費刷新及之後的刷新不增加");
    CHECK(fact(gamblerPresentation, "賭運", "觸發機率").value == "每層 +11%，最多7層（77%）");
    CHECK(fact(gamblerPresentation, "致命傷害", "成功效果").value == "保留2生命，無敵121幀");
    CHECK_FALSE(chessTalentPresentationUsesEquipmentTable(gamblerPresentation));

    const auto backbonePresentation = buildChessTalentPresentation(balance, ChessTalentId::Backbone);
    REQUIRE(backbonePresentation.facts.size() == 6);
    CHECK(fact(backbonePresentation, "開場內力", "適用對象").value
        == "4費棋子；只計算非4費友軍的額外星級");
    CHECK(fact(backbonePresentation, "開場內力", "計算方式").value
        == "非4費友軍每多1星 +12，最多計6星，最高 +72");
    CHECK(fact(backbonePresentation, "攻防強化", "開場次數").value
        == "同開場內力的星級計算；每多1星 +2次，最多12次");
    CHECK(fact(backbonePresentation, "攻防強化", "共用消耗").value
        == "造成直接傷害 +37%或受到傷害 -37%，每次消耗1次；攻防共用");
    CHECK(fact(backbonePresentation, "定向增援", "觸發時機").value
        == "4費棋子由1星升至2星時，商店保證2枚同名棋子");
    CHECK(fact(backbonePresentation, "定向增援", "升至3星").value
        == "升至3星後取消尚未使用的保證棋子");
    CHECK_FALSE(chessTalentPresentationUsesEquipmentTable(backbonePresentation));
    CHECK(chessTalentDescription(balance, ChessTalentId::Backbone).contains("每多1星 +12"));
    balance.talents.emplace(ChessTalentId::Alliance, ChessTalentDefinition{
        .description = "合縱設定說明", .selectedComboBonus = 1,
        .reducedThresholdMinimum = 7, .thresholdReduction = 2});
    const auto alliance = buildChessTalentPresentation(balance, ChessTalentId::Alliance);
    CHECK(alliance.description == "合縱設定說明");
    CHECK(fact(alliance, "開局結盟", "選擇羈絆").value.contains("+1人"));
    CHECK(fact(alliance, "高階羈絆", "降低門檻").value.contains("達7人"));
    CHECK(fact(alliance, "高階羈絆", "降低門檻").value.contains("降低2人"));
    CHECK_FALSE(chessTalentPresentationUsesEquipmentTable(alliance));
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
    CHECK(displayTextWidth(descriptions[0]) <= unindentedUnits);
    CHECK(displayTextWidth(descriptions[0]) > indentedUnits);
    CHECK(displayTextWidth(descriptions[1]) <= indentedUnits);
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

    REQUIRE(sections.size() == 8);
    const auto contains = [&](const std::string& title, const std::string& expected) {
        return std::ranges::any_of(guideSection(sections, title).lines,
            [&](const auto& line) { return line.text.contains(expected); });
    };
    CHECK(contains("基本流程", "共 17 關，每 3 關"));
    CHECK(contains("基本流程", "6 經驗；強敵勝利獲得 11 經驗"));
    CHECK(contains("棋子與升星", "1費 2金、2費 4金、3費 7金、4費 11金、5費 16金"));
    CHECK(contains("棋子與升星", "每增加一星乘 4"));
    CHECK(contains("棋子與升星", "生命 +12、攻擊 +1、防禦 +1、兵器熟練度 +0.5、輕功 +0.25"));
    CHECK(contains("金幣與等級", "開局 23 金；刷新商店 5 金；購買經驗 7 金換 9 點"));
    CHECK(contains("金幣與等級", "15%，小數捨去，上限 6 金"));
    CHECK(contains("金幣與等級", "最高 9 級；出戰容量隨等級增加，最多 9 人；候補可放 12 人"));
    CHECK(contains("裝備與內功", "共 2 次，最早第 4 關，最高 4 階"));
    CHECK(contains("裝備與內功", "通過第 13 關後"));
    CHECK(contains("裝備與內功", "每件 37 金"));
    CHECK(contains("裝備與內功", "最多 5 本免費候選中選一本"));
    CHECK(contains("裝備與內功", "額外候選需 6 金"));
    CHECK(contains("遠征與對戰", "遠征共 3 關"));
}

TEST_CASE("game guide follows selected talent and disabled rule branches", "[chess][menu-formatting][guide]")
{
    ChessGameContentData data;
    data.difficulty = Difficulty::Hard;
    data.balance.bossInterval = 0;
    data.balance.interestPercent = 0;
    data.balance.banBaseCount = 2;
    data.balance.banCountPerLevel = 1;
    data.balance.banUnlocks = {{8, 1, 3}, {3, 2, 2}};
    data.balance.talents[ChessTalentId::Gambler].openingBans = 4;
    data.balance.talentEquipmentRewards[ChessTalentId::Gambler] = {{5, 3, 1, 0}};
    const ChessGameContent content(std::move(data));
    const auto sections = buildChessGameGuideSections(content, ChessTalentId::Gambler);
    const auto contains = [&](const std::string& title, const std::string& expected) {
        return std::ranges::any_of(guideSection(sections, title).lines,
            [&](const auto& line) { return line.text.contains(expected); });
    };
    REQUIRE(sections.size() == 8);
    CHECK(contains("基本流程", "不設固定強敵戰"));
    CHECK(contains("金幣與等級", "不計存款利息"));
    CHECK(contains("羈絆與禁棋", "開局 2 個名額，每升級增加 1 個"));
    CHECK(contains("羈絆與禁棋", "關卡獎勵共 2 段；本天賦開局另提供 4 個"));
    CHECK(contains("裝備與內功", "共 1 次，最早第 5 關，最高 3 階"));
    CHECK(contains("裝備與內功", "此天賦沒有神兵商店"));
    CHECK(contains("裝備與內功", "沒有固定強敵內功獎勵"));
    CHECK(contains("遠征與對戰", "本局沒有遠征挑戰"));
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


    observation.bans = observation.seenRoles;
    const auto fullRows = buildChessBanManagementRows(observation, content);
    REQUIRE(fullRows.size() == observation.seenRoles.size());
    CHECK(std::ranges::all_of(fullRows, &ChessBanManagementRow::banned));

    observation.seenRoles = {20};
    observation.bans = {11}; // 開局強制封棋未必曾在商店出現。
    const auto openingBans = buildChessBanManagementRows(observation, content);
    REQUIRE(openingBans.size() == 2);
    CHECK(openingBans[0].roleId == 20);
    CHECK_FALSE(openingBans[0].banned);
    CHECK(openingBans[1].roleId == 11);
    CHECK(openingBans[1].banned);
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
    const ChessGameContent content(std::move(data));
    using Type = BalanceConfig::ChallengeRewardType;

    CHECK(chessChallengeRewardDescription(content, {Type::Gold, 23}) == "獲取23金幣");
    CHECK(chessChallengeRewardDescription(content, {Type::GetPiece, 4}) == "獲取棋子(最高4費)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetNeigong, 3}) == "獲取內功(最高3階)");
    CHECK(chessChallengeRewardDescription(content, {Type::StarUp1to2, 2}) == "升星★→★★(最高2費)");
    CHECK(chessChallengeRewardDescription(content, {Type::StarUp2to3, 5}) == "升星★★→★★★(最高5費)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetEquipment, 4}) == "獲取裝備(最高4階)");
    CHECK(chessChallengeRewardDescription(content, {Type::GetSpecificEquipment, 99}) == "獲取指定裝備: 倚天劍");
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
