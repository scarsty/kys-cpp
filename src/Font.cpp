#include "Font.h"
#include "PotConv.h"
#include "TextureManager.h"

Font::Font()
{
}

BP_Rect Font::getBoxSize(int textLen, int size, int x, int y)
{
    BP_Rect r;
    r.x = x - 10;
    r.y = y - 3;
    r.w = size * textLen / 2 + 20;
    r.h = size + 6;
    return r;
}

void Font::draw(const std::string& text, int size, int x, int y, BP_Color color, uint8_t alpha)
{
    int p = 0;
    int char_count = 0;
    int s1;
    color.a = alpha;
    if (stat_message_)
    {
        s1 = getBufferSize();
    }
    auto ss = PotConv::utf8tocp936(text);
    while (p < text.size())
    {
        int w = size, h = size;
        uint32_t c = (uint8_t)text[p];
        p++;
        if (c >= 128)
        {
            c += (uint8_t)text[p] * 256 + (uint8_t)text[p + 1] * 256 * 256;
            p += 2;
        }
        if (buffer_[c].count(size) == 0)
        {
            auto s = (char*)(&c);
            buffer_[c][size] = Engine::getInstance()->createTextTexture(fontnamec_, s, size, { 255, 255, 255, 255 });
        }
        auto tex = buffer_[c][size];
        char_count++;
        int w1 = w;
        int x1 = x;
        //Engine::getInstance()->queryTexture(tex, &w, &h);
        if (c <= 128)
        {
            w = size / 2;
            Engine::getInstance()->queryTexture(tex, &w1, nullptr);
            w1 = (std::min)(w1, w);
            x1 += (w - w1) / 2;
        }
        if (c != 32)
        {
            Engine::getInstance()->setColor(tex, { uint8_t(color.r / 2), uint8_t(color.g / 2), uint8_t(color.b / 2), color.a });
            Engine::getInstance()->renderCopy(tex, x1 + 1, y, w1, h);
            Engine::getInstance()->setColor(tex, color);
            Engine::getInstance()->renderCopy(tex, x1, y, w1, h);
        }
        x += w;
    }
    if (stat_message_)
    {
        int s = getBufferSize() - s1;
        if (s > 0)
        {
            fmt::print(" %d/%d, %d, total = %d\n", s, char_count, size, getBufferSize());
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
    auto r = getBoxSize(getTextDrawSize(text), size, x, y);
    TextureManager::getInstance()->renderTexture("title", 126, r, { 255, 255, 255, 255 }, alpha_box);
    draw(text, size, x, y, color, alpha);
}

//此处仅接受utf8
void Font::drawText(const std::string& fontname, std::string& text, int size, int x, int y, uint8_t alpha, int align, BP_Color c)
{
    if (alpha == 0)
    {
        return;
    }
    auto text_t = Engine::getInstance()->createTextTexture(fontname, text, size, c);
    if (!text_t)
    {
        return;
    }
    Engine::getInstance()->setTextureAlphaMod(text_t, alpha);
    BP_Rect rect;
    Engine::getInstance()->queryTexture(text_t, &rect.w, &rect.h);
    rect.y = y;
    switch (align)
    {
    case BP_ALIGN_LEFT:
        rect.x = x;
        break;
    case BP_ALIGN_RIGHT:
        rect.x = x - rect.w;
        break;
    case BP_ALIGN_MIDDLE:
        rect.x = x - rect.w / 2;
        break;
    }
    Engine::getInstance()->renderCopy(text_t, nullptr, &rect);
    Engine::getInstance()->destroyTexture(text_t);
}

void Font::clearBuffer()
{
    for (auto& f : buffer_)
    {
        for (auto& s : f.second)
        {
            Engine::getInstance()->destroyTexture(s.second);
        }
    }
    buffer_.clear();
}

int Font::getBufferSize()
{
    int sum = 0;
    for (auto& f : buffer_)
    {
        sum += f.second.size();
    }
    return sum;
}

int Font::getTextDrawSize(const std::string& text)
{
    int len = 0;
    for (int i = 0; i < text.size();)
    {
        uint8_t v = text[i];
        if (v < 128)
        {
            len++;
            i++;
        }
        else
        {
            len += 2;
            i += 3;
        }
    }
    return len;
}
