#include "Font.h"
#include "PotConv.h"

Font Font::font_;

Font::~Font()
{
    for (auto buffer : buffer_)
    {
        Engine::destroyTexture(buffer.second);
    }
}

void Font::draw(std::string text, int size, int x, int y, BP_Color color, uint8_t alpha)
{
    int p = 0;
    while (p < text.size())
    {
        uint16_t c = (uint8_t)text[p];
        p++;
        if (c > 128)
        {
            c += (uint8_t)text[p] * 256;
            p++;
        }
        auto index = calIndex(size, c);
        if (buffer_.count(index) == 0)
        {
            uint16_t c2[2] = { 0 };
            c2[0] = c;
            auto s = PotConv::cp936toutf8((char*)(c2));
            auto tex = Engine::getInstance()->createTextTexture(fontnamec_, s, size, { 255, 255, 255, 255 });
            buffer_[index] = tex;
        }
        auto tex = buffer_[index];
        int w, h;
        Engine::getInstance()->queryTexture(tex, &w, &h);
        Engine::getInstance()->setColor(tex, color, alpha);
        Engine::getInstance()->renderCopy(tex, x, y, w, h);
        x += w;
    }
}
