#include "Font.h"
#include "PotConv.h"
#include "TextureManager.h"

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
        int w = size, h = size;
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
            BP_Texture* tex;
            if (c > 128)
            {
                tex = Engine::getInstance()->createTextTexture(fontnamec_, s, size, { 255, 255, 255, 255 });
            }
            else
            {
                tex = Engine::getInstance()->createTextTexture(fontnamec_, s, size, { 255, 255, 255, 255 });
            }
            buffer_[index] = tex;
        }
        auto tex = buffer_[index];

        //Engine::getInstance()->queryTexture(tex, &w, &h);
        Engine::getInstance()->setColor(tex, color, alpha);
        if (c > 128)
        {
            //Engine::getInstance()->renderCopy(tex, x+1, y, w, h);
            Engine::getInstance()->renderCopy(tex, x, y, w, h);
        }
        else
        {
            w = size / 2;
            if (c != 32)
            {
                //Engine::getInstance()->renderCopy(tex, x+1, y, w, h);
                Engine::getInstance()->renderCopy(tex, x, y, w, h);
            }
        }
        x += w;
    }
}

void Font::drawWithBox(std::string text, int size, int x, int y, BP_Color color, uint8_t alpha /*= 255*/)
{
    TextureManager::getInstance()->renderTexture("title", 19, x, y);
    for (int i = 0; i < text.size(); i++)
    {
        TextureManager::getInstance()->renderTexture("title", 20, x + 19 + 10 * i, y);
    }
    TextureManager::getInstance()->renderTexture("title", 21, x + 19 + 10 * text.size(), y);

    draw(text, size, x + 19, y + 3, color, alpha);

}
