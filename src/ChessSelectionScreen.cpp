#include "GameUtil.h"
#include "ChessTalentUi.h"
#include "ChessSelectionScreen.h"
#include "../others/Hanz2Piny.h"
#include "DisplayText.h"
#include "InputBox.h"
#include "Button.h"
#include "TextureManager.h"
#include "ChessCharacterUi.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <utility>

namespace KysChess
{
namespace
{
using namespace ChessUiStyle;

bool contains(SDL_Rect rect, SDL_FPoint point)
{
    return point.x >= rect.x && point.x < rect.x + rect.w
        && point.y >= rect.y && point.y < rect.y + rect.h;
}

void image(const ChessSelectionImage& image, SDL_Rect frame)
{
    asset(image.group, image.id, frame);
}

}

ChessSelectionScreen::ChessSelectionScreen(std::string title, std::vector<ChessSelectionItem> items)
    : title_(std::move(title)), items_(std::move(items))
{
    assert(!items_.empty());
    for (const auto& item : items_)
        if (!item.category.empty() && std::ranges::find(categories_, item.category) == categories_.end())
            categories_.push_back(item.category);
    filter();
}

void ChessSelectionScreen::onEntrance()
{
    onPointerInputReset();
    updateCompact();
    expand(expanded_);
}

void ChessSelectionScreen::updateCompact()
{
    const auto* engine = Engine::getInstance();
    const bool compact = layout_ != ChessSelectionLayout::Talent && layout_ != ChessSelectionLayout::Overview
        && layout_ != ChessSelectionLayout::Tabbed
        && chessSelectionCompact(engine->getPresentWidth(), engine->getPresentHeight());
    if (compact == compact_) return;
    compact_ = compact;
    const auto found = std::ranges::find(visible_, selected_);
    page_ = found == visible_.end() ? 0 : static_cast<int>(found - visible_.begin()) / pageSize();
    ++revision_;
}

void ChessSelectionScreen::setSelectedItem(int index)
{
    assert(index >= 0 && index < static_cast<int>(items_.size()));
    selected_ = index;
    const auto found = std::ranges::find(visible_, index);
    if (found != visible_.end()) page_ = static_cast<int>(found - visible_.begin()) / pageSize();
}

void ChessSelectionScreen::filter()
{
    visible_.clear();
    const auto query = Font::getInstance()->T2S(query_);
    for (int i = 0; i < static_cast<int>(items_.size()); ++i)
    {
        const auto& item = items_[i];
        if (!category_.empty() && item.category != category_) continue;
        if (!query.empty() && !Font::getInstance()->T2S(item.title).contains(query)
            && !Hanz2Piny::hanz2pinyin(item.title).contains(query)) continue;
        visible_.push_back(i);
    }
    page_ = 0;
    if (!visible_.empty()) select(visible_.front());
    ++revision_;
}

void ChessSelectionScreen::select(int index)
{
    selected_ = index;
    inspectedRow_ = -1;
    inspection_.reset();
    detailScroll_ = 0;
    detailTab_ = 0;
    characterInspector_ = {.expanded = expanded_};
    ++revision_;
}

void ChessSelectionScreen::changePage(int step)
{
    const int pages = std::max(1, (static_cast<int>(visible_.size()) + pageSize() - 1) / pageSize());
    page_ = std::clamp(page_ + step, 0, pages - 1);
    focused_ = 0;
    pressed_ = -1;
    ++revision_;
}

void ChessSelectionScreen::expand(bool value)
{
    expanded_ = value;
    detailScroll_ = 0;
    detailTab_ = 0;
    characterInspector_ = {.expanded = value};
    focused_ = 0;
    pressed_ = -1;
    ++revision_;
}

void ChessSelectionScreen::control(SDL_Rect frame, std::function<void()> activate)
{
    controls_.push_back({frame, std::move(activate)});
}

void ChessSelectionScreen::button(SDL_Rect frame, const std::string& text, std::function<void()> activate,
    bool primary, std::optional<Icon> symbol, bool selected, bool action)
{
    const ControlState state{.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
        .selected = selected, .focused = keyboard_ && focused_ == static_cast<int>(controls_.size()),
        .primary = primary, .action = action};
    controlSurface(frame, state);
    const int inset = symbol ? (compact_ ? 44 : 32) : 0;
    const int size = std::min(compact_ ? 28 : Type::body,
        (frame.w - 20 - inset) * 2 / std::max(1, Font::getTextDrawSize(text)));
    controlLabel(frame, text, size, controlText(state), symbol);
    control(frame, std::move(activate));
}

void ChessSelectionScreen::characterInspector(const ChessCharacterPresentation& character, SDL_Rect frame,
    std::optional<SDL_Rect> navigationFrame)
{
    auto content = composeChessCharacterInspector(character, frame, characterInspector_, compact_, navigationFrame);
    for (const auto& itemControl : content.controls)
    {
        button(itemControl.frame, itemControl.label, [this, itemControl] {
            if (itemControl.comboId)
            {
                items_[selected_].openCombo(*itemControl.comboId);
                return;
            }
            applyChessInspectorControl(characterInspector_, itemControl);
            if (!inspection_) expanded_ = characterInspector_.expanded;
            ++revision_;
        }, false, itemControl.icon, itemControl.selected, itemControl.action);
        controls_.back().tooltip = itemControl.tooltip;
    }
    for (const auto& draw : content.drawings) draw();
}

void ChessSelectionScreen::inspector()
{
    const auto& item = items_[selected_];
    SDL_Rect frame = compact_ ? SDL_Rect{568, 112, 696, 592} : SDL_Rect{904, 80, 360, 560};
    if (!browseOnly_) frame.h -= compact_ ? 96 : 64;
    panel(frame.x, frame.y, frame.w, frame.h);
    if (item.character) characterInspector(*item.character, frame);
    else
    {
        const int x = frame.x + 12, y = frame.y + 12, width = frame.w - 24;
        const int art = compact_ ? 100 : 78, size = compact_ ? 30 : 22;
        image(item.image, {x, y, art, art});
        label(item.title, size, x + art + 12, y + 2, layout_ == ChessSelectionLayout::ShortChoice ? item.accent : ink, width - art - 12, 2);
        label(item.badge, compact_ ? 24 : 16, x + art + 12, y + art - 24, item.accent, width - art - 12);
        int bodyY = y + art + 12;
        if (!item.subtitle.empty())
        {
            label(item.subtitle, compact_ ? 24 : 17, x, bodyY, muted, width, 2);
            bodyY += compact_ ? 64 : 48;
        }
        const int actionHeight = compact_ ? 84 : 44;
        const int actionY = frame.y + frame.h - actionHeight - 12;
        const bool hasDetails = detailRenderer_ || !item.detailTabs.empty() || !item.description.empty();
        if (expanded_ && hasDetails)
        {
            auto tabs = item.detailTabs;
            if (!tabs.empty())
            {
                detailTab_ = std::clamp(detailTab_, 0, static_cast<int>(tabs.size()) - 1);
                const int tabHeight = compact_ ? 64 : 44;
                for (int i = 0; i < static_cast<int>(tabs.size()); ++i)
                    button({x + i * width / static_cast<int>(tabs.size()), bodyY,
                        width / static_cast<int>(tabs.size()) - 4, tabHeight}, tabs[i].first,
                        [this, i] { detailTab_ = i; detailScroll_ = 0; ++revision_; }, false, std::nullopt, detailTab_ == i);
                bodyY += tabHeight + 12;
            }
            if (detailRenderer_) detailRenderer_(selected_, {x, bodyY, width, actionY - bodyY - 12});
            else
            {
                const int fontSize = compact_ ? 26 : 18, rowHeight = fontSize + 6;
                const auto rows = wrapLines(tabs.empty() ? item.description : tabs[detailTab_].second, width * 2 / fontSize);
                const int count = std::max(1, (actionY - bodyY - 12) / rowHeight);
                detailScrollMax_ = std::max(0, (static_cast<int>(rows.size()) - 1) / count);
                detailScroll_ = std::clamp(detailScroll_, 0, detailScrollMax_);
                for (int i = 0; i < count && detailScroll_ * count + i < static_cast<int>(rows.size()); ++i)
                    label(rows[detailScroll_ * count + i], fontSize, x, bodyY + i * rowHeight, ink, width);
                if (detailScrollMax_ > 0)
                {
                    const int arrow = compact_ ? 84 : 44;
                    if (detailScroll_ > 0) button({x, actionY, arrow, actionHeight}, {},
                        [this] { --detailScroll_; }, false, Icon::Up);
                    label(std::format("{} / {}", detailScroll_ + 1, detailScrollMax_ + 1),
                        compact_ ? 26 : 16, x + arrow + 8, actionY + (compact_ ? 28 : 14), muted, compact_ ? 100 : 68);
                    if (detailScroll_ < detailScrollMax_) button({x + arrow + (compact_ ? 112 : 76), actionY, arrow, actionHeight}, {},
                        [this] { ++detailScroll_; }, false, Icon::Down);
                }
            }
            button({x + width - (compact_ ? 164 : 112), actionY, compact_ ? 164 : 112, actionHeight},
                "收起", [this] { expand(false); }, false, Icon::Collapse);
        }
        else
        {
            for (int i = 0; i < static_cast<int>(item.facts.size()); ++i)
            {
                const int columnX = x + i % 2 * width / 2;
                const int top = bodyY + i / 2 * (compact_ ? 76 : 56);
                label(item.facts[i].first, compact_ ? 22 : 15, columnX, top, muted, width / 2 - 8);
                label(item.facts[i].second, size, columnX, top + (compact_ ? 28 : 21), ink, width / 2 - 8);
            }
            bodyY += static_cast<int>((item.facts.size() + 1) / 2) * (compact_ ? 76 : 56);
            const int summaryEnd = item.related.empty() ? actionY : actionY - (compact_ ? 112 : 92);
            label(item.summary, compact_ ? 28 : 18, x, bodyY + 8, ink, width,
                std::max(1, (summaryEnd - bodyY - 16) / (compact_ ? 36 : 24)));
            if (!item.related.empty())
            {
                const int relatedY = actionY - (compact_ ? 104 : 84);
                label(item.relatedTitle, compact_ ? 22 : 15, x, relatedY, muted, width);
                for (int i = 0; i < std::min(6, static_cast<int>(item.related.size())); ++i)
                    image(item.related[i], {x + i * width / 6, relatedY + (compact_ ? 30 : 24), width / 6 - 8, compact_ ? 64 : 48});
            }
            if (hasDetails) button({x, actionY, width, actionHeight}, "詳細資料", [this] { expand(true); }, false, Icon::Right);
        }
    }
    if (!browseOnly_) button(compact_ ? SDL_Rect{584, 612, 664, 84} : SDL_Rect{920, 588, 328, 52},
        confirmationLabel(), [this] { exitWithResult(selected_); }, true);
}

void ChessSelectionScreen::drawTopmostOverlay()
{
    if (!isCurrentRunOwner() || GameUtil::isMobileDevice()) return;
    const int index = keyboard_ ? focused_ : hit(PointerInput::instance().logicalPointerUiPosition());
    if (index < 0 || index >= static_cast<int>(controls_.size()) || controls_[index].tooltip.empty()) return;
    const auto& item = controls_[index];
    const int width = compact_ ? 600 : 360, size = compact_ ? 26 : 18;
    const auto lines = wrapLines(item.tooltip, (width - 24) * 2 / size);
    const int height = static_cast<int>(lines.size()) * (size + 4) + 24;
    const int x = std::clamp(item.frame.x, 12, 1268 - width);
    const int y = std::clamp(item.frame.y - height - 8, 12, 708 - height);
    panel(x, y, width, height, {29, 26, 23, 245});
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        label(lines[i], size, x + 12, y + 12 + i * (size + 4), ink, width - 24);
}

void ChessSelectionScreen::draw()
{
    if (!isCurrentRunOwner()) return;
    updateCompact();
    controls_.clear();
    // 與整備共用場景底圖及一層半透明表面，避免重複疊加遮罩。
    if (layout_ == ChessSelectionLayout::Dialog)
    {
        const int dialogColumns = items_.size() == 2 ? 2 : compact_ ? 1 : 2;
        const int rowHeight = compact_ ? 82 : 60;
        const int rows = (static_cast<int>(items_.size()) + dialogColumns - 1) / dialogColumns;
        const int height = 120 + rows * (rowHeight + 12);
        const int top = (720 - height) / 2;
        modalShell({240, top, 800, height}, title_, compact_);
        if (exitable_) button({compact_ ? 948 : 984, top + 12, compact_ ? 80 : 44, compact_ ? 80 : 44},
            {}, [this] { onPressedCancel(); }, false, Icon::Close);
        for (int visual = 0; visual < static_cast<int>(items_.size()); ++visual)
        {
            const int i = dialogPrimary_ == 0 && items_.size() == 2 ? 1 - visual : visual;
            SDL_Rect frame{272 + visual % dialogColumns * 376,
                top + 96 + visual / dialogColumns * (rowHeight + 12), dialogColumns == 1 ? 736 : 360, rowHeight};
            const ControlState state{.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
                .focused = keyboard_ && focused_ == static_cast<int>(controls_.size()),
                .primary = dialogPrimary_ == i && dialogDestructive_ != i,
                .destructive = dialogDestructive_ == i};
            controlSurface(frame, state);
            label(items_[i].title, compact_ ? 28 : 18, frame.x + 16,
                frame.y + (frame.h - (compact_ ? 28 : 18)) / 2,
                controlText(state), frame.w - 32, 2);
            control(frame, [this, i] { exitWithResult(i); });
        }
        return;
    }
    ChessUiStyle::page(title_, {}, compact_ ? 30 : Type::sectionTitle);
    if (!headerNote_.empty()) label(headerNote_, Type::metadata, 220, 34, muted, 844);
    if (exitable_) button(compact_ ? SDL_Rect{1008, 16, 240, 84} : SDL_Rect{1080, 20, 168, 44},
        "返回", [this] {
            if (browseOnly_ && inspection_) { closeInspection(); return; }
            if (!exitConfirmation_ || exitConfirmation_()) exitWithResult(-1);
        }, false, Icon::Left);
    if (layout_ == ChessSelectionLayout::Overview)
    {
        drawOverview();
        return;
    }
    if (layout_ == ChessSelectionLayout::Tabbed)
    {
        drawTabbed();
        return;
    }
    if (layout_ == ChessSelectionLayout::Talent)
    {
        drawTalent();
        return;
    }
    if (layout_ == ChessSelectionLayout::ShortChoice)
    {
        drawChoice();
        return;
    }
    if (layout_ == ChessSelectionLayout::Command)
    {
        const int first = page_ * pageSize();
        const int count = std::min(pageSize(), static_cast<int>(items_.size()) - first);
        const int rowHeight = compact_ ? 88 : 64;
        const int top = compact_ ? 116 : (720 - count * rowHeight) / 2;
        const int left = compact_ ? 128 : 304, width = compact_ ? 1024 : 672;
        panel(left - 16, top - 12, width + 32, count * rowHeight + 24);
        for (int local = 0; local < count; ++local)
        {
            const int i = first + local;
            const auto& item = items_[i];
            SDL_Rect frame{left, top + local * rowHeight, width, rowHeight - 4};
            const ControlState state{.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
                .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())};
            controlSurface(frame, state);
            const int iconSize = compact_ ? 36 : 24;
            image(item.image, {frame.x + 20, frame.y + (frame.h - iconSize) / 2, iconSize, iconSize});
            label(item.title, compact_ ? 28 : 22, frame.x + 76, frame.y + (frame.h - (compact_ ? 28 : 22)) / 2, ink, frame.w - 104, 2);
            control(frame, [this, i] { exitWithResult(i); });
        }
        const int pages = std::max(1, (static_cast<int>(items_.size()) + pageSize() - 1) / pageSize());
        if (pages > 1) label(std::format("{} / {} · 共 {} 項", page_ + 1, pages, items_.size()),
            compact_ ? 26 : 18, 488, compact_ ? 640 : 666, muted, 320);
        if (page_ > 0) button(compact_ ? SDL_Rect{368, 612, 88, 84} : SDL_Rect{368, 652, 64, 44},
            {}, [this] { changePage(-1); }, false, Icon::Left);
        if (first + count < static_cast<int>(items_.size()))
            button(compact_ ? SDL_Rect{824, 612, 88, 84} : SDL_Rect{824, 652, 64, 44},
                {}, [this] { changePage(1); }, false, Icon::Right);
        return;
    }
    if (compact_) { drawCompactCatalogue(); return; }
    panel(16, 80, 872, 560);
    int tabX = 32;
    const auto tab = [&](const std::string& caption, const std::string& category) {
        SDL_Rect frame{tabX, 92, std::max(64, Font::getTextDrawSize(caption) * 8 + 28), 44};
        if (category_ == category) Engine::getInstance()->fillColor(gold, frame.x + 8, frame.y + 40, frame.w - 16, 2);
        button(frame, caption, [this, category] { category_ = category; filter(); });
        tabX += frame.w + 4;
    };
    if (categories_.size() > 1)
    {
        tab("全部", {});
        for (const auto& category : categories_) tab(category, category);
    }
    else label(std::format("{} 項", visible_.size()), 16, 40, 106, muted, 560);
    if (searchEnabled_) button({760, 92, 104, 44}, query_.empty() ? "搜尋" : "搜尋中", [this] { search(); }, false, Icon::Search);
    const int first = page_ * pageSize();
    const int count = std::min(pageSize(), static_cast<int>(visible_.size()) - first);
    for (int i = 0; i < count; ++i)
    {
        const int index = visible_[first + i];
        const auto& item = items_[index];
        SDL_Rect frame{32 + i % 4 * 212, 150 + i / 4 * 138, 204, 128};
        const bool selected = selected_ == index;
        const bool hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition());
        controlSurface(frame, {.hovered = hovered, .selected = selected,
            .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
        image(item.image, {frame.x + 12, frame.y + 8, 60, 64});
        label(item.badge, 15, frame.x + 84, frame.y + 16, item.accent, 108, 2);
        label(item.title, 19, frame.x + 12, frame.y + 80, ink, frame.w - 24);
        label(item.subtitle, 14, frame.x + 12, frame.y + 106, shopMode_ ? gold : muted, frame.w - 24);
        control(frame, [this, index] { select(index); });
        controls_.back().itemIndex = index;
    }
    if (visible_.empty()) label("沒有符合的項目", 20, 48, 190, muted, 768);
    const int pages = std::max(1, (static_cast<int>(visible_.size()) + pageSize() - 1) / pageSize());
    if (pages > 1)
    {
        if (page_ > 0) button({316, 580, 64, 44}, {}, [this] { changePage(-1); }, false, Icon::Left);
        label(std::format("{} / {}", page_ + 1, pages), 16, 408, 594, muted, 100);
        if (page_ + 1 < pages) button({508, 580, 64, 44}, {}, [this] { changePage(1); }, false, Icon::Right);
    }
    if (!visible_.empty()) inspector();
}

void ChessSelectionScreen::drawOverview()
{
    panel(16, 80, 276, 624);
    const int first = page_ * pageSize();
    const int count = std::min(pageSize(), static_cast<int>(items_.size()) - first);
    for (int row = 0; row < count; ++row)
    {
        const int index = first + row;
        const auto& item = items_[index];
        const SDL_Rect area{28, 92 + row * 60, 252, 56};
        controlSurface(area, {.hovered = contains(area, PointerInput::instance().logicalPointerUiPosition()),
            .selected = selected_ == index, .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
        image(item.image, {area.x + 8, area.y + 8, 36, 36});
        label(item.title, 20, area.x + 54, area.y + 6, item.accent, 190);
        label(item.badge, 16, area.x + 54, area.y + 32, muted, 190);
        control(area, [this, index] { select(index); });
        controls_.back().itemIndex = index;
    }
    const int pages = (static_cast<int>(items_.size()) + pageSize() - 1) / pageSize();
    if (page_ > 0) button({28, 648, 52, 44}, {}, [this] { changePage(-1); }, false, Icon::Left);
    label(std::format("{} / {}", page_ + 1, pages), 18, 126, 662, muted, 100);
    if (page_ + 1 < pages) button({228, 648, 52, 44}, {}, [this] { changePage(1); }, false, Icon::Right);

    const auto& item = items_[selected_];
    panel(304, 80, browseOnly_ ? (inspection_ ? 276 : 960) : 536, 624);
    if (!browseOnly_) panel(852, 80, 412, 624);
    image(item.image, {324, 96, 48, 48});
    label(item.title, 28, 388, 104, gold, 480);
    if (browseOnly_)
    {
        if (!inspection_) label(item.badge + " · " + item.subtitle, 20, 900, 110, item.accent, 336);
        drawOverviewBody({324, 174, 920, 454}, 20);
        if (inspection_)
        {
            panel(592, 80, 672, 624);
            Engine::getInstance()->fillColor(line, 604, 640, 648, 1);
            button({604, 648, 316, 44}, "返回羈絆", [this] { closeInspection(); }, false, Icon::Left);
            characterInspector(*inspection_, {592, 80, 672, 560}, SDL_Rect{936, 648, 316, 44});
        }
    }
    else
    {
        Engine::getInstance()->fillColor(line, 864, 640, 388, 1);
        if (!inspection_) label(item.badge, Type::body, 1060, 110, item.accent, 176);
        label(item.subtitle, Type::body, 324, 152, muted, 500);
        drawOverviewBody({324, 190, 920, 502}, Type::body);
        if (inspection_)
        {
            Engine::getInstance()->fillColor(line, 864, 572, 388, 1);
            button({864, 584, 188, 44}, "返回獎勵", [this] { closeInspection(); }, false, Icon::Left);
            characterInspector(*inspection_, {852, 80, 412, 496}, SDL_Rect{1064, 584, 188, 44});
        }
        button({864, 648, 388, 44}, confirmationLabel(), [this] { exitWithResult(selected_); }, true);
    }
}

void ChessSelectionScreen::drawTabbed()
{
    const int count = static_cast<int>(items_.size());
    const int width = (1232 - (count - 1) * 6) / count;
    for (int i = 0; i < count; ++i)
        button({24 + i * (width + 6), 84, width, 52}, items_[i].title,
            [this, i] { select(i); }, false, std::nullopt, selected_ == i);

    panel(16, 148, 1248, 556);
    const auto& item = items_[selected_];
    label(item.title, 28, 40, 169, gold, 320);
    label(item.subtitle, 18, 384, 177, muted, 840);
    drawOverviewBody({40, 222, 1200, 410}, 22);
}

void ChessSelectionScreen::drawOverviewBody(SDL_Rect frame, int fontSize)
{
    const auto& item = items_[selected_];
    const int bodyY = frame.y, bodyHeight = frame.h, lineHeight = fontSize + 6;
    struct Line { std::string text; Color color; int y{}; int sourceRow{}; };
    std::vector<std::vector<std::vector<Line>>> columns;
    detailScrollMax_ = 0;
    for (const auto& column : item.overview)
    {
        if (inspection_ && &column == &item.overview.back()) break;
        const int columnHeight = layout_ == ChessSelectionLayout::Overview && !browseOnly_
            && &column == &item.overview.back() ? 448 : bodyHeight;
        const auto& rows = column.rows;
        std::vector<std::vector<Line>> pages(1);
        int y{};
        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex)
        {
            const auto& row = rows[rowIndex];
            if (row.synergies)
            {
                const int perLine = (column.width - 80) / 90;
                const int count = static_cast<int>(row.synergies().size());
                const int height = count == 0 ? 32 : (count + perLine - 1) / perLine * 40;
                if (y + height + lineHeight > columnHeight) { pages.emplace_back(); y = 0; }
                pages.back().push_back({row.text, row.color, y, rowIndex});
                y += height;
                continue;
            }
            const auto lines = wrapDisplayText((row.prefix ? row.prefix->first : "") + row.text,
                (column.width - (row.inspect ? 48 : 16)) * 2 / fontSize, true, DisplayTextWrapping::Prose);
            if (row.heading && y > 0) y += 8;
            // 標題至少與下一行一起留在同頁。
            if (y + lineHeight * (row.heading ? 2 : 1) > columnHeight) { pages.emplace_back(); y = 0; }
            for (const auto& line : lines)
            {
                if (y + lineHeight > columnHeight) { pages.emplace_back(); y = 0; }
                pages.back().push_back({line, row.color, y, rowIndex});
                y += lineHeight;
            }
            y += 8;
        }
        detailScrollMax_ = std::max(detailScrollMax_, static_cast<int>(pages.size()) - 1);
        columns.push_back(std::move(pages));
    }
    detailScroll_ = std::clamp(detailScroll_, 0, detailScrollMax_);
    int x = frame.x;
    for (int i = 0; i < static_cast<int>(columns.size()); ++i)
    {
        const auto& pages = columns[i];
        const auto& lines = pages[std::min(detailScroll_, static_cast<int>(pages.size()) - 1)];
        for (int start = 0; start < static_cast<int>(lines.size());)
        {
            const auto& line = lines[start];
            int end = start + 1;
            while (end < static_cast<int>(lines.size()) && lines[end].sourceRow == line.sourceRow) ++end;
            const auto& rows = item.overview[i].rows;
            const auto& row = rows[line.sourceRow];
            if (row.synergies)
            {
                label(row.text, fontSize, x, bodyY + line.y, row.color, 60);
                const auto entries = row.synergies();
                const int perLine = (item.overview[i].width - 80) / 90;
                for (int n = 0; n < static_cast<int>(entries.size()); ++n)
                {
                    const auto& entry = entries[n];
                    const SDL_Rect area{x + 64 + n % perLine * 90, bodyY + line.y - 3 + n / perLine * 40, 88, 38};
                    controlSurface(area, {.hovered = contains(area, PointerInput::instance().logicalPointerUiPosition()),
                        .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
                    asset(entry.imageGroup, entry.imageId, {area.x + 2, area.y + 5, 28, 28});
                    label(entry.value, 16, area.x + 34, area.y + 10, positive, 56);
                    control(area, [this, id = entry.id] { items_[selected_].openCombo(id); });
                    controls_.back().tooltip = entry.tooltip;
                }
                start = end;
                continue;
            }
            if (row.inspect)
            {
                const SDL_Rect area{x, bodyY + line.y - 3, item.overview[i].width - 20,
                    lines[end - 1].y - line.y + lineHeight + 6};
                controlSurface(area, {.hovered = contains(area, PointerInput::instance().logicalPointerUiPosition()),
                    .selected = inspectedRow_ == line.sourceRow,
                    .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
                control(area, [this, index = line.sourceRow, inspect = row.inspect] {
                    inspection_ = inspect();
                    characterInspector_ = {};
                    inspectedRow_ = index;
                    inspectionOriginScroll_ = detailScroll_;
                    ++revision_;
                });
                label("›", fontSize, area.x + area.w - 20, area.y + 3, context, 20);
            }
            for (int index = start; index < end; ++index)
            {
                int textX = x + (row.inspect ? 6 : 0);
                int textWidth = item.overview[i].width - (row.inspect ? 48 : 16);
                auto text = lines[index].text;
                if (row.prefix && text.starts_with(row.prefix->first))
                {
                    const auto& [prefix, color] = *row.prefix;
                    const int prefixWidth = Font::getTextDrawSize(prefix) * fontSize / 2;
                    label(prefix, fontSize, textX, bodyY + lines[index].y, color, prefixWidth);
                    text.erase(0, prefix.size());
                    textX += prefixWidth;
                    textWidth -= prefixWidth;
                }
                label(text, fontSize, textX, bodyY + lines[index].y, lines[index].color, textWidth);
            }
            start = end;
        }
        x += item.overview[i].width;
    }
    if (detailScrollMax_ > 0)
    {
        const bool actionOverview = layout_ == ChessSelectionLayout::Overview && !browseOnly_;
        const int x = actionOverview ? 324 : 976;
        const int nextX = actionOverview ? 544 : 1196;
        if (detailScroll_ > 0) button({x, 648, 44, 44}, {}, [this] { --detailScroll_; }, false, Icon::Up);
        label(std::format("{} / {}", detailScroll_ + 1, detailScrollMax_ + 1), 16, x + 52, 662, muted, nextX - x - 60);
        if (detailScroll_ < detailScrollMax_) button({nextX, 648, 44, 44}, {}, [this] { ++detailScroll_; }, false, Icon::Down);
    }
}

void ChessSelectionScreen::drawTalent()
{
    // 固定 1280 × 720 畫布：選項、說明與完整表格同屏呈現。
    panel(16, 80, 288, 560);
    for (int index = 0; index < static_cast<int>(items_.size()); ++index)
    {
        const auto& item = items_[index];
        const SDL_Rect frame{28, 96 + index * 104, 264, 96};
        controlSurface(frame, {.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
            .selected = selected_ == index, .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
        image(item.image, {frame.x + 12, frame.y + 12, 72, 72});
        label(item.title, 28, frame.x + 104, frame.y + 30, selected_ == index ? gold : ink, 148);
        control(frame, [this, index] { select(index); });
        controls_.back().itemIndex = index;
    }

    const auto& item = items_[selected_];
    assert(item.talent);
    panel(316, 80, 948, 560);
    image(item.image, {336, 100, 144, 144});
    label(item.title, 32, 504, 104, gold, 720);
    const auto description = wrapChessTalentDescription(item.talent->description,
        displayTextUnitsForPixelWidth(21, 736));
    for (int i = 0; i < static_cast<int>(description.size()); ++i)
        label(description[i], 21, 504, 154 + i * 27, ink, 736);
    if (!item.talent->facts.empty())
        drawChessTalentFactTable(*item.talent, 336, 256, 908, 368, 20, 20);

    label("上下切換天賦 · 確認開始棋局 · 返回選擇難度", 18, 112, 674, muted, 740);
    button({936, 652, 312, 52}, confirmationLabel(), [this] { exitWithResult(selected_); }, true);
}

void ChessSelectionScreen::drawChoice()
{
    panel(16, compact_ ? 112 : 80, compact_ ? 536 : 872, compact_ ? 592 : 560);
    for (int index = 0; index < static_cast<int>(items_.size()); ++index)
    {
        const auto& item = items_[index];
        const SDL_Rect frame = compact_ ? SDL_Rect{32, 128 + index * 166, 504, 154}
            : SDL_Rect{32 + index * 284, 108, 272, 488};
        controlSurface(frame, {.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
            .selected = selected_ == index, .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
        const int art = compact_ ? 72 : 128;
        image(item.image, {frame.x + 16, frame.y + 12, art, art});
        label(item.title, compact_ ? 32 : 24, frame.x + (compact_ ? art + 28 : 20),
            frame.y + (compact_ ? 26 : 168), item.accent, compact_ ? 376 : 232);
        label(item.subtitle, compact_ ? 24 : 18, frame.x + 16, frame.y + (compact_ ? 100 : 218), muted, frame.w - 32, 2);
        if (!compact_) label(item.summary, 20, frame.x + 16, frame.y + 282, ink, frame.w - 32, 6, 6);
        control(frame, [this, index] { select(index); });
        controls_.back().itemIndex = index;
    }
    inspector();
}

std::string ChessSelectionScreen::confirmationLabel() const
{
    return layout_ == ChessSelectionLayout::Talent ? actionLabel_ + " · " + items_[selected_].title : actionLabel_;
}

void ChessSelectionScreen::drawCompactCatalogue()
{
    panel(16, 112, 536, 592);
    if (categories_.size() > 1)
        button({32, 120, 304, 84}, category_.empty() ? "全部分類" : category_, [this] {
            const auto current = std::ranges::find(categories_, category_);
            category_ = current == categories_.end() ? categories_.front()
                : current + 1 == categories_.end() ? std::string{} : *(current + 1);
            filter();
        }, false, Icon::Refresh);
    else label(std::format("{} 項", visible_.size()), 27, 40, 148, muted, 280);
    if (searchEnabled_) button({360, 120, 176, 84}, "搜尋", [this] { search(); }, false, Icon::Search);
    const int first = page_ * pageSize();
    const int count = std::min(pageSize(), static_cast<int>(visible_.size()) - first);
    for (int local = 0; local < count; ++local)
    {
        const int index = visible_[first + local];
        const auto& item = items_[index];
        SDL_Rect frame{32, 212 + local * 130, 504, 122};
        controlSurface(frame, {.hovered = contains(frame, PointerInput::instance().logicalPointerUiPosition()),
            .selected = selected_ == index, .focused = keyboard_ && focused_ == static_cast<int>(controls_.size())});
        image(item.image, {frame.x + 12, frame.y + 12, 96, 96});
        label(item.title, 30, frame.x + 124, frame.y + 22, ink, frame.w - 140);
        label(item.badge, 24, frame.x + 124, frame.y + 66, item.accent, frame.w - 140);
        control(frame, [this, index] { select(index); });
        controls_.back().itemIndex = index;
    }
    if (visible_.empty()) label("沒有符合的項目", 30, 48, 232, muted, 480);
    const int pages = std::max(1, (static_cast<int>(visible_.size()) + pageSize() - 1) / pageSize());
    if (pages > 1)
    {
        if (page_ > 0) button({80, 612, 88, 84}, {}, [this] { changePage(-1); }, false, Icon::Left);
        label(std::format("{} / {}", page_ + 1, pages), 26, 224, 644, muted, 160);
        if (page_ + 1 < pages) button({400, 612, 88, 84}, {}, [this] { changePage(1); }, false, Icon::Right);
    }
    if (!visible_.empty()) inspector();
}

int ChessSelectionScreen::hit(SDL_FPoint point) const
{
    for (int i = static_cast<int>(controls_.size()) - 1; i >= 0; --i)
        if (contains(controls_[i].frame, point)) return i;
    return -1;
}

RunNode::PointerResult ChessSelectionScreen::onPointerEvent(const PointerEvent& event)
{
    if (event.phase == PointerPhase::Wheel)
    {
        if (inspection_ && event.uiPosition.x >= (browseOnly_ ? 592 : 852))
        {
            if (characterInspector_.expanded) characterInspector_.page += event.wheel.y > 0 ? -1 : 1;
            return PointerResult::Handled;
        }
        if ((layout_ == ChessSelectionLayout::Overview && event.uiPosition.x >= 304)
            || layout_ == ChessSelectionLayout::Tabbed)
        {
            detailScroll_ = std::clamp(detailScroll_ + (event.wheel.y > 0 ? -1 : 1), 0, detailScrollMax_);
            return PointerResult::Handled;
        }
        const bool overInspector = event.uiPosition.x >= (compact_ ? 568 : 904);
        if (!expanded_ || !overInspector) changePage(event.wheel.y > 0 ? -1 : 1);
        else if (items_[selected_].character) characterInspector_.page += event.wheel.y > 0 ? -1 : 1;
        else detailScroll_ = std::clamp(detailScroll_ + (event.wheel.y > 0 ? -1 : 1), 0, detailScrollMax_);
        return PointerResult::Handled;
    }
    if (event.phase == PointerPhase::Cancel) { onPointerInputReset(); return PointerResult::Handled; }
    if (event.button != SDL_BUTTON_LEFT) return PointerResult::Handled;
    const int index = hit(event.uiPosition);
    if (event.phase == PointerPhase::ButtonDown)
    {
        keyboard_ = false;
        pressed_ = index;
        return PointerResult::Captured;
    }
    if (event.phase == PointerPhase::ButtonUp)
    {
        const int pressed = std::exchange(pressed_, -1);
        if (index >= 0 && index == pressed)
        {
            focused_ = index;
            const auto activate = controls_[index].activate;
            activate();
        }
    }
    return PointerResult::Handled;
}

void ChessSelectionScreen::dealEvent(EngineEvent& event)
{
    if (event.type == EVENT_KEY_DOWN && !event.key.repeat && event.key.key == SDLK_TAB)
    {
        keyboard_ = true;
        focused_ = nextChessSelectionFocus(focused_, static_cast<int>(controls_.size()));
        return;
    }
    if (expanded_ || layout_ == ChessSelectionLayout::Overview || layout_ == ChessSelectionLayout::Tabbed)
    {
        if (event.type == EVENT_KEY_DOWN && !event.key.repeat)
        {
            if (items_[selected_].character || (inspection_ && characterInspector_.expanded))
            {
                if (event.key.key == SDLK_PAGEUP) --characterInspector_.page;
                if (event.key.key == SDLK_PAGEDOWN) ++characterInspector_.page;
                return;
            }
            if (event.key.key == SDLK_PAGEUP) detailScroll_ = std::max(0, detailScroll_ - 1);
            if (event.key.key == SDLK_PAGEDOWN) detailScroll_ = std::min(detailScrollMax_, detailScroll_ + 1);
        }
        if (expanded_ || (event.type == EVENT_KEY_DOWN
            && (event.key.key == SDLK_PAGEUP || event.key.key == SDLK_PAGEDOWN))) return;
    }
    int dx{}, dy{};
    if (event.type == EVENT_GAMEPAD_BUTTON_UP)
    {
        switch (event.gbutton.button)
        {
        case GAMEPAD_BUTTON_DPAD_LEFT: dx = -1; break;
        case GAMEPAD_BUTTON_DPAD_RIGHT: dx = 1; break;
        case GAMEPAD_BUTTON_DPAD_UP: dy = -1; break;
        case GAMEPAD_BUTTON_DPAD_DOWN: dy = 1; break;
        default: break;
        }
    }
    if (event.type == EVENT_KEY_DOWN && !event.key.repeat)
    {
        switch (event.key.key)
        {
        case SDLK_LEFT: dx = -1; break;
        case SDLK_RIGHT: dx = 1; break;
        case SDLK_UP: dy = -1; break;
        case SDLK_DOWN: dy = 1; break;
        case SDLK_PAGEUP: changePage(-1); return;
        case SDLK_PAGEDOWN: changePage(1); return;
        default: break;
        }
    }
    if ((dx || dy) && !visible_.empty() && !expanded_)
    {
        if (layout_ == ChessSelectionLayout::Command || layout_ == ChessSelectionLayout::Dialog)
        {
            keyboard_ = true;
            focused_ = nextChessSelectionFocus(focused_, static_cast<int>(controls_.size()), dx + dy < 0);
            return;
        }
        keyboard_ = false;
        const auto position = std::ranges::find(visible_, selected_) - visible_.begin();
        const int columns = this->columns();
        const int next = std::clamp(static_cast<int>(position) + dx + dy * columns, 0, static_cast<int>(visible_.size()) - 1);
        select(visible_[next]);
        page_ = next / pageSize();
        ++revision_;
    }
}

void ChessSelectionScreen::onPressedOK()
{
    if (keyboard_ && focused_ < static_cast<int>(controls_.size()))
    {
        const auto activate = controls_[focused_].activate;
        activate();
    }
    else if (expanded_ || inspection_.has_value() || layout_ == ChessSelectionLayout::Tabbed) return;
    else if (!visible_.empty())
    {
        if (compact_ && layout_ == ChessSelectionLayout::Catalogue) expand(true);
        else if (browseOnly_) { if (items_[selected_].character || detailRenderer_ || !items_[selected_].description.empty()) expand(true); }
        else exitWithResult(selected_);
    }
}

void ChessSelectionScreen::closeInspection()
{
    inspectedRow_ = -1;
    inspection_.reset();
    characterInspector_ = {};
    detailScroll_ = inspectionOriginScroll_;
    ++revision_;
}

void ChessSelectionScreen::onPressedCancel()
{
    if (inspection_)
    {
        if (characterInspector_.expanded)
        {
            characterInspector_ = {};
            ++revision_;
            return;
        }
        closeInspection();
        return;
    }
    if (expanded_) { expand(false); return; }
    if (exitable_ && (!exitConfirmation_ || exitConfirmation_())) exitWithResult(-1);
}

void ChessSelectionScreen::search()
{
    class SearchBox : public InputBox
    {
    public:
        SearchBox() : InputBox("搜尋名稱或拼音", 24)
        {
            dark_ = 0;
            confirm_ = std::make_shared<Button>();
            cancel_ = std::make_shared<Button>();
            confirm_->setText("搜尋");
            cancel_->setText("取消");
            confirm_->setFlatPrimary(true);
            addChild(cancel_);
            addChild(confirm_);
            arrangeControls();
        }
        void draw() override
        {
            const bool compact = chessSelectionCompact(Engine::getInstance()->getPresentWidth(), Engine::getInstance()->getPresentHeight());
            if (compact != compact_) arrangeControls();
            Engine::getInstance()->fillColor(canvasScrim, 0, 0, 1280, 720);
            modalShell(frame_, title_, compact_);
            const int size = compact_ ? 32 : 24;
            const int y = frame_.y + (compact_ ? 116 : 84);
            controlSurface({frame_.x + 24, y - 12, frame_.w - 48, compact_ ? 88 : 56}, {.focused = true});
            label(text_ + "|", size, frame_.x + 40, y, ink, frame_.w - 80);
            Engine::getInstance()->setTextInputArea(frame_.x + 40, y, frame_.w - 80, size);
        }

        void onPressedOK() override
        {
            if (confirm_->getResult() >= 0) exitWithResult(0);
            else if (cancel_->getResult() >= 0) exitWithResult(-1);
        }
    private:
        void arrangeControls()
        {
            compact_ = chessSelectionCompact(Engine::getInstance()->getPresentWidth(), Engine::getInstance()->getPresentHeight());
            frame_ = modalFrame(compact_);
            const auto footer = actionFooter(frame_, compact_);
            const int size = compact_ ? 30 : 18;
            for (const auto& button : {cancel_, confirm_})
            {
                button->setFontSize(size);
                button->setFlatStyle(footer.leading.w, footer.leading.h);
                button->setTextPosition((footer.leading.w - size * 2) / 2, 0);
                button->setTextColor(ink);
            }
            cancel_->setPosition(footer.leading.x, footer.leading.y);
            confirm_->setPosition(footer.trailing.x, footer.trailing.y);
        }
        std::shared_ptr<Button> confirm_, cancel_;
        bool compact_{};
        SDL_Rect frame_{};
    };
    auto box = std::make_shared<SearchBox>();
    box->setText(query_);
    if (box->run() == 0) { query_ = box->getText(); filter(); }
}

} // namespace KysChess
