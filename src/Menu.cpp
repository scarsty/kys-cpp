#include "Menu.h"
#include "Button.h"
#include "Font.h"

Menu::Menu()
{
}

Menu::~Menu()
{
}

void Menu::draw()
{
    if (tex_)
    {
        TextureManager::getInstance()->renderTexture(tex_, x_, y_);
    }
    if (title_.size() > 0)
    {
        Font::getInstance()->draw(title_, 20, x_ + title_x_, y_ + title_y_, { 255, 255, 255, 255 });
    }
}

void Menu::dealEvent(BP_Event& e)
{
    if ((e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
        || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT))
    {
        for (int i = 0; i < childs_.size(); i++)
        {
            if (childs_[i]->getResult() == 0)
            {
                result_ = i;
                setExit(true);
            }
        }
    }
    if ((e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE)
        || (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT))
    {
        result_ = -1;
        setExit(true);
    }

}

void Menu::arrange(int x, int y, int inc_x, int inc_y)
{
    for (auto c : childs_)
    {
        if (c->getVisible())
        {
            c->setPosition(x_ + x, y_ + y);
            x += inc_x;
            y += inc_y;
        }
    }
}

MenuText::MenuText(std::vector<std::string> items) : MenuText()
{
    setStrings(items);
}

void MenuText::setStrings(std::vector<std::string> strings)
{
    clearChilds();
    int len = 0;
    int i = 0;
    for (auto& str : strings)
    {
        if (str.length() > len) { len = str.length(); }
        auto b = new Button();
        b->setText(str);
        addChild(b, 0, i * 25);
        i++;
    }
    w_ = 10 * len;
    h_ = 25 * strings.size();
}

void MenuText::draw()
{
    if (title_.size() > 0)
    {
        Font::getInstance()->draw(title_, 20, x_ + title_x_, y_ + title_y_, { 255, 255, 255, 255 });
    }
    //Engine::getInstance()->fillColor({ 255, 255, 255, 128 }, x_, y_, w_, h_);
}

void TextBox::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEBUTTONUP || e.type == BP_KEYUP)
    {
        setExit(true);
    }
}
