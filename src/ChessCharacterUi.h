#pragma once

#include "ChessCharacterPresentation.h"
#include "ChessUiStyle.h"

namespace KysChess
{
inline void drawChessCharacterIdentity(const ChessCharacterPresentation& character, SDL_Rect frame, int size)
{
    using namespace ChessUiStyle;
    const int portrait = frame.h;
    asset("head", character.headId, {frame.x, frame.y, portrait, portrait});
    const int x = frame.x + portrait + 12, width = frame.w - portrait - 12;
    label(character.name, size, x, frame.y + 2, ink, width);
    const bool showContext = character.context != "商店預覽" && character.context != "圖鑑預覽";
    label(std::format("{}費 · {}星", character.cost, character.star)
        + (frame.h < 78 && showContext ? " · " + character.context : ""), size * 3 / 4,
        x, frame.y + size + 10, chessPieceTierColor(character.cost), width);
    if (showContext && frame.h >= 78)
        label(character.context, size * 3 / 4, x, frame.y + size * 2 + 10, muted, width);
}

inline void drawChessCharacterStats(const ChessCharacterPresentation& character, SDL_Rect frame,
    int columns, int count, int size)
{
    using namespace ChessUiStyle;
    const auto stats = chessCharacterStats(character.stats);
    const int width = frame.w / columns, height = frame.h / ((count + columns - 1) / columns);
    for (int i = 0; i < count; ++i)
    {
        const int x = frame.x + i % columns * width, y = frame.y + i / columns * height;
        label(stats[i].first, size * 3 / 4, x, y, muted, width - 8);
        label(std::to_string(stats[i].second), size, x, y + size, i == 0 ? positive : i == 1 ? negative : i == 3 ? context : ink, width - 8);
    }
}

// 摘要與完整資料共用項目識別、圖示、數值及語意色彩；完整效果由各容器分頁。
inline void drawChessCharacterEntry(const ChessCharacterEntry& entry, SDL_Rect frame, int size, bool stacked = false)
{
    using namespace ChessUiStyle;
    const int art = std::min(frame.h - 8, size + 12);
    int x = frame.x + 4;
    if (!entry.imageGroup.empty() || entry.ultimate)
    {
        asset(entry.ultimate ? "chess-preparation" : entry.imageGroup,
            entry.ultimate ? iconId(Icon::Ultimate) : entry.imageId,
            {x, frame.y + 4, art, art});
        x += art + 8;
    }
    const auto color = entry.ultimate ? gold : entry.active ? positive : ink;
    const auto title = entry.title + (entry.ultimate && stacked ? " · 絕招" : "");
    const auto value = entry.combatStyle.empty() ? entry.value : entry.combatStyle + " · " + entry.value;
    const int valueWidth = Font::getTextDrawSize(value) * (size - 2) / 2 + 12;
    label(title, size, x, frame.y + 6, color,
        frame.x + frame.w - x - (stacked ? 8 : valueWidth));
    label(value, size - 2,
        stacked ? x : frame.x + frame.w - valueWidth,
        frame.y + (stacked ? size + 12 : 6), muted, stacked ? frame.x + frame.w - x : valueWidth);
}
struct ChessInspectorControl
{
    SDL_Rect frame{};
    std::string label;
    std::optional<ChessUiStyle::Icon> icon;
    bool selected{};
    int section = -1;
    int entry = -1;
    int pageStep{};
    int entryStep{};
    std::optional<int> comboId;
    std::string tooltip;
};

struct ChessInspectorContent
{
    std::vector<std::function<void()>> drawings;
    std::vector<ChessInspectorControl> controls;
};

inline void applyChessInspectorControl(ChessCharacterInspectorState& state, const ChessInspectorControl& control)
{
    if (control.entryStep) { state.entry += control.entryStep; state.page = 0; }
    else if (control.pageStep) state.page += control.pageStep;
    else if (control.section >= 0) state.showSection(control.section, control.entry);
    else state.expanded = false;
}

// 整備與圖鑑共用同一個檢視器；容器只負責輸入、焦點及自己的操作按鈕。
inline ChessInspectorContent composeChessCharacterInspector(const ChessCharacterPresentation& character,
    SDL_Rect frame, ChessCharacterInspectorState& state, bool compact)
{
    using namespace ChessUiStyle;
    ChessInspectorContent content;
    const int x = frame.x + 12, y = frame.y + 12, width = frame.w - 24;
    const bool tight = compact && frame.h < 540;
    const int size = tight ? 26 : compact ? 30 : 22;
    const int identityHeight = tight ? 60 : compact ? 100 : 78;
    const auto text = [&](std::string value, int px, int py, int fontSize, Color color, int w, int lines = 1) {
        content.drawings.push_back([=] { label(value, fontSize, px, py, color, w, lines); });
    };
    const auto entry = [&](ChessCharacterEntry value, SDL_Rect area, int fontSize, bool stacked) {
        content.drawings.push_back([=] { drawChessCharacterEntry(value, area, fontSize, stacked); });
    };
    content.drawings.push_back([=] {
        drawChessCharacterIdentity(character, {x, y, width, identityHeight}, size);
    });
    if (!state.expanded)
    {
        const int statsY = y + identityHeight + 6;
        content.drawings.push_back([=] {
            drawChessCharacterStats(character, {x, statsY, width, compact ? 72 : 56}, 4, 4, size);
        });
        int top = statsY + (compact ? 76 : 60);
        for (int i = 0; i < std::min(2, static_cast<int>(character.abilities.size())); ++i)
        {
            const int index = i == 1 ? static_cast<int>(character.abilities.size()) - 1 : 0;
            const SDL_Rect area{x, top + i * 44, width, 42};
            entry(character.abilities[index], area, compact ? 26 : 18, false);
            content.controls.push_back({.frame = area, .section = 1, .entry = index,
                .tooltip = character.abilities[index].tooltip});
        }
        top += 94;
        for (int slot = 0; slot < 2; ++slot)
        {
            const SDL_Rect area{x + slot * (width + 8) / 2, top, (width - 8) / 2, compact ? 62 : 54};
            entry(character.equipment[slot], area, compact ? 24 : 16, true);
            content.controls.push_back({.frame = area, .section = 2, .entry = slot});
        }
        top += compact ? 66 : 58;
        const int synergyColumns = tight ? 4 : 2;
        for (int i = 0; i < std::min(4, static_cast<int>(character.synergies.size())); ++i)
        {
            const SDL_Rect area{x + i % synergyColumns * (width + 8) / synergyColumns,
                top + i / synergyColumns * 52, (width + 8) / synergyColumns - 8, 50};
            entry(character.synergies[i], area, tight ? 22 : compact ? 24 : 16, true);
            content.controls.push_back({.frame = area, .comboId = character.synergies[i].id, .tooltip = character.synergies[i].tooltip});
        }
        const int actionHeight = compact ? 84 : 44;
        const int actionY = frame.y + frame.h - actionHeight - 12;
        content.controls.push_back({.frame = {x, actionY, width, actionHeight}, .label = "詳細資料", .icon = Icon::Right, .section = 0});
        return content;
    }

    const int tabY = y + identityHeight + 12, tabHeight = tight ? 56 : compact ? 76 : 44;
    constexpr std::array names{"屬性", "武學", "裝備", "羈絆"};
    for (int i = 0; i < 4; ++i)
        content.controls.push_back({.frame = {x + i * width / 4, tabY, width / 4 - 4, tabHeight},
            .label = names[i], .selected = state.section == i, .section = i,
            .comboId = i == 3 ? std::optional(character.synergies.empty() ? -1 : character.synergies.front().id) : std::nullopt});
    const int bodyY = tabY + tabHeight + 16;
    const int footerY = frame.y + frame.h - (tight ? 76 : compact ? 96 : 56);
    int pageCount = 1;
    if (state.section == 0)
    {
        const int statsHeight = compact ? 156 : 156;
        const int contextY = bodyY + statsHeight + 12;
        const bool contextPage = contextY + 68 > footerY;
        pageCount = contextPage ? 2 : 1;
        state.page = std::clamp(state.page, 0, pageCount - 1);
        if (!contextPage || state.page == 0)
            content.drawings.push_back([=] {
                drawChessCharacterStats(character, {x, bodyY, width, statsHeight}, 4, 8, size);
            });
        if (!contextPage || state.page == 1)
        {
            const int top = contextPage ? bodyY : contextY;
            text(character.growth, x, top, compact ? 23 : 15, muted, width, 2);
            text("擁有：" + character.owned, x, top + (compact ? 52 : 40), compact ? 23 : 15, muted, width);
        }
    }
    else
    {
        const std::span<const ChessCharacterEntry> entries = state.section == 1 ? std::span(character.abilities)
            : std::span<const ChessCharacterEntry>(character.equipment);
        const int textSize = compact ? 26 : 17;
        state.entry = std::clamp(state.entry, 0, std::max(0, static_cast<int>(entries.size()) - 1));
        if (entries.empty()) text("無", x, bodyY, textSize, muted, width);
        else
        {
            const auto& selected = entries[state.entry];
            const int navWidth = compact ? 64 : 40;
            const int headingHeight = tight ? 60 : compact ? 82 : 64;
            if (state.entry > 0) content.controls.push_back({.frame = {x, bodyY, navWidth, headingHeight},
                .icon = Icon::Left, .entryStep = -1, .tooltip = std::string("上一項") + names[state.section]});
            if (state.entry + 1 < static_cast<int>(entries.size())) content.controls.push_back({
                .frame = {x + width - navWidth, bodyY, navWidth, headingHeight},
                .icon = Icon::Right, .entryStep = 1, .tooltip = std::string("下一項") + names[state.section]});
            auto heading = selected;
            heading.value += std::format(" · {}/{}", state.entry + 1, entries.size());
            entry(heading, {x + navWidth + 4, bodyY, width - navWidth * 2 - 8, headingHeight}, tight ? 24 : compact ? 28 : 20, true);
            int textY = bodyY + headingHeight + 8;
            auto rows = selected.details;
            const int availableHeight = footerY - textY - 8;
            const auto layout = layoutPanelText(rows, width - 8, textSize);
            std::vector<std::vector<PanelTextPhysicalLine>> pages(1);
            int origin{};
            for (auto line : layout.lines)
            {
                if (!pages.back().empty() && line.y + line.fontSize - origin > availableHeight)
                {
                    pages.emplace_back();
                    origin = line.y;
                }
                line.y -= origin;
                pages.back().push_back(std::move(line));
            }
            pageCount = static_cast<int>(pages.size());
            state.page = std::clamp(state.page, 0, pageCount - 1);
            for (const auto& line : pages[state.page])
                text(line.text, x + line.indentPixels, textY + line.y,
                    line.fontSize, line.tone == PanelTextTone::Heading ? gold
                        : line.tone == PanelTextTone::Context ? context : ink, width - line.indentPixels);
        }
    }
    if (pageCount > 1)
    {
        const int buttonWidth = tight ? 64 : compact ? 84 : 44, height = tight ? 64 : compact ? 84 : 44;
        if (state.page > 0) content.controls.push_back({.frame = {x, footerY, buttonWidth, height}, .icon = Icon::Up, .pageStep = -1});
        text(std::format("{} / {}", state.page + 1, pageCount), x + buttonWidth + 8,
            footerY + (compact ? 28 : 13), compact ? 26 : 16, muted, compact ? 100 : 68);
        if (state.page + 1 < pageCount) content.controls.push_back({.frame = {x + buttonWidth + (compact ? 112 : 76), footerY, buttonWidth, height}, .icon = Icon::Down, .pageStep = 1});
    }
    content.controls.push_back({.frame = {x + width - (compact ? 164 : 112), footerY, compact ? 164 : 112, tight ? 64 : compact ? 84 : 44},
        .label = "收起", .icon = Icon::Collapse});
    return content;
}
} // namespace KysChess
