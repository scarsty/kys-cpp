#include "UI.h"
#include "Types.h"
#include "Save.h"
#include "Head.h"
#include "UIStatus.h"
#include "UISkill.h"
#include "UIItem.h"
#include "UISystem.h"

UI UI::ui_;

UI::UI()
{
    //注意，此处约定childs_[0]为子UI，创建好对应的指针，需要显示哪个赋值到childs_[0]即可
    ui_status_ = new UIStatus();
    ui_skill_ = new UISkill();
    ui_item_ = new UIItem();
    ui_system_ = new UISystem();
    ui_status_->setPosition(300, 0);
    ui_skill_->setPosition(300, 0);
    ui_item_->setPosition(300, 0);
    ui_system_->setPosition(300, 0);
    addChild(ui_status_);

    //貌似这里不能直接调用其他单例，静态量的创建顺序不确定
    button_status_ = new Button("title", 122);
    button_skill_ = new Button("title", 123);
    button_item_ = new Button("title", 124);
    button_system_ = new Button("title", 125);
    addChild(button_status_, 10, 10);
    addChild(button_skill_, 90, 10);
    addChild(button_item_, 170, 10);
    addChild(button_system_, 250, 10);
    buttons_.push_back(button_status_);
    buttons_.push_back(button_skill_);
    buttons_.push_back(button_item_);
    buttons_.push_back(button_system_);

    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        addChild(h, 20, 60 + i * 90);
        heads_.push_back(h);
    }
    heads_[0]->setState(Pass);
    result_ = -1; //非负：物品id，负数：其他情况，再定
}


UI::~UI()
{
    childs_[0] = nullptr;
    delete ui_status_;
    delete ui_skill_;
    delete ui_item_;
    delete ui_system_;
}

void UI::onEntrance()
{
}

void UI::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 192 }, 0, 0, -1, -1);
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        heads_[i]->setRole(Save::getInstance()->getTeamMate(i));
        if (heads_[i]->getState() == Pass)
        {
            auto role = heads_[i]->getRole();
            if (role)
            {
                ui_status_->setRole(role);
                ui_skill_->setRole(role);
                current_head_ = i;
            }
        }
    }
}

void UI::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        //这里检测是否使用了物品，返回物品的id
        if (childs_[0] == ui_item_)
        {
            result_ = ui_item_->getResult();
            if (result_ >= 0)
            {
                setExit(true);
            }
        }
        if (button_status_->getState() == Press)
        {
            childs_[0] = ui_status_;
            current_button_ = 0;
        }
        if (button_skill_->getState() == Press)
        {
            childs_[0] = ui_skill_;
            current_button_ = 1;
        }
        if (button_item_->getState() == Press)
        {
            childs_[0] = ui_item_;
            current_button_ = 2;
        }
        if (button_system_->getState() == Press)
        {
            childs_[0] = ui_system_;
            current_button_ = 3;
        }
    }
    //这里设定当前头像为Press，令其不变暗，因为检测事件是先检测子节点，所以这里可以生效
    heads_[current_head_]->setState(Press);
    buttons_[current_button_]->setState(Press);
}
