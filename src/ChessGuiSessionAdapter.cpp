#include "ChessGuiSessionAdapter.h"

#include "BattleSceneHades.h"
#include "battle/BattlePresentationVisuals.h"
#include "BattleStatsView.h"
#include "BattleSummaryEquipment.h"
#include "BattlefieldData.h"
#include "Button.h"
#include "Audio.h"
#include "ChessBattleAnalysis.h"
#include "ChessBattleMapCatalog.h"
#include "ChessCatalogQueries.h"
#include "ChessCombo.h"
#include "ChessContextMenu.h"
#include "ChessEftIds.h"
#include "ChessEffectDescription.h"
#include "ChessGuiBattleFlow.h"
#include "ChessGuiSavePolicy.h"
#include "ChessManagementRules.h"
#include "ChessMenuFormatting.h"
#include "ChessPresentationHelpers.h"
#include "ChessPreparationScreen.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessPvp.h"
#include "ChessScreenLayout.h"
#include "ChessSessionCheckpoint.h"
#include "ChessStandaloneBattle.h"
#include "ChessSystemSettingsMenu.h"
#include "ChessUiCommon.h"
#include "ChessUiStyle.h"
#include "ChessSelectionScreen.h"
#include "DrawableOnCall.h"
#include "Engine.h"
#include "Event.h"
#include "ExternalJsonFileTransfer.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "Menu.h"
#include "ScenePreloader.h"
#include "SystemSettings.h"
#include "Talk.h"
#include "TextBox.h"
#include "TextureManager.h"
#include "UISave.h"
#include "strfunc.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace KysChess
{
namespace
{

struct SessionMenuData
{
    std::vector<std::string> labels;
    std::vector<Color> colors;
    std::vector<ChessSelectionItem> cards;
    bool browseOnly{};
    bool commandMode{};
    bool shortChoice{};
    bool shopMode{};
    std::string actionLabel = "選擇";
};


bool isPanelRowInRange(int row, int rowCount)
{
    return row >= 0 && row < rowCount;
}

int menuDisplayWidth(const std::string& text)
{
    return Font::getTextDrawSize(text);
}

std::vector<std::string> alignedMenuLabels(const std::vector<ChessMenuColumnRow>& rows)
{
    return buildAlignedChessMenuLabels(rows, menuDisplayWidth);
}

std::vector<std::string> contentSizedAlignedMenuLabels(const std::vector<ChessMenuColumnRow>& rows)
{
    return buildContentSizedAlignedChessMenuLabels(rows, menuDisplayWidth);
}

const char* tierLabel(int tier)
{
    static constexpr std::array<const char*, 5> labels{
        "一費", "二費", "三費", "四費", "五費",
    };
    return labels[std::clamp(tier, 1, 5) - 1];
}

Color effectCatalogSourceColor(ChessEffectCatalogSource source)
{
    switch (source)
    {
    case ChessEffectCatalogSource::Magic: return {255, 140, 255, 255};
    case ChessEffectCatalogSource::Equipment: return {255, 220, 100, 255};
    case ChessEffectCatalogSource::EquipmentSynergy: return {255, 170, 100, 255};
    case ChessEffectCatalogSource::Neigong: return {120, 255, 160, 255};
    case ChessEffectCatalogSource::ComboThreshold: return {130, 210, 255, 255};
    }
    std::unreachable();
}

struct BattlePreviewUnit
{
    std::string name;
    std::string skillNames;
    std::string weaponName;
    std::string armorName;
    int headId{};
    int star{};
    int hp{};
    int attack{};
    int defence{};
    int speed{};
    int weaponId{};
    int armorId{};
};

struct BattlePreviewPresentation
{
    int mapId = -1;
    std::string mapName;
    std::vector<BattlePreviewUnit> allies;
    std::vector<BattlePreviewUnit> enemies;
    std::vector<std::string> allyComboLines;
    std::vector<std::string> enemyComboLines;
    std::vector<int> attackSoundIds;
    std::vector<int> effectSoundIds;
};

std::vector<std::string> activeBattleComboLines(
    const std::vector<ChessComboMetadata>& synergies)
{
    std::vector<std::string> result;
    for (const auto& synergy : synergies)
    {
        assert(synergy.activeThresholdIndex >= 0);
        assert(synergy.activeThresholdIndex < static_cast<int>(synergy.thresholds.size()));
        const int targetThresholdIndex = synergy.nextThresholdIndex >= 0
            ? synergy.nextThresholdIndex
            : static_cast<int>(synergy.thresholds.size()) - 1;
        assert(targetThresholdIndex < static_cast<int>(synergy.thresholds.size()));
        result.push_back(std::format(
            "{} ({}/{})",
            synergy.name,
            synergy.effectiveCount,
            synergy.thresholds[targetThresholdIndex].requiredCount));
    }
    return result;
}

BattlePreviewPresentation makeBattlePreviewPresentation(
    const ChessGameSession& session,
    const PreparedChessBattle& prepared)
{
    const auto analysis = analyzePreparedChessBattle(
        prepared,
        session.content(),
        session.state().options.battleFrameLimit,
        std::nullopt);
    assert(analysis.combatInitialized);

    BattlePreviewPresentation result;
    result.mapId = analysis.identity.chosenMapId;
    result.mapName = analysis.identity.chosenMapName;
    result.allyComboLines = activeBattleComboLines(analysis.allySynergies);
    result.enemyComboLines = activeBattleComboLines(analysis.enemySynergies);
    result.attackSoundIds = analysis.presentationAssets.attackSoundIds;
    result.effectSoundIds = analysis.presentationAssets.effectIds;
    for (const auto& analyzedUnit : analysis.units)
    {
        assert(analyzedUnit.initializedCombatStats);
        const auto& stats = *analyzedUnit.initializedCombatStats;
        BattlePreviewUnit unit{
            analyzedUnit.name,
            analyzedUnit.skillNames,
            analyzedUnit.weaponName,
            analyzedUnit.armorName,
            analyzedUnit.headId,
            analyzedUnit.star,
            stats.maxHp,
            stats.attack,
            stats.defence,
            stats.speed,
            analyzedUnit.weaponItemId,
            analyzedUnit.armorItemId,
        };
        (analyzedUnit.team == 0 ? result.allies : result.enemies).push_back(std::move(unit));
    }
    return result;
}

void preloadPreparedBattlePresentation(
    const ChessGameSession& session,
    const BattlePreviewPresentation& preview)
{
    const auto preloadTextureGroup = [](std::string_view pathView) {
        const std::string path(pathView);
        const int frameCount = TextureManager::getInstance()->getTextureGroupCount(path);
        for (int frame = 0; frame < frameCount; ++frame)
        {
            if (auto* texture = TextureManager::getInstance()->getTexture(path, frame))
            {
                texture->load();
            }
        }
    };

    const auto preloadEffect = [&](int effectId) {
        if (effectId >= 0)
        {
            preloadTextureGroup(std::format("eft/eft{:03}", effectId));
        }
    };

    const auto preloadTeam = [&](const std::vector<BattlePreviewUnit>& team) {
        for (const auto& unit : team)
        {
            preloadTextureGroup(std::format("fight/fight{:03}", unit.headId));
        }
    };
    preloadTeam(preview.allies);
    preloadTeam(preview.enemies);
    for (const int effectId : preview.effectSoundIds)
    {
        preloadEffect(effectId);
    }
    for (int effect = 0; effect < 5; ++effect)
    {
        if (auto* texture = TextureManager::getInstance()->getTexture(std::format("eft/bld{:03}", effect), 0))
        {
            texture->load();
        }
    }
    for (const auto effect : EFT_ALL)
    {
        preloadEffect(std::to_underlying(effect));
    }
    for (const std::string_view path : KysChess::Battle::BattleEffectVisualPaths)
    {
        preloadTextureGroup(path);
    }

    const auto map = session.content().battleMaps().find(preview.mapId);
    assert(map != session.content().battleMaps().end());
    assert(session.state().preparedBattle);
    const int battlefieldId = session.state().preparedBattle->layout
            == PreparedChessBattleLayout::PvpArena
        ? ChessPvpMapLayout::BattlefieldId
        : map->second.battlefieldId;
    ScenePreloader::preloadBattlefieldAssets(battlefieldId);
    Audio::getInstance()->preloadBattleAudio(
        map->second.musicId,
        preview.attackSoundIds,
        preview.effectSoundIds);
}

int deployedCount(const ChessSessionState& state)
{
    return static_cast<int>(std::ranges::count_if(state.roster, [](const auto& entry) {
        return entry.second.deployed;
    }));
}

int benchCount(const ChessSessionState& state)
{
    return static_cast<int>(state.roster.size()) - deployedCount(state);
}

std::optional<ChessLegalActionDescriptor> legalAction(
    const ChessGameSession& session,
    ChessActionType type)
{
    const auto legal = session.legalActions();
    const auto found = std::ranges::find(legal, type, &ChessLegalActionDescriptor::type);
    return found == legal.end() ? std::nullopt : std::optional(*found);
}

void showShopPurchaseFeedback(
    const ChessGameSession& session,
    const ChessActionResult& result)
{
    const auto purchased = std::ranges::find(
        result.events,
        ChessSemanticEventType::ChessPurchased,
        &ChessSemanticEvent::type);
    assert(purchased != result.events.end());
    const auto* role = session.content().role(purchased->secondaryId);
    assert(role);
    const bool merged = std::ranges::contains(
        result.events,
        ChessSemanticEventType::ChessMerged,
        &ChessSemanticEvent::type);
    showChessMessage(merged
        ? std::format("消費{}，{}升星！", purchased->value, role->Name)
        : std::format("消費{}，獲取{}", purchased->value, role->Name));
    if (merged)
    {
        playChessUpgradeSound();
    }
}

void showCampaignVictoryFeedback(
    const ChessGameSession& session,
    const ChessActionResult& result,
    int oldLevel)
{
    const auto gold = std::ranges::find(
        result.events,
        ChessSemanticEventType::GoldAwarded,
        &ChessSemanticEvent::type);
    const auto experience = std::ranges::find(
        result.events,
        ChessSemanticEventType::ExperienceAwarded,
        &ChessSemanticEvent::type);
    assert(gold != result.events.end() && experience != result.events.end());
    const int comboBonus = gold->value - gold->primaryId - gold->secondaryId;
    const auto& state = session.state();
    const auto& balance = session.content().balance();
    const std::string levelMessage = state.level > oldLevel
        ? std::format(" 升級！等級{}", state.level + 1)
        : std::string{};
    const bool nextIsBoss = balance.bossInterval > 0
        && (state.fight + 1) % balance.bossInterval == 0;
    const std::string nextInformation = state.campaignComplete
        ? " 通關！"
        : nextIsBoss
            ? std::format(" 下一關：第{}關(Boss)", state.fight + 1)
            : std::format(" 下一關：第{}關", state.fight + 1);
    const std::string interestMessage = gold->secondaryId > 0
        ? std::format("(利息+${})", gold->secondaryId)
        : std::string{};
    const std::string comboMessage = comboBonus > 0
        ? chessVictoryComboBonusText(session.content(), *gold, comboBonus)
        : std::string{};
    showChessMessage(std::format(
        "勝利！獲得${}{}{} 經驗+{}{}{}",
        gold->primaryId,
        interestMessage,
        comboMessage,
        experience->value,
        levelMessage,
        nextInformation));
}

std::function<void(int, SDL_Rect)> makeBattleMapPreviewPanel(
    const ChessGameSession& session,
    std::vector<int> mapIds)
{
    return [&session, mapIds = std::move(mapIds)](int row, SDL_Rect panelFrame) {
        if (row < 0 || row >= static_cast<int>(mapIds.size()))
        {
            return;
        }
        const int mapId = mapIds[row];
        const auto map = session.content().battleMaps().find(mapId);
        assert(map != session.content().battleMaps().end());
        const auto battlefield = session.content().battlefields().find(map->second.battlefieldId);
        const BattlefieldData terrain(
            battlefield == session.content().battlefields().end() ? nullptr : &battlefield->second);
        const auto* catalog = ChessBattleMapCatalog::find(mapId);

        PanelTextCursor cursor{Font::getInstance(), panelFrame.x + 12, panelFrame.y + 10};
        cursor.line(chessBattleMapDisplayName(session.content(), mapId), 25, ChessUiStyle::gold, 4);
        cursor.line(
            std::format(
                "我方上限{}　敵軍上限{}",
                catalog ? catalog->teammatePositions.size() : map->second.teammateX.size(),
                catalog ? catalog->enemyCapacity : map->second.enemyX.size()),
            18,
            ChessUiStyle::ink,
            6);

        std::vector<Point> allyPositions;
        if (catalog)
        {
            allyPositions = catalog->teammatePositions;
        }
        else
        {
            for (std::size_t index = 0;
                 index < std::min(map->second.teammateX.size(), map->second.teammateY.size());
                 ++index)
            {
                allyPositions.push_back({map->second.teammateX[index], map->second.teammateY[index]});
            }
        }
        std::vector<Point> enemyPositions;
        for (std::size_t index = 0;
             index < std::min(map->second.enemyX.size(), map->second.enemyY.size());
             ++index)
        {
            enemyPositions.push_back({map->second.enemyX[index], map->second.enemyY[index]});
        }
        assert(!allyPositions.empty() || !enemyPositions.empty());
        int minimumX = BattlefieldData::CoordinateCount - 1;
        int maximumX = 0;
        int minimumY = BattlefieldData::CoordinateCount - 1;
        int maximumY = 0;
        const auto include = [&](const Point& point) {
            minimumX = std::min(minimumX, point.x);
            maximumX = std::max(maximumX, point.x);
            minimumY = std::min(minimumY, point.y);
            maximumY = std::max(maximumY, point.y);
        };
        for (const auto& point : allyPositions) include(point);
        for (const auto& point : enemyPositions) include(point);
        constexpr int padding = 3;
        minimumX = std::max(0, minimumX - padding);
        maximumX = std::min(BattlefieldData::CoordinateCount - 1, maximumX + padding);
        minimumY = std::max(0, minimumY - padding);
        maximumY = std::min(BattlefieldData::CoordinateCount - 1, maximumY + padding);

        const int columns = maximumX - minimumX + 1;
        const int rows = maximumY - minimumY + 1;
        const int availableWidth = panelFrame.w - 24;
        const int availableHeight = panelFrame.y + panelFrame.h - cursor.y - 10;
        const int halfWidth = std::max(2, std::min(
            availableWidth / std::max(2, columns + rows + 2),
            availableHeight * 2 / std::max(2, columns + rows + 2)));
        const int halfHeight = std::max(1, halfWidth / 2);
        const int originX = panelFrame.x + panelFrame.w / 2;
        const int originY = cursor.y + halfHeight + std::max(
            0,
            (availableHeight - (columns + rows) * halfHeight) / 2);
        const auto contains = [](const std::vector<Point>& positions, int x, int y) {
            return std::ranges::any_of(positions, [&](const Point& point) {
                return point.x == x && point.y == y;
            });
        };
        const auto drawDiamond = [](int centerX, int centerY, int halfW, int halfH, Color color) {
            for (int offsetY = -halfH; offsetY <= halfH; ++offsetY)
            {
                const float scale = 1.0f - static_cast<float>(std::abs(offsetY)) / static_cast<float>(halfH + 1);
                const int span = std::max(1, static_cast<int>(std::round(halfW * scale)));
                Engine::getInstance()->fillColor(color, centerX - span, centerY + offsetY, span * 2 + 1, 1);
            }
        };
        for (int y = minimumY; y <= maximumY; ++y)
        {
            for (int x = minimumX; x <= maximumX; ++x)
            {
                Color color = terrain.canWalk(x, y)
                    ? Color{90, 120, 80, 255}
                    : Color{75, 68, 58, 255};
                if (contains(enemyPositions, x, y)) color = {220, 90, 90, 255};
                if (contains(allyPositions, x, y)) color = {90, 190, 235, 255};
                const int localX = x - minimumX;
                const int localY = y - minimumY;
                drawDiamond(
                    originX + (localX - localY) * halfWidth,
                    originY + (localX + localY) * halfHeight,
                    halfWidth,
                    halfHeight,
                    color);
            }
        }
    };
}

ChessSelectionItem roleCard(const ChessGameSession& session, int roleId, int star = 1, int instanceId = -1)
{
    auto character = buildChessCharacterPresentation(session.content(), session.state(), roleId, star, instanceId);
    ChessSelectionItem item{
        .title = character.name,
        .subtitle = character.context,
        .badge = std::format("{}費 · {}星", character.cost, character.star),
        .image = {"head", character.headId},
        .accent = chessPieceTierColor(character.cost),
        .category = std::format("{}費", character.cost),
    };
    const auto stats = chessCharacterStats(character.stats);
    for (int i = 0; i < 4; ++i) item.facts.emplace_back(stats[i].first, std::to_string(stats[i].second));
    for (const auto& ability : character.abilities)
    {
        if (!item.summary.empty()) item.summary += " · ";
        item.summary += ability.title;
    }
    item.character = std::move(character);
    item.openCombo = [&session](int id) { showChessComboOverview(session.content(), session.state(), id); };
    return item;
}

std::vector<ChessSelectionItem> roleCards(const ChessGameSession& session, const std::vector<int>& roles,
    const std::vector<int>& stars = {}, const std::vector<int>& instances = {})
{
    std::vector<ChessSelectionItem> cards;
    for (int i = 0; i < static_cast<int>(roles.size()); ++i)
        cards.push_back(roleCard(session, roles[i], stars.empty() ? 1 : stars[i], instances.empty() ? -1 : instances[i]));
    return cards;
}

ChessSelectionItem equipmentCard(const ChessGameSession& session, const EquipmentDef& equipment)
{
    const auto metadata = chessEquipmentMetadata(session.content(), equipment.itemId, EffectDescriptionStyle::Compact);
    ChessSelectionItem card{
        .title = metadata.name,
        .subtitle = chessEquipmentTypeName(equipment.equipType),
        .badge = chessRewardTierLabel(equipment.tier),
        .image = {"item", equipment.itemId},
        .accent = chessRewardTierColor(equipment.tier),
        .category = chessEquipmentTypeName(equipment.equipType),
    };
    for (const auto& row : panelTextRowsForEquipment(metadata))
        if (row.fontSizeDelta == 0 && !row.text.empty()) { card.summary = row.text; break; }
    for (const auto& row : panelTextRowsForEquipment(chessEquipmentMetadata(session.content(), equipment.itemId, EffectDescriptionStyle::Full)))
        card.description += row.text + "\n";
    return card;
}

ChessSelectionItem neigongCard(const ChessGameSession& session, const NeigongDef& neigong)
{
    return {
        .title = neigong.name,
        .subtitle = session.state().obtainedNeigongIds.contains(neigong.magicId) ? "已獲得" : "",
        .badge = chessRewardTierLabel(neigong.tier),
        .image = {"item", neigong.itemId},
        .accent = chessRewardTierColor(neigong.tier),
        .category = chessRewardTierLabel(neigong.tier),
        .summary = joinEffectDescriptionRows(describeGameplayEffects(neigong.effects, EffectDescriptionStyle::Compact), " · "),
        .description = joinEffectDescriptionRows(describeGameplayEffects(neigong.effects, EffectDescriptionStyle::Full)),
    };
}

ChessSelectionItem comboCard(const ChessGameSession& session, const ComboDef& combo)
{
    const auto progress = evaluateChessComboProgress(session.state(), session.content(), combo);
    ChessSelectionItem card{
        .title = combo.name,
        .subtitle = progress.active ? "已啟動" : "",
        .badge = formatChessComboProgressCount(progress),
        .image = {"chess-synergies", combo.iconId},
        .accent = progress.active ? ChessUiStyle::positive : ChessUiStyle::muted,
        .category = progress.active ? "已啟動" : "未啟動",
        .relatedTitle = "羈絆成員",
    };
    const int threshold = progress.activeThresholdIndex >= 0 ? progress.activeThresholdIndex : 0;
    if (!combo.thresholds.empty())
    {
        const auto& tier = combo.thresholds[threshold];
        card.facts = {{"人數", std::format("{} / {}", progress.effectiveCount, progress.displayTargetCount)}, {"階段", tier.name}};
        card.summary = joinEffectDescriptionRows(describeGameplayEffectsAndManagementRules(
            tier.effects, tier.managementRules, EffectDescriptionStyle::Compact), " · ");
    }
    for (const int id : combo.memberRoleIds)
        if (const auto* role = session.content().role(id)) card.related.push_back({"head", role->HeadID});
    for (const auto& tier : combo.thresholds)
    {
        card.description += std::format("{} 人 · {}\n", tier.count, tier.name);
        card.description += joinEffectDescriptionRows(describeGameplayEffectsAndManagementRules(
            tier.effects, tier.managementRules, EffectDescriptionStyle::Full)) + "\n\n";
    }
    card.description += "羈絆成員\n";
    for (const int id : combo.memberRoleIds)
        if (const auto* role = session.content().role(id)) card.description += role->Name + "　";
    return card;
}

int runSelection(
    const std::string& title,
    const SessionMenuData& data,
    std::function<void(int, SDL_Rect)> detailRenderer = {},
    bool showSearch = false,
    bool exitable = true,
    std::function<bool()> exitConfirmation = {})
{
    if (data.labels.empty())
    {
        return -1;
    }

    auto cards = data.cards;
    if (cards.empty())
        for (int i = 0; i < static_cast<int>(data.labels.size()); ++i)
        {
            std::string caption;
            bool previousSpace = false;
            for (const char ch : data.labels[i])
            {
                if (ch != ' ' || !previousSpace) caption += ch;
                previousSpace = ch == ' ';
            }
            cards.push_back({.title = strfunc::trim(caption), .image = chessSelectionIcon(ChessUiStyle::Icon::Book),
                .accent = i < static_cast<int>(data.colors.size()) ? data.colors[i] : ChessUiStyle::gold});
        }
    assert(cards.size() == data.labels.size());
    auto menu = std::make_shared<ChessSelectionScreen>(title, std::move(cards));
    menu->setBrowseOnly(data.browseOnly);
    menu->setShopMode(data.shopMode);
    menu->setCommandMode(data.commandMode || (!detailRenderer && data.cards.empty()));
    if (data.shortChoice) menu->setShortChoiceMode();
    menu->setActionLabel(data.actionLabel);
    menu->setExitable(exitable);
    menu->setSearchEnabled(showSearch || data.labels.size() > 12);
    menu->setExitConfirmation(std::move(exitConfirmation));
    menu->setDetailRenderer(std::move(detailRenderer));
    menu->run();
    return menu->getResult();
}

std::optional<ChessContextMenuAction> runContextActionMenu(
    const std::string& title, const std::vector<ChessContextMenuItem>& items)
{
    SessionMenuData data;
    data.labels = chessContextMenuLabels(items);
    data.commandMode = true;
    for (const auto& item : items)
    {
        using Icon = ChessUiStyle::Icon;
        Icon icon = Icon::Book;
        switch (item.action)
        {
        case ChessContextMenuAction::ShowPositionSwap:
        case ChessContextMenuAction::SelectForBattle: icon = Icon::Formation; break;
        case ChessContextMenuAction::RerollBattleSeed: icon = Icon::Refresh; break;
        case ChessContextMenuAction::ViewCombos: icon = Icon::Link; break;
        case ChessContextMenuAction::ViewChessPool: icon = Icon::Character; break;
        case ChessContextMenuAction::ViewNeigong: icon = Icon::Cultivation; break;
        case ChessContextMenuAction::ViewEffects: icon = Icon::Spark; break;
        case ChessContextMenuAction::OpenEquipmentMenu:
        case ChessContextMenuAction::ShowEquipmentInventory: icon = Icon::Bag; break;
        case ChessContextMenuAction::ShowSystemSettings: icon = Icon::Sliders; break;
        case ChessContextMenuAction::LoadProgress: icon = Icon::Load; break;
        case ChessContextMenuAction::SaveProgress: icon = Icon::Save; break;
        case ChessContextMenuAction::ShowGameGuide: icon = Icon::Book; break;
        case ChessContextMenuAction::ShowExpeditionChallenge: icon = Icon::Mountain; break;
        case ChessContextMenuAction::ReturnToTitle: icon = Icon::Home; break;
        default: break;
        }
        data.cards.push_back({.title = item.label, .image = chessSelectionIcon(icon)});
    }
    const int result = runSelection(title, data);
    if (result < 0)
    {
        return std::nullopt;
    }
    assert(result < static_cast<int>(items.size()));
    return items[result].action;
}

int runSaveSlotMenu(int mode)
{
    auto menu = std::make_shared<UISave>();
    menu->setMode(mode);
    return menu->run();
}

bool confirmReturnToTitle()
{
    return runChessChoiceMenu("返回開頭？", {"返回開頭", "取消"},
        ChessChoiceStyle::DestructiveConfirmation) == 0;
}

constexpr int kBattlePreviewComboFontSize = 16;
constexpr int kBattlePreviewComboLineHeight = 20;
constexpr int kBattlePreviewComboColumnGap = 16;
constexpr int kBattlePreviewComboMinColumnWidth = 150;

int battlePreviewComboColumnCount(std::size_t lineCount, int width)
{
    return std::min(
        static_cast<int>(lineCount),
        std::max(
            1,
            (width + kBattlePreviewComboColumnGap)
                / (kBattlePreviewComboMinColumnWidth + kBattlePreviewComboColumnGap)));
}

int battlePreviewComboRequiredRows(
    const std::vector<std::string>& lines,
    int width)
{
    if (lines.empty())
    {
        return 0;
    }
    const int columns = battlePreviewComboColumnCount(lines.size(), width);
    return (static_cast<int>(lines.size()) + columns - 1) / columns;
}

void drawBattlePreviewComboLines(
    const std::vector<std::string>& lines,
    int x,
    int y,
    int width,
    int bottom)
{
    auto* font = Font::getInstance();
    if (lines.empty())
    {
        font->draw("無", kBattlePreviewComboFontSize, x, y, {150, 150, 150, 255});
        return;
    }

    const int availableHeight = std::max(kBattlePreviewComboLineHeight, bottom - y);
    const int maxColumns = battlePreviewComboColumnCount(lines.size(), width);
    const auto wrapForColumns = [&](int columns) {
        const int columnWidth = std::max(
            kBattlePreviewComboMinColumnWidth,
            (width - (columns - 1) * kBattlePreviewComboColumnGap) / columns);
        std::vector<std::vector<std::string>> wrapped;
        for (const auto& line : lines)
        {
            auto entry = wrapDisplayText(
                line,
                std::max(6, (columnWidth - 8) * 2 / kBattlePreviewComboFontSize));
            if (entry.empty())
            {
                entry.push_back(line);
            }
            wrapped.push_back(std::move(entry));
        }
        return wrapped;
    };
    const auto fits = [&](const std::vector<std::vector<std::string>>& wrapped, int columns) {
        int usedColumns = 1;
        int height{};
        for (const auto& entry : wrapped)
        {
            const int entryHeight = std::max(1, static_cast<int>(entry.size()))
                * kBattlePreviewComboLineHeight;
            if (height > 0 && height + entryHeight > availableHeight)
            {
                ++usedColumns;
                height = 0;
            }
            height += entryHeight;
        }
        return usedColumns <= columns;
    };

    int chosenColumns = maxColumns;
    auto wrapped = wrapForColumns(chosenColumns);
    for (int columns = 1; columns <= maxColumns; ++columns)
    {
        auto candidate = wrapForColumns(columns);
        if (fits(candidate, columns))
        {
            chosenColumns = columns;
            wrapped = std::move(candidate);
            break;
        }
    }
    const int columnWidth = std::max(
        kBattlePreviewComboMinColumnWidth,
        (width - (chosenColumns - 1) * kBattlePreviewComboColumnGap) / chosenColumns);
    int column{};
    int drawY = y;
    for (const auto& entry : wrapped)
    {
        const int entryHeight = std::max(1, static_cast<int>(entry.size()))
            * kBattlePreviewComboLineHeight;
        if (drawY > y && drawY - y + entryHeight > availableHeight)
        {
            ++column;
            drawY = y;
        }
        if (column >= chosenColumns)
        {
            break;
        }
        for (const auto& line : entry)
        {
            font->draw(
                line,
                kBattlePreviewComboFontSize,
                x + column * (columnWidth + kBattlePreviewComboColumnGap),
                drawY,
                {0, 255, 100, 255});
            drawY += kBattlePreviewComboLineHeight;
        }
    }
}

class PreBattleInformationPanel : public DismissibleTextBox
{
public:
    explicit PreBattleInformationPanel(std::function<void()> preload)
        : preload_(std::move(preload))
    {
    }

    void markFirstFramePresented()
    {
        flow_.markFirstFramePresented();
    }

    bool loadingVisible() const
    {
        return flow_.loadingVisible();
    }

    void dealEvent(EngineEvent& event) override
    {
        if (flow_.runPreloadAfterFirstFrame(preload_))
        {
            return;
        }

        ChessGuiPreBattleDismissInput input = ChessGuiPreBattleDismissInput::None;
        if (event.type == EVENT_KEY_UP)
        {
            input = ChessGuiPreBattleDismissInput::KeyboardRelease;
        }
        else if (event.type == EVENT_GAMEPAD_BUTTON_UP)
        {
            input = ChessGuiPreBattleDismissInput::GamepadRelease;
        }
        if (flow_.shouldDismiss(input))
        {
            setExit(true);
        }
    }

    PointerResult onPointerEvent(const PointerEvent& event) override
    {
        if (event.button != SDL_BUTTON_LEFT)
        {
            return RunNode::onPointerEvent(event);
        }
        if (event.phase == PointerPhase::ButtonDown)
        {
            if (!event.insidePresent)
            {
                return PointerResult::Ignored;
            }
            pointerReleaseArmed_ = flow_.canDismiss();
            return PointerResult::Captured;
        }
        if (event.phase == PointerPhase::ButtonUp)
        {
            if (pointerReleaseArmed_
                && flow_.shouldDismiss(ChessGuiPreBattleDismissInput::PointerRelease))
            {
                setExit(true);
            }
            pointerReleaseArmed_ = false;
            return PointerResult::Handled;
        }
        if (event.phase == PointerPhase::Cancel)
        {
            pointerReleaseArmed_ = false;
        }
        return PointerResult::Handled;
    }

    void onPointerInputReset() override
    {
        pointerReleaseArmed_ = false;
    }

    void onPressedOK() override
    {
    }

    void onPressedCancel() override
    {
    }

private:
    ChessGuiPreBattleFlow flow_;
    std::function<void()> preload_;
    bool pointerReleaseArmed_{};
};

void runBattleInformationPanel(
    const BattlePreviewPresentation& preview,
    std::function<void()> preload)
{
    auto box = std::make_shared<PreBattleInformationPanel>(std::move(preload));
    auto* information = box.get();
    auto panel = std::make_shared<DrawableOnCall>([preview, information](DrawableOnCall*) {
        auto* engine = Engine::getInstance();
        auto* font = Font::getInstance();
        const int width = engine->getUIWidth();
        const int height = engine->getUIHeight();
        const int halfWidth = width / 2;
        constexpr int kHeaderFontSize = 20;
        constexpr int kRowFontSize = 20;
        constexpr int kMaximumRowHeight = 44;
        constexpr int kMinimumRowHeight = 32;
        constexpr int kNameX = 46;
        constexpr int kStarX = 130;
        constexpr int kHpX = 160;
        constexpr int kAttackX = 220;
        constexpr int kDefenceX = 270;
        constexpr int kSpeedX = 320;
        constexpr int kEquipmentX = 365;
        constexpr int kSkillX = 475;
        const Color headerColor{180, 180, 180, 255};
        const Color white{255, 255, 255, 255};

        engine->fillColor({0, 0, 0, 200}, 0, 0, width, height);
        const std::string title = "戰前總覽";
        font->draw(
            title,
            28,
            width / 2 - 28 * Font::getTextDrawSize(title) / 4,
            10,
            {255, 215, 0, 255});

        const int columnWidth = halfWidth - 20;
        const int leftX = 10;
        const int rightX = halfWidth + 10;
        const int tableTop = 45;
        const int headerY = tableTop + 29;
        const int rowsY = headerY + 25;
        const int maximumRows = static_cast<int>(std::max(preview.allies.size(), preview.enemies.size()));
        assert(maximumRows > 0);
        const int footerY = height - 35;
        const int maximumComboRows = std::max(
            battlePreviewComboRequiredRows(preview.allyComboLines, columnWidth),
            battlePreviewComboRequiredRows(preview.enemyComboLines, columnWidth));
        const int comboBaseY = tableTop + 30 + 26 + 10;
        const int latestComboY = footerY - 7 - 24
            - maximumComboRows * kBattlePreviewComboLineHeight;
        const int rowHeight = std::clamp(
            (latestComboY - comboBaseY) / maximumRows,
            kMinimumRowHeight,
            kMaximumRowHeight);

        const auto drawTeam = [&](
            const std::vector<BattlePreviewUnit>& team,
            int x,
            const char* title,
            Color nameColor) {
            font->draw(title, 24, x, tableTop, {255, 215, 0, 255});
            font->draw("名稱", kHeaderFontSize, x + kNameX, headerY, headerColor);
            font->draw("★", kHeaderFontSize, x + kStarX, headerY, headerColor);
            font->draw("HP", kHeaderFontSize, x + kHpX, headerY, headerColor);
            font->draw("攻", kHeaderFontSize, x + kAttackX, headerY, headerColor);
            font->draw("防", kHeaderFontSize, x + kDefenceX, headerY, headerColor);
            font->draw("速", kHeaderFontSize, x + kSpeedX, headerY, headerColor);
            font->draw("裝備", kHeaderFontSize - 2, x + kEquipmentX, headerY, headerColor);
            font->draw("武學", kHeaderFontSize - 2, x + kSkillX, headerY, headerColor);

            int y = rowsY;
            for (const auto& unit : team)
            {
                if (auto* texture = TextureManager::getInstance()->getTexture("head", unit.headId))
                {
                    TextureManager::getInstance()->renderTexture(
                        texture,
                        x,
                        y - 2,
                        TextureManager::RenderInfo{white, 255, 0.22, 0.22});
                }
                font->draw(unit.name, kRowFontSize, x + kNameX, y, nameColor);
                font->draw(std::to_string(unit.star), kRowFontSize, x + kStarX, y, {255, 215, 0, 255});
                font->draw(std::to_string(unit.hp), kRowFontSize, x + kHpX, y, white);
                font->draw(std::to_string(unit.attack), kRowFontSize, x + kAttackX, y, white);
                font->draw(std::to_string(unit.defence), kRowFontSize, x + kDefenceX, y, white);
                font->draw(std::to_string(unit.speed), kRowFontSize, x + kSpeedX, y, white);
                drawBattleSummaryEquipment(
                    unit.weaponId,
                    unit.weaponName,
                    unit.armorId,
                    unit.armorName,
                    x + kEquipmentX,
                    y - 2);
                const auto skillSeparator = unit.skillNames.find(' ');
                font->draw(
                    unit.skillNames.substr(0, skillSeparator),
                    kRowFontSize - 6,
                    x + kSkillX,
                    y,
                    {180, 180, 180, 255});
                if (skillSeparator != std::string::npos)
                {
                    font->draw(
                        unit.skillNames.substr(skillSeparator + 1),
                        kRowFontSize - 6,
                        x + kSkillX,
                        y + 20,
                        {180, 180, 180, 255});
                }
                y += rowHeight;
            }
        };

        drawTeam(preview.allies, leftX, "我方", {100, 255, 100, 255});
        drawTeam(preview.enemies, rightX, "敵方", {255, 100, 100, 255});

        const int comboY = comboBaseY + maximumRows * rowHeight;
        if (!preview.allyComboLines.empty())
        {
            font->draw("我方羈絆", 20, leftX, comboY, {255, 215, 0, 255});
            drawBattlePreviewComboLines(preview.allyComboLines, leftX, comboY + 24, columnWidth, footerY - 7);
        }
        if (!preview.enemyComboLines.empty())
        {
            font->draw("敵方羈絆", 20, rightX, comboY, {255, 215, 0, 255});
            drawBattlePreviewComboLines(preview.enemyComboLines, rightX, comboY + 24, columnWidth, footerY - 7);
        }

        const std::string footer = "按任意鍵開始戰鬥";
        const int footerX = width / 2 - 20 * Font::getTextDrawSize(footer) / 4;
        font->draw(footer, 20, footerX, footerY, {200, 200, 200, 255});
        if (information->loadingVisible())
        {
            font->drawWithBoxCentered("載入戰鬥資源中...", 32, height / 2 - 16);
        }
        information->markFirstFramePresented();
    });

    box->setText("　");
    box->setHaveBox(false);
    box->setFontSize(1);
    box->addChild(panel);
    box->runAtPosition(0, 0);
}

class ChessPvpUiButton : public Button
{
public:
    ChessPvpUiButton(std::string text, int width, int height, int fontSize = 22)
    {
        resize_with_text_ = false;
        setText(std::move(text));
        setFontSize(fontSize);
        setSize(width, height);
    }

    void setSelected(bool selected) { selected_ = selected; }
    void setFillColor(Color color) { fill_ = color; }

    void draw() override
    {
        Color fill = fill_;
        if (selected_)
        {
            fill = ChessUiStyle::activeFill;
        }
        else if (state_ == NodePass)
        {
            fill = {115, 98, 67, 130};
        }
        else if (state_ == NodePress)
        {
            fill = {132, 111, 69, 180};
        }
        Engine::getInstance()->fillRoundedRect(fill, x_, y_, w_, h_, 7);
        if (selected_) Engine::getInstance()->fillColor(ChessUiStyle::gold, x_ + 8, y_ + h_ - 2, w_ - 16, 2);
        const int textWidth = font_size_ * Font::getTextDrawSize(text_) / 2;
        Font::getInstance()->draw(
            text_,
            font_size_,
            x_ + std::max(6, (w_ - textWidth) / 2),
            y_ + (h_ - font_size_) / 2,
            {245, 240, 225, 255});
    }

private:
    Color fill_ = ChessUiStyle::panelFill;
    bool selected_ = false;
};

const ChessPvpPiece* pvpPiece(const ChessPvpComposition& composition, int instanceId)
{
    const auto found = std::ranges::find(
        composition.pieces,
        instanceId,
        &ChessPvpPiece::chessInstanceId);
    return found == composition.pieces.end() ? nullptr : &*found;
}

std::string pvpSlotLabel(
    const ChessPvpComposition& composition,
    const ChessGameContent& content,
    int slot)
{
    assert(slot >= 0 && slot < static_cast<int>(composition.formationSlots.size()));
    const int instanceId = composition.formationSlots[slot];
    if (instanceId < 0)
    {
        return std::format("{:02} 空位", slot + 1);
    }
    const auto* piece = pvpPiece(composition, instanceId);
    assert(piece);
    const auto* role = content.role(piece->roleId);
    assert(role);
    return std::format("{:02} {}★{}", slot + 1, role->Name, piece->star);
}

constexpr int kPvpFormationPanelWidth = 535;
constexpr int kPvpFormationPanelHeight = 252;
constexpr int kPvpFormationSlotWidth = 145;
constexpr int kPvpFormationSlotHeight = 54;

std::array<Point, kChessFormationSlotCount> makeFormationUiOffsets(
    const std::array<Point, kChessFormationSlotCount>& positions)
{
    int minimumX = positions.front().x;
    int maximumX = positions.front().x;
    int minimumY = positions.front().y;
    int maximumY = positions.front().y;
    for (const auto& position : positions)
    {
        minimumX = std::min(minimumX, position.x);
        maximumX = std::max(maximumX, position.x);
        minimumY = std::min(minimumY, position.y);
        maximumY = std::max(maximumY, position.y);
    }

    assert(minimumX < maximumX && minimumY < maximumY);
    std::array<Point, kChessFormationSlotCount> result{};
    for (int slot = 0; slot < kChessFormationSlotCount; ++slot)
    {
        result[slot] = {
            (positions[slot].x - minimumX)
                * (kPvpFormationPanelWidth - kPvpFormationSlotWidth)
                / (maximumX - minimumX),
            (positions[slot].y - minimumY)
                * (kPvpFormationPanelHeight - kPvpFormationSlotHeight)
                / (maximumY - minimumY),
        };
    }
    return result;
}

const std::array<Point, kChessFormationSlotCount>& formationUiOffsets(
    bool opponentSide)
{
    static const auto local = makeFormationUiOffsets(
        ChessPvpMapLayout::localFormation());
    static const auto opponent = makeFormationUiOffsets(
        ChessPvpMapLayout::opponentFormation());
    return opponentSide ? opponent : local;
}

void drawPvpFormationFrame(
    int originX,
    int originY,
    bool opponentSide)
{
    auto* engine = Engine::getInstance();
    engine->fillRoundedRect(
        ChessUiStyle::quietFill,
        originX - 8,
        originY - 8,
        kPvpFormationPanelWidth + 16,
        kPvpFormationPanelHeight + 16,
        8);

    auto* font = Font::getInstance();
    const Color guideColor{175, 185, 185, 255};
    font->draw(opponentSide ? "← 前線" : "後方", 18, originX + 38, originY - 31, guideColor);
    font->draw("中陣", 18, originX + 236, originY - 31, guideColor);
    font->draw(opponentSide ? "後方" : "前線 →", 18, originX + 420, originY - 31, guideColor);
}

void drawPvpFormation(
    const ChessPvpComposition& composition,
    const ChessGameContent& content,
    int originX,
    int originY,
    bool opponentSide,
    Color occupiedColor)
{
    drawPvpFormationFrame(originX, originY, opponentSide);
    const auto& offsets = formationUiOffsets(opponentSide);
    for (int slot = 0; slot < kChessFormationSlotCount; ++slot)
    {
        const auto offset = offsets[slot];
        const bool occupied = composition.formationSlots[slot] >= 0;
        Color fill = occupied ? occupiedColor : Color{18, 18, 18, 210};
        fill.a = 210;
        Engine::getInstance()->fillRoundedRect(
            fill,
            originX + offset.x,
            originY + offset.y,
            kPvpFormationSlotWidth,
            kPvpFormationSlotHeight,
            7);
        Font::getInstance()->draw(
            pvpSlotLabel(composition, content, slot),
            18,
            originX + offset.x + 7,
            originY + offset.y + 17,
            {245, 240, 225, 255});
    }
}

std::optional<std::uint32_t> editPvpBattleSeed(std::uint32_t current);
bool copyPvpBattleSeed(std::uint32_t seed, std::string& error);
std::optional<std::uint32_t> pastePvpBattleSeed(std::string& error);
std::uint32_t nextPvpBattleSeed();
std::string pvpExportFilename();

enum class OfflinePvpAction
{
    Export,
    Import,
    Start,
    Back,
};

std::optional<ChessPvpComposition> importAndVerifyPvpOpponent(
    std::shared_ptr<const ChessGameContent> content);
std::optional<ChessPvpSaveVerificationResult> runPvpSaveVerification(
    std::shared_ptr<const ChessGameContent> content,
    std::string_view payload,
    std::string targetLabel,
    std::string_view operation);

void drawOfflineBattlePresentation(
    const ChessPvpComposition& local,
    const std::optional<ChessPvpComposition>& opponent,
    const ChessGameContent& content,
    std::uint32_t battleSeed)
{
    auto* engine = Engine::getInstance();
    ChessUiStyle::page("離線對戰");
    ChessUiStyle::panel(24, 76, 572, 398);
    ChessUiStyle::panel(684, 76, 572, 398);
    ChessUiStyle::panel(304, 476, 672, 132);
    ChessUiStyle::panel(16, 620, 1248, 84);
    auto* font = Font::getInstance();
    font->draw("我的陣形", 24, 240, 80, ChessUiStyle::context);
    font->draw("對手陣形", 24, 875, 80, ChessUiStyle::negative);
    drawPvpFormationFrame(45, 125, false);
    if (opponent)
    {
        drawPvpFormation(*opponent, content, 700, 125, true, {64, 45, 39, 200});
        font->draw("已驗證", 22, 925, 405, ChessUiStyle::positive);
    }
    else
    {
        drawPvpFormationFrame(700, 125, true);
        font->draw("尚未匯入對手存檔", 24, 835, 225, {210, 210, 200, 255});
    }
    font->draw("VS", 28, 618, 230, ChessUiStyle::gold);
    font->draw(
        std::format("我方出戰 {} 名", local.pieces.size()),
        20,
        235,
        445,
        local.pieces.empty() ? ChessUiStyle::negative : Color{220, 220, 210, 255});
    const auto seedText = std::format("戰鬥種子：{}", battleSeed);
    const int seedTextWidth = 22 * Font::getTextDrawSize(seedText) / 2;
    font->draw(
        seedText,
        22,
        (engine->getUIWidth() - seedTextWidth) / 2,
        480,
        ChessUiStyle::gold);
}

class ChessOfflineBattleScreenNode : public Menu
{
public:
    ChessOfflineBattleScreenNode(
        ChessPvpComposition local,
        std::optional<ChessPvpComposition> opponent,
        ChessGameSession& session,
        std::function<ChessActionResult(const ChessAction&)> submitFormation,
        std::uint32_t battleSeed,
        std::vector<OfflinePvpAction> actionIds,
        std::vector<std::string> actionLabels)
        : local_(std::move(local)),
          opponent_(std::move(opponent)),
          session_(session),
          content_(session.content()),
          submitFormation_(std::move(submitFormation)),
          battleSeed_(battleSeed),
          actions_(std::move(actionIds))
    {
        assert(actions_.size() == actionLabels.size());
        full_window_ = 0;
        for (int slot = 0; slot < kChessFormationSlotCount; ++slot)
        {
            const auto offset = formationUiOffsets(false)[slot];
            addChild(
                std::make_shared<ChessPvpUiButton>(
                    "",
                    kPvpFormationSlotWidth,
                    kPvpFormationSlotHeight,
                    18),
                45 + offset.x,
                125 + offset.y);
        }
        constexpr int seedActionWidth = 140;
        constexpr int seedActionGap = 14;
        const std::array<std::string_view, kSeedActionCount> seedActions{
            "編輯種子",
            "複製種子",
            "貼上種子",
            "重擲種子",
        };
        const int seedActionTotalWidth = kSeedActionCount * seedActionWidth
            + (kSeedActionCount - 1) * seedActionGap;
        int seedActionX = (1280 - seedActionTotalWidth) / 2;
        for (const auto& action : seedActions)
        {
            addChild(
                std::make_shared<ChessPvpUiButton>(
                    std::string(action),
                    seedActionWidth,
                    48,
                    20),
                seedActionX,
                520);
            seedActionX += seedActionWidth + seedActionGap;
        }

        const int count = static_cast<int>(actionLabels.size());
        const int width = std::min(220, (1180 - (count - 1) * 14) / std::max(1, count));
        const int totalWidth = count * width + (count - 1) * 14;
        int x = (1280 - totalWidth) / 2;
        for (const auto& label : actionLabels)
        {
            addChild(std::make_shared<ChessPvpUiButton>(label, width, 58, 22), x, 635);
            x += width + 14;
        }
        refreshFormationButtons();
        forceActiveChild(0);
    }

    std::uint32_t battleSeed() const { return battleSeed_; }
    std::optional<OfflinePvpAction> selectedAction() const
    {
        if (result_ < 0)
        {
            return std::nullopt;
        }
        assert(result_ < static_cast<int>(actions_.size()));
        return actions_[result_];
    }
    std::optional<ChessPvpComposition> takeOpponent()
    {
        return std::move(opponent_);
    }

    void draw() override
    {
        drawOfflineBattlePresentation(local_, opponent_, content_, battleSeed_);
        Font::getInstance()->draw(
            selectedSlot_
                ? std::format("已選第 {} 格，再選一格交換", *selectedSlot_ + 1)
                : "點選兩格交換陣位",
            20,
            195,
            405,
            selectedSlot_
                ? Color{255, 225, 130, 255}
                : Color{190, 205, 210, 255});
        if (!status_.empty())
        {
            const int statusWidth = 18 * Font::getTextDrawSize(status_) / 2;
            Font::getInstance()->draw(
                status_,
                18,
                (Engine::getInstance()->getUIWidth() - statusWidth) / 2,
                582,
                {130, 225, 160, 255});
        }
    }

    void onPressedOK() override
    {
        checkActiveToResult();
        const int selected = result_;
        result_ = -1;
        if (selected < 0)
        {
            return;
        }
        if (selected < kChessFormationSlotCount)
        {
            selectFormationSlot(selected);
            forceActiveChild(selected);
            return;
        }

        const int control = selected - kChessFormationSlotCount;
        if (control >= kSeedActionCount)
        {
            const int actionIndex = control - kSeedActionCount;
            assert(actionIndex < static_cast<int>(actions_.size()));
            const auto action = actions_[actionIndex];
            if (action == OfflinePvpAction::Export)
            {
                const auto revision = static_cast<std::uint64_t>(
                    std::chrono::system_clock::now().time_since_epoch().count());
                const auto checkpoint = ChessSessionCheckpoint::capture(
                    session_,
                    revision,
                    "離線對戰匯出");
                const auto payload = checkpoint.serializeJson();
                if (payload.empty())
                {
                    LOG(
                        "[離線對戰存檔匯出] 失敗（步驟：序列化檢查點）：未產生 JSON 內容\n");
                    showChessMessage(
                        "匯出失敗：無法序列化目前的存檔",
                        28);
                    forceActiveChild(selected);
                    return;
                }
                const auto verification = runPvpSaveVerification(
                    session_.sharedContent(),
                    payload,
                    "我的存檔",
                    "匯出前");
                if (!verification)
                {
                    forceActiveChild(selected);
                    return;
                }
                if (!verification->valid)
                {
                    showChessMessage(std::format(
                        "匯出前驗證失敗：{}",
                        verification->message),
                        28);
                    forceActiveChild(selected);
                    return;
                }
                const auto transfer = ExternalJsonFileTransfer::exportJson(
                    "匯出離線對戰檢查點",
                    pvpExportFilename(),
                    payload);
                if (transfer.status == ExternalJsonTransferStatus::Error)
                {
                    LOG(
                        "[離線對戰存檔匯出] 失敗（步驟：寫出外部檔案）：{}\n",
                        transfer.error);
                    showChessMessage(
                        std::format("匯出失敗：{}", transfer.error),
                        28);
                }
                else if (transfer.status == ExternalJsonTransferStatus::Completed)
                {
                    status_ = "已匯出離線對戰檢查點";
                }
                forceActiveChild(selected);
                return;
            }
            if (action == OfflinePvpAction::Import)
            {
                if (auto imported = importAndVerifyPvpOpponent(session_.sharedContent()))
                {
                    opponent_ = std::move(*imported);
                    exitWithResult(actionIndex);
                }
                else
                {
                    forceActiveChild(selected);
                }
                return;
            }
            exitWithResult(actionIndex);
            return;
        }

        switch (control)
        {
        case 0:
            if (const auto edited = editPvpBattleSeed(battleSeed_))
            {
                battleSeed_ = *edited;
                status_ = "已更新戰鬥種子";
            }
            break;
        case 1:
        {
            std::string error;
            if (copyPvpBattleSeed(battleSeed_, error))
            {
                status_ = "已複製戰鬥種子";
            }
            else
            {
                showChessMessage(std::format("無法複製戰鬥種子：{}", error));
            }
            break;
        }
        case 2:
        {
            std::string error;
            if (const auto pasted = pastePvpBattleSeed(error))
            {
                battleSeed_ = *pasted;
                status_ = "已從剪貼簿貼上戰鬥種子";
            }
            else
            {
                showChessMessage(error);
            }
            break;
        }
        case 3:
        {
            auto rerolled = nextPvpBattleSeed();
            while (rerolled == battleSeed_)
            {
                rerolled = nextPvpBattleSeed();
            }
            battleSeed_ = rerolled;
            status_ = "已重擲戰鬥種子";
            break;
        }
        default:
            std::unreachable();
        }
        forceActiveChild(selected);
    }

    void onPressedCancel() override
    {
        if (selectedSlot_)
        {
            selectedSlot_.reset();
            refreshFormationButtons();
            return;
        }
        exitWithResult(-1);
    }

private:
    static constexpr int kSeedActionCount = 4;

    void selectFormationSlot(int slot)
    {
        if (!selectedSlot_)
        {
            selectedSlot_ = slot;
            refreshFormationButtons();
            return;
        }
        if (*selectedSlot_ == slot)
        {
            selectedSlot_.reset();
            refreshFormationButtons();
            return;
        }

        auto formation = local_.formationSlots;
        std::swap(formation[*selectedSlot_], formation[slot]);
        ChessAction action;
        action.type = ChessActionType::SetFormation;
        action.chessInstanceIds = std::move(formation);
        const auto result = submitFormation_(action);
        if (result.accepted)
        {
            local_ = extractChessPvpComposition(session_);
            status_ = "陣形已更新";
        }
        else
        {
            showChessMessage(result.description);
        }
        selectedSlot_.reset();
        refreshFormationButtons();
    }

    void refreshFormationButtons()
    {
        for (int slot = 0; slot < kChessFormationSlotCount; ++slot)
        {
            auto button = std::dynamic_pointer_cast<ChessPvpUiButton>(childs_[slot]);
            assert(button);
            button->setText(pvpSlotLabel(local_, content_, slot));
            button->setFillColor(local_.formationSlots[slot] >= 0
                ? Color{64, 60, 45, 200}
                : Color{18, 18, 18, 210});
            button->setSelected(selectedSlot_ && *selectedSlot_ == slot);
        }
    }

    ChessPvpComposition local_;
    std::optional<ChessPvpComposition> opponent_;
    ChessGameSession& session_;
    const ChessGameContent& content_;
    std::function<ChessActionResult(const ChessAction&)> submitFormation_;
    std::uint32_t battleSeed_{};
    std::string status_;
    std::vector<OfflinePvpAction> actions_;
    std::optional<int> selectedSlot_;
};

class ChessPvpVerificationNode : public Menu
{
public:
    ChessPvpVerificationNode(
        std::shared_ptr<const ChessGameContent> content,
        std::string_view payload,
        std::string targetLabel)
        : verifier_(std::move(content), payload),
          targetLabel_(std::move(targetLabel))
    {
        dark_ = 1;
        addChild(
            std::make_shared<ChessPvpUiButton>("取消驗證", 220, 50, 24),
            530,
            635);
    }

    void draw() override
    {
        auto* engine = Engine::getInstance();
        engine->fillRoundedRect(ChessUiStyle::panelFill, 320, 12, 640, 54, 10);

        engine->fillRoundedRect(ChessUiStyle::panelFill, 250, 500, 780, 195, 12);

        auto* font = Font::getInstance();
        font->draw(
            std::format("離線對戰  >  驗證{}", targetLabel_),
            24,
            390,
            27,
            {175, 195, 205, 255});
        font->draw(
            std::format("正在驗證{}...", targetLabel_),
            28,
            495,
            520,
            {255, 220, 90, 255});
        const auto total = verifier_.totalActionCount();
        const auto completed = verifier_.completedActionCount();
        const auto current = total == 0 ? 0 : std::min(total, completed + 1);
        font->draw(
            std::format("操作 {} / {}", current, total),
            22,
            565,
            558,
            {225, 225, 215, 255});
        constexpr int progressWidth = 500;
        const int completedWidth = total == 0
            ? 0
            : static_cast<int>(progressWidth * completed / total);
        engine->fillRoundedRect({35, 40, 45, 255}, 390, 595, progressWidth, 20, 5);
        if (completedWidth > 0)
        {
            engine->fillRoundedRect(
                ChessUiStyle::gold,
                390,
                595,
                completedWidth,
                20,
                5);
        }
        engine->drawRoundedRect({150, 165, 170, 255}, 390, 595, progressWidth, 20, 5);
    }

    void backRun() override
    {
        if (verifier_.finished())
        {
            exitWithResult(0);
            return;
        }
        verifier_.step(160, 2400);
        if (verifier_.finished())
        {
            exitWithResult(0);
        }
    }

    void onPressedOK() override
    {
        cancelled_ = true;
        exitWithResult(-1);
    }

    void onPressedCancel() override
    {
        cancelled_ = true;
        exitWithResult(-1);
    }

    bool cancelled() const { return cancelled_; }
    ChessPvpSaveVerificationResult takeVerificationResult()
    {
        assert(!cancelled_ && verifier_.finished());
        return verifier_.takeResult();
    }

private:
    ChessPvpSaveVerifier verifier_;
    std::string targetLabel_;
    bool cancelled_ = false;
};

std::string_view pvpSaveValidationStep(ChessPvpSaveError error)
{
    switch (error)
    {
    case ChessPvpSaveError::None: return "完成驗證";
    case ChessPvpSaveError::Malformed: return "解析檢查點";
    case ChessPvpSaveError::VersionMismatch: return "檢查遊戲版本";
    case ChessPvpSaveError::HardModeRequired: return "檢查困難模式";
    case ChessPvpSaveError::ReplayVerificationFailed: return "驗證重播";
    case ChessPvpSaveError::UnrepresentableSnapshot: return "檢查穩定決策邊界";
    case ChessPvpSaveError::SnapshotStateMismatch: return "比對快照狀態";
    case ChessPvpSaveError::SnapshotRandomMismatch: return "比對隨機狀態";
    case ChessPvpSaveError::SnapshotHashMismatch: return "比對存檔完整性";
    case ChessPvpSaveError::InvalidFormation: return "檢查陣形";
    case ChessPvpSaveError::NoDeployedPieces: return "檢查出戰棋子";
    }
    std::unreachable();
}

std::optional<ChessPvpSaveVerificationResult> runPvpSaveVerification(
    std::shared_ptr<const ChessGameContent> content,
    std::string_view payload,
    std::string targetLabel,
    std::string_view operation)
{
    auto verification = std::make_shared<ChessPvpVerificationNode>(
        std::move(content),
        payload,
        std::move(targetLabel));
    verification->run();
    if (verification->cancelled())
    {
        return std::nullopt;
    }

    auto result = verification->takeVerificationResult();
    if (!result.valid)
    {
        LOG(
            "[離線對戰存檔驗證] {}失敗（步驟：{}）：{}\n",
            operation,
            pvpSaveValidationStep(result.error),
            result.message);
    }
    return result;
}

std::optional<ChessPvpComposition> importAndVerifyPvpOpponent(
    std::shared_ptr<const ChessGameContent> content)
{
    auto transfer = ExternalJsonFileTransfer::importJson("匯入對手存檔");
    if (transfer.status == ExternalJsonTransferStatus::Cancelled)
    {
        return std::nullopt;
    }
    if (transfer.status == ExternalJsonTransferStatus::Error)
    {
        showChessMessage(
            std::format("匯入失敗：{}", transfer.error),
            28);
        return std::nullopt;
    }

    auto verified = runPvpSaveVerification(
        std::move(content),
        transfer.text,
        "對手存檔",
        "匯入");
    if (!verified)
    {
        return std::nullopt;
    }
    if (!verified->valid)
    {
        showChessMessage(
            std::format("匯入驗證失敗：{}", verified->message),
            28);
        return std::nullopt;
    }
    return std::move(verified->composition);
}

std::string pvpExportFilename()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t value = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &value);
#else
    localtime_r(&value, &local);
#endif
    return std::format(
        "kys-opponent-{:04}-{:02}-{:02}-{:02}-{:02}.json",
        local.tm_year + 1900,
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min);
}

std::optional<std::uint32_t> parsePvpBattleSeed(std::string_view text)
{
    while (!text.empty()
        && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }
    while (!text.empty()
        && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }
    if (text.empty())
    {
        return std::nullopt;
    }

    std::uint64_t value{};
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value);
    if (error != std::errc{}
        || end != text.data() + text.size()
        || value > std::numeric_limits<std::uint32_t>::max())
    {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(value);
}

std::string invalidPvpBattleSeedMessage()
{
    return std::format(
        "戰鬥種子必須是 0 至 {} 的整數",
        std::numeric_limits<std::uint32_t>::max());
}

std::optional<std::uint32_t> editPvpBattleSeed(std::uint32_t current)
{
    auto input = std::make_shared<InputBox>(
        std::format("目前種子 {}，請輸入新的戰鬥種子：", current),
        26);
    input->setInputPosition(300, 280);
    input->run();
    if (input->getResult() < 0)
    {
        return std::nullopt;
    }
    const auto parsed = parsePvpBattleSeed(input->getText());
    if (!parsed)
    {
        showChessMessage(invalidPvpBattleSeedMessage());
    }
    return parsed;
}

bool copyPvpBattleSeed(std::uint32_t seed, std::string& error)
{
    const auto text = std::to_string(seed);
    if (!SDL_SetClipboardText(text.c_str()))
    {
        error = SDL_GetError();
        return false;
    }
    return true;
}

std::optional<std::uint32_t> pastePvpBattleSeed(std::string& error)
{
    char* clipboard = SDL_GetClipboardText();
    if (!clipboard)
    {
        error = SDL_GetError();
        return std::nullopt;
    }
    const std::string text(clipboard);
    SDL_free(clipboard);
    const auto parsed = parsePvpBattleSeed(text);
    if (!parsed)
    {
        error = invalidPvpBattleSeedMessage();
    }
    return parsed;
}

std::uint32_t nextPvpBattleSeed()
{
    static std::uint32_t previous = static_cast<std::uint32_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    return ++previous;
}

ChessGuiFlowResult currentChessGuiFlowResult()
{
    return chessGuiFlowResult(RunNode::runOwnerExitRequested());
}

void autoSavePersistableGuiState(const ChessGameSession& session)
{
    if (canPersistChessGuiState(session))
    {
        UISave::autoSave();
    }
}

}

ChessActionResult ChessGuiSessionAdapter::submitGuiAction(const ChessAction& action)
{
    auto result = session_.submitAndDrain(action);
    if (result.accepted)
    {
        autoSavePersistableGuiState(session_);
    }
    return result;
}

void ChessGuiSessionAdapter::showShop()
{
    for (;;)
    {
        const auto observation = session_.observe();
        SessionMenuData data;
        data.shopMode = true;
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<int> roleIds{-1, -1};
        std::vector<int> starRows{1, 1};
        std::vector<int> instanceRows{-1, -1};
        std::vector<int> shopSlots{-1, -1};

        const bool freeRefresh = observation.freeShopRefreshAvailable;
        data.cards.push_back({.title = "刷新", .badge = freeRefresh ? "免費" : std::format("{} 金", session_.content().balance().refreshCost),
            .image = chessSelectionIcon(ChessUiStyle::Icon::Refresh)});
        data.cards.push_back({.title = observation.shopLocked ? "解除鎖定" : "鎖定商店", .image = chessSelectionIcon(ChessUiStyle::Icon::Lock)});
        labelRows.push_back({
            {},
            "刷新",
            freeRefresh ? "免費" : "",
            freeRefresh
                ? std::format("(${})", session_.content().balance().refreshCost)
                : std::format("${}", session_.content().balance().refreshCost),
        });
        data.colors.push_back(freeRefresh
            ? Color{255, 235, 120, 255}
            : Color{255, 204, 229, 255});
        labelRows.push_back({{}, observation.shopLocked ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定", {}, {}});
        data.colors.push_back(observation.shopLocked ? ChessUiStyle::negative : Color{128, 128, 128, 255});

        for (int shopSlot = 0; shopSlot < static_cast<int>(observation.shop.size()); ++shopSlot)
        {
            const auto& slot = observation.shop[shopSlot];
            if (slot.roleId < 0)
            {
                continue;
            }
            const auto* role = session_.content().role(slot.roleId);
            assert(role);
            const int price = session_.content().balance().tierPrices[slot.tier - 1];
            labelRows.push_back({{}, role->Name, chessStars(1), std::format("${}", price)});
            data.colors.push_back(chessPieceTierColor(slot.tier));
            roleIds.push_back(slot.roleId);
            starRows.push_back(1);
            instanceRows.push_back(-1);
            shopSlots.push_back(shopSlot);
            auto card = roleCard(session_, slot.roleId);
            card.character->context = "商店預覽";
            card.subtitle = std::format("{} 金", price);
            data.cards.push_back(std::move(card));
        }
        data.labels = alignedMenuLabels(labelRows);

        const int selected = runSelection(std::format(
                "購買棋子　等級{}　${}　背包{}/{}",
                observation.level + 1,
                observation.money,
                benchCount(session_.state()),
                session_.content().balance().benchSize), data, {}, false);
        if (selected < 0)
        {
            return;
        }

        ChessAction action;
        if (selected == 0)
        {
            action.type = ChessActionType::RefreshShop;
        }
        else if (selected == 1)
        {
            action.type = ChessActionType::SetShopLocked;
            action.value = !session_.state().shopLocked;
        }
        else
        {
            action.type = ChessActionType::BuyShopSlot;
            action.shopSlot = shopSlots[selected];
        }
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
        }
        else if (action.type == ChessActionType::BuyShopSlot)
        {
            showShopPurchaseFeedback(session_, result);
        }
    }
}

void ChessGuiSessionAdapter::chooseChess(ChessActionType actionType)
{
    for (;;)
    {
        const auto observation = session_.observe();
        if (observation.roster.empty())
        {
            showChessMessage("目前沒有棋子");
            return;
        }
        SessionMenuData data;
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<int> roleIds;
        std::vector<int> starRows;
        std::vector<int> instanceIds;
        for (const auto& piece : observation.roster)
        {
            const auto* role = session_.content().role(piece.roleId);
            assert(role);
            labelRows.push_back({
                piece.deployed ? "[戰]" : "",
                role->Name,
                chessStars(piece.star),
                std::format("${}", ChessManagementRules::pieceValue(session_.content(), piece.roleId, piece.star)),
            });
            data.colors.push_back(chessPieceTierColor(role->Cost));
            roleIds.push_back(piece.roleId);
            starRows.push_back(piece.star);
            instanceIds.push_back(piece.instanceId);
        }
        data.labels = alignedMenuLabels(labelRows);
        data.cards = roleCards(session_, roleIds, starRows, instanceIds);
        data.actionLabel = actionType == ChessActionType::SellChess ? "出售" : "選擇";
        const int selected = runSelection(actionType == ChessActionType::SellChess
                ? std::format("出售棋子　背包{}/{}", benchCount(session_.state()), session_.content().balance().benchSize)
                : "選擇棋子", data, {}, false);
        if (selected < 0)
        {
            return;
        }
        ChessAction action;
        action.type = actionType;
        action.chessInstanceId = observation.roster[selected].instanceId;
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
        }
        else if (actionType == ChessActionType::SellChess)
        {
            const auto sold = std::ranges::find(
                result.events,
                ChessSemanticEventType::ChessSold,
                &ChessSemanticEvent::type);
            assert(sold != result.events.end());
            const auto* role = session_.content().role(sold->secondaryId);
            assert(role);
            showChessMessage(std::format("售出棋子{}，獲得${}", role->Name, sold->value));
        }
        if (actionType != ChessActionType::SellChess)
        {
            return;
        }
    }
}

void ChessGuiSessionAdapter::chooseDeployment()
{
    if (session_.state().roster.empty())
    {
        showChessMessage("目前沒有棋子");
        return;
    }
    for (;;)
    {
        const auto observation = session_.observe();
        std::set<int> selectedIds;
        for (const auto& piece : observation.roster)
        {
            if (piece.deployed)
            {
                selectedIds.insert(piece.instanceId);
            }
        }

        SessionMenuData data;
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<int> roleIds;
        std::vector<int> starRows;
        std::vector<int> instanceIds;
        std::vector<const ChessSessionPiece*> displayPieces;
        displayPieces.reserve(observation.roster.size());
        for (const auto& piece : observation.roster)
        {
            displayPieces.push_back(&piece);
        }
        std::ranges::sort(displayPieces, [&](const auto* lhs, const auto* rhs) {
            return std::pair{selectedIds.contains(lhs->instanceId) ? 0 : 1, lhs->instanceId}
                < std::pair{selectedIds.contains(rhs->instanceId) ? 0 : 1, rhs->instanceId};
        });
        for (const auto* piece : displayPieces)
        {
            const auto* role = session_.content().role(piece->roleId);
            assert(role);
            const bool deployed = selectedIds.contains(piece->instanceId);
            labelRows.push_back({
                deployed ? "[戰]" : "",
                role->Name,
                chessStars(piece->star),
                std::format("${}", ChessManagementRules::pieceValue(session_.content(), piece->roleId, piece->star)),
            });
            data.colors.push_back(deployed ? ChessUiStyle::gold : chessPieceTierColor(role->Cost));
            roleIds.push_back(piece->roleId);
            starRows.push_back(piece->star);
            instanceIds.push_back(piece->instanceId);
        }
        data.labels = alignedMenuLabels(labelRows);
        data.cards = roleCards(session_, roleIds, starRows, instanceIds);
        data.actionLabel = "切換出戰";

        const int selected = runSelection(std::format(
                "選擇出戰棋子 {}/{} 背包{}/{}",
                selectedIds.size(),
                observation.maximumDeployment,
                observation.roster.size() - selectedIds.size(),
                session_.content().balance().benchSize), data, {}, false);
        if (selected < 0)
        {
            return;
        }
        const int instanceId = displayPieces[selected]->instanceId;
        if (!selectedIds.erase(instanceId))
        {
            if (selectedIds.size() >= static_cast<std::size_t>(observation.maximumDeployment))
            {
                showChessMessage(std::format("最多只能出戰 {} 名棋子", observation.maximumDeployment));
                continue;
            }
            selectedIds.insert(instanceId);
        }
        ChessAction action;
        action.type = ChessActionType::SetDeployment;
        action.chessInstanceIds.assign(selectedIds.begin(), selectedIds.end());
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
        }
    }
}

void ChessGuiSessionAdapter::showBanManagement()
{
    const int maximum = ChessManagementRules::maximumBanCount(session_.state(), session_.content());
    if (maximum <= 0)
    {
        showChessMessage("禁棋功能尚未開放");
        return;
    }

    for (;;)
    {
        const auto observation = session_.observe();
        if (observation.seenRoles.empty())
        {
            showChessMessage("尚未遇到可禁棋子");
            return;
        }

        const auto rows = buildChessBanManagementRows(observation, session_.content());
        SessionMenuData data;
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<int> roleIds;
        for (const auto& row : rows)
        {
            const auto* role = session_.content().role(row.roleId);
            assert(role);
            labelRows.push_back({
                row.banned ? "[已禁] " : "[未禁] ",
                role->Name,
                {},
                std::format("${}", ChessManagementRules::pieceValue(session_.content(), row.roleId, 1)),
            });
            data.colors.push_back(chessPieceTierColor(role->Cost));
            roleIds.push_back(row.roleId);
        }
        data.labels = alignedMenuLabels(labelRows);
        data.cards = roleCards(session_, roleIds);
        data.actionLabel = "封棋";
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
            data.cards[i].subtitle = rows[i].banned ? "已封棋" : "可封棋";

        const int bannedCount = static_cast<int>(observation.bans.size());
        const int choice = runSelection(std::format(
                "禁棋管理　已禁{}/{}　剩餘{}",
                bannedCount,
                maximum,
                std::max(0, maximum - bannedCount)), data, {}, false);
        if (choice < 0)
        {
            return;
        }
        if (rows[choice].banned)
        {
            continue;
        }
        if (bannedCount >= maximum)
        {
            showChessMessage(std::format("禁棋已滿 {}/{}", bannedCount, maximum));
            continue;
        }

        ChessAction action;
        action.type = ChessActionType::AddBan;
        action.roleId = rows[choice].roleId;
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
            continue;
        }
        const auto* role = session_.content().role(action.roleId);
        assert(role);
        showChessMessage(std::format("禁棋：{}", role->Name));
    }
}

bool ChessGuiSessionAdapter::chooseBan(const ChessLegalActionDescriptor& descriptor)
{
    const bool forced = session_.state().phase == ChessSessionPhase::RewardChoice
        && !session_.state().pendingRewards.empty()
        && session_.state().pendingRewards.front().kind == ChessRewardKind::ForcedBan;
    SessionMenuData data;
    std::vector<ChessMenuColumnRow> labelRows;
    std::vector<int> roleIds;
    for (const int roleId : descriptor.candidateIds)
    {
        const auto* role = session_.content().role(roleId);
        assert(role);
        labelRows.push_back({
            forced ? "" : "[未禁] ",
            role->Name,
            {},
            std::format("${}", ChessManagementRules::pieceValue(session_.content(), roleId, 1)),
        });
        data.colors.push_back(chessPieceTierColor(role->Cost));
        roleIds.push_back(roleId);
    }
    data.labels = alignedMenuLabels(labelRows);
    data.cards = roleCards(session_, roleIds);
    data.actionLabel = "封棋";
    const std::string title = forced
        ? std::format(
            "禁棋選擇 (≤{}費)　剩餘{}/{}",
            session_.state().pendingRewards.front().eligibleTiers.front(),
            session_.state().pendingRewards.front().parameter,
            session_.state().pendingRewards.front().choiceCount)
        : std::format(
            "禁棋管理　已禁{}/{}　剩餘{}",
            session_.state().bannedRoleIds.size(),
            ChessManagementRules::maximumBanCount(session_.state(), session_.content()),
            std::max(0, ChessManagementRules::maximumBanCount(session_.state(), session_.content())
                - static_cast<int>(session_.state().bannedRoleIds.size())));
    std::function<bool()> exitConfirmation;
    if (forced)
    {
        const int remaining = session_.state().pendingRewards.front().parameter;
        exitConfirmation = [remaining]() {
            return runChessChoiceMenu("放棄本次禁棋機會？", {
                std::format("確認放棄剩餘{}次（不會保留）", remaining), "繼續選擇"},
                ChessChoiceStyle::DestructiveConfirmation) == 0;
        };
    }
    const int choice = runSelection(title, data, {}, false, true, std::move(exitConfirmation));
    if (choice < 0)
    {
        return false;
    }
    ChessAction action;
    action.type = ChessActionType::AddBan;
    action.roleId = descriptor.candidateIds[choice];
    const auto result = submitGuiAction(action);
    if (!result.accepted)
    {
        showChessMessage(result.description);
    }
    else
    {
        const auto* role = session_.content().role(action.roleId);
        assert(role);
        showChessMessage(std::format("禁棋：{}", role->Name));
    }
    return true;
}

void ChessGuiSessionAdapter::showEquipmentMenu()
{
    const auto& shop = session_.content().balance().legendaryShop;
    const bool legendaryUnlocked = session_.content().balance().talent(session_.state().talent).legendaryShop && shop.unlockFight > 0 && session_.state().fight >= shop.unlockFight;
    if (!legendaryUnlocked)
    {
        showEquipmentInventory();
        return;
    }
    for (;;)
    {
        const auto items = buildChessEquipmentMenu(legendaryUnlocked);
        SessionMenuData data;
        std::vector<ChessMenuColumnRow> labelRows;
        for (const auto& item : items)
        {
            labelRows.push_back({
                {},
                item.label,
                {},
                item.action == ChessContextMenuAction::BuyLegendaryEquipment
                    ? std::format("${}", session_.content().balance().legendaryShop.price)
                    : std::string{},
            });
            data.colors.push_back(item.action == ChessContextMenuAction::BuyLegendaryEquipment
                ? Color{255, 140, 255, 255}
                : ChessUiStyle::ink);
        }
        data.labels = alignedMenuLabels(labelRows);
        const int selected = runSelection(std::format("裝備管理　${}", session_.state().money), data, {}, false);
        if (selected < 0)
        {
            return;
        }
        switch (items[selected].action)
        {
        case ChessContextMenuAction::ShowEquipmentInventory:
            showEquipmentInventory();
            return;
        case ChessContextMenuAction::BuyLegendaryEquipment:
        {
            ChessLegalActionDescriptor legendary{ChessActionType::BuyLegendaryEquipment};
            for (const auto& equipment : session_.content().equipment())
            {
                if (equipment.tier == 4)
                {
                    legendary.candidateIds.push_back(equipment.itemId);
                }
            }
            chooseLegendary(legendary);
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return;
            }
            break;
        }
        default:
            assert(false);
            return;
        }
    }
}

void ChessGuiSessionAdapter::showEquipmentInventory()
{
    struct Row
    {
        const EquipmentDef* definition{};
        int instanceId = -1;
        bool owned{};
    };
    for (;;)
    {
        SessionMenuData data;
        std::vector<Row> rows;
        for (const auto& definition : session_.content().equipment())
        {
            std::vector<const ChessEquipmentInstance*> instances;
            for (const auto& [instanceId, instance] : session_.state().equipmentInventory)
            {
                if (instance.itemId == definition.itemId)
                {
                    instances.push_back(&instance);
                }
            }
            if (instances.empty())
            {
                rows.push_back({&definition, -1, false});
                continue;
            }
            for (const auto* instance : instances)
            {
                rows.push_back({&definition, instance->instanceId, true});
            }
        }

        std::stable_sort(rows.begin(), rows.end(), [](const Row& left, const Row& right) {
            return chessEquipmentMenuOrderLess(
                {left.owned, left.definition->tier, left.definition->itemId},
                {right.owned, right.definition->tier, right.definition->itemId});
        });

        std::vector<ChessMenuColumnRow> labelRows;
        for (const auto& row : rows)
        {
            const auto* item = session_.content().item(row.definition->itemId);
            const std::string name = item ? item->name : std::format("裝備 {}", row.definition->itemId);
            const auto* instance = equipmentInstance(session_.state(), row.instanceId);
            labelRows.push_back({
                chessMenuPrefixWithSeparator(std::format("[{}]", chessRewardTierLabel(row.definition->tier))),
                name,
                chessEquipmentAssignmentColumn(instance && instance->assignedChessInstanceId >= 0),
                {},
            });
            data.colors.push_back(row.owned ? ChessUiStyle::positive : Color{120, 120, 120, 255});
            auto card = equipmentCard(session_, *row.definition);
            card.subtitle = !row.owned ? "未持有" : instance && instance->assignedChessInstanceId >= 0 ? "已裝備" : "行囊";
            data.cards.push_back(std::move(card));
        }
        data.labels = contentSizedAlignedMenuLabels(labelRows);
        const int selected = runSelection("裝備一覽", data);
        if (selected < 0)
        {
            return;
        }
        if (rows[selected].instanceId < 0)
        {
            continue;
        }
        const auto descriptor = legalAction(session_, ChessActionType::Equip);
        if (!descriptor)
        {
            showChessMessage("目前沒有可裝備的棋子");
            continue;
        }
        ChessLegalActionDescriptor selectedEquipment = *descriptor;
        selectedEquipment.candidateIds = {rows[selected].instanceId};
        chooseEquipment(selectedEquipment);
    }
}

void ChessGuiSessionAdapter::chooseEquipment(const ChessLegalActionDescriptor& descriptor)
{
    assert(descriptor.candidateIds.size() == 1 || !descriptor.candidateIds.empty());
    int equipmentId = descriptor.candidateIds.front();
    if (descriptor.candidateIds.size() > 1)
    {
        SessionMenuData equipmentData;
        std::vector<ChessMenuColumnRow> equipmentRows;
        for (const int id : descriptor.candidateIds)
        {
            const auto* instance = equipmentInstance(session_.state(), id);
            assert(instance);
            const auto* item = session_.content().item(instance->itemId);
            equipmentRows.push_back({
                {},
                item ? item->name : std::format("裝備 {}", instance->itemId),
                chessEquipmentAssignmentColumn(instance->assignedChessInstanceId >= 0),
                {},
            });
            equipmentData.colors.push_back(ChessUiStyle::positive);
            equipmentData.cards.push_back(equipmentCard(session_, *equipmentDefinition(session_.content(), instance->itemId)));
        }
        equipmentData.labels = alignedMenuLabels(equipmentRows);
        const int choice = runSelection("選擇裝備", equipmentData);
        if (choice < 0)
        {
            return;
        }
        equipmentId = descriptor.candidateIds[choice];
    }

    const auto observation = session_.observe();
    SessionMenuData pieceData;
    std::vector<ChessMenuColumnRow> pieceRows;
    std::vector<int> roleIds;
    std::vector<int> starRows;
    std::vector<int> instanceIds;
    for (const auto& piece : observation.roster)
    {
        const auto* role = session_.content().role(piece.roleId);
        assert(role);
        pieceRows.push_back({
            piece.deployed ? "[戰]" : "",
            role->Name,
            chessStars(piece.star),
            std::format("${}", ChessManagementRules::pieceValue(session_.content(), piece.roleId, piece.star)),
        });
        pieceData.colors.push_back(chessPieceTierColor(role->Cost));
        roleIds.push_back(piece.roleId);
        starRows.push_back(piece.star);
        instanceIds.push_back(piece.instanceId);
    }
    pieceData.labels = alignedMenuLabels(pieceRows);
    pieceData.cards = roleCards(session_, roleIds, starRows, instanceIds);
    pieceData.actionLabel = "裝備";
    const int selected = runSelection("選擇棋子", pieceData, {}, false);
    if (selected < 0)
    {
        return;
    }
    ChessAction action;
    action.type = ChessActionType::Equip;
    action.equipmentInstanceId = equipmentId;
    action.targetChessInstanceId = observation.roster[selected].instanceId;
    const auto result = submitGuiAction(action);
    if (!result.accepted)
    {
        showChessMessage(result.description);
    }
}

void ChessGuiSessionAdapter::chooseLegendary(const ChessLegalActionDescriptor& descriptor)
{
    for (;;)
    {
        SessionMenuData data;
        data.shopMode = true;
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<const EquipmentDef*> definitions;
        for (const int itemId : descriptor.candidateIds)
        {
            const auto* definition = equipmentDefinition(session_.content(), itemId);
            assert(definition);
            const auto* item = session_.content().item(itemId);
            labelRows.push_back({
                chessMenuPrefixWithSeparator("[傳說]"),
                item ? item->name : "未知裝備",
                {},
                std::format("${}", session_.content().balance().legendaryShop.price),
            });
            data.colors.push_back({255, 100, 255, 255});
            definitions.push_back(definition);
            auto card = equipmentCard(session_, *definition);
            card.subtitle = std::format("{} 金", session_.content().balance().legendaryShop.price);
            data.cards.push_back(std::move(card));
        }
        data.labels = alignedMenuLabels(labelRows);
        const int choice = runSelection(std::format("神兵商店　${}", session_.state().money), data);
        if (choice < 0)
        {
            return;
        }
        ChessAction action;
        action.type = ChessActionType::BuyLegendaryEquipment;
        action.itemId = descriptor.candidateIds[choice];
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage("金錢不足，無法購買神兵");
        }
        else
        {
            const auto* item = session_.content().item(action.itemId);
            showChessMessage(std::format(
                "消費${}，獲得神兵：{}",
                session_.content().balance().legendaryShop.price,
                item ? item->name : "???"));
        }
    }
}

void ChessGuiSessionAdapter::showOverviewMenu()
{
    const auto items = buildChessOverviewMenu();
    const auto action = runContextActionMenu("棋局圖鑑", items);
    if (!action)
    {
        return;
    }
    switch (*action)
    {
    case ChessContextMenuAction::ShowPositionSwap: showPositionSwap(); break;
    case ChessContextMenuAction::RerollBattleSeed: showEnemyReroll(); break;
    case ChessContextMenuAction::ViewCombos: viewCombos(); break;
    case ChessContextMenuAction::ViewChessPool: viewChessPool(); break;
    case ChessContextMenuAction::ViewNeigong: viewNeigong(); break;
    case ChessContextMenuAction::ViewEffects: viewEffects(); break;
    default: assert(false); break;
    }
}

void ChessGuiSessionAdapter::showSystemMenu()
{
    runSystemMenu();
}

void ChessGuiSessionAdapter::showOfflineBattle()
{
    if (session_.state().phase != ChessSessionPhase::Management)
    {
        showChessMessage("離線對戰只能在整備階段使用");
        return;
    }
    if (session_.content().difficulty() != Difficulty::Hard)
    {
        showChessMessage("離線對戰只能使用困難模式進度");
        return;
    }

    std::optional<ChessPvpComposition> opponent;
    std::uint32_t battleSeed = nextPvpBattleSeed();
    for (;;)
    {
        assert(ChessManagementRules::formationIsValid(
            session_.state(),
            session_.state().formationSlots));
        auto local = extractChessPvpComposition(session_);
        const bool canStart = !local.pieces.empty()
            && opponent
            && ChessManagementRules::formationIsValid(
                session_.state(),
                local.formationSlots);

        std::vector<OfflinePvpAction> actionIds;
        std::vector<std::string> labels;
        const auto addAction = [&](OfflinePvpAction action, std::string label) {
            actionIds.push_back(action);
            labels.push_back(std::move(label));
        };
        addAction(OfflinePvpAction::Export, "匯出對戰檢查點");
        addAction(OfflinePvpAction::Import, opponent ? "匯入/替換對手" : "匯入對手存檔");
        if (canStart)
        {
            addAction(OfflinePvpAction::Start, "對戰");
        }
        addAction(OfflinePvpAction::Back, "返回");

        auto screen = std::make_shared<ChessOfflineBattleScreenNode>(
            local,
            opponent,
            session_,
            [this](const ChessAction& action) {
                return submitGuiAction(action);
            },
            battleSeed,
            std::move(actionIds),
            std::move(labels));
        screen->run();
        battleSeed = screen->battleSeed();
        const auto selected = screen->selectedAction();
        if (!selected || *selected == OfflinePvpAction::Back)
        {
            return;
        }

        switch (*selected)
        {
        case OfflinePvpAction::Import:
            opponent = screen->takeOpponent();
            break;
        case OfflinePvpAction::Start:
        {
            assert(opponent && canStart);
            local = extractChessPvpComposition(session_);
            ChessStandaloneBattleRequest request;
            request.stableBattleId = "offline_pvp";
            request.rootSeed = battleSeed;
            request.battleSeed = battleSeed;
            request.mapId = ChessPvpMapLayout::BattleId;
            request.layout = PreparedChessBattleLayout::PvpArena;
            request.options = session_.state().options;
            request.teams[0] = chessStandaloneBattleTeam(local);
            request.teams[1] = chessStandaloneBattleTeam(*opponent);

            std::string error;
            auto build = ChessStandaloneBattle::prepare(
                session_.sharedContent(),
                request,
                error);
            if (!build)
            {
                showChessMessage(error);
                break;
            }
            const auto campaignState = session_.state();
            const auto campaignRandom = session_.random().state();
            const auto campaignReplaySize = session_.journal().decisions().size();
            auto battleSession = std::move(*build).createSession();
            if (ChessGuiSessionAdapter(*battleSession).runPreparedBattle()
                == ChessGuiFlowResult::Aborted)
            {
                return;
            }
            assert(session_.state() == campaignState);
            assert(session_.random().state() == campaignRandom);
            assert(session_.journal().decisions().size() == campaignReplaySize);
            break;
        }
        case OfflinePvpAction::Export:
        case OfflinePvpAction::Back:
            std::unreachable();
        }
    }
}

bool ChessGuiSessionAdapter::runSystemMenu()
{
    for (;;)
    {
        const auto action = runContextActionMenu("系統選單", buildChessSystemMenu());
        if (!action)
        {
            return false;
        }
        switch (*action)
        {
        case ChessContextMenuAction::LoadProgress:
            if (runSaveSlotMenu(0) >= 0)
            {
                return true;
            }
            break;
        case ChessContextMenuAction::SaveProgress:
            runSaveSlotMenu(1);
            break;
        case ChessContextMenuAction::ShowSystemSettings:
            showSystemSettings();
            break;
        case ChessContextMenuAction::ShowGameGuide:
            showGameGuide();
            break;
        case ChessContextMenuAction::ReturnToTitle:
            if (confirmReturnToTitle())
            {
                RunNode::exitAll(1);
                Event::getInstance()->forceExit();
                return true;
            }
            break;
        default:
            assert(false);
            break;
        }
    }
}

void ChessGuiSessionAdapter::showPositionSwap()
{
    const bool currentEnabled = session_.state().options.positionSwapEnabled;
    auto menu = std::make_shared<ChessSelectionScreen>(
        std::format("戰前佈陣 · 目前{}", currentEnabled ? "開啟" : "關閉"),
        std::vector<ChessSelectionItem>{
            {.title = "關閉戰前佈陣", .image = chessSelectionIcon(ChessUiStyle::Icon::Close)},
            {.title = "開啟戰前佈陣", .image = chessSelectionIcon(ChessUiStyle::Icon::Formation)},
        });
    menu->setCommandMode(true);
    const int selected = menu->run();
    if (selected == 0 || selected == 1)
    {
        ChessAction action;
        action.type = ChessActionType::SetPositionSwapEnabled;
        action.value = selected == 1;
        submitGuiAction(action);
    }
}

void ChessGuiSessionAdapter::showEnemyReroll()
{
    const int cost = session_.content().balance().enemyRerollCost;
    if (runChessChoiceMenu(
        "逆天改命 · 重抽下場敵軍，地圖可能不變",
        {std::format("花費 ${} 重抽", cost), "取消"}, ChessChoiceStyle::Confirmation) == 0)
    {
        ChessAction action;
        action.type = ChessActionType::RerollEnemySeed;
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(std::format("金幣不足，需要${}！", cost));
        }
        else
        {
            showChessMessage(std::format(
                "花費${}逆天改命！下場戰鬥的敵人陣容和隨機種子已改變（地圖不保證改變）",
                cost));
        }
    }
}

void ChessGuiSessionAdapter::viewCombos()
{
    showChessComboOverview(session_.content(), session_.state());
}

void ChessGuiSessionAdapter::viewChessPool()
{
    SessionMenuData data;
    data.browseOnly = true;
    std::vector<ChessMenuColumnRow> labelRows;
    std::vector<int> roleIds;
    std::set<int> owned;
    for (const auto& [instanceId, piece] : session_.state().roster)
    {
        owned.insert(piece.roleId);
    }
    for (const int roleId : buildChessPoolBrowseOrder(session_.content()))
    {
        const auto* role = session_.content().role(roleId);
        assert(role);
        labelRows.push_back({
            chessMenuPrefixWithSeparator(owned.contains(roleId) ? "[✓]" : "[ ]"),
            role->Name,
            {},
            std::format("${}", ChessManagementRules::pieceValue(session_.content(), roleId, 1)),
        });
        data.colors.push_back(chessPieceTierColor(role->Cost));
        roleIds.push_back(roleId);
        auto card = roleCard(session_, roleId);
        card.subtitle = owned.contains(roleId) ? "已招募 · 圖鑑預覽" : "圖鑑預覽";
        data.cards.push_back(std::move(card));
    }
    data.labels = alignedMenuLabels(labelRows);
    runSelection("棋子一覽", data, {}, false);
}

void ChessGuiSessionAdapter::viewNeigong()
{
    SessionMenuData data;
    data.browseOnly = true;
    std::vector<ChessMenuColumnRow> labelRows;
    std::vector<const NeigongDef*> rows;
    for (const auto& neigong : session_.content().neigong())
    {
        const bool owned = session_.state().obtainedNeigongIds.contains(neigong.magicId);
        labelRows.push_back({
            chessMenuPrefixWithSeparator(std::format("[{}]", chessRewardTierLabel(neigong.tier))),
            neigong.name,
            owned ? " ✓" : "",
            {},
        });
        data.colors.push_back(owned ? ChessUiStyle::positive : Color{120, 120, 120, 255});
        rows.push_back(&neigong);
        data.cards.push_back(neigongCard(session_, neigong));
    }
    data.labels = alignedMenuLabels(labelRows);
    runSelection("內功一覽", data);
}

void ChessGuiSessionAdapter::viewEffects()
{
    auto entries = chessEffectCatalog(session_.content());
    SessionMenuData data;
    data.browseOnly = true;
    std::vector<ChessMenuColumnRow> labelRows;
    labelRows.reserve(entries.size());
    for (const auto& entry : entries)
    {
        labelRows.push_back({
            chessMenuPrefixWithSeparator(std::format(
                "[{}]",
                chessEffectCatalogSourceLabel(entry.source))),
            entry.sourceName,
            std::format(" {}/{}", entry.ruleOrdinal, entry.sourceRuleCount),
            {},
        });
        data.colors.push_back(effectCatalogSourceColor(entry.source));
        data.cards.push_back({
            .title = entry.sourceName,
            .subtitle = std::format("效果 {} / {}", entry.ruleOrdinal, entry.sourceRuleCount),
            .badge = chessEffectCatalogSourceLabel(entry.source),
            .image = chessSelectionIcon(ChessUiStyle::Icon::Spark),
            .accent = effectCatalogSourceColor(entry.source),
            .category = chessEffectCatalogSourceLabel(entry.source),
            .summary = joinEffectDescriptionRows(entry.effects, " · "),
            .description = entry.sourceContext + "\n\n" + joinEffectDescriptionRows(entry.effects),
        });
    }
    data.labels = contentSizedAlignedMenuLabels(labelRows);
    runSelection("效果全覽", data);
}

void ChessGuiSessionAdapter::showGameGuide()
{
    const auto sections = buildChessGameGuideSections(session_.content(), session_.state().talent);
    SessionMenuData data;
    data.browseOnly = true;
    for (const auto& section : sections)
    {
        data.labels.push_back(section.title);
        data.cards.push_back({.title = section.title, .image = chessSelectionIcon(ChessUiStyle::Icon::Book),
            .summary = section.lines.empty() ? std::string{} : section.lines.front().text});
        for (const auto& line : section.lines) data.cards.back().description += line.text + "\n\n";
    }
    runSelection("遊戲說明", data);
}

void ChessGuiSessionAdapter::showSystemSettings()
{
    const auto original = SystemSettings::getInstance()->snapshot();
    auto menu = std::make_shared<ChessSystemSettingsMenu>(original);
    if (menu->run() >= 0)
    {
        SystemSettings::getInstance()->update(menu->settings());
    }
    else
    {
        SystemSettings::getInstance()->update(original, false);
    }
}

ChessGuiFlowResult ChessGuiSessionAdapter::chooseChallenge(const ChessLegalActionDescriptor& descriptor)
{
    for (;;)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }

        SessionMenuData data;
        data.actionLabel = "挑戰";
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<const BalanceConfig::ChallengeDef*> rows;
        for (const auto& name : descriptor.candidateStableIds)
        {
            const auto found = std::ranges::find(
                session_.content().balance().challenges,
                name,
                &BalanceConfig::ChallengeDef::name);
            assert(found != session_.content().balance().challenges.end());
            const bool complete = session_.state().completedChallengeNames.contains(name);
            labelRows.push_back({
                chessMenuPrefixWithSeparator(chessChallengeCompletionLabel(complete)),
                found->name,
                {},
                {},
            });
            data.colors.push_back(chessChallengeMenuRowColor(complete));
            rows.push_back(&*found);
            ChessSelectionItem card{
                .title = found->name,
                .subtitle = complete ? "已完成" : "",
                .badge = std::format("{} 名對手", found->enemies.size()),
                .image = chessSelectionIcon(ChessUiStyle::Icon::Mountain),
                .accent = complete ? ChessUiStyle::positive : ChessUiStyle::gold,
                .category = complete ? "已完成" : "未完成",
                .facts = {{"對手", std::to_string(found->enemies.size())}, {"獎勵", "擇一"}},
                .summary = found->rewards.empty() ? "" : chessChallengeRewardDescription(session_.content(), found->rewards.front()),
                .relatedTitle = "敵方陣容",
            };
            for (const auto& enemy : found->enemies)
            {
                const auto* role = session_.content().role(enemy.roleId);
                assert(role);
                card.related.push_back({"head", role->HeadID});
            }
            if (!card.related.empty()) card.image = card.related.front();
            std::string enemies, rewards;
            for (const auto& enemy : found->enemies)
                enemies += chessChallengeEnemyDescription(session_.content(), enemy) + "\n\n";
            for (const auto& reward : found->rewards)
                rewards += chessChallengeRewardDescription(session_.content(), reward) + "\n\n";
            card.description = found->description;
            card.detailTabs = {{"概況", found->description}, {"敵陣", std::move(enemies)}, {"獎勵", "通關後擇一\n\n" + rewards}};
            data.cards.push_back(std::move(card));
        }
        data.labels = alignedMenuLabels(labelRows);
        const int choice = runSelection("遠征挑戰", data);
        if (choice < 0)
        {
            return currentChessGuiFlowResult();
        }

        ChessAction action;
        action.type = ChessActionType::StartChallenge;
        action.challengeName = descriptor.candidateStableIds[choice];
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.error == ChessRuleErrorCode::DeploymentRequired
                ? "請先選擇出戰棋子"
                : result.description);
            continue;
        }
        if (drainPreparedBattle() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
    }
}

ChessGuiFlowResult ChessGuiSessionAdapter::chooseReward(const ChessLegalActionDescriptor& descriptor)
{
    (void)descriptor;
    for (;;)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        assert(!session_.state().pendingRewards.empty());
        const auto pending = session_.state().pendingRewards.front();
        std::vector<ChessRewardOption> availableOptions;
        std::ranges::copy_if(
            pending.options,
            std::back_inserter(availableOptions),
            [&](const ChessRewardOption& option) {
                return option.goldCost <= session_.state().money;
            });
        SessionMenuData data;
        data.actionLabel = "領取";
        std::vector<ChessMenuColumnRow> labelRows;
        for (const auto& option : availableOptions)
        {
            if (option.kind == ChessRewardKind::Equipment)
            {
                const auto* definition = equipmentDefinition(session_.content(), option.value);
                assert(definition);
                const auto* item = session_.content().item(option.value);
                labelRows.push_back({
                    chessMenuPrefixWithSeparator(std::format("[{}]", chessRewardTierLabel(definition->tier))),
                    item ? item->name : "未知裝備",
                    {},
                    chessRewardOptionCostColumn(option),
                });
                data.colors.push_back(chessRewardTierColor(definition->tier));
                data.cards.push_back(equipmentCard(session_, *definition));
            }
            else if (option.kind == ChessRewardKind::InternalSkill)
            {
                const auto found = std::ranges::find(session_.content().neigong(), option.value, &NeigongDef::magicId);
                assert(found != session_.content().neigong().end());
                labelRows.push_back({
                    chessMenuPrefixWithSeparator(std::format("[{}]", chessRewardTierLabel(found->tier))),
                    found->name,
                    {},
                    chessRewardOptionCostColumn(option),
                });
                data.colors.push_back(chessRewardTierColor(found->tier));
                data.cards.push_back(neigongCard(session_, *found));
            }
            else if (option.kind == ChessRewardKind::Piece)
            {
                const auto* role = session_.content().role(option.value);
                assert(role);
                labelRows.push_back({{}, role->Name, chessStars(1), chessRewardOptionCostColumn(option)});
                data.colors.push_back(chessPieceTierColor(role->Cost));
                data.cards.push_back(roleCard(session_, role->ID));
            }
            else if (option.kind == ChessRewardKind::Synergy)
            {
                const auto combo = std::ranges::find(session_.content().combos(), option.value, &ComboDef::id);
                assert(combo != session_.content().combos().end());
                const auto& talent = session_.content().balance().talent(session_.state().talent);
                labelRows.push_back({{}, combo->name, std::format("+{}人", talent.selectedComboBonus), {}});
                data.colors.push_back({115, 220, 190, 255});
                data.cards.push_back(comboCard(session_, *combo));
            }
            else if (option.kind == ChessRewardKind::StarUpgrade)
            {
                const auto& piece = session_.state().roster.at(option.value);
                const auto* role = session_.content().role(piece.roleId);
                assert(role);
                labelRows.push_back({
                    {},
                    role->Name,
                    std::format("{}→{}", chessStars(piece.star), chessStars(option.value2)),
                    chessRewardOptionCostColumn(option),
                });
                data.colors.push_back(chessPieceTierColor(role->Cost));
                data.cards.push_back(roleCard(session_, piece.roleId, option.value2, piece.instanceId));
            }
            else
            {
                const auto challenge = std::ranges::find(
                    session_.content().balance().challenges,
                    pending.challengeName,
                    &BalanceConfig::ChallengeDef::name);
                assert(challenge != session_.content().balance().challenges.end());
                const auto reward = std::ranges::find_if(challenge->rewards, [&](const auto& candidate) {
                    return chessChallengeRewardDescription(session_.content(), candidate) == option.id;
                });
                assert(reward != challenge->rewards.end());
                labelRows.push_back({
                    {},
                    chessChallengeRewardDescription(session_.content(), *reward),
                    {},
                    chessRewardOptionCostColumn(option),
                });
                data.colors.push_back(ChessUiStyle::gold);
                ChessSelectionItem card{.image = chessSelectionIcon(ChessUiStyle::Icon::Spark), .summary = option.id};
                using Type = BalanceConfig::ChallengeRewardType;
                switch (reward->type)
                {
                case Type::Gold:
                    card.title = std::format("+{} 金幣", reward->value);
                    card.image = chessSelectionIcon(ChessUiStyle::Icon::Coin);
                    break;
                case Type::GetPiece:
                    card.title = "招募棋子";
                    card.badge = std::format("最高{}費", reward->value);
                    card.image = chessSelectionIcon(ChessUiStyle::Icon::Character);
                    break;
                case Type::GetNeigong:
                    card.title = "獲得內功";
                    card.badge = std::format("最高{}階", reward->value);
                    card.image = chessSelectionIcon(ChessUiStyle::Icon::Cultivation);
                    break;
                case Type::StarUp1to2:
                case Type::StarUp2to3:
                    card.title = reward->type == Type::StarUp1to2 ? "★ → ★★" : "★★ → ★★★";
                    card.badge = std::format("最高{}費", reward->value);
                    card.image = chessSelectionIcon(ChessUiStyle::Icon::Up);
                    break;
                case Type::GetEquipment:
                    card.title = "獲得裝備";
                    card.badge = std::format("最高{}階", reward->value);
                    card.image = chessSelectionIcon(ChessUiStyle::Icon::Bag);
                    break;
                case Type::GetSpecificEquipment:
                    card.title = chessItemDisplayName(session_.content(), reward->value);
                    card.image = {"item", reward->value};
                    break;
                }
                data.cards.push_back(std::move(card));
            }
            if (option.goldCost > 0) data.cards.back().subtitle = std::format("{} 金", option.goldCost);
        }
        data.labels = buildAlignedChessRewardMenuLabels(
            pending.kind,
            labelRows,
            menuDisplayWidth);
        data.shortChoice = pending.kind == ChessRewardKind::ChallengeReward
            && availableOptions.size() <= 3;
        std::string title = "選擇獎勵";
        if (pending.kind == ChessRewardKind::Equipment) title = "選擇裝備獎勵";
        else if (pending.kind == ChessRewardKind::InternalSkill) title = "內功對所有成員自動生效";
        else if (pending.kind == ChessRewardKind::Piece) title = "選擇棋子";
        else if (pending.kind == ChessRewardKind::Synergy) title = "合縱：選擇永久加點羈絆";
        else if (pending.kind == ChessRewardKind::StarUpgrade)
            title = chessStarUpgradeRewardTitle(session_.state(), pending);
        const int choice = runSelection(title, data, {}, false, false);
        if (choice < 0)
        {
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
            continue;
        }
        const auto selected = availableOptions[choice];
        ChessAction action;
        action.type = ChessActionType::ChooseReward;
        action.rewardId = selected.id;
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
            continue;
        }
        if (pending.kind == ChessRewardKind::Equipment)
        {
            const auto* item = session_.content().item(selected.value);
            showChessMessage(std::format("獲得裝備：{}", item ? item->name : "???"));
        }
        else if (pending.kind == ChessRewardKind::InternalSkill)
        {
            const auto found = std::ranges::find(session_.content().neigong(), selected.value, &NeigongDef::magicId);
            assert(found != session_.content().neigong().end());
            showChessMessage(std::format("獲得內功：{}", found->name));
        }
        else if (pending.kind == ChessRewardKind::Piece)
        {
            const auto* role = session_.content().role(selected.value);
            assert(role);
            const bool merged = std::ranges::contains(result.events, ChessSemanticEventType::ChessMerged, &ChessSemanticEvent::type);
            showChessMessage(merged ? std::format("{}升星！", role->Name) : std::format("獲得棋子：{}", role->Name));
            if (merged)
            {
                playChessUpgradeSound();
            }
        }
        else if (pending.kind == ChessRewardKind::StarUpgrade)
        {
            const auto merged = std::ranges::find(result.events, ChessSemanticEventType::ChessMerged, &ChessSemanticEvent::type);
            assert(merged != result.events.end());
            const auto* role = session_.content().role(merged->secondaryId);
            assert(role);
            showChessMessage(std::format("{}升星至{}★！", role->Name, selected.value2));
        }
        else if (pending.kind == ChessRewardKind::ChallengeReward)
        {
            using RT = BalanceConfig::ChallengeRewardType;
            const auto type = static_cast<RT>(selected.value);
            if (type == RT::Gold)
            {
                showChessMessage(std::format("獲得{}金幣！", selected.value2));
            }
            else if (type == RT::GetSpecificEquipment)
            {
                const auto* item = session_.content().item(selected.value2);
                showChessMessage(std::format("獲得裝備：{}", item ? item->name : "???"));
            }
        }
        return currentChessGuiFlowResult();
    }
}

void ChessGuiSessionAdapter::chooseMap(const ChessLegalActionDescriptor& descriptor)
{
    SessionMenuData data;
    data.actionLabel = "前往戰場";
    for (const int mapId : descriptor.candidateIds)
    {
        data.labels.push_back(chessBattleMapDisplayName(session_.content(), mapId));
        data.colors.push_back({230, 230, 220, 255});
        const auto* map = ChessBattleMapCatalog::find(mapId);
        data.cards.push_back({.title = chessBattleMapDisplayName(session_.content(), mapId),
            .image = chessSelectionIcon(ChessUiStyle::Icon::Mountain),
            .facts = map ? std::vector<std::pair<std::string, std::string>>{{"我方上限", std::to_string(map->teammatePositions.size())},
                {"敵方上限", std::to_string(map->enemyCapacity)}} : std::vector<std::pair<std::string, std::string>>{}});
    }
    const int choice = runSelection("選擇戰場", data, makeBattleMapPreviewPanel(session_, descriptor.candidateIds), false, false);
    if (choice >= 0)
    {
        ChessAction action;
        action.type = ChessActionType::ChooseMap;
        action.mapId = descriptor.candidateIds[choice];
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
        }
    }
}

void ChessGuiSessionAdapter::chooseSwap(const ChessLegalActionDescriptor& descriptor)
{
    SessionMenuData data;
    std::vector<ChessMenuColumnRow> labelRows;
    std::vector<int> roleIds;
    std::vector<int> starRows;
    for (const int unitId : descriptor.candidateIds)
    {
        const auto& prepared = *session_.state().preparedBattle;
        const auto found = std::ranges::find(prepared.units, unitId, &PreparedChessBattleUnit::unitId);
        assert(found != prepared.units.end());
        const auto* role = session_.content().role(found->roleId);
        labelRows.push_back({{}, role->Name, chessStars(found->star), {}});
        data.colors.push_back(chessPieceTierColor(role->Cost));
        roleIds.push_back(found->roleId);
        starRows.push_back(found->star);
    }
    data.labels = alignedMenuLabels(labelRows);
    data.cards = roleCards(session_, roleIds, starRows);
    const int first = runSelection("第一個位置", data);
    if (first < 0) return;
    const int second = runSelection("第二個位置", data);
    if (second < 0) return;
    ChessAction action;
    action.type = ChessActionType::SwapPositions;
    action.chessInstanceId = descriptor.candidateIds[first];
    action.targetChessInstanceId = descriptor.candidateIds[second];
    const auto result = submitGuiAction(action);
    if (!result.accepted)
    {
        showChessMessage(result.description);
    }
}

void ChessGuiSessionAdapter::showBattlePreview() const
{
    if (!session_.state().preparedBattle) return;
    auto preview = makeBattlePreviewPresentation(
        session_,
        *session_.state().preparedBattle);
    auto preloadPreview = preview;
    runBattleInformationPanel(
        preview,
        [this, preview = std::move(preloadPreview)] {
            preloadPreparedBattlePresentation(session_, preview);
        });
}

ChessGuiFlowResult ChessGuiSessionAdapter::drainPreparedBattle()
{
    while (session_.state().phase == ChessSessionPhase::BattlePreparation)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        const auto legal = session_.legalActions();
        if (const auto map = std::ranges::find(legal, ChessActionType::ChooseMap, &ChessLegalActionDescriptor::type); map != legal.end())
        {
            chooseMap(*map);
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
            continue;
        }
        showBattlePreview();
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        assert(session_.state().preparedBattle);
        const auto prepared = *session_.state().preparedBattle;
        const bool repeatedChallenge = prepared.kind == PreparedChessBattleKind::Challenge
            && session_.state().completedChallengeNames.contains(prepared.stableBattleId);
        const int oldLevel = session_.state().level;
        auto battle = std::make_shared<BattleSceneHades>(session_);
        battle->setNoExp();
        battle->run();
        if (!chessGuiBattleReadyForPostBattle(
                battle->completedSceneRun(),
                battle->completedActionResult()))
        {
            return ChessGuiFlowResult::Aborted;
        }
        assert(!session_.transitionPending());
        auto stats = std::make_shared<BattleStatsView>();
        stats->setupPostBattle(
            battle->makePostBattleSummary(),
            battle->getBattleReport(),
            session_.content());
        stats->run();
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        const auto actionResult = *battle->completedActionResult();
        stats.reset();
        battle.reset();
        if (prepared.kind == PreparedChessBattleKind::Standalone)
        {
            continue;
        }
        if (session_.state().lastBattleOutcome != Battle::BattleOutcome::PlayerVictory)
        {
            if (session_.state().lastBattleOutcome == Battle::BattleOutcome::Timeout)
            {
                showChessMessage(std::format(
                    "{}。請調整陣容後再試",
                    chessBattleOutcomeDescription(session_.state().lastBattleOutcome)));
            }
            else
            {
                showChessMessage(prepared.kind == PreparedChessBattleKind::Challenge
                    ? "挑戰失敗！請調整陣容後再試"
                    : "戰鬥失敗！請調整陣容後再試");
            }
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
            continue;
        }
        if (prepared.kind == PreparedChessBattleKind::Challenge)
        {
            if (repeatedChallenge)
            {
                showChessMessage("挑戰勝利！(已領取過獎勵)");
                if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
                {
                    return ChessGuiFlowResult::Aborted;
                }
            }
            else if (drainRewards() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
            continue;
        }

        if (drainRewards(true) == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        showCampaignVictoryFeedback(session_, actionResult, oldLevel);
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        const auto& legendaryShop = session_.content().balance().legendaryShop;
        if (session_.content().balance().talent(session_.state().talent).legendaryShop && legendaryShop.unlockFight > 0 && session_.state().fight == legendaryShop.unlockFight)
        {
            auto unlockTalk = std::make_shared<Talk>(
                std::format(
                    "少俠既已闖過第{}關，老夫便為你開啟神兵商店。此後可於「裝備管理」中前往「神兵商店」，花費${}購買傳說裝備。",
                    legendaryShop.unlockFight,
                    legendaryShop.price),
                116);
            unlockTalk->run();
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
        }
        if (drainRewards() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        if (session_.state().campaignComplete)
        {
            auto outro = std::make_shared<Talk>(
                "少俠果然不凡！珍瓏棋局已破。若有興趣，可嘗試「遠征挑戰」，那裡有更強的對手等著你。"
                "當然，你也可以嘗試其他羈絆組合，探索更多可能。",
                115);
            outro->run();
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
        }
        autoSavePersistableGuiState(session_);
    }
    return drainRewards();
}

ChessGuiFlowResult ChessGuiSessionAdapter::drainRewards(bool stopBeforeForcedBan)
{
    while (session_.state().phase == ChessSessionPhase::RewardChoice)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        if (stopBeforeForcedBan
            && session_.state().pendingRewards.front().kind == ChessRewardKind::ForcedBan)
        {
            return ChessGuiFlowResult::Continue;
        }
        const auto legal = session_.legalActions();
        const auto ban = std::ranges::find(legal, ChessActionType::AddBan, &ChessLegalActionDescriptor::type);
        if (ban != legal.end())
        {
            const bool selected = chooseBan(*ban);
            if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
            if (!selected)
            {
                ChessAction skip;
                skip.type = ChessActionType::SkipForcedBans;
                const auto result = submitGuiAction(skip);
                assert(result.accepted);
            }
            continue;
        }
        const auto choose = std::ranges::find(legal, ChessActionType::ChooseReward, &ChessLegalActionDescriptor::type);
        if (choose != legal.end())
        {
            if (chooseReward(*choose) == ChessGuiFlowResult::Aborted)
            {
                return ChessGuiFlowResult::Aborted;
            }
        }
        else
        {
            break;
        }
    }
    return currentChessGuiFlowResult();
}

ChessGuiFlowResult ChessGuiSessionAdapter::chooseAndSubmit(const ChessLegalActionDescriptor& descriptor)
{
    ChessAction action;
    action.type = descriptor.type;
    switch (descriptor.type)
    {
    case ChessActionType::BuyShopSlot: showShop(); return currentChessGuiFlowResult();
    case ChessActionType::SellChess: chooseChess(descriptor.type); return currentChessGuiFlowResult();
    case ChessActionType::SetDeployment: chooseDeployment(); return currentChessGuiFlowResult();
    case ChessActionType::AddBan: chooseBan(descriptor); return currentChessGuiFlowResult();
    case ChessActionType::Equip: chooseEquipment(descriptor); return currentChessGuiFlowResult();
    case ChessActionType::BuyLegendaryEquipment: chooseLegendary(descriptor); return currentChessGuiFlowResult();
    case ChessActionType::StartChallenge: return chooseChallenge(descriptor);
    case ChessActionType::ChooseReward: return chooseReward(descriptor);
    case ChessActionType::ChooseMap: chooseMap(descriptor); return currentChessGuiFlowResult();
    case ChessActionType::SwapPositions: chooseSwap(descriptor); return currentChessGuiFlowResult();
    case ChessActionType::SetShopLocked: action.value = !session_.state().shopLocked; break;
    case ChessActionType::SetPositionSwapEnabled: action.value = !session_.state().options.positionSwapEnabled; break;
    default: break;
    }
    const auto result = submitGuiAction(action);
    if (!result.accepted)
    {
        showChessMessage(result.description);
        return currentChessGuiFlowResult();
    }
    if (descriptor.type == ChessActionType::PrepareBattle)
    {
        return drainPreparedBattle();
    }
    return currentChessGuiFlowResult();
}

bool ChessGuiSessionAdapter::openPreparationCommand(ChessContextMenuAction command)
{
    switch (command)
    {
    case ChessContextMenuAction::EnterBattle:
        if (const auto descriptor = legalAction(session_, ChessActionType::PrepareBattle))
        {
            if (chooseAndSubmit(*descriptor) == ChessGuiFlowResult::Aborted) return true;
        }
        else showChessMessage("出戰棋子不足，請先選擇出戰陣容");
        break;
    case ChessContextMenuAction::ManageBans: showBanManagement(); break;
    case ChessContextMenuAction::OpenEquipmentMenu: showEquipmentMenu(); break;
    case ChessContextMenuAction::OpenOverviewMenu: showOverviewMenu(); break;
    case ChessContextMenuAction::OpenOfflineBattle: showOfflineBattle(); break;
    case ChessContextMenuAction::ViewCombos: viewCombos(); break;
    case ChessContextMenuAction::ViewNeigong: viewNeigong(); break;
    case ChessContextMenuAction::ShowExpeditionChallenge:
    {
        ChessLegalActionDescriptor descriptor{ChessActionType::StartChallenge};
        descriptor.candidateStableIds = chessChallengeBrowseNames(session_.content());
        if (!descriptor.candidateStableIds.empty()
            && chooseChallenge(descriptor) == ChessGuiFlowResult::Aborted) return true;
        break;
    }
    case ChessContextMenuAction::OpenSystemMenu:
        if (runSystemMenu()) return true;
        break;
    case ChessContextMenuAction::LoadProgress:
        if (runSaveSlotMenu(0) >= 0) return true;
        break;
    case ChessContextMenuAction::SaveProgress:
        runSaveSlotMenu(1);
        break;
    default: std::unreachable();
    }
    return currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted;
}

void ChessGuiSessionAdapter::showContextMenu()
{
    if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted
        || drainRewards() == ChessGuiFlowResult::Aborted) return;
    if (!preparation_)
    {
        preparation_ = std::make_shared<ChessPreparationScreen>(session_, ChessPreparationCallbacks{
            [this](const ChessAction& action) { return submitGuiAction(action); },
        });
    }
    for (;;)
    {
        preparation_->run();
        const auto command = preparation_->takeCommand();
        if (!command || openPreparationCommand(*command)) return;
    }
}

}
