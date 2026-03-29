#include "Font.h"
#include "GameUtil.h"
#include "PotConv.h"
#include "TextureManager.h"
#include <iostream>

Font::Font()
{
    fontnamec_ = GameUtil::PATH() + "font/chinese.ttf";
    fontnamee_ = GameUtil::PATH() + "font/english.ttf";
    cct2s_.init({ GameUtil::PATH() + "cc/TSPhrases.txt", GameUtil::PATH() + "cc/TSCharacters.txt" });
    ccs2t_.init({ GameUtil::PATH() + "cc/STPhrases.txt", GameUtil::PATH() + "cc/STCharacters.txt" });
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

int Font::utf8CharLength(unsigned char c)
{
    if (c < 0x80)
    {
        return 1;
    }
    if ((c & 0xE0) == 0xC0)
    {
        return 2;
    }
    if ((c & 0xF0) == 0xE0)
    {
        return 3;
    }
    if ((c & 0xF8) == 0xF0)
    {
        return 4;
    }
    return 1;
}

int Font::utf8DisplayWidth(unsigned char c)
{
    if (c < 0x80)
    {
        return 1;
    }
    int charLength = utf8CharLength(c);
    if (charLength == 2)
    {
        return 1;
    }
    if (charLength >= 3)
    {
        return 2;
    }
    return 1;
}

//此处仅接受utf8，将绘制调用加入延迟队列
//返回值为估算的行数
int Font::draw(const std::string& text, int size, int x, int y, Color color, uint8_t alpha)
{
    if (text.empty() || size <= 0 || alpha == 0)
    {
        return 0;
    }

    auto* engine = Engine::getInstance();
    if (engine->getRenderTarget() != engine->getMainTexture())
    {
        return renderText(text, size, x, y, color, alpha);
    }

    draw_calls_.push_back({ text, size, x, y, color, alpha });

    int lines = 1;
    for (char c : text)
    {
        if (c == '\n')
        {
            lines++;
        }
    }
    return lines;
}

std::string Font::localize(const std::string& str)
{
    if (!simplified_ || str.empty())
    {
        return str;
    }

    auto& cached = t2s_buffer_[str];
    if (cached.empty())
    {
        cached = cct2s_.conv(str);
    }
    return cached;
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
        text1 = localize(text);
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
            buffer_[c][size] = Engine::getInstance()->createTextTexture(fontname, s, size, { 255, 255, 255, 255 });
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
            y1 = y + (std::max)(1, size / 24);
            if (c > ' ')
            {
                if (Engine::uiStyle() != 1)
                {
                    Engine::getInstance()->setColor(tex, { uint8_t(color.r / 10), uint8_t(color.g / 10), uint8_t(color.b / 10), color.a });
                    Engine::getInstance()->renderTexture(tex, x1 + 1, y1, draw_w, size);
                }
                Engine::getInstance()->setColor(tex, color);
                Engine::getInstance()->renderTexture(tex, x1, y1, draw_w, size);
            }
        }
        else if (c > ' ')
        {
            if (Engine::uiStyle() != 1)
            {
                Engine::getInstance()->setColor(tex, { uint8_t(color.r / 10), uint8_t(color.g / 10), uint8_t(color.b / 10), color.a });
                Engine::getInstance()->renderTexture(tex, x1 + 1, y1, -1, -1);
            }
            Engine::getInstance()->setColor(tex, color);
            Engine::getInstance()->renderTexture(tex, x1, y1, -1, -1);
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

    int ui_w = 0, ui_h = 0;
    Engine::getInstance()->getUISize(ui_w, ui_h);
    int present_x = 0, present_y = 0, present_w = 0, present_h = 0;
    Engine::getInstance()->getPresentRect(present_x, present_y, present_w, present_h);
    if (present_w <= 0 || present_h <= 0 || ui_w <= 0 || ui_h <= 0)
    {
        draw_calls_.clear();
        return;
    }

    float sx = float(present_w) / ui_w;
    float sy = float(present_h) / ui_h;

    for (const auto& call : draw_calls_)
    {
        int wx = present_x + int(call.x * sx);
        int wy = present_y + int(call.y * sy);
        int wsize = (std::max)(1, int(call.size * sx));
        renderText(call.text, wsize, wx, wy, call.color, call.alpha);
    }

    draw_calls_.clear();
}

void Font::drawWithBox(const std::string& text, int size, int x, int y, Color color, uint8_t alpha, uint8_t alpha_box)
{
    auto r = getBoxSize(getTextDrawSize(text), size, x, y);
    if (Engine::uiStyle() == 1)
    {
        // Dark translucent rounded box
        int radius = 6;
        Color bg = { 0, 0, 0, (uint8_t)(alpha_box * 128 / 255) };
        Color outline = { 180, 170, 140, (uint8_t)(alpha_box * 200 / 255) };
        Engine::getInstance()->fillRoundedRect(bg, r.x, r.y, r.w, r.h, radius);
        Engine::getInstance()->drawRoundedRect(outline, r.x, r.y, r.w, r.h, radius);
    }
    else
    {
        TextureManager::getInstance()->renderTexture("title", 126, r.x, r.y, TextureManager::RenderInfo{ { 255, 255, 255, 255 }, alpha_box }, r.w, r.h);
    }
    draw(text, size, x, y, color, alpha);
}

void Font::drawWithBoxCentered(const std::string& text, int size, int y, Color color, uint8_t alpha, uint8_t alpha_box)
{
    int textW = size * getTextDrawSize(text) / 2;
    int x = Engine::getInstance()->getUIWidth() / 2 - textW / 2;
    drawWithBox(text, size, x, y, color, alpha, alpha_box);
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
    draw_calls_.clear();
}

int Font::getBufferSize()
{
    int sum = 0;
    for (auto& f : buffer_)
    {
        sum += static_cast<int>(f.second.size());
    }
    return sum;
}

int Font::getTextDrawSize(const std::string& text)
{
    int len = 0;
    for (int i = 0; i < text.size();)
    {
        uint8_t v = text[i];
        int charLength = utf8CharLength(v);
        if (v >= 0x80 && charLength == 1)
        {
            i++;
        }
        else
        {
            len += utf8DisplayWidth(v);
            i += charLength;
        }
    }
    return len;
}
