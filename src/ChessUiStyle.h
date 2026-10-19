#pragma once

#include "Engine.h"
#include "Font.h"
#include "DisplayText.h"
#include "TextureManager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>

namespace KysChess::ChessUiStyle
{
inline constexpr Color ink{236, 232, 214, 255};
inline constexpr Color muted{183, 179, 166, 255};
inline constexpr Color gold{221, 200, 141, 255};
inline constexpr Color line{177, 158, 120, 40};
inline constexpr Color panelFill{25, 23, 22, 180};
inline constexpr Color activeFill{115, 98, 67, 90};
inline constexpr Color actionFill{88, 75, 53, 220};
inline constexpr Color actionHoverFill{132, 111, 69, 235};
inline constexpr Color disabledFill{69, 62, 49, 190};
inline constexpr Color positive{184, 204, 164, 255};
inline constexpr Color negative{218, 159, 146, 255};
inline constexpr Color context{158, 201, 213, 255};
inline constexpr Color exclusive{210, 182, 229, 255};
inline constexpr Color canvasScrim{18, 17, 16, 48};
inline constexpr Color quietFill{12, 11, 10, 48};
inline constexpr Color modalFill = panelFill;
inline constexpr Color focusLine{236, 232, 214, 255};
inline constexpr Color darkLabel{32, 27, 20, 255};

namespace Spacing
{
inline constexpr int gutter = 16;
inline constexpr int gap = 12;
inline constexpr int padding = 24;
inline constexpr int panelCorner = 6;
inline constexpr int controlCorner = 4;
}

namespace Type
{
inline constexpr int pageTitle = 24;
inline constexpr int sectionTitle = 22;
inline constexpr int body = 18;
inline constexpr int metadata = 16;
}

enum class Icon : int
{
    Coin, Bag, Armor, Seal, Left, Right, Up, Down, Link, Eye, Refresh, Lock,
    Collapse, Sword, More, Sliders, Mountain, Spark, Book, Cultivation, Close, Ultimate,
    Character, Formation, Load, Save, Search, Transfer, Home,
    Play, Pause, Speed, CameraFollow, CameraFree, View2D, View3D, Flag, List, Map, Ban,
    Dice,
};

inline int iconId(Icon icon) { return static_cast<int>(icon); }

struct ControlState
{
    bool hovered{};
    bool selected{};
    bool focused{};
    bool disabled{};
    bool primary{};
    bool action{};
    bool destructive{};
};

inline void controlSurface(SDL_Rect frame, ControlState state)
{
    auto* engine = Engine::getInstance();
    const Color fill = state.disabled ? disabledFill : state.primary ? gold
        : state.action ? (state.hovered || state.selected ? actionHoverFill : actionFill)
        : state.selected || state.hovered ? activeFill : quietFill;
    engine->fillRoundedRect(fill, frame.x, frame.y, frame.w, frame.h, Spacing::controlCorner);
    if (state.focused)
    {
        engine->drawRoundedRect(focusLine, frame.x, frame.y, frame.w, frame.h, Spacing::controlCorner);
        engine->drawRoundedRect(focusLine, frame.x + 1, frame.y + 1,
            frame.w - 2, frame.h - 2, Spacing::controlCorner);
    }
    if (state.selected && !state.primary)
        engine->fillColor(gold, frame.x + 8, frame.y + frame.h - 3, frame.w - 16, 3);
    if (state.destructive)
        engine->fillColor(negative, frame.x + 8, frame.y + frame.h - 3, frame.w - 16, 2);
}

inline Color controlText(ControlState state)
{
    return state.disabled ? muted : state.primary ? darkLabel : state.destructive ? negative : ink;
}

inline void attentionOutline(SDL_Rect frame, Color color, int thickness = 3)
{
    for (int inset = 0; inset < thickness; ++inset)
        Engine::getInstance()->drawAnimatedRoundedRect(color, frame.x + inset, frame.y + inset,
            frame.w - inset * 2, frame.h - inset * 2, std::max(0, 4 - inset),
            std::fmod(Engine::getTicks() * 0.0005, 1.0));
}

inline void panel(int x, int y, int w, int h, Color fill = panelFill)
{
    Engine::getInstance()->fillRoundedRect(fill, x, y, w, h, Spacing::panelCorner);
}

inline std::vector<std::string> wrapLines(const std::string& value, int width)
{
    std::vector<std::string> rows;
    if (value.empty()) return rows;
    for (std::size_t start = 0; start <= value.size();)
    {
        const auto end = value.find('\n', start);
        const auto paragraph = value.substr(start, end == std::string::npos ? end : end - start);
        const auto wrapped = wrapDisplayText(paragraph, width, true);
        if (wrapped.empty()) rows.emplace_back();
        else rows.insert(rows.end(), wrapped.begin(), wrapped.end());
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return rows;
}

inline void label(const std::string& value, int size, int x, int y, Color color, int width, int lines = 1, int spacing = 2)
{
    const auto rows = wrapLines(value, std::max(2, width * 2 / size));
    for (int i = 0; i < std::min(lines, static_cast<int>(rows.size())); ++i)
    {
        auto row = rows[i];
        if (i + 1 == lines && static_cast<int>(rows.size()) > lines)
        {
            if (!row.empty()) row = wrapDisplayText(row, std::max(2, width * 2 / size - 2), true).front();
            row += "…";
        }
        Font::getInstance()->draw(row, size, x, y + i * (size + spacing), color);
    }
}

inline void drawUtilityIcon(Icon symbol, SDL_Rect frame, Color color)
{
    auto* engine = Engine::getInstance();
    const int unit = std::min(frame.w, frame.h);
    const int x = frame.x + (frame.w - unit) / 2, y = frame.y + (frame.h - unit) / 2;
    const auto box = [&](int left, int top, int width, int height) {
        engine->fillColor(color, x + left * unit / 24, y + top * unit / 24,
            std::max(1, width * unit / 24), std::max(1, height * unit / 24));
    };
    const auto line = [&](int x1, int y1, int x2, int y2) {
        for (int offset = 0; offset < std::max(1, unit / 16); ++offset)
            engine->drawLine(color, {float(x + x1 * unit / 24), float(y + y1 * unit / 24 + offset)},
                {float(x + x2 * unit / 24), float(y + y2 * unit / 24 + offset)});
    };
    const auto triangle = [&](int left, int width) {
        line(left, 4, left + width, 12);
        line(left + width, 12, left, 20);
        line(left, 20, left, 4);
    };
    switch (symbol)
    {
    case Icon::Dice:
        line(3, 3, 21, 3); line(21, 3, 21, 21); line(21, 21, 3, 21); line(3, 21, 3, 3);
        box(6, 6, 3, 3); box(15, 6, 3, 3); box(10, 10, 4, 4);
        box(6, 15, 3, 3); box(15, 15, 3, 3); break;
    case Icon::Play: triangle(6, 14); break;
    case Icon::Speed: triangle(3, 9); triangle(13, 9); break;
    case Icon::Pause: box(5, 4, 5, 16); box(14, 4, 5, 16); break;
    case Icon::Flag: box(5, 3, 2, 19); box(7, 4, 12, 8); break;
    case Icon::List:
        for (int row = 0; row < 3; ++row) { box(3, 4 + row * 7, 3, 3); box(9, 4 + row * 7, 12, 3); }
        break;
    case Icon::Map:
        for (int i = 0; i < 4; ++i) { line(3 + i * 6, 3, 3 + i * 6, 21); line(3, 3 + i * 6, 21, 3 + i * 6); }
        box(10, 10, 4, 4); break;
    case Icon::Ban:
        line(8, 3, 16, 3); line(16, 3, 21, 8); line(21, 8, 21, 16);
        line(21, 16, 16, 21); line(16, 21, 8, 21); line(8, 21, 3, 16);
        line(3, 16, 3, 8); line(3, 8, 8, 3); line(5, 5, 19, 19); break;
    case Icon::CameraFollow:
        line(7, 7, 17, 7); line(17, 7, 17, 17); line(17, 17, 7, 17); line(7, 17, 7, 7);
        box(11, 2, 2, 7); box(11, 15, 2, 7); box(2, 11, 7, 2); box(15, 11, 7, 2); break;
    case Icon::CameraFree:
        for (int corner : {3, 15}) { line(corner, 3, corner + 6, 3); line(corner, 21, corner + 6, 21); }
        line(3, 3, 3, 9); line(21, 3, 21, 9); line(3, 15, 3, 21); line(21, 15, 21, 21); break;
    case Icon::View2D:
        line(3, 12, 12, 5); line(12, 5, 21, 12); line(21, 12, 12, 19); line(12, 19, 3, 12); break;
    case Icon::View3D:
        line(3, 7, 12, 2); line(12, 2, 21, 7); line(21, 7, 12, 12); line(12, 12, 3, 7);
        line(3, 7, 3, 17); line(21, 7, 21, 17); line(12, 12, 12, 22); line(3, 17, 12, 22); line(12, 22, 21, 17); break;
    default: assert(false); break;
    }
}

inline void asset(const std::string& group, int id, SDL_Rect frame, bool bottomAligned = false,
    std::optional<Color> tintOverride = std::nullopt)
{
    if (id < 0) return;
    const Color tint = tintOverride.value_or(group == "chess-preparation" || group == "chess-synergies" ? gold : Color{255, 255, 255, 255});
    if (group == "chess-preparation" && id >= iconId(Icon::Play))
    {
        drawUtilityIcon(static_cast<Icon>(id), frame, tint);
        return;
    }
    if (group == "chess-preparation" && id == iconId(Icon::Home))
    {
        const int unit = std::min(frame.w, frame.h);
        const int x = frame.x + (frame.w - unit) / 2, y = frame.y + (frame.h - unit) / 2;
        auto* engine = Engine::getInstance();
        for (int row = 0; row < unit / 3; ++row)
            engine->fillColor(tint, x + unit / 2 - row, y + unit / 8 + row, row * 2 + 1, 1);
        engine->fillColor(tint, x + unit / 4, y + unit * 5 / 12, unit / 2, unit * 5 / 12);
        engine->fillColor(darkLabel, x + unit * 5 / 12, y + unit * 7 / 12, std::max(2, unit / 6), unit / 4);
        return;
    }
    if (group == "chess-preparation" && id == iconId(Icon::Formation))
    {
        // 編隊以三枚棋子的剪影表示，與其餘工具圖示共用留白，不內嵌按鈕外框。
        const int unit = std::min(frame.w, frame.h);
        const int left = frame.x + (frame.w - unit) / 2, top = frame.y + (frame.h - unit) / 2;
        for (int i = 0; i < 3; ++i)
        {
            const int x = left + unit * (20 + i * 30) / 100;
            const int y = top + unit * (48 - i * 16) / 100;
            const int head = std::max(3, unit * 14 / 100);
            Engine::getInstance()->fillRoundedRect(tint, x - head / 2, y, head, head, head / 2);
            Engine::getInstance()->fillRoundedRect(tint, x - head / 2, y + head,
                head, unit * 22 / 100, std::max(1, head / 4));
            Engine::getInstance()->fillRoundedRect(tint, x - unit * 11 / 100, y + unit * 34 / 100,
                unit * 22 / 100, std::max(2, unit * 6 / 100), 1);
        }
        return;
    }
    auto* texture = TextureManager::getInstance()->getTexture(group, id);
    if (!texture) return;
    texture->load();
    if (texture->w <= 0 || texture->h <= 0) return;
    const double scale = std::min(double(frame.w) / texture->w, double(frame.h) / texture->h);
    const int width = static_cast<int>(texture->w * scale);
    const int height = static_cast<int>(texture->h * scale);
    TextureManager::getInstance()->renderTexture(texture,
        frame.x + (frame.w - width) / 2 + texture->dx,
        frame.y + (frame.h - height) / (bottomAligned ? 1 : 2) + texture->dy, {tint, 255}, width, height);
}

inline void controlLabel(SDL_Rect frame, const std::string& text, int size, Color color,
    std::optional<Icon> symbol = std::nullopt, Color iconColor = gold)
{
    const int iconSize = size >= 28 ? 36 : 24;
    const int inset = symbol && !text.empty() ? iconSize + 8 : 0;
    const int labelWidth = Font::getTextDrawSize(text) * size / 2;
    const int contentWidth = text.empty() && symbol ? iconSize : inset + labelWidth;
    const int x = frame.x + std::max(8, (frame.w - contentWidth) / 2);
    if (symbol) asset("chess-preparation", iconId(*symbol),
        {x, frame.y + (frame.h - iconSize) / 2, iconSize, iconSize}, false, iconColor);
    if (!text.empty()) label(text, size, x + inset, frame.y + (frame.h - size) / 2,
        color, frame.w - (x - frame.x) - inset - 8);
}

inline void page(const std::string& title, const std::string& hint = {}, int headingSize = Type::pageTitle)
{
    auto* engine = Engine::getInstance();
    const int width = engine->getUIWidth();
    const int height = engine->getUIHeight();
    const bool compact = headingSize >= 30;
    panel(16, 16, width - 32, compact ? 84 : 52);
    const int titleSize = std::min(headingSize, (width - 288) * 2 / std::max(1, Font::getTextDrawSize(title)));
    Font::getInstance()->draw(title, titleSize, 32, compact ? 38 : 30, gold);
    if (!hint.empty())
        Font::getInstance()->draw(hint, 18, 96, height - 40, muted);
}

struct ActionFooter
{
    SDL_Rect leading{};
    SDL_Rect trailing{};
};

inline ActionFooter actionFooter(SDL_Rect frame, bool compact, int actionWidth = 0, int controlHeight = 0)
{
    const int padding = compact ? 24 : 16;
    const int height = controlHeight > 0 ? controlHeight : compact ? 84 : 52;
    const int width = actionWidth > 0 ? actionWidth : compact ? 304 : 224;
    const int y = frame.y + frame.h - padding - height;
    return {{frame.x + padding, y, width, height},
        {frame.x + frame.w - padding - width, y, width, height}};
}

inline SDL_Rect modalFrame(bool compact)
{
    return compact ? SDL_Rect{120, 154, 1040, 412} : SDL_Rect{328, 234, 624, 252};
}

inline void modalShell(SDL_Rect frame, const std::string& title, bool compact)
{
    panel(frame.x, frame.y, frame.w, frame.h, modalFill);
    label(title, compact ? 32 : Type::pageTitle, frame.x + 32, frame.y + 28, gold, frame.w - 64);
}
} // namespace KysChess::ChessUiStyle
