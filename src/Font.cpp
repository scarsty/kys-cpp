#include "Font.h"
#include "PotConv.h"

Font Font::font_;

void Font::draw(std::string text, int size, int x, int y, BP_Color c, uint8_t alpha)
{
    Engine::getInstance()->drawText("../game/font/chinese.ttf", PotConv::cp936toutf8(text), size, x, y, alpha, BP_ALIGN_LEFT, { 255, 255, 255, 255 });
}
