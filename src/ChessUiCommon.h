#pragma once

#include "Font.h"

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
std::vector<std::string> wrapDisplayText(const std::string& text, int maxWidth);

}    // namespace KysChess
