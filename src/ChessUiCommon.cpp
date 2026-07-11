#include "ChessUiCommon.h"

#include "Audio.h"
#include "Engine.h"
#include "Font.h"
#include "TextBox.h"

#include <array>
#include <format>
#include <vector>

namespace KysChess
{

namespace
{

constexpr std::array<int, 20> kChessMusicIds = {0, 1, 2, 8, 9, 13, 14, 16, 18, 19, 20, 21, 23, 24, 25, 31, 59, 60, 61, 62};
constexpr std::array<int, 17> kBattleMusicIds = {3, 4, 5, 6, 7, 10, 17, 48, 53, 55, 70, 75, 79, 80, 84, 88, 90};

template <size_t N>
int pickRandomMusic(const std::array<int, N>& musicIds)
{
    static int lastPlayed = -1;

    int idx = rand() % musicIds.size();
    if (musicIds.size() > 1 && musicIds[idx] == lastPlayed)
    {
        idx = (idx + 1) % musicIds.size();
    }
    lastPlayed = musicIds[idx];
    return musicIds[idx];
}

template <size_t N>
bool containsMusicId(const std::array<int, N>& musicIds, int musicId)
{
    for (int value : musicIds)
    {
        if (value == musicId)
        {
            return true;
        }
    }
    return false;
}

struct DisplayGlyph
{
    std::string text;
    int width;
    bool preferredBreakAfter;
};

DisplayGlyph readDisplayGlyph(const std::string& text, size_t index)
{
    size_t charSize = Font::utf8CharLength(static_cast<unsigned char>(text[index]));
    std::string glyph = text.substr(index, charSize);
    int charWidth = Font::getTextDrawSize(glyph);
    bool preferredBreak = glyph == " " || glyph == ":" || glyph == "：" || glyph == "(" || glyph == ")" || glyph == "（" || glyph == "）" || glyph == "·" || glyph == "/";
    return {glyph, charWidth, preferredBreak};
}

std::string trimLine(std::string line)
{
    while (!line.empty() && line.back() == ' ')
    {
        line.pop_back();
    }
    return line;
}

}    // namespace

ChessSelectorPresenter& chessPresenter()
{
    static ChessSelectorPresenter value;
    return value;
}

void showChessMessage(const std::string& text, int fontSize)
{
    auto box = std::make_shared<DismissibleTextBox>();
    box->setText(text);
    box->setFontSize(fontSize);
    box->runCentered(Engine::getInstance()->getUIHeight() / 2);
}

void playChessUpgradeSound()
{
    Audio::getInstance()->playESound(72);
}

int getRandomChessMusic()
{
    return pickRandomMusic(kChessMusicIds);
}

int getRandomBattleMusic()
{
    return pickRandomMusic(kBattleMusicIds);
}

bool isChessSceneMusic(int musicId)
{
    return containsMusicId(kChessMusicIds, musicId);
}

ChessManager makeChessManager(ChessRoster& roster, ChessEquipmentInventory& equipmentInventory, ChessEconomy& economy)
{
    return ChessManager(roster, equipmentInventory, economy);
}

ChessManager makeChessManager(const ChessSelectorServices& services)
{
    return makeChessManager(services.roster, services.equipmentInventory, services.economy);
}


std::vector<std::string> wrapDisplayText(const std::string& text, int maxWidth)
{
    if (text.empty() || maxWidth <= 0)
    {
        return {};
    }

    std::vector<DisplayGlyph> glyphs;
    for (size_t index = 0; index < text.size();)
    {
        auto glyph = readDisplayGlyph(text, index);
        index += glyph.text.size();
        glyphs.push_back(std::move(glyph));
    }

    std::vector<std::string> lines;
    size_t start = 0;
    while (start < glyphs.size())
    {
        int width = 0;
        size_t end = start;
        size_t preferredBreak = start;
        while (end < glyphs.size() && width + glyphs[end].width <= maxWidth)
        {
            width += glyphs[end].width;
            if (glyphs[end].preferredBreakAfter)
            {
                preferredBreak = end + 1;
            }
            ++end;
        }

        size_t lineEnd = end;
        if (end < glyphs.size() && preferredBreak > start)
        {
            lineEnd = preferredBreak;
        }
        if (lineEnd == start)
        {
            lineEnd = std::min(start + 1, glyphs.size());
        }

        std::string line;
        for (size_t i = start; i < lineEnd; ++i)
        {
            line += glyphs[i].text;
        }
        lines.push_back(trimLine(std::move(line)));

        start = lineEnd;
        while (start < glyphs.size() && glyphs[start].text == " ")
        {
            ++start;
        }
    }
    return lines;
}

}    // namespace KysChess
