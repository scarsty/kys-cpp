#include "TeamMenu.h"
#include "Save.h"
#include "Button.h"
#include "GameUtil.h"

TeamMenu::TeamMenu()
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        //h->setOnlyHead(true);
        heads_.push_back(h);
        addChild(h, i % 2 * 250, i / 2 * 100);
        selected_.push_back(0);
    }
    auto button = new Button();
    button->setText("È«²¿ßx“ñ");
    if (mode_ == 1)
    {
        addChild(button, 50, 50);
    }
    setPosition(200, 100);
}

TeamMenu::~TeamMenu()
{
}

void TeamMenu::onEntrance()
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto r = Save::getInstance()->getTeamMate(i);
        if (r)
        {
            heads_[i]->setRole(r);
            //heads_[i]->setText(r->Name);
            //heads_[i]->setHaveBox(true);
        }
    }
}

Role* TeamMenu::getRole()
{
    return role_;
}

std::vector<Role*> TeamMenu::getRoles()
{
    std::vector<Role*> roles;
    return roles;
}

void TeamMenu::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
}

void TeamMenu::pressedOK()
{
    if (mode_ == 0)
    {
        for (auto h:heads_)
        {
            if (item_)
            {
                if (h->getState() != Normal && !GameUtil::canUseItem(h->getRole(), item_))
                {
                    h->setState(Normal);
                }
            }
            if (h->getState() == Press)
            {
                role_ = h->getRole();
            }
        }
        setExit(true);
    }
}
