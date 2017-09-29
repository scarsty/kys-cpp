#pragma once
#include "Base.h"
//#include "Head.h"
//#include "UIStatus.h"
//#include "UISkill.h"
//#include "UIItem.h"
//#include "UISystem.h"

//前置声明，减少编译依赖
class Head;
class Button;
class UIStatus;
class UISkill;
class UIItem;
class UISystem;

class UI : public Base
{
private:
    UI();
    ~UI();
    //UI菜单单例即可，无需重复创建
    static UI ui_;
public:
    virtual void entrance() override;
    virtual void draw() override;
    virtual void dealEvent(BP_Event& e) override;

    static UI* getInstance() { return &ui_; }

    std::vector<Head*> heads_;

    Button* button_status_, *button_skill_, *button_item_, *button_system_;
    UIStatus* ui_status_ = nullptr;
    UISkill* ui_skill_ = nullptr;
    UIItem* ui_item_ = nullptr;
    UISystem* ui_system_ = nullptr;
    int item_id_ = -1;
};

