#include "ChessCatalogQueries.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessUiCommon.h"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <iterator>
#include <ranges>

using namespace KysChess;

namespace
{

EffectRule attributeRule(BattleAttribute attribute, int amount)
{
    EffectRule rule;
    rule.event = EffectEvent::BattleInitialized;
    ModifyAttributeAction action;
    action.attribute = attribute;
    action.amount.flat = amount;
    action.operation = AttributeOperation::FlatAdd;
    rule.actions.push_back({EffectActionValue{action}});
    return rule;
}

ChessGameContent catalogContent()
{
    ChessGameContentData data;
    data.balance.starHPMult = 2.0;
    data.balance.starAtkMult = 1.5;
    data.balance.starDefMult = 1.25;
    data.balance.starMartialMult = 1.0;
    data.balance.starSpdMult = 1.0;
    data.balance.fightWinGrowthHP = 10;
    data.balance.fightWinGrowthAtk = 2;
    data.balance.fightWinGrowthDef = 3;
    data.balance.fightWinGrowthWeapon = 4;
    data.balance.fightWinGrowthSpeed = 5;

    ChessRoleDefinition role;
    role.ID = 10;
    role.Name = "共用查詢棋子";
    role.Cost = 2;
    role.MaxHP = 100;
    role.MaxMP = 80;
    role.Attack = 20;
    role.Defence = 12;
    role.Speed = 9;
    role.Fist = 7;
    role.MagicID[0] = 100;
    role.MagicPower[0] = 30;
    role.MagicID[1] = 100;
    role.MagicPower[1] = 45;
    role.MagicID[2] = 100;
    role.MagicPower[2] = 60;
    data.roles.emplace(role.ID, role);

    ChessMagicDefinition magic;
    magic.ID = 100;
    magic.Name = "共用掌法";
    magic.NeedMP = 12;
    magic.AttackAreaType = 3;
    magic.SelectDistance = 4;
    magic.AttackDistance = 2;
    data.magics.emplace(magic.ID, magic);

    ChessMagicEffectDefinition magicEffects;
    magicEffects.magicId = magic.ID;
    magicEffects.name = magic.Name;
    magicEffects.rules = {
        attributeRule(BattleAttribute::Attack, 4),
        attributeRule(BattleAttribute::Speed, 5),
    };
    data.magicEffects.push_back(std::move(magicEffects));

    ChessItemDefinition item;
    item.id = 500;
    item.equipType = 0;
    item.name = "共用寶劍";
    item.addMaxHP = 25;
    item.addAttack = 8;
    item.addSword = 6;
    data.items.emplace(item.id, item);
    data.equipment.push_back({
        item.id,
        2,
        0,
        {attributeRule(BattleAttribute::Defence, 7)},
        {CountsAsComboRule{"共用羈絆"}},
    });
    data.equipmentSynergies.push_back({
        {role.ID},
        item.id,
        {attributeRule(BattleAttribute::Speed, 5)},
        {CountsAsComboRule{"角色羈絆"}},
    });

    ComboDef combo;
    combo.id = 3;
    combo.name = "共用羈絆";
    combo.memberRoleIds = {role.ID};
    combo.thresholds.push_back({
        1,
        "啟動",
        {attributeRule(BattleAttribute::Attack, 10)},
    });
    data.combos.push_back(std::move(combo));

    NeigongDef neigong;
    neigong.magicId = 700;
    neigong.itemId = 701;
    neigong.tier = 3;
    neigong.name = "共用內功";
    neigong.rules.push_back(attributeRule(BattleAttribute::MaxHp, 30));
    data.neigong.push_back(std::move(neigong));

    ChessBattleMapDefinition map;
    map.id = 900;
    map.name = "共用戰場";
    data.battleMaps.emplace(map.id, std::move(map));

    BalanceConfig::ChallengeDef challenge;
    challenge.name = "共用遠征";
    challenge.description = "共用描述";
    challenge.enemies.push_back({role.ID, 2, item.id});
    challenge.rewards.push_back({BalanceConfig::ChallengeRewardType::Gold, 9});
    data.balance.challenges.push_back(std::move(challenge));
    return ChessGameContent(std::move(data));
}

}  // namespace

TEST_CASE("catalog queries keep map identity and stat scopes authoritative", "[chess][catalog][queries]")
{
    const auto content = catalogContent();
    CHECK(chessBattleMapDisplayName(content, 900) == "共用戰場");
    CHECK(chessBattleMapDisplayName(content, 901) == "戰場 901");

    PreparedChessBattleUnit unit;
    unit.roleId = 10;
    unit.star = 2;
    unit.fightsWon = 3;
    unit.weaponItemId = 500;
    const auto baseline = chessPreparedUnitBaselineStats(content, unit);
    CHECK(baseline.maxHp == 555);
    CHECK(baseline.attack == 79);
    CHECK(baseline.defence == 46);
    CHECK(baseline.speed == 33);
    CHECK(baseline.sword == 33);
}

TEST_CASE("catalog role and equipment metadata preserve normalized semantics", "[chess][catalog][metadata]")
{
    const auto content = catalogContent();
    const auto role = chessRoleMetadata(content, 10);
    REQUIRE(role.abilities.size() == 1);
    CHECK(role.abilities.front().powerByStar == std::vector<ChessAbilityStarPower>{{1, 45}, {2, 60}});
    CHECK(role.abilities.front().geometry == "範圍；可選擇距離 4 格內的中心，影響中心周圍方形半徑 2 格");
    CHECK(role.abilities.front().effectNote.empty());
    CHECK(joinEffectDescriptionRows(role.abilities.front().effects).contains("攻擊+4"));
    CHECK(joinEffectDescriptionRows(role.abilities.front().effects).contains("速度+5"));
    CHECK(role.combos == std::vector<std::string>{"共用羈絆"});

    const auto equipment = chessEquipmentMetadata(content, 500);
    CHECK(equipment.baseStatEffects == std::vector<std::string>{"生命+25", "攻擊+8", "御劍+6"});
    CHECK(effectDescriptionTextRows(equipment.specialEffects)
        == std::vector<std::string>{"防禦+7"});
    REQUIRE(equipment.specialEffects.sections.size() == 1);
    REQUIRE(equipment.specialEffects.sections[0].blocks.size() == 1);
    REQUIRE(equipment.specialEffects.sections[0].blocks[0].rows.size() == 1);
    const auto& specialEffectRow = equipment.specialEffects.sections[0].blocks[0].rows[0];
    CHECK(specialEffectRow.kind == EffectDescriptionRowKind::Prose);
    CHECK(specialEffectRow.text == "防禦+7");
    CHECK(specialEffectRow.indent == 0);
    CHECK(specialEffectRow.breakBefore == EffectDescriptionSemanticBreak::Block);
    CHECK(equipment.countsAsCombos == std::vector<std::string>{"共用羈絆"});
    REQUIRE(equipment.characterBonuses.size() == 1);
    CHECK(equipment.characterBonuses.front().roles == std::vector<std::string>{"共用查詢棋子"});
    CHECK(effectDescriptionTextRows(equipment.characterBonuses.front().effects)
        == std::vector<std::string>{"速度+5"});
    CHECK(equipment.characterBonuses.front().countsAsCombos == std::vector<std::string>{"角色羈絆"});
}

TEST_CASE("catalog combo and challenge metadata retain provenance and ordering", "[chess][catalog][metadata]")
{
    const auto content = catalogContent();
    ResolvedChessComboContribution contribution;
    contribution.roleId = 10;
    contribution.unitIds = {41};
    contribution.countedStar = 2;
    contribution.physicalPoints = 1;
    contribution.starBonusPoints = 1;
    contribution.naturalMember = true;
    contribution.equipmentItemIds = {500};
    const auto combo = chessComboMetadata(
        content,
        content.combos().front(),
        1,
        2,
        0,
        -1,
        {contribution});
    REQUIRE(combo.contributions.size() == 1);
    CHECK(combo.contributions.front().equipmentSources.front().name == "共用寶劍");
    CHECK(combo.contributions.front().explanation.contains("不重複加點"));
    CHECK(combo.contributions.front().explanation.contains("2★另提供 1 點"));

    const auto challenge = chessChallengeMetadata(content, content.balance().challenges.front());
    CHECK(challenge.name == "共用遠征");
    CHECK(challenge.summaryDescription.contains("1 名敵人"));
    REQUIRE(challenge.enemies.size() == 1);
    REQUIRE(challenge.enemies.front().weapon);
    CHECK(challenge.enemies.front().weapon->name == "共用寶劍");
    CHECK(challenge.rewards == std::vector<std::string>{"獲取9金幣"});
}

TEST_CASE("effect catalog covers every configured runtime rule by source",
          "[chess][catalog][effects]")
{
    const auto content = catalogContent();
    const auto catalog = chessEffectCatalog(content);

    REQUIRE(catalog.size() == 6);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::Magic,
              &ChessEffectCatalogEntry::source)
        == 2);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::Equipment,
              &ChessEffectCatalogEntry::source)
        == 1);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::EquipmentSynergy,
              &ChessEffectCatalogEntry::source)
        == 1);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::Neigong,
              &ChessEffectCatalogEntry::source)
        == 1);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::ComboThreshold,
              &ChessEffectCatalogEntry::source)
        == 1);

    CHECK(catalog[0].sourceName == "共用掌法");
    CHECK(catalog[0].ruleOrdinal == 1);
    CHECK(catalog[0].sourceRuleCount == 2);
    CHECK(catalog[1].ruleOrdinal == 2);
    CHECK(joinEffectDescriptionRows(catalog[0].effects).contains("攻擊+4"));

    const auto synergy = std::ranges::find(
        catalog,
        ChessEffectCatalogSource::EquipmentSynergy,
        &ChessEffectCatalogEntry::source);
    REQUIRE(synergy != catalog.end());
    CHECK(synergy->sourceName == "共用寶劍 · 共用查詢棋子");
    CHECK(synergy->sourceContext.contains("共用查詢棋子專屬"));
    CHECK(effectDescriptionTextRows(synergy->effects)
        == std::vector<std::string>{"速度+5"});

    const auto combo = std::ranges::find(
        catalog,
        ChessEffectCatalogSource::ComboThreshold,
        &ChessEffectCatalogEntry::source);
    REQUIRE(combo != catalog.end());
    CHECK(combo->sourceName == "共用羈絆 · 啟動");
    CHECK(combo->sourceContext.contains("1人門檻"));
    CHECK(chessEffectCatalogSourceLabel(combo->source) == std::string_view{"羈絆"});
}

TEST_CASE("formal effect catalog renders every configured rule at readable panel size",
          "[chess][catalog][effects][content]")
{
    const auto content = Test::actualContent(Difficulty::Normal);
    REQUIRE(content);

    std::size_t configuredRuleCount{};
    for (const auto& definition : content->magicEffects())
        configuredRuleCount += definition.rules.size();
    for (const auto& definition : content->equipment())
        configuredRuleCount += definition.rules.size();
    for (const auto& definition : content->equipmentSynergies())
        configuredRuleCount += definition.rules.size();
    for (const auto& definition : content->neigong())
        configuredRuleCount += definition.rules.size();
    for (const auto& combo : content->combos())
        for (const auto& threshold : combo.thresholds)
            configuredRuleCount += threshold.rules.size();

    const auto catalog = chessEffectCatalog(*content);
    REQUIRE(catalog.size() == configuredRuleCount);
    REQUIRE_FALSE(catalog.empty());
    CHECK(std::ranges::is_sorted(catalog, {}, &ChessEffectCatalogEntry::source));

    constexpr int minimumDetailWidth = 540;
    constexpr int detailHeight = 590;
    constexpr int minimumFontSize = 14;
    for (const auto& entry : catalog)
    {
        CAPTURE(entry.sourceName, entry.sourceContext,
            entry.ruleOrdinal, entry.sourceRuleCount);
        CHECK(entry.ruleOrdinal >= 1);
        CHECK(entry.ruleOrdinal <= entry.sourceRuleCount);
        REQUIRE_FALSE(entry.effects.sections.empty());

        std::vector<PanelTextSourceRow> rows{
            { .text = entry.sourceName, .fontSizeDelta = 5, .spacingAfter = 4 },
            { .text = std::format("{}效果 · 配置 {}/{}",
                  chessEffectCatalogSourceLabel(entry.source),
                  entry.ruleOrdinal,
                  entry.sourceRuleCount),
              .fontSizeDelta = 1,
              .spacingAfter = 3 },
            { .text = entry.sourceContext,
              .fontSizeDelta = -2,
              .spacingAfter = 12 },
        };
        auto effectRows = panelTextRowsForEffectDescription(entry.effects);
        rows.insert(rows.end(),
            std::make_move_iterator(effectRows.begin()),
            std::make_move_iterator(effectRows.end()));
        const auto layout = layoutPanelText(
            rows,
            minimumDetailWidth,
            minimumFontSize);
        CHECK(layout.height <= detailHeight);
    }
}
