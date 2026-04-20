#include "Font.h"
#include "GameUtil.h"
#include "PotConv.h"
#include "TextureManager.h"
#include <iostream>

Font::Font()
{
    fontnamec_ = GameUtil::PATH() + "font/chinese.ttf";
    fontnamee_ = GameUtil::PATH() + "font/chinese.ttf";
    cct2s_.init({ GameUtil::PATH() + "cc/TSPhrases.txt", GameUtil::PATH() + "cc/TSCharacters.txt" });
}

Rect Font::getBoxRect(int textLen, int size, int x, int y)
{
    Rect r;
    r.x = x - 10;
    r.y = y - 3;
    r.w = size * textLen / 2 + 20;
    r.h = size + 8;
    return r;
}

//此处仅接受utf8，将绘制调用加入延迟队列
//返回值为估算的行数
int Font::draw(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    if (text.empty() || size <= 0 || alpha == 0)
    {
        return 0;
    }
    draw_calls_.push_back({ text, size, x, y, color, alpha });

    int lines = 1;
    for (char c : text)
    {
        if (c == '\n') { lines++; }
    }
    return lines;
}

//实际执行渲染，在当前渲染目标上绘制文字
//size 为渲染目标像素尺寸，(x, y) 为渲染目标坐标
int Font::renderText(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    if (text.empty() || size <= 0 || alpha == 0)
    {
        return 0;
    }
    int line = 1;
    int p = 0;
    color.a = alpha;
    std::string text1;
    int x0 = x;
    const std::string* textp = &text;
    if (simplified_)
    {
        text1 = t2s_buffer_[text];
        if (text1.empty())
        {
            text1 = cct2s_.conv(text);
            t2s_buffer_[text] = text1;
        }
        textp = &text1;
    }

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
        if (c >= 0xe0)
        {
            c += (uint8_t)textp->data()[p] * 256 + (uint8_t)textp->data()[p + 1] * 256 * 256;
            p += 2;
        }
        else if (c >= 0xc0)
        {
            c += (uint8_t)textp->data()[p] * 256;
            p++;
        }
        if (buffer_[c].count(size) == 0)
        {
            auto s = std::string((char*)(&c));
            auto fontname = (c < 128) ? fontnamee_ : fontnamec_;
            auto key = fontname + std::to_string(size);
            auto ttf_font = font_buffer_[key];
            if (!ttf_font)
            {
                ttf_font = TTF_OpenFont(fontname.c_str(), size);
                if (!ttf_font)
                {
                    LOG("Failed to load font {}, size {}\n", fontname, size);
                    continue;
                }
                font_buffer_[key] = ttf_font;
            }
            buffer_[c][size] = Engine::getInstance()->createTextTexture(ttf_font, s, { 255, 255, 255, 255 });
        }
        auto tex = buffer_[c][size];
        int x1 = x;
        int y1 = y;
        if (c < 128)
        {
            w = size / 2;
            int native_w, native_h;
            Engine::getInstance()->getTextureSize(tex, native_w, native_h);
            int draw_w = (std::min)(native_w, w);
            x1 += (w - draw_w) / 2;
            y1 = y + 4;
            if (c > ' ')
            {
                Engine::getInstance()->setColor(tex, { uint8_t(color.r / 10), uint8_t(color.g / 10), uint8_t(color.b / 10), color.a });
                Engine::getInstance()->renderTexture(tex, x1 + 1, y1, draw_w, size);
                Engine::getInstance()->setColor(tex, color);
                Engine::getInstance()->renderTexture(tex, x1, y1, draw_w, size);
            }
        }
        else
        {
            if (c > ' ')
            {
                Engine::getInstance()->setColor(tex, { uint8_t(color.r / 10), uint8_t(color.g / 10), uint8_t(color.b / 10), color.a });
                Engine::getInstance()->renderTexture(tex, x1 + 1, y1, -1, -1);
                Engine::getInstance()->setColor(tex, color);
                Engine::getInstance()->renderTexture(tex, x1, y1, -1, -1);
            }
        }
        x += w;
    }
    return line;
}

//将延迟队列中的文字绘制调用执行到窗口帧缓冲（已无缩放），保证字体清晰
//应在 renderMainTextureToWindow() 之后、renderPresent() 之前调用
void Font::executeDrawCalls()
{
    if (draw_calls_.empty())
    {
        return;
    }

    int px, py, pw, ph;
    Engine::getInstance()->getPresentRect(px, py, pw, ph);
    int ui_w, ui_h;
    Engine::getInstance()->getUISize(ui_w, ui_h);

    if (pw <= 0 || ph <= 0 || ui_w <= 0 || ui_h <= 0)
    {
        draw_calls_.clear();
        return;
    }

    float sx = float(pw) / ui_w;
    float sy = float(ph) / ui_h;

    for (auto& call : draw_calls_)
    {
        int wx = px + int(call.x * sx);
        int wy = py + int(call.y * sy);
        int wsize = std::max(1, int(call.size * sx));
        renderText(call.text, wsize, wx, wy, call.color, call.alpha);
    }

    draw_calls_.clear();
}

void Font::drawWithBox(const std::string& text, int size, int x, int y, Color color, uint8_t alpha, uint8_t alpha_box)
{
    auto r = getBoxRect(getTextDrawSize(text), size, x, y);
    TextureManager::getInstance()->renderTexture("title", 126, r.x, r.y, { { 255, 255, 255, 255 }, alpha_box }, r.w, r.h);
    draw(text, size, x, y, color, alpha);
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
        if (v < 0x80)
        {
            len++;
            i++;
        }
        else if (v >= 0xc0 && v < 0xe0)
        {
            len++;
            i += 2;
        }
        else if (v >= 0xe0 && v < 0xf0)
        {
            len += 2;
            i += 3;
        }
        else if (v >= 0xf0 && v < 0xf8)
        {
            len += 2;
            i += 4;
        }
        else
        {
            //不应出现在这里的字符，跳过避免死循环
            i++;
        }
    }
    return len;
}
