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
    if (role->Moved)
    {
        childs_[0]->setVisible(false);
    }
    //用毒
    if (role->UsePoison <= 0)
    {
        childs_[2]->setVisible(false);
    }
    //解毒
    if (role->Detoxification <= 0)
    {
        childs_[3]->setVisible(false);
    }
    //医疗
    if (role->Medcine <= 0)
    {
        childs_[4]->setVisible(false);
    }
    
    arrange(0, 0, 0, 28);
}

int BattleMenu::runAsRole(Role* r)
{
    setRole(r);
    return run();
}


