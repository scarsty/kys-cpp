#include "TeamMenu.h"
#include "Save.h"
#include "Button.h"

TeamMenu::TeamMenu()
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        heads_.push_back(h);
        addChild(h, i % 2 * 250, i / 2 * 100);
        selected_.push_back(0);
    }
    auto button = new Button();
    button->setText("È«²¿ßx“ñ");
    addChild(button, 50, 50);
}

TeamMenu::~TeamMenu()
{
}

void TeamMenu::onEntrance()
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        heads_[i]->setRole(Save::getInstance()->getTeamMate(i));
    }
}
