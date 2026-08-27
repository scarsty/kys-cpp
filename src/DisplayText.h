#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace KysChess
{

inline int utf8DisplayTextCharacterLength(unsigned char value)
{
    if (value < 0x80) return 1;
    if ((value & 0xE0) == 0xC0) return 2;
    if ((value & 0xF0) == 0xE0) return 3;
    if ((value & 0xF8) == 0xF0) return 4;
    return 1;
}

inline int utf8DisplayTextCharacterWidth(unsigned char value)
{
    if (value < 0x80) return 1;
    return utf8DisplayTextCharacterLength(value) >= 3 ? 2 : 1;
}

inline int displayTextWidth(std::string_view text)
{
    int result = 0;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto value = static_cast<unsigned char>(text[index]);
        const int characterLength = utf8DisplayTextCharacterLength(value);
        if (value >= 0x80 && characterLength == 1)
        {
            ++index;
            continue;
        }
        result += utf8DisplayTextCharacterWidth(value);
        index += characterLength;
    }
    return result;
}

inline std::vector<std::string> wrapDisplayText(const std::string& text, int maximumWidth)
{
    if (text.empty() || maximumWidth <= 0)
    {
        return {};
    }

    struct Glyph
    {
        std::string text;
        int width{};
        bool preferredBreakAfter = false;
    };
    std::vector<Glyph> glyphs;
    for (std::size_t index = 0; index < text.size();)
    {
        const auto value = static_cast<unsigned char>(text[index]);
        const int characterLength = utf8DisplayTextCharacterLength(value);
        auto glyph = text.substr(index, characterLength);
        index += characterLength;
        const bool preferredBreak = glyph == " " || glyph == ":" || glyph == "："
            || glyph == "(" || glyph == ")" || glyph == "（" || glyph == "）"
            || glyph == "·" || glyph == "/";
        glyphs.push_back({
            .text = std::move(glyph),
            .width = utf8DisplayTextCharacterWidth(value),
            .preferredBreakAfter = preferredBreak,
        });
    }

    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < glyphs.size())
    {
        int width = 0;
        std::size_t end = start;
        std::size_t preferredBreak = start;
        while (end < glyphs.size() && width + glyphs[end].width <= maximumWidth)
        {
            width += glyphs[end].width;
            if (glyphs[end].preferredBreakAfter)
            {
                preferredBreak = end + 1;
            }
            ++end;
        }

        std::size_t lineEnd = end;
        if (end < glyphs.size() && preferredBreak > start)
        {
            lineEnd = preferredBreak;
        }
        if (lineEnd == start)
        {
            lineEnd = std::min(start + 1, glyphs.size());
        }

        std::string line;
        for (std::size_t index = start; index < lineEnd; ++index)
        {
            line += glyphs[index].text;
        }
        while (!line.empty() && line.back() == ' ')
        {
            line.pop_back();
        }
        lines.push_back(std::move(line));

        start = lineEnd;
        while (start < glyphs.size() && glyphs[start].text == " ")
        {
            ++start;
        }
    }
    return lines;
}

}  // namespace KysChess
