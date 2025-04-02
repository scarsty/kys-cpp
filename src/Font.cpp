#include "Font.h"

#include <iostream>

#include "GameUtil.h"
#include "OpenCCConverter.h"
#include "TextureManager.h"
#include "PotConv.h"

Font::Font()
{
    fontnamec_ = GameUtil::PATH() + "font/chinese.ttf";
    fontnamee_ = GameUtil::PATH() + "font/english.ttf";
}

Rect Font::getBoxSize(int textLen, int size, int x, int y)
{
    Rect r;
    r.x = x - 10;
    r.y = y - 3;
    r.w = size * textLen / 2 + 20;
    r.h = size + 6;
    return r;
}

//此处仅接受utf8
//返回值为实际画的行数
int Font::draw(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    int line = 1;
    int p = 0;
    int char_count = 0;
    int s1;
    color.a = alpha;
    std::string text1;
    int x0 = x;
    const std::string* textp = &text;
    if (simplified_)
    {
        text1 = t2s_buffer_[text];
        if (text1.empty())
        {
            text1 = OpenCCConverter::getInstance()->UTF8t2s(text);
            t2s_buffer_[text] = text1;
        }
        textp = &text1;
    }
    if (stat_message_)
    {
        s1 = getBufferSize();
    }
    //auto ss = PotConv::utf8tocp936(text);
    while (p < textp->size())
    {
        int w = size, h = size;
        uint32_t c = (uint8_t)textp->data()[p];
        p++;
        if (c == '\n')
        {
            y += h;
            x = x0;
            line++;
            continue;
        }
        if (c >= 128)
        {
            c += (uint8_t)textp->data()[p] * 256 + (uint8_t)textp->data()[p + 1] * 256 * 256;
            p += 2;
        }
        if (buffer_[c].count(size) == 0)
        {
            auto s = std::string((char*)(&c));
            //auto ws = PotConv::ToWide(s, "utf-8");
            //buffer_[c][size] = Engine::getInstance()->createTextTexture(fontnamec_, ws[0], size, { 255, 255, 255, 255 });
            buffer_[c][size] = Engine::getInstance()->createTextTexture(fontnamec_, s, size, { 255, 255, 255, 255 });
        }
        auto tex = buffer_[c][size];
        char_count++;
        int w1 = w;
        int x1 = x;
        //Engine::getInstance()->getTextureSize(tex, w, h);
        if (c < 128)
        {
            w = size / 2;
            int h1;
            Engine::getInstance()->getTextureSize(tex, w1, h1);
            w1 = (std::min)(w1, w);
            x1 += (w - w1) / 2;
        }
        if (c > ' ')
        {
            Engine::getInstance()->setColor(tex, { uint8_t(color.r / 10), uint8_t(color.g / 10), uint8_t(color.b / 10), color.a });
            Engine::getInstance()->setColor(tex, { 0, 0, 0, color.a });
            Engine::getInstance()->renderTexture(tex, x1 + 1, y, -1, -1);
            Engine::getInstance()->setColor(tex, color);
            Engine::getInstance()->renderTexture(tex, x1, y, -1, -1);
        }
        x += w;
    }
    if (stat_message_)
    {
        int s = getBufferSize() - s1;
        if (s > 0)
        {
            LOG(" {}/{}, {}, total = {}, t2s buffer = {}\n", s, char_count, size, getBufferSize(), t2s_buffer_.size());
        }
    }
    return line;
}

void Font::drawWithBox(const std::string& text, int size, int x, int y, Color color, uint8_t alpha, uint8_t alpha_box)
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
/*
void Font::drawText(const std::string& fontname, std::string& text, int size, int x, int y, uint8_t alpha, int align, Color c)
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
    Rect rect;
    Engine::getInstance()->getTextureSize(text_t, rect.w, rect.h);
    rect.y = y;
    switch (align)
    {
    case ALIGN_LEFT:
        rect.x = x;
        break;
    case ALIGN_RIGHT:
        rect.x = x - rect.w;
        break;
    case ALIGN_MIDDLE:
        rect.x = x - rect.w / 2;
        break;
    }
    Engine::getInstance()->renderTexture(text_t, nullptr, &rect);
    Engine::getInstance()->destroyTexture(text_t);
}*/

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
