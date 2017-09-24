#include "Menu.h"

Menu::Menu()
{
}

Menu::~Menu()
{
    for (auto b : bts)
    {
        safe_delete(b);
    }
}

void Menu::draw()
{
    TextureManager::getInstance()->renderTexture(tex_, x_, y_);
    for (auto& b : bts)
    {
        b->draw();
    }
}

void Menu::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEMOTION)
    {
        for (auto b : bts)
        {
            if (b->inSide(e.motion.x, e.motion.y))
            {
                b->setState(Button::Pass);
            }
            else
            {
                b->setState(Button::Normal);
            }
        }
    }
    if (e.type == BP_MOUSEBUTTONDOWN)
    {
        for (auto b : bts)
        {
            if (b->inSide(e.motion.x, e.motion.y))
            {
                b->setState(Button::Press);
            }
        }
    }
    if (e.type == BP_MOUSEBUTTONUP)
    {
        for (int i = 0; i < bts.size(); i++)
        {
            if (bts[i]->inSide(e.motion.x, e.motion.y))
            {
                result_ = i;
                loop_ = false;
            }
        }
    }
}

void Menu::addButton(Button* b, int x, int y)
{
    bts.push_back(b);
    b->setPosition(x_ + x, y_ + y);
}

int Menu::menu(std::vector<Button*> bts, int x, int y)
{
    int r = -1;
    auto m = new Menu();
    for (int i = 0; i < bts.size(); i++)
    {
        m->addButton(bts[i], x, y + i * 20);
    }
    m->push(m);
    return r;
}

MenuText::MenuText(std::vector<std::string> items) : MenuText()
{
    setItems(items);
}

void MenuText::setItems(std::vector<std::string> items)
{
    items_ = items;
    int len = 0;
    for (auto& item : items_)
    {
        if (item.length() > len) len = item.length();
    }
    w_ = 10 * len;
    h_ = 25 * items_.size();
}

void MenuText::draw()
{
    Engine::getInstance()->fillColor({ 255, 255, 255, 128 }, x_, y_, w_, h_);
    for (int i = 0; i < items_.size(); i++)
    {
        int x = x_;
        int y = y_ + i * 25;
        Engine::getInstance()->drawText("../game/font/chinese.ttf", items_[i], 20, x, y, 255, BP_ALIGN_LEFT, { 0, 0, 0, 255 });
    }
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

