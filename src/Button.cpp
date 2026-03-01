#include "Button.h"
#include "Font.h"
#include "TextureManager.h"
#include <cmath>

Button::Button(const std::string& path, int normal_id, int pass_id /*= -1*/, int press_id /*= -1*/) :
    Button()
{
    setTexture(path, normal_id, pass_id, press_id);
}

Button::~Button()
{
}

void Button::dealEvent(EngineEvent& e)
{
    result_ = -1;
    if (e.type == EVENT_MOUSE_BUTTON_UP)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            result_ = 0;
        }
    }
}

void Button::draw()
{
    if (text_only_)
    {
        if (!text_.empty())
        {
            int x = x_ + text_x_;
            int y = y_ + text_y_;
            Color color = color_normal_;
            if (Engine::uiStyle() == 1 && color.r < 64 && color.g < 64 && color.b < 64)
            {
                color = { 240, 230, 210, 255 };
            }
            if (state_ == NodePass)
            {
                // Glow effect: draw a dimmer copy offset behind, then bright text
                Color glow = { color.r, color.g, color.b, 80 };
                Font::getInstance()->draw(text_, font_size_, x - 1, y - 1, glow, 80);
                Font::getInstance()->draw(text_, font_size_, x + 1, y + 1, glow, 80);
                color = { 255, 255, 255, 255 };
            }
            else if (state_ == NodePress)
            {
                x += 2;
                y += 2;
                color = { 255, 255, 200, 255 };
            }
            Font::getInstance()->draw(text_, font_size_, x, y, color, 255);
        }
        return;
    }

    //视情况重新计算尺寸
    if (w_ * h_ == 0)
    {
        auto tex = TextureManager::getInstance()->getTexture(texture_path_, texture_normal_id_);
        if (tex)
        {
            w_ = tex->w;
            h_ = tex->h;
        }
    }

    if (Engine::uiStyle() == 1)
    {
        // Dark translucent style for buttons
        int x = x_;
        int y = y_;
        int radius = 8;
        Color bg, outline;
        if (state_ == NodePass)
        {
            bg = { 30, 30, 30, 200 };
            outline = { 220, 200, 150, 240 };
            x += 1;
        }
        else if (state_ == NodePress)
        {
            bg = { 60, 55, 40, 220 };
            outline = { 255, 230, 160, 255 };
            x += 2;
            y += 2;
        }
        else
        {
            bg = { 0, 0, 0, 180 };
            outline = { 180, 170, 140, 200 };
        }

        // Apply custom outline color if set
        if (custom_outline_)
        {
            outline = *custom_outline_;
        }

        uint8_t alpha = alpha_;
        bg.a = (uint8_t)(bg.a * alpha / 255);
        outline.a = (uint8_t)(outline.a * alpha / 255);

        // Determine the box area: use text-based sizing for text buttons, texture sizing otherwise
        int bx = x, by = y, bw = w_, bh = h_;
        if (!text_.empty() && texture_path_.empty())
        {
            auto r = Font::getBoxSize(Font::getTextDrawSize(text_), font_size_, x + text_x_, y + text_y_);
            bx = r.x;
            by = r.y;
            bw = r.w;
            bh = r.h;
        }
        Engine::getInstance()->fillRoundedRect(bg, bx, by, bw, bh, radius);
        // Only draw outline if custom outline is set, animating, or in interactive state
        if (animate_outline_)
        {
            double phase = std::fmod(Engine::getTicks() * 0.0005, 1.0);  // one revolution per ~2 seconds
            for (int t = 0; t < outline_thickness_; t++)
            {
                int off = t;
                int r = std::max(0, radius - t);
                Engine::getInstance()->drawAnimatedRoundedRect(outline,
                    bx - off, by - off, bw + 2 * off, bh + 2 * off, r + off, phase);
            }
        }
        else if (custom_outline_ || state_ != NodeNormal)
        {
            Engine::getInstance()->drawRoundedRect(outline, bx, by, bw, bh, radius);
        }

        if (!text_.empty())
        {
            Color color_text = color_normal_;
            if (color_text.r < 64 && color_text.g < 64 && color_text.b < 64)
            {
                color_text = { 240, 230, 210, 255 };
            }
            if (state_ == NodePass)
            {
                color_text = { 255, 255, 255, 255 };
            }
            else if (state_ == NodePress)
            {
                color_text = { 255, 255, 200, 255 };
            }
            Font::getInstance()->draw(text_, font_size_, x + text_x_, y + text_y_, color_text, 255);
        }
        return;
    }

    // Classic texture style
    int x = x_;
    int y = y_;
    auto id = texture_normal_id_;
    Color color = { 255, 255, 255, 255 };
    uint8_t alpha = 255;
    if (state_ == NodeNormal)
    {
        if (texture_normal_id_ == texture_pass_id_)
        {
            color = { 224, 224, 224, 255 };
        }
    }
    if (state_ == NodePass)
    {
        id = texture_pass_id_;
        alpha = 240;
        x += 2;
    }
    else if (state_ == NodePress)
    {
        id = texture_press_id_;
        alpha = 255;
        x += 2;
        y += 2;
    }
    if (alpha_ != 255) { alpha = alpha_; }
    TextureManager::getInstance()->renderTexture(texture_path_, id, x, y, color, alpha);

    if (!text_.empty())
    {
        Color color_text = color_normal_;
        Font::getInstance()->drawWithBox(text_, font_size_, x + text_x_, y + text_y_, color_text, 255, alpha);
    }
}

ButtonGetKey::~ButtonGetKey()
{
}

void ButtonGetKey::dealEvent(EngineEvent& e)
{
    if (e.type == EVENT_KEY_UP)
    {
        result_ = e.key.key;
        setExit(true);
    }
}
