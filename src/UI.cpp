﻿#include "UI.h"
#include "GameUtil.h"
#include "Head.h"
#include "Save.h"

UI::UI()
{
    button_status_ = std::make_shared<Button>();
    button_status_->setTexture("title", 122);
    button_status_->setAlpha(192);
    button_item_ = std::make_shared<Button>();
    button_item_->setTexture("title", 124);
    button_item_->setAlpha(192);
    button_system_ = std::make_shared<Button>();
    button_system_->setTexture("title", 125);
    button_system_->setAlpha(192);
    addChild(button_status_, 10, 10);
    addChild(button_item_, 90, 10);
    addChild(button_system_, 170, 10);
    heads_ = std::make_shared<Menu>();
    addChild(heads_);
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = std::make_shared<Head>();
        heads_->addChild(h, 20, 60 + i * 90);
        heads_ptrs_.push_back(h);
    }
    heads_->getChild(0)->setState(NodePass);

    ui_index_ = getChildCount();    //UI的索引，方便在其他地方使用
    //内容最后画，拖拽时显示在头像上方
    //注意，此处约定childs_[ui_index_]为子UI，创建好对应的指针，需要显示哪个赋值到childs_[ui_index_]即可
    ui_status_ = std::make_shared<UIStatus>();
    ui_item_ = std::make_shared<UIItem>();
    ui_system_ = std::make_shared<UISystem>();
    ui_status_->setPosition(300, 0);
    ui_item_->setPosition(300, 0);
    ui_system_->setPosition(300, 0);
    addChild(ui_status_);

    //addChild(heads_);
    setIsDark(1);
    result_ = -1;    //非负：物品id，负数：其他情况，再定
}

UI::~UI()
{
}

void UI::onEntrance()
{
}

void UI::draw()
{
    //Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
}

void UI::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        std::shared_ptr<Head> head = std::dynamic_pointer_cast<Head>(heads_->getChild(i));
        auto role = Save::getInstance()->getTeamMate(i);
        head->setRole(role);
        head->setVisible(role != nullptr);
        if (role == nullptr)
        {
            continue;
        }
        if (head->getState() == NodePass)
        {
            ui_status_->setRole(role);
            current_head_ = i;
        }
        head->setText("");
        //如在物品栏则判断是否在使用，或者可以使用，设置对应的头像状态
        if (childs_[ui_index_] == ui_item_)
        {
            Item* item = ui_item_->getCurrentItem();
            if (item)
            {
                if (role->Equip0 == item->ID || role->Equip1 == item->ID || role->PracticeItem == item->ID)
                {
                    head->setText("使用中");
                    //Font::getInstance()->draw("使用中", 25, x + 5, y + 60, { 255,255,255,255 });
                }
                if (role->canUseItem(item))
                {
                    head->setState(NodePass);
                }
            }
        }
    }

    //这里设定当前头像为Pass，令其不变暗，因为检测事件是先检测子节点，所以这里可以生效
    if (childs_[ui_index_] == ui_status_)
    {
        //heads_->getChild(current_head_)->setState(NodePass);
    }
    childs_[current_button_]->setState(NodePass);

    //快捷键切换
    {
        int cb = current_button_;
        if (e.type == EVENT_KEY_UP)
        {
            switch (e.key.key)
            {
            case K_1:
                cb = 0;
                break;
            case K_2:
                cb = 1;
                break;
            case K_3:
                cb = 2;
                break;
            default:
                break;
            }
        }

        if (engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFT_TRIGGER))
        {
            cb = GameUtil::limit(cb - 1, 0, 3);
            engine->setInterValControllerPress(200);
        }
        if (engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHT_TRIGGER))
        {
            cb = GameUtil::limit(cb + 1, 0, 3);
            engine->setInterValControllerPress(200);
        }
        if (cb != current_button_)
        {
            current_button_ = cb;
            switch (current_button_)
            {
            case 0:
                childs_[ui_index_] = ui_status_;
                setAllChildState(NodeNormal);
                button_status_->setState(NodePress);
                break;
            case 1:
                childs_[ui_index_] = ui_item_;
                setAllChildState(NodeNormal);
                button_item_->setState(NodePress);
                break;
            case 2:
                childs_[ui_index_] = ui_system_;
                setAllChildState(NodeNormal);
                button_system_->setState(NodePress);
                break;
            default:
                break;
            }
        }
    }

    heads_->setDealEvent(0);
    //仅在状态部分，左侧头像才接收事件
    if (childs_[ui_index_] == ui_status_)
    {
        heads_->setDealEvent(1);
        heads_->setUDStyle(1);
    }
    else if (childs_[ui_index_] == ui_item_)
    {
        Role* r = nullptr;
        for (auto& h : heads_->getChilds())
        {
            if (h->mouseIn())
            {
                r = std::dynamic_pointer_cast<Head>(h)->getRole();
                break;
            }
        }
        ui_item_->setTryRole(r);
    }
}

void UI::backRun()
{
    if (childs_[ui_index_] == ui_status_)
    {
        for (auto& h : heads_ptrs_)
        {
            if (h->getRole() == ui_status_->getRole())
            {
                h->setState(NodePress);    //当前人物常亮
            }
        }
    }

    if (childs_[ui_index_] == ui_system_)
    {
        for (auto& h : heads_->getChilds())
        {
            h->setState(NodePress);    //系统菜单不需要头像变暗
        }
    }
}

void UI::onPressedOK()
{
    //这里检测是否使用了物品，返回物品的id
    if (childs_[ui_index_] == ui_item_)
    {
        auto item = ui_item_->getCurrentItem();
        if (item && item->ItemType == 0)
        {
            setExit(true);
            return;
        }
    }

    if (childs_[ui_index_] == ui_system_)
    {
        if (ui_system_->getResult() == 0)
        {
            setExit(true);
            return;
        }
    }

    //按钮的响应
    if (button_status_->getState() == NodePress)
    {
        childs_[ui_index_] = ui_status_;
        current_button_ = button_status_->getTag();
    }
    if (button_item_->getState() == NodePress)
    {
        childs_[ui_index_] = ui_item_;
        current_button_ = button_item_->getTag();
    }
    if (button_system_->getState() == NodePress)
    {
        childs_[ui_index_] = ui_system_;
        current_button_ = button_system_->getTag();
    }
    //current_button_ = childs_[0]->getTag();
}
