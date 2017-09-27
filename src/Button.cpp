#include "Button.h"
#include "PotConv.h"
#include "Font.h"

Button::Button(const std::string& path, int n1, int n2/*=-1*/, int n3/*=-1*/)
{
    setTexture(path, n1, n2, n3);
}

void Button::setTexture(const std::string& path, int n1, int n2 /*= -1*/, int n3 /*= -1*/)
{
    if (n2 < 0) { n2 = n1; }
    if (n3 < 0) { n3 = n1; }
    tex_normal_ = TextureManager::getInstance()->loadTexture(path, n1);
    tex_pass_ = TextureManager::getInstance()->loadTexture(path, n2);
    tex_press_ = TextureManager::getInstance()->loadTexture(path, n3);
    w_ = tex_normal_->w;
    h_ = tex_normal_->h;
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
    auto tex = tex_normal_;
    BP_Color color = { 255, 255, 255, 255 };
    BP_Color color_text = { 255, 255, 255, 255 };

    if (state_ == Normal)
    {
        if (tex_normal_ == tex_pass_)
        {
            color = { 128, 128, 128, 255 };
        }
    }
    if (state_ == Pass)
    {
        tex = tex_pass_;
        color_text = { 255, 255, 0, 255 };
    }
    else if (state_ == Press)
    {
        tex = tex_press_;
        color_text = { 255, 0, 0, 255 };
    }
    TextureManager::getInstance()->renderTexture(tex, x_, y_, color);
    if (text_.size())
    {
        Font::getInstance()->draw(text_, 20, x_, y_, color_text, 255);
    }
}

void Button::setText(std::string text)
{
    text_ = text;
    w_ = font_size_ * text.length();
    h_ = font_size_;
}
