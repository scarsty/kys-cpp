#include "Font.h"
#include "PotConv.h"

Font Font::font_;

void Font::splitText(std::string text)
{
    int p = 0;
    while (p < text.size())
    {
        uint16_t c = (uint8_t)text[p];
        p++;
        if (c > 128)
        {
            c = (uint8_t)text[p] * 256;
            p++;
        }
    }
}

void Font::draw(std::string text, int size, int x, int y, BP_Color c, uint8_t alpha)
{
    Engine::getInstance()->drawText(fontnamec_, PotConv::cp936toutf8(text), size, x, y, alpha, BP_ALIGN_LEFT, { 255, 255, 255, 255 });
}
