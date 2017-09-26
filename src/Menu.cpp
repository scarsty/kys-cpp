#include "Menu.h"
#include "Button.h"

Menu::Menu()
{
}

Menu::~Menu()
{
}

void Menu::draw()
{
    TextureManager::getInstance()->renderTexture(tex_, x_, y_);
}

void Menu::dealEvent(BP_Event& e)
{
    for (int i = 0; i < childs_.size(); i++)
    {
        if (childs_[i]->getResult() == 0)
        {
            result_ = i;
            loop_ = false;
        }
    }
}

MenuText::MenuText(std::vector<std::string> items) : MenuText()
{
    setStrings(items);
}

void MenuText::setStrings(std::vector<std::string> strings)
{
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
    Engine::getInstance()->fillColor({ 255, 255, 255, 128 }, x_, y_, w_, h_);
}

void MenuText::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEMOTION)
    {

    }
    if (e.type == BP_MOUSEBUTTONDOWN)
    {

    }
    if (e.type == BP_MOUSEBUTTONUP)
    {
        result_ = 1;
        loop_ = false;
    }
}

