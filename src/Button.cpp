#include "Button.h"
#include "PotConv.h"
#include "Font.h"

Button::Button(const std::string& path, int normal_id, int pass_id /*= -1*/, int press_id /*= -1*/)
{
    setTexture(path, normal_id, pass_id, press_id);
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
    //视情况重新计算尺寸
    if (w_ * h_ == 0)
    {
        auto tex = TextureManager::getInstance()->loadTexture(texture_path_, texture_normal_id_);
        if (tex)
        {
            w_ = tex->w;
            h_ = tex->h;
        }
    }
    int x = x_;
    int y = y_;
    auto id = texture_normal_id_;
    BP_Color color = { 255, 255, 255, 255 };
    uint8_t alpha = 225;
    if (state_ == Normal)
    {
        if (texture_normal_id_ == texture_pass_id_)
        {
            color = { 128, 128, 128, 255 };
        }
    }
    if (state_ == Pass)
    {
        id = texture_pass_id_;
        alpha = 240;
    }
    else if (state_ == Press)
    {
        id = texture_press_id_;
        alpha = 255;
    }
    TextureManager::getInstance()->renderTexture(texture_path_, id, x, y, color, alpha);


    if (!text_.empty())
    {
        BP_Color color_text = color_normal_;
        if (state_ == Pass)
        {
            color_text = color_pass_;
        }
        else if (state_ == Press)
        {
            color_text = color_press_;
        }
        Font::getInstance()->drawWithBox(text_, font_size_, x_ + text_x_, y_ + text_y_, color_text, 255, alpha);
    }
}


