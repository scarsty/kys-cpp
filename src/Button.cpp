#include "Button.h"
#include "Font.h"
#include "TextureManager.h"

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
        //if (state_ == NodePass)
        //{
        //    color_text = color_pass_;
        //}
        //else if (state_ == NodePress)
        //{
        //    color_text = color_press_;
        //}
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
