#include "BattleMenu.h"
#include "Save.h"
#include "Random.h"
#include "BattleScene.h"
#include "libconvert.h"
#include "Event.h"
#include "PotConv.h"

BattleActionMenu::BattleActionMenu()
{
    setStrings({ "�Ƅ�", "��W", "�ö�", "�ⶾ", "�t��", "����", "ˎƷ", "�ȴ�", "��B", "�Ԅ�", "�Y��" });
    distance_layer_ = new MapSquareInt();
    distance_layer_->resize(BATTLEMAP_COORD_COUNT);
}

BattleActionMenu::~BattleActionMenu()
{
    delete distance_layer_;
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

    //�ƶ����򲻿��ƶ�
    if (role_->Moved || role_->PhysicalPower < 10)
    {
        childs_text_["�Ƅ�"]->setVisible(false);
    }
    if (role_->getLearnedMagicCount() <= 0 || role_->PhysicalPower < 20)
    {
        childs_text_["��W"]->setVisible(false);
    }
    if (role_->UsePoison <= 0 || role_->PhysicalPower < 30)
    {
        childs_text_["�ö�"]->setVisible(false);
    }
    if (role_->Detoxification <= 0 || role_->PhysicalPower < 30)
    {
        childs_text_["�ⶾ"]->setVisible(false);
    }
    if (role_->Medicine <= 0 || role_->PhysicalPower < 10)
    {
        childs_text_["�t��"]->setVisible(false);
    }
    if (role_->HiddenWeapon <= 15 || role_->PhysicalPower < 10)
    {
        childs_text_["����"]->setVisible(false);
    }

    if (role_->Competing)
    {
        childs_text_["ˎƷ"]->setVisible(false);
        childs_text_["����"]->setVisible(false);
    }

    childs_text_["�ȴ�"]->setVisible(false);  //���õȴ�

    setFontSize(20);
    arrange(0, 0, 0, 28);
    if (start_ == 0 || !getChild(active_child_)->getVisible())
    {
        active_child_ = findFristVisibleChild();
        forceActiveChild();
    }
    if (!role_->Moved) { role_->AI_Action = -1; }  //����Ϊδ�����ai���ж�
}

void BattleActionMenu::dealEvent(BP_Event& e)
{
    if (battle_scene_ == nullptr) { return; }
    //�����ai�������ж������ٽ�������ѭ��
    if (role_->isAuto())
    {
        int act = autoSelect(role_);
        setResult(act);
        setAllChildState(Normal);
        childs_[act]->setState(Press);
        setExit(true);
        setVisible(false);  //AI�����˵��ˣ�̫��
        return;
    }
    Menu::dealEvent(e);
}

//"0�Ƅ�", "1��W", "2�ö�", "3�ⶾ", "4�t��", "5����", "6ˎƷ", "7�ȴ�", "8��B", "9�Ԅ�", "10�Y��"
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

    //aiΪÿ���ж�����
    std::vector<AIAction> ai_action;

    Random<double> rand;    //÷ɭ��ת�������
    rand.set_seed();
    rand.set_parameter(0, 1);

    if (role->AI_Action == -1)
    {
        auto _role_temp = *role;
        auto role_temp = &_role_temp;    //��ʱ����ָ�룬����һЩ���о���˥���ļ���

        //��ʼ���㱾�ֵĲ���
        role->AI_Action = getResultFromString("�Y��");
        role->AI_MoveX = role->X();
        role->AI_MoveY = role->Y();
        role->AI_ActionX = role->X();
        role->AI_ActionY = role->Y();
        role->AI_Magic = nullptr;
        role->AI_Item = nullptr;

        //����һ��Ϊ�գ�û��ս����ȥ������
        if (!friends.empty() && !enemies.empty())
        {
            //��������ƶ���λ��
            battle_scene_->calSelectLayer(role, 0, battle_scene_->calMoveStep(role));

            //���ǳ�ҩ
            std::string action_text = "ˎƷ";
            if (childs_text_[action_text]->getVisible() &&
                (role->HP < 0.2 * role->MaxHP || role->MP < 0.2 * role->MaxMP || role_->PhysicalPower < 0.2 * Save::getInstance()->MaxPhysicalPower))
            {
                AIAction aa;
                aa.Action = getResultFromString(action_text);
                auto items = BattleItemMenu::getAvaliableItems(role, 2);
                for (auto item : items)
                {
                    //�������㣬����Ĳ��Ǿ����Ըոպõ�ҩ
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
                        aa.point *= 1.5;  //�Ա���ϵ���Դ�
                        getFarthestToAll(role, enemies, aa.MoveX, aa.MoveY);
                        ai_action.push_back(aa);
                    }
                }
            }

            //�ⶾ��ҽ�ƣ��ö�����Ϊ��������
            if (role->Morality > 50)
            {
                action_text = "�ⶾ";
                //��ⶾ�ģ��������������ж������ߣ��ӽ����ⶾ
                if (childs_text_[action_text]->getVisible())
                {
                    for (auto r2 : friends)
                    {
                        if (r2->Poison > 50)
                        {
                            AIAction aa;
                            calAIActionNearest(r2, aa);    //����Ŀ�꣬����Ŀ������ĵ㣬��ͬ
                            int action_dis = battle_scene_->calActionStep(role->Detoxification);    //��������ж��ľ���
                            if (action_dis >= calNeedActionDistance(aa))    //����Ҫ�ж��ľ���Ƚ�
                            {
                                //���ھ���֮������ʹ�ã����¸���������
                                aa.Action = getResultFromString(action_text);
                                aa.point = r2->Poison;
                                ai_action.push_back(aa);
                            }
                        }
                    }
                }
                action_text = "�t��";
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
                //�����ö�
                action_text = "�ö�";
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
                        if (r2->HP < 10) { aa.point = 1; }
                        ai_action.push_back(aa);
                    }
                }
            }

            //ʹ�ð���
            action_text = "����";
            if (childs_text_[action_text]->getVisible())
            {
                auto r2 = getNearestRole(role, enemies);
                AIAction aa;
                calAIActionNearest(r2, aa, role_temp);    //��ʱ����ָ�����������˺�����Ϊ���о���˥������ѧͬ
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
                        aa.point *= 0.5;    //������ֵ�Ե�
                        aa.item = item;
                        ai_action.push_back(aa);
                    }
                }
            }

            //ʹ����ѧ
            action_text = "��W";
            if (childs_text_[action_text]->getVisible())
            {
                AIAction aa;
                aa.Action = getResultFromString(action_text);
                auto r2 = getNearestRole(role, enemies);
                calAIActionNearest(r2, aa, role_temp);
                //������ѧ
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    int max_hurt = -1;
                    auto magic = Save::getInstance()->getRoleLearnedMagic(role, i);
                    if (magic == nullptr) { continue; }
                    int level_index = role->getRoleMagicLevelIndex(i);

                    battle_scene_->calSelectLayerByMagic(aa.MoveX, aa.MoveY, role->Team, magic, level_index);
                    //��������ѡ���ĵ���ԣ���������
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
                                    //printf("AI %s %s (%d, %d): %d\n", PotConv::to_read(role->Name).c_str(), PotConv::to_read(magic->Name).c_str(), ix, iy, total_hurt);
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
        //����������ֵ��ж�
        double max_point = -1;
        for (auto aa : ai_action)
        {
            printf("AI %s: %s ", PotConv::to_read(role->Name).c_str(), PotConv::to_read(getStringFromResult(aa.Action)).c_str());
            if (aa.item) { printf("%s ", PotConv::to_read(aa.item->Name).c_str()); }
            if (aa.magic) { printf("%s ", PotConv::to_read(aa.magic->Name).c_str()); }
            double r = rand.rand() * 10;    //����ͬ�ֵ�������������ѡ��
            printf("score %.2f(%.2f)\n", aa.point, r);
            //�����ֽ���һ���������ֵ��˵�����ڷ�Χ�ڣ����ƶ�������
            if (aa.point == 0)
            {
                aa.Action = getResultFromString("�Y��");
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
        //δ�ƶ��򷵻��ƶ�
        return getResultFromString("�Ƅ�");
    }
    else
    {
        return role->AI_Action;
    }
}

//�������
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
            //δ������ҿ����ߵĸ��Ӳ�����һ���ļ���
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
            if (count >= distance_layer_->squareSize()) { break; }  //�������������������
        }
        if (cal_stack_next.size() == 0) { break; }  //���µĵ㣬����
        cal_stack = cal_stack_next;
        step++;
    }
}

void BattleActionMenu::getFarthestToAll(Role* role, std::vector<Role*> roles, int& x, int& y)
{
    Random<double> rand;   //÷ɭ��ת�������
    rand.set_seed();
    //ѡ�������ез���������ĵ�
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
    Random<double> rand;   //÷ɭ��ת�������
    rand.set_seed();
    calDistanceLayer(x0, y0);
    double min_dis = BATTLEMAP_COORD_COUNT * BATTLEMAP_COORD_COUNT;
    for (int ix = 0; ix < BATTLEMAP_COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < BATTLEMAP_COORD_COUNT; iy++)
        {
            if (battle_scene_->canSelect(ix, iy))
            {
                double cur_dis = distance_layer_->data(ix, iy) + rand.rand();
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
    //ѡ���������ĵ���
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

//�����ȼ���ÿ����ƶ��ķ�Χ
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
    if (role_ == nullptr) { return; }
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
            magic_names.push_back(convert::formatString("%-12s%4d  ", m->Name, role_->getRoleShowLearnedMagicLevel(i)));
        }
        else
        {
            magic_names.push_back("");
        }
    }
    setStrings(magic_names);
    setPosition(160, 200);

    //������Ϊ0��������
    for (auto child : childs_)
    {
        int w, h;
        child->getSize(w, h);
        if (w <= 0) { child->setVisible(false); }
    }
    arrange(0, 0, 0, 30);
}

void BattleMagicMenu::onPressedOK()
{
    checkActiveToResult();
    magic_ = Save::getInstance()->getRoleLearnedMagic(role_, result_);
    if (magic_) { setExit(true); }
}

BattleItemMenu::BattleItemMenu()
{
    setSelectUser(false);
}

void BattleItemMenu::onEntrance()
{
    if (role_ == nullptr) { return; }
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
    //ѡ����Ʒ�б�
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
    auto item_menu = new BattleItemMenu();
    item_menu->setRole(role);
    item_menu->setForceItemType(type);
    auto items = item_menu->getAvaliableItems();
    delete item_menu;
    return items;
}
