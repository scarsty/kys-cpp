#include "Button.h"

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
    //switch (e.type)
    //{
    //case BP_MOUSEMOTION:
    //{
    //    nState = Normal;
    //    if (inSide(e.button.x, e.button.y))
    //    {
    //        nState = Pass;
    //    }
    //    break;
    //}
    //case  BP_MOUSEBUTTONDOWN:
    //{
    //    if (e.button.button == BP_BUTTON_LEFT)
    //    {
    //        if (inSide(e.button.x, e.button.y))
    //        {
    //            nState = Press;
    //        }
    //    }
    //}
    //case BP_MOUSEBUTTONUP:
    //{
    //    if (e.button.button == BP_BUTTON_LEFT)
    //    {
    //        if (inSide(e.button.x, e.button.y))
    //        {
    //            func();
    //        }
    //    }
    //    if (e.button.button == BP_BUTTON_RIGHT)
    //    {
    //        pop();
    //    }
    //    break;
    //}

    //}
}

void Button::draw()
{
    auto tex = normal;
    if (Pass == state_)
    {
        tex = pass;
    }
    else if (Press == state_)
    {
        tex = press;
    }
    TextureManager::getInstance()->renderTexture(tex, x_, y_);
}
