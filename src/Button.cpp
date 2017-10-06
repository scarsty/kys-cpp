#include "Button.h"
#include "PotConv.h"
#include "Font.h"

Button::Button(const std::string& path, int normal_id, int pass_id /*= -1*/, int press_id /*= -1*/)
{
    setTexture(path, normal_id, pass_id, press_id);
}

void Button::setTexture(const std::string& path, int normal_id, int pass_id /*= -1*/, int press_id /*= -1*/)
{
    if (pass_id < 0) { pass_id = normal_id; }
    if (press_id < 0) { press_id = normal_id; }
    tex_path_ = path;
    tex_normal_id_ = normal_id;
    tex_pass_id_ = pass_id;
    tex_press_id_ = press_id;
}

Button::~Button()
{
}

void Button::dealEvent(BP_Event& e)
{
    result_ = -1;
    if (e.type == BP_MOUSEBUTTONUP)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            result_ = 0;
        }
    }
}

void Button::draw()
{
    int x = x_;
    int y = y_;
    auto id = tex_normal_id_;
    BP_Color color = { 255, 255, 255, 255 };
    BP_Color color_text = { 32, 32, 32, 255 };

    if (state_ == Normal)
    {
        if (tex_normal_id_ == tex_pass_id_)
        {
            color = { 128, 128, 128, 255 };
        }
    }
    if (state_ == Pass)
    {
        id = tex_pass_id_;
        color_text = { 255, 255, 255, 255 };
    }
    else if (state_ == Press)
    {
        id = tex_press_id_;
        color_text = { 255, 0, 0, 255 };
    }
    TextureManager::getInstance()->renderTexture(tex_path_, id, x, y, color);
    if (text_.size())
    {
        Font::getInstance()->drawWithBox(text_, 20, x, y, color_text, 255);
    }

    //视情况重新计算尺寸
    if (w_ * h_ == 0)
    {
        auto tex = TextureManager::getInstance()->loadTexture(tex_path_, tex_normal_id_);
        if (tex)
        {
            w_ = tex->w;
            h_ = tex->h;
        }
    }
}

void Button::setText(std::string text)
{
    text_ = text;
    w_ = font_size_ * text.length() / 2;
    h_ = font_size_;
}
