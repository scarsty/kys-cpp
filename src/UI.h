#pragma once
#include "Element.h"
#include "Types.h"
#include "Head.h"
#include "UIStatus.h"
#include "UIItem.h"
#include "UISystem.h"

class UI : public Element
{
private:
    UI();
    ~UI();
    //UI�˵��������ɣ������ظ�����
    int current_head_ = 0;
    int current_button_ = 0;
public:
    virtual void onEntrance() override;
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;

    static UI* getInstance()
    {
        static UI ui;
        return &ui;
    }

    std::vector<Head*> heads_;
    std::vector<Button*> buttons_;

    Button* button_status_, *button_item_, *button_system_;
    UIStatus ui_status_;
    UIItem ui_item_;
    UISystem ui_system_;
    int item_id_ = -1;

    virtual void onPressedOK() override;
    DEFAULT_CANCEL_EXIT;
    Item* getUsedItem() { return ui_item_.getCurrentItem(); }
};

