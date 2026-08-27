#pragma once

#include "DisplayText.h"
#include "Font.h"

#include <cassert>
#include <string>
#include <vector>

namespace KysChess
{

struct PanelTextCursor
{
    Font* font;
    int x;
    int y;

    void line(const std::string& text, int fontSize, Color color, int extraSpacing = 4, int indent = 0)
    {
        font->draw(text, fontSize, x + indent, y, color);
        y += fontSize + extraSpacing;
    }

    void skip(int spacing)
    {
        y += spacing;
    }
};

struct LabelValueColumn
{
    Font* font;
    int fontSize;
    int labelX;
    int valueX;
    Color labelColor;

    void line(int rowY, const char* label, const std::string& value, Color valueColor) const
    {
        font->draw(label, fontSize, labelX, rowY, labelColor);
        font->draw(value, fontSize, valueX, rowY, valueColor);
    }
};

void showChessMessage(const std::string& text, int fontSize = 32);
void playChessUpgradeSound();
int getRandomChessMusic();
int getRandomBattleMusic();
bool isChessSceneMusic(int musicId);
inline int displayTextUnitsForPixelWidth(int fontSize, int pixelWidth, int indent = 0)
{
    assert(fontSize > 0);
    assert(pixelWidth - indent >= fontSize);
    return (pixelWidth - indent) * 2 / fontSize;
}

}    // namespace KysChess
