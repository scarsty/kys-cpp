#include "Button.h"
#include "PotConv.h"

Button::Button(const std::string& path, int n1, int n2/*=-1*/, int n3/*=-1*/)
{
    if (n2 < 0) { n2 = n1; }
    if (n3 < 0) { n3 = n1; }
    normal = TextureManager::getInstance()->loadTexture(path, n1);
    pass = TextureManager::getInstance()->loadTexture(path, n2);
    press = TextureManager::getInstance()->loadTexture(path, n3);
    w_ = normal->w;
    h_ = normal->h;
}

Button::~Button()
{
}

void Button::dealEvent(BP_Event& e)
{
    result_ = -1;
    if (e.type == BP_MOUSEMOTION)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            setState(Button::Pass);
        }
        else
        {
            setState(Button::Normal);
        }
    }
    if (e.type == BP_MOUSEBUTTONDOWN)
    {
        if (inSide(e.motion.x, e.motion.y))
        {
            setState(Button::Press);
        }
    }
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
    auto tex = normal;
    BP_Color color = { 255, 255, 255, 255 };
    if (Pass == state_)
    {
        tex = pass;
        color = { 255, 255, 0, 255 };
    }
    else if (Press == state_)
    {
        tex = press;
        color = { 255, 0, 0, 255 };
    }
    TextureManager::getInstance()->renderTexture(tex, x_, y_);
    if (text_.size())
    {
        Engine::getInstance()->drawText("../game/font/chinese.ttf", text_, 20, x_, y_, 255, BP_ALIGN_LEFT, color);
    }
}

void Button::setText(std::string text)
{
    text_ = text;
    w_ = font_size_ * text.length();
    h_ = font_size_;
}
