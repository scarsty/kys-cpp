#include "ChessCatalogQueries.h"
#include "ChessCharacterPresentation.h"
#include "ChessTalentUi.h"
#include "ChessGameSessionTestHelpers.h"
#include "ChessUiCommon.h"

#include <catch2/catch_test_macros.hpp>

#include <format>
#include <iterator>
#include <ranges>

using namespace KysChess;

namespace
{

GameplayEffect attributeDefinition(BattleAttribute attribute, int amount)
{
    const auto name = attribute == BattleAttribute::Attack ? "攻擊加成"
        : attribute == BattleAttribute::Defence ? "防禦加成"
        : attribute == BattleAttribute::Speed ? "速度加成" : "生命上限加成";
    const auto catalog = gameplayEffectCatalog();
    const auto found = std::ranges::find(catalog, name, &GameplayEffectRegistration::name);
    REQUIRE(found != catalog.end());
    const std::array values{amount};
    return found->create(values);
}

EffectRule attributeRule(BattleAttribute attribute, int amount)
{
    auto rule = attributeDefinition(attribute, amount)->buildRules().front();
    rule.id = EffectRuleId{static_cast<std::uint64_t>(attribute) + 1};
    return rule;
}

ChessGameContent catalogContent(int attackAreaType = 3, bool twoAbilities = false)
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
    data.balance.talents.emplace(ChessTalentId::Gambler,
        ChessTalentDefinition{.luckChancePerStack = 15, .luckStackCap = 8});

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
    role.MagicID[0] = twoAbilities ? 101 : 100;
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
    magic.AttackAreaType = attackAreaType;
    magic.SelectDistance = 4;
    magic.AttackDistance = 2;
    data.magics.emplace(magic.ID, magic);
    if (twoAbilities)
    {
        auto normal = magic;
        normal.ID = 101;
        data.magics.emplace(normal.ID, normal);
    }

    ChessMagicEffectDefinition magicEffects;
    magicEffects.magicId = magic.ID;
    magicEffects.name = magic.Name;
    magicEffects.rules = {
        attributeRule(BattleAttribute::Attack, 4),
        attributeRule(BattleAttribute::Speed, 5),
    };
    magicEffects.effects = {
        attributeDefinition(BattleAttribute::Attack, 4),
        attributeDefinition(BattleAttribute::Speed, 5),
    };
    if (twoAbilities)
    {
        auto normal = magicEffects;
        normal.magicId = 101;
        data.magicEffects.push_back(std::move(normal));
    }
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
        {attributeDefinition(BattleAttribute::Defence, 7)},
    });
    data.equipmentSynergies.push_back({
        {role.ID},
        item.id,
        {attributeRule(BattleAttribute::Speed, 5)},
        {CountsAsComboRule{"角色羈絆"}},
        {attributeDefinition(BattleAttribute::Speed, 5)},
    });

    ComboDef combo;
    combo.id = 3;
    combo.iconId = 13;
    combo.name = "共用羈絆";
    combo.memberRoleIds = {role.ID};
    combo.thresholds.push_back({
        1,
        "啟動",
        {attributeRule(BattleAttribute::Attack, 10)},
        {},
        {attributeDefinition(BattleAttribute::Attack, 10)},
    });
    data.combos.push_back(std::move(combo));

    ComboDef hiddenCombo;
    hiddenCombo.id = 4;
    hiddenCombo.iconId = 14;
    hiddenCombo.name = "隱藏羈絆";
    hiddenCombo.isNpcOnly = true;
    hiddenCombo.memberRoleIds = {role.ID};
    hiddenCombo.thresholds.push_back({1, "隱藏門檻", {}, {}, {attributeDefinition(BattleAttribute::Attack, 99)}});
    data.combos.push_back(std::move(hiddenCombo));

    NeigongDef neigong;
    neigong.magicId = 700;
    neigong.itemId = 701;
    neigong.tier = 3;
    neigong.name = "共用內功";
    neigong.rules.push_back(attributeRule(BattleAttribute::MaxHp, 30));
    neigong.effects.push_back(attributeDefinition(BattleAttribute::MaxHp, 30));
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
    CHECK(baseline.maxHp == 530);
    CHECK(baseline.attack == 71);
    CHECK(baseline.defence == 46);
    CHECK(baseline.speed == 33);
    CHECK(baseline.sword == 27);
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
    CHECK(role.combos == std::vector<std::string>{"共用羈絆", "隱藏羈絆"});

    const auto equipment = chessEquipmentMetadata(content, 500);
    CHECK(effectDescriptionTextRows(equipment.specialEffects)
        == std::vector<std::string>{"防禦+7。"});
    REQUIRE(equipment.specialEffects.sections.size() == 1);
    REQUIRE(equipment.specialEffects.sections[0].blocks.size() == 1);
    REQUIRE(equipment.specialEffects.sections[0].blocks[0].rows.size() == 1);
    const auto& specialEffectRow = equipment.specialEffects.sections[0].blocks[0].rows[0];
    CHECK(specialEffectRow.kind == EffectDescriptionRowKind::Prose);
    CHECK(specialEffectRow.text == "防禦+7。");
    CHECK(specialEffectRow.indent == 0);
    CHECK(specialEffectRow.breakBefore == EffectDescriptionSemanticBreak::None);
    CHECK(equipment.countsAsCombos == std::vector<std::string>{"共用羈絆"});
    REQUIRE(equipment.characterBonuses.size() == 1);
    CHECK(equipment.characterBonuses.front().roleIds == std::vector<int>{10});
    CHECK(equipment.characterBonuses.front().roles == std::vector<std::string>{"共用查詢棋子"});
    CHECK(effectDescriptionTextRows(equipment.characterBonuses.front().effects)
        == std::vector<std::string>{"速度+5。"});
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

TEST_CASE("equipment panel excludes SQLite bonuses and retains compact configured effects",
          "[chess][catalog][effects][panel-text]")
{
    const auto content = catalogContent();
    const auto rows = panelTextRowsForEquipment(chessEquipmentMetadata(content, 500, EffectDescriptionStyle::Compact));
    std::string text;
    for (const auto& row : rows) text += row.text + "\n";
    CHECK_FALSE(text.contains("基礎屬性"));
    CHECK_FALSE(text.contains("生命+25"));
    CHECK_FALSE(text.contains("攻擊+8"));
    CHECK_FALSE(text.contains("御劍+6"));
    CHECK(text.contains("+7防"));
    CHECK(text.contains("計作共用羈絆"));
    CHECK(text.contains("共用查詢棋子專屬"));
    CHECK(text.contains("計作角色羈絆"));
    CHECK(text.contains("速度+5"));
}

TEST_CASE("equipment hover uses configured effects and marks recipient-only bonuses after wrapping",
          "[chess][catalog][effects][panel-text]")
{
    const auto content = catalogContent();
    const auto metadata = chessEquipmentMetadata(content, 500);
    for (const auto recipient : {std::optional<int>{}, std::optional{10}, std::optional{20}})
    {
        const auto rows = panelTextRowsForEquipment(metadata, recipient);
        REQUIRE(rows.size() == 6);
        CHECK(rows[0].tone == PanelTextTone::Heading);
        CHECK(rows[1].text == "防禦+7。");
        CHECK(rows[1].tone == PanelTextTone::Effect);
        CHECK(rows[2].tone == PanelTextTone::Combo);
        const bool applicable = recipient != 20;
        CHECK(rows[3].tone == (applicable ? PanelTextTone::Exclusive : PanelTextTone::Inactive));
        CHECK(rows[4].tone == (applicable ? PanelTextTone::Combo : PanelTextTone::Inactive));
        CHECK(rows[5].tone == (applicable ? PanelTextTone::Exclusive : PanelTextTone::Inactive));
        CHECK(rows[3].text.contains("（適用）") == (recipient == 10));
        CHECK(rows[3].text.contains("（不適用）") == (recipient == 20));

        std::string text;
        for (const auto& row : rows) text += row.text;
        CHECK_FALSE(text.contains("攻擊+8"));
        CHECK_FALSE(text.contains("生命+25"));
        CHECK(text.contains("速度+5"));
        // 長專屬標題及羈絆規則換行後，仍須保留適用性與語意色彩。
        const auto layout = layoutPanelText(rows, 112, 17);
        CHECK(layout.lines.size() > rows.size());
        for (const auto& line : layout.lines) CHECK(line.tone == rows[line.sourceRow].tone);
    }
}

TEST_CASE("public effect catalog covers player rules by source and excludes NPC combos",
          "[chess][catalog][effects]")
{
    const auto content = catalogContent();
    const auto catalog = chessEffectCatalog(content);

    REQUIRE(catalog.size() == 8);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::Magic,
              &ChessEffectCatalogEntry::source)
        == 2);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::Equipment,
              &ChessEffectCatalogEntry::source)
        == 2);
    CHECK(std::ranges::count(catalog, ChessEffectCatalogSource::EquipmentSynergy,
              &ChessEffectCatalogEntry::source)
        == 2);
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
    // 圖示依來源 ID 解析，戰鬥與整備規則皆須保留相同來源。
    for (const auto& entry : catalog)
    {
        switch (entry.source)
        {
        case ChessEffectCatalogSource::Magic: CHECK(entry.sourceId == 100); break;
        case ChessEffectCatalogSource::Equipment:
        case ChessEffectCatalogSource::EquipmentSynergy: CHECK(entry.sourceId == 500); break;
        case ChessEffectCatalogSource::Neigong: CHECK(entry.sourceId == 700); break;
        case ChessEffectCatalogSource::ComboThreshold: CHECK(entry.sourceId == 3); break;
        }
    }
    CHECK(joinEffectDescriptionRows(catalog[0].effects).contains("攻擊+4"));

    const auto synergy = std::ranges::find(
        catalog,
        ChessEffectCatalogSource::EquipmentSynergy,
        &ChessEffectCatalogEntry::source);
    REQUIRE(synergy != catalog.end());
    CHECK(synergy->sourceName == "共用寶劍");
    CHECK(synergy->sourceContext.contains("共用查詢棋子專屬"));
    CHECK(effectDescriptionTextRows(synergy->effects)
        == std::vector<std::string>{"速度+5。"});

    const auto combo = std::ranges::find(
        catalog,
        ChessEffectCatalogSource::ComboThreshold,
        &ChessEffectCatalogEntry::source);
    REQUIRE(combo != catalog.end());
    CHECK(combo->sourceName == "共用羈絆 · 啟動");
    CHECK(combo->sourceContext.contains("1人門檻"));
    CHECK(chessEffectCatalogSourceLabel(combo->source) == std::string_view{"羈絆"});
    CHECK(std::ranges::any_of(catalog, [](const auto& entry) {
        return joinEffectDescriptionRows(entry.effects).contains("計作「共用羈絆」");
    }));
    CHECK(std::ranges::any_of(catalog, [](const auto& entry) {
        return joinEffectDescriptionRows(entry.effects).contains("計作「角色羈絆」");
    }));
}

TEST_CASE("character presentation preserves owned equipment growth and preview context", "[chess][ui][catalog]")
{
    const auto content = catalogContent();
    ChessSessionState state;
    ChessSessionPiece piece;
    piece.instanceId = 1;
    piece.roleId = 10;
    piece.star = 2;
    piece.fightsWon = 3;
    piece.luckStacks = 4;
    piece.weaponInstanceId = 2;
    piece.deployed = true;
    state.talent = ChessTalentId::Gambler;
    state.roster.emplace(piece.instanceId, piece);
    ChessEquipmentInstance equipment;
    equipment.instanceId = 2;
    equipment.itemId = 500;
    state.equipmentInventory.emplace(equipment.instanceId, equipment);

    const auto owned = buildChessCharacterPresentation(content, state, 10, 2, 1);
    CHECK(owned.stats == chessPieceStats(content, piece));
    CHECK(owned.stats.maxHp == 530);
    CHECK(owned.stats.attack == 71);
    CHECK(owned.context == "出戰中");
    REQUIRE(owned.growthSummary);
    CHECK(owned.growthSummary->fightsWon == 3);
    CHECK(owned.growthSummary->luckStacks == 4);
    CHECK(owned.growthSummary->luckChance == 60);
    CHECK(owned.growthSummary->showLuck);
    CHECK(owned.growthSummary->stats.maxHp == 30);
    CHECK(owned.growthSummary->stats.attack == 6);
    CHECK(owned.equipment[0].title == "共用寶劍");
    CHECK(owned.equipment[0].imageId == 500);
    CHECK(owned.equipment[1].value.empty());
    CHECK(owned.equipment[1].title == "防具");
    REQUIRE(owned.abilities.size() == 1);
    CHECK(owned.abilities.front().ultimate);
    CHECK(owned.abilities.front().value == "60");
    REQUIRE(owned.synergies.size() == 1);
    CHECK(owned.synergies.front().title == "共用羈絆");

    const auto shop = buildChessCharacterPresentation(content, state, 10, 1, -1, ChessCharacterContext::Shop);
    const auto catalogue = buildChessCharacterPresentation(content, state, 10);
    CHECK(shop.stats == catalogue.stats);
    CHECK(shop.context == "商店預覽");
    CHECK_FALSE(shop.growthSummary);
    CHECK_FALSE(catalogue.growthSummary);
    CHECK(catalogue.context == "圖鑑預覽");
    CHECK(shop.stats == chessRoleStats(*content.role(10), content.balance(), 1, 0));
    CHECK(shop.equipment[0].id == -1);
    CHECK(shop.star == 1);
    CHECK(owned.star == 2);
    state.talent = ChessTalentId::DivineArms;
    state.roster.at(1).luckStacks = 0;
    const auto ordinary = buildChessCharacterPresentation(content, state, 10, 2, 1);
    REQUIRE(ordinary.growthSummary);
    CHECK_FALSE(ordinary.growthSummary->showLuck);
    CHECK(ordinary.growthSummary->fightsWon == 3);

    auto upgradedPiece = piece;
    upgradedPiece.star = 3;
    const auto upgrade = buildChessCharacterPresentation(content, state, 10, 3, 1);
    CHECK(upgrade.context == "升星預覽");
    CHECK(upgrade.stats == chessPieceStats(content, upgradedPiece));
    CHECK(state.roster.at(1).star == 2);
}

TEST_CASE("character inspector uses chess attacks and limits effects to the ultimate", "[chess][ui][catalog]")
{
    for (const auto& [area, style] : std::array<std::pair<int, Battle::BattleOperationType>, 4>{{
        {0, Battle::BattleOperationType::Melee}, {1, Battle::BattleOperationType::RangedProjectile},
        {2, Battle::BattleOperationType::RangedProjectile}, {3, Battle::BattleOperationType::TrackingProjectile}}})
    {
        const auto content = catalogContent(area, true);
        const auto character = buildChessCharacterPresentation(content, {}, 10);
        REQUIRE(character.abilities.size() == 2);
        CHECK(character.abilities[0].combatOperation == style);
        CHECK_FALSE(character.abilities[0].ultimate);
        CHECK(character.abilities[0].details.size() == 1);
        CHECK(character.abilities[1].ultimate);
        CHECK(character.abilities[1].details.size() > 1);
        const auto compact = chessAbilitiesForRoleStar(content, *content.role(10), 1, EffectDescriptionStyle::Compact);
        const auto effectRows = effectDescriptionTextRows(compact.back().effects);
        REQUIRE_FALSE(effectRows.empty());
        CHECK(character.abilities[1].details.front().text == effectRows.front());
        CHECK(character.abilities[1].details.back().tone == PanelTextTone::Context);
        if (area == 1 || area == 2)
        {
            CHECK(character.abilities[0].details.back().text.contains("兩枚20%"));
            CHECK(character.abilities[1].details.back().text.contains("三枚35%"));
            CHECK(character.abilities[1].details.back().text.contains("穿透"));
            CHECK_FALSE(character.abilities[1].details.back().text.contains("抵消或削弱"));
        }
        for (const auto& ability : character.abilities)
            for (const auto& row : ability.details)
            {
                CHECK_FALSE(row.text.contains("內力消耗"));
                CHECK_FALSE(row.text.contains("格內"));
                CHECK_FALSE(row.text.contains("沒有額外配置"));
            }
        REQUIRE(character.synergies.size() == 1);
        CHECK(character.synergies[0].tooltip.empty());
        CHECK(character.synergies[0].id == content.combos().front().id);
    }
}

TEST_CASE("effect pages retain every line and semantic color within their height", "[chess][ui][panel-text]")
{
    const std::vector<PanelTextSourceRow> rows{
        {.text = "特殊效果：", .fontSizeDelta = 2, .spacingAfter = 4, .tone = PanelTextTone::Heading},
        {.text = std::string(90, 'a'), .spacingAfter = 3, .tone = PanelTextTone::Effect},
        {.text = "測試角色專屬：", .spacingBefore = 12, .tone = PanelTextTone::Exclusive},
        {.text = std::string(120, 'b'), .indentUnits = 2, .tone = PanelTextTone::Exclusive},
    };
    for (const int width : {112, 336, 672})
    {
        const auto layout = layoutPanelText(rows, width, 18);
        const auto pages = paginatePanelText(layout, 76);
        REQUIRE(pages.size() > 1);
        std::size_t index{};
        for (const auto& page : pages)
            for (const auto& line : page)
            {
                REQUIRE(index < layout.lines.size());
                CHECK(line.text == layout.lines[index].text);
                CHECK(line.tone == layout.lines[index].tone);
                CHECK(line.indentPixels == layout.lines[index].indentPixels);
                CHECK(line.y >= 0);
                CHECK(line.y + line.fontSize <= 76);
                ++index;
            }
        CHECK(index == layout.lines.size());
    }
}

TEST_CASE("enemy character inspector shares equipment and skills while using enemy stats and synergies", "[chess][ui][catalog]")
{
    const auto content = catalogContent(3, true);
    const auto analysis = analyzeChessChallengeEnemies(content, content.balance().challenges.front());
    REQUIRE(analysis.units.size() == 1);
    const auto& unit = analysis.units.front();
    const auto enemy = buildChessBattleCharacterPresentation(content, unit, analysis.enemySynergies);

    ChessSessionState state;
    state.roster.emplace(1, ChessSessionPiece{.instanceId = 1, .roleId = 10, .star = 2, .weaponInstanceId = 2});
    state.equipmentInventory.emplace(2, ChessEquipmentInstance{.instanceId = 2, .itemId = 500});
    const auto owned = buildChessCharacterPresentation(content, state, 10, 2, 1);
    CHECK(enemy.stats == *unit.initializedCombatStats);
    CHECK(enemy.stats != owned.stats);
    CHECK_FALSE(enemy.showCost);
    CHECK(owned.showCost);
    CHECK_FALSE(enemy.growthSummary);
    CHECK(enemy.equipment[0].imageId == 500);
    CHECK(enemy.equipment[1].title == "防具");
    REQUIRE(enemy.equipment[0].details.size() == owned.equipment[0].details.size());
    for (int i = 0; i < static_cast<int>(enemy.equipment[0].details.size()); ++i)
        CHECK(enemy.equipment[0].details[i].text == owned.equipment[0].details[i].text);
    REQUIRE(enemy.abilities.size() == owned.abilities.size());
    for (int i = 0; i < static_cast<int>(enemy.abilities.size()); ++i)
    {
        CHECK(enemy.abilities[i].value == owned.abilities[i].value);
        CHECK(enemy.abilities[i].combatOperation == owned.abilities[i].combatOperation);
        REQUIRE(enemy.abilities[i].details.size() == owned.abilities[i].details.size());
        for (int j = 0; j < static_cast<int>(enemy.abilities[i].details.size()); ++j)
            CHECK(enemy.abilities[i].details[j].text == owned.abilities[i].details[j].text);
    }
    REQUIRE_FALSE(enemy.synergies.empty());
    for (const auto& entry : enemy.synergies)
    {
        const auto metadata = std::ranges::find(analysis.enemySynergies, entry.id, &ChessComboMetadata::comboId);
        REQUIRE(metadata != analysis.enemySynergies.end());
        CHECK(entry.active);
        CHECK(entry.value == metadata->progressCount);
        CHECK(entry.tooltip.empty());
        CHECK(entry.imageGroup == "chess-synergies");
        CHECK(entry.imageId == (entry.id == 3 ? 13 : 14));
    }
    CHECK(std::ranges::any_of(enemy.synergies, [](const auto& entry) { return entry.id == 4; }));
    CHECK_FALSE(owned.synergies.front().active);
}

TEST_CASE("win growth breakdown includes talent and the current contributing synergy", "[chess][ui][catalog]")
{
    ChessGameContentData data;
    data.balance.starHPMult = 0.8;
    data.balance.starAtkMult = 0.5;
    data.balance.starDefMult = 0.25;
    data.balance.fightWinGrowthHP = 2.5;
    data.balance.fightWinGrowthAtk = 1;
    data.balance.fightWinGrowthDef = 1;
    data.balance.talents.emplace(ChessTalentId::LateBloomer,
        ChessTalentDefinition{.amplifiedGrowthPercent = 120});
    for (int id : {10, 20, 30})
    {
        ChessRoleDefinition role;
        role.ID = id;
        role.MaxHP = 101;
        role.Attack = 20;
        role.Defence = 10;
        data.roles.emplace(id, role);
    }
    ComboDef combo{.id = 1, .name = "成長羈絆", .memberRoleIds = {10}};
    combo.thresholds = {{.count = 1, .managementRules = {FightWinGrowthRule{90, 90, 90}}},
        {.count = 2, .managementRules = {FightWinGrowthRule{4, 2, 3}}}};
    data.combos.push_back(combo);
    data.equipment.push_back({.itemId = 500, .managementRules = {CountsAsComboRule{"成長羈絆"}}});
    const ChessGameContent content(std::move(data));
    ChessSessionState state;
    state.talent = ChessTalentId::LateBloomer;
    for (int id = 1; id <= 3; ++id)
        state.roster.emplace(id, ChessSessionPiece{.instanceId = id, .roleId = id * 10,
            .star = 2, .deployed = true, .fightsWon = 3});
    state.roster.at(2).weaponInstanceId = 5;
    state.equipmentInventory.emplace(5, ChessEquipmentInstance{.instanceId = 5, .itemId = 500, .assignedChessInstanceId = 2});

    const auto natural = chessPieceStatBreakdown(content, state, state.roster.at(1));
    CHECK(natural.fightWinGrowth.maxHp == 37);
    CHECK(natural.fightWinGrowth.attack == 14);
    CHECK(natural.fightWinGrowth.defence == 16);
    const auto transferred = chessPieceStatBreakdown(content, state, state.roster.at(2));
    CHECK(transferred.fightWinGrowth == natural.fightWinGrowth);
    const auto outsider = chessPieceStatBreakdown(content, state, state.roster.at(3));
    CHECK(outsider.fightWinGrowth.maxHp == 14);
    CHECK(outsider.fightWinGrowth.attack == 4);
    CHECK(outsider.fightWinGrowth.defence == 4);

    auto preview = state.roster.at(1);
    preview.fightsWon = 0;
    CHECK(chessPieceStatBreakdown(content, state, preview).fightWinGrowth == ChessCalculatedStats{});
    CHECK(chessStatDelta(natural.total, chessPieceStatBreakdown(content, state, preview).total) == natural.fightWinGrowth);
    preview = state.roster.at(1);
    preview.deployed = false;
    CHECK(chessPieceStatBreakdown(content, state, preview).fightWinGrowth == outsider.fightWinGrowth);
    state.talent = ChessTalentId::DivineArms;
    const auto ordinary = chessPieceStatBreakdown(content, state, state.roster.at(1));
    CHECK(ordinary.fightWinGrowth.maxHp == 19);
    CHECK(ordinary.fightWinGrowth.attack == 9);
    CHECK(ordinary.fightWinGrowth.defence == 12);
}

TEST_CASE("character synergy detail selects one threshold and distinguishes activation", "[chess][ui][catalog]")
{
    const ComboDef combo{.id = 8, .name = "測試羈絆", .iconId = 17};
    ChessComboMetadata metadata{.comboId = 8, .activeThresholdIndex = 1, .active = true, .progressCount = "4/6 ✓"};
    for (int count : {2, 4, 6})
    {
        ChessComboThresholdMetadata threshold{.requiredCount = count, .name = std::format("門檻{}", count)};
        appendStandaloneDescriptionRow(threshold.effects, std::format("效果{}", count));
        metadata.thresholds.push_back(threshold);
    }
    const auto active = chessCharacterSynergyEntry(combo, metadata);
    REQUIRE(active.details.size() == 2);
    CHECK(active.active);
    CHECK(active.details[0].text.contains("門檻4"));
    CHECK(active.details[0].tone == PanelTextTone::Effect);
    CHECK(active.details[1].text == "效果4");
    metadata.active = false;
    metadata.activeThresholdIndex = -1;
    metadata.progressCount = "1/2";
    const auto inactive = chessCharacterSynergyEntry(combo, metadata);
    REQUIRE(inactive.details.size() == 2);
    CHECK_FALSE(inactive.active);
    CHECK(inactive.details[0].text.contains("未啟動"));
    CHECK(inactive.details[1].text == "效果2");
    CHECK(inactive.details[1].tone == PanelTextTone::Inactive);
}

TEST_CASE("talent tables paginate long cells without shrinking or dropping text", "[chess][ui][catalog]")
{
    ChessTalentPresentation presentation;
    std::string effect;
    for (int i = 0; i < 80; ++i) effect += "長篇效果";
    presentation.facts.push_back({.category = "賭運", .label = "觸發條件", .value = effect, .kind = ChessTalentFactKind::Mechanic});
    for (const auto& [width, height, size] : std::array<std::array<int, 3>, 2>{{{334, 280, 18}, {672, 236, 26}}})
    {
        const auto pages = paginateChessTalentFacts(presentation, width, height, size);
        REQUIRE(pages.size() > 1);
        std::string recovered;
        for (const auto& page : pages)
        {
            const auto metrics = measureChessTalentFactTable(page, width, height, size, size);
            CHECK(metrics.fontSize == size);
            CHECK(metrics.height <= height);
            for (const auto& row : page.facts) recovered += row.value;
        }
        CHECK(recovered == effect);
    }
}
