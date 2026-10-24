#include "ChessPreparationScreen.h"

#include "ChessGameQueries.h"
#include "ChessMenuFormatting.h"
#include "ChessPresentationHelpers.h"
#include "ChessTalentUi.h"
#include "GameUtil.h"
#include "TextureManager.h"
#include "ChessUiStyle.h"
#include "ChessCharacterUi.h"
#include "SystemSettings.h"

#include <cmath>
#include <array>
#include <format>
#include <limits>
#include <utility>

namespace KysChess
{
namespace
{
using namespace ChessUiStyle;
constexpr Color equipmentContext = context;

// Engine 的 UI 固定為 1280×720；畫布與觸控座標的縮放由共用呈現層處理。
void fill(PanelFrame frame, Color color, int radius = 0)
{
    Engine::getInstance()->fillRoundedRect(color, frame.x, frame.y, frame.w, frame.h, radius);
}

void outline(PanelFrame frame, Color color, int radius)
{
    Engine::getInstance()->drawRoundedRect(color, frame.x, frame.y, frame.w, frame.h, radius);
}

void drawText(const std::string& value, int size, int x, int y, Color color)
{
    Font::getInstance()->draw(value, size, x, y, color);
}

bool contains(const PanelFrame& frame, SDL_FPoint point)
{
    return point.x >= frame.x && point.x < frame.x + frame.w
        && point.y >= frame.y && point.y < frame.y + frame.h;
}

ChessAction simpleAction(ChessActionType type)
{
    ChessAction action;
    action.type = type;
    return action;
}

std::vector<PanelTextSourceRow> paragraph(const std::string& value)
{
    std::vector<PanelTextSourceRow> rows;
    for (const auto& part : wrapLines(value, 200))
        if (!part.empty()) rows.push_back({.text = part, .spacingAfter = 4});
    return rows;
}

PanelTextLayout layoutProse(const std::vector<PanelTextSourceRow>& rows, int width, int size)
{
    std::vector<PanelTextSourceRow> physicalRows;
    for (const auto& row : rows)
    {
        // 排版器以單行計高；先拆換行，讓捲動與分頁都保留完整效果文字。
        for (std::size_t start = 0; start < row.text.size();)
        {
            const auto end = row.text.find('\n', start);
            auto part = row;
            part.text = row.text.substr(start, end == std::string::npos ? end : end - start);
            if (start > 0) part.spacingBefore = 0;
            if (!part.text.empty()) physicalRows.push_back(std::move(part));
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    return layoutPanelText(physicalRows, width, size);
}
}

ChessPreparationScreen::ChessPreparationScreen(ChessGameSession& session, ChessPreparationCallbacks callbacks)
    : session_(session), callbacks_(std::move(callbacks))
{
    // 保留擂鼓山及天氣的繪製與 backRun；此節點獨佔輸入。
    full_window_ = 0;
}

void ChessPreparationScreen::onEntrance()
{
    onPointerInputReset();
    touchInput_ = GameUtil::isMobileDevice();
    confirmation_.reset();
    touchHint_.clear();
    singleClickMovement_ = SystemSettings::getInstance()->data().preparationSingleClick;
    guideStep_ = SystemSettings::getInstance()->data().preparationGuideSeen ? -1 : 0;
    rebuild();
}

void ChessPreparationScreen::panel(PanelFrame frame)
{
    drawings_.push_back([frame] { fill(frame, panelFill, Spacing::panelCorner); });
}

void ChessPreparationScreen::text(std::string value, int x, int y, int size, Color color, int width, int lines)
{
    drawings_.push_back([=] {
        if (width <= 0)
        {
            drawText(value, size, x, y, color);
            return;
        }
        ChessUiStyle::label(value, size, x, y, color, width, lines);
    });
}

void ChessPreparationScreen::image(std::string group, int id, PanelFrame frame)
{
    drawings_.push_back([=] { ChessUiStyle::asset(group, id, {frame.x, frame.y, frame.w, frame.h}, true); });
}

// 圖示沿用正式資源的 WebP 貼圖群組，由 TextureManager 管理生命週期。
void ChessPreparationScreen::drawIcon(Icon symbol, PanelFrame frame, Color color)
{
    ChessUiStyle::asset("chess-preparation", iconId(symbol),
        {frame.x, frame.y, frame.w, frame.h}, false, color);
}

void ChessPreparationScreen::icon(Icon symbol, PanelFrame frame, Color color)
{
    drawings_.push_back([=, this] { drawIcon(symbol, frame, color); });
}

void ChessPreparationScreen::synergyIcon(int iconId, PanelFrame frame, Color color)
{
    drawings_.push_back([=] {
        ChessUiStyle::asset("chess-synergies", iconId,
            {frame.x, frame.y, frame.w, frame.h}, false, color);
    });
}

int ChessPreparationScreen::iconButton(std::string key, PanelFrame frame, Icon symbol, std::string label,
    std::function<void()> action, std::string tooltip, bool enabled, bool selected, Color color, Treatment treatment)
{
    const int index = button(std::move(key), frame, {}, std::move(action), std::move(tooltip), enabled, selected, treatment);
    enabled = controls_[index].enabled;
    drawings_.push_back([frame, symbol, label = std::move(label), enabled, color] {
        // 擴大圖示使用按鈕內部空間，保留原有控制項邊界。
        const int iconSize = std::min({36, frame.h - 8,
            label.empty() ? frame.w : std::max(24, frame.w - 24 - Font::getTextDrawSize(label) * 8)});
        const int size = label.empty() ? 20 : std::min(20, (frame.w - iconSize - 24) * 2 / Font::getTextDrawSize(label));
        controlLabel({frame.x, frame.y, frame.w, frame.h}, label, size,
            enabled ? color : muted, symbol, enabled ? color : muted, iconSize);
    });
    return index;
}

int ChessPreparationScreen::button(std::string key, PanelFrame frame, std::string label,
    std::function<void()> action, std::string tooltip, bool enabled, bool selected, Treatment treatment, Color color,
    bool touchConfirmation, std::function<void()> preview)
{
    if (replacementIncomingId_ >= 0 && !key.starts_with("replace-") && key != "text-prev" && key != "text-next") enabled = false;
    const int index = static_cast<int>(controls_.size());
    controls_.push_back({std::move(key), frame, std::move(tooltip), std::move(action), enabled, touchConfirmation, std::move(preview), {}});
    drawings_.push_back([=, this] {
        const auto& control = controls_[index];
        const auto pointer = PointerInput::instance().logicalPointerUiPosition();
        const bool hovered = !touchInput_ && contains(frame, pointer);
        const bool focused = keyboardFocus_ && focusKey_ == control.key;
        const bool armed = confirmation_.armedKey == control.key;
        const bool confirming = armed && !control.confirmationLabel.empty();
        const auto& caption = confirming ? control.confirmationLabel : label;
        const bool highlighted = enabled && (hovered || armed || pressedKey_ == control.key);
        if (treatment != Treatment::Quiet || highlighted || selected || focused)
            controlSurface({frame.x, frame.y, frame.w, frame.h}, {
                .hovered = highlighted, .selected = selected, .focused = (focused || armed) && enabled,
                .disabled = !enabled, .primary = treatment == Treatment::Primary,
                .action = treatment == Treatment::Action, .destructive = confirming});
        int size = 20;
        while (size > 16 && Font::getTextDrawSize(caption) * size / 2 > frame.w - 16) --size;
        const auto labelColor = !enabled ? muted : confirming ? ink : treatment == Treatment::Primary ? darkLabel : color;
        ChessUiStyle::label(caption, size,
            frame.x + std::max(6, (frame.w - Font::getTextDrawSize(caption) * size / 2) / 2),
            frame.y + (frame.h - size) / 2, labelColor, frame.w - 16);
    });
    return index;
}

void ChessPreparationScreen::rebuild()
{
    ++layoutRevision_;
    equipmentPopover_.dismiss();
    popoverFrame_ = {};
    pressedKey_.clear();
    pressPreview_.reset();
    selection_.reconcile(session_.state());
    observation_ = session_.observe();
    w_ = Engine::getInstance()->getUIWidth();
    h_ = Engine::getInstance()->getUIHeight();
    controls_.clear();
    drawings_.clear();
    pieceCards_.clear();
    rosterOrder_.reconcile(session_.state());
    const int rightWidth = std::max(300, w_ * 28 / 100);
    constexpr int gutter = 8;
    constexpr int mainTop = 80;
    const int leftWidth = w_ - rightWidth - 32 - gutter;
    // 手機角落保留給 44 px/dp 的平台按鈕；沿用原有操作列，不另加工具列。
    const int footerHeight = (GameUtil::isMobileDevice() ? 82 : 52) + (observation_.deploymentOverflow > 0 ? 24 : 0);
    footer_ = {16, h_ - footerHeight - 14, w_ - 32, footerHeight};
    const int mainHeight = footer_.y - gutter - mainTop;
    const int benchHeight = 120;
    const int shopHeight = 182;
    team_ = {16, mainTop, leftWidth, mainHeight - benchHeight - shopHeight - 2 * gutter};
    bench_ = {16, team_.y + team_.h + gutter, leftWidth, benchHeight};
    shop_ = {16, bench_.y + bench_.h + gutter, leftWidth, shopHeight};
    inspector_ = {16 + leftWidth + gutter, mainTop, rightWidth, mainHeight};
    header();
    roster();
    shop();
    inspector();
    footer();
    guideDrawingStart_ = drawings_.size();
    if (guideStep_ >= 0) guide();
    for (auto& control : controls_) if (!control.enabled) { control.shortcut = {}; control.preview = {}; }
    if (focusIndex() < 0 || !controls_[focusIndex()].enabled)
    {
        const auto first = std::ranges::find(controls_, true, &Control::enabled);
        if (first != controls_.end()) focusKey_ = first->key;
    }
}

void ChessPreparationScreen::header()
{
    panel({16, 12, w_ - 32, 60});
    text("整備", 32, 28, 26, gold);
    text(observation_.campaignComplete ? "通關"
        : std::format("{} / {} 關", observation_.fight + 1, session_.content().balance().totalFights),
        102, 33, 18, observation_.campaignComplete ? positive : ink);
    iconButton("economy", {220, 20, 100, 44}, Icon::Coin, std::to_string(observation_.money),
        [this] { navigate(View::Information, -1, "economy"); }, "查看金幣、利息與勝利收入", true, false, gold);
    icon(Icon::Right, {308, 36, 12, 12}, muted);
    button("level", {328, 20, 246, 44}, {},
        [this] { navigate(View::Information, -1, "level"); }, "查看經驗、修練與商店機率");
    icon(Icon::Cultivation, {336, 29, 26, 26});
    text(std::format("等級 {}", observation_.level + 1), 372, 31, 20, context);
    text(observation_.level < session_.content().balance().maxLevel
        ? std::format("經驗 {} / {}", observation_.experience, observation_.experienceForNextLevel)
        : "已滿級", 458, 34, 16, positive);
    icon(Icon::Right, {560, 36, 12, 12}, muted);
    const int x = w_ - 698;
    iconButton("difficulty", {x, 20, 104, 44}, Icon::Mountain, chessDifficultyDisplayName(observation_.difficulty),
        [this] { navigate(View::Information, -1, "difficulty"); }, "查看本局難度", true, false, observation_.difficulty == Difficulty::Hard ? negative : observation_.difficulty == Difficulty::Easy ? positive : gold);
    iconButton("talent", {x + 112, 20, 116, 44}, Icon::Spark, observation_.talentName,
        [this] { navigate(View::Information, -1, "talent"); }, "查看天賦說明與效果");
    iconButton("neigongs", {x + 236, 20, 72, 44}, Icon::Book, std::to_string(observation_.obtainedNeigongIds.size()),
        [this] { navigate(View::Neigongs); }, "已獲得內功與效果");
    const int remainingBans = std::max(0, observation_.maximumBanCount - static_cast<int>(observation_.bans.size()));
    iconButton("bans", {x + 316, 20, 142, 44}, Icon::Ban, std::format("禁({})", remainingBans),
        [this] { open(ChessContextMenuAction::ManageBans); }, "管理已見角色的永久封禁；括號內為剩餘名額");
    iconButton("collapse", {x + 466, 20, 100, 44}, Icon::Collapse, "",
        [this] { setExit(true); }, "收起整備，欣賞擂鼓山；Q 再次開啟");
    iconButton("system", {x + 574, 20, 92, 44}, Icon::Sliders, "",
        [this] { open(ChessContextMenuAction::OpenSystemMenu); }, "系統選單");
}

void ChessPreparationScreen::pieceCard(const ChessSessionPiece& piece, PanelFrame frame, bool compact)
{
    pieceCards_.push_back({piece, frame, compact});
    const auto* role = session_.content().role(piece.roleId);
    assert(role);
    controls_.push_back({.key = std::format("piece:{}", piece.instanceId), .frame = frame,
        .tooltip = std::format("{} {} · {}階；{}{}", role->Name, chessStars(piece.star), role->Cost,
            singleClickMovement_ ? "單擊" : "雙擊", piece.deployed ? "下陣" : "上陣"),
        .activate = [this, id = piece.instanceId] { selectPiece(id); }});
    if (replacementIncomingId_ >= 0)
    {
        controls_.back().tooltip = piece.deployed ? "選擇要換下的棋子，再於檢視器確認" : "正在選擇出戰替換對象";
        controls_.back().enabled = piece.deployed;
        return;
    }
    controls_.back().shortcut = [this, id = piece.instanceId] {
        selection_.selectPiece(id);
        characterInspector_ = {}; deploy();
    };
    for (const auto& [equipment, itemFrame] : pieceEquipmentFrames(pieceCards_.back(), frame))
    {
        const auto& item = session_.state().equipmentInventory.at(equipment);
        controls_.push_back({.key = std::format("piece-gear:{}:{}", piece.instanceId, equipment), .frame = itemFrame,
            .tooltip = session_.content().item(item.itemId)->name,
            .activate = [this, id = piece.instanceId, equipment] {
                navigate(View::Equipment); selection_.selectPiece(id);
                equipmentId_ = equipment;
                equipmentType_ = equipmentDefinition(session_.content(), session_.state().equipmentInventory.at(equipment).itemId)->equipType;
                rebuild();
                equipmentPopover_.pin(equipment);
            }, .equipmentId = equipment});
    }
}

std::vector<std::pair<int, PanelFrame>> ChessPreparationScreen::pieceEquipmentFrames(const PieceCard& card, PanelFrame frame) const
{
    std::vector<std::pair<int, PanelFrame>> result;
    const int portraitSize = std::min(card.compact ? 52 : 64, frame.h - 8);
    const int portraitY = frame.y + (frame.h - portraitSize) / 2;
    int itemX = frame.x + frame.w - 28;
    for (int equipment : {card.piece.armorInstanceId, card.piece.weaponInstanceId})
    {
        if (equipment < 0) continue;
        const bool namedBench = card.compact && benchNames_;
        const int itemSize = namedBench ? 20 : 26;
        result.push_back({equipment, {itemX,
            namedBench ? frame.y + 18 : card.compact ? frame.y + frame.h - 28 : portraitY + portraitSize - 28,
            itemSize, itemSize}});
        itemX -= itemSize + 2;
    }
    return result;
}

void ChessPreparationScreen::drawPieceCard(const PieceCard& card, PanelFrame frame)
{
    const auto& piece = card.piece;
    const bool compact = card.compact;
    const auto* role = session_.content().role(piece.roleId);
    assert(role);
    const Color tier = chessPieceTierColor(role->Cost);
    const auto key = std::format("piece:{}", piece.instanceId);
    const bool moving = cardMotions_.contains(piece.instanceId) && cardMotions_.at(piece.instanceId).active(Engine::getTicks());
    const bool hovered = !moving && !touchInput_ && contains(frame, PointerInput::instance().logicalPointerUiPosition());
    controlSurface({frame.x, frame.y, frame.w, frame.h}, {
        .hovered = hovered || pressedKey_ == key, .selected = selection_.instanceId == piece.instanceId,
        .focused = keyboardFocus_ && focusKey_ == key});
    fill({frame.x + 1, frame.y + 3, 3, frame.h - 6}, tier);
    if (Engine::getTicks() < arrivalUntil_ && std::ranges::contains(arrivalIds_, piece.instanceId))
    {
        const uint8_t alpha = static_cast<uint8_t>(40 + 36 * (1 + std::sin(Engine::getTicks() / 110)));
        Color arrival = arrivalMerged_ ? Color{255, 215, 0, alpha} : Color{100, 200, 255, alpha};
        fill({frame.x + 5, frame.y + 2, frame.w - 7, frame.h - 4}, arrival, 4);
    }
    const int portraitSize = std::min(compact ? 52 : 64, frame.h - 8);
    const int portraitY = frame.y + (frame.h - portraitSize) / 2;
    asset("head", role->HeadID, {frame.x + 6, portraitY, portraitSize, portraitSize}, true);
    if (!compact)
    {
        label(role->Name, 18, frame.x + portraitSize + 10, portraitY + 6, ink, frame.w - portraitSize - 16);
        // 星級留在肖像下緣，裝備固定在右下角，三星與雙裝備也不共用空間。
        const PanelFrame stars{frame.x + 6, portraitY + portraitSize - 16, portraitSize, 18};
        fill(stars, {25, 23, 22, 190}, 3);
        drawText(chessStars(piece.star), 14, stars.x + (stars.w - piece.star * 14) / 2, stars.y + 1, gold);
    }
    else
    {
        drawText(chessStars(piece.star), 14, frame.x + 5, frame.y + 2, gold);
        if (benchNames_)
        {
            const PanelFrame caption{frame.x + 4, frame.y + frame.h - 20, frame.w - 8, 20};
            fill(caption, panelFill, 3);
            label(role->Name, 14, caption.x + 2, caption.y + 2, ink, caption.w - 4);
        }
    }
    for (const auto& [equipment, itemFrame] : pieceEquipmentFrames(card, frame))
    {
        const auto& item = session_.state().equipmentInventory.at(equipment);
        const bool hover = !moving && !touchInput_ && contains(itemFrame, PointerInput::instance().logicalPointerUiPosition());
        if (hover) controlSurface({itemFrame.x, itemFrame.y, itemFrame.w, itemFrame.h}, {.hovered = true});
        asset("item", session_.content().item(item.itemId)->id,
            {itemFrame.x + 1, itemFrame.y + 1, itemFrame.w - 2, itemFrame.h - 2}, true);
    }
    const auto feedback = std::ranges::find(luckFeedback_, piece.instanceId, &LuckFeedback::instanceId);
    if (feedback == luckFeedback_.end() || Engine::getTicks() >= feedback->until) return;
    const double remaining = feedback->until - Engine::getTicks();
    const double progress = 1.0 - remaining / 1800.0;
    const double opacity = std::min(1.0, remaining / 500.0);
    const Color glow{gold.r, gold.g, gold.b, static_cast<uint8_t>((60 + 30 * std::sin(progress * 18)) * opacity)};
    fill({frame.x + 2, frame.y + 2, frame.w - 4, frame.h - 4}, glow, 4);
    const int badgeX = frame.x + (frame.w - 56) / 2;
    const int badgeY = frame.y + std::max(0, (frame.h - 30) / 2 - static_cast<int>(progress * 16));
    fill({badgeX, badgeY, 56, 28}, {29, 26, 23, static_cast<uint8_t>(230 * opacity)}, 4);
    const Color color{gold.r, gold.g, gold.b, static_cast<uint8_t>(255 * opacity)};
    drawIcon(Icon::Luck, {badgeX + 3, badgeY + 2, 24, 24}, color);
    drawText(std::format("+{}", feedback->stacks), 16, badgeX + 29, badgeY + 6, color);
}

std::map<int, PanelFrame> ChessPreparationScreen::cardFrames() const
{
    std::map<int, PanelFrame> frames;
    const auto now = Engine::getTicks();
    for (const auto& card : pieceCards_)
    {
        const auto motion = cardMotions_.find(card.piece.instanceId);
        frames.emplace(card.piece.instanceId, motion == cardMotions_.end() ? card.frame : motion->second.frame(now));
    }
    return frames;
}

void ChessPreparationScreen::animateCardsFrom(const std::map<int, PanelFrame>& previous)
{
    cardMotions_.clear();
    const auto now = Engine::getTicks();
    for (const auto& card : pieceCards_)
    {
        const auto found = previous.find(card.piece.instanceId);
        if (found == previous.end()) continue;
        const auto& from = found->second;
        const auto& to = card.frame;
        if (from.x != to.x || from.y != to.y || from.w != to.w || from.h != to.h)
            cardMotions_.emplace(card.piece.instanceId, ChessPreparationCardMotion{from, to, now});
    }
}

void ChessPreparationScreen::roster()
{
    panel(team_);
    panel(bench_);
    const int deployed = static_cast<int>(std::ranges::count(observation_.roster, true, &ChessSessionPiece::deployed));
    text(std::format("出戰 {}/{}{}", deployed, observation_.maximumDeployment,
        observation_.deploymentOverflow > 0 ? std::format(" · 超出{}", observation_.deploymentOverflow) : ""),
        team_.x + 12, team_.y + 14, observation_.deploymentOverflow > 0 ? 18 : 24,
        observation_.deploymentOverflow > 0 ? negative : gold);
    button("deployment-info", {team_.x + 8, team_.y + 4, 160, 36}, "",
        [this] { navigate(View::Information, -1, "level"); },
        std::format("等級名額 {} + 羈絆加成 {} = {}", observation_.baseDeployment, observation_.deploymentBonus, observation_.maximumDeployment));
    auto combos = chessPreparationCombos(session_.state(), session_.content());
    std::erase_if(combos, [](const auto& combo) { return !combo.progress.active; });
    constexpr int columns = 8;
    constexpr int capacity = columns * 2;
    const int chipWidth = (team_.w - 174 - 12 - (columns - 1) * 4) / columns;
    const int visible = std::min(capacity, static_cast<int>(combos.size()));
    for (int i = 0; i < visible; ++i)
    {
        const PanelFrame chip{team_.x + 174 + (i % columns) * (chipWidth + 4), team_.y + 4 + (i / columns) * 40, chipWidth, 40};
        if (i == capacity - 1 && static_cast<int>(combos.size()) > capacity)
        {
            iconButton("combo-overflow", chip, Icon::More, std::format("+{}", combos.size() - i),
                [this] { showChessComboOverview(session_.content(), session_.state()); rebuild(); }, "查看其餘已啟動羈絆");
            break;
        }
        const auto& [definition, progress] = combos[i];
        const auto& combo = *definition;
        const auto metadata = chessComboMetadata(session_.content(), combo, progress.physicalCount, progress.effectiveCount,
            progress.activeThresholdIndex, progress.nextThresholdIndex, progress.contributions,
            EffectDescriptionStyle::Compact, progress.requiredCounts);
        const auto& threshold = metadata.thresholds[metadata.activeThresholdIndex];
        button(std::format("combo-chip:{}", combo.id), chip, {},
            [this, id = combo.id] { showChessComboOverview(session_.content(), session_.state(), id); rebuild(); }, {},
            true, false, Treatment::Quiet);
        controls_.back().tooltipBelow = true;
        auto& tooltipRows = controls_.back().tooltipRows;
        tooltipRows.push_back({.text = combo.name + " · " + threshold.name,
            .spacingAfter = 6, .tone = PanelTextTone::Heading});
        const auto effects = panelTextRowsForEffectDescription(threshold.effects);
        tooltipRows.insert(tooltipRows.end(), effects.begin(), effects.end());
        synergyIcon(combo.iconId, {chip.x, chip.y + 2, 36, 36}, gold);
        const auto count = progress.isAntiCombo ? std::format("獨/{}", progress.displayTargetCount)
            : std::format("{}/{}", progress.effectiveCount, progress.displayTargetCount);
        const int countSize = std::min(22, (chip.w - 38) * 2 / Font::getTextDrawSize(count));
        text(count, chip.x + 38, chip.y + (chip.h - countSize) / 2, countSize, ink);
    }
    iconButton("all-combos", {team_.x + 8, team_.y + 44, 150, 40}, Icon::Link,
        observation_.deploymentBonus > 0 ? observation_.deploymentBonusSources.front() + " +1 ›" : std::format("啟動 {} ›", combos.size()),
        [this] { showChessComboOverview(session_.content(), session_.state()); rebuild(); }, "查看全部羈絆與門檻，包含尚未啟動的羈絆");
    if (combos.empty()) text("尚無羈絆啟動", team_.x + 186, team_.y + 18, 18, muted);
    const int gap = 8;
    const int cardColumns = std::max(deployed, observation_.maximumDeployment) == 11 ? 6 : 5;
    const int width = (team_.w - 24 - gap * (cardColumns - 1)) / cardColumns;
    const int top = team_.y + 88;
    const int slots = std::max(deployed, observation_.maximumDeployment);
    auto visibleFormation = rosterOrder_.deployed;
    visibleFormation.resize(slots, -1);
    const bool twoRows = slots > cardColumns;
    const int height = (team_.h - 100 - (twoRows ? gap : 0)) / (twoRows ? 2 : 1);
    for (int slot = 0; slot < slots; ++slot)
    {
        const PanelFrame frame{team_.x + 12 + (slot % cardColumns) * (width + gap), top + (slot / cardColumns) * (height + gap), width, height};
        const int id = visibleFormation[slot];
        if (id >= 0) pieceCard(session_.state().roster.at(id), frame, false);
        else drawings_.push_back([frame] { fill(frame, {12, 11, 10, 32}, 4); });
    }
    if (slots < kChessFormationSlotCount)
    {
        PanelFrame frame{team_.x + 12 + (slots % cardColumns) * (width + gap), top + (slots / cardColumns) * (height + gap), width, height};
        // 五人時下一格放在標題內，避免製造空的第二排。
        if (slots == 5 || slots == 10) frame = {team_.x + 126, team_.y + 4, 40, 40};
        const bool canLevel = observation_.level < session_.content().balance().maxLevel;
        iconButton("next-slot", frame, Icon::Lock, "",
            [this, canLevel] {
                if (canLevel) navigate(View::Information, -1, "level");
                else { showChessComboOverview(session_.content(), session_.state()); rebuild(); }
            }, canLevel ? "查看升級與出戰名額" : "啟動智將五人門檻可增加一名出戰名額");
    }
    const auto& bench = rosterOrder_.bench;
    const int pages = std::max(1, (static_cast<int>(bench.size()) + 9) / 10);
    benchPage_ = std::clamp(benchPage_, 0, pages - 1);
    text(std::format("候補  {}/{}", bench.size(), session_.content().balance().benchSize), bench_.x + 12, bench_.y + 14, 24, gold);
    iconButton("single-click", {bench_.x + 190, bench_.y + 4, 46, 40}, Icon::Transfer,
        "", [this] {
            singleClickMovement_ = !singleClickMovement_;
            portraitClicks_.reset();
            auto settings = SystemSettings::getInstance()->snapshot();
            settings.preparationSingleClick = singleClickMovement_;
            SystemSettings::getInstance()->update(settings);
            rebuild();
        }, "開啟後單擊上陣或下陣；目的地滿員時無法移動。關閉則單擊查看、雙擊移動", true, singleClickMovement_);
    iconButton("guide", {bench_.x + 244, bench_.y + 4, 46, 40}, Icon::Help, "", [this] {
        onPointerInputReset(); guideStep_ = 0; rebuild();
    }, "重看購買、調度與常用按鈕的操作導覽");
    iconButton("bench-names", {bench_.x + bench_.w - 58, bench_.y + 4, 46, 40}, Icon::Eye, "",
        [this] { benchNames_ = !benchNames_; rebuild(); }, benchNames_ ? "隱藏候補姓名" : "顯示候補姓名", true, benchNames_);
    if (pages > 1)
    {
        iconButton("bench-prev", {bench_.x + bench_.w - 170, bench_.y + 4, 46, 40}, Icon::Left, "",
            [this] { --benchPage_; rebuild(); }, "上一頁候補", benchPage_ > 0);
        iconButton("bench-next", {bench_.x + bench_.w - 114, bench_.y + 4, 46, 40}, Icon::Right, "",
            [this] { ++benchPage_; rebuild(); }, "下一頁候補", benchPage_ + 1 < pages);
    }
    const int benchWidth = (bench_.w - 24 - gap * 9) / 10;
    for (int slot = 0; slot < 10; ++slot)
    {
        PanelFrame frame{bench_.x + 12 + slot * (benchWidth + gap), bench_.y + 48, benchWidth, bench_.h - 60};
        const int index = benchPage_ * 10 + slot;
        if (index < static_cast<int>(bench.size())) pieceCard(session_.state().roster.at(bench[index]), frame, true);
        else drawings_.push_back([frame] { fill(frame, {12, 11, 10, 24}, 4); });
    }
}

void ChessPreparationScreen::shop()
{
    panel(shop_);
    text("商店", shop_.x + 12, shop_.y + 16, 24, gold);
    iconButton("refresh", {shop_.x + shop_.w - 228, shop_.y + 6, 140, 44}, Icon::Refresh,
        observation_.freeShopRefreshAvailable ? "免費" : std::format("${}", session_.content().balance().refreshCost),
        [this] { const auto result = submit(simpleAction(ChessActionType::RefreshShop), "商店已刷新");
            if (result.accepted && selection_.shopSlot >= 0) { selection_ = {}; rebuild(); } }, "刷新商店");
    controls_.back().touchConfirmation = true;
    if (observation_.freeShopRefreshAvailable)
    {
        const auto frame = controls_.back().frame;
        drawings_.push_back([frame] {
            attentionOutline({frame.x, frame.y, frame.w, frame.h},
                chessShopRowOutlineStyle(ChessShopRowEmphasis::Merge).color);
        });
    }
    iconButton("lock", {shop_.x + shop_.w - 76, shop_.y + 6, 64, 44}, Icon::Lock, "",
        [this] { auto action = simpleAction(ChessActionType::SetShopLocked); action.value = !session_.state().shopLocked;
            submit(action, action.value ? "商店已鎖定" : "商店已解鎖"); }, "鎖定商店", true, observation_.shopLocked);
    const int pages = std::max(1, (static_cast<int>(observation_.shop.size()) + 5) / 6);
    shopPage_ = std::clamp(shopPage_, 0, pages - 1);
    if (pages > 1)
        button("shop-page", {shop_.x + 244, shop_.y + 6, 112, 44}, std::format("商店 {}/{} >", shopPage_ + 1, pages),
            [this, pages] { shopPage_ = (shopPage_ + 1) % pages; rebuild(); });
    const int columns = std::max(1, std::min(6, static_cast<int>(observation_.shop.size())));
    const int width = (shop_.w - 24 - (columns - 1) * 8) / columns;
    for (int column = 0; column < columns; ++column)
    {
        const int slot = shopPage_ * 6 + column;
        if (slot >= static_cast<int>(observation_.shop.size())) break;
        const auto& offer = observation_.shop[slot];
        PanelFrame frame{shop_.x + 12 + column * (width + 8), shop_.y + 52, width, shop_.h - 62};
        if (offer.roleId < 0)
        {
            button(std::format("sold:{}", slot), frame, "已招募", {}, {}, false);
            continue;
        }
        const auto* role = session_.content().role(offer.roleId);
        assert(role);
        const auto analysis = queryChessShopSlot(session_.state(), session_.content(), slot);
        const auto emphasis = chessShopRowEmphasis(session_.state(), role->ID);
        const auto emphasisStyle = chessShopRowOutlineStyle(emphasis);
        const bool banned = session_.state().bannedRoleIds.contains(offer.roleId);
        const Color tier = chessPieceTierColor(role->Cost);
        button(std::format("offer:{}", slot), {frame.x, frame.y, frame.w, frame.h - 40}, {},
            [this, slot] { selectShop(slot); },
            std::format("{} · {}階；雙擊購買", role->Name, offer.tier),
            true, selection_.shopSlot == slot, Treatment::Surface);
        controls_.back().key = std::format("offer:{}:{}", slot, offer.roleId);
        controls_.back().preview = [this, slot] { if (selection_.shopSlot != slot) selectShop(slot); };
        controls_.back().shortcut = [this, slot] { buy(slot); };
        image("head", role->HeadID, {frame.x + 12, frame.y + 2, frame.w - 24, frame.h - 66});
        const int nameWidth = std::min(frame.w - 16, Font::getTextDrawSize(role->Name) * 9);
        text(role->Name, frame.x + (frame.w - nameWidth) / 2, frame.y + frame.h - 62, 18, tier, nameWidth);
        if (emphasis == ChessShopRowEmphasis::Merge)
            icon(Icon::Up, {frame.x + frame.w - 24, frame.y + 5, 18, 18}, gold);
        const std::string reason = analysis.purchaseError == ChessRuleErrorCode::InsufficientGold ? "金錢不足"
            : analysis.purchaseError == ChessRuleErrorCode::BenchFull ? "候補已滿" : "購買";
        const bool affordable = analysis.purchaseError == ChessRuleErrorCode::None;
        button(std::format("buy:{}", slot), {frame.x, frame.y + frame.h - 40, frame.w, 40},
            {}, [this, slot] { buy(slot); },
            reason == "購買" ? "購買此棋子" : reason,
            affordable, false, Treatment::Quiet, gold);
        const auto price = std::to_string(analysis.goldCost);
        const int priceX = frame.x + (frame.w - 30 - Font::getTextDrawSize(price) * 10) / 2;
        icon(Icon::Coin, {priceX, frame.y + frame.h - 32, 24, 24}, affordable ? gold : muted);
        text(price, priceX + 30, frame.y + frame.h - 30, 20, affordable ? gold : muted);
        drawings_.push_back([=] {
            // 沿用舊選單的短光點與兩秒週期；不再疊加常亮外框。
            // dotLength 是周長比例，使用共用預設值，不能傳入像素長度。
            if (emphasisStyle.animated)
                attentionOutline({frame.x, frame.y, frame.w, frame.h}, emphasisStyle.color, emphasisStyle.thickness);
            fill({frame.x + 1, frame.y + 3, 3, frame.h - 6}, tier);
            if (banned) bannedOutline({frame.x, frame.y, frame.w, frame.h});
        });
    }
}

void ChessPreparationScreen::inspector()
{
    panel(inspector_);
    if (view_ != View::Piece)
    {
        iconButton("back", {inspector_.x + inspector_.w - 98, inspector_.y + 8, 86, 44}, Icon::Left, "返回", [this] { back(); });
    }
    switch (view_)
    {
    case View::Piece: pieceInspector(); break;
    case View::Equipment: equipmentInspector(); break;
    case View::Information: informationInspector(); break;
    case View::Neigongs:
    case View::Neigong: neigongInspector(); break;
    }
}

void ChessPreparationScreen::pieceInspector()
{
    if (replacementIncomingId_ >= 0)
    {
        const int x = inspector_.x + 12, y = inspector_.y + 16, width = inspector_.w - 24;
        const int target = replacementTargetId_ >= 0 ? replacementTargetId_ : replacementPreviewId_;
        const auto& state = session_.state();
        text("替換出戰", x, y, 24, gold);
        auto rows = paragraph("點選要換下的出戰棋子；確認前不改動陣容。Esc 可取消。\n換入棋子繼承換出棋子的戰場位置。");
        if (target >= 0)
        {
            auto projected = state;
            projected.roster.at(replacementIncomingId_).deployed = true;
            projected.roster.at(target).deployed = false;
            rows.push_back({.text = std::format("{}上陣 ↔ {}下陣",
                session_.content().role(state.roster.at(replacementIncomingId_).roleId)->Name,
                session_.content().role(state.roster.at(target).roleId)->Name), .spacingAfter = 12});
            const int after = ChessManagementRules::maximumDeployment(projected, session_.content());
            rows.push_back({.text = std::format("出戰上限 {} → {}", observation_.maximumDeployment, after), .spacingAfter = 8});
            for (const auto& combo : session_.content().combos())
            {
                const auto before = evaluateChessComboProgress(state, session_.content(), combo);
                const auto changed = evaluateChessComboProgress(projected, session_.content(), combo);
                if (before.effectiveCount != changed.effectiveCount)
                    rows.push_back({.text = std::format("{} {} → {}{}", combo.name, before.effectiveCount,
                        changed.effectiveCount, state.selectedTalentComboId == combo.id ? "（含合縱）" : ""), .spacingAfter = 4});
            }
            if (observation_.deployedCount > after)
                rows.push_back({.text = std::format("替換後超出 {} 名，需調整後才能開戰", observation_.deployedCount - after), .spacingAfter = 8});
        }
        prose(rows, {x, y + 48, width, inspector_.h - 128});
        const int bottom = inspector_.y + inspector_.h - 60;
        button("replace-confirm", {x, bottom, width / 2 - 6, 44}, "確認替換", [this] {
            const auto action = chessPreparationReplacement(session_.state(), replacementIncomingId_, replacementTargetId_);
            replacementIncomingId_ = replacementTargetId_ = replacementPreviewId_ = -1;
            submit(action, "已替換出戰，戰場位置保留");
        }, {}, replacementTargetId_ >= 0, false, Treatment::Action);
        button("replace-cancel", {x + width / 2 + 6, bottom, width / 2 - 6, 44}, "取消", [this] { back(); });
        return;
    }
    const auto* piece = rosterPiece(session_.state(), selection_.instanceId);
    const int roleId = piece ? piece->roleId : selection_.shopRoleId;
    const auto* role = roleId >= 0 ? session_.content().role(roleId) : nullptr;
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    if (!role)
    {
        text("檢視器", x, y + 4, 24, gold);
        prose(paragraph("選擇出戰、候補或商店的棋子，查看屬性、武學與羈絆。"), {x, y + 64, width, 160});
        iconButton("empty-equipment", {x, y + 250, width, 52}, Icon::Bag, "裝備行囊", [this] { navigate(View::Equipment); });
        iconButton("empty-combos", {x, y + 314, width, 52}, Icon::Link, "羈絆一覽", [this] { showChessComboOverview(session_.content(), session_.state()); rebuild(); });
        return;
    }
    const auto character = buildChessCharacterPresentation(
        session_.content(), session_.state(), roleId, piece ? piece->star : 1,
        piece ? piece->instanceId : -1, piece ? ChessCharacterContext::Owned : ChessCharacterContext::Shop);
    const bool showTraining = !characterInspector_.expanded && piece && session_.state().talent == ChessTalentId::LateBloomer
        && (!session_.state().lateBloomerSelectionUsed || piece->lateBloomerChosen);
    const int detailWidth = (width - 12) / 2;
    const std::optional<SDL_Rect> navigationFrame = showTraining
        ? std::optional(SDL_Rect{x, inspector_.y + inspector_.h - 120, detailWidth, 44}) : std::nullopt;
    auto content = composeChessCharacterInspector(character,
        {inspector_.x, inspector_.y, inspector_.w, inspector_.h - 64}, characterInspector_, navigationFrame);
    for (int i = 0; i < static_cast<int>(content.controls.size()); ++i)
    {
        const auto control = content.controls[i];
        const PanelFrame area{control.frame.x, control.frame.y, control.frame.w, control.frame.h};
        const auto activate = [this, control] {
            if (control.equipmentSlot)
            {
                pickEquipment(*control.equipmentSlot);
                return;
            }
            if (control.comboId)
            {
                showChessComboOverview(session_.content(), session_.state(), *control.comboId);
                rebuild();
                return;
            }
            applyChessInspectorControl(characterInspector_, control);
            rebuild();
        };
        const auto treatment = control.action ? Treatment::Action : Treatment::Quiet;
        if (control.icon) iconButton(std::format("character-control:{}", i), area, *control.icon,
            control.label, activate, control.tooltip, true, control.selected, ink, treatment);
        else button(std::format("character-control:{}", i), area, control.label, activate, control.tooltip, true, control.selected, treatment);
    }
    for (auto& draw : content.drawings) drawings_.push_back(std::move(draw));
    const int bottom = inspector_.y + inspector_.h - 64;
    if (piece)
    {
        if (showTraining)
        {
            const int wins = session_.content().balance().talent(ChessTalentId::LateBloomer).evolutionWins;
            const auto label = !piece->lateBloomerChosen ? std::string("晚成培養")
                : piece->star == 4 ? std::string("已突破四星")
                : std::format("培養 {}/{}", std::min(piece->fightsWon, wins), wins);
            button("late-bloomer", {x + detailWidth + 12, bottom - 56, width - detailWidth - 12, 44}, label,
                [this, id = piece->instanceId] { chooseLateBloomer(id); },
                chessLateBloomerSelectionDescription(session_.content(), *piece),
                !piece->lateBloomerChosen, false, Treatment::Action);
        }
        const int price = ChessManagementRules::pieceValue(session_.content(), piece->roleId, piece->star);
        const int actionWidth = piece->deployed ? width - 136 : (width - 16) / 3;
        button("deployment", {x, bottom, actionWidth, 52}, piece->deployed ? "下陣" : "上陣", [this] { deploy(); }, {}, true, false, Treatment::Action);
        if (!piece->deployed)
            button("replace", {x + actionWidth + 8, bottom, actionWidth, 52}, "替換出戰", [this, id = piece->instanceId] {
                replacementIncomingId_ = id;
                replacementTargetId_ = replacementPreviewId_ = -1;
                portraitClicks_.reset(); rebuild();
            }, "候補滿員也可一對一替換", observation_.deployedCount > 0, false, Treatment::Action);
        const int sellX = piece->deployed ? x + width - 124 : x + 2 * (actionWidth + 8);
        button(std::format("sell:{}", piece->instanceId), {sellX, bottom, x + width - sellX, 52}, std::format("出售 {}金", price),
            [this, id = piece->instanceId] {
                auto action = simpleAction(ChessActionType::SellChess);
                action.chessInstanceId = id;
                submit(action, "棋子已出售，裝備已回到行囊");
            }, "再點一次出售，裝備回到行囊", true, false, Treatment::Action);
        controls_.back().confirmationLabel = std::format("確認 {}金", price);
    }
    else
    {
        const auto analysis = queryChessShopSlot(session_.state(), session_.content(), selection_.shopSlot);
        button("inspector-buy", {x, bottom, width - 100, 52}, std::format("購買  {} 金", analysis.goldCost),
            [this, slot = selection_.shopSlot] { buy(slot); }, "購買此棋子", analysis.purchaseError == ChessRuleErrorCode::None, false, Treatment::Action, ink);
        auto action = simpleAction(ChessActionType::AddBan);
        action.roleId = roleId;
        const auto error = ChessManagementRules::validate(session_.state(), session_.content(), action);
        button(std::format("inspector-ban:{}", roleId), {x + width - 92, bottom, 92, 52}, "封禁",
            [this, roleId] { ban(roleId); },
            error == ChessRuleErrorCode::RoleAlreadyBanned ? "本局已封禁此角色"
                : error == ChessRuleErrorCode::BanLimitReached ? "封禁名額已滿；升級可增加名額"
                : "封禁後本局無法解除；保留持有棋子與當輪商品",
            error == ChessRuleErrorCode::None, false, Treatment::Action);
    }
}

void ChessPreparationScreen::prose(const std::vector<PanelTextSourceRow>& rows, PanelFrame frame)
{
    const auto layout = layoutProse(rows, frame.w, 19);
    const int pageHeight = std::max(24, frame.h - 46);
    // 依完整行分頁，不裁斷長裝備效果，也不把字縮到無法閱讀。
    const auto pages = paginatePanelText(layout, pageHeight);
    textPage_ = std::clamp(textPage_, 0, static_cast<int>(pages.size()) - 1);
    for (const auto& row : pages[textPage_])
        text(row.text, frame.x + row.indentPixels, frame.y + row.y, row.fontSize, panelTextColor(row.tone));
    if (pages.size() > 1)
    {
        button("text-prev", {frame.x, frame.y + frame.h - 44, 64, 44}, "<", [this] { --textPage_; rebuild(); }, {}, textPage_ > 0);
        text(std::format("{}/{}", textPage_ + 1, pages.size()), frame.x + 78, frame.y + frame.h - 30, 18, muted);
        button("text-next", {frame.x + frame.w - 64, frame.y + frame.h - 44, 64, 44}, ">", [this] { ++textPage_; rebuild(); }, {}, textPage_ + 1 < static_cast<int>(pages.size()));
    }
}

void ChessPreparationScreen::equipmentInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    text("裝備", x, y + 8, 24, gold);
    iconButton("equipment-catalog", {x + width - 144, y - 4, 52, 44}, Icon::More, "",
        [this] { open(ChessContextMenuAction::OpenEquipmentMenu); }, "裝備圖鑑／神兵商店");
    const auto* target = rosterPiece(session_.state(), selection_.instanceId);
    if (target)
    {
        const auto* role = session_.content().role(target->roleId);
        image("head", role->HeadID, {x, y + 40, 40, 44});
        text("為" + role->Name + "選擇" + chessEquipmentTypeName(equipmentType_), x + 48, y + 54, 20, ink, width - 52);
    }
    else text("點選出戰或候補棋子，選擇持有人", x, y + 52, 20, muted, width);
    for (int type = 0; type < 2; ++type)
        button(std::format("gear-filter:{}", type), {x + type * (width + 8) / 2, y + 88, (width - 8) / 2, 44},
            chessEquipmentTypeName(type), [this, type] { equipmentType_ = type; equipmentId_ = -1; listPage_ = 0; textPage_ = 0; rebuild(); }, {}, true, equipmentType_ == type);
    std::vector<const ChessEquipmentInstance*> inventory;
    for (const auto& [id, equipment] : session_.state().equipmentInventory)
        if (equipmentDefinition(session_.content(), equipment.itemId)->equipType == equipmentType_) inventory.push_back(&equipment);
    std::stable_sort(inventory.begin(), inventory.end(), [&](const auto* left, const auto* right) {
        const bool leftAvailable = left->assignedChessInstanceId < 0;
        const bool rightAvailable = right->assignedChessInstanceId < 0;
        if (leftAvailable != rightAvailable) return leftAvailable;
        return chessEquipmentMenuOrderLess({true, equipmentDefinition(session_.content(), left->itemId)->tier, left->itemId},
            {true, equipmentDefinition(session_.content(), right->itemId)->tier, right->itemId});
    });
    constexpr int columns = 3;
    constexpr int pageSize = columns * 3;
    constexpr int gap = 6;
    constexpr int cellHeight = 60;
    const int cellWidth = (width - (columns - 1) * gap) / columns;
    const int pages = std::max(1, (static_cast<int>(inventory.size()) + pageSize - 1) / pageSize);
    listPage_ = std::clamp(listPage_, 0, pages - 1);
    const auto selected = std::ranges::find(inventory, equipmentId_, &ChessEquipmentInstance::instanceId);
    if (selected != inventory.end()) listPage_ = static_cast<int>(selected - inventory.begin()) / pageSize;
    else equipmentId_ = inventory.empty() ? -1 : inventory[listPage_ * pageSize]->instanceId;
    if (inventory.empty())
        text(std::format("尚無{}。可從關卡獎勵或神兵商店取得，再回到這裡裝備。", chessEquipmentTypeName(equipmentType_)),
            x, y + 156, 20, muted, width, 4);
    for (int slot = 0; slot < pageSize; ++slot)
    {
        const int index = listPage_ * pageSize + slot;
        if (index >= static_cast<int>(inventory.size())) break;
        const auto& equipment = *inventory[index];
        const auto* item = session_.content().item(equipment.itemId);
        const auto* definition = equipmentDefinition(session_.content(), equipment.itemId);
        PanelFrame frame{x + (slot % columns) * (cellWidth + gap), y + 140 + (slot / columns) * (cellHeight + gap), cellWidth, cellHeight};
        button(std::format("equipment:{}", equipment.instanceId), frame, {}, [this, id = equipment.instanceId] {
            equipmentId_ = id; textPage_ = 0; rebuild();
        }, item->name, true, equipmentId_ == equipment.instanceId, Treatment::Surface);
        controls_.back().equipmentId = equipment.instanceId;
        image("item", item->id, {frame.x + 8, frame.y + 4, frame.w - 16, 32});
        text(item->name, frame.x + 4, frame.y + 40, 16, chessRewardTierColor(definition->tier), frame.w - 8);
        drawings_.push_back([frame, tier = definition->tier] {
            fill({frame.x + 2, frame.y + 6, 2, frame.h - 12}, chessRewardTierColor(tier));
        });
        if (equipment.assignedChessInstanceId >= 0)
            text("裝", frame.x + frame.w - 19, frame.y + 5, 16, equipmentContext);
    }
    if (pages > 1)
    {
        text(std::format("{}–{} / {} 件", listPage_ * pageSize + 1,
            std::min((listPage_ + 1) * pageSize, static_cast<int>(inventory.size())), inventory.size()), x, y + 350, 17, muted);
        iconButton("gear-prev", {x + width - 96, y + 336, 44, 44}, Icon::Left, "",
            [this] { --listPage_; equipmentId_ = -1; rebuild(); }, "上一頁裝備", listPage_ > 0);
        iconButton("gear-next", {x + width - 44, y + 336, 44, 44}, Icon::Right, "",
            [this] { ++listPage_; equipmentId_ = -1; rebuild(); }, "下一頁裝備", listPage_ + 1 < pages);
    }
    else if (!inventory.empty())
        text(std::format("{} 件 · 選擇後按{}", inventory.size(), target ? "下方裝備" : "棋子以裝備"), x, y + 350, 17, muted, width);
    const auto* equipment = equipmentInstance(session_.state(), equipmentId_);
    const int detailY = y + 388;
    const int bottom = inspector_.y + inspector_.h - 64;
    if (equipment)
    {
        const auto metadata = chessEquipmentMetadata(session_.content(), equipment->itemId);
        button("equipment-effects", {x, detailY, width, 56}, {},
            [this] { equipmentPopover_.pin(equipmentId_); }, "完整效果與專屬加成");
        controls_.back().equipmentId = equipmentId_;
        text(metadata.name, x, detailY + 4, 20, chessRewardTierColor(metadata.tier), width - 36);
        icon(Icon::Right, {x + width - 24, detailY + 20, 16, 16}, muted);
        const auto* wearer = rosterPiece(session_.state(), equipment->assignedChessInstanceId);
        if (wearer)
        {
            const auto* role = session_.content().role(wearer->roleId);
            icon(Icon::Link, {x, detailY + 32, 16, 16}, muted);
            text(role->Name, x + 24, detailY + 32, 17, muted, width - 60);
        }
        else
        {
            icon(Icon::Bag, {x, detailY + 32, 16, 16}, muted);
            text("行囊", x + 24, detailY + 32, 17, muted);
        }
        if (target)
        {
            const int oldId = equipmentType_ == 0 ? target->weaponInstanceId : target->armorInstanceId;
            if (oldId >= 0 && oldId != equipmentId_)
                text("替下：" + chessItemDisplayName(session_.content(), session_.state().equipmentInventory.at(oldId).itemId),
                    x, detailY + 64, 17, muted, width);
        }
        const bool equipped = wearer && target && wearer->instanceId == target->instanceId;
        button("equip", {x, bottom, width, 52}, equipped ? "已裝備" : target
                ? std::format("{}給{}", wearer ? "轉交" : "裝備", session_.content().role(target->roleId)->Name) : "先選擇持有人",
            [this] { auto action = simpleAction(ChessActionType::Equip); action.equipmentInstanceId = equipmentId_;
                action.targetChessInstanceId = selection_.instanceId; submit(action, "已裝備"); },
            equipped ? "" : wearer ? "轉交給所選棋子" : "裝備給所選棋子", target && !equipped, false, Treatment::Action);
    }
}

void ChessPreparationScreen::informationInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    const auto& cfg = session_.content().balance();
    std::string title;
    std::vector<PanelTextSourceRow> rows;
    const auto fact = [&](const std::string& label, const std::string& value, int top, Color color) {
        text(label, x, top, 17, muted, width / 2);
        text(value, x + width / 2, top, 19, color, width / 2);
    };
    const auto divider = [&](int top) {
        drawings_.push_back([=] { fill({x, top, width, 1}, ChessUiStyle::line); });
    };
    if (information_ == "economy")
    {
        text("金幣與利息", x, y + 8, 22, gold, width - 88);
        text("目前金幣", x, y + 64, 18, muted);
        icon(Icon::Coin, {x, y + 90, 28, 28}, gold);
        text(std::to_string(observation_.money), x + 40, y + 86, 32, gold);
        divider(y + 134);
        text("利息", x, y + 154, 24, gold);
        fact("本輪利息", std::format("+{} 金", observation_.interestGold), y + 190, gold);
        fact("計算比例", std::format("{}% · 上限 {} 金", cfg.interestPercent, observation_.maximumInterestGold), y + 226, muted);
        if (observation_.nextInterestThreshold)
        {
            fact("下一檔存款", std::format("{} 金", *observation_.nextInterestThreshold), y + 262, gold);
            fact("還需保留", std::format("{} 金", *observation_.nextInterestThreshold - observation_.money), y + 298, context);
        }
        else text(cfg.interestPercent > 0 ? "已達利息上限" : "本難度不計利息", x, y + 266, 20, muted, width);
        divider(y + 336);
        text("預計勝利收入", x, y + 356, 24, gold);
        fact("基本獎勵", std::format("+{} 金", observation_.projectedBaseVictoryGold), y + 392, gold);
        fact("利息", std::format("+{} 金", observation_.interestGold), y + 428, gold);
        divider(y + 460);
        fact("合計", std::format("+{} 金", observation_.projectedVictoryIncome), y + 480, positive);
        return;
    }
    else if (information_ == "level")
    {
        const bool canLevel = observation_.level < cfg.maxLevel;
        text("等級與經驗", x, y + 8, 22, gold, width - 88);
        text(std::format("等級 {}", observation_.level + 1), x, y + 66, 28, context);
        text(canLevel ? std::format("經驗 {} / {}", observation_.experience, observation_.experienceForNextLevel) : "已達最高等級",
            x, y + 106, 18, context, width);
        divider(y + 138);
        fact("出戰上限", std::format("{} + {} = {} 名", observation_.baseDeployment, observation_.deploymentBonus, observation_.maximumDeployment), y + 156, positive);
        if (canLevel)
        {
            const auto deployment = buildChessBuyExpDeploymentPreview(session_.state(), session_.content());
            fact("下一級出戰", std::format("{} 名", deployment.next.value_or(deployment.current)), y + 190, positive);
        }
        text("商店階級機率", x, y + 238, 24, gold);
        text("階級", x, y + 276, 18, muted);
        text("目前", x + 112, y + 276, 18, muted);
        text(canLevel ? "下一級" : "已滿級", x + 222, y + 276, 18, muted);
        for (int tier = 0; tier < 5; ++tier)
        {
            const auto color = chessPieceTierColor(tier + 1);
            const int top = y + 308 + tier * 32;
            text(std::format("{}費", tier + 1), x, top, 18, color);
            text(std::format("{}%", cfg.shopWeights[observation_.level][tier]), x + 112, top, 18, color);
            text(canLevel ? std::format("{}%", cfg.shopWeights[observation_.level + 1][tier]) : "—", x + 222, top, 18, canLevel ? color : muted);
        }
        if (canLevel)
        {
            iconButton("experience", {x, inspector_.y + inspector_.h - 64, width, 52}, Icon::Cultivation,
                std::format("{} 金 → +{} 經驗", cfg.buyExpCost, cfg.buyExpAmount),
                [this] { submit(simpleAction(ChessActionType::BuyExp), "已購買經驗"); }, "購買經驗");
            controls_.back().touchConfirmation = true;
        }
        return;
    }
    else if (information_ == "talent")
    {
        text("天賦 · " + observation_.talentName, x, y + 8, 22, gold, width - 88, 2);
        const auto presentation = buildChessTalentPresentation(cfg, observation_.talent);
        const bool shopUnlocked = ChessManagementRules::legendaryShopUnlocked(session_.state(), session_.content());
        const int shopSpace = shopUnlocked ? 64 : 0;
        const int height = inspector_.h - 150 - shopSpace;
        const auto pages = paginateChessTalentFacts(presentation, width, height, 18);
        textPage_ = std::clamp(textPage_, 0, static_cast<int>(pages.size()) - 1);
        drawings_.push_back([=, page = pages[textPage_]] { drawChessTalentFactTable(page, x, y + 64, width, height, 18, 18); });
        const int bottom = inspector_.y + inspector_.h - 60 - shopSpace;
        iconButton("talent-prev", {x, bottom, 44, 44}, Icon::Up, "", [this] { --textPage_; rebuild(); }, {}, textPage_ > 0);
        text(std::format("{} / {}", textPage_ + 1, pages.size()), x + 64, bottom + 12, 18, muted);
        iconButton("talent-next", {x + width - 44, bottom, 44, 44}, Icon::Down, "", [this] { ++textPage_; rebuild(); }, {}, textPage_ + 1 < static_cast<int>(pages.size()));
        if (shopUnlocked)
            iconButton("talent-shop", {x, inspector_.y + inspector_.h - 64, width, 52}, Icon::Sword, "進入神兵商店",
                [this] { open(ChessContextMenuAction::BuyLegendaryEquipment); }, "購買傳說裝備", true, false, gold, Treatment::Action);
        return;
    }
    else
    {
        title = "難度";
        rows = paragraph(std::format("{}\n第 {} / {} 關\n出戰上限 {} 名\n候補容量 {} 名", chessDifficultyDisplayName(observation_.difficulty),
            observation_.fight + 1, cfg.totalFights, observation_.maximumDeployment, cfg.benchSize));
    }
    text(title, x, y + 8, 22, gold, width - 88, 2);
    prose(rows, {x, y + 76, width, inspector_.h - 160});
}

void ChessPreparationScreen::neigongInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    std::vector<PanelTextSourceRow> rows;
    if (view_ == View::Neigongs)
    {
        text("已獲得內功", x, y + 8, 24, gold);
        std::vector<const NeigongDef*> owned;
        for (const auto& ng : session_.content().neigong())
            if (session_.state().obtainedNeigongIds.contains(ng.magicId)) owned.push_back(&ng);
        if (owned.empty()) text("尚未獲得內功", x, y + 80, 18, muted);
        const int pages = std::max(1, (static_cast<int>(owned.size()) + 5) / 6);
        listPage_ = std::clamp(listPage_, 0, pages - 1);
        for (int i = 0; i < 6 && listPage_ * 6 + i < static_cast<int>(owned.size()); ++i)
        {
            const auto& ng = *owned[listPage_ * 6 + i];
            PanelFrame frame{x, y + 64 + i * 64, width, 56};
            button(std::format("neigong:{}", ng.magicId), frame, {}, [this, id = ng.magicId] { navigate(View::Neigong, id); }, ng.name);
            image("item", ng.itemId, {frame.x + 6, frame.y + 4, 44, 46});
            text(ng.name, frame.x + 62, frame.y + 18, 18, chessRewardTierColor(ng.tier), width - 72);
        }
        button("neigong-page", {x, inspector_.y + inspector_.h - 60, width, 44}, std::format("{} / {}  下一頁", listPage_ + 1, pages),
            [this, pages] { listPage_ = (listPage_ + 1) % pages; rebuild(); }, {}, pages > 1);
        return;
    }
    const auto ng = std::ranges::find(session_.content().neigong(), neigongId_, &NeigongDef::magicId);
    assert(ng != session_.content().neigong().end());
    text("內功詳情", x, y + 8, 24, gold);
    image("item", ng->itemId, {x, y + 64, 64, 64});
    text(ng->name, x + 78, y + 80, 22, chessRewardTierColor(ng->tier), width - 82, 2);
    rows = panelTextRowsForEffectDescription(describeGameplayEffects(ng->effects, EffectDescriptionStyle::Full));
    rows.insert(rows.begin(), {.text = "對所有成員自動生效", .spacingAfter = 12});
    prose(rows, {x, y + 152, width, inspector_.h - 180});
}

void ChessPreparationScreen::footer()
{
    const int warningHeight = observation_.deploymentOverflow > 0 ? 24 : 0;
    const int y = footer_.y + warningHeight + (footer_.h - warningHeight - 52) / 2;
    panel(footer_);
    if (warningHeight > 0)
        text("出戰加成已失效。恢復羈絆或減少出戰人數後即可開戰。", footer_.x + 16, footer_.y + 4, 16, negative);
    constexpr int navigationWidth = 108;
    const auto slot = [y](int index) { return PanelFrame{112 + index * (navigationWidth + 8), y + 4, navigationWidth, 44}; };
    iconButton("expedition", slot(0), Icon::Expedition, "遠征", [this] { open(ChessContextMenuAction::ShowExpeditionChallenge); },
        observation_.deploymentReadinessReason.empty() ? "遠征挑戰" : observation_.deploymentReadinessReason,
        observation_.deploymentReadinessReason.empty());
    iconButton("equipment", slot(1), Icon::Bag, "包", [this] { navigate(View::Equipment); }, "裝備行囊");
    iconButton("overview", slot(2), Icon::Catalog, "鑑", [this] { open(ChessContextMenuAction::OpenOverviewMenu); }, "圖鑑");
    iconButton("offline", slot(3), Icon::Duel, "演", [this] { open(ChessContextMenuAction::OpenOfflineBattle); },
        observation_.deploymentReadinessReason.empty() ? "離線對戰" : observation_.deploymentReadinessReason,
        observation_.deploymentReadinessReason.empty());
    iconButton("reroll-enemy", slot(4), Icon::Dice, "改",
        [this] { open(ChessContextMenuAction::RerollBattleSeed); }, "逆天改命：重抽下場敵軍，地圖可能不變");
    iconButton("save", slot(5), Icon::Save, "存", [this] { open(ChessContextMenuAction::SaveProgress); }, "存檔");
    iconButton("load", slot(6), Icon::Load, "讀", [this] { open(ChessContextMenuAction::LoadProgress); }, "讀檔");
    button("battle", {w_ - 304, y + 4, 176, 44}, observation_.campaignComplete ? "已通關 · 遠征" : "進入戰鬥",
        [this] { open(observation_.campaignComplete ? ChessContextMenuAction::ShowExpeditionChallenge : ChessContextMenuAction::EnterBattle); },
        observation_.deploymentReadinessReason.empty() ? "使用目前陣容出戰" : observation_.deploymentReadinessReason,
        observation_.deploymentReadinessReason.empty(), false, Treatment::Primary);
}

bool ChessPreparationScreen::guideAllows(const std::string& key) const
{
    if (key.starts_with("guide-")) return true;
    switch (guideStep_)
    {
    case 0: return key.starts_with("buy:") || key.starts_with("offer:");
    case 1: return key.starts_with("piece:") || key == "single-click"
        || key == "back" || key == "bench-prev" || key == "bench-next";
    case 2: return key == "refresh" || key == "lock";
    default: return false;
    }
}

void ChessPreparationScreen::guide()
{
    // 導覽直接標示整備中的真實控制項，不建立另一套棋盤或示範棋子。
    for (auto& control : controls_) control.enabled = control.enabled && guideAllows(control.key);
    const PanelFrame spotlight = guideStep_ == 1
        ? PanelFrame{team_.x, team_.y, team_.w, bench_.y + bench_.h - team_.y}
        : guideStep_ == 3 ? footer_ : shop_;
    const PanelFrame popup = guideStep_ == 1
        ? shop_ : PanelFrame{(w_ - 800) / 2, 154, 800, 242};
    drawings_.push_back([=, this] {
        // 留出正在講解的真實區域；其餘畫面使用共用遮罩。
        fill({0, 0, w_, spotlight.y}, canvasScrim);
        fill({0, spotlight.y, spotlight.x, spotlight.h}, canvasScrim);
        fill({spotlight.x + spotlight.w, spotlight.y, w_ - spotlight.x - spotlight.w, spotlight.h}, canvasScrim);
        fill({0, spotlight.y + spotlight.h, w_, h_ - spotlight.y - spotlight.h}, canvasScrim);
        outline(spotlight, context, Spacing::panelCorner);
        outline({spotlight.x + 1, spotlight.y + 1, spotlight.w - 2, spotlight.h - 2}, context, Spacing::panelCorner);
        fill(popup, darkLabel, Spacing::panelCorner);
        outline(popup, gold, Spacing::panelCorner);
    });
    constexpr std::array titles{"點商店的金幣購買", "在出戰與候補之間移動", "刷新與鎖定商店", "準備好，就出戰"};
    constexpr std::array descriptions{
        "下方就是真正的商店。點肖像查看資料，點金幣價格直接購買。\n有出戰空位時自動上陣，否則留在候補。這裡的操作會使用本局金幣。",
        "單擊查看、雙擊移動。點候補列箭頭開啟單擊模式後，點棋子即可移動。\n出戰或候補已滿時無法移入；先騰出目的地空位，再移動棋子。",
        "刷新圖示旁的 $ 是花費；手機點兩下確認刷新。鎖頭亮起時，下一關保留這批棋子。\n可以直接操作下方的真實按鈕，也可以按下一步繼續。",
        "下方「包」是裝備行囊，「存／讀」是存讀檔。手機長按圖示查看說明，放開不執行。\n右側「進入戰鬥」開始出戰；候補列的問號可重新開啟導覽。"};
    text(titles[guideStep_], popup.x + 24, popup.y + 18, 24, gold);
    text(std::format("{} / 4", guideStep_ + 1), popup.x + popup.w - 118, popup.y + 22, 18, muted);
    iconButton("guide-dismiss", {popup.x + popup.w - 52, popup.y + 10, 40, 40}, Icon::Close, "",
        [this] { finishGuide(); }, "關閉教學");
    const auto layout = layoutProse(paragraph(descriptions[guideStep_]), popup.w - 48, 20);
    for (const auto& row : layout.lines)
        text(row.text, popup.x + 24 + row.indentPixels, popup.y + 56 + row.y, row.fontSize, ink);
    const int bottom = popup.y + popup.h - 52;
    button("guide-close", {popup.x + 16, bottom, 104, 40}, "結束導覽", [this] { finishGuide(); });
    button("guide-prev", {popup.x + popup.w - 280, bottom, 112, 40}, "上一步",
        [this] { --guideStep_; rebuild(); }, {}, guideStep_ > 0);
    button("guide-next", {popup.x + popup.w - 152, bottom, 128, 40}, guideStep_ == 3 ? "開始整備" : "下一步",
        [this] { if (guideStep_ == 3) finishGuide(); else { ++guideStep_; rebuild(); } },
        {}, true, false, Treatment::Primary);
    // 直接框選價格／模式／常用操作，讓玩家把說明對應到真正的按鈕。
    for (const auto& control : controls_)
    {
        const bool marked = guideStep_ == 0 ? control.key.starts_with("buy:")
            : guideStep_ == 1 ? control.key == "single-click"
            : guideStep_ == 2 ? control.key == "refresh" || control.key == "lock"
            : control.key == "equipment" || control.key == "save" || control.key == "load" || control.key == "battle";
        if (marked) drawings_.push_back([frame = control.frame] { outline(frame, context, 4); });
    }
    if (!guideAllows(focusKey_)) focusKey_ = "guide-next";
}

void ChessPreparationScreen::finishGuide()
{
    guideStep_ = -1;
    onPointerInputReset();
    auto settings = SystemSettings::getInstance()->snapshot();
    settings.preparationGuideSeen = true;
    SystemSettings::getInstance()->update(settings);
    focusKey_ = "guide";
    rebuild();
}

void ChessPreparationScreen::navigate(View view, int detailId, std::string information)
{
    if (view_ == view && view != View::Neigong
        && (view != View::Information || information_ == information)) { rebuild(); return; }
    history_.push({view_, listPage_, textPage_, focusKey_, selection_, equipmentId_, equipmentType_, neigongId_, information_});
    view_ = view;
    if (view == View::Neigong) neigongId_ = detailId;
    if (view == View::Information) information_ = std::move(information);
    listPage_ = 0;
    textPage_ = 0;
    rebuild();
}

void ChessPreparationScreen::back()
{
    if (replacementIncomingId_ >= 0)
    {
        replacementIncomingId_ = replacementTargetId_ = replacementPreviewId_ = -1;
        portraitClicks_.reset(); rebuild(); return;
    }
    if (view_ == View::Piece && characterInspector_.expanded)
    {
        characterInspector_.expanded = false;
        rebuild();
        return;
    }
    if (auto previous = history_.pop())
    {
        auto context = std::move(*previous);
        view_ = context.view;
        selection_ = context.selection;
        equipmentId_ = context.equipmentId;
        equipmentType_ = context.equipmentType;
        neigongId_ = context.neigongId;
        information_ = std::move(context.information);
        listPage_ = context.listPage;
        textPage_ = context.textPage;
        focusKey_ = std::move(context.focusKey);
    }
    else { view_ = View::Piece; textPage_ = 0; listPage_ = 0; }
    rebuild();
}

void ChessPreparationScreen::selectPiece(int id)
{
    if (replacementIncomingId_ >= 0)
    {
        if (session_.state().roster.at(id).deployed) replacementTargetId_ = id;
        rebuild(); return;
    }
    selection_.selectPiece(id);
    characterInspector_ = {};
    if (view_ != View::Equipment) { view_ = View::Piece; history_.clear(); }
    rebuild();
}

void ChessPreparationScreen::selectShop(int slot)
{
    selection_.selectShop(slot, session_.state().shop[slot].roleId);
    characterInspector_ = {};
    view_ = View::Piece;
    history_.clear();
    rebuild();
}

void ChessPreparationScreen::pickEquipment(int type)
{
    assert(selection_.instanceId >= 0);
    navigate(View::Equipment);
    equipmentType_ = type;
    equipmentId_ = -1;
    listPage_ = 0;
    textPage_ = 0;
    rebuild();
}

void ChessPreparationScreen::ban(int roleId)
{
    auto action = simpleAction(ChessActionType::AddBan);
    action.roleId = roleId;
    submit(action, {});
}

void ChessPreparationScreen::chooseLateBloomer(int instanceId)
{
    const auto& piece = session_.state().roster.at(instanceId);
    const auto& name = session_.content().role(piece.roleId)->Name;
    if (runChessChoiceMenu(std::format("確認培養「{}」", name), {"確定培養", "取消"},
        ChessChoiceStyle::Confirmation, nullptr, chessLateBloomerSelectionDescription(session_.content(), piece)) == 0)
    {
        auto action = simpleAction(ChessActionType::ChooseLateBloomer);
        action.chessInstanceId = instanceId;
        submit(action, std::format("已選定{}，出戰勝場會累積培養進度", name));
    }
    else rebuild();
}

void ChessPreparationScreen::open(ChessContextMenuAction command)
{
    // 先離開整備的輸入派送，再由 adapter 執行戰鬥／獎勵等既有流程。
    // WebAssembly 不必把整個戰鬥疊在按鈕回呼與整備 run 迴圈之下。
    pendingCommand_ = command;
    setExit(true);
}

std::optional<ChessContextMenuAction> ChessPreparationScreen::takeCommand()
{
    return std::exchange(pendingCommand_, std::nullopt);
}

ChessActionResult ChessPreparationScreen::submit(const ChessAction& action, const std::string& message)
{
    const auto previous = cardFrames();
    auto result = callbacks_.submit(action);
    notice_ = result.accepted ? message : result.description;
    if (action.type == ChessActionType::SetDeployment)
    {
        if (result.error == ChessRuleErrorCode::BenchFull) notice_ = "候補已滿，請先騰出候補空位";
        if (result.error == ChessRuleErrorCode::DeploymentLimitExceeded) notice_ = "出戰已滿，請先讓出戰棋子下陣或提高出戰上限";
    }
    const int control = focusIndex();
    noticeAnchor_ = control >= 0 ? controls_[control].frame : inspector_;
    noticeUntil_ = Engine::getTicks() + (result.accepted ? 2600 : 5000);
    if (result.accepted)
    {
        std::erase_if(luckFeedback_, [](const auto& feedback) { return Engine::getTicks() >= feedback.until; });
        arrivalIds_.clear();
        arrivalMerged_ = false;
        for (const auto& event : result.events)
        {
            if (event.type == ChessSemanticEventType::LuckGranted)
            {
                std::erase_if(luckFeedback_, [&](const auto& feedback) { return feedback.instanceId == event.primaryId; });
                luckFeedback_.push_back({event.primaryId, event.value, Engine::getTicks() + 1800});
            }
            if (event.type == ChessSemanticEventType::LateBloomerEvolved)
                notice_ = std::format("{}突破四星！", session_.content().role(event.secondaryId)->Name);
            if (event.type == ChessSemanticEventType::ChessPurchased || event.type == ChessSemanticEventType::ChessMerged
                || event.type == ChessSemanticEventType::LateBloomerEvolved)
            {
                arrivalIds_.push_back(event.primaryId);
                if (event.type != ChessSemanticEventType::ChessPurchased) arrivalMerged_ = true;
            }
        }
        if (arrivalMerged_) playChessUpgradeSound();
        if (!arrivalIds_.empty()) arrivalUntil_ = Engine::getTicks() + 1500;
    }
    rebuild();
    if (result.accepted && (action.type == ChessActionType::SetDeployment || action.type == ChessActionType::SellChess))
        animateCardsFrom(previous);
    return result;
}

void ChessPreparationScreen::buy(int slot)
{
    auto previous = cardFrames();
    const auto offer = std::ranges::find(controls_, std::format("offer:{}:{}", slot, session_.state().shop[slot].roleId), &Control::key);
    assert(offer != controls_.end());
    const auto shopFrame = offer->frame;
    const auto* role = session_.content().role(session_.state().shop[slot].roleId);
    assert(role);
    const auto name = role->Name;
    auto action = simpleAction(ChessActionType::BuyShopSlot);
    action.shopSlot = slot;
    const auto result = submit(action, "已招募 " + name);
    if (!result.accepted) return;
    const auto destination = chessPreparationPurchaseDestination(session_.state(), result);
    assert(destination);
    selection_.selectPiece(*destination);
    const bool merged = arrivalMerged_;
    const auto arrivals = arrivalIds_;
    view_ = View::Piece;
    history_.clear();
    const auto& actual = session_.state().roster.at(*destination);
    notice_ = std::format("{} {} {} · {}", merged ? "合成完成" : "已招募", name, chessStars(actual.star), actual.deployed ? "出戰" : "候補");
    arrivalIds_ = arrivals;
    arrivalMerged_ = merged;
    benchPage_ = actual.deployed ? 0 : static_cast<int>(rosterOrder_.bench.size() - 1) / 10;
    rebuild();
    if (!merged) previous.emplace(*destination, shopFrame);
    animateCardsFrom(previous);
}

void ChessPreparationScreen::deploy()
{
    const auto& piece = session_.state().roster.at(selection_.instanceId);
    const auto action = chessPreparationDeployment(session_.state(), piece.instanceId);
    submit(action, piece.deployed ? "棋子已撤回候補" : "棋子已上陣");
}

void ChessPreparationScreen::draw()
{
    // Talk／獎勵／戰前面板執行時，只顯示底下的場景與天氣。
    if (!isCurrentRunOwner()) return;
    for (std::size_t i = 0; i < guideDrawingStart_; ++i) drawings_[i]();
    const auto now = Engine::getTicks();
    // 靜止卡片先畫，移動卡片置頂；導覽遮罩與按鈕仍在最上層。
    for (const bool moving : {false, true})
        for (const auto& card : pieceCards_)
        {
            const auto motion = cardMotions_.find(card.piece.instanceId);
            const bool active = motion != cardMotions_.end() && motion->second.active(now);
            if (active == moving) drawPieceCard(card, active ? motion->second.frame(now) : card.frame);
        }
    for (std::size_t i = guideDrawingStart_; i < drawings_.size(); ++i) drawings_[i]();
}

int ChessPreparationScreen::hit(SDL_FPoint point) const
{
    for (int i = static_cast<int>(controls_.size()) - 1; i >= 0; --i)
    {
        if (guideStep_ >= 0 && !guideAllows(controls_[i].key)) continue;
        if ((controls_[i].key.starts_with("piece:") || controls_[i].key.starts_with("piece-gear:"))
            && std::ranges::any_of(cardMotions_, [](const auto& entry) { return entry.second.active(Engine::getTicks()); })) continue;
        if (contains(controls_[i].frame, point)) return i;
    }
    return -1;
}

int ChessPreparationScreen::focusIndex() const
{
    const auto found = std::ranges::find(controls_, focusKey_, &Control::key);
    return found == controls_.end() ? -1 : static_cast<int>(found - controls_.begin());
}

void ChessPreparationScreen::activate(std::string key, bool touch)
{
    confirmation_.cancelUnless(key);
    if (confirmation_.armedKey.empty()) touchHint_.clear();
    const auto found = std::ranges::find(controls_, key, &Control::key);
    if (found == controls_.end() || !found->enabled || !found->activate) return;
    focusKey_ = key;
    if (((touch && found->touchConfirmation) || !found->confirmationLabel.empty()) && !confirmation_.confirm(key))
    {
        auto preview = found->preview;
        const bool inlineConfirmation = !found->confirmationLabel.empty();
        if (preview) preview();
        touchHint_ = inlineConfirmation ? "" : "再點一次確認";
        rebuild();
        return;
    }
    confirmation_.reset();
    touchHint_.clear();
    // 操作會重建 controls_；執行前複製，避免銷毀正在執行的函式。
    auto action = found->activate;
    action();
}

RunNode::PointerResult ChessPreparationScreen::onPointerEvent(const PointerEvent& event)
{
    if (event.phase == PointerPhase::ButtonDown)
    {
        const int target = hit(event.uiPosition);
        confirmation_.cancelUnless(target >= 0 ? controls_[target].key : std::string{});
        if (confirmation_.armedKey.empty()) touchHint_.clear();
    }
    if (event.phase == PointerPhase::Cancel) { onPointerInputReset(); return PointerResult::Released; }
    touchInput_ = event.source == PointerSource::Touch || GameUtil::isMobileDevice();
    if (event.phase == PointerPhase::Motion && (event.uiDelta.x != 0 || event.uiDelta.y != 0))
        keyboardFocus_ = false;
    if (equipmentPopover_.equipmentId >= 0)
    {
        const bool inside = contains(popoverFrame_, event.uiPosition);
        if (event.phase == PointerPhase::Wheel && inside)
        {
            equipmentPopover_.scrollBy(static_cast<int>(-event.wheel.y * 36), popoverScrollMax_);
            return PointerResult::Handled;
        }
        if (event.phase == PointerPhase::Motion && popoverDragging_)
        {
            equipmentPopover_.scrollBy(static_cast<int>(-event.uiDelta.y), popoverScrollMax_);
            return PointerResult::Handled;
        }
        if (event.button == SDL_BUTTON_LEFT && event.phase == PointerPhase::ButtonDown)
        {
            if (inside)
            {
                pressedKey_ = contains(popoverClose_, event.uiPosition) ? "popover-close" : "popover-body";
                popoverDragging_ = pressedKey_ == "popover-body";
                return PointerResult::Captured;
            }
            // 釘選視窗外的第一下只關閉，避免觸控誤買或誤換裝。
            const bool pinned = equipmentPopover_.pinned;
            dismissEquipmentPopover();
            if (pinned) { pressedKey_ = "popover-dismiss"; return PointerResult::Captured; }
        }
    }
    if (event.phase == PointerPhase::Wheel && view_ == View::Piece
        && characterInspector_.expanded && contains(inspector_, event.uiPosition))
    {
        characterInspector_.page += event.wheel.y > 0 ? -1 : 1;
        rebuild();
        return PointerResult::Handled;
    }
    if (event.phase == PointerPhase::ButtonUp && pressedKey_.starts_with("popover-"))
    {
        const auto key = std::exchange(pressedKey_, {});
        popoverDragging_ = false;
        if (key == "popover-close" && contains(popoverClose_, event.uiPosition)) dismissEquipmentPopover();
        return PointerResult::Handled;
    }
    const int index = hit(event.uiPosition);
    if (replacementIncomingId_ >= 0 && replacementTargetId_ < 0 && event.phase == PointerPhase::Motion
        && pressedKey_.empty() && index >= 0 && controls_[index].key.starts_with("piece:"))
    {
        const int id = std::stoi(controls_[index].key.substr(6));
        if (session_.state().roster.at(id).deployed && replacementPreviewId_ != id)
        { replacementPreviewId_ = id; rebuild(); return PointerResult::Handled; }
    }
    if (event.phase == PointerPhase::Motion)
        pressPreview_.move(index >= 0 ? controls_[index].key : std::string{});
    if (event.button != SDL_BUTTON_LEFT) return PointerResult::Handled;
    if (event.phase == PointerPhase::ButtonDown)
    {
        keyboardFocus_ = false;
        pressedKey_ = index >= 0 ? controls_[index].key : std::string{};
        if (index >= 0) focusKey_ = pressedKey_;
        pressPreview_.reset();
        if (touchInput_ && index >= 0 && (!controls_[index].tooltip.empty()
            || !controls_[index].tooltipRows.empty() || controls_[index].equipmentId >= 0))
            pressPreview_.start(pressedKey_, Engine::getTicks());
        return PointerResult::Captured;
    }
    if (event.phase == PointerPhase::ButtonUp)
    {
        const auto key = std::exchange(pressedKey_, {});
        if (pressPreview_.release(Engine::getTicks()))
        {
            portraitClicks_.reset();
            hoverKey_.clear();
            dismissEquipmentPopover();
            return PointerResult::Handled;
        }
        if (index >= 0 && controls_[index].key == key)
        {
            const auto shortcut = controls_[index].shortcut;
            const bool touch = event.source == PointerSource::Touch || GameUtil::isMobileDevice();
            if (shortcut)
            {
                // 出戰狀態也屬於身分，避免同一串輸入反轉剛完成的上／下陣。
                const auto identity = key + (key.starts_with("piece:")
                    && session_.state().roster.at(std::stoi(key.substr(6))).deployed ? ":deployed" : "");
                const bool moveOnClick = singleClickMovement_ && key.starts_with("piece:");
                if (portraitClicks_.release(identity, event.clicks, event.timestamp, touch, moveOnClick))
                { confirmation_.reset(); touchHint_.clear(); shortcut(); }
                else activate(key, touch);
            }
            else { portraitClicks_.reset(); activate(key, touch); }
        }
        else portraitClicks_.reset();
        // 已自行完成命中與啟動；框架仍會解除擷取，不再轉送 onPressedOK。
        return PointerResult::Handled;
    }
    return PointerResult::Handled;
}

void ChessPreparationScreen::onPointerInputReset()
{
    pressedKey_.clear();
    hoverKey_.clear();
    pressPreview_.reset();
    portraitClicks_.reset();
    confirmation_.reset();
    touchHint_.clear();
    equipmentPopover_.dismiss();
    popoverFrame_ = {};
    popoverDragging_ = false;
}

void ChessPreparationScreen::moveFocus(int dx, int dy)
{
    const int index = focusIndex();
    if (index < 0) return;
    const auto& start = controls_[index].frame;
    const float sx = start.x + start.w / 2.0f, sy = start.y + start.h / 2.0f;
    float best = std::numeric_limits<float>::max();
    int chosen = index;
    for (int i = 0; i < static_cast<int>(controls_.size()); ++i)
    {
        if (i == index || !controls_[i].enabled || !controls_[i].activate) continue;
        const auto& frame = controls_[i].frame;
        const float x = frame.x + frame.w / 2.0f - sx, y = frame.y + frame.h / 2.0f - sy;
        const float forward = dx * x + dy * y;
        if (forward <= 0) continue;
        const float across = std::abs(dx * y - dy * x);
        const float score = forward + across * 3;
        if (score < best) { best = score; chosen = i; }
    }
    focusKey_ = controls_[chosen].key;
    if (replacementIncomingId_ >= 0 && replacementTargetId_ < 0 && focusKey_.starts_with("piece:"))
    {
        const int id = std::stoi(focusKey_.substr(6));
        if (session_.state().roster.at(id).deployed) { replacementPreviewId_ = id; rebuild(); }
    }
}

void ChessPreparationScreen::dealEvent(EngineEvent& event)
{
    // 與場景的 Q 快捷鍵同樣在放開時處理，避免收起的 key-up 又把整備打開。
    if (event.type == EVENT_KEY_UP && event.key.key == SDLK_Q)
    { if (replacementIncomingId_ >= 0) back(); else if (guideStep_ >= 0) finishGuide(); else setExit(true); return; }
    if (event.type == EVENT_GAMEPAD_BUTTON_UP)
    {
        keyboardFocus_ = true;
        if (equipmentPopover_.pinned)
        {
            if (event.gbutton.button == GAMEPAD_BUTTON_DPAD_UP) equipmentPopover_.scrollBy(-48, popoverScrollMax_);
            if (event.gbutton.button == GAMEPAD_BUTTON_DPAD_DOWN) equipmentPopover_.scrollBy(48, popoverScrollMax_);
            return;
        }
        switch (event.gbutton.button)
        {
        case GAMEPAD_BUTTON_DPAD_LEFT: moveFocus(-1, 0); break;
        case GAMEPAD_BUTTON_DPAD_RIGHT: moveFocus(1, 0); break;
        case GAMEPAD_BUTTON_DPAD_UP: moveFocus(0, -1); break;
        case GAMEPAD_BUTTON_DPAD_DOWN: moveFocus(0, 1); break;
        default: break;
        }
        return;
    }
    if (event.type != EVENT_KEY_DOWN || event.key.repeat) return;
    keyboardFocus_ = true;
    if (equipmentPopover_.equipmentId >= 0)
    {
        switch (event.key.key)
        {
        case SDLK_PAGEDOWN: equipmentPopover_.scrollBy(180, popoverScrollMax_); return;
        case SDLK_PAGEUP: equipmentPopover_.scrollBy(-180, popoverScrollMax_); return;
        case SDLK_HOME: equipmentPopover_.scroll = 0; return;
        case SDLK_END: equipmentPopover_.scroll = popoverScrollMax_; return;
        default: break;
        }
        if (equipmentPopover_.pinned && event.key.key != SDLK_ESCAPE) return;
    }
    if (view_ == View::Piece && characterInspector_.expanded
        && (event.key.key == SDLK_PAGEUP || event.key.key == SDLK_PAGEDOWN))
    {
        characterInspector_.page += event.key.key == SDLK_PAGEUP ? -1 : 1;
        rebuild();
        return;
    }
    switch (event.key.key)
    {
    case SDLK_TAB:
    {
        const int count = static_cast<int>(controls_.size());
        int index = focusIndex();
        const int step = event.key.mod & SDL_KMOD_SHIFT ? -1 : 1;
        for (int i = 0; i < count; ++i)
        {
            index = (index + count + step) % count;
            if (controls_[index].enabled && controls_[index].activate) { focusKey_ = controls_[index].key; break; }
        }
        break;
    }
    case SDLK_LEFT: moveFocus(-1, 0); break;
    case SDLK_RIGHT: moveFocus(1, 0); break;
    case SDLK_UP: moveFocus(0, -1); break;
    case SDLK_DOWN: moveFocus(0, 1); break;
    default: break;
    }
}

void ChessPreparationScreen::onPressedCancel()
{
    if (replacementIncomingId_ >= 0) { back(); return; }
    if (guideStep_ >= 0) { finishGuide(); return; }
    if (equipmentPopover_.equipmentId >= 0) { dismissEquipmentPopover(); return; }
    if (!confirmation_.armedKey.empty())
    {
        confirmation_.reset(); touchHint_.clear(); rebuild(); return;
    }
    if (view_ == View::Piece && characterInspector_.expanded) back();
    else if (view_ == View::Piece) setExit(true);
    else back();
}

void ChessPreparationScreen::onPressedContextMenu() { onPressedCancel(); }

void ChessPreparationScreen::onPressedOK()
{
    keyboardFocus_ = true;
    if (equipmentPopover_.pinned) { dismissEquipmentPopover(); return; }
    activate(focusKey_);
}

void ChessPreparationScreen::dismissEquipmentPopover()
{
    dismissedPopoverId_ = equipmentPopover_.equipmentId;
    equipmentPopover_.dismiss();
    popoverFrame_ = {};
    popoverDragging_ = false;
}

void ChessPreparationScreen::drawEquipmentPopover()
{
    const auto* equipment = equipmentInstance(session_.state(), equipmentPopover_.equipmentId);
    assert(equipment);
    const auto metadata = chessEquipmentMetadata(session_.content(), equipment->itemId);
    const auto* wearer = rosterPiece(session_.state(), equipment->assignedChessInstanceId);
    const auto* recipient = view_ == View::Equipment ? rosterPiece(session_.state(), selection_.instanceId) : wearer;
    auto rows = panelTextRowsForEquipment(metadata, recipient ? std::optional{recipient->roleId} : std::nullopt);
    const std::string owner = wearer ? session_.content().role(wearer->roleId)->Name : "行囊";
    const auto context = recipient && recipient != wearer
        ? owner + " → " + session_.content().role(recipient->roleId)->Name
        : wearer ? owner + " · 已裝備" : "未裝備";
    const int width = 500;
    const auto layout = layoutProse(rows, width - 48, 17);
    const int height = std::min(h_ - 144, layout.height + 140);
    const int x = inspector_.x - width - 8;
    const int y = std::clamp(inspector_.y + (inspector_.h - height) / 2, 80, h_ - height - 72);
    popoverFrame_ = {x, y, width, height};
    popoverClose_ = {x + width - 52, y + 8, 44, 44};
    fill({x - 4, y + 4, width + 8, height + 4}, {0, 0, 0, 65}, 10);
    fill(popoverFrame_, {29, 26, 23, 250}, 8);
    outline(popoverFrame_, {177, 158, 120, 90}, 8);
    fill({x + 20, y + 20, 3, 28}, chessRewardTierColor(metadata.tier));
    drawText(metadata.name, 22, x + 36, y + 24, chessRewardTierColor(metadata.tier));
    drawText(context, 15, x + 36, y + 54, muted);
    drawIcon(Icon::Close, {popoverClose_.x + 12, popoverClose_.y + 12, 20, 20}, muted);
    const int bodyY = y + 84;
    const int bodyHeight = height - 134;
    popoverScrollMax_ = std::max(0, layout.height - bodyHeight);
    equipmentPopover_.scrollBy(0, popoverScrollMax_);
    for (const auto& row : layout.lines)
    {
        const int rowY = row.y - equipmentPopover_.scroll;
        // 以完整行顯示，不讓文字越過標題、提示或畫布邊緣。
        if (rowY >= 0 && rowY + row.fontSize <= bodyHeight)
            drawText(row.text, row.fontSize, x + 20 + row.indentPixels, bodyY + rowY, panelTextColor(row.tone));
    }
    if (popoverScrollMax_ > 0)
    {
        const int thumb = std::max(24, bodyHeight * bodyHeight / layout.height);
        fill({x + width - 14, bodyY, 3, bodyHeight}, {177, 158, 120, 35}, 1);
        fill({x + width - 14, bodyY + (bodyHeight - thumb) * equipmentPopover_.scroll / popoverScrollMax_, 3, thumb}, gold, 1);
    }
    const auto hint = popoverScrollMax_ > 0 ? "滾輪／拖曳／Page Up、Down 捲動 · Esc 關閉"
        : equipmentPopover_.pinned ? "已固定 · 點外側或按 Esc 關閉" : "點選裝備可固定說明";
    drawText(hint, 14, x + 20, y + height - 30, muted);
}

void ChessPreparationScreen::drawActionFeedback()
{
    const bool confirming = !touchHint_.empty();
    if (!confirming && (notice_.empty() || Engine::getTicks() >= noticeUntil_)) return;
    PanelFrame anchor = noticeAnchor_;
    if (confirming)
    {
        const auto control = std::ranges::find(controls_, confirmation_.armedKey, &Control::key);
        if (control == controls_.end()) return; // 預覽重建後，原操作可能已不在這一頁。
        anchor = control->frame;
    }
    const auto& message = confirming ? touchHint_ : notice_;
    const int width = std::min(360, Font::getTextDrawSize(message) * 8 + 24);
    const auto rows = wrapDisplayText(message, (width - 24) / 8, true);
    const int height = static_cast<int>(rows.size()) * 20 + 16;
    const int x = std::clamp(anchor.x + (anchor.w - width) / 2, 16, w_ - width - 16);
    const int y = anchor.y - height - 8 >= 80 ? anchor.y - height - 8 : anchor.y + anchor.h + 8;
    // 回饋緊鄰來源操作；成功訊息短暫顯示，觸控二次確認則保留至操作結束。
    const double opacity = confirming ? 1.0 : std::clamp((noticeUntil_ - Engine::getTicks()) / 450.0, 0.0, 1.0);
    fill({x, y, width, height}, {29, 26, 23, static_cast<uint8_t>(246 * opacity)}, 5);
    const Color color{gold.r, gold.g, gold.b, static_cast<uint8_t>(255 * opacity)};
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        drawText(rows[i], 16, x + 12, y + 8 + i * 20, color);
}

void ChessPreparationScreen::drawTopmostOverlay()
{
    if (!isCurrentRunOwner() || !visible_) return;
    drawActionFeedback();
    if (guideStep_ >= 0) return;
    const auto pointer = PointerInput::instance().logicalPointerUiPosition();
    if (equipmentPopover_.pinned)
    {
        drawEquipmentPopover();
        return;
    }
    const bool touchPreview = touchInput_ && !keyboardFocus_;
    if (touchPreview && !pressPreview_.visible(Engine::getTicks())) return;
    if (equipmentPopover_.equipmentId >= 0 && contains(popoverFrame_, pointer) && !keyboardFocus_)
    {
        popoverLeaveSince_ = 0;
        drawEquipmentPopover();
        return;
    }
    int index = hit(pointer);
    if (keyboardFocus_) index = focusIndex();
    if (touchPreview)
    {
        const auto found = std::ranges::find(controls_, pressPreview_.key, &Control::key);
        index = found == controls_.end() ? -1 : static_cast<int>(found - controls_.begin());
    }
    const int peekId = index >= 0 ? controls_[index].equipmentId : -1;
    if (equipmentPopover_.equipmentId >= 0 && peekId != equipmentPopover_.equipmentId && !keyboardFocus_)
    {
        // 留一小段跨越浮窗間隙的時間，讓游標可移進說明閱讀／捲動。
        if (popoverLeaveSince_ == 0) popoverLeaveSince_ = Engine::getTicks();
        if (Engine::getTicks() - popoverLeaveSince_ < 240)
        {
            drawEquipmentPopover();
            return;
        }
    }
    else popoverLeaveSince_ = 0;
    if (peekId != dismissedPopoverId_) dismissedPopoverId_ = -1;
    if (index < 0)
    {
        hoverKey_.clear();
        equipmentPopover_.dismiss();
        popoverFrame_ = {};
        return;
    }
    const auto& control = controls_[index];
    if (hoverKey_ != control.key)
    {
        hoverKey_ = control.key;
        hoverSince_ = Engine::getTicks();
        equipmentPopover_.dismiss();
        popoverFrame_ = {};
    }
    if (peekId >= 0 && peekId != dismissedPopoverId_ && (touchPreview || Engine::getTicks() - hoverSince_ >= 175))
    {
        equipmentPopover_.preview(peekId);
        drawEquipmentPopover();
        return;
    }
    if ((control.tooltip.empty() && control.tooltipRows.empty()) || peekId >= 0) return;
    if (!touchPreview && Engine::getTicks() - hoverSince_ < 325) return;
    const auto rows = control.tooltipRows.empty() ? std::vector<PanelTextSourceRow>{{.text = control.tooltip}} : control.tooltipRows;
    int textWidth = 0;
    for (const auto& row : rows) textWidth = std::max(textWidth, Font::getTextDrawSize(row.text) * 8);
    const int width = std::clamp(textWidth + 24, 80, std::min(480, w_ - 40));
    const auto layout = layoutProse(rows, width - 24, 16);
    const int height = layout.height + 20;
    int x = std::clamp(control.frame.x, 20, w_ - width - 20);
    int y = control.tooltipBelow ? control.frame.y + control.frame.h + 8 : control.frame.y - height - 8;
    if (y < 8) y = control.frame.y + control.frame.h + 8;
    y = std::clamp(y, 8, std::max(8, h_ - height - 8));
    fill({x, y, width, height}, {29, 26, 23, 250}, 6);
    outline({x, y, width, height}, line, 6);
    for (const auto& row : layout.lines)
        drawText(row.text, row.fontSize, x + 12 + row.indentPixels, y + 10 + row.y, panelTextColor(row.tone));
}

} // namespace KysChess
