﻿#pragma once
#include "Head.h"
#include "RunNode.h"
#include "Types.h"
#include "UIItem.h"
#include "UIStatus.h"
#include "UISystem.h"

class UI : public RunNode
{
public:
    UI();
    ~UI();
    //UI单例即可，无需重复创建
private:
    int current_head_ = 0;
    int current_button_ = 0;

    int ui_index_ = 0;

public:
    void onEntrance() override;
    void draw() override;
    void dealEvent(EngineEvent& e) override;
    void backRun() override;

    static std::shared_ptr<UI> getInstance()
    {
        static std::shared_ptr<UI> ui = std::make_shared<UI>();
        return ui;
    }

    std::shared_ptr<Menu> heads_;

    std::shared_ptr<Button> button_status_, button_item_, button_system_;
    std::shared_ptr<UIStatus> ui_status_;
    std::shared_ptr<UIItem> ui_item_;
    std::shared_ptr<UISystem> ui_system_;
    int item_id_ = -1;

    void onPressedOK() override;
    DEFAULT_CANCEL_EXIT;
    Item* getUsedItem() { return ui_item_->getCurrentItem(); }

    std::vector<std::shared_ptr<Head>> heads_ptrs_;    //方便一些操作
};
