#include "UI.h"
#include "Font.h"
#include "GameUtil.h"
#include "Save.h"

UI::UI()
{
    //注意，此处约定childs_[0]为子UI，创建好对应的指针，需要显示哪个赋值到childs_[0]即可
    ui_status_ = std::make_shared<UIStatus>();
    ui_item_ = std::make_shared<UIItem>();
    ui_system_ = std::make_shared<UISystem>();
    ui_status_->setPosition(300, 0);
    ui_item_->setPosition(300, 0);
    ui_system_->setPosition(300, 0);
    addChild(ui_status_);

    //貌似这里不能直接调用其他单例，静态量的创建顺序不确定
    button_status_ = std::make_shared<Button>();
    button_status_->setTexture("title", 122);
    button_item_ = std::make_shared<Button>();
    button_item_->setTexture("title", 124);
    button_system_ = std::make_shared<Button>();
    button_system_->setTexture("title", 125);
    addChild(button_status_, 10, 10);
    addChild(button_item_, 90, 10);
    addChild(button_system_, 170, 10);
    heads_ = std::make_shared<Menu>();
    addChild(heads_);
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        heads_->addChild(std::make_shared<Head>(), 20, 60 + i * 90);
    }
    heads_->getChild(0)->setState(Pass);
    //addChild(heads_);
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
    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
}

void UI::dealEvent(BP_Event& e)
{
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        std::shared_ptr<Head> head = std::dynamic_pointer_cast<Head>(heads_->getChild(i));
        auto role = Save::getInstance()->getTeamMate(i);
        head->setRole(role);
        if (role == nullptr)
        {
            continue;
        }
        if (head->getState() == Pass)
        {
            ui_status_->setRole(role);
            current_head_ = i;
        }
        head->setText("");
        //如在物品栏则判断是否在使用，或者可以使用，设置对应的头像状态
        if (childs_[0] == ui_item_)
        {
            Item* item = ui_item_->getCurrentItem();
            if (item)
            {
                if (role->Equip0 == item->ID || role->Equip1 == item->ID || role->PracticeItem == item->ID)
                {
                    head->setText("使用中");
                    //Font::getInstance()->draw("使用中", 25, x + 5, y + 60, { 255,255,255,255 });
                }
                if (GameUtil::canUseItem(role, item))
                {
                    head->setState(Pass);
                }
            }
        }
    }

    //这里设定当前头像为Pass，令其不变暗，因为检测事件是先检测子节点，所以这里可以生效
    if (childs_[0] == ui_status_)
    {
        heads_->getChild(current_head_)->setState(Pass);
    }
    childs_[current_button_]->setState(Pass);

    //快捷键切换
    if (e.type == BP_KEYUP)
    {
        switch (e.key.keysym.sym)
        {
        case BPK_1:
            childs_[0] = ui_status_;
            setAllChildState(Normal);
            button_status_->setState(Press);
            current_button_ = 0;
            break;
        case BPK_2:
            childs_[0] = ui_item_;
            setAllChildState(Normal);
            button_item_->setState(Press);
            current_button_ = 1;
            break;
        case BPK_3:
            childs_[0] = ui_system_;
            setAllChildState(Normal);
            button_system_->setState(Press);
            current_button_ = 2;
            break;
        default:
            break;
        }
    }

    //仅在状态部分，左侧头像才接收事件
    if (childs_[0] == ui_status_)
    {
        heads_->setDealEvent(1);
    }
    else
    {
        heads_->setDealEvent(0);
    }
}

void UI::onPressedOK()
{
    //这里检测是否使用了物品，返回物品的id
    if (childs_[0] == ui_item_)
    {
        auto item = ui_item_->getCurrentItem();
        if (item && item->ItemType == 0)
        {
            setExit(true);
        }
    }

    if (childs_[0] == ui_system_)
    {
        if (ui_system_->getResult() == 0)
        {
            setExit(true);
        }
    }

    //四个按钮的响应
    if (button_status_->getState() == Press)
    {
        childs_[0] = ui_status_;
        current_button_ = button_status_->getTag();
    }
    if (button_item_->getState() == Press)
    {
        childs_[0] = ui_item_;
        current_button_ = button_item_->getTag();
    }
    if (button_system_->getState() == Press)
    {
        childs_[0] = ui_system_;
        current_button_ = button_system_->getTag();
    }
}
