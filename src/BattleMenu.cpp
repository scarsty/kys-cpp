#include "BattleMenu.h"
#include "Save.h"
#include "Random.h"
#include "BattleScene.h"
#include "others\libconvert.h"
#include "Event.h"

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

    childs_text_["等待"]->setVisible(false);  //禁用等待

    setFontSize(20);
    arrange(0, 0, 0, 28);
    pass_child_ = findFristVisibleChild();
    forcePassChild();
    role_->AI_Action = -1;  //设置为未计算过ai的行动
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

    //ai为每种行动评分，武学，用毒，解毒，医疗

    std::vector<AIAction> ai_action;

    int move_step = 0;
    if (role->AI_Action == -1)
    {
        //开始计算本轮的策略
        role->AI_Action == getResultFromString("結束");
        role->AI_MoveX = role->X();
        role->AI_MoveY = role->Y();
        role->AI_ActionX = role->X();
        role->AI_ActionY = role->Y();
        role->AI_Magic = nullptr;
        role->AI_Item = nullptr;

        //计算可以移动的位置
        battle_scene_->calSelectLayer(role, 0, battle_scene_->calMoveStep(role));

        //若自身生命低于20%，0.8概率考虑吃药
        if (role->HP < 0.2 * role->MaxHP || role->MP < 0.2 * role->MaxMP)
        {
            AIAction aa;
            aa.Action = getResultFromString("藥品");
            auto bi = new BattleItemMenu();
            bi->setRole(role);
            bi->run();
            aa.item = bi->getCurrentItem();
            if (aa.item)
            {
                //这个分是瞎算的
                aa.point = std::max(aa.item->AddHP, aa.item->AddMP) + role->MaxHP - role->HP;
            }
            //选择离所有敌方距离和最大的点
            int max_dis = 0;
            int k = 2;  //这个概率计算方法是出现指标相同的点时，选到的概率相同
            for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
            {
                for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
                {
                    if (battle_scene_->canSelect(ix, iy))
                    {
                        int cur_dis = 0;
                        for (auto r2 : enemies)
                        {
                            cur_dis += battle_scene_->calDistance(role, r2);
                        }
                        if (cur_dis > max_dis || (cur_dis == max_dis && rand.rand() < 1.0 / (k++)))
                        {
                            max_dis = cur_dis;
                            aa.MoveX = ix;
                            aa.MoveY = iy;
                        }
                    }
                }
            }
            ai_action.push_back(aa);
        }

        //解毒，医疗，用毒的行为与道德相关
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
            }

            if (childs_text_["醫療"]->getVisible())
            {
                for (auto r : friends)
                {
                    if (r->HP < 0.2 * r->MaxHP)
                    {

                    }
                }
            }
        }
        else
        {
            //考虑用毒
            if (childs_text_["用毒"]->getVisible())
            {

            }
        }

        //使用武学
        if (childs_text_["武學"]->getVisible())
        {
            AIAction aa;
            aa.Action = getResultFromString("武學");

            Role* r2 = nullptr;
            int min_dis = 4096;
            //选择离得最近的敌人
            for (auto r : enemies)
            {
                auto cur_dis = battle_scene_->calDistance(role, r);
                if (cur_dis < min_dis)
                {
                    r2 = r;
                    min_dis = cur_dis;
                }
            }
            aa.ActionX = r2->X();
            aa.ActionY = r2->Y();
            calDistanceLayer(aa.ActionX, aa.ActionY);

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
                            aa.MoveX = ix;
                            aa.MoveY = iy;
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

                battle_scene_->calSelectLayerByMagic(aa.MoveX, aa.MoveY, role->Team, magic, level_index);

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
                                aa.magic = magic;
                                aa.ActionX = ix;
                                aa.ActionY = iy;
                            }
                            printf("AI %s use %s to attack (%d, %d) will get hurt: %d\n", role->Name, magic->Name, ix, iy, total_hurt);
                        }
                    }
                }
            }
            aa.point = max_hurt / 2;
            ai_action.push_back(aa);
        }

        //查找最大评分的行动
        int point = -1;
        for (auto aa : ai_action)
        {
            if (aa.point >= point)
            {
                point = aa.point;
                setAIAction(&aa, role);
            }
        }
    }

    if (!role->Moved)
    {
        //未移动则返回移动
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

void BattleItemMenu::dealEvent(BP_Event& e)
{
    if (role_ == nullptr) { return; }
    if (role_->isAuto())
    {
        if (role_->AI_Item)
        {
            current_item_ = role_->AI_Item;
            return;
        }
        std::vector<Item*> items;
        //选出物品列表
        if (role_->Team == 0)
        {
            getAvailableItem(force_item_type_);
        }
        else
        {
            available_items_.clear();
            for (int i = 0; i < ROLE_TAKING_ITEM_COUNT; i++)
            {
                auto item = Save::getInstance()->getItem(role_->TakingItem[i]);
                if (getItemDetailType(item) == force_item_type_)
                {
                    available_items_.push_back(item);
                }
            }
        }
        //随机选一个
        current_item_ = getAvailableItem(RandomClassical::rand(available_items_.size()));
        setExit(true);
        setVisible(false);
    }
    else
    {
        UIItem::dealEvent(e);
    }
}

void BattleItemMenu::addItem(Item* item, int count)
{
    if (role_->Team == 0)
    {
        Event::getInstance()->addItemWithoutHint(item->ID, count);
    }
    else
    {
        Event::getInstance()->roleAddItem(role_->ID, item->ID, count);
    }
}
