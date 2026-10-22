#include "ChessGuiSessionAdapter.h"

#include "BattleSceneHades.h"
#include "battle/BattlePresentationVisuals.h"
#include "BattleStatsView.h"
#include "ChessBattleOverview.h"
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
#include "ChessManagementRules.h"
#include "ChessMenuFormatting.h"
#include "ChessGameGuide.h"
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
    bool overviewMode{};
    bool doubleClickConfirms{};
    std::string actionLabel = "選擇";
    std::string headerNote;
    ChessSelectionPosition* position{};
};


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

ChessSelectionImage effectCatalogImage(const ChessGameContent& content, const ChessEffectCatalogEntry& entry)
{
    switch (entry.source)
    {
    case ChessEffectCatalogSource::Magic:
        for (const auto& [id, item] : content.items())
            if (item.magicId == entry.sourceId) return {"item", id};
        return chessSelectionIcon(ChessUiStyle::Icon::Ultimate);
    case ChessEffectCatalogSource::Equipment:
    case ChessEffectCatalogSource::EquipmentSynergy:
        return {"item", entry.sourceId};
    case ChessEffectCatalogSource::Neigong:
    {
        const auto found = std::ranges::find(content.neigong(), entry.sourceId, &NeigongDef::magicId);
        assert(found != content.neigong().end());
        return {"item", found->itemId};
    }
    case ChessEffectCatalogSource::ComboThreshold:
    {
        const auto found = std::ranges::find(content.combos(), entry.sourceId, &ComboDef::id);
        assert(found != content.combos().end());
        return {"chess-synergies", found->iconId};
    }
    }
    std::unreachable();
}

std::optional<ChessLegalActionDescriptor> legalAction(
    const ChessGameSession& session,
    ChessActionType type)
{
    const auto legal = session.legalActions();
    const auto found = std::ranges::find(legal, type, &ChessLegalActionDescriptor::type);
    return found == legal.end() ? std::nullopt : std::optional(*found);
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
    card.effectRows = panelTextRowsForEquipment(chessEquipmentMetadata(session.content(), equipment.itemId, EffectDescriptionStyle::Full));
    return card;
}

ChessSelectionItem neigongCard(const ChessGameSession& session, const NeigongDef& neigong)
{
    ChessSelectionItem card{
        .title = neigong.name,
        .subtitle = session.state().obtainedNeigongIds.contains(neigong.magicId) ? "已獲得" : "",
        .badge = chessRewardTierLabel(neigong.tier),
        .image = {"item", neigong.itemId},
        .accent = chessRewardTierColor(neigong.tier),
        .category = chessRewardTierLabel(neigong.tier),
        .summary = joinEffectDescriptionRows(describeGameplayEffects(neigong.effects, EffectDescriptionStyle::Compact), " · "),
    };
    card.effectRows = panelTextRowsForEffectDescription(describeGameplayEffects(neigong.effects, EffectDescriptionStyle::Full));
    for (auto& row : card.effectRows) row.tone = PanelTextTone::Effect;
    return card;
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
    menu->setDoubleClickConfirms(data.doubleClickConfirms);
    menu->setCommandMode(data.commandMode || (!detailRenderer && data.cards.empty()));
    if (data.shortChoice) menu->setShortChoiceMode();
    if (data.overviewMode) menu->setOverviewMode();
    menu->setActionLabel(data.actionLabel);
    menu->setHeaderNote(data.headerNote);
    if (data.position) menu->restorePosition(*data.position);
    menu->setExitable(exitable);
    menu->setSearchEnabled(showSearch || data.labels.size() > 12);
    menu->setExitConfirmation(std::move(exitConfirmation));
    menu->setDetailRenderer(std::move(detailRenderer));
    menu->run();
    if (data.position) *data.position = menu->position();
    return menu->getResult();
}

std::optional<ChessContextMenuAction> runContextActionMenu(
    const std::string& title, const std::vector<ChessContextMenuItem>& items, ChessSelectionPosition& position)
{
    SessionMenuData data;
    data.position = &position;
    data.labels = chessContextMenuLabels(items);
    data.commandMode = true;
    for (const auto& item : items)
    {
        using Icon = ChessUiStyle::Icon;
        Icon icon = Icon::Book;
        switch (item.action)
        {
        case ChessContextMenuAction::RerollBattleSeed: icon = Icon::Dice; break;
        case ChessContextMenuAction::ManageBans: icon = Icon::Ban; break;
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

constexpr int kPvpFormationTop = 158;
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
    drawPvpFormationFrame(45, kPvpFormationTop, false);
    if (opponent)
    {
        drawPvpFormation(*opponent, content, 700, kPvpFormationTop, true, {64, 45, 39, 200});
        font->draw("已驗證", 20, 925, 418, ChessUiStyle::positive);
    }
    else
    {
        drawPvpFormationFrame(700, kPvpFormationTop, true);
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
                kPvpFormationTop + offset.y);
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
            418,
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

void autoSavePostBattleGuiState(const ChessGameSession& session, ChessSessionPhase previousPhase)
{
    if (session.isPostBattleAutosavePoint(previousPhase))
    {
        UISave::autoSave();
    }
}

}

ChessActionResult ChessGuiSessionAdapter::submitGuiAction(const ChessAction& action)
{
    const auto previousPhase = session_.state().phase;
    auto result = session_.submitAndDrain(action);
    if (result.accepted)
    {
        autoSavePostBattleGuiState(session_, previousPhase);
    }
    return result;
}

void ChessGuiSessionAdapter::showBanManagement()
{
    const int maximum = ChessManagementRules::maximumBanCount(session_.state(), session_.content());
    ChessSelectionPosition position;
    for (;;)
    {
        const auto observation = session_.observe();
        if (observation.seenRoles.empty())
        {
            showChessMessage("尚未遇到可封禁的棋子；商店出現的角色會收錄到封禁清單。");
            return;
        }

        const auto rows = buildChessBanManagementRows(observation, session_.content());
        SessionMenuData data;
        data.position = &position;
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
        data.actionLabel = "封禁";
        data.doubleClickConfirms = true;
        data.headerNote = std::format("已禁 {}/{} · 剩餘 {} · 雙擊棋子或選取後按封禁 · 本局永久",
            observation.bans.size(), maximum, std::max(0, maximum - static_cast<int>(observation.bans.size())));
        for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        {
            data.cards[i].subtitle = rows[i].banned ? "已封禁 · 本局永久"
                : static_cast<int>(observation.bans.size()) >= maximum ? "封禁名額已滿" : "可封禁";
            data.cards[i].character->context = data.cards[i].subtitle;
            data.cards[i].actionEnabled = !rows[i].banned && static_cast<int>(observation.bans.size()) < maximum;
        }

        const int bannedCount = static_cast<int>(observation.bans.size());
        const int choice = runSelection("封禁管理", data, {}, false);
        if (choice < 0)
        {
            return;
        }
        assert(!rows[choice].banned && bannedCount < maximum);

        ChessAction action;
        action.type = ChessActionType::AddBan;
        action.roleId = rows[choice].roleId;
        const auto result = submitGuiAction(action);
        if (!result.accepted)
        {
            showChessMessage(result.description);
            continue;
        }
    }
}

bool ChessGuiSessionAdapter::chooseForcedBans()
{
    const auto pending = session_.state().pendingRewards.front();
    assert(pending.kind == ChessRewardKind::ForcedBan);
    std::vector<int> roleIds;
    for (const auto& option : pending.options) roleIds.push_back(option.value);
    ChessSelectionPosition position;
    for (;;)
    {
        SessionMenuData data;
        data.position = &position;
        data.cards = roleCards(session_, roleIds);
        data.actionLabel = "封禁";
        data.doubleClickConfirms = true;
        const int remaining = session_.state().pendingRewards.front().parameter;
        data.headerNote = std::format(
            "≤{}費 · 剩餘 {}/{} · 雙擊棋子或選取後按封禁 · 本局永久",
            pending.eligibleTiers.front(), remaining, pending.choiceCount);
        for (auto& card : data.cards)
        {
            data.labels.push_back(card.title);
            card.actionEnabled = !card.character->banned;
            card.subtitle = card.character->banned ? "已封禁 · 本局永久" : "可封禁";
            card.character->context = card.subtitle;
        }
        const int choice = runSelection("禁棋選擇", data, {}, false, true, [remaining] {
            return runChessChoiceMenu("放棄本次禁棋機會？", {
                std::format("確認放棄剩餘{}次（不會保留）", remaining), "繼續選擇"},
                ChessChoiceStyle::DestructiveConfirmation) == 0;
        });
        if (choice < 0) return false;
        const auto result = submitGuiAction({.type = ChessActionType::AddBan, .roleId = roleIds[choice]});
        if (!result.accepted) showChessMessage(result.description);
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted
            || session_.state().phase != ChessSessionPhase::RewardChoice
            || session_.state().pendingRewards.front().id != pending.id)
            return true;
    }
}

void ChessGuiSessionAdapter::showEquipmentMenu()
{
    const bool legendaryUnlocked = ChessManagementRules::legendaryShopUnlocked(session_.state(), session_.content());
    if (!legendaryUnlocked)
    {
        showEquipmentInventory();
        return;
    }
    ChessSelectionPosition position;
    for (;;)
    {
        const auto items = buildChessEquipmentMenu(legendaryUnlocked);
        SessionMenuData data;
        data.position = &position;
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
            break;
        case ChessContextMenuAction::BuyLegendaryEquipment:
        {
            if (openPreparationCommand(items[selected].action)) return;
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
    ChessSelectionPosition position;
    for (;;)
    {
        SessionMenuData data;
        data.position = &position;
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
    ChessSelectionPosition position;
    for (;;)
    {
        SessionMenuData data;
        data.position = &position;
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
    ChessSelectionPosition position;
    for (;;)
    {
        const auto action = runContextActionMenu("圖簽", items, position);
        if (!action) return;
        switch (*action)
        {
        case ChessContextMenuAction::ViewCombos: viewCombos(); break;
        case ChessContextMenuAction::ViewChessPool: viewChessPool(); break;
        case ChessContextMenuAction::ViewNeigong: viewNeigong(); break;
        case ChessContextMenuAction::ViewEffects: viewEffects(); break;
        default: assert(false); break;
        }
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted) return;
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
    int activeChild{};
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
        screen->setStartItem(activeChild);
        screen->run();
        activeChild = screen->getActiveChildIndex();
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
            if (ChessGuiSessionAdapter(*battleSession, {local.talent, opponent->talent}).runPreparedBattle()
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
    ChessSelectionPosition position;
    for (;;)
    {
        const auto action = runContextActionMenu("系統選單", buildChessSystemMenu(), position);
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
            .image = effectCatalogImage(session_.content(), entry),
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
    std::vector<ChessSelectionItem> items;
    for (const auto& section : sections)
    {
        ChessSelectionItem item{.title = section.title,
            .subtitle = std::format("本局規則 · {} · {}", chessDifficultyDisplayName(session_.content().difficulty()),
                chessTalentName(session_.state().talent))};
        ChessOverviewColumn column{.width = 1200};
        for (const auto& line : section.lines)
            column.rows.push_back({line.text, line.tone == ChessGameGuideLineTone::Highlight ? ChessUiStyle::gold
                : line.tone == ChessGameGuideLineTone::Information ? ChessUiStyle::context : ChessUiStyle::ink});
        item.overview.push_back(std::move(column));
        items.push_back(std::move(item));
    }
    auto screen = std::make_shared<ChessSelectionScreen>("遊戲說明", std::move(items));
    screen->setTabbedMode();
    screen->run();
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
    ChessSelectionPosition position;
    for (;;)
    {
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }

        SessionMenuData data;
        data.position = &position;
        data.actionLabel = "挑戰";
        data.overviewMode = true;
        data.headerNote = "不推進主線 · 僅首勝領獎與累計勝場 · 獎勵依條件提供";
        std::vector<ChessMenuColumnRow> labelRows;
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
            ChessSelectionItem card{
                .title = found->name,
                .subtitle = found->description,
                .badge = complete ? "已完成" : "未完成",
                .image = chessSelectionIcon(ChessUiStyle::Icon::Mountain),
                .accent = complete ? ChessUiStyle::positive : ChessUiStyle::gold,
            };
            const auto metadata = chessChallengeMetadata(session_.content(), *found);
            auto analysis = std::make_shared<std::optional<ChessPreparedBattleAnalysis>>();
            const auto enemyAnalysis = [this, challenge = &*found, analysis]() -> const ChessPreparedBattleAnalysis& {
                if (!*analysis) *analysis = analyzeChessChallengeEnemies(session_.content(), *challenge);
                return **analysis;
            };
            ChessOverviewColumn enemies{.width = 540};
            enemies.rows.push_back({.text = std::format("{} 人", metadata.enemies.size()),
                .color = ChessUiStyle::context, .heading = true,
                .synergies = [this, enemyAnalysis] {
                    std::vector<ChessCharacterEntry> entries;
                    for (const auto& synergy : enemyAnalysis().enemySynergies)
                    {
                        const auto combo = std::ranges::find(session_.content().combos(), synergy.comboId, &ComboDef::id);
                        assert(combo != session_.content().combos().end());
                        entries.push_back({.id = combo->id, .title = combo->name, .value = chessComboCountWithoutStatus(synergy),
                            .imageGroup = "chess-synergies", .imageId = combo->iconId, .active = synergy.active,
                            .tooltip = combo->name});
                    }
                    return entries;
                }});
            for (int i = 0; i < static_cast<int>(metadata.enemies.size()); ++i)
            {
                const auto& enemy = metadata.enemies[i];
                std::string equipment;
                if (enemy.weapon) equipment += " · " + enemy.weapon->name;
                if (enemy.armor) equipment += " · " + enemy.armor->name;
                enemies.rows.push_back({.text = chessStars(enemy.star) + equipment,
                    .inspect = [this, enemyAnalysis, i] {
                        const auto& battle = enemyAnalysis();
                        return buildChessBattleCharacterPresentation(session_.content(), battle.units[i], battle.enemySynergies);
                    },
                    .prefix = std::pair{enemy.name + " ", chessPieceTierColor(session_.content().role(enemy.roleId)->Cost)}});
            }
            ChessOverviewColumn rewards{.width = 380};
            rewards.rows.push_back({complete ? "首勝獎勵 · 已領取"
                : metadata.rewards.size() > 1 ? "首勝獎勵 · 擇一" : "首勝獎勵",
                complete ? ChessUiStyle::muted : ChessUiStyle::gold, true});
            for (int i = 0; i < static_cast<int>(metadata.rewards.size()); ++i)
                rewards.rows.push_back({std::format("{}. {}", i + 1, metadata.rewards[i]),
                    complete ? ChessUiStyle::muted : ChessUiStyle::ink});
            card.overview = {std::move(enemies), std::move(rewards)};
            card.openCombo = [this, enemyAnalysis](int id) {
                const auto& battle = enemyAnalysis();
                if (battle.enemySynergies.empty())
                {
                    showChessMessage("敵方尚無羈絆啟動");
                    return;
                }
                std::vector<int> roles;
                for (const auto& unit : battle.units) roles.push_back(unit.roleId);
                showChessComboOverview(session_.content(), battle.enemySynergies, roles, id, "敵方羈絆");
            };
            if (!found->enemies.empty())
                card.image = {"head", session_.content().role(found->enemies.front().roleId)->HeadID};
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

void ChessGuiSessionAdapter::showBattlePreview() const
{
    if (!session_.state().preparedBattle) return;
    showChessBattleOverview(session_);
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
        const int battleFight = session_.state().fight;
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
        auto talents = teamTalents_;
        if (prepared.kind != PreparedChessBattleKind::Standalone) talents[0] = session_.state().talent;
        const auto context = prepared.layout == PreparedChessBattleLayout::PvpArena ? std::string("演武")
            : prepared.kind == PreparedChessBattleKind::Challenge ? std::format("挑戰 · {}", prepared.stableBattleId)
            : prepared.kind == PreparedChessBattleKind::Standalone ? std::string("獨立戰鬥")
            : std::format("第 {} 關", battleFight + 1);
        stats->setupPostBattle(
            battle->makePostBattleSummary(),
            battle->getBattleReport(),
            session_.content(), prepared, talents,
            std::format("{} · {} · {}", context, chessDifficultyDisplayName(session_.content().difficulty()),
                chessBattleMapDisplayName(session_.content(), prepared.chosenMapId)));
        stats->run();
        if (currentChessGuiFlowResult() == ChessGuiFlowResult::Aborted)
        {
            return ChessGuiFlowResult::Aborted;
        }
        const auto actionResult = *battle->completedActionResult();
        stats.reset();
        battle.reset();
        autoSavePostBattleGuiState(session_, ChessSessionPhase::BattlePreparation);
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
                    "少俠既已闖過第{}關，老夫便為你開啟神兵商店。此後可由「神兵」天賦說明或「裝備」前往「神兵商店」，花費${}購買傳說裝備。",
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
            const bool selected = chooseForcedBans();
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

bool ChessGuiSessionAdapter::openPreparationCommand(ChessContextMenuAction command)
{
    switch (command)
    {
    case ChessContextMenuAction::EnterBattle:
        if (legalAction(session_, ChessActionType::PrepareBattle))
        {
            const auto result = submitGuiAction({ChessActionType::PrepareBattle});
            assert(result.accepted);
            if (drainPreparedBattle() == ChessGuiFlowResult::Aborted) return true;
        }
        else showChessMessage("出戰棋子不足，請先選擇出戰陣容");
        break;
    case ChessContextMenuAction::ManageBans: showBanManagement(); break;
    case ChessContextMenuAction::RerollBattleSeed: showEnemyReroll(); break;
    case ChessContextMenuAction::BuyLegendaryEquipment:
    {
        ChessLegalActionDescriptor legendary{ChessActionType::BuyLegendaryEquipment};
        for (const auto& equipment : session_.content().equipment())
            if (equipment.tier == 4) legendary.candidateIds.push_back(equipment.itemId);
        chooseLegendary(legendary);
        break;
    }
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
