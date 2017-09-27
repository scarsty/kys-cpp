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
    //注意，此处约定前MAX_TEAMMATE_COUNT个子项为小头像，后面连续4个为切换用的按钮
    for (int i = 0; i < MAX_TEAMMATE_COUNT; i++)
    {
        auto h = new Head();
        addChild(h, 10, i * 80);
        h->setSize(200, 80);
        heads_.push_back(h);
    }
    //此处创建好对应的指针，需要显示哪个就将childs_末尾的指针替换即可
    uistatus_ = new UIStatus();
    uiskill_ = new UISkill();
    uiitem_ = new UIItem();
    uisystem_ = new UISystem();
    addChild(uistatus_, 200, 0);
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
    for (int i = 0; i < MAX_TEAMMATE_COUNT; i++)
    {
        heads_[i]->setRole(Save::getInstance()->getTeamMate(i));
        if (heads_[i]->getState() != Normal)
        {
            uistatus_->setRole(heads_[i]->getRole());
            uiskill_->setRole(heads_[i]->getRole());
        }
    }
}

void UI::dealEvent(BP_Event& e)
{
    if (e.type == BP_KEYUP)
    {
        if (e.key.keysym.sym == BPK_ESCAPE)
        {
            loop_ = false;
        }
    }
}
