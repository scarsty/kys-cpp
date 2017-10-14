#include "BattleMenu.h"
#include "Save.h"
#include "Random.h"
#include "BattleScene.h"
#include "others\libconvert.h"

BattleActionMenu::BattleActionMenu()
{
    setStrings({ "移動", "武學", "用毒", "解毒", "醫療", "暗器", "藥品", "等待", "狀態", "自動", "結束" });
    distance_layer_ = new MapSquare();
    distance_layer_->resize(BATTLEMAP_COORD_COUNT);
}

BattleActionMenu::~BattleActionMenu()
{
    delete distance_layer_;
}

void BattleActionMenu::onEntrance()
{
    setVisible(true);
    for (auto c : childs_)
    {
        c->setVisible(true);
        c->setState(Normal);
    }

    //移动过则不可移动
    if (role_->Moved || role_->PhysicalPower < 10)
    {
        childs_text_["移動"]->setVisible(false);
    }
    if (role_->getLearnedMagicCount() <= 0 || role_->PhysicalPower < 20)
    {
        childs_text_["武學"]->setVisible(false);
    }
    if (role_->UsePoison <= 0 || role_->PhysicalPower < 30)
    {
        childs_text_["用毒"]->setVisible(false);
    }
    if (role_->Detoxification <= 0 || role_->PhysicalPower < 30)
    {
        childs_text_["解毒"]->setVisible(false);
    }
    if (role_->Medcine <= 0 || role_->PhysicalPower < 10)
    {
        childs_text_["醫療"]->setVisible(false);
    }
    if (role_->HiddenWeapon <= 15 || role_->PhysicalPower < 10)
    {
        childs_text_["暗器"]->setVisible(false);
    }

    //禁用等待
    childs_text_["等待"]->setVisible(false);

    setFontSize(20);
    arrange(0, 0, 0, 28);
    pass_child_ = findFristVisibleChild();
    forcePassChild();
}

void BattleActionMenu::dealEvent(BP_Event& e)
{
    if (battle_scene_ == nullptr) { return; }
    if (role_->isAuto())
    {
        int act = autoSelect(role_);
        setResult(act);
        setAllChildState(Normal);
        childs_[act]->setState(Press);
        setExit(true);
        setVisible(false);  //AI不画菜单了，太乱
    }
    else
    {
        Menu::dealEvent(e);
    }
}

//"0移動", "1武學", "2用毒", "3解毒", "4醫療", "5暗器", "6藥品", "7等待", "8狀態", "9自動", "10結束"
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
            //points[5] = 0;
        }

        if (role->MP < 0.2 * role->MaxMP)
        {
            //points[5] = 0;
        }

        if (role->Morality > 50)
        {
            //会解毒的，检查队友中有无中毒较深者，接近并解毒
            if (childs_text_["解毒"]->getVisible())
            {
                for (auto r : friends)
                {
                    if (r->Poison > 50)
                    {

                    }
                }


                //points[3] == 0;
            }

            if (childs_text_["醫療"]->getVisible())
            {
                for (auto r : friends)
                {
                    if (r->HP < 0.2 * r->MaxHP)
                    {

                    }
                }
                //points[4] == 0;
            }
        }
        else
        {
            //考虑用毒
            if (childs_text_["用毒"]->getVisible())
            {
                //points[2] == 0;
            }
        }

        //使用武学
        if (childs_text_["武學"]->getVisible())
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
            int k = 2;
            for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
            {
                for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
                {
                    if (battle_scene_->canSelect(ix, iy))
                    {
                        int cur_dis = distance_layer_->data(ix, iy);
                        if (cur_dis < min_dis || (cur_dis == min_dis && rand.rand() < 1.0 / (k++)))
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
                for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
                {
                    for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
                    {
                        int total_hurt = 0;
                        if (battle_scene_->canSelect(ix, iy))
                        {

                            battle_scene_->calEffectLayer(role->X(), role->Y(), ix, iy, magic, level_index);
                            total_hurt = battle_scene_->calAllHurt(role, magic, true);
                            if (total_hurt > max_hurt)
                            {
                                max_hurt = total_hurt;
                                role->AI_Magic = magic;
                                role->AI_ActionX = ix;
                                role->AI_ActionY = iy;
                            }
                            printf("AI %s use %s to attack (%d, %d) will get hurt: %d\n", role->Name, magic->Name, ix, iy, total_hurt);
                        }

                    }
                }
            }
            //points[1] = 0;
            role->AI_Action = getResultFromString("武學");
        }
        //首次必定返回移动
        return getResultFromString("移動");
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
            if (count >= distance_layer_->squareSize()) { break; }  //最多计算次数，避免死掉
        }
        if (cal_stack_next.size() == 0) { break; }  //无新的点，结束
        cal_stack = cal_stack_next;
        step++;
    }
}

void BattleMagicMenu::onEntrance()
{
    setVisible(true);
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
    if (role_->isAuto())
    {
        magic_ = role_->AI_Magic;
        setAllChildState(Normal);
        setResult(0);
        setExit(true);
        setVisible(false);
    }
    else
    {
        Menu::dealEvent(e);
    }
}

void BattleMagicMenu::onPressedOK()
{
    magic_ = Save::getInstance()->getRoleLearnedMagic(role_, result_);
    setExit(true);
}

BattleItemMenu::BattleItemMenu()
{
    setSelectUser(false);
}
