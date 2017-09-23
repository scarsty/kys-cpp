#include "Menu.h"
#include"Dialogues.h"
#include "Head.h"


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
    auto m = new Menu(&r);
    for (int i=0;i<bts.size();i++)
    {
        m->addButton(bts[i], x, y+i*20);
    }
    m->push(m);
    return r;
}

/*========================================================================================*/

void MenuText::draw()
{
    SDL_Color color = { 0, 0, 0, 255 };

    for (auto& b : bts)
    {
        b->draw();
    }
    Engine::getInstance()->drawText("fonts/Dialogues.ttf", _title, 20, 5, 5, 255, BP_ALIGN_LEFT, color); //这里有问题，字符无法显示
}
void MenuText::setTitle(std::string str)
{
    _title = str;
}

bool MenuText::getResult()
{
    return _rs;
}
void MenuText::func(BP_Event& e, void* data)
{
    auto i = *(int*)(data);
    if (i == 0)
    {
        _rs = true;
    }
    if (i == 1)
    {
        _rs = false;
    }

}

void MenuText::ini()
{
    if (!_isIni)
    {

    }
}

void MenuText::setButton(std::string str, int num11, int num12, int num13, int num21, int num22, int num23)
{
    _path = str;
    _num11 = num11;
    _num12 = num12;
    _num13 = num13;
    if (num21 == -1)
    { num21 = num11; }
    _num21 = num21;
    _num22 = num22;
    _num23 = num23;
}