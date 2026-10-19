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
    if (character.banned) bannedOutline({frame.x, frame.y, portrait, portrait});
    const int x = frame.x + portrait + 12;
    const int width = frame.w - portrait - 12 - (character.growthSummary ? 68 : 0);
    label(character.name, size, x, frame.y + 2, ink, width);
    const bool showContext = character.context != "商店預覽" && character.context != "圖鑑預覽";
    label((character.showCost ? std::format("{}費 · {}星", character.cost, character.star)
        : std::format("{}星", character.star))
        + (frame.h < 78 && showContext ? " · " + character.context : ""), size * 3 / 4,
        x, frame.y + size + 10, chessPieceTierColor(character.cost), width);
    if (showContext && frame.h >= 78)
        label(character.context, size * 3 / 4, x, frame.y + size * 2 + 10,
            character.banned ? negative : muted, width);
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
    const auto& value = entry.value;
    const int styleSize = std::max(24, size);
    const int styleWidth = entry.combatOperation ? styleSize + 4 : 0;
    const int valueWidth = Font::getTextDrawSize(value) * (size - 2) / 2 + 12 + styleWidth;
    label(title, size, x, frame.y + 6, color,
        frame.x + frame.w - x - (stacked ? 8 : valueWidth));
    const int valueX = stacked ? x : frame.x + frame.w - valueWidth;
    const int valueY = frame.y + (stacked ? size + 12 : 6);
    if (entry.combatOperation)
    {
        const auto symbol = *entry.combatOperation == Battle::BattleOperationType::Melee ? Icon::LightStrike
            : *entry.combatOperation == Battle::BattleOperationType::TrackingProjectile ? Icon::HeavyStrike : Icon::RangedStrike;
        asset("chess-preparation", iconId(symbol), {valueX, valueY + (size - 2 - styleSize) / 2, styleSize, styleSize}, false, muted);
    }
    label(value, size - 2, valueX + styleWidth, valueY, muted,
        (stacked ? frame.x + frame.w - x : valueWidth) - styleWidth);
}
struct ChessInspectorControl
{
    SDL_Rect frame{};
    std::string label;
    std::optional<ChessUiStyle::Icon> icon;
    bool selected{};
    bool action{};
    int section = -1;
    int entry = -1;
    int pageStep{};
    int entryStep{};
    std::optional<int> comboId;
    std::string tooltip;
    std::optional<int> equipmentSlot;
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

// 整備、圖鑑與遠征共用同一個檢視器；容器只負責輸入、焦點及自己的操作按鈕。
inline ChessInspectorContent composeChessCharacterInspector(const ChessCharacterPresentation& character,
    SDL_Rect frame, ChessCharacterInspectorState& state,
    std::optional<SDL_Rect> navigationFrame = std::nullopt)
{
    using namespace ChessUiStyle;
    ChessInspectorContent content;
    const int x = frame.x + 12, y = frame.y + 12, width = frame.w - 24;
    const int size = 22;
    const int identityHeight = 78;
    const auto text = [&](std::string value, int px, int py, int fontSize, Color color, int w, int lines = 1) {
        content.drawings.push_back([=] { label(value, fontSize, px, py, color, w, lines); });
    };
    const auto entry = [&](ChessCharacterEntry value, SDL_Rect area, int fontSize, bool stacked) {
        content.drawings.push_back([=] { drawChessCharacterEntry(value, area, fontSize, stacked); });
    };
    content.drawings.push_back([=] {
        drawChessCharacterIdentity(character, {x, y, width, identityHeight}, size);
    });
    if (character.growthSummary)
    {
        const auto growth = *character.growthSummary;
        for (int i = 0; i < 2; ++i)
        {
            const SDL_Rect area{x + width - 64, y + i * 32, 64, 30};
            const auto symbol = i == 0 ? Icon::Victory : Icon::Luck;
            const auto value = std::to_string(i == 0 ? growth.fightsWon : growth.luckStacks);
            content.drawings.push_back([=] {
                asset("chess-preparation", iconId(symbol), {area.x, area.y + 3, 24, 24}, false, gold);
                label(value, 16, area.x + 28, area.y + 7, ink, area.w - 28);
            });
            content.controls.push_back({.frame = area, .section = 0,
                .tooltip = i == 0 ? std::format("勝場 {} · 已計入屬性成長", growth.fightsWon)
                    : std::format("賭運 {} 層 · 致命傷害時免死機率 {}%", growth.luckStacks, growth.luckChance)});
        }
    }
    if (!state.expanded)
    {
        const int statsY = y + identityHeight + 6;
        content.drawings.push_back([=] {
            drawChessCharacterStats(character, {x, statsY, width, 56}, 4, 4, size);
        });
        int top = statsY + 60;
        for (int i = 0; i < std::min(2, static_cast<int>(character.abilities.size())); ++i)
        {
            const int index = i == 1 ? static_cast<int>(character.abilities.size()) - 1 : 0;
            const SDL_Rect area{x, top + i * 44, width, 42};
            entry(character.abilities[index], area, 18, false);
            content.controls.push_back({.frame = area, .section = 1, .entry = index,
                .tooltip = character.abilities[index].tooltip});
        }
        top += 94;
        for (int slot = 0; slot < 2; ++slot)
        {
            const SDL_Rect area{x + slot * (width + 8) / 2, top, (width - 8) / 2, 54};
            auto equipment = character.equipment[slot];
            const bool pick = character.equipmentEditable && equipment.id < 0;
            if (equipment.id < 0)
                content.drawings.push_back([=] {
                    controlLabel(area, character.equipmentEditable ? equipment.title : "—", 16,
                        muted, slot == 0 ? Icon::Sword : Icon::Armor, muted);
                });
            else entry(equipment, area, 16, true);
            content.controls.push_back({.frame = area, .section = 2, .entry = slot,
                .tooltip = pick ? (slot == 0 ? "選擇武器" : "選擇防具")
                    : equipment.id < 0 ? (slot == 0 ? "未裝備武器" : "未裝備防具") : "查看裝備效果",
                .equipmentSlot = pick ? std::optional(slot) : std::nullopt});
        }
        top += 58;
        const int synergyColumns = 2;
        for (int i = 0; i < std::min(4, static_cast<int>(character.synergies.size())); ++i)
        {
            const SDL_Rect area{x + i % synergyColumns * (width + 8) / synergyColumns,
                top + i / synergyColumns * 52, (width + 8) / synergyColumns - 8, 50};
            entry(character.synergies[i], area, 16, true);
            content.controls.push_back({.frame = area, .section = 3, .entry = i});
        }
        const int actionHeight = 44;
        const int actionY = frame.y + frame.h - actionHeight - 12;
        content.controls.push_back({.frame = navigationFrame.value_or(SDL_Rect{x, actionY, width, actionHeight}),
            .label = "詳細資料", .icon = Icon::List, .action = true, .section = 0});
        return content;
    }

    const int tabY = y + identityHeight + 12, tabHeight = 44;
    constexpr std::array names{"屬性", "武學", "裝備", "羈絆"};
    for (int i = 0; i < 4; ++i)
        content.controls.push_back({.frame = {x + i * width / 4, tabY, width / 4 - 4, tabHeight},
            .label = names[i], .selected = state.section == i, .section = i});
    const int bodyY = tabY + tabHeight + 16;
    const int footerY = frame.y + frame.h - 56;
    int pageCount = 1;
    if (state.section == 0)
    {
        const int statsHeight = 132;
        const int contextY = bodyY + statsHeight + 12;
        std::vector<std::pair<std::string, int>> gains;
        if (character.growthSummary)
            for (const auto& stat : chessCharacterStats(character.growthSummary->stats))
                if (stat.second != 0) gains.push_back(stat);
        const int growthRowHeight = 34;
        const int contextHeight = character.growthSummary
            ? 28 + std::max(1, (static_cast<int>(gains.size()) + 3) / 4) * growthRowHeight
            : character.statsNote.empty() ? 0 : 48;
        const bool contextPage = contextY + contextHeight > footerY;
        pageCount = contextPage ? 2 : 1;
        state.page = std::clamp(state.page, 0, pageCount - 1);
        if (!contextPage || state.page == 0)
            content.drawings.push_back([=] {
                drawChessCharacterStats(character, {x, bodyY, width, statsHeight}, 4, 8, size);
            });
        if (!contextPage || state.page == 1)
        {
            const int top = contextPage ? bodyY : contextY;
            if (character.growthSummary)
            {
                text("勝場加成", x, top, 18, gold, width);
                if (gains.empty()) text("尚無加成", x, top + 28, 16, muted, width);
                for (int i = 0; i < static_cast<int>(gains.size()); ++i)
                {
                    const int px = x + (i % 4) * width / 4, py = top + 28 + (i / 4) * growthRowHeight;
                    text(gains[i].first, px, py, 14, muted, width / 4 - 8);
                    text(std::format("{:+}", gains[i].second), px, py + 16,
                        16, positive, width / 4 - 8);
                }
            }
            else text(character.statsNote, x, top, 15, muted, width, 2);
        }
    }
    else
    {
        const std::span<const ChessCharacterEntry> entries = state.section == 1 ? std::span(character.abilities)
            : state.section == 3 ? std::span(character.synergies) : std::span<const ChessCharacterEntry>(character.equipment);
        const int textSize = 17;
        state.entry = std::clamp(state.entry, 0, std::max(0, static_cast<int>(entries.size()) - 1));
        if (entries.empty()) text("無", x, bodyY, textSize, muted, width);
        else
        {
            const auto& selected = entries[state.entry];
            const int navWidth = 40;
            const int headingHeight = 64;
            if (state.entry > 0) content.controls.push_back({.frame = {x, bodyY, navWidth, headingHeight},
                .icon = Icon::Left, .entryStep = -1, .tooltip = std::string("上一項") + names[state.section]});
            if (state.entry + 1 < static_cast<int>(entries.size())) content.controls.push_back({
                .frame = {x + width - navWidth, bodyY, navWidth, headingHeight},
                .icon = Icon::Right, .entryStep = 1, .tooltip = std::string("下一項") + names[state.section]});
            entry(selected, {x + navWidth + 4, bodyY, width - navWidth * 2 - 8, headingHeight}, 20, true);
            int textY = bodyY + headingHeight + 8;
            auto rows = selected.details;
            const int availableHeight = footerY - textY - 8
                - ((state.section == 2 && character.equipmentEditable) || state.section == 3 ? 52 : 0);
            const auto pages = paginatePanelText(rows, width - 8, textSize, availableHeight);
            pageCount = static_cast<int>(pages.size());
            state.page = std::clamp(state.page, 0, pageCount - 1);
            for (const auto& line : pages[state.page])
                text(line.text, x + line.indentPixels, textY + line.y,
                    line.fontSize, panelTextColor(line.tone), width - line.indentPixels);
            if (state.section == 2 && character.equipmentEditable)
                content.controls.push_back({.frame = {x, footerY - 52, width, 44},
                    .label = selected.id < 0 ? "選擇裝備" : "更換裝備", .icon = Icon::Bag,
                    .action = true, .equipmentSlot = state.entry});
            if (state.section == 3)
                content.controls.push_back({.frame = {x, footerY - 52, width, 44},
                    .label = "羈絆一覽", .icon = Icon::Link, .action = true, .comboId = selected.id});
        }
    }
    if (pageCount > 1)
    {
        const int buttonWidth = 44, height = 44;
        if (state.page > 0) content.controls.push_back({.frame = {x, footerY, buttonWidth, height}, .icon = Icon::Up, .pageStep = -1});
        text(std::format("{} / {}", state.page + 1, pageCount), x + buttonWidth + 8,
            footerY + 13, 16, muted, 68);
        if (state.page + 1 < pageCount) content.controls.push_back({.frame = {x + buttonWidth + 76, footerY, buttonWidth, height}, .icon = Icon::Down, .pageStep = 1});
    }
    content.controls.push_back({.frame = navigationFrame.value_or(SDL_Rect{
            x + width - 148, footerY, 148, 44}),
        .label = "角色摘要", .icon = Icon::Collapse, .action = true});
    return content;
}
} // namespace KysChess
