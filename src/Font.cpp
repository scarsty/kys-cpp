#include "Font.h"
#include "PotConv.h"
#include "TextureManager.h"

Font::~Font()
{
    for (auto buffer : buffer_)
    {
        Engine::destroyTexture(buffer.second);
    }
}

BP_Texture* Font::indexTex(int size, uint16_t c)
{
    auto index = size * 0x1000000 + c;
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
        if (stat_message_)
        {
            printf("%s", (char*)(c2));
        }
    }
    return buffer_[index];
}

void Font::draw(const std::string& text, int size, int x, int y, BP_Color color, uint8_t alpha)
{
    int p = 0;
    int char_count = 0;
    int s1;
    if (stat_message_)
    {
        s1 = buffer_.size();
    }
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
        auto tex = indexTex(size, c);
        char_count++;
        //Engine::getInstance()->queryTexture(tex, &w, &h);
        if (c <= 128)
        {
            w = size / 2;
        }
        if (c != 32)
        {
            Engine::getInstance()->setColor(tex, { uint8_t(color.r / 2), uint8_t(color.g / 2), uint8_t(color.b / 2), color.a });
            Engine::getInstance()->renderCopy(tex, x + 1, y, w, h);
            Engine::getInstance()->setColor(tex, color);
            Engine::getInstance()->renderCopy(tex, x, y, w, h);
        }
        x += w;
    }
    if (stat_message_)
    {
        int s = buffer_.size() - s1;
        if (s > 0)
        {
            printf(" %d/%d, %d, total = %d\n", s, char_count, size, buffer_.size());
        }
    }
}

void Font::drawWithBox(const std::string& text, int size, int x, int y, BP_Color color, uint8_t alpha, uint8_t alpha_box)
{
    //TextureManager::getInstance()->renderTexture("title", 19, x - 19, y - 3);
    //for (int i = 0; i < text.size(); i++)
    //{
    //TextureManager::getInstance()->renderTexture("title", 20, x + 10 * i, y - 3);
    //}
    BP_Rect r;
    r.x = x - 10;
    r.y = y - 3;
    r.w = size * text.size() / 2 + 20;
    r.h = size + 6;
    TextureManager::getInstance()->renderTexture("title", 126, r, { 255, 255, 255, 255 }, alpha_box);
    draw(text, size, x, y, color, alpha);
}
