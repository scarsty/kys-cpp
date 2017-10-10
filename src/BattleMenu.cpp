#include "BattleMenu.h"
#include "Save.h"
#include "Random.h"

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
        c->setState(Normal);
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
    childs_[findNextVisibleChild(getChildCount() - 1, 1)]->setState(Pass);
}

int BattleMenu::runAsRole(Role* r)
{
    setRole(r);
    return run();
}

void BattleMenu::dealEvent(BP_Event& e)
{

    if (battle_scene_ == nullptr) { return; }
    if (role_->Auto)
    {
        setResult(autoSelect(role_));
    }
    else
    {
        Menu::dealEvent(e);
    }
}

//"0移動", "1武學", "2用毒", "3解毒", "4醫療", "5物品", "6等待"
int BattleMenu::autoSelect(Role* r)
{
    Random<double> rand;   //梅森旋转法随机数
    rand.set_seed();

    std::vector<int> points(10);
    //ai为每种行动评分，武学，用毒，解毒，医疗

    //若自身生命低于20%，0.8概率考虑吃药
    if (r->HP < 0.2*r->MaxHP)
    {
        points[5] = 0;
    }

    if (r->MP < 0.2*r->MaxMP)
    {
        points[5] = 0;
    }

    if (r->Morality > 50)
    {
        //会解毒的，检查队友中有无中毒较深者，接近并解毒
        if (childs_[3]->getVisible())
        {
            points[3] == 0;
        }

        if (childs_[4]->getVisible())
        {
            points[4] == 0;
        }
    }
    else
    {
        //考虑用毒
        if (childs_[2]->getVisible())
        {
            points[2] == 0;
        }
    }

    if (childs_[1]->getVisible())
    {
        points[1] = 0;
    }

    return 0;
}

