#include "BattleMenu.h"
#include "Save.h"
#include "Random.h"
#include "BattleScene.h"
#include "others\libconvert.h"

BattleActionMenu::BattleActionMenu()
{
    setStrings({ "移動", "武學", "用毒", "解毒", "醫療", "物品", "等待", "狀態", "自動", "結束" });
    distance_layer_ = new MapSquare();
    distance_layer_->resize(BATTLEMAP_COORD_COUNT);
}

BattleActionMenu::~BattleActionMenu()
{
    delete distance_layer_;
}

void BattleActionMenu::onEntrance()
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

void BattleActionMenu::dealEvent(BP_Event& e)
{
    if (battle_scene_ == nullptr) { return; }
    if (role_->Auto)
    {
        int act = autoSelect(role_);
        setResult(act);
        setAllChildState(Normal);
        childs_[act]->setState(Press);
        setExit(true);
    }
    else
    {
        Menu::dealEvent(e);
    }
}

//"0移動", "1武學", "2用毒", "3解毒", "4醫療", "5物品", "6等待"
int BattleActionMenu::autoSelect(Role* role)
{
    Random<double> rand;   //梅森旋转法随机数
    rand.set_seed();

    std::vector<int> points(10);
    std::vector<Role*> friends, enemies;

    for (auto r : battle_scene_->battle_roles_)
    {
        if (r->Team == role->Team)
        {
            friends.push_back(r);
        }
        else
        {
            enemies.push_back(r);
        }
    }

    //ai为每种行动评分，武学，用毒，解毒，医疗 -- 暂未使用

    int move_step = 0;
    if (!role->Moved)
    {
        //如果还没有移动过，则计算本轮的策略
        //计算
        battle_scene_->calSelectLayer(role, 0, battle_scene_->calMoveStep(role));


        //若自身生命低于20%，0.8概率考虑吃药
        if (role->HP < 0.2 * role->MaxHP)
        {
            points[5] = 0;
        }

        if (role->MP < 0.2 * role->MaxMP)
        {
            points[5] = 0;
        }

        if (role->Morality > 50)
        {
            //会解毒的，检查队友中有无中毒较深者，接近并解毒
            if (childs_[3]->getVisible())
            {
                for (auto r : friends)
                {
                    if (r->Poison > 50)
                    {

                    }
                }


                points[3] == 0;
            }

            if (childs_[4]->getVisible())
            {
                for (auto r : friends)
                {
                    if (r->HP < 0.2 * r->MaxHP)
                    {

                    }
                }
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

        //使用武学
        if (childs_[1]->getVisible())
        {
            Role* r2 = nullptr;
            int min_dis = 4096;
            for (auto r : enemies)
            {
                auto cur_dis = battle_scene_->calDistance(role, r);
                if (cur_dis < min_dis)
                {
                    r2 = r;
                    min_dis = cur_dis;
                }
            }
            role->AI_ActionX = r2->X();
            role->AI_ActionY = r2->Y();
            calDistanceLayer(role->AI_ActionX, role->AI_ActionY);

            //选择离目标点最近的地点

            min_dis = 4096;
            for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
            {
                for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
                {
                    if (battle_scene_->canSelect(ix, iy))
                    {
                        int cur_dis = distance_layer_->data(ix, iy);
                        if (cur_dis < min_dis || (cur_dis == min_dis && rand.rand() < 0.5))
                        {
                            min_dis = cur_dis;
                            role->AI_MoveX = ix;
                            role->AI_MoveY = iy;
                        }
                    }
                }
            }




            //遍历武学
            int max_hurt = -1;
            for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
            {
                auto magic = Save::getInstance()->getRoleLearnedMagic(role, i);
                if (magic == nullptr) { continue; }
                int level_index = role->getRoleMagicLevelIndex(i);

                battle_scene_->calSelectLayerByMagic(role->AI_MoveX, role->AI_MoveY, role->Team, magic, level_index);

                //对所有能选到的点测试，估算收益
                auto& ix = battle_scene_->select_x_;
                auto& iy = battle_scene_->select_y_;
                for (ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
                {
                    for (iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
                    {
                        int total_hurt = 0;
                        if (battle_scene_->canSelect(ix, iy))
                        {

                            battle_scene_->calEffectLayer(role->X(), role->Y(), magic, level_index);
                            total_hurt = battle_scene_->calAllHurt(role, magic, true);
                            if (total_hurt > max_hurt)
                            {
                                max_hurt = total_hurt;
                                role->AI_Magic = magic;
                                role->AI_ActionX = ix;
                                role->AI_ActionY = iy;
                            }
                            printf("%s (%d,%d): %d\n", magic->Name, ix, iy, total_hurt);
                        }

                    }
                }
            }
            points[1] = 0;
            role->AI_Action = 1;
        }
        //返回移动的计算
        return 0;
    }
    else
    {
        return role->AI_Action;
    }
}

//计算距离
void BattleActionMenu::calDistanceLayer(int x, int y, int max_step/*=64*/)
{
    distance_layer_->setAll(max_step + 1);
    std::vector<Point> cal_stack;
    distance_layer_->data(x, y) = 0;
    cal_stack.push_back({ x, y });
    int count = 0;
    int step = 0;
    while (step <= 64)
    {
        std::vector<Point> cal_stack_next;
        auto check_next = [&](Point p1)->void
        {
            //未计算过且可以走的格子参与下一步的计算
            if (distance_layer_->data(p1.x, p1.y) == max_step + 1 && battle_scene_->canWalk(p1.x, p1.y))
            {
                distance_layer_->data(p1.x, p1.y) = step + 1;
                cal_stack_next.push_back(p1);
                count++;
            }
        };
        for (auto p : cal_stack)
        {
            distance_layer_->data(p.x, p.y) = step;
            check_next({ p.x - 1, p.y });
            check_next({ p.x + 1, p.y });
            check_next({ p.x, p.y - 1 });
            check_next({ p.x, p.y + 1 });
            if (count >= battle_scene_->COORD_COUNT * battle_scene_->COORD_COUNT) { break; }  //最多计算次数，避免死掉
        }
        if (cal_stack_next.size() == 0) { break; }  //无新的点，结束
        cal_stack = cal_stack_next;
        step++;
    }
}

void BattleMagicMenu::onEntrance()
{
    std::vector<std::string> magic_names;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        auto m = Save::getInstance()->getRoleLearnedMagic(role_, i);
        if (m)
        {
            magic_names.push_back(convert::formatString("%s%7d", m->Name, role_->getRoleShowLearnedMagicLevel(i)));
        }
    }
    setStrings(magic_names);
    setPosition(160, 200);
}

void BattleMagicMenu::dealEvent(BP_Event& e)
{
    if (role_ == nullptr) { return; }
    if (role_->Auto)
    {
        magic_ = role_->AI_Magic;
        setAllChildState(Normal);
        setResult(0);
        setExit(true);
    }
    else
    {
        Menu::dealEvent(e);
        magic_ = Save::getInstance()->getRoleLearnedMagic(role_, result_);
    }
}
