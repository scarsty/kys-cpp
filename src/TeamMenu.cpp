#include "TeamMenu.h"
#include "Button.h"
#include "GameUtil.h"
#include "Save.h"

TeamMenu::TeamMenu()
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = std::make_shared<Head>();
        addChild(h, i % 2 * 250, i / 2 * 100);
        h->setHaveBox(false);
        //h->setOnlyHead(true);
        heads_.push_back(h);
        //selected_.push_back(0);
    }
    button_all_ = std::make_shared<Button>();
    button_all_->setText("全選");
    button_ok_ = std::make_shared<Button>();
    button_ok_->setText("確定");
    addChild(button_all_, 0, 300);
    addChild(button_ok_, 100, 300);
    setPosition(200, 150);
    setTextPosition(20, -30);
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
            if (mode_ == 0 && item_)
            {
                if (!GameUtil::canUseItem(r, item_))
                {
                    heads_[i]->setText("不適合");
                }
                if (r->PracticeItem == item_->ID || r->Equip0 == item_->ID || r->Equip1 == item_->ID)
                {
                    heads_[i]->setText("使用中");
                }
            }
        }
    }
    if (mode_ == 0)
    {
        button_all_->setVisible(false);
        button_ok_->setVisible(false);
    }
    for (auto h : heads_)
    {
        h->setVisible(h->getRole());
    }
}

Role* TeamMenu::getRole()
{
    return role_;
}

std::vector<Role*> TeamMenu::getRoles()
{
    std::vector<Role*> roles;
    for (auto h : heads_)
    {
        if (h->getResult() == 0 && h->getRole())
        {
            roles.push_back(h->getRole());
        }
    }
    return roles;
}

void TeamMenu::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    TextBox::draw();
}

void TeamMenu::onPressedOK()
{
    if (mode_ == 0)
    {
        role_ = nullptr;
        for (auto h : heads_)
        {
            if (h->getState() == Press)
            {
                role_ = h->getRole();
            }
        }
        if (role_)
        {
            result_ = 0;
            setExit(true);
        }
    }
    if (mode_ == 1)
    {
        for (auto h : heads_)
        {
            if (h->getState() == Press)
            {
                if (h->getResult() == -1)
                {
                    h->setResult(0);
                }
                else
                {
                    h->setResult(-1);
                }
            }
        }
        if (button_all_->getState() == Press)
        {
            //如果已经全选，则是清除
            int all = -1;
            for (auto h : heads_)
            {
                if (h->getResult() != 0)
                {
                    all = 0;
                    break;
                }
            }
            for (auto h : heads_)
            {
                h->setResult(all);
            }
        }
        if (button_ok_->getState() == Press)
        {
            //没有人被选中，不能确定
            for (auto h : heads_)
            {
                if (h->getResult() == 0)
                {
                    setExit(true);
                }
            }
        }
    }
}

void TeamMenu::onPressedCancel()
{
    if (mode_ == 0)
    {
        role_ = nullptr;
        result_ = -1;
        setExit(true);
    }
}

void TeamMenu::dealEvent(BP_Event& e)
{
    Menu::dealEvent(e);
    if (mode_ == 0)
    {
        if (item_)
        {
            for (auto h : heads_)
            {
                if (h->getState() != Normal && !GameUtil::canUseItem(h->getRole(), item_))
                {
                    h->setState(Normal);
                }
            }
        }
    }
    if (mode_ == 1)
    {
        for (auto h : heads_)
        {
            if (h->getResult() == 0)
            {
                h->setText("已選中");
            }
            else
            {
                h->setText("");
            }
        }
        getChild(active_child_)->setState(Press);
    }
}
