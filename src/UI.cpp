#include "UI.h"
#include "Types.h"
#include "Save.h"
#include "Head.h"
#include "UIStatus.h"
#include "UISkill.h"
#include "UIItem.h"
#include "UISystem.h"

UI* UI::ui_;

UI::UI()
{
    //注意，此处约定childs_[0]为子UI，创建好对应的指针，需要显示哪个赋值到childs_[0]即可
    ui_status_ = new UIStatus();
    ui_skill_ = new UISkill();
    ui_item_ = new UIItem();
    ui_system_ = new UISystem();
    addChild(ui_status_, 300, 0);

    //貌似这里不能直接调用其他单例，静态量的创建顺序不确定
    button_status_ = new Button("mmap", 2022);
    button_skill_ = new Button("mmap", 2023);
    button_item_ = new Button("mmap", 2024);
    button_system_ = new Button("mmap", 2025);
    addChild(button_status_, 710, 20);
    addChild(button_skill_, 710, 60);
    addChild(button_item_, 710, 100);
    addChild(button_system_, 710, 140);

    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        addChild(h, 10, i * 80);
        h->setSize(200, 80);
        heads_.push_back(h);
    }
    heads_[0]->setState(Pass);
    instance_ = true;
}


UI::~UI()
{
}

void UI::entrance()
{
}

void UI::draw()
{
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        heads_[i]->setRole(Save::getInstance()->getTeamMate(i));
        if (heads_[i]->getState() != Normal)
        {
            ui_status_->setRole(heads_[i]->getRole());
            ui_skill_->setRole(heads_[i]->getRole());
        }
    }
}

void UI::dealEvent(BP_Event& e)
{
    if (e.type == BP_MOUSEBUTTONUP)
    {
        if (button_status_->getState() == Press) { childs_[0] = ui_status_; }
        if (button_skill_->getState() == Press) { childs_[0] = ui_skill_; }
        if (button_item_->getState() == Press) { childs_[0] = ui_item_; }
        if (button_system_->getState() == Press) { childs_[0] = ui_system_; }
    }
    if (e.type == BP_KEYUP)
    {
        if (e.key.keysym.sym == BPK_ESCAPE)
        {
            loop_ = false;
        }
    }
}
