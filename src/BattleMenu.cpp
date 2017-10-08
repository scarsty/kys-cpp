#include "BattleMenu.h"
#include "Save.h"

BattleMenu::BattleMenu()
{
    setStrings({ "移動", "武學", "用毒", "解毒", "醫療", "物品", "等待", "狀態", "自動", "結束" });
}

BattleMenu::~BattleMenu()
{
}

void BattleMenu::onEntrance()
{
    for (auto c : childs_)
    {
        c->setVisible(true);
    }

    //移动过则不可移动
    if (role_->Moved || role_->PhysicalPower < 10)
    {
        childs_[0]->setVisible(false);
    }
    //武学
    if (role_->getLearnedMagicCount() <= 0 || role_->PhysicalPower < 20)
    {
        childs_[1]->setVisible(false);
    }
    //用毒
    if (role_->UsePoison <= 0 || role_->PhysicalPower < 30)
    {
        childs_[2]->setVisible(false);
    }
    //解毒
    if (role_->Detoxification <= 0 || role_->PhysicalPower < 30)
    {
        childs_[3]->setVisible(false);
    }
    //医疗
    if (role_->Medcine <= 0 || role_->PhysicalPower < 10)
    {
        childs_[4]->setVisible(false);
    }
    //禁用等待
    childs_[6]->setVisible(false);
    setFontSize(20);
    arrange(0, 0, 0, 28);
}

int BattleMenu::runAsRole(Role* r)
{
    setRole(r);
    return run();
}

void BattleMenu::dealEvent(BP_Event& e)
{
    Menu::dealEvent(e);
    if (battle_scene_ == nullptr) { return; }
    if (role_)
    {
    }
}

