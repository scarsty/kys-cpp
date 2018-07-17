#include "Font.h"
#include "GameUtil.h"
#include "Save.h"
#include "UI.h"

UI::UI()
{
    //ע�⣬�˴�Լ��childs_[0]Ϊ��UI�������ö�Ӧ��ָ�룬��Ҫ��ʾ�ĸ���ֵ��childs_[0]����
    ui_status_.setPosition(300, 0);
    ui_item_.setPosition(300, 0);
    ui_system_.setPosition(300, 0);
    addChild(&ui_status_);

    //ò�����ﲻ��ֱ�ӵ���������������̬���Ĵ���˳��ȷ��
    button_status_ = new Button("title", 122);
    button_item_ = new Button("title", 124);
    button_system_ = new Button("title", 125);
    addChild(button_status_, 10, 10);
    addChild(button_item_, 90, 10);
    addChild(button_system_, 170, 10);
    buttons_.push_back(button_status_);
    buttons_.push_back(button_item_);
    buttons_.push_back(button_system_);

    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        addChild(h, 20, 60 + i * 90);
        heads_.push_back(h);
    }
    heads_[0]->setState(Pass);
    result_ = -1;    //�Ǹ�����Ʒid������������������ٶ�
}

UI::~UI()
{
    childs_[0] = nullptr;
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
        auto head = heads_[i];
        auto role = Save::getInstance()->getTeamMate(i);
        head->setRole(role);
        if (role == nullptr)
        {
            continue;
        }
        if (head->getState() == Pass)
        {
            ui_status_.setRole(role);
            current_head_ = i;
        }
        head->setText("");
        //������Ʒ�����ж��Ƿ���ʹ�ã����߿���ʹ�ã����ö�Ӧ��ͷ��״̬
        if (childs_[0] == &ui_item_)
        {
            Item* item = ui_item_.getCurrentItem();
            if (item)
            {
                if (role->Equip0 == item->ID || role->Equip1 == item->ID || role->PracticeItem == item->ID)
                {
                    head->setText("ʹ����");
                    //Font::getInstance()->draw("ʹ����", 25, x + 5, y + 60, { 255,255,255,255 });
                }
                if (GameUtil::canUseItem(role, item))
                {
                    head->setState(Pass);
                }
            }
        }
    }

    //�����趨��ǰͷ��ΪPass�����䲻�䰵����Ϊ����¼����ȼ���ӽڵ㣬�������������Ч
    if (childs_[0] == &ui_status_)
    {
        heads_[current_head_]->setState(Pass);
    }
    buttons_[current_button_]->setState(Pass);
}

void UI::onPressedOK()
{
    //�������Ƿ�ʹ������Ʒ��������Ʒ��id
    if (childs_[0] == &ui_item_)
    {
        auto item = ui_item_.getCurrentItem();
        if (item && item->ItemType == 0)
        {
            setExit(true);
        }
    }

    if (childs_[0] == &ui_system_)
    {
        if (ui_system_.getResult() == 0)
        {
            setExit(true);
        }
    }

    //�ĸ���ť����Ӧ
    if (button_status_->getState() == Press)
    {
        childs_[0] = &ui_status_;
        current_button_ = 0;
    }
    if (button_item_->getState() == Press)
    {
        childs_[0] = &ui_item_;
        current_button_ = 1;
    }
    if (button_system_->getState() == Press)
    {
        childs_[0] = &ui_system_;
        current_button_ = 2;
    }
}
