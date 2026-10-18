#include "ChessTalentUi.h"
#include "ChessPreparationScreen.h"

#include "ChessGameQueries.h"
#include "ChessMenuFormatting.h"
#include "ChessPresentationHelpers.h"
#include "ChessTalentUi.h"
#include "GameUtil.h"
#include "TextureManager.h"
#include "ChessUiStyle.h"
#include "ChessCharacterUi.h"

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
constexpr Color synergyActive = positive;

Color equipmentTextColor(PanelTextTone tone)
{
    switch (tone)
    {
    case PanelTextTone::Heading: return gold;
    case PanelTextTone::Effect: return {187, 213, 170, 255};
    case PanelTextTone::Exclusive: return {210, 182, 229, 255};
    case PanelTextTone::Inactive: return muted;
    case PanelTextTone::Combo:
    case PanelTextTone::Context: return equipmentContext;
    default: return ink;
    }
}

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
    for (const auto& part : wrapChessTalentDescription(value, 200))
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
    touchConfirmation_.reset();
    touchHint_.clear();
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
    ChessUiStyle::asset("chess-preparation", static_cast<int>(symbol),
        {frame.x, frame.y, frame.w, frame.h}, false, color);
}

void ChessPreparationScreen::icon(Icon symbol, PanelFrame frame, Color color)
{
    drawings_.push_back([=, this] { drawIcon(symbol, frame, color); });
}

void ChessPreparationScreen::synergyIcon(int iconId, PanelFrame frame, Color color)
{
    drawings_.push_back([=] {
        TextureManager::getInstance()->renderTexture(
            "chess-synergies", iconId, frame.x, frame.y, {color, color.a}, frame.w, frame.h);
    });
}

void ChessPreparationScreen::disclosure(std::string key, PanelFrame frame, std::string label, std::function<void()> action)
{
    button(std::move(key), frame, std::move(label), std::move(action), {}, true, false, Treatment::Row);
    icon(Icon::Right, {frame.x + frame.w - 28, frame.y + (frame.h - 16) / 2, 16, 16}, muted);
}

int ChessPreparationScreen::iconButton(std::string key, PanelFrame frame, Icon symbol, std::string label,
    std::function<void()> action, std::string tooltip, bool enabled, bool selected, Color color, Treatment treatment)
{
    const int index = button(std::move(key), frame, {}, std::move(action), std::move(tooltip), enabled, selected, treatment);
    drawings_.push_back([frame, symbol, label = std::move(label), enabled, color] {
        controlLabel({frame.x, frame.y, frame.w, frame.h}, label, 18,
            enabled ? color : muted, symbol, enabled ? color : muted);
    });
    return index;
}

int ChessPreparationScreen::button(std::string key, PanelFrame frame, std::string label,
    std::function<void()> action, std::string tooltip, bool enabled, bool selected, Treatment treatment, Color color,
    bool touchConfirmation, std::function<void()> preview)
{
    const int index = static_cast<int>(controls_.size());
    controls_.push_back({std::move(key), frame, std::move(tooltip), std::move(action), enabled, touchConfirmation, std::move(preview), {}});
    drawings_.push_back([=, this] {
        const auto& control = controls_[index];
        const auto pointer = PointerInput::instance().logicalPointerUiPosition();
        const bool hovered = !touchInput_ && contains(frame, pointer);
        const bool focused = keyboardFocus_ && focusKey_ == control.key;
        const bool armed = touchConfirmation_.armedKey == control.key;
        const bool highlighted = enabled && (hovered || armed || pressedKey_ == control.key);
        if (treatment != Treatment::Quiet || highlighted || selected || focused)
            controlSurface({frame.x, frame.y, frame.w, frame.h}, {
                .hovered = highlighted, .selected = selected, .focused = (focused || armed) && enabled,
                .disabled = !enabled, .primary = treatment == Treatment::Primary,
                .action = treatment == Treatment::Action});
        int size = Type::body;
        while (size > 12 && Font::getTextDrawSize(label) * size / 2 > frame.w - 12) --size;
        const auto labelColor = !enabled ? muted : treatment == Treatment::Primary ? darkLabel : color;
        drawText(label, size,
            frame.x + (treatment == Treatment::Row ? 8 : std::max(6, (frame.w - Font::getTextDrawSize(label) * size / 2) / 2)),
            frame.y + (frame.h - size) / 2, labelColor);
    });
    return index;
}

void ChessPreparationScreen::rebuild()
{
    ++layoutRevision_;
    equipmentPopover_.dismiss();
    popoverFrame_ = {};
    pressedKey_.clear();
    reconcileChessPreparationView(selection_, view_, session_.state());
    if (view_ == View::Replacement && (selection_.instanceId < 0
        || session_.state().roster.at(selection_.instanceId).deployed)) view_ = View::Piece;
    if (replacementId_ >= 0 && (!session_.state().roster.contains(replacementId_)
        || !session_.state().roster.at(replacementId_).deployed)) replacementId_ = -1;
    if (view_ == View::Sell && selection_.instanceId < 0) view_ = View::Piece;
    observation_ = session_.observe();
    w_ = Engine::getInstance()->getUIWidth();
    h_ = Engine::getInstance()->getUIHeight();
    controls_.clear();
    drawings_.clear();
    const int rightWidth = std::max(300, w_ * 28 / 100);
    constexpr int gutter = 8;
    constexpr int mainTop = 80;
    const int leftWidth = w_ - rightWidth - 32 - gutter;
    // 手機角落保留給 44 px/dp 的平台按鈕；沿用原有操作列，不另加工具列。
    const int footerHeight = GameUtil::isMobileDevice() ? 82 : 52;
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
    if (focusIndex() < 0 && !controls_.empty()) focusKey_ = controls_.front().key;
}

void ChessPreparationScreen::header()
{
    panel({16, 12, w_ - 32, 60});
    text("整備", 32, 30, 24, gold);
    button("progress", {102, 20, 128, 44}, std::format("{} / {} 關", observation_.fight + 1, session_.content().balance().totalFights),
        [this] { navigate(View::Information, -1, "progress"); }, "關卡進度");
    icon(Icon::Right, {218, 36, 12, 12}, muted);
    iconButton("economy", {242, 20, 100, 44}, Icon::Coin, std::to_string(observation_.money),
        [this] { navigate(View::Information, -1, "economy"); }, "查看金幣、利息與勝利收入", true, false, gold);
    icon(Icon::Right, {332, 36, 12, 12}, muted);
    button("level", {354, 20, 288, 44}, {},
        [this] { navigate(View::Information, -1, "level"); }, "查看經驗、修練與商店機率");
    icon(Icon::Cultivation, {362, 29, 26, 26});
    text(std::format("等級 {}", observation_.level + 1), 398, 31, 20, context);
    text(observation_.level < session_.content().balance().maxLevel
        ? std::format("經驗 {} / {}", observation_.experience, observation_.experienceForNextLevel)
        : "已滿級", 492, 34, 16, positive);
    icon(Icon::Right, {628, 36, 12, 12}, muted);
    const int x = w_ - 620;
    iconButton("difficulty", {x, 20, 112, 44}, Icon::Mountain, chessDifficultyDisplayName(observation_.difficulty),
        [this] { navigate(View::Information, -1, "difficulty"); }, "查看本局難度", true, false, observation_.difficulty == Difficulty::Hard ? negative : observation_.difficulty == Difficulty::Easy ? positive : gold);
    iconButton("talent", {x + 124, 20, 156, 44}, Icon::Spark, observation_.talentName,
        [this] { navigate(View::Information, -1, "talent"); }, "查看天賦說明與效果");
    iconButton("neigongs", {x + 292, 20, 84, 44}, Icon::Book, std::to_string(observation_.obtainedNeigongIds.size()),
        [this] { navigate(View::Neigongs); }, "已獲得內功與效果");
    iconButton("collapse", {x + 388, 20, 104, 44}, Icon::Collapse, "收起",
        [this] { setExit(true); }, "收起整備，欣賞擂鼓山；Q 再次開啟");
    iconButton("system", {x + 504, 20, 84, 44}, Icon::Sliders, "系統",
        [this] { open(ChessContextMenuAction::OpenSystemMenu); }, "系統選單");
}

void ChessPreparationScreen::pieceCard(const ChessSessionPiece& piece, PanelFrame frame, bool compact)
{
    const auto* role = session_.content().role(piece.roleId);
    assert(role);
    const Color tier = chessPieceTierColor(role->Cost);
    const bool selected = selection_.instanceId == piece.instanceId || replacementId_ == piece.instanceId;
    button(std::format("piece:{}", piece.instanceId), frame, {},
        [this, id = piece.instanceId] { selectPiece(id); },
        compact ? "" : std::format("{} {} · {}階 · 出戰；雙擊下陣", role->Name, chessStars(piece.star), role->Cost),
        true, selected, Treatment::Card);
    controls_.back().shortcut = [this, id = piece.instanceId] {
        if (view_ == View::Replacement) return;
        selection_.selectPiece(id);
    characterInspector_ = {}; deploy();
    };
    drawings_.push_back([=, this] {
        fill({frame.x + 1, frame.y + 3, 3, frame.h - 6}, tier);
        if (Engine::getTicks() < arrivalUntil_ && std::ranges::contains(arrivalIds_, piece.instanceId))
        {
            const uint8_t alpha = static_cast<uint8_t>(40 + 36 * (1 + std::sin(Engine::getTicks() / 110)));
            Color arrival = arrivalMerged_ ? Color{255, 215, 0, alpha} : Color{100, 200, 255, alpha};
            fill({frame.x + 5, frame.y + 2, frame.w - 7, frame.h - 4}, arrival, 4);
        }
    });
    const int portraitSize = compact ? std::min(52, frame.h - 8) : std::min(64, frame.h - 8);
    const int portraitY = frame.y + (frame.h - portraitSize) / 2;
    image("head", role->HeadID, {frame.x + 6, portraitY, portraitSize, portraitSize});
    if (!compact)
    {
        text(role->Name, frame.x + portraitSize + 10, portraitY + 6, 16, ink, frame.w - portraitSize - 16);
        // 星級留在肖像下緣，裝備固定在右下角，三星與雙裝備也不共用空間。
        const PanelFrame stars{frame.x + 6, portraitY + portraitSize - 16, portraitSize, 18};
        drawings_.push_back([stars] { fill(stars, {25, 23, 22, 190}, 3); });
        text(chessStars(piece.star), stars.x + (stars.w - piece.star * 14) / 2, stars.y + 1, 14, gold);
    }
    else
    {
        text(chessStars(piece.star), frame.x + 5, frame.y + 2, 12, gold);
        if (benchNames_) text(role->Name, frame.x + 6, frame.y + frame.h - 20, 12, ink, frame.w - 14);
    }
    int itemX = frame.x + frame.w - 28;
    for (int equipment : {piece.armorInstanceId, piece.weaponInstanceId})
    {
        if (equipment < 0) continue;
        const auto& item = session_.state().equipmentInventory.at(equipment);
        const PanelFrame itemFrame{itemX, compact ? frame.y + frame.h - 28 : portraitY + portraitSize - 28, 26, 26};
        button(std::format("piece-gear:{}:{}", piece.instanceId, equipment), itemFrame, {},
            [this, id = piece.instanceId, equipment] {
                navigate(View::Equipment); selection_.selectPiece(id);
                equipmentId_ = equipment;
                equipmentType_ = equipmentDefinition(session_.content(), session_.state().equipmentInventory.at(equipment).itemId)->equipType;
                rebuild();
                equipmentPopover_.pin(equipment);
            }, session_.content().item(item.itemId)->name);
        controls_.back().equipmentId = equipment;
        image("item", session_.content().item(item.itemId)->id,
            {itemFrame.x + 2, itemFrame.y + 2, 22, 22});
        itemX -= 28;
    }
}

void ChessPreparationScreen::roster()
{
    panel(team_);
    panel(bench_);
    const int deployed = static_cast<int>(std::ranges::count(observation_.roster, true, &ChessSessionPiece::deployed));
    text(std::format("出戰  {}/{}", deployed, observation_.maximumDeployment), team_.x + 12, team_.y + 14, 20, gold);
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
        button(std::format("combo-chip:{}", combo.id), chip, {},
            [this, id = combo.id] { showChessComboOverview(session_.content(), session_.state(), id); rebuild(); }, combo.name + " · 已啟動",
            true, false, Treatment::Quiet);
        drawings_.push_back([chip] {
            fill({chip.x, chip.y + 3, chip.w, chip.h - 6}, {111, 132, 88, 38}, 3);
            fill({chip.x + 29, chip.y + 29, 5, 5}, synergyActive, 2);
        });
        synergyIcon(combo.iconId, {chip.x, chip.y + 2, 36, 36}, synergyActive);
        const auto count = progress.isAntiCombo ? std::format("獨/{}", progress.displayTargetCount)
            : std::format("{}/{}", progress.effectiveCount, progress.displayTargetCount);
        text(count, chip.x + 38, chip.y + 13, 14, synergyActive);
    }
    iconButton("all-combos", {team_.x + 8, team_.y + 44, 150, 40}, Icon::Link,
        std::format("啟動 {} ›", combos.size()),
        [this] { showChessComboOverview(session_.content(), session_.state()); rebuild(); }, "查看全部羈絆與門檻，包含尚未啟動的羈絆");
    if (combos.empty()) text("尚無羈絆啟動", team_.x + 186, team_.y + 18, 16, muted);
    const int gap = 8;
    const int width = (team_.w - 24 - gap * 4) / 5;
    const int top = team_.y + 88;
    const int slots = observation_.maximumDeployment;
    const auto visibleFormation = chessPreparationVisibleFormation(observation_.formationSlots, slots);
    const bool twoRows = slots > 5;
    const int height = (team_.h - 100 - (twoRows ? gap : 0)) / (twoRows ? 2 : 1);
    for (int slot = 0; slot < slots; ++slot)
    {
        const PanelFrame frame{team_.x + 12 + (slot % 5) * (width + gap), top + (slot / 5) * (height + gap), width, height};
        const int id = visibleFormation[slot];
        if (id >= 0) pieceCard(session_.state().roster.at(id), frame, false);
        else drawings_.push_back([frame] { fill(frame, {12, 11, 10, 32}, 4); });
    }
    if (slots < kChessFormationSlotCount)
    {
        PanelFrame frame{team_.x + 12 + (slots % 5) * (width + gap), top + (slots / 5) * (height + gap), width, height};
        // 五人時下一格放在標題內，避免製造空的第二排。
        if (slots == 5) frame = {team_.x + 126, team_.y + 4, 40, 40};
        iconButton("next-slot", frame, Icon::Lock, "",
            [this] { navigate(View::Information, -1, "level"); }, "查看下一個出戰位置與升級條件");
    }
    std::vector<const ChessSessionPiece*> bench;
    for (const auto& piece : observation_.roster) if (!piece.deployed) bench.push_back(&piece);
    const int pages = std::max(1, (static_cast<int>(bench.size()) + 9) / 10);
    benchPage_ = std::clamp(benchPage_, 0, pages - 1);
    text(std::format("候補  {}/{}", bench.size(), session_.content().balance().benchSize), bench_.x + 12, bench_.y + 14, 18, gold);
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
        if (index < static_cast<int>(bench.size())) pieceCard(*bench[index], frame, true);
        else drawings_.push_back([frame] { fill(frame, {12, 11, 10, 24}, 4); });
    }
}

void ChessPreparationScreen::shop()
{
    panel(shop_);
    text(banManagement_ ? "封棋" : "商店", shop_.x + 12, shop_.y + 16, 20, gold);
    iconButton("shop-bans", {shop_.x + shop_.w - 344, shop_.y + 6, 46, 44}, banManagement_ ? Icon::Left : Icon::Seal, "",
        [this] { banManagement_ = !banManagement_; portraitClicks_.reset(); rebuild(); },
        banManagement_ ? "返回商店；保留候選與鎖定" : "管理已出現的棋子封印", true, banManagement_);
    if (banManagement_) { bans(); return; }
    iconButton("auto-deploy", {shop_.x + shop_.w - 286, shop_.y + 6, 46, 44}, Icon::Up, "",
        [this] { automaticDeployment_ = !automaticDeployment_; rebuild(); },
        "購買後自動上陣", true, automaticDeployment_);
    iconButton("refresh", {shop_.x + shop_.w - 228, shop_.y + 6, 140, 44}, Icon::Refresh,
        observation_.freeShopRefreshAvailable ? "免費" : std::to_string(session_.content().balance().refreshCost),
        [this] { const auto result = submit(simpleAction(ChessActionType::RefreshShop), "商店已刷新");
            if (result.accepted && selection_.shopSlot >= 0) { selection_ = {}; rebuild(); } }, "刷新商店");
    controls_.back().touchConfirmation = true;
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
        const Color tier = chessPieceTierColor(role->Cost);
        button(std::format("offer:{}", slot), {frame.x, frame.y, frame.w, frame.h - 40}, {},
            [this, slot] { selectShop(slot); },
            std::format("{} · {}階；雙擊購買", role->Name, offer.tier),
            true, selection_.shopSlot == slot, Treatment::Card);
        controls_.back().key = std::format("offer:{}:{}", slot, offer.roleId);
        controls_.back().preview = [this, slot] { if (selection_.shopSlot != slot) selectShop(slot); };
        controls_.back().shortcut = [this, slot] { buy(slot); };
        image("head", role->HeadID, {frame.x + 12, frame.y + 2, frame.w - 24, frame.h - 66});
        const int nameWidth = std::min(frame.w - 16, Font::getTextDrawSize(role->Name) * 8);
        text(role->Name, frame.x + (frame.w - nameWidth) / 2, frame.y + frame.h - 62, 16, tier, nameWidth);
        if (emphasis == ChessShopRowEmphasis::Merge)
            icon(Icon::Up, {frame.x + frame.w - 24, frame.y + 5, 18, 18}, gold);
        const std::string reason = analysis.purchaseError == ChessRuleErrorCode::InsufficientGold ? "金錢不足"
            : analysis.purchaseError == ChessRuleErrorCode::BenchFull ? "候補已滿" : "購買";
        const bool affordable = analysis.purchaseError == ChessRuleErrorCode::None;
        button(std::format("buy:{}", slot), {frame.x, frame.y + frame.h - 40, frame.w, 40},
            {}, [this, slot] { buy(slot); },
            reason == "購買" ? "購買此棋子" : reason,
            affordable, false, Treatment::Quiet, gold, true, [this, slot] { selectShop(slot); });
        const auto price = std::to_string(analysis.goldCost);
        const int priceX = frame.x + (frame.w - 26 - Font::getTextDrawSize(price) * 9) / 2;
        icon(Icon::Coin, {priceX, frame.y + frame.h - 30, 20, 20}, affordable ? gold : muted);
        text(price, priceX + 26, frame.y + frame.h - 29, 18, affordable ? gold : muted);
        drawings_.push_back([=] {
            auto* engine = Engine::getInstance();
            // 沿用舊選單的短光點與兩秒週期；不再疊加常亮外框。
            // dotLength 是周長比例，使用共用預設值，不能傳入像素長度。
            if (emphasisStyle.animated)
            {
                for (int inset = 0; inset < emphasisStyle.thickness; ++inset)
                    engine->drawAnimatedRoundedRect(emphasisStyle.color, frame.x + inset, frame.y + inset,
                        frame.w - 2 * inset, frame.h - 2 * inset, std::max(0, 4 - inset),
                        std::fmod(Engine::getTicks() * 0.0005, 1.0));
            }
            fill({frame.x + 1, frame.y + 3, 3, frame.h - 6}, tier);
        });
    }
}

void ChessPreparationScreen::bans()
{
    const auto rows = buildChessBanManagementRows(observation_, session_.content());
    const int pages = std::max(1, (static_cast<int>(rows.size()) + 5) / 6);
    banPage_ = std::clamp(banPage_, 0, pages - 1);
    text(std::format("{} / {}", observation_.bans.size(), observation_.maximumBanCount), shop_.x + 80, shop_.y + 18, 16, muted);
    iconButton("ban-prev", {shop_.x + shop_.w - 228, shop_.y + 6, 46, 44}, Icon::Left, "",
        [this] { --banPage_; rebuild(); }, "上一頁封棋", banPage_ > 0);
    text(std::format("{} / {}", banPage_ + 1, pages), shop_.x + shop_.w - 168, shop_.y + 20, 16, muted);
    iconButton("ban-next", {shop_.x + shop_.w - 76, shop_.y + 6, 64, 44}, Icon::Right, "",
        [this] { ++banPage_; rebuild(); }, "下一頁封棋", banPage_ + 1 < pages);
    if (rows.empty()) text("尚未見過可封棋的角色", shop_.x + 12, shop_.y + 88, 18, muted);
    const int width = (shop_.w - 64) / 6;
    for (int column = 0; column < 6; ++column)
    {
        const int index = banPage_ * 6 + column;
        if (index >= static_cast<int>(rows.size())) break;
        const auto row = rows[index];
        const auto* role = session_.content().role(row.roleId);
        auto action = simpleAction(ChessActionType::AddBan); action.roleId = row.roleId;
        const bool eligible = ChessManagementRules::validate(session_.state(), session_.content(), action) == ChessRuleErrorCode::None;
        PanelFrame frame{shop_.x + 12 + column * (width + 8), shop_.y + 58, width, shop_.h - 70};
        button(std::format("ban:{}", row.roleId), frame, {}, [this, action] { submit(action, "已封棋；影響後續生成的商店"); },
            row.banned ? role->Name + " · 已封棋（本局永久）" : role->Name + " · 封棋；影響後續刷新，保留當輪商店",
            eligible, row.banned, Treatment::Card, ink, true);
        image("head", role->HeadID, {frame.x + 12, frame.y + 4, frame.w - 24, frame.h - 42});
        if (row.banned)
        {
            drawings_.push_back([frame] { fill({frame.x + 4, frame.y + 4, frame.w - 8, frame.h - 40}, {25,23,22,145}); });
            icon(Icon::Seal, {frame.x + frame.w - 30, frame.y + 8, 24, 24});
        }
        text(role->Name, frame.x + 8, frame.y + frame.h - 30, 16, ink, frame.w - 16);
        drawings_.push_back([frame, tier = chessPieceTierColor(role->Cost)] { fill({frame.x + 1, frame.y + 4, 3, frame.h - 8}, tier); });
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
    case View::Attributes:
    case View::Ability:
    case View::Neigongs:
    case View::Neigong: expandedInspector(); break;
    case View::Replacement: replacementInspector(); break;
    case View::Sell:
    {
        const auto& piece = session_.state().roster.at(selection_.instanceId);
        const auto* role = session_.content().role(piece.roleId);
        text("確認出售", inspector_.x + 12, inspector_.y + 20, 22, gold);
        image("head", role->HeadID, {inspector_.x + 18, inspector_.y + 76, 96, 96});
        text(role->Name, inspector_.x + 126, inspector_.y + 100, 22, ink, inspector_.w - 142, 2);
        prose(paragraph(std::format("出售 {} {}，獲得 {} 金。持有的裝備會依現有規則回到行囊。",
            role->Name, chessStars(piece.star), ChessManagementRules::pieceValue(session_.content(), piece.roleId, piece.star))),
            {inspector_.x + 14, inspector_.y + 190, inspector_.w - 28, inspector_.h - 260});
        button("confirm-sell", {inspector_.x + 12, inspector_.y + inspector_.h - 64, inspector_.w - 24, 52}, "確認出售",
            [this, id = piece.instanceId] { auto action = simpleAction(ChessActionType::SellChess); action.chessInstanceId = id;
                view_ = View::Piece; history_.clear(); submit(action, "棋子已出售，裝備已回到行囊"); }, {}, true, false, Treatment::Action);
        break;
    }
    }
}

void ChessPreparationScreen::pieceInspector()
{
    const auto* piece = rosterPiece(session_.state(), selection_.instanceId);
    const int roleId = piece ? piece->roleId : selection_.shopRoleId;
    const auto* role = roleId >= 0 ? session_.content().role(roleId) : nullptr;
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    if (!role)
    {
        text("檢視器", x, y + 4, 22, gold);
        prose(paragraph("選擇出戰、候補或商店的棋子，查看屬性、武學與羈絆。"), {x, y + 64, width, 160});
        iconButton("empty-equipment", {x, y + 250, width, 52}, Icon::Bag, "裝備行囊", [this] { navigate(View::Equipment); });
        iconButton("empty-combos", {x, y + 314, width, 52}, Icon::Link, "羈絆一覽", [this] { showChessComboOverview(session_.content(), session_.state()); rebuild(); });
        return;
    }
    const auto character = std::make_shared<ChessCharacterPresentation>(buildChessCharacterPresentation(
        session_.content(), session_.state(), roleId, piece ? piece->star : 1,
        piece ? piece->instanceId : -1, piece ? ChessCharacterContext::Owned : ChessCharacterContext::Shop));
    auto content = composeChessCharacterInspector(*character,
        {inspector_.x, inspector_.y, inspector_.w, inspector_.h - 64}, characterInspector_, false);
    for (int i = 0; i < static_cast<int>(content.controls.size()); ++i)
    {
        const auto control = content.controls[i];
        const PanelFrame area{control.frame.x, control.frame.y, control.frame.w, control.frame.h};
        const auto activate = [this, control] {
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
        button("deployment", {x, bottom, width - 102, 52}, piece->deployed ? "下陣" : "上陣／替換", [this] { deploy(); }, {}, true, false, Treatment::Action);
        button("sell", {x + width - 90, bottom, 90, 52}, "出售", [this] { navigate(View::Sell); });
    }
    else
    {
        const auto analysis = queryChessShopSlot(session_.state(), session_.content(), selection_.shopSlot);
        button("inspector-buy", {x, bottom, width, 52}, std::format("購買  {} 金", analysis.goldCost),
            [this, slot = selection_.shopSlot] { buy(slot); }, "購買此棋子", analysis.purchaseError == ChessRuleErrorCode::None, false, Treatment::Action, ink, true);
    }
}

void ChessPreparationScreen::prose(const std::vector<PanelTextSourceRow>& rows, PanelFrame frame)
{
    const auto layout = layoutProse(rows, frame.w, 17);
    const int pageHeight = std::max(24, frame.h - 46);
    // 依完整行分頁，不裁斷長裝備效果，也不把字縮到無法閱讀。
    std::vector<std::vector<PanelTextPhysicalLine>> pages(1);
    int origin = 0;
    for (auto line : layout.lines)
    {
        if (!pages.back().empty() && line.y + line.fontSize - origin > pageHeight)
        {
            pages.emplace_back(); origin = line.y;
        }
        line.y -= origin;
        pages.back().push_back(std::move(line));
    }
    textPage_ = std::clamp(textPage_, 0, static_cast<int>(pages.size()) - 1);
    for (const auto& row : pages[textPage_]) text(row.text, frame.x + row.indentPixels, frame.y + row.y, row.fontSize);
    if (pages.size() > 1)
    {
        button("text-prev", {frame.x, frame.y + frame.h - 44, 64, 44}, "<", [this] { --textPage_; rebuild(); }, {}, textPage_ > 0);
        text(std::format("{}/{}", textPage_ + 1, pages.size()), frame.x + 78, frame.y + frame.h - 30, 16, muted);
        button("text-next", {frame.x + frame.w - 64, frame.y + frame.h - 44, 64, 44}, ">", [this] { ++textPage_; rebuild(); }, {}, textPage_ + 1 < static_cast<int>(pages.size()));
    }
}

void ChessPreparationScreen::equipmentInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    text("裝備", x, y + 8, 22, gold);
    iconButton("equipment-catalog", {x + width - 144, y - 4, 52, 44}, Icon::More, "",
        [this] { open(ChessContextMenuAction::OpenEquipmentMenu); }, "裝備圖鑑／神兵商店");
    const auto* target = rosterPiece(session_.state(), selection_.instanceId);
    if (target)
    {
        const auto* role = session_.content().role(target->roleId);
        image("head", role->HeadID, {x, y + 40, 40, 44});
        text(role->Name, x + 48, y + 54, 18, ink, width - 52);
    }
    else text("點選出戰或候補棋子，選擇持有人", x, y + 52, 16, muted, width);
    for (int type = 0; type < 2; ++type)
        button(std::format("gear-filter:{}", type), {x + type * (width + 8) / 2, y + 88, (width - 8) / 2, 44},
            chessEquipmentTypeName(type), [this, type] { equipmentType_ = type; equipmentId_ = -1; listPage_ = 0; textPage_ = 0; rebuild(); }, {}, true, equipmentType_ == type);
    std::vector<const ChessEquipmentInstance*> inventory;
    for (const auto& [id, equipment] : session_.state().equipmentInventory)
        if (equipmentDefinition(session_.content(), equipment.itemId)->equipType == equipmentType_) inventory.push_back(&equipment);
    std::stable_sort(inventory.begin(), inventory.end(), [&](const auto* left, const auto* right) {
        return chessEquipmentMenuOrderLess({true, equipmentDefinition(session_.content(), left->itemId)->tier, left->itemId},
            {true, equipmentDefinition(session_.content(), right->itemId)->tier, right->itemId});
    });
    constexpr int columns = 4;
    constexpr int pageSize = columns * 3;
    constexpr int gap = 6;
    constexpr int cellHeight = 60;
    const int cellWidth = (width - (columns - 1) * gap) / columns;
    const int pages = std::max(1, (static_cast<int>(inventory.size()) + pageSize - 1) / pageSize);
    listPage_ = std::clamp(listPage_, 0, pages - 1);
    const auto selected = std::ranges::find(inventory, equipmentId_, &ChessEquipmentInstance::instanceId);
    if (selected != inventory.end()) listPage_ = static_cast<int>(selected - inventory.begin()) / pageSize;
    else equipmentId_ = inventory.empty() ? -1 : inventory[listPage_ * pageSize]->instanceId;
    if (inventory.empty()) text("尚無此類裝備", x, y + 156, 18, muted);
    for (int slot = 0; slot < pageSize; ++slot)
    {
        const int index = listPage_ * pageSize + slot;
        if (index >= static_cast<int>(inventory.size())) break;
        const auto& equipment = *inventory[index];
        const auto* item = session_.content().item(equipment.itemId);
        const auto* definition = equipmentDefinition(session_.content(), equipment.itemId);
        PanelFrame frame{x + (slot % columns) * (cellWidth + gap), y + 140 + (slot / columns) * (cellHeight + gap), cellWidth, cellHeight};
        button(std::format("equipment:{}", equipment.instanceId), frame, {}, [this, id = equipment.instanceId] {
            equipmentId_ = id; textPage_ = 0; rebuild(); equipmentPopover_.pin(id);
        }, item->name, true, equipmentId_ == equipment.instanceId, Treatment::Slot);
        controls_.back().equipmentId = equipment.instanceId;
        image("item", item->id, {frame.x + 8, frame.y + 6, frame.w - 16, 46});
        drawings_.push_back([frame, tier = definition->tier] {
            fill({frame.x + 2, frame.y + 6, 2, frame.h - 12}, chessRewardTierColor(tier));
        });
        if (equipment.assignedChessInstanceId >= 0)
            text("裝", frame.x + frame.w - 19, frame.y + 5, 14, equipmentContext);
    }
    if (pages > 1)
    {
        text(std::format("{}–{} / {} 件", listPage_ * pageSize + 1,
            std::min((listPage_ + 1) * pageSize, static_cast<int>(inventory.size())), inventory.size()), x, y + 350, 15, muted);
        iconButton("gear-prev", {x + width - 96, y + 336, 44, 44}, Icon::Left, "",
            [this] { --listPage_; equipmentId_ = -1; rebuild(); }, "上一頁裝備", listPage_ > 0);
        iconButton("gear-next", {x + width - 44, y + 336, 44, 44}, Icon::Right, "",
            [this] { ++listPage_; equipmentId_ = -1; rebuild(); }, "下一頁裝備", listPage_ + 1 < pages);
    }
    else if (!inventory.empty())
        text(std::format("{} 件 · {}", inventory.size(), touchInput_ ? "點選裝備查看效果" : "滑過查看，點選固定"), x, y + 350, 15, muted, width);
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
            text(role->Name, x + 24, detailY + 32, 15, muted, width - 60);
        }
        else
        {
            icon(Icon::Bag, {x, detailY + 32, 16, 16}, muted);
            text("行囊", x + 24, detailY + 32, 15, muted);
        }
        if (target)
        {
            const int oldId = equipmentType_ == 0 ? target->weaponInstanceId : target->armorInstanceId;
            if (oldId >= 0 && oldId != equipmentId_)
                text("替下：" + chessItemDisplayName(session_.content(), session_.state().equipmentInventory.at(oldId).itemId),
                    x, detailY + 64, 15, muted, width);
        }
        const bool equipped = wearer && target && wearer->instanceId == target->instanceId;
        button("equip", {x, bottom, width, 52}, equipped ? "已裝備" : wearer ? "轉交" : "裝備",
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
        text("目前金幣", x, y + 64, 16, muted);
        icon(Icon::Coin, {x, y + 90, 28, 28}, gold);
        text(std::to_string(observation_.money), x + 40, y + 86, 32, gold);
        divider(y + 134);
        text("利息", x, y + 154, 20, gold);
        fact("本輪利息", std::format("+{} 金", observation_.interestGold), y + 190, gold);
        fact("計算比例", std::format("{}% · 上限 {} 金", cfg.interestPercent, observation_.maximumInterestGold), y + 226, muted);
        if (observation_.nextInterestThreshold)
        {
            fact("下一檔存款", std::format("{} 金", *observation_.nextInterestThreshold), y + 262, gold);
            fact("還需保留", std::format("{} 金", *observation_.nextInterestThreshold - observation_.money), y + 298, context);
        }
        else text(cfg.interestPercent > 0 ? "已達利息上限" : "本難度不計利息", x, y + 266, 18, muted, width);
        divider(y + 336);
        text("預計勝利收入", x, y + 356, 20, gold);
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
        fact("出戰上限", std::format("{} 名", observation_.maximumDeployment), y + 156, positive);
        if (canLevel)
        {
            const auto deployment = buildChessBuyExpDeploymentPreview(session_.state(), session_.content());
            fact("下一級出戰", std::format("{} 名", deployment.next.value_or(deployment.current)), y + 190, positive);
        }
        text("商店階級機率", x, y + 238, 20, gold);
        text("階級", x, y + 276, 16, muted);
        text("目前", x + 112, y + 276, 16, muted);
        text(canLevel ? "下一級" : "已滿級", x + 222, y + 276, 16, muted);
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
        const int height = inspector_.h - 150;
        const auto pages = paginateChessTalentFacts(presentation, width, height, 18);
        textPage_ = std::clamp(textPage_, 0, static_cast<int>(pages.size()) - 1);
        drawings_.push_back([=, page = pages[textPage_]] { drawChessTalentFactTable(page, x, y + 64, width, height, 18, 18); });
        const int bottom = inspector_.y + inspector_.h - 60;
        iconButton("talent-prev", {x, bottom, 44, 44}, Icon::Up, "", [this] { --textPage_; rebuild(); }, {}, textPage_ > 0);
        text(std::format("{} / {}", textPage_ + 1, pages.size()), x + 64, bottom + 12, 18, muted);
        iconButton("talent-next", {x + width - 44, bottom, 44, 44}, Icon::Down, "", [this] { ++textPage_; rebuild(); }, {}, textPage_ + 1 < static_cast<int>(pages.size()));
        return;
    }
    else
    {
        title = information_ == "progress" ? "關卡進度" : "難度";
        rows = paragraph(std::format("{}\n第 {} / {} 關\n出戰上限 {} 名\n候補容量 {} 名", chessDifficultyDisplayName(observation_.difficulty),
            observation_.fight + 1, cfg.totalFights, observation_.maximumDeployment, cfg.benchSize));
    }
    text(title, x, y + 8, 22, gold, width - 88, 2);
    prose(rows, {x, y + 76, width, inspector_.h - 160});
}

void ChessPreparationScreen::expandedInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    std::vector<PanelTextSourceRow> rows;
    if (view_ == View::Neigongs)
    {
        text("已獲得內功", x, y + 8, 22, gold);
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
    if (view_ == View::Neigong)
    {
        const auto ng = std::ranges::find(session_.content().neigong(), neigongId_, &NeigongDef::magicId);
        assert(ng != session_.content().neigong().end());
        text("內功詳情", x, y + 8, 22, gold);
        image("item", ng->itemId, {x, y + 64, 64, 64});
        text(ng->name, x + 78, y + 80, 22, chessRewardTierColor(ng->tier), width - 82, 2);
        rows = panelTextRowsForEffectDescription(describeGameplayEffects(ng->effects, EffectDescriptionStyle::Full));
        rows.insert(rows.begin(), {.text = "對所有成員自動生效", .spacingAfter = 12});
        prose(rows, {x, y + 152, width, inspector_.h - 180});
        return;
    }
    const auto* piece = rosterPiece(session_.state(), selection_.instanceId);
    const auto* role = session_.content().role(piece ? piece->roleId : selection_.shopRoleId);
    assert(role);
    const int star = piece ? piece->star : 1;
    text(view_ == View::Attributes ? "完整屬性" : "武學詳情", x, y + 8, 22, gold);
    image("head", role->HeadID, {x, y + 60, 64, 64});
    text(role->Name + " " + chessStars(star), x + 78, y + 78, 20, ink, width - 82, 2);
    if (view_ == View::Attributes)
    {
        const auto stats = piece ? chessPieceStats(session_.content(), *piece,
            session_.content().balance().talent(observation_.talent).amplifiedGrowthPercent)
            : chessRoleStats(*role, session_.content().balance(), star, 0);
        rows = paragraph(std::format("生命  {}\n內力  {}\n攻擊  {}\n防禦  {}\n輕功  {}\n拳掌  {}\n御劍  {}\n耍刀  {}\n特殊  {}\n暗器  {}\n勝場  {}",
            stats.maxHp, stats.maxMp, stats.attack, stats.defence, stats.speed, stats.fist, stats.sword, stats.knife, stats.unusual, stats.hiddenWeapon,
            piece ? piece->fightsWon : 0));
    }
    else
    {
        const auto abilities = chessAbilitiesForRoleStar(session_.content(), *role, star, EffectDescriptionStyle::Full);
        if (abilityId_ < 0)
        {
            const int pages = std::max(1, (static_cast<int>(abilities.size()) + 5) / 6);
            listPage_ = std::clamp(listPage_, 0, pages - 1);
            for (int i = 0; i < 6 && listPage_ * 6 + i < static_cast<int>(abilities.size()); ++i)
            {
                const auto& ability = abilities[listPage_ * 6 + i];
                disclosure(std::format("all-ability:{}", ability.magicId), {x, y + 152 + i * 48, width, 44}, ability.name,
                    [this, id = ability.magicId] { navigate(View::Ability, id); });
            }
            button("ability-page", {x, inspector_.y + inspector_.h - 60, width, 44}, std::format("{} / {}  下一頁", listPage_ + 1, pages),
                [this, pages] { listPage_ = (listPage_ + 1) % pages; rebuild(); }, {}, pages > 1);
            return;
        }
        const auto ability = std::ranges::find(abilities, abilityId_, &ChessAbilityMetadata::magicId);
        assert(ability != abilities.end());
        rows = paragraph(std::format("{}\n威力 {} · 內力消耗 {}\n範圍：{}", ability->name, ability->powerByStar.front().power,
            ability->mpCost, ability->geometry));
        auto effects = panelTextRowsForEffectDescription(ability->effects);
        rows.insert(rows.end(), effects.begin(), effects.end());
        if (!ability->effectNote.empty()) rows.push_back({.text = ability->effectNote});
    }
    prose(rows, {x, y + 152, width, inspector_.h - 180});
}

void ChessPreparationScreen::replacementInspector()
{
    const int x = inspector_.x + 12, y = inspector_.y + 12, width = inspector_.w - 24;
    text("上陣預覽", x, y + 8, 22, gold);
    const auto& incoming = session_.state().roster.at(selection_.instanceId);
    const auto* role = session_.content().role(incoming.roleId);
    image("head", role->HeadID, {x, y + 58, 64, 64});
    text("上陣 " + role->Name, x + 76, y + 72, 20, ink, width - 80, 2);
    const auto* outgoing = rosterPiece(session_.state(), replacementId_);
    if (outgoing)
    {
        const auto* oldRole = session_.content().role(outgoing->roleId);
        text("候補 " + oldRole->Name, x, y + 138, 20, ink, width);
        const auto changes = chessPreparationReplacementPreview(session_.state(), session_.content(), incoming.instanceId, outgoing->instanceId);
        std::vector<PanelTextSourceRow> rows;
        for (const auto& change : changes)
        {
            rows.push_back({.text = std::format("{}：{} → {}{}", change.name,
                formatChessComboProgressCount(change.before), formatChessComboProgressCount(change.after),
                change.before.activeThresholdIndex == change.after.activeThresholdIndex ? "" : "（門檻改變）"), .spacingAfter = 10});
        }
        if (rows.empty()) rows.push_back({.text = "羈絆數值與門檻不變"});
        prose(rows, {x, y + 178, width, inspector_.h - 266});
    }
    else prose(paragraph("點選左側出戰棋子，預覽替換後的羈絆變化。確認前不改變陣容。"),
        {x, y + 142, width, 170});
    button("confirm-replacement", {x, inspector_.y + inspector_.h - 64, width, 52}, "確認替換",
        [this] { const auto action = chessPreparationDeployment(session_.state(), selection_.instanceId, replacementId_);
            view_ = View::Piece; history_.clear(); submit(action, "已替換出戰棋子"); }, {}, outgoing != nullptr, false, Treatment::Action);
}

void ChessPreparationScreen::footer()
{
    const int y = footer_.y + (footer_.h - 52) / 2;
    panel(footer_);
    iconButton("equipment", {112, y + 4, 100, 44}, Icon::Bag, "裝備", [this] { navigate(View::Equipment); });
    button("overview", {220, y + 4, 100, 44}, "棋局圖鑑", [this] { open(ChessContextMenuAction::OpenOverviewMenu); });
    button("expedition", {328, y + 4, 72, 44}, "遠征", [this] { open(ChessContextMenuAction::ShowExpeditionChallenge); });
    button("offline", {408, y + 4, 108, 44}, "離線對戰", [this] { open(ChessContextMenuAction::OpenOfflineBattle); });
    iconButton("save", {552, y + 4, 100, 44}, Icon::Save, "存檔", [this] { open(ChessContextMenuAction::SaveProgress); });
    iconButton("load", {660, y + 4, 100, 44}, Icon::Load, "讀檔", [this] { open(ChessContextMenuAction::LoadProgress); });
    button("battle", {w_ - 304, y + 4, 176, 44}, observation_.campaignComplete ? "已通關 · 遠征" : "進入戰鬥",
        [this] { open(observation_.campaignComplete ? ChessContextMenuAction::ShowExpeditionChallenge : ChessContextMenuAction::EnterBattle); },
        "使用目前陣容出戰", true, false, Treatment::Primary);
}

void ChessPreparationScreen::navigate(View view, int detailId, std::string information)
{
    if (view_ == view && view != View::Ability && view != View::Neigong
        && (view != View::Information || information_ == information)) { rebuild(); return; }
    history_.push({view_, listPage_, textPage_, focusKey_, selection_, equipmentId_, equipmentType_, abilityId_, neigongId_, information_});
    view_ = view;
    if (view == View::Ability) abilityId_ = detailId;
    if (view == View::Neigong) neigongId_ = detailId;
    if (view == View::Information) information_ = std::move(information);
    listPage_ = 0;
    textPage_ = 0;
    rebuild();
}

void ChessPreparationScreen::back()
{
    if (view_ == View::Piece && characterInspector_.expanded)
    {
        characterInspector_.expanded = false;
        rebuild();
        return;
    }
    if (view_ == View::Replacement) replacementId_ = -1;
    if (auto previous = history_.pop())
    {
        auto context = std::move(*previous);
        view_ = context.view;
        selection_ = context.selection;
        equipmentId_ = context.equipmentId;
        equipmentType_ = context.equipmentType;
        abilityId_ = context.abilityId;
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
    const auto& piece = session_.state().roster.at(id);
    if (view_ == View::Replacement && piece.deployed) { replacementId_ = id; textPage_ = 0; rebuild(); return; }
    selection_.selectPiece(id);
    characterInspector_ = {};
    replacementId_ = -1;
    if (view_ != View::Equipment) { view_ = View::Piece; history_.clear(); }
    rebuild();
}

void ChessPreparationScreen::selectShop(int slot)
{
    selection_.selectShop(slot, session_.state().shop[slot].roleId);
    characterInspector_ = {};
    view_ = View::Piece;
    history_.clear();
    replacementId_ = -1;
    rebuild();
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
    auto result = callbacks_.submit(action);
    notice_ = result.accepted ? message : result.description;
    const int control = focusIndex();
    noticeAnchor_ = control >= 0 ? controls_[control].frame : inspector_;
    noticeUntil_ = Engine::getTicks() + (result.accepted ? 2600 : 5000);
    if (result.accepted)
    {
        arrivalIds_.clear();
        arrivalMerged_ = false;
        for (const auto& event : result.events)
        {
            if (event.type == ChessSemanticEventType::ChessPurchased || event.type == ChessSemanticEventType::ChessMerged)
            {
                arrivalIds_.push_back(event.primaryId);
                if (event.type == ChessSemanticEventType::ChessMerged) arrivalMerged_ = true;
            }
        }
        if (arrivalMerged_) playChessUpgradeSound();
        if (!arrivalIds_.empty()) arrivalUntil_ = Engine::getTicks() + 1500;
    }
    rebuild();
    return result;
}

void ChessPreparationScreen::buy(int slot)
{
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
    std::string deploymentFailure;
    if (automaticDeployment_ && !session_.state().roster.at(*destination).deployed
        && std::ranges::count(observation_.roster, true, &ChessSessionPiece::deployed) < observation_.maximumDeployment)
    {
        // 購買先經原有容量與合成檢查；成功後才串接原有上陣操作。
        const auto deployed = callbacks_.submit(chessPreparationDeployment(session_.state(), *destination));
        if (!deployed.accepted) deploymentFailure = "；" + deployed.description;
    }
    view_ = View::Piece;
    history_.clear();
    const auto& actual = session_.state().roster.at(*destination);
    notice_ = std::format("{} {} {} · {}", merged ? "合成完成" : "已招募", name, chessStars(actual.star), actual.deployed ? "出戰" : "候補") + deploymentFailure;
    arrivalIds_ = arrivals;
    arrivalMerged_ = merged;
    benchPage_ = 0;
    rebuild();
}

void ChessPreparationScreen::deploy()
{
    const auto& piece = session_.state().roster.at(selection_.instanceId);
    if (!piece.deployed && std::ranges::count(observation_.roster, true, &ChessSessionPiece::deployed) >= observation_.maximumDeployment)
    {
        replacementId_ = -1;
        navigate(View::Replacement);
        return;
    }
    const auto action = piece.deployed ? chessPreparationDeployment(session_.state(), -1, piece.instanceId)
        : chessPreparationDeployment(session_.state(), piece.instanceId);
    submit(action, piece.deployed ? "棋子已撤回候補" : "棋子已上陣");
}

void ChessPreparationScreen::draw()
{
    // Talk／獎勵／戰前面板執行時，只顯示底下的場景與天氣。
    if (!isCurrentRunOwner()) return;
    for (const auto& draw : drawings_) draw();
}

int ChessPreparationScreen::hit(SDL_FPoint point) const
{
    for (int i = static_cast<int>(controls_.size()) - 1; i >= 0; --i)
        if (contains(controls_[i].frame, point)) return i;
    return -1;
}

int ChessPreparationScreen::focusIndex() const
{
    const auto found = std::ranges::find(controls_, focusKey_, &Control::key);
    return found == controls_.end() ? -1 : static_cast<int>(found - controls_.begin());
}

void ChessPreparationScreen::activate(std::string key, bool touch)
{
    const auto found = std::ranges::find(controls_, key, &Control::key);
    if (found == controls_.end() || !found->enabled || !found->activate) return;
    focusKey_ = key;
    if (touch && found->touchConfirmation && !touchConfirmation_.confirm(key))
    {
        auto preview = found->preview;
        if (preview) preview();
        touchHint_ = "再點一次確認";
        rebuild();
        return;
    }
    touchConfirmation_.reset();
    touchHint_.clear();
    // 操作會重建 controls_；執行前複製，避免銷毀正在執行的函式。
    auto action = found->activate;
    action();
}

RunNode::PointerResult ChessPreparationScreen::onPointerEvent(const PointerEvent& event)
{
    if (event.phase == PointerPhase::Cancel) { pressedKey_.clear(); popoverDragging_ = false; return PointerResult::Released; }
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
    if (event.button != SDL_BUTTON_LEFT) return PointerResult::Handled;
    const int index = hit(event.uiPosition);
    if (event.phase == PointerPhase::ButtonDown)
    {
        keyboardFocus_ = false;
        pressedKey_ = index >= 0 ? controls_[index].key : std::string{};
        if (index >= 0) focusKey_ = pressedKey_;
        return PointerResult::Captured;
    }
    if (event.phase == PointerPhase::ButtonUp)
    {
        const auto key = std::exchange(pressedKey_, {});
        if (index >= 0 && controls_[index].key == key)
        {
            const auto shortcut = controls_[index].shortcut;
            const bool touch = event.source == PointerSource::Touch || GameUtil::isMobileDevice();
            if (shortcut)
            {
                // 出戰狀態也屬於身分，避免同一串輸入反轉剛完成的上／下陣。
                const auto identity = key + (key.starts_with("piece:")
                    && session_.state().roster.at(std::stoi(key.substr(6))).deployed ? ":deployed" : "");
                if (portraitClicks_.release(identity, event.clicks, event.timestamp, touch))
                { touchConfirmation_.reset(); touchHint_.clear(); shortcut(); }
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
    portraitClicks_.reset();
    touchConfirmation_.reset();
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
}

void ChessPreparationScreen::dealEvent(EngineEvent& event)
{
    // 與場景的 Q 快捷鍵同樣在放開時處理，避免收起的 key-up 又把整備打開。
    if (event.type == EVENT_KEY_UP && event.key.key == SDLK_Q) { setExit(true); return; }
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
    if (equipmentPopover_.equipmentId >= 0) { dismissEquipmentPopover(); return; }
    if (!touchConfirmation_.armedKey.empty())
    {
        touchConfirmation_.reset(); touchHint_.clear(); rebuild(); return;
    }
    if (banManagement_) { banManagement_ = false; rebuild(); return; }
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
            drawText(row.text, row.fontSize, x + 20 + row.indentPixels, bodyY + rowY, equipmentTextColor(row.tone));
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
        const auto control = std::ranges::find(controls_, touchConfirmation_.armedKey, &Control::key);
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
    const auto pointer = PointerInput::instance().logicalPointerUiPosition();
    if (equipmentPopover_.pinned)
    {
        drawEquipmentPopover();
        return;
    }
    if (touchInput_ && !keyboardFocus_) return;
    if (equipmentPopover_.equipmentId >= 0 && contains(popoverFrame_, pointer) && !keyboardFocus_)
    {
        popoverLeaveSince_ = 0;
        drawEquipmentPopover();
        return;
    }
    int index = hit(pointer);
    if (keyboardFocus_) index = focusIndex();
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
    if (peekId >= 0 && peekId != dismissedPopoverId_ && Engine::getTicks() - hoverSince_ >= 350)
    {
        equipmentPopover_.preview(peekId);
        drawEquipmentPopover();
        return;
    }
    if (control.tooltip.empty() || peekId >= 0) return;
    if (Engine::getTicks() - hoverSince_ < 650) return;
    const int width = std::clamp(Font::getTextDrawSize(control.tooltip) * 8 + 24, 80, std::min(480, w_ - 40));
    const auto lines = wrapDisplayText(control.tooltip, (width - 24) * 2 / 16, true);
    const int height = static_cast<int>(lines.size()) * 20 + 20;
    int x = std::clamp(control.frame.x, 20, w_ - width - 20);
    int y = control.frame.y - height - 8;
    if (y < 8) y = control.frame.y + control.frame.h + 8;
    y = std::clamp(y, 8, std::max(8, h_ - height - 8));
    fill({x, y, width, height}, {29, 26, 23, 250}, 6);
    outline({x, y, width, height}, line, 6);
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        drawText(lines[i], 16, x + 12, y + 10 + i * 20, ink);
}

} // namespace KysChess
