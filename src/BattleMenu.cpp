#include "BattleMenu.h"
#include "BattleScene.h"
#include "Event.h"
#include "PotConv.h"
#include "Random.h"
#include "Save.h"
#include "convert.h"

BattleActionMenu::BattleActionMenu(BattleScene* b)
{
    setStrings({ "移動", "武學", "用毒", "解毒", "醫療", "暗器", "藥品", "等待", "狀態", "自動", "結束" });
    distance_layer_.resize(BATTLEMAP_COORD_COUNT);
    battle_scene_ = b;
}

BattleActionMenu::~BattleActionMenu()
{
}

void BattleActionMenu::setRole(Role* r)
{
    role_ = r;
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
    if (role_->Medicine <= 0 || role_->PhysicalPower < 10)
    {
        childs_text_["醫療"]->setVisible(false);
    }
    if (role_->HiddenWeapon <= 15 || role_->PhysicalPower < 10)
    {
        childs_text_["暗器"]->setVisible(false);
    }

    if (role_->Competing)
    {
        childs_text_["藥品"]->setVisible(false);
        childs_text_["暗器"]->setVisible(false);
    }

    childs_text_["等待"]->setVisible(false);    //禁用等待

    setFontSize(20);
    arrange(0, 0, 0, 28);
    if (start_ == 0 || !getChild(active_child_)->getVisible())
    {
        active_child_ = findFristVisibleChild();
        forceActiveChild();
    }
    if (!role_->Moved)
    {
        role_->AI_Action = -1;
    }    //设置为未计算过ai的行动
}

void BattleActionMenu::dealEvent(BP_Event& e)
{
    if (battle_scene_ == nullptr)
    {
        return;
    }
    //如果是ai，计算行动，不再进入其他循环
    if (role_->isAuto())
    {
        int act = autoSelect(role_);
        setResult(act);
        setAllChildState(Normal);
        childs_[act]->setState(Press);
        setExit(true);
        setVisible(false);    //AI不画菜单了，太乱
        return;
    }
    Menu::dealEvent(e);
}

//"0移動", "1武學", "2用毒", "3解毒", "4醫療", "5暗器", "6藥品", "7等待", "8狀態", "9自動", "10結束"
int BattleActionMenu::autoSelect(Role* role)
{
    std::vector<Role*> friends, enemies;
    for (auto r : battle_scene_->getBattleRoles())
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

    //ai为每种行动评分
    std::vector<AIAction> ai_action;

    Random<double> rand;    //梅森旋转法随机数
    rand.set_seed();
    rand.set_parameter(0, 1);

    if (role->AI_Action == -1)
    {
        auto _role_temp = *role;
        auto role_temp = &_role_temp;    //临时人物指针，用于一些含有距离衰减的计算

        //开始计算本轮的策略
        role->AI_Action = getResultFromString("結束");
        role->AI_MoveX = role->X();
        role->AI_MoveY = role->Y();
        role->AI_ActionX = role->X();
        role->AI_ActionY = role->Y();
        role->AI_Magic = nullptr;
        role->AI_Item = nullptr;

        //若有一方为空，没有战斗下去的意义
        if (!friends.empty() && !enemies.empty())
        {
            //计算可以移动的位置
            battle_scene_->calSelectLayer(role, 0, battle_scene_->calMoveStep(role));

            //考虑吃药
            std::string action_text = "藥品";
            if (childs_text_[action_text]->getVisible() && (role->HP < 0.2 * role->MaxHP || role->MP < 0.2 * role->MaxMP || role_->PhysicalPower < 0.2 * Save::getInstance()->MaxPhysicalPower))
            {
                AIAction aa;
                aa.Action = getResultFromString(action_text);
                auto items = BattleItemMenu::getAvaliableItems(role, 2);
                for (auto item : items)
                {
                    //分数计算，后面的差是尽量吃刚刚好的药
                    aa.point = 0;
                    if (item->AddHP > 0)
                    {
                        aa.point += (std::min)(item->AddHP, role->MaxHP - role->HP) - item->AddHP / 10;
                    }
                    if (item->AddMP > 0)
                    {
                        aa.point += (std::min)(item->AddMP, role->MaxMP - role->MP) / 2 - item->AddMP / 10;
                    }
                    else if (item->AddPhysicalPower > 0)
                    {
                        aa.point += (std::min)(item->AddPhysicalPower, Save::getInstance()->MaxPhysicalPower - role->PhysicalPower)
                            - item->AddPhysicalPower / 10;
                    }
                    if (aa.point > 0)
                    {
                        aa.item = item;
                        aa.point *= 1.5;    //自保的系数略大
                        getFarthestToAll(role, enemies, aa.MoveX, aa.MoveY);
                        ai_action.push_back(aa);
                    }
                }
            }

            //解毒，医疗，用毒的行为与道德相关
            if (role->Morality > 50)
            {
                action_text = "解毒";
                //会解毒的，检查队友中有无中毒较深者，接近并解毒
                if (childs_text_[action_text]->getVisible())
                {
                    for (auto r2 : friends)
                    {
                        if (r2->Poison > 50)
                        {
                            AIAction aa;
                            calAIActionNearest(r2, aa);                                             //计算目标，和离目标最近的点，下同
                            int action_dis = battle_scene_->calActionStep(role->Detoxification);    //计算可以行动的距离
                            if (action_dis >= calNeedActionDistance(aa))                            //与需要行动的距离比较
                            {
                                //若在距离之内则考虑使用，以下各个都类似
                                aa.Action = getResultFromString(action_text);
                                aa.point = r2->Poison;
                                ai_action.push_back(aa);
                            }
                        }
                    }
                }
                action_text = "醫療";
                if (childs_text_[action_text]->getVisible())
                {
                    for (auto r2 : friends)
                    {
                        if (r2->HP < 0.2 * r2->MaxHP)
                        {
                            AIAction aa;
                            calAIActionNearest(r2, aa);
                            int action_dis = battle_scene_->calActionStep(role->Medicine);
                            if (action_dis >= calNeedActionDistance(aa))
                            {
                                aa.Action = getResultFromString(action_text);
                                aa.point = r2->Medicine;
                                ai_action.push_back(aa);
                            }
                        }
                    }
                }
            }
            else
            {
                //考虑用毒
                action_text = "用毒";
                if (childs_text_[action_text]->getVisible())
                {
                    auto r2 = getNearestRole(role, enemies);
                    AIAction aa;
                    calAIActionNearest(r2, aa);
                    int action_dis = battle_scene_->calActionStep(role->UsePoison);
                    if (action_dis >= calNeedActionDistance(aa))
                    {
                        aa.Action = getResultFromString(action_text);
                        aa.point = (std::min)(Save::getInstance()->MaxPosion - r2->Poison, role->UsePoison) / 2;
                        if (r2->HP < 10)
                        {
                            aa.point = 1;
                        }
                        ai_action.push_back(aa);
                    }
                }
            }

            //使用暗器
            action_text = "暗器";
            if (childs_text_[action_text]->getVisible())
            {
                auto r2 = getNearestRole(role, enemies);
                AIAction aa;
                calAIActionNearest(r2, aa, role_temp);    //临时人物指针用来计算伤害，因为含有距离衰减，武学同
                int action_dis = battle_scene_->calActionStep(role->HiddenWeapon);
                if (action_dis >= calNeedActionDistance(aa))
                {
                    aa.Action = getResultFromString(action_text);
                    auto items = BattleItemMenu::getAvaliableItems(role, 3);
                    for (auto item : items)
                    {
                        aa.point = battle_scene_->calHiddenWeaponHurt(role_temp, r2, item);
                        if (aa.point > r2->HP)
                        {
                            aa.point = r2->HP * 1.25;
                        }
                        aa.point *= 0.5;    //暗器分值略低
                        aa.item = item;
                        ai_action.push_back(aa);
                    }
                }
            }

            //使用武学
            action_text = "武學";
            if (childs_text_[action_text]->getVisible())
            {
                AIAction aa;
                aa.Action = getResultFromString(action_text);
                auto r2 = getNearestRole(role, enemies);
                calAIActionNearest(r2, aa, role_temp);
                //遍历武学
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    int max_hurt = -1;
                    auto magic = Save::getInstance()->getRoleLearnedMagic(role, i);
                    if (magic == nullptr)
                    {
                        continue;
                    }
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
                                battle_scene_->calEffectLayer(aa.MoveX, aa.MoveY, ix, iy, magic, level_index);
                                total_hurt = battle_scene_->calMagiclHurtAllEnemies(role_temp, magic, true);
                                if (total_hurt > max_hurt)
                                {
                                    max_hurt = total_hurt;
                                    aa.magic = magic;
                                    aa.ActionX = ix;
                                    aa.ActionY = iy;
                                }
                                if (total_hurt > -1)
                                {
                                    //fmt::print("AI %s %s (%d, %d): %d\n", PotConv::to_read(role->Name).c_str(), PotConv::to_read(magic->Name).c_str(), ix, iy, total_hurt);
                                }
                            }
                        }
                    }
                    aa.point = max_hurt;
                    //if (role->AttackTwice) { aa.point *= 2; }
                    ai_action.push_back(aa);
                }
            }
        }
        //查找最大评分的行动
        double max_point = -1;
        for (auto aa : ai_action)
        {
            fmt::print("AI {}: {} ", role->Name, getStringFromResult(aa.Action));
            if (aa.item) { fmt::print("{} ", aa.item->Name); }
            if (aa.magic) { fmt::print("{} ", aa.magic->Name); }
            double r = rand.rand() * 10;    //用于同分的情况，可以随机选择
            fmt::print("score {:.2f}({:.2f})\n", aa.point, r);
            //若评分仅有一个随机数的值，说明不在范围内，仅移动并结束
            if (aa.point == 0)
            {
                aa.Action = getResultFromString("結束");
            }
            double p = aa.point + r;
            if (p > max_point)
            {
                max_point = p;
                setAIActionToRole(aa, role);
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
void BattleActionMenu::calDistanceLayer(int x, int y, int max_step /*=64*/)
{
    distance_layer_.setAll(max_step + 1);
    std::vector<Point> cal_stack;
    distance_layer_.data(x, y) = 0;
    cal_stack.push_back({ x, y });
    int count = 0;
    int step = 0;
    while (step <= 64)
    {
        std::vector<Point> cal_stack_next;
        auto check_next = [&](Point p1) -> void
        {
            //未计算过且可以走的格子参与下一步的计算
            if (distance_layer_.data(p1.x, p1.y) == max_step + 1 && battle_scene_->canWalk(p1.x, p1.y))
            {
                distance_layer_.data(p1.x, p1.y) = step + 1;
                cal_stack_next.push_back(p1);
                count++;
            }
        };
        for (auto p : cal_stack)
        {
            distance_layer_.data(p.x, p.y) = step;
            check_next({ p.x - 1, p.y });
            check_next({ p.x + 1, p.y });
            check_next({ p.x, p.y - 1 });
            check_next({ p.x, p.y + 1 });
            if (count >= distance_layer_.squareSize())
            {
                break;
            }    //最多计算次数，避免死掉
        }
        if (cal_stack_next.size() == 0) { break; }    //无新的点，结束
        cal_stack = cal_stack_next;
        step++;
    }
}

void BattleActionMenu::getFarthestToAll(Role* role, std::vector<Role*> roles, int& x, int& y)
{
    Random<double> rand;    //梅森旋转法随机数
    rand.set_seed();
    //选择离所有敌方距离和最大的点
    double max_dis = 0;
    for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
        {
            if (battle_scene_->canSelect(ix, iy))
            {
                double cur_dis = rand.rand();
                for (auto r2 : roles)
                {
                    cur_dis += battle_scene_->calDistance(ix, iy, r2->X(), r2->Y());
                }
                if (cur_dis > max_dis)
                {
                    max_dis = cur_dis;
                    x = ix;
                    y = iy;
                }
            }
        }
    }
}

void BattleActionMenu::getNearestPosition(int x0, int y0, int& x, int& y)
{
    Random<double> rand;    //梅森旋转法随机数
    rand.set_seed();
    calDistanceLayer(x0, y0);
    double min_dis = BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT;
    for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
        {
            if (battle_scene_->canSelect(ix, iy))
            {
                double cur_dis = distance_layer_.data(ix, iy) + rand.rand();
                if (cur_dis < min_dis)
                {
                    min_dis = cur_dis;
                    x = ix;
                    y = iy;
                }
            }
        }
    }
}

Role* BattleActionMenu::getNearestRole(Role* role, std::vector<Role*> roles)
{
    int min_dis = 4096;
    Role* r2 = nullptr;
    //选择离得最近的敌人
    for (auto r : roles)
    {
        auto cur_dis = battle_scene_->calRoleDistance(role, r);
        if (cur_dis < min_dis)
        {
            r2 = r;
            min_dis = cur_dis;
        }
    }
    return r2;
}

//需事先计算好可以移动的范围
void BattleActionMenu::calAIActionNearest(Role* r2, AIAction& aa, Role* r_temp)
{
    getNearestPosition(r2->X(), r2->Y(), aa.MoveX, aa.MoveY);
    aa.ActionX = r2->X();
    aa.ActionY = r2->Y();
    if (r_temp)
    {
        r_temp->setPositionOnly(aa.MoveX, aa.MoveY);
    }
}

int BattleActionMenu::calNeedActionDistance(AIAction& aa)
{
    return battle_scene_->calDistance(aa.MoveX, aa.MoveY, aa.ActionX, aa.ActionY);
}

void BattleMagicMenu::onEntrance()
{
    if (role_ == nullptr)
    {
        return;
    }
    if (role_->isAuto())
    {
        magic_ = role_->AI_Magic;
        setAllChildState(Normal);
        setResult(0);
        setExit(true);
        setVisible(false);
        return;
    }
    forceActiveChild();
}

void BattleMagicMenu::setRole(Role* r)
{
    role_ = r;
    result_ = -1;
    magic_ = nullptr;
    setVisible(true);
    std::vector<std::string> magic_names;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        auto m = Save::getInstance()->getRoleLearnedMagic(role_, i);
        if (m)
        {
            magic_names.push_back(fmt::format("%-12s%4d  ", m->Name, role_->getRoleShowLearnedMagicLevel(i)));
        }
        else
        {
            magic_names.push_back("");
        }
    }
    setStrings(magic_names);
    setPosition(160, 200);

    //如果宽度为0的项隐藏
    for (auto child : childs_)
    {
        int w, h;
        child->getSize(w, h);
        if (w <= 0)
        {
            child->setVisible(false);
        }
    }
    arrange(0, 0, 0, 30);
}

void BattleMagicMenu::onPressedOK()
{
    checkActiveToResult();
    magic_ = Save::getInstance()->getRoleLearnedMagic(role_, result_);
    if (magic_)
    {
        setExit(true);
    }
}

BattleItemMenu::BattleItemMenu()
{
    setSelectUser(false);
}

void BattleItemMenu::onEntrance()
{
    if (role_ == nullptr)
    {
        return;
    }
    if (role_->isAuto())
    {
        if (role_->AI_Item)
        {
            current_item_ = role_->AI_Item;
            setExit(true);
            setVisible(false);
        }
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

std::vector<Item*> BattleItemMenu::getAvaliableItems()
{
    //选出物品列表
    if (role_->Team == 0)
    {
        geItemsByType(force_item_type_);
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
    return available_items_;
}

std::vector<Item*> BattleItemMenu::getAvaliableItems(Role* role, int type)
{
    BattleItemMenu item_menu;
    item_menu.setRole(role);
    item_menu.setForceItemType(type);
    return item_menu.getAvaliableItems();
}
