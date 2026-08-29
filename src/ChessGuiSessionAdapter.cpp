#include "ChessGuiSessionAdapter.h"

#include "BattleSceneHades.h"
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
#include "ChessMagicEffectDisplay.h"
#include "ChessMenuFormatting.h"
#include "ChessPresentationHelpers.h"
#include "ChessPreparedBattleAnalysis.h"
#include "ChessPvp.h"
#include "ChessScreenLayout.h"
#include "ChessSessionCheckpoint.h"
#include "ChessStandaloneBattle.h"
#include "ChessSystemSettingsMenu.h"
#include "ChessUiCommon.h"
#include "DrawableOnCall.h"
#include "Engine.h"
#include "Event.h"
#include "ExternalJsonFileTransfer.h"
#include "GameUtil.h"
#include "InputBox.h"
#include "Menu.h"
#include "ScenePreloader.h"
#include "SuperMenuText.h"
#include "SystemSettings.h"
#include "Talk.h"
#include "TextBox.h"
#include "TextureManager.h"
#include "UISave.h"

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

constexpr int kContextMenuX = 200;
constexpr int kContextMenuFontSize = 36;
constexpr int kContextMenuRowSpacing = 45;
constexpr int kAvatarMinWidth = 96;
constexpr int kAvatarHeight = 128;
constexpr int kStatsDetailOffsetY = 4;
constexpr int kSkillTopGap = 14;
constexpr int kOwnedTextInset = 8;
constexpr int kMagicBottomReserve = 10;
constexpr int kMagicEffectInset = 10;
constexpr int kComboRowGap = 2;
constexpr int kEquipIconOffsetX = 46;
constexpr int kEquipIconSize = 28;
constexpr int kEquipIconTopAdjust = 3;
constexpr int kEquipFallbackTopAdjust = 2;
constexpr int kEquipNameGap = 8;

struct StatusBlock
{
    int x{};
    int y{};
    int w{};
    int h{};
};

struct PanelColumnFlow
{
    Font* font{};
    PanelFrame frame{};
    int x{};
    int y{};
    int top{};
    int bottom{};
    int columnWidth{};

    bool ensureSpace(int requiredHeight)
    {
        if (y + requiredHeight <= bottom)
        {
            return true;
        }
        x += columnWidth;
        y = top;
        return x + columnWidth <= frame.x + frame.w;
    }

    bool line(const std::string& value, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        const int requiredHeight = fontSize + extraSpacing;
        if (!ensureSpace(requiredHeight))
        {
            return false;
        }
        font->draw(value, fontSize, x + indent, y, color);
        y += requiredHeight;
        return true;
    }
};

struct SessionStatusLayout
{
    int fontSize = 22;
    int smallFontSize = 20;
    int titleFontSize = 24;
    int pad = 12;
    int gap = 14;
    int lineHeight = fontSize + 4;
    int topY{};
    int bottomY{};
    int sectionTitleY{};
    int sectionContentY{};
    int magicStartY{};
    int magicAvailableHeight = 1;
    int comboRows = 2;
    int comboCols = 1;
    int comboColWidth{};
    StatusBlock panel{};
    StatusBlock avatar{};
    StatusBlock magic{};
    StatusBlock owned{};
    StatusBlock combo{};
    StatusBlock equip{};
    LabelValueColumn statsColumn{};
    LabelValueColumn skillCol1{};
    LabelValueColumn skillCol2{};

    static SessionStatusLayout build(Font* font, const StatusBlock& panel, int avatarWidth)
    {
        constexpr int kTopTextY = 18;
        constexpr int kAvatarTop = 14;
        constexpr int kStatValueOffset = 62;
        constexpr int kSkillValueOffset = 44;
        constexpr int kSkillSecondColumnX = 118;
        constexpr int kMagicStartOffsetX = 210;
        constexpr int kMagicHeaderHeight = 30;
        constexpr int kBottomSectionMinHeight = 96;
        constexpr int kBottomSectionMaxHeight = 128;
        constexpr int kBottomSectionTitleTop = 6;
        constexpr int kBottomSectionHeaderHeight = 28;
        constexpr int kOwnedSectionWidth = 150;
        constexpr int kEquipSectionWidth = 200;
        constexpr int kComboSectionMinWidth = 160;

        SessionStatusLayout layout;
        layout.panel = panel;
        layout.topY = panel.y + kTopTextY;
        layout.avatar = {panel.x + layout.pad, panel.y + kAvatarTop, avatarWidth, kAvatarHeight};

        const int bottomHeight = std::clamp(panel.h / 3, kBottomSectionMinHeight, kBottomSectionMaxHeight);
        layout.bottomY = panel.y + panel.h - layout.pad - bottomHeight;
        layout.sectionTitleY = layout.bottomY + kBottomSectionTitleTop;
        layout.sectionContentY = layout.sectionTitleY + kBottomSectionHeaderHeight;

        const int statsX = layout.avatar.x + layout.avatar.w + layout.gap;
        layout.statsColumn = {font, layout.fontSize, statsX, statsX + kStatValueOffset, {255, 250, 205, 255}};
        layout.skillCol1 = {font, layout.fontSize, layout.avatar.x, layout.avatar.x + kSkillValueOffset, {255, 250, 205, 255}};
        layout.skillCol2 = {font, layout.fontSize, layout.avatar.x + kSkillSecondColumnX, layout.avatar.x + kSkillSecondColumnX + kSkillValueOffset, {255, 250, 205, 255}};

        const int magicX = statsX + kMagicStartOffsetX;
        layout.magic = {magicX, layout.topY, panel.x + panel.w - layout.pad - magicX, layout.bottomY - layout.topY};
        layout.magicStartY = layout.magic.y + kMagicHeaderHeight;
        layout.magicAvailableHeight = std::max(
            1,
            layout.bottomY - kMagicBottomReserve - layout.magicStartY);

        const int innerW = panel.w - layout.pad * 2;
        const int comboW = std::max(kComboSectionMinWidth, innerW - kOwnedSectionWidth - kEquipSectionWidth - layout.gap * 2);
        layout.owned = {panel.x + layout.pad, layout.sectionTitleY, kOwnedSectionWidth, bottomHeight};
        layout.combo = {layout.owned.x + layout.owned.w + layout.gap, layout.sectionTitleY, comboW, bottomHeight};
        layout.equip = {layout.combo.x + layout.combo.w + layout.gap, layout.sectionTitleY, kEquipSectionWidth, bottomHeight};
        return layout;
    }

    void finalizeComboColumns(int comboCount)
    {
        comboRows = std::max(2, (combo.h - 32) / (smallFontSize + 2));
        comboCols = comboCount > comboRows ? 2 : 1;
        constexpr int kComboMinColumnWidth = 90;
        comboColWidth = comboCols == 1 ? combo.w : std::max(kComboMinColumnWidth, (combo.w - gap) / 2);
    }
};

struct SessionMenuData
{
    std::vector<std::string> labels;
    std::vector<Color> colors;
    std::vector<Color> outlineColors;
    std::vector<bool> animateOutlines;
    std::vector<int> outlineThicknesses;
};

using PanelDrawer = std::function<void(int, const PanelFrame&)>;
using PanelVisibility = std::function<bool(int)>;

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
        session.state().options.battleFrameLimit);
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
    const auto preloadEffect = [&](int effectId) {
        if (effectId < 0)
        {
            return;
        }
        const auto path = std::format("eft/eft{:03}", effectId);
        const int frameCount = TextureManager::getInstance()->getTextureGroupCount(path);
        for (int frame = 0; frame < frameCount; ++frame)
        {
            if (auto* texture = TextureManager::getInstance()->getTexture(path, frame))
            {
                texture->load();
            }
        }
    };

    const auto preloadTeam = [](const std::vector<BattlePreviewUnit>& team) {
        for (const auto& unit : team)
        {
            const auto fightGroup = std::format("fight/fight{:03}", unit.headId);
            const int fightFrameCount = TextureManager::getInstance()->getTextureGroupCount(fightGroup);
            for (int frame = 0; frame < fightFrameCount; ++frame)
            {
                if (auto* texture = TextureManager::getInstance()->getTexture(fightGroup, frame))
                {
                    texture->load();
                }
            }
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

const EquipmentDef* equipmentDefinition(const ChessGameContent& content, int itemId)
{
    const auto found = std::ranges::find(content.equipment(), itemId, &EquipmentDef::itemId);
    return found == content.equipment().end() ? nullptr : &*found;
}

const ChessEquipmentInstance* equipmentInstance(const ChessSessionState& state, int instanceId)
{
    const auto found = state.equipmentInventory.find(instanceId);
    return found == state.equipmentInventory.end() ? nullptr : &found->second;
}

const ChessSessionPiece* rosterPiece(const ChessSessionState& state, int instanceId)
{
    const auto found = state.roster.find(instanceId);
    return found == state.roster.end() ? nullptr : &found->second;
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
    auto text = std::make_shared<DismissibleTextBox>();
    text->setText(std::format(
        "勝利！獲得${}{}{} 經驗+{}{}{}",
        gold->primaryId,
        interestMessage,
        comboMessage,
        experience->value,
        levelMessage,
        nextInformation));
    text->setFontSize(32);
    text->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

void drawWrappedLines(
    PanelTextCursor& cursor,
    const std::string& text,
    int fontSize,
    Color color,
    int pixelWidth,
    int extraSpacing = 3,
    int indent = 0)
{
    const int displayWidth = displayTextUnitsForPixelWidth(fontSize, pixelWidth, indent);
    for (const auto& line : wrapDisplayText(text, displayWidth))
    {
        cursor.line(line, fontSize, color, extraSpacing, indent);
    }
}

struct PanelVisualTextRow
{
    PanelTextSourceRow layout;
    Color color;
};

void appendPanelTextRow(
    std::vector<PanelVisualTextRow>& rows,
    std::string text,
    Color color,
    int fontSizeDelta = 0,
    int indentUnits = 0,
    int spacingBefore = 0,
    int spacingAfter = 0)
{
    rows.push_back({
        .layout = {
            .text = std::move(text),
            .fontSizeDelta = fontSizeDelta,
            .indentUnits = indentUnits,
            .spacingBefore = spacingBefore,
            .spacingAfter = spacingAfter,
        },
        .color = color,
    });
}

void appendRenderedEffectDescriptionRows(
    std::vector<PanelVisualTextRow>& rows,
    const RenderedEffectDescription& rendered,
    Color color,
    int fontSizeDelta = 0,
    int extraSpacing = 3,
    int baseIndentUnits = 0)
{
    for (auto& layout : panelTextRowsForEffectDescription(
             rendered,
             fontSizeDelta,
             extraSpacing,
             baseIndentUnits))
    {
        rows.push_back({
            .layout = std::move(layout),
            .color = color,
        });
    }
}

std::vector<PanelTextSourceRow> panelTextLayoutRows(
    std::span<const PanelVisualTextRow> rows)
{
    std::vector<PanelTextSourceRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(row.layout);
    return result;
}

void drawPanelText(
    Font* font,
    std::span<const PanelVisualTextRow> rows,
    const PanelTextLayout& layout,
    int x,
    int y)
{
    for (const auto& line : layout.lines)
    {
        font->draw(
            line.text,
            line.fontSize,
            x + line.indentPixels,
            y + line.y,
            rows[line.sourceRow].color);
    }
}

PanelTextLayout fitVisualPanelText(
    std::span<const PanelVisualTextRow> rows,
    int pixelWidth,
    int pixelHeight,
    int preferredFontSize,
    int minimumFontSize)
{
    const auto layoutRows = panelTextLayoutRows(rows);
    return fitPanelText(
        layoutRows,
        pixelWidth,
        pixelHeight,
        preferredFontSize,
        minimumFontSize);
}

std::shared_ptr<DrawableOnCall> makePanel(
    PanelFrame frame,
    PanelDrawer drawer,
    PanelVisibility visibility = {},
    Color fillColor = {0, 0, 0, 180},
    Color borderColor = {180, 170, 140, 200},
    int radius = 8)
{
    return std::make_shared<DrawableOnCall>(
        [frame,
            drawer = std::move(drawer),
            visibility = std::move(visibility),
            fillColor,
            borderColor,
            radius](DrawableOnCall* self) {
            const int row = self->getItemIndex();
            if (visibility && !visibility(row))
            {
                return;
            }
            ChessScreenLayout::drawPanel(frame, fillColor, borderColor, radius);
            drawer(row, frame);
        });
}

std::vector<std::string> equipmentEquippedBy(
    const ChessGameSession& session,
    int itemId,
    std::optional<int> instanceFilter = std::nullopt)
{
    std::vector<std::string> names;
    for (const auto& [instanceId, equipment] : session.state().equipmentInventory)
    {
        if (equipment.itemId != itemId
            || (instanceFilter && instanceId != *instanceFilter)
            || equipment.assignedChessInstanceId < 0)
        {
            continue;
        }
        const auto* piece = rosterPiece(session.state(), equipment.assignedChessInstanceId);
        assert(piece);
        const auto* role = session.content().role(piece->roleId);
        assert(role);
        names.push_back(std::format("{} {}", role->Name, chessStars(piece->star)));
    }
    return names;
}

void drawEquipmentDetail(
    const ChessGameSession& session,
    const EquipmentDef& equipment,
    const PanelFrame& frame,
    const std::vector<std::string>& equippedBy)
{
    const auto* item = session.content().item(equipment.itemId);
    if (item)
    {
        TextureManager::getInstance()->renderTexture("item", equipment.itemId, frame.x + 10, frame.y + 10);
    }

    constexpr int fontSize = 28;
    PanelTextCursor header{Font::getInstance(), frame.x + 150, frame.y + 10};
    header.line(item ? item->name : "???", fontSize + 4, {255, 255, 100, 255}, 6);
    int metadataX = header.x;
    const auto drawMetadata = [&](const std::string& text, Color color) {
        header.font->draw(text, fontSize, metadataX, header.y, color);
        metadataX += fontSize * Font::getTextDrawSize(text) / 2;
    };
    constexpr Color metadataLabelColor{220, 220, 190, 255};
    drawMetadata("層級: ", metadataLabelColor);
    drawMetadata(chessRewardTierLabel(equipment.tier), chessRewardTierColor(equipment.tier));
    drawMetadata("　類型: ", metadataLabelColor);
    drawMetadata(chessEquipmentTypeName(equipment.equipType), chessEquipmentTypeColor(equipment.equipType));
    header.skip(fontSize + 4);

    std::vector<PanelVisualTextRow> bodyRows;
    const int bodyWidth = frame.w - 20;
    if (!equipment.rules.empty())
    {
        appendPanelTextRow(bodyRows, "特殊效果:", {255, 200, 100, 255}, 2, 0, 0, 2);
        const auto document = buildEffectDescriptionDocument({
            EffectDescriptionContainerKind::Equipment,
            equipment.rules,
        });
        const auto rendered = renderEffectDescription(
            document,
            EffectDescriptionStyle::Full,
            {});
        appendRenderedEffectDescriptionRows(
            bodyRows,
            rendered,
            {220, 220, 100, 255},
            0,
            2);
    }

    const bool hasSynergies = std::ranges::any_of(
        session.content().equipmentSynergies(),
        [&](const auto& synergy) { return synergy.equipmentId == equipment.itemId; });
    if (hasSynergies)
    {
        appendPanelTextRow(
            bodyRows,
            "裝備羈絆:",
            {255, 200, 100, 255},
            2,
            0,
            equipment.rules.empty() ? 0 : 12,
            2);
        for (const auto& synergy : session.content().equipmentSynergies())
        {
            if (synergy.equipmentId != equipment.itemId) continue;
            std::string heading;
            for (std::size_t index = 0; index < synergy.roleIds.size(); ++index)
            {
                if (index > 0) heading += "、";
                const auto* role = session.content().role(synergy.roleIds[index]);
                assert(role);
                heading += role->Name;
            }
            heading += "：";
            const auto comboNames = countsAsComboNames(synergy.managementRules);
            if (!comboNames.empty())
            {
                heading += "計作";
                for (std::size_t index = 0; index < comboNames.size(); ++index)
                {
                    if (index > 0) heading += "、";
                    heading += comboNames[index];
                }
            }
            appendPanelTextRow(
                bodyRows,
                std::move(heading),
                {220, 220, 100, 255},
                0,
                0,
                0,
                2);
            const auto document = buildEffectDescriptionDocument({
                EffectDescriptionContainerKind::EquipmentSynergy,
                synergy.rules,
            });
            const auto rendered = renderEffectDescription(
                document,
                EffectDescriptionStyle::Full,
                {});
            appendRenderedEffectDescriptionRows(
                bodyRows,
                rendered,
                {220, 220, 100, 255},
                0,
                2,
                2);
        }
    }

    if (!equippedBy.empty())
    {
        appendPanelTextRow(
            bodyRows,
            "裝備棋子:",
            {140, 220, 255, 255},
            2,
            0,
            !equipment.rules.empty() || hasSynergies ? 16 : 0,
            4);
        for (const auto& name : equippedBy)
        {
            appendPanelTextRow(bodyRows, name, {210, 235, 255, 255}, 0, 1, 0, 2);
        }
    }
    if (!bodyRows.empty())
    {
        constexpr int bodyTop = 100;
        constexpr int bodyBottomInset = 10;
        const auto layout = fitVisualPanelText(
            bodyRows,
            bodyWidth,
            frame.h - bodyTop - bodyBottomInset,
            fontSize - 2,
            14);
        drawPanelText(Font::getInstance(), bodyRows, layout, frame.x + 10, frame.y + bodyTop);
    }
}

std::shared_ptr<DrawableOnCall> makeEquipmentDetailPanel(
    const ChessGameSession& session,
    std::vector<const EquipmentDef*> equipments,
    std::vector<std::vector<std::string>> equippedByRows,
    PanelFrame frame)
{
    const int rowCount = static_cast<int>(equipments.size());
    return makePanel(
        frame,
        [&session,
            equipments = std::move(equipments),
            equippedByRows = std::move(equippedByRows)](int row, const PanelFrame& panelFrame) {
            const std::vector<std::string> empty;
            drawEquipmentDetail(
                session,
                *equipments[row],
                panelFrame,
                row < static_cast<int>(equippedByRows.size()) ? equippedByRows[row] : empty);
        },
        [rowCount](int row) { return isPanelRowInRange(row, rowCount); });
}

void drawNeigongDetail(
    const ChessGameSession& session,
    const NeigongDef& neigong,
    const PanelFrame& frame,
    bool showOwnedState)
{
    TextureManager::getInstance()->renderTexture("item", neigong.itemId, frame.x + 10, frame.y + 10);
    constexpr int fontSize = 24;
    PanelTextCursor header{Font::getInstance(), frame.x + 100, frame.y + 10};
    header.line(neigong.name, fontSize + 4, {255, 255, 100, 255}, 6);
    header.line(
        std::format("層級: {}", chessRewardTierLabel(neigong.tier)),
        fontSize,
        chessRewardTierColor(neigong.tier));
    if (showOwnedState)
    {
        const bool owned = session.state().obtainedNeigongIds.contains(neigong.magicId);
        header.line(
            owned ? "已獲得" : "未獲得",
            fontSize,
            owned ? Color{0, 255, 0, 255} : Color{180, 180, 180, 255},
            8);
    }

    std::vector<PanelVisualTextRow> bodyRows;
    appendPanelTextRow(bodyRows, "效果:", {200, 200, 200, 255}, 2, 0, 0, 2);
    const auto document = buildEffectDescriptionDocument({
        EffectDescriptionContainerKind::Neigong,
        neigong.rules,
    });
    const auto rendered = renderEffectDescription(
        document,
        EffectDescriptionStyle::Full,
        {});
    appendRenderedEffectDescriptionRows(
        bodyRows,
        rendered,
        {220, 220, 220, 255},
        0,
        2,
        2);
    constexpr int bodyTop = 100;
    constexpr int bodyBottomInset = 10;
    const auto layout = fitVisualPanelText(
        bodyRows,
        frame.w - 20,
        frame.h - bodyTop - bodyBottomInset,
        fontSize,
        14);
    drawPanelText(Font::getInstance(), bodyRows, layout, frame.x + 10, frame.y + bodyTop);
}

std::shared_ptr<DrawableOnCall> makeNeigongDetailPanel(
    const ChessGameSession& session,
    std::vector<const NeigongDef*> neigongs,
    PanelFrame frame,
    bool showOwnedState)
{
    const int rowCount = static_cast<int>(neigongs.size());
    return makePanel(
        frame,
        [&session,
            neigongs = std::move(neigongs),
            showOwnedState](int row, const PanelFrame& panelFrame) {
            drawNeigongDetail(session, *neigongs[row], panelFrame, showOwnedState);
        },
        [rowCount](int row) { return isPanelRowInRange(row, rowCount); });
}

std::shared_ptr<DrawableOnCall> makeBattleMapPreviewPanel(
    const ChessGameSession& session,
    std::vector<int> mapIds)
{
    const auto frame = ChessScreenLayout::largeModalPreviewPanel();
    return makePanel(frame, [&session, mapIds = std::move(mapIds)](int row, const PanelFrame& panelFrame) {
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
        cursor.line(chessBattleMapDisplayName(session.content(), mapId), 25, {255, 215, 0, 255}, 4);
        cursor.line(
            std::format(
                "我方上限{}　敵軍上限{}",
                catalog ? catalog->teammatePositions.size() : map->second.teammateX.size(),
                catalog ? catalog->enemyCapacity : map->second.enemyX.size()),
            18,
            {220, 220, 220, 255},
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
    });
}

int runIndexedMenu(
    const std::string& title,
    const SessionMenuData& data,
    int fontSize = kChessBrowseMenuPresentation.fontSize,
    int perPage = kChessBrowseMenuPresentation.itemsPerPage,
    PanelAnchor anchor = ChessScreenLayout::contentMenuAnchor(),
    const std::vector<std::shared_ptr<DrawableOnCall>>& panels = {},
    bool showSearch = false,
    bool showNavigation = true,
    bool exitable = true,
    std::function<bool()> exitConfirmation = {})
{
    if (data.labels.empty())
    {
        return -1;
    }

    SuperMenuTextExtraOptions options;
    options.itemColors_ = data.colors;
    options.outlineColors_ = data.outlineColors;
    options.animateOutlines_ = data.animateOutlines;
    options.outlineThicknesses_ = data.outlineThicknesses;
    options.needInputBox_ = showSearch;
    options.exitable_ = exitable;
    auto menu = std::make_shared<SuperMenuText>(
        title,
        fontSize,
        data.labels,
        std::max(1, perPage),
        std::move(options));
    menu->setExitConfirmation(std::move(exitConfirmation));
    menu->setInputPosition(anchor.x, anchor.y);
    menu->setShowNavigationButtons(showNavigation && data.labels.size() > static_cast<std::size_t>(perPage));
    menu->setDoubleTapMode(GameUtil::isMobileDevice());
    for (const auto& panel : panels)
    {
        menu->addDrawableOnCall(panel);
    }
    menu->run();
    return menu->getResult();
}

std::optional<ChessContextMenuAction> runContextActionMenu(
    const std::vector<ChessContextMenuItem>& items)
{
    auto menu = std::make_shared<MenuText>(chessContextMenuLabels(items));
    menu->setFontSize(kContextMenuFontSize);
    menu->arrange(0, 0, 0, kContextMenuRowSpacing);
    const auto region = ChessScreenLayout::fullContentRegion();
    menu->runAtPosition(
        kContextMenuX,
        centerChessContextMenuY(items.size(), region.y, region.h, kContextMenuRowSpacing));
    const int result = menu->getResult();
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
    menu->setFontSize(kContextMenuFontSize);
    menu->arrange(0, 0, 0, kContextMenuRowSpacing);
    const auto region = ChessScreenLayout::fullContentRegion();
    return menu->runAtPosition(
        kContextMenuX,
        centerChessContextMenuY(
            menu->getChildCount(),
            region.y,
            region.h,
            kContextMenuRowSpacing));
}

bool confirmReturnToTitle()
{
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認返回開頭", "取消"});
    menu->setFontSize(kContextMenuFontSize);
    menu->arrange(0, 0, 0, kContextMenuRowSpacing);
    const auto region = ChessScreenLayout::fullContentRegion();
    return menu->runAtPosition(
               kContextMenuX,
               centerChessContextMenuY(
                   menu->getChildCount(),
                   region.y,
                   region.h,
                   kContextMenuRowSpacing))
        == 0;
}

ChessMagicEffectDisplayLayout drawRoleDetail(
    const ChessGameSession& session,
    int roleId,
    int star,
    int instanceId,
    const PanelFrame& frame,
    int magicScrollOffset)
{
    const auto* role = session.content().role(roleId);
    assert(role);
    auto* font = Font::getInstance();
    const Color colorWhite{255, 255, 255, 255};
    const Color colorName{255, 215, 0, 255};
    const Color colorAbility{255, 250, 205, 255};
    const Color colorRed{255, 90, 60, 255};
    const Color colorMagic{236, 200, 40, 255};
    const Color colorMagicEffect{150, 220, 255, 255};
    const Color colorPurple{208, 152, 208, 255};
    const Color colorEquip{210, 230, 220, 255};
    const Color colorInactive{180, 180, 180, 255};

    const auto selectStatColor = [&](int value, int maximum) -> Color {
        if (value >= maximum * 0.9) return colorRed;
        if (value >= maximum * 0.8) return {255, 165, 79, 255};
        if (value >= maximum * 0.7) return {255, 255, 50, 255};
        if (value < 0) return colorPurple;
        return colorWhite;
    };

    const auto* piece = rosterPiece(session.state(), instanceId);
    const int fightsWon = piece ? piece->fightsWon : 0;
    const auto stats = chessRoleStats(
        *role,
        session.content().balance(),
        star,
        fightsWon);

    int avatarWidth = 128;
    auto* headTexture = TextureManager::getInstance()->getTexture("head", role->HeadID);
    if (headTexture)
    {
        headTexture->load();
        if (headTexture->h > 0)
        {
            avatarWidth = std::max(kAvatarMinWidth, headTexture->w * kAvatarHeight / headTexture->h);
        }
    }

    auto layout = SessionStatusLayout::build(font, {frame.x, frame.y, frame.w, frame.h}, avatarWidth);
    if (headTexture)
    {
        const int renderH = std::max(1, layout.avatar.h);
        const int renderW = std::max(kAvatarMinWidth, headTexture->h > 0 ? headTexture->w * renderH / headTexture->h : renderH);
        TextureManager::getInstance()->renderTexture(
            headTexture,
            layout.avatar.x + headTexture->dx,
            layout.avatar.y + headTexture->dy,
            TextureManager::RenderInfo{colorWhite, 255},
            renderW,
            renderH);
    }
    else
    {
        TextureManager::getInstance()->renderTexture("head", role->HeadID, layout.avatar.x, layout.avatar.y);
    }

    const int statsY = layout.topY;
    Color mpColor = colorWhite;
    if (role->MPType == 0) mpColor = colorPurple;
    else if (role->MPType == 1) mpColor = colorMagic;
    layout.statsColumn.line(statsY + layout.lineHeight * 0, "生命", std::format("{:5}/{:5}", stats.maxHp, stats.maxHp), colorWhite);
    layout.statsColumn.line(
        statsY + layout.lineHeight * 1,
        "內力",
        formatChessRolePreviewMp(GameUtil::MAX_MP),
        mpColor);
    layout.statsColumn.line(statsY + layout.lineHeight * 2 + kStatsDetailOffsetY, "攻擊", std::format("{:5}", stats.attack), selectStatColor(stats.attack, Role::getMaxValue()->Attack));
    layout.statsColumn.line(statsY + layout.lineHeight * 3 + kStatsDetailOffsetY, "防禦", std::format("{:5}", stats.defence), selectStatColor(stats.defence, Role::getMaxValue()->Defence));
    layout.statsColumn.line(statsY + layout.lineHeight * 4 + kStatsDetailOffsetY, "輕功", std::format("{:5}", stats.speed), selectStatColor(stats.speed, Role::getMaxValue()->Speed));

    const int skillY = layout.avatar.y + layout.avatar.h + kSkillTopGap;
    layout.skillCol1.line(skillY + layout.lineHeight * 0, "拳掌", std::format("{:5}", stats.fist), selectStatColor(stats.fist, Role::getMaxValue()->Fist));
    layout.skillCol2.line(skillY + layout.lineHeight * 0, "御劍", std::format("{:5}", stats.sword), selectStatColor(stats.sword, Role::getMaxValue()->Sword));
    layout.skillCol1.line(skillY + layout.lineHeight * 1, "耍刀", std::format("{:5}", stats.knife), selectStatColor(stats.knife, Role::getMaxValue()->Knife));
    layout.skillCol2.line(skillY + layout.lineHeight * 1, "特殊", std::format("{:5}", stats.unusual), selectStatColor(stats.unusual, Role::getMaxValue()->Unusual));

    font->draw("武學", layout.titleFontSize, layout.magic.x, layout.magic.y, colorName);
    const auto selectedMagics = chessRoleMagicsForStar(session.content(), *role, star);
    std::vector<const MagicSave*> magics;
    magics.reserve(selectedMagics.size());
    for (const auto& selection : selectedMagics)
    {
        magics.push_back(selection.first);
    }
    const int ultimateMagicId = selectedMagics.empty()
        ? -1
        : selectedMagics.back().first->ID;
    const auto rawMagicRows = buildChessMagicEffectDisplayRows(
        magics,
        session.content().magicEffects(),
        ultimateMagicId);
    const auto magicDisplay = layoutChessMagicEffectDisplay(
        rawMagicRows,
        layout.magic.w,
        layout.magicAvailableHeight);
    for (const auto& displayLine : visibleChessMagicEffectDisplayLines(
        magicDisplay,
        magicScrollOffset))
    {
        const auto& magicRow = displayLine.content;
        const int lineX = layout.magic.x + displayLine.x;
        const int lineY = layout.magicStartY + displayLine.y;
        if (magicRow.kind == ChessMagicEffectDisplayLineKind::Effect)
        {
            font->draw(
                magicRow.text,
                displayLine.fontSize,
                lineX,
                lineY,
                colorMagicEffect);
            continue;
        }
        const int magicPower = [&] {
            int result = 0;
            for (const auto& [magic, power] : selectedMagics)
            {
                if (magic->ID == magicRow.magic->ID)
                {
                    result = std::max(result, power);
                }
            }
            return result;
        }();
        const int operationType = BattleSceneHades::getOperationType(magicRow.magic->AttackAreaType);
        font->draw(
            magicRow.text,
            displayLine.fontSize,
            lineX,
            lineY,
            magicRow.ultimate ? colorMagic : colorAbility);
        font->draw(
            std::format("{:4} {}", magicPower, BattleSceneHades::getOperationTypeName(operationType)),
            displayLine.fontSize,
            layout.magic.x + magicDisplay.skillValueX,
            lineY,
            colorWhite);
    }
    if (magicDisplay.scrollable)
    {
        const auto scrollOffset = clampChessMagicEffectDisplayScrollOffset(
            magicDisplay,
            magicScrollOffset);
        const auto stop = std::ranges::find(magicDisplay.scrollStops, scrollOffset);
        const int scrollStopIndex = stop == magicDisplay.scrollStops.end()
            ? 1
            : static_cast<int>(std::distance(magicDisplay.scrollStops.begin(), stop)) + 1;
        font->draw(
            std::format("捲動 PgUp/PgDn  {}/{}", scrollStopIndex, magicDisplay.scrollStops.size()),
            magicDisplay.effectFontSize,
            layout.magic.x + kMagicEffectInset,
            layout.magicStartY + magicDisplay.viewportHeight
                - magicDisplay.scrollIndicatorHeight,
            colorInactive);
    }

    font->draw("擁有", layout.titleFontSize, layout.owned.x, layout.sectionTitleY, colorName);
    std::map<int, int> starCounts;
    for (const auto& [ownedInstanceId, ownedPiece] : session.state().roster)
    {
        if (ownedPiece.roleId == roleId) ++starCounts[ownedPiece.star];
    }
    if (starCounts.empty())
    {
        font->draw("無", layout.smallFontSize, layout.owned.x + kOwnedTextInset, layout.sectionContentY, colorInactive);
    }
    else
    {
        PanelTextCursor ownedCursor{font, layout.owned.x + kOwnedTextInset, layout.sectionContentY};
        for (const auto& [ownedStar, count] : starCounts)
        {
            ownedCursor.line(std::format("{} x{}", chessStars(ownedStar), count), layout.smallFontSize, colorWhite, 4);
        }
        if (piece)
        {
            ownedCursor.line(
                std::format("勝場 {}（成長）", fightsWon),
                layout.smallFontSize,
                colorAbility,
                4);
        }
    }
    if (starCounts.empty() && piece)
    {
        font->draw(
            std::format("勝場 {}（成長）", fightsWon),
            layout.smallFontSize,
            layout.owned.x + kOwnedTextInset,
            layout.sectionContentY + layout.smallFontSize + 8,
            colorAbility);
    }

    std::vector<const ComboDef*> roleCombos;
    for (const auto& combo : session.content().combos())
    {
        if (std::ranges::find(combo.memberRoleIds, roleId) != combo.memberRoleIds.end())
        {
            roleCombos.push_back(&combo);
        }
    }
    font->draw("羈絆", layout.titleFontSize, layout.combo.x, layout.sectionTitleY, colorName);
    if (roleCombos.empty())
    {
        font->draw("無", layout.smallFontSize, layout.combo.x + kOwnedTextInset, layout.sectionContentY, colorInactive);
    }
    else
    {
        layout.finalizeComboColumns(static_cast<int>(roleCombos.size()));
        for (int index = 0; index < static_cast<int>(roleCombos.size()); ++index)
        {
            const int column = index / layout.comboRows;
            const int row = index % layout.comboRows;
            if (column >= layout.comboCols) break;
            const auto& combo = *roleCombos[index];
            const auto progress = evaluateChessComboProgress(session.state(), session.content(), combo);
            font->draw(
                combo.name,
                layout.smallFontSize,
                layout.combo.x + column * layout.comboColWidth,
                layout.sectionContentY + row * (layout.smallFontSize + kComboRowGap),
                progress.active ? Color{0, 255, 100, 255} : colorInactive);
        }
    }

    if (piece)
    {
        font->draw("裝備", layout.titleFontSize, layout.equip.x, layout.sectionTitleY, colorName);
        PanelTextCursor equipCursor{font, layout.equip.x, layout.sectionContentY};
        const auto drawEquipment = [&](const char* slotName, int equipmentInstanceId) {
            const int rowY = equipCursor.y;
            const auto* instance = equipmentInstance(session.state(), equipmentInstanceId);
            const auto* item = instance ? session.content().item(instance->itemId) : nullptr;
            const int iconX = equipCursor.x + kEquipIconOffsetX;
            font->draw(slotName, layout.smallFontSize, equipCursor.x, rowY, colorAbility);
            if (item)
            {
                if (auto* itemTexture = TextureManager::getInstance()->getTexture("item", item->id))
                {
                    itemTexture->load();
                    const int renderH = kEquipIconSize;
                    const int renderW = itemTexture->h > 0 ? itemTexture->w * renderH / itemTexture->h : renderH;
                    TextureManager::getInstance()->renderTexture(
                        itemTexture,
                        iconX + itemTexture->dx,
                        rowY - kEquipIconTopAdjust + itemTexture->dy,
                        TextureManager::RenderInfo{colorWhite, 255},
                        renderW,
                        renderH);
                }
                else
                {
                    TextureManager::getInstance()->renderTexture(
                        "item",
                        item->id,
                        iconX,
                        rowY - kEquipFallbackTopAdjust,
                        TextureManager::RenderInfo{colorWhite, 255, 0.26, 0.26});
                }
                font->draw(item->name, layout.smallFontSize, iconX + kEquipIconSize + kEquipNameGap, rowY, colorEquip);
            }
            else
            {
                font->draw("無", layout.smallFontSize, iconX, rowY, colorInactive);
            }
            equipCursor.skip(layout.smallFontSize + 8);
        };
        drawEquipment("武器", piece->weaponInstanceId);
        drawEquipment("護甲", piece->armorInstanceId);
    }
    return magicDisplay;
}

std::shared_ptr<DrawableOnCall> makeRoleDetailPanel(
    const ChessGameSession& session,
    std::vector<int> roleIds,
    std::vector<int> starsByRow = {},
    std::vector<int> instanceIds = {},
    std::optional<PanelFrame> requestedFrame = std::nullopt)
{
    const auto frame = requestedFrame.value_or(ChessScreenLayout::browseDetailRegion());
    std::vector<bool> visibleRows;
    visibleRows.reserve(roleIds.size());
    for (const int roleId : roleIds)
    {
        visibleRows.push_back(roleId >= 0);
    }
    struct ScrollState
    {
        int row = -1;
        int offset{};
        std::optional<ChessMagicEffectDisplayLayout> layout;
    };
    auto scroll = std::make_shared<ScrollState>();
    auto panel = std::make_shared<DrawableOnCall>(
        [frame,
            &session,
            roleIds = std::move(roleIds),
            starsByRow = std::move(starsByRow),
            instanceIds = std::move(instanceIds),
            visibleRows = std::move(visibleRows),
            scroll](DrawableOnCall* self) {
            const int row = self->getItemIndex();
            if (!isPanelRowInRange(row, static_cast<int>(visibleRows.size()))
                || !visibleRows[row])
            {
                scroll->layout.reset();
                return;
            }
            if (scroll->row != row)
            {
                scroll->row = row;
                scroll->offset = 0;
            }
            ChessScreenLayout::drawPanel(
                frame,
                {0, 0, 0, 128},
                {180, 170, 140, 200},
                8);
            const int star = row < static_cast<int>(starsByRow.size()) ? starsByRow[row] : 1;
            const int instanceId = row < static_cast<int>(instanceIds.size()) ? instanceIds[row] : -1;
            scroll->layout = drawRoleDetail(
                session,
                roleIds[row],
                star,
                instanceId,
                frame,
                scroll->offset);
            scroll->offset = clampChessMagicEffectDisplayScrollOffset(
                *scroll->layout,
                scroll->offset);
        });
    panel->setEventHandler([scroll](DrawableOnCall*, EngineEvent& event) {
        if (!scroll->layout || !scroll->layout->scrollable) return;
        int direction{};
        if (event.type == EVENT_KEY_UP)
        {
            if (event.key.key == K_PAGEUP) direction = -1;
            if (event.key.key == K_PAGEDOWN) direction = 1;
        }
        else if (event.type == EVENT_GAMEPAD_BUTTON_UP)
        {
            if (event.gbutton.button == GAMEPAD_BUTTON_LEFT_SHOULDER) direction = -1;
            if (event.gbutton.button == GAMEPAD_BUTTON_RIGHT_SHOULDER) direction = 1;
        }
        if (direction != 0)
        {
            scroll->offset = stepChessMagicEffectDisplayScrollOffset(
                *scroll->layout,
                scroll->offset,
                direction);
            event.type = EVENT_FIRST;
        }
    });
    return panel;
}

std::shared_ptr<DrawableOnCall> makeComboInfoPanel(
    const ChessGameSession& session,
    std::vector<int> roleIds,
    PanelFrame frame)
{
    return std::make_shared<DrawableOnCall>(
        [&session, roleIds = std::move(roleIds), frame](DrawableOnCall* self) {
            const int row = self->getItemIndex();
            if (row < 0 || row >= static_cast<int>(roleIds.size()) || roleIds[row] < 0)
            {
                return;
            }

            std::vector<const ComboDef*> roleCombos;
            for (const auto& combo : session.content().combos())
            {
                if (std::ranges::find(combo.memberRoleIds, roleIds[row]) != combo.memberRoleIds.end())
                {
                    roleCombos.push_back(&combo);
                }
            }
            if (roleCombos.empty())
            {
                return;
            }

            ChessScreenLayout::drawPanel(frame, {0, 0, 0, 160});
            auto* font = Font::getInstance();
            constexpr int preferredFontSize = 19;
            constexpr int minimumFontSize = 12;
            font->draw("羈絆資訊", preferredFontSize + 5,
                frame.x + 10, frame.y + 5, {255, 255, 100, 255});

            std::vector<std::vector<PanelVisualTextRow>> blocks;
            blocks.reserve(roleCombos.size());
            for (const auto* combo : roleCombos)
            {
                const auto progress = evaluateChessComboProgress(
                    session.state(), session.content(), *combo);
                const ComboThreshold* shownThreshold = nullptr;
                if (progress.activeThresholdIndex >= 0)
                    shownThreshold = &combo->thresholds[progress.activeThresholdIndex];
                else if (progress.nextThresholdIndex >= 0)
                    shownThreshold = &combo->thresholds[progress.nextThresholdIndex];
                else if (!combo->thresholds.empty())
                    shownThreshold = &combo->thresholds.back();

                const auto headerColor = progress.active
                    ? Color{0, 255, 100, 255}
                    : Color{200, 200, 200, 255};
                const auto effectColor = progress.active
                    ? Color{180, 220, 255, 255}
                    : Color{180, 180, 180, 255};
                std::vector<PanelVisualTextRow> block;
                appendPanelTextRow(
                    block,
                    std::format("{} ({})", combo->name, formatChessComboProgressCount(progress)),
                    headerColor,
                    1,
                    0,
                    0,
                    2);
                if (shownThreshold)
                {
                    const auto document = buildEffectDescriptionDocument({
                        EffectDescriptionContainerKind::ComboThreshold,
                        shownThreshold->rules,
                    });
                    const auto rendered = renderEffectDescription(
                        document,
                        EffectDescriptionStyle::Compact,
                        {});
                    appendRenderedEffectDescriptionRows(
                        block,
                        rendered,
                        effectColor,
                        0,
                        1,
                        2);
                }
                blocks.push_back(std::move(block));
            }

            const auto combineBlocks = [&](std::size_t first, std::size_t last)
            {
                std::vector<PanelVisualTextRow> result;
                for (std::size_t index = first; index < last; ++index)
                    result.insert(result.end(), blocks[index].begin(), blocks[index].end());
                return result;
            };
            constexpr int contentTop = 30;
            constexpr int contentBottomInset = 10;
            const int availableHeight = frame.h - contentTop - contentBottomInset;
            std::vector<std::vector<PanelTextSourceRow>> layoutBlocks;
            layoutBlocks.reserve(blocks.size());
            for (const auto& block : blocks)
                layoutBlocks.push_back(panelTextLayoutRows(block));
            const auto chosen = fitPanelTextBlocks(
                layoutBlocks,
                frame.w - 20,
                availableHeight,
                preferredFontSize,
                minimumFontSize);
            assert(chosen && "formal Compact combo descriptions must fit two readable columns");
            for (std::size_t columnIndex = 0; columnIndex < chosen->columns.size(); ++columnIndex)
            {
                const auto& column = chosen->columns[columnIndex];
                const auto rows = combineBlocks(column.firstBlock, column.lastBlock);
                drawPanelText(
                    font,
                    rows,
                    column.layout,
                    frame.x + 10 + static_cast<int>(columnIndex) * chosen->columnWidth,
                    frame.y + contentTop);
            }
        });
}

std::shared_ptr<DrawableOnCall> makeRosterPanel(
    const ChessGameSession& session,
    PanelFrame frame)
{
    return makePanel(frame, [&session](int, const PanelFrame& panelFrame) {
        auto* font = Font::getInstance();
        constexpr int kFontSize = 20;
        font->draw("我方棋子", kFontSize + 4, panelFrame.x + 10, panelFrame.y + 5, {255, 255, 100, 255});

        std::map<std::pair<int, int>, int> totalCount;
        std::map<std::pair<int, int>, int> deployedCount;
        for (const auto& [instanceId, piece] : session.state().roster)
        {
            ++totalCount[{piece.roleId, piece.star}];
            if (piece.deployed)
            {
                ++deployedCount[{piece.roleId, piece.star}];
            }
        }

        struct Entry
        {
            std::string label;
            Color color;
        };
        std::vector<Entry> deployed;
        std::vector<Entry> bench;
        for (const auto& [roleAndStar, count] : totalCount)
        {
            const auto* role = session.content().role(roleAndStar.first);
            assert(role);
            int deployedForRole = deployedCount[roleAndStar];
            const auto tierColor = chessPieceTierColor(role->Cost);
            const std::string baseName = std::format("{}{}", role->Name, chessStars(roleAndStar.second));
            for (int index = 0; index < count; ++index)
            {
                if (deployedForRole > 0)
                {
                    deployed.push_back({"▶" + baseName, tierColor});
                    --deployedForRole;
                }
                else
                {
                    bench.push_back({
                        " " + baseName,
                        {
                            static_cast<unsigned char>(tierColor.r / 2 + 80),
                            static_cast<unsigned char>(tierColor.g / 2 + 80),
                            static_cast<unsigned char>(tierColor.b / 2 + 80),
                            200,
                        },
                    });
                }
            }
        }

        const int columnWidth = std::max(150, (panelFrame.w - 30) / 3);
        PanelColumnFlow flow{
            font,
            panelFrame,
            panelFrame.x + 10,
            panelFrame.y + kFontSize + 34,
            panelFrame.y + kFontSize + 34,
            panelFrame.y + panelFrame.h,
            columnWidth,
        };
        for (const auto& entry : deployed)
        {
            flow.line(entry.label, kFontSize, entry.color);
        }
        for (const auto& entry : bench)
        {
            flow.line(entry.label, kFontSize, entry.color);
        }
    },
        [&session](int) { return !session.state().roster.empty(); },
        {0, 0, 0, 160});
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
        Color outline{150, 160, 165, 230};
        if (selected_)
        {
            fill = {45, 65, 35, 235};
            outline = {255, 220, 70, 255};
        }
        else if (state_ == NodePass)
        {
            fill = {35, 45, 55, 235};
            outline = {220, 220, 190, 255};
        }
        else if (state_ == NodePress)
        {
            fill = {65, 60, 35, 240};
            outline = {255, 235, 150, 255};
        }
        Engine::getInstance()->fillRoundedRect(fill, x_, y_, w_, h_, 7);
        Engine::getInstance()->drawRoundedRect(outline, x_, y_, w_, h_, 7);
        const int textWidth = font_size_ * Font::getTextDrawSize(text_) / 2;
        Font::getInstance()->draw(
            text_,
            font_size_,
            x_ + std::max(6, (w_ - textWidth) / 2),
            y_ + (h_ - font_size_) / 2,
            {245, 240, 225, 255});
    }

private:
    Color fill_{15, 20, 24, 220};
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
        {10, 14, 18, 210},
        originX - 8,
        originY - 8,
        kPvpFormationPanelWidth + 16,
        kPvpFormationPanelHeight + 16,
        8);
    engine->drawRoundedRect(
        {80, 90, 95, 220},
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
        Engine::getInstance()->drawRoundedRect(
            occupied ? Color{220, 220, 200, 230} : Color{100, 100, 100, 210},
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
    engine->fillColor({0, 0, 0, 245}, 0, 0, engine->getUIWidth(), engine->getUIHeight());
    auto* font = Font::getInstance();
    font->draw("離線對戰", 36, 555, 24, {255, 220, 90, 255});
    font->draw("我的陣形", 24, 240, 68, {120, 220, 255, 255});
    font->draw("對手陣形", 24, 875, 68, {255, 140, 140, 255});
    drawPvpFormationFrame(45, 125, false);
    if (opponent)
    {
        drawPvpFormation(*opponent, content, 700, 125, true, {90, 30, 30, 210});
        font->draw("已驗證", 22, 925, 405, {100, 255, 120, 255});
    }
    else
    {
        drawPvpFormationFrame(700, 125, true);
        font->draw("尚未匯入對手存檔", 24, 835, 225, {210, 210, 200, 255});
    }
    font->draw("VS", 28, 618, 230, {255, 220, 90, 255});
    font->draw(
        std::format("我方出戰 {} 名", local.pieces.size()),
        20,
        235,
        445,
        local.pieces.empty() ? Color{255, 100, 100, 255} : Color{220, 220, 210, 255});
    const auto seedText = std::format("戰鬥種子：{}", battleSeed);
    const int seedTextWidth = 22 * Font::getTextDrawSize(seedText) / 2;
    font->draw(
        seedText,
        22,
        (engine->getUIWidth() - seedTextWidth) / 2,
        480,
        {255, 220, 120, 255});
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
        full_window_ = 1;
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
                ? Color{25, 65, 90, 220}
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
        engine->fillRoundedRect({12, 16, 22, 245}, 320, 12, 640, 54, 10);
        engine->drawRoundedRect({150, 160, 165, 240}, 320, 12, 640, 54, 10);
        engine->fillRoundedRect({12, 16, 22, 245}, 250, 500, 780, 195, 12);
        engine->drawRoundedRect({150, 160, 165, 240}, 250, 500, 780, 195, 12);
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
                {80, 175, 220, 255},
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
        std::vector<ChessMenuColumnRow> labelRows;
        std::vector<int> roleIds{-1, -1};
        std::vector<int> starRows{1, 1};
        std::vector<int> instanceRows{-1, -1};
        std::vector<int> shopSlots{-1, -1};

        const bool freeRefresh = observation.freeShopRefreshAvailable;
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
        data.outlineColors.push_back(freeRefresh ? Color{255, 215, 0, 255} : Color{});
        data.animateOutlines.push_back(freeRefresh);
        data.outlineThicknesses.push_back(freeRefresh ? 3 : 1);
        labelRows.push_back({{}, observation.shopLocked ? "[已鎖定] 點擊解鎖" : "[未鎖定] 點擊鎖定", {}, {}});
        data.colors.push_back(observation.shopLocked ? Color{255, 80, 80, 255} : Color{128, 128, 128, 255});
        data.outlineColors.push_back({});
        data.animateOutlines.push_back(false);
        data.outlineThicknesses.push_back(1);

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
            const auto outline = chessShopRowOutlineStyle(
                chessShopRowEmphasis(session_.state(), slot.roleId));
            data.outlineColors.push_back(outline.color);
            data.animateOutlines.push_back(outline.animated);
            data.outlineThicknesses.push_back(outline.thickness);
        }
        data.labels = alignedMenuLabels(labelRows);

        const auto panels = ChessScreenLayout::shopPanelsForMenu(
            ChessScreenLayout::contentMenuAnchor(),
            data.labels,
            32,
            8);
        const int selected = runIndexedMenu(
            std::format(
                "購買棋子　等級{}　${}　背包{}/{}",
                observation.level + 1,
                observation.money,
                benchCount(session_.state()),
                session_.content().balance().benchSize),
            data,
            32,
            static_cast<int>(data.labels.size()),
            ChessScreenLayout::contentMenuAnchor(),
            {
                makeRoleDetailPanel(session_, roleIds, starRows, instanceRows, panels.status),
                makeRosterPanel(session_, panels.owned),
                makeComboInfoPanel(session_, roleIds, panels.combo),
            },
            false,
            false);
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
        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, data.labels, 32, 8);
        const int selected = runIndexedMenu(
            actionType == ChessActionType::SellChess
                ? std::format("出售棋子　背包{}/{}", benchCount(session_.state()), session_.content().balance().benchSize)
                : "選擇棋子",
            data,
            32,
            12,
            anchor,
            {
                makeRoleDetailPanel(session_, roleIds, starRows, instanceIds, panels.status),
                makeComboInfoPanel(session_, roleIds, panels.combo),
            },
            false);
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
            data.colors.push_back(deployed ? Color{255, 215, 0, 255} : chessPieceTierColor(role->Cost));
            roleIds.push_back(piece->roleId);
            starRows.push_back(piece->star);
            instanceIds.push_back(piece->instanceId);
        }
        data.labels = alignedMenuLabels(labelRows);

        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, data.labels, 32, 8);
        const int selected = runIndexedMenu(
            std::format(
                "選擇出戰棋子 {}/{} 背包{}/{}",
                selectedIds.size(),
                observation.maximumDeployment,
                observation.roster.size() - selectedIds.size(),
                session_.content().balance().benchSize),
            data,
            32,
            12,
            anchor,
            {
                makeRoleDetailPanel(session_, roleIds, starRows, instanceIds, panels.status),
                makeComboInfoPanel(session_, roleIds, panels.combo),
            },
            false);
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
            const auto outline = chessBanManagementRowOutlineStyle(row.banned);
            data.outlineColors.push_back(outline.color);
            data.animateOutlines.push_back(outline.animated);
            data.outlineThicknesses.push_back(outline.thickness);
            roleIds.push_back(row.roleId);
        }
        data.labels = alignedMenuLabels(labelRows);

        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, data.labels, 32, 8);
        const int bannedCount = static_cast<int>(observation.bans.size());
        const int choice = runIndexedMenu(
            std::format(
                "禁棋管理　已禁{}/{}　剩餘{}",
                bannedCount,
                maximum,
                std::max(0, maximum - bannedCount)),
            data,
            32,
            12,
            anchor,
            {makeRoleDetailPanel(session_, roleIds, {}, {}, panels.status)},
            false);
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
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, data.labels, 32, 8);
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
        exitConfirmation = [remaining, anchor]() {
            auto prompt = std::make_shared<MenuText>(std::vector<std::string>{
                std::format("確認放棄剩餘{}次（不會保留）", remaining),
                "繼續選擇",
            });
            prompt->setFontSize(32);
            prompt->arrange(0, 0, 0, 42);
            prompt->setIsDark(1);
            return prompt->runAtPosition(anchor.x, anchor.y) == 0;
        };
    }
    const int choice = runIndexedMenu(
        title,
        data,
        32,
        12,
        anchor,
        {makeRoleDetailPanel(session_, roleIds, {}, {}, panels.status)},
        false,
        true,
        true,
        std::move(exitConfirmation));
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
    const bool legendaryUnlocked = shop.unlockFight > 0 && session_.state().fight >= shop.unlockFight;
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
                : Color{220, 220, 220, 255});
        }
        data.labels = alignedMenuLabels(labelRows);
        const int selected = runIndexedMenu(
            std::format("裝備管理　${}", session_.state().money),
            data,
            kChessCompactMenuPresentation.fontSize,
            static_cast<int>(data.labels.size()),
            ChessScreenLayout::contentMenuAnchor(),
            {},
            false,
            false);
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
            data.colors.push_back(row.owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255});
        }
        data.labels = contentSizedAlignedMenuLabels(labelRows);
        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto frame = ChessScreenLayout::browseDetailRegionForMenu(
            anchor,
            data.labels,
            kChessBrowseMenuPresentation.fontSize);
        std::vector<const EquipmentDef*> detailEquipments;
        std::vector<std::vector<std::string>> equippedByRows;
        detailEquipments.reserve(rows.size());
        equippedByRows.reserve(rows.size());
        for (const auto& row : rows)
        {
            detailEquipments.push_back(row.definition);
            equippedByRows.push_back(row.instanceId >= 0
                ? equipmentEquippedBy(session_, row.definition->itemId, row.instanceId)
                : std::vector<std::string>{});
        }
        auto detail = makeEquipmentDetailPanel(
            session_,
            std::move(detailEquipments),
            std::move(equippedByRows),
            frame);
        const int selected = runIndexedMenu(
            "裝備一覽",
            data,
            kChessBrowseMenuPresentation.fontSize,
            kChessBrowseMenuPresentation.itemsPerPage,
            anchor,
            {detail},
            false);
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
            equipmentData.colors.push_back({0, 255, 0, 255});
        }
        equipmentData.labels = alignedMenuLabels(equipmentRows);
        const int choice = runIndexedMenu("選擇裝備", equipmentData);
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
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, pieceData.labels, 32, 8);
    const int selected = runIndexedMenu(
        "選擇棋子",
        pieceData,
        32,
        12,
        anchor,
        {makeRoleDetailPanel(session_, roleIds, starRows, instanceIds, panels.status)},
        false);
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
        }
        data.labels = alignedMenuLabels(labelRows);
        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto frame = ChessScreenLayout::browseDetailRegionForMenu(anchor, data.labels, 32);
        std::vector<std::vector<std::string>> equippedByRows;
        equippedByRows.reserve(definitions.size());
        for (const auto* definition : definitions)
        {
            equippedByRows.push_back(equipmentEquippedBy(session_, definition->itemId));
        }
        auto detail = makeEquipmentDetailPanel(
            session_,
            definitions,
            std::move(equippedByRows),
            frame);
        const int choice = runIndexedMenu(
            std::format("神兵商店　${}", session_.state().money),
            data,
            kChessCompactMenuPresentation.fontSize,
            kChessBrowseMenuPresentation.itemsPerPage,
            anchor,
            {detail},
            false);
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
    const auto action = runContextActionMenu(items);
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
        const auto action = runContextActionMenu(buildChessSystemMenu());
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
    const auto frame = ChessScreenLayout::positionSwapPanel();
    auto panel = makePanel(frame, [currentEnabled](int selectedRow, const PanelFrame& panelFrame) {
        const bool willEnable = chessPositionSwapPreviewEnabled(currentEnabled, selectedRow);
        constexpr int fontSize = 32;
        PanelTextCursor cursor{Font::getInstance(), panelFrame.x + 10, panelFrame.y + 10};
        cursor.line(
            willEnable ? "狀態：開啟" : "狀態：關閉",
            fontSize,
            willEnable ? Color{0, 255, 0, 255} : Color{255, 80, 80, 255},
            6);
        cursor.line("戰鬥開始前可交換我方棋子位置", fontSize, {255, 215, 0, 255}, 6);
        cursor.line("點擊兩個我方棋子即可互換位置", fontSize, {255, 255, 255, 255}, 6);
        cursor.line("右鍵確認完成佈陣", fontSize, {255, 255, 255, 255}, 6);
    }, PanelVisibility{}, {0, 0, 0, 180}, {180, 170, 140, 200}, 6);
    SessionMenuData data;
    data.labels = {"  關閉  ", "  開啟  "};
    const auto anchor = ChessScreenLayout::modalMenuAnchor();
    const int selected = runIndexedMenu(
        "",
        data,
        kChessPositionSwapMenuPresentation.fontSize,
        kChessPositionSwapMenuPresentation.itemsPerPage,
        anchor,
        {panel},
        false,
        false);
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
    const auto frame = ChessScreenLayout::largeModalPreviewPanel();
    auto panel = makePanel(frame, [cost](int, const PanelFrame& panelFrame) {
        constexpr int fontSize = 22;
        PanelTextCursor cursor{Font::getInstance(), panelFrame.x + 10, panelFrame.y + 10};
        cursor.line("逆天改命", fontSize + 2, {255, 215, 0, 255}, 8);
        cursor.line(
            std::format("花費 ${}  重新抽取下場戰鬥的敵方隨機種子", cost),
            fontSize,
            {255, 255, 255, 255},
            12);
        cursor.line("敵人陣容會改變，地圖不保證改變", fontSize, {130, 220, 255, 255}, 8);
        cursor.line("確認後立即生效", fontSize, {100, 255, 100, 255}, 8);
    });
    auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認逆天改命", "取消"});
    menu->setFontSize(36);
    menu->arrange(0, 0, 0, 45);
    menu->addChild(panel);
    const auto anchor = ChessScreenLayout::modalMenuAnchor();
    menu->runAtPosition(anchor.x, anchor.y);
    if (menu->getResult() == 0)
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
    SessionMenuData data;
    std::vector<std::pair<std::string, std::string>> labelRows;
    std::vector<const ComboDef*> rows;
    const std::unordered_set<int> poolRoles(
        session_.content().poolRoleIds().begin(),
        session_.content().poolRoleIds().end());
    std::map<int, int> deployedStarByRole;
    for (const auto& [instanceId, piece] : session_.state().roster)
    {
        if (piece.deployed)
        {
            deployedStarByRole[piece.roleId] = std::max(deployedStarByRole[piece.roleId], piece.star);
        }
    }
    for (const auto& combo : session_.content().combos())
    {
        if (std::ranges::none_of(combo.memberRoleIds, [&](int roleId) { return poolRoles.contains(roleId); }))
        {
            continue;
        }
        const auto progress = evaluateChessComboProgress(session_.state(), session_.content(), combo);
        labelRows.push_back({combo.name, formatChessComboProgressCount(progress)});
        data.colors.push_back(progress.physicalCount > 0
            ? Color{0, 255, 0, 255}
            : Color{180, 180, 180, 255});
        rows.push_back(&combo);
    }
    data.labels = buildAlignedComboCatalogLabels(labelRows, menuDisplayWidth);
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto frame = ChessScreenLayout::browseDetailRegionForMenu(
        anchor,
        data.labels,
        kChessBrowseMenuPresentation.fontSize);
    auto detail = makePanel(frame, [this, rows, poolRoles, deployedStarByRole](int row, const PanelFrame& panelFrame) {
        if (row < 0 || row >= static_cast<int>(rows.size()))
        {
            return;
        }
        const auto& combo = *rows[row];
        constexpr int preferredFontSize = 24;
        constexpr int minimumFontSize = 14;
        auto* font = Font::getInstance();
        font->draw(combo.name, preferredFontSize + 4,
            panelFrame.x + 10, panelFrame.y + 10, {255, 255, 100, 255});

        const auto progress = evaluateChessComboProgress(session_.state(), session_.content(), combo);
        std::vector<PanelVisualTextRow> memberRows;
        appendPanelTextRow(memberRows, "成員:", {200, 200, 200, 255}, 0, 0, 0, 2);
        int totalBonus = 0;
        for (const int roleId : combo.memberRoleIds)
        {
            if (!poolRoles.contains(roleId))
            {
                continue;
            }
            const auto* role = session_.content().role(roleId);
            assert(role);
            const auto foundStar = deployedStarByRole.find(roleId);
            const bool deployed = foundStar != deployedStarByRole.end();
            std::string starSuffix;
            if (deployed && combo.starSynergyBonus)
            {
                starSuffix = std::format(" ★{}", foundStar->second);
                totalBonus += foundStar->second - 1;
            }
            appendPanelTextRow(
                memberRows,
                std::format("  {} ({}費){}{}", role->Name, role->Cost, deployed ? " ✓" : "", starSuffix),
                deployed ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255},
                0,
                0,
                0,
                1);
        }

        std::vector<PanelVisualTextRow> thresholdRows;
        appendPanelTextRow(
            thresholdRows,
            combo.isAntiCombo ? "條件:" : "閾值:",
            {200, 200, 200, 255});
        if (combo.starSynergyBonus)
        {
            appendPanelTextRow(
                thresholdRows,
                totalBonus > 0
                    ? std::format("★ 成員星級計人數，當前+{}人", totalBonus)
                    : "★ 成員星級計人數（2★=2人）",
                {255, 200, 50, 255},
                -4,
                0,
                4,
                2);
            appendPanelTextRow(
                thresholdRows,
                "每額外★計入1羈絆人數",
                {180, 160, 80, 255},
                -6,
                2);
        }
        for (const auto& threshold : combo.thresholds)
        {
            const bool active = progress.effectiveCount >= threshold.count;
            appendPanelTextRow(
                thresholdRows,
                std::format("{}人: {}{}", threshold.count, threshold.name, active ? " ✓" : ""),
                active ? Color{0, 255, 0, 255} : Color{255, 200, 100, 255},
                0,
                0,
                8,
                1);
            const auto document = buildEffectDescriptionDocument({
                EffectDescriptionContainerKind::ComboThreshold,
                threshold.rules,
            });
            const auto rendered = renderEffectDescription(
                document,
                EffectDescriptionStyle::Full,
                {});
            appendRenderedEffectDescriptionRows(
                thresholdRows,
                rendered,
                active ? Color{180, 220, 255, 255} : Color{200, 200, 200, 255},
                -2,
                0,
                2);
        }

        constexpr int contentTop = 45;
        constexpr int contentBottomInset = 10;
        constexpr int memberWidth = 270;
        const int thresholdWidth = panelFrame.w - 300;
        const int availableHeight = panelFrame.h - contentTop - contentBottomInset;
        const auto memberSourceRows = panelTextLayoutRows(memberRows);
        const auto thresholdSourceRows = panelTextLayoutRows(thresholdRows);
        std::optional<std::pair<PanelTextLayout, PanelTextLayout>> fitted;
        for (int fontSize = preferredFontSize; fontSize >= minimumFontSize; --fontSize)
        {
            auto memberLayout = layoutPanelText(memberSourceRows, memberWidth, fontSize);
            auto thresholdLayout = layoutPanelText(thresholdSourceRows, thresholdWidth, fontSize);
            if (memberLayout.height <= availableHeight
                && thresholdLayout.height <= availableHeight)
            {
                fitted = std::pair{
                    std::move(memberLayout),
                    std::move(thresholdLayout),
                };
                break;
            }
        }
        assert(fitted && "formal Full combo descriptions must fit the detail panel");
        drawPanelText(font, memberRows, fitted->first,
            panelFrame.x + 10, panelFrame.y + contentTop);
        drawPanelText(font, thresholdRows, fitted->second,
            panelFrame.x + 290, panelFrame.y + contentTop);
    });
    runIndexedMenu(
        "羈絆一覽",
        data,
        kChessBrowseMenuPresentation.fontSize,
        kChessBrowseMenuPresentation.itemsPerPage,
        anchor,
        {detail},
        false);
}

void ChessGuiSessionAdapter::viewChessPool()
{
    SessionMenuData data;
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
    }
    data.labels = alignedMenuLabels(labelRows);
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto panels = ChessScreenLayout::shopPanelsForMenu(anchor, data.labels, 32, 8);
    runIndexedMenu(
        "棋子一覽",
        data,
        kChessCompactMenuPresentation.fontSize,
        kChessCompactMenuPresentation.itemsPerPage,
        anchor,
        {
            makeRoleDetailPanel(session_, roleIds, {}, {}, panels.status),
            makeComboInfoPanel(session_, roleIds, panels.combo),
        },
        false);
}

void ChessGuiSessionAdapter::viewNeigong()
{
    SessionMenuData data;
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
        data.colors.push_back(owned ? Color{0, 255, 0, 255} : Color{120, 120, 120, 255});
        rows.push_back(&neigong);
    }
    data.labels = alignedMenuLabels(labelRows);
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto frame = ChessScreenLayout::browseDetailRegionForMenu(
        anchor,
        data.labels,
        kChessBrowseMenuPresentation.fontSize);
    auto detail = makeNeigongDetailPanel(session_, std::move(rows), frame, true);
    runIndexedMenu(
        "內功一覽",
        data,
        kChessBrowseMenuPresentation.fontSize,
        kChessBrowseMenuPresentation.itemsPerPage,
        anchor,
        {detail},
        false);
}

void ChessGuiSessionAdapter::viewEffects()
{
    auto entries = chessEffectCatalog(session_.content());
    const auto effectCount = entries.size();
    SessionMenuData data;
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
    }
    data.labels = contentSizedAlignedMenuLabels(labelRows);

    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto frame = ChessScreenLayout::browseDetailRegionForMenu(
        anchor,
        data.labels,
        kChessCompactMenuPresentation.fontSize);
    auto detail = makePanel(
        frame,
        [entries = std::move(entries)](int row, const PanelFrame& panelFrame)
        {
            if (!isPanelRowInRange(row, static_cast<int>(entries.size()))) return;
            const auto& entry = entries[row];
            const auto sourceColor = effectCatalogSourceColor(entry.source);
            std::vector<PanelVisualTextRow> rows;
            appendPanelTextRow(
                rows,
                entry.sourceName,
                {255, 255, 100, 255},
                5,
                0,
                0,
                4);
            appendPanelTextRow(
                rows,
                std::format("{}效果 · 配置 {}/{}",
                    chessEffectCatalogSourceLabel(entry.source),
                    entry.ruleOrdinal,
                    entry.sourceRuleCount),
                sourceColor,
                1,
                0,
                0,
                3);
            appendPanelTextRow(
                rows,
                entry.sourceContext,
                {185, 185, 185, 255},
                -2,
                0,
                0,
                12);
            appendRenderedEffectDescriptionRows(
                rows,
                entry.effects,
                {225, 225, 225, 255},
                0,
                3);
            const auto layout = fitVisualPanelText(
                rows,
                panelFrame.w - 20,
                panelFrame.h - 20,
                22,
                14);
            drawPanelText(
                Font::getInstance(),
                rows,
                layout,
                panelFrame.x + 10,
                panelFrame.y + 10);
        });
    runIndexedMenu(
        std::format("效果全覽 · {}項", effectCount),
        data,
        kChessCompactMenuPresentation.fontSize,
        kChessCompactMenuPresentation.itemsPerPage,
        anchor,
        {detail},
        true);
}

void ChessGuiSessionAdapter::showGameGuide()
{
    const auto frame = ChessScreenLayout::fullContentRegion();
    const auto sections = buildChessGameGuideSections(session_.content());
    auto panel = makePanel(frame, [sections](int, const PanelFrame& panelFrame) {
        auto* font = Font::getInstance();
        constexpr int titleFontSize = 22;
        constexpr int bodyFontSize = 19;
        constexpr int bodySpacing = 4;
        const int columnWidth = (panelFrame.w - 90) / 2;

        font->draw(
            "珍瓏棋局 · 遊戲說明",
            titleFontSize + 4,
            panelFrame.x + 30,
            panelFrame.y + 14,
            {255, 255, 100, 255});

        const auto lineColor = [](ChessGameGuideLineTone tone) {
            switch (tone)
            {
            case ChessGameGuideLineTone::Standard: return Color{220, 220, 220, 255};
            case ChessGameGuideLineTone::Information: return Color{200, 230, 255, 255};
            case ChessGameGuideLineTone::Highlight: return Color{255, 210, 150, 255};
            }
            std::unreachable();
        };

        const auto measureWrappedLines = [columnWidth](const std::string& text) {
            const int displayWidth = displayTextUnitsForPixelWidth(bodyFontSize, columnWidth - 24);
            return std::max(1, static_cast<int>(wrapDisplayText(text, displayWidth).size()));
        };

        const auto measureSectionHeight = [&](const ChessGameGuideSection& section) {
            int height = titleFontSize + 8;
            for (const auto& line : section.lines)
            {
                height += measureWrappedLines(line.text) * (bodyFontSize + bodySpacing);
            }
            return height + 8;
        };

        PanelColumnFlow flow{
            font,
            panelFrame,
            panelFrame.x + 30,
            panelFrame.y + 58,
            panelFrame.y + 58,
            panelFrame.y + panelFrame.h - 42,
            columnWidth,
        };
        for (const auto& section : sections)
        {
            if (!flow.ensureSpace(measureSectionHeight(section)))
            {
                break;
            }

            PanelTextCursor cursor{font, flow.x, flow.y};
            cursor.line(section.title, titleFontSize, {255, 255, 100, 255}, 8);
            for (const auto& line : section.lines)
            {
                drawWrappedLines(
                    cursor,
                    line.text,
                    bodyFontSize,
                    lineColor(line.tone),
                    columnWidth - 12,
                    bodySpacing);
            }
            cursor.skip(8);
            flow.y = cursor.y;
        }

        font->draw(
            "點擊任意處返回",
            bodyFontSize - 1,
            panelFrame.x + 30,
            panelFrame.y + panelFrame.h - 28,
            {150, 150, 150, 255});
    });
    auto box = std::make_shared<DismissibleTextBox>();
    box->setText("　");
    box->setHaveBox(false);
    box->setFontSize(1);
    box->addChild(panel);
    box->runAtPosition(0, 0);
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
        }
        data.labels = alignedMenuLabels(labelRows);
        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        const auto frame = ChessScreenLayout::browseDetailRegionForMenu(
            anchor,
            data.labels,
            kChessChallengeMenuPresentation.fontSize);
        auto detail = makePanel(frame, [this, rows](int row, const PanelFrame& panelFrame) {
            if (row < 0 || row >= static_cast<int>(rows.size()))
            {
                return;
            }
            const auto& challenge = *rows[row];
            constexpr int fontSize = 22;
            PanelTextCursor cursor{Font::getInstance(), panelFrame.x + 10, panelFrame.y + 10};
            cursor.line(challenge.name, fontSize + 4, {255, 255, 100, 255}, 6);
            cursor.line(challenge.description, fontSize, {200, 200, 200, 255}, 8);
            if (session_.state().completedChallengeNames.contains(challenge.name))
            {
                cursor.line(chessChallengeCompletionLabel(true), fontSize, {0, 255, 0, 255}, 8);
            }

            cursor.line("敵方陣容:", fontSize, {255, 150, 150, 255});
            for (const auto& enemy : challenge.enemies)
            {
                cursor.line(
                    chessChallengeEnemyDescription(session_.content(), enemy),
                    fontSize - 2,
                    {220, 180, 180, 255},
                    2);
            }
            cursor.skip(8);

            cursor.line("通關獎勵(擇一):", fontSize, {100, 255, 100, 255});
            for (const auto& reward : challenge.rewards)
            {
                cursor.line(
                    "  " + chessChallengeRewardDescription(session_.content(), reward),
                    fontSize - 2,
                    {200, 255, 200, 255},
                    2);
            }
        });
        const int choice = runIndexedMenu(
            "遠征挑戰",
            data,
            kChessChallengeMenuPresentation.fontSize,
            kChessChallengeMenuPresentation.itemsPerPage,
            anchor,
            {detail},
            false);
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
            }
            else if (option.kind == ChessRewardKind::Piece)
            {
                const auto* role = session_.content().role(option.value);
                assert(role);
                labelRows.push_back({{}, role->Name, chessStars(1), chessRewardOptionCostColumn(option)});
                data.colors.push_back(chessPieceTierColor(role->Cost));
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
                data.colors.push_back({255, 255, 100, 255});
            }
        }
        data.labels = buildAlignedChessRewardMenuLabels(
            pending.kind,
            labelRows,
            menuDisplayWidth);
        const auto menuPresentation = chessRewardMenuPresentation(
            pending,
            static_cast<int>(data.labels.size()));

        const auto anchor = ChessScreenLayout::contentMenuAnchor();
        std::vector<std::shared_ptr<DrawableOnCall>> detailPanels;
        if (pending.kind == ChessRewardKind::Equipment)
        {
            std::vector<const EquipmentDef*> equipments;
            equipments.reserve(availableOptions.size());
            for (const auto& option : availableOptions)
            {
                assert(option.kind == ChessRewardKind::Equipment);
                const auto* equipment = equipmentDefinition(session_.content(), option.value);
                assert(equipment);
                equipments.push_back(equipment);
            }
            detailPanels.push_back(makeEquipmentDetailPanel(
                session_,
                std::move(equipments),
                {},
                ChessScreenLayout::browseDetailRegionForMenu(
                    anchor,
                    data.labels,
                    menuPresentation.fontSize)));
        }
        else if (pending.kind == ChessRewardKind::InternalSkill)
        {
            const auto detailFrame = ChessScreenLayout::browseDetailRegionForMenu(
                anchor,
                data.labels,
                menuPresentation.fontSize);
            std::vector<const NeigongDef*> neigongs;
            neigongs.reserve(availableOptions.size());
            for (const auto& option : availableOptions)
            {
                const auto found = std::ranges::find(
                    session_.content().neigong(),
                    option.value,
                    &NeigongDef::magicId);
                assert(found != session_.content().neigong().end());
                neigongs.push_back(&*found);
            }
            detailPanels.push_back(makeNeigongDetailPanel(
                session_,
                std::move(neigongs),
                detailFrame,
                false));
        }
        else if (pending.kind == ChessRewardKind::Piece || pending.kind == ChessRewardKind::StarUpgrade)
        {
            std::vector<int> roleIds;
            std::vector<int> starRows;
            std::vector<int> instanceIds;
            roleIds.reserve(availableOptions.size());
            starRows.reserve(availableOptions.size());
            instanceIds.reserve(availableOptions.size());
            for (const auto& option : availableOptions)
            {
                const auto preview = chessRewardRolePreview(session_.state(), option);
                assert(preview);
                roleIds.push_back(preview->roleId);
                starRows.push_back(preview->star);
                instanceIds.push_back(preview->instanceId);
            }
            const auto panels = ChessScreenLayout::shopPanelsForMenu(
                anchor,
                data.labels,
                menuPresentation.fontSize,
                8);
            detailPanels.push_back(makeRoleDetailPanel(
                session_,
                roleIds,
                starRows,
                instanceIds,
                panels.status));
            if (chessRewardShowsComboPanel(pending.kind))
            {
                detailPanels.push_back(makeComboInfoPanel(session_, std::move(roleIds), panels.combo));
            }
        }

        std::string title = "選擇獎勵";
        if (pending.kind == ChessRewardKind::Equipment) title = "選擇裝備獎勵";
        else if (pending.kind == ChessRewardKind::InternalSkill) title = "內功對所有成員自動生效";
        else if (pending.kind == ChessRewardKind::Piece) title = "選擇棋子";
        else if (pending.kind == ChessRewardKind::StarUpgrade)
            title = chessStarUpgradeRewardTitle(session_.state(), pending);
        const int choice = runIndexedMenu(
            title,
            data,
            menuPresentation.fontSize,
            menuPresentation.itemsPerPage,
            anchor,
            detailPanels,
            false,
            true,
            false);
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
    for (const int mapId : descriptor.candidateIds)
    {
        data.labels.push_back(chessBattleMapDisplayName(session_.content(), mapId));
        data.colors.push_back({230, 230, 220, 255});
    }
    const int choice = runIndexedMenu(
        "選擇戰場",
        data,
        32,
        12,
                ChessScreenLayout::contentMenuAnchor(),
        {makeBattleMapPreviewPanel(session_, descriptor.candidateIds)},
        false,
        true,
        false);
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
    const auto anchor = ChessScreenLayout::contentMenuAnchor();
    const auto frame = ChessScreenLayout::browseDetailRegionForMenu(anchor, data.labels, 32);
    const auto panel = makeRoleDetailPanel(session_, roleIds, starRows, {}, frame);
    const int first = runIndexedMenu("第一個位置", data, 32, 12, anchor, {panel});
    if (first < 0) return;
    const int second = runIndexedMenu("第二個位置", data, 32, 12, anchor, {panel});
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
        if (legendaryShop.unlockFight > 0 && session_.state().fight == legendaryShop.unlockFight)
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

void ChessGuiSessionAdapter::showContextMenu()
{
    for (;;)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return;
        }
        if (drainRewards() == ChessGuiFlowResult::Aborted)
        {
            return;
        }
        const bool banEnabled = ChessManagementRules::maximumBanCount(session_.state(), session_.content()) > 0;
        const auto action = runContextActionMenu(buildChessContextMenu(banEnabled));
        if (!action)
        {
            return;
        }
        switch (*action)
        {
        case ChessContextMenuAction::BuyChess:
            showShop();
            break;
        case ChessContextMenuAction::SellChess:
            chooseChess(ChessActionType::SellChess);
            break;
        case ChessContextMenuAction::SelectForBattle:
            chooseDeployment();
            break;
        case ChessContextMenuAction::EnterBattle:
            if (session_.state().campaignComplete)
            {
                showChessMessage("恭喜通關！");
            }
            else if (const auto descriptor = legalAction(session_, ChessActionType::PrepareBattle))
            {
                if (chooseAndSubmit(*descriptor) == ChessGuiFlowResult::Aborted)
                {
                    return;
                }
            }
            else
            {
                showChessMessage("出戰棋子不足，請先選擇出戰陣容");
            }
            break;
        case ChessContextMenuAction::OpenOfflineBattle:
            showOfflineBattle();
            break;
        case ChessContextMenuAction::BuyExp:
        {
            if (session_.state().level >= session_.content().balance().maxLevel)
            {
                showChessMessage("已達最高等級");
                break;
            }
            const auto frame = ChessScreenLayout::largeModalPreviewPanel();
            auto panel = makePanel(frame, [this](int, const PanelFrame& panelFrame) {
                constexpr int fontSize = 20;
                const auto text = buildChessBuyExpPreviewText(session_.state(), session_.content());
                PanelTextCursor cursor{Font::getInstance(), panelFrame.x + 10, panelFrame.y + 10};
                cursor.line(text.header, fontSize, {255, 215, 0, 255}, 6);
                cursor.line(text.costLine, fontSize, {255, 255, 255, 255}, 12);
                const int deploymentY = cursor.y;
                cursor.line(text.deploymentLine, fontSize, {130, 220, 255, 255}, 12);
                if (!text.deploymentNext.empty())
                {
                    cursor.font->draw(
                        text.deploymentNext,
                        fontSize,
                        cursor.x + fontSize * 8,
                        deploymentY,
                        {100, 255, 100, 255});
                }
                cursor.line("當前商店權重:", fontSize, {160, 160, 160, 255}, 6);
                cursor.line(text.currentWeights, fontSize, {255, 255, 255, 255}, 10);
                cursor.line("下一級商店權重:", fontSize, {160, 160, 160, 255}, 6);
                cursor.line(text.nextWeights, fontSize, {100, 255, 100, 255}, 6);
            });
            auto menu = std::make_shared<MenuText>(std::vector<std::string>{"確認購買", "取消"});
            menu->setFontSize(36);
            menu->arrange(0, 0, 0, 45);
            menu->addChild(panel);
            const auto anchor = ChessScreenLayout::modalMenuAnchor();
            menu->runAtPosition(anchor.x, anchor.y);
            if (menu->getResult() == 0)
            {
                ChessAction buyExperience;
                buyExperience.type = ChessActionType::BuyExp;
                const auto result = submitGuiAction(buyExperience);
                if (!result.accepted)
                {
                    showChessMessage(result.description);
                }
                else if (std::ranges::contains(
                    result.events,
                    ChessSemanticEventType::LevelChanged,
                    &ChessSemanticEvent::type))
                {
                    showChessMessage(std::format(
                        "升級！等級{} 經驗{}/{}",
                        session_.state().level + 1,
                        session_.state().experience,
                        ChessManagementRules::experienceForNextLevel(session_.state(), session_.content())));
                }
                else
                {
                    showChessMessage(std::format(
                        "經驗{}/{}",
                        session_.state().experience,
                        ChessManagementRules::experienceForNextLevel(session_.state(), session_.content())));
                }
            }
            break;
        }
        case ChessContextMenuAction::ManageBans:
            showBanManagement();
            break;
        case ChessContextMenuAction::OpenEquipmentMenu:
            showEquipmentMenu();
            break;
        case ChessContextMenuAction::OpenOverviewMenu:
            showOverviewMenu();
            break;
        case ChessContextMenuAction::ShowExpeditionChallenge:
        {
            ChessLegalActionDescriptor descriptor{ChessActionType::StartChallenge};
            descriptor.candidateStableIds = chessChallengeBrowseNames(session_.content());
            if (descriptor.candidateStableIds.empty())
            {
                break;
            }
            if (chooseChallenge(descriptor) == ChessGuiFlowResult::Aborted)
            {
                return;
            }
            break;
        }
        case ChessContextMenuAction::OpenSystemMenu:
            if (runSystemMenu())
            {
                return;
            }
            break;
        default:
            assert(false);
            break;
        }
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return;
        }
    }
}

}
