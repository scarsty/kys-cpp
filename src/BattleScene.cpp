#include "BattleScene.h"
#include "MainScene.h"
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>
#include "Save.h"
#include "Menu.h"
#include "others/libconvert.h"
#include "File.h"
#include "GameUtil.h"
#include "Random.h"

BattleScene::BattleScene()
{
    full_window_ = 1;
    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);
    role_layer_.resize(COORD_COUNT);
    select_layer_.resize(COORD_COUNT);
    effect_layer_.resize(COORD_COUNT);
    battle_menu_ = new BattleMenu();
    battle_menu_->setPosition(160, 200);
    head_ = new Head();
    addChild(head_, 100, 100);
    battle_operator_ = new BattleOperator();
    battle_operator_->setBattleScene(this);
    //battle_operator_->setOperator(&select_x_, &select_y_, &select_layer_, &effect_layer_);
    save_ = Save::getInstance();
}

BattleScene::BattleScene(int id) : BattleScene()
{
    setID(id);
}

BattleScene::~BattleScene()
{
    delete battle_menu_;
    delete battle_operator_;
}

void BattleScene::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_.data(0));
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_.data(0));

    role_layer_.setAll(-1);
    select_layer_.setAll(-1);
    effect_layer_.setAll(-1);
}

void BattleScene::draw()
{
    auto r0 = battle_roles_[0];  //当前正在行动中的角色
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, screen_center_x_ * 2, screen_center_y_ * 2);
#ifndef _DEBUG0
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            if (!isOutLine(i1, i2))
            {
                int num = earth_layer_.data(i1, i2) / 2;
                BP_Color color = { 255, 255, 255, 255 };
                if (battle_operator_->isRunning())
                {
                    if (select_layer_.data(i1, i2) < 0)
                    {
                        color = { 64, 64, 64, 255 };
                    }
                    else
                    {
                        color = { 128, 128, 128, 255 };
                    }
                    if (battle_operator_->getMode() == BattleOperator::Action)
                    {
                        if (effect_layer_.data(i1, i2) >= 0)
                        {
                            if (select_layer_.data(i1, i2) < 0)
                            {
                                color = { 160, 160, 160, 255 };
                            }
                            else
                            {
                                color = { 192, 192, 192, 255 };
                            }
                        }
                    }
                    if (i1 == select_x_ && i2 == select_y_)
                    {
                        color = { 255, 255, 255, 255 };
                    }
                }

                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, color);
                }
            }
        }
    }
#endif
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            if (!isOutLine(i1, i2))
            {
                int num = building_layer_.data(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);

                }
                num = role_layer_.data(i1, i2);
                if (num >= 0)
                {
                    auto r = Save::getInstance()->getRole(num);
                    std::string path = convert::formatString("fight/fight%03d", r->HeadID);
                    BP_Color color = { 255, 255, 255, 255 };
                    if (effect_layer_.data(i1, i2) >= 0)
                    {
                        if (r0->ActTeam == 0 && battle_roles_[0]->Team == r->Team)
                        {
                            color = { 255, 255, 192, 255 };
                        }
                        else if (r0->ActTeam != 0 && battle_roles_[0]->Team != r->Team)
                        {
                            color = { 255, 192, 192, 255 };
                        }
                    }
                    TextureManager::getInstance()->renderTexture(path, calRolePic(r), p.x, p.y, color);
                }
            }
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();
}

void BattleScene::dealEvent(BP_Event& e)
{
    //选择第一个人
    auto r = battle_roles_[0];
    man_x_ = r->X();
    man_y_ = r->Y();
    select_x_ = r->X();
    select_y_ = r->Y();
    head_->setRole(r);
    head_->setState(Element::Pass);

    int result = battle_menu_->runAsRole(r);

    switch (result)
    {
    case 0: actMove(r); break;
    case 1: actUseMagic(r); break;
    case 2: actUsePoison(r); break;
    case 3: actDetoxification(r); break;
    case 4: actMedcine(r); break;
    case 5: actUseItem(r); break;
    case 6: actWait(r); break;
    case 7: actStatus(r); break;
    case 8: actAuto(r); break;
    case 9: actRest(r); break;
    default:
        //默认值为什么都不做
        break;
    }

    //如果此人行动过，则清除行动状态，放到队尾
    if (r->Acted)
    {
        r->Acted = 0;
        r->Moved = 0;
        battle_roles_.erase(battle_roles_.begin());
        battle_roles_.push_back(r);
    }
}

void BattleScene::onEntrance()
{
    calViewRegion();
    //设置全部角色的位置层，避免今后出错
    for (auto& r : Save::getInstance()->roles_)
    {
        r.setPoitionLayer(&role_layer_);
        r.Team = 2;
    }

    //首先设置位置和阵营，其他的后面统一处理
    //队友
    for (int i = 0; i < TEAMMATE_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(Save::getInstance()->Team[i]);
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
        }
    }
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r)
        {
            battle_roles_.push_back(r);
            r->setPosition(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
        }
    }

    //初始状态
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);
    }
    //排序
    sortRoles();
}

void BattleScene::setRoleInitState(Role* r)
{
    r->Acted = 0;
    r->ExpGot = 0;
    r->ShowNumber = 0;
    r->FightingFrame = 0;
    //读取动作帧数
    SAVE_INT frame[10];
    std::string file = convert::formatString("../game/resource/fight/fight%03d/fightframe.ka", r->HeadID);
    File::readFile(file, frame, 10);
    for (int i = 0; i < 5; i++)
    {
        r->FightFrame[i] = frame[i];
    }

    //寻找离自己最近的敌方，设置面向
    int min_distance = COORD_COUNT * COORD_COUNT;
    Role* r_near;
    for (auto r1 : battle_roles_)
    {
        if (r->Team != r1->Team)
        {
            int dis = abs(r->X() - r1->X()) + abs(r->Y() - r1->Y());
            if (dis < min_distance)
            {
                r_near = r1;
                min_distance = dis;
            }
        }
    }

    r->Face = (int)calFace(r->X(), r->Y(), r_near->X(), r_near->Y());
    r->Face = RandomClassical::rand(4);
}

//角色排序
void BattleScene::sortRoles()
{
    std::sort(battle_roles_.begin(), battle_roles_.end(), compareRole);
}

//角色排序的规则
bool BattleScene::compareRole(Role* r1, Role* r2)
{
    return r1->Speed > r2->Speed;
}

//计算可移动步数(考虑装备)
int BattleScene::calMoveStep(Role* r)
{
    int speed = r->Speed;
    if (r->Equip1 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip1);
        speed += i->AddSpeed;
    }
    if (r->Equip2 >= 0)
    {
        auto i = Save::getInstance()->getItem(r->Equip2);
        speed += i->AddSpeed;
    }
    return speed / 15 + 1;
}

//依据动作帧数计算角色的贴图编号
int BattleScene::calRolePic(Role* r)
{
    for (int i = 0; i < 5; i++)
    {
        if (r->FightFrame[i] > 0)
        {
            return r->FightFrame[i] * r->Face + r->FightingFrame;
        }
    }
    return r->Face;
}

//计算可以被选择的范围，会改写选择层
//mode含义：0-移动，受步数和障碍影响；1攻击用毒医疗等仅受步数影响；2查看状态，全都能选；3仅能选直线的格子
void BattleScene::calSelectLayer(Role* r, int mode, int step)
{
    if (mode == 0)
    {
        select_layer_.setAll(-1);
        std::vector<Point> cal_stack;
        cal_stack.push_back({ r->X(), r->Y() });
        int count = 0;
        while (step >= 0)
        {
            std::vector<Point> cal_stack_next;
            for (auto p : cal_stack)
            {
                select_layer_.data(p.x, p.y) = step;
                auto check_next = [&](Point p1)->void
                {
                    //未计算过且可以走的格子参与下一步的计算
                    if (canWalk(p1.x, p1.y) && select_layer_.data(p1.x, p1.y) == -1)
                    {
                        cal_stack_next.push_back(p1);
                        count++;
                    }
                };
                //检测是否在地方身旁，视情况打开此选项
                if (!isNearEnemy(r, p.x, p.y))
                {
                    //检测4个相邻点
                    check_next({ p.x - 1, p.y });
                    check_next({ p.x + 1, p.y });
                    check_next({ p.x, p.y - 1 });
                    check_next({ p.x, p.y + 1 });
                }
                if (count >= COORD_COUNT * COORD_COUNT) { break; }  //最多计算次数，避免死掉
            }
            cal_stack = cal_stack_next;
            step--;
        }
    }
    else if (mode == 1)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                select_layer_.data(ix, iy) = step - abs(ix - r->X()) - abs(iy - r->Y());
            }
        }
    }
    else if (mode == 2)
    {
        select_layer_.setAll(0);
    }
    else if (mode == 3)
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                int dx = abs(ix - r->X());
                int dy = abs(iy - r->Y());
                if (dx == 0 && dy <= step || dy == 0 && dx <= step)
                {
                    select_layer_.data(ix, iy) = 0;
                }
                else
                {
                    select_layer_.data(ix, iy) = -1;
                }
            }
        }
    }
}

void BattleScene::calEffectLayer(Role* r, Magic* m, int level_index)
{
    if (m->AttackAreaType == 3)
    {
        int x = select_x_, y = select_y_;
        effect_layer_.setAll(-1);
        int dis = m->AttackDistance[level_index];
        for (int ix = x - dis; ix <= x + dis; ix++)
        {
            for (int iy = y - dis; iy <= y + dis; iy++)
            {
                if (!isOutLine(ix, iy))
                {
                    effect_layer_.data(ix, iy) = 0;
                }
            }
        }
    }
}

bool BattleScene::canSelect(int x, int y)
{
    return (!isOutLine(x, y) && select_layer_.data(x, y) >= 0);
}

void BattleScene::walk(Role* r, int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        r->setPosition(x, y);
    }
}

bool BattleScene::canWalk(int x, int y)
{
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) || isRole(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleScene::isBuilding(int x, int y)
{
    return building_layer_.data(x, y) > 0;
}

bool BattleScene::isOutLine(int x, int y)
{
    return (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT);
}

bool BattleScene::isWater(int x, int y)
{
    int num = earth_layer_.data(x, y) / 2;
    if (num >= 179 && num <= 181
        || num == 261 || num == 511
        || num >= 662 && num <= 665
        || num == 674)
    {
        return true;
    }
    return false;
}

bool BattleScene::isRole(int x, int y)
{
    return role_layer_.data(x, y) > 0;
}

bool BattleScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 16 || abs(man_y_ - y) >= 20);
}

bool BattleScene::isNearEnemy(Role* r, int x, int y)
{
    for (auto r1 : battle_roles_)
    {
        if (r->Team != r1->Team && calDistance(r1->X(), r1->Y(), x, y) <= 1)
        {
            return true;
        }
    }
    return false;
}

void BattleScene::actMove(Role* r)
{
    int step = calMoveStep(r);
    calSelectLayer(r, 0, step);
    battle_operator_->setRoleAndMagic(r);
    battle_operator_->setMode(BattleOperator::Move);
    if (battle_operator_->run() == 0)
    {
        r->setPrevPosition(r->X(), r->Y());
        moveAnimation(r, select_x_, select_y_);
        r->Moved = 1;
    }
}

void BattleScene::actUseMagic(Role* r)
{
    auto magic_menu = new MenuText();
    std::vector<std::string> magic_names;
    for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
    {
        auto m = Save::getInstance()->getRoleLearnedMagic(r, i);
        if (m)
        {
            magic_names.push_back(convert::formatString("%s%7d", m->Name, r->MagicLevel[i]));
        }
    }
    magic_menu->setStrings(magic_names);
    magic_menu->setPosition(200, 200);

    while (true)
    {
        int select_magic = magic_menu->run();
        if (select_magic < 0) { break; }

        r->ActTeam = 1;
        auto magic = Save::getInstance()->getRoleLearnedMagic(r, select_magic);
        //level_index表示从0到9，而level从0到999
        int level_index = r->getMagicLevelIndex(select_magic);
        //计算可选择的范围
        if (magic->AttackAreaType == 1 || magic->AttackAreaType == 3)
        {
            calSelectLayer(r, 1, magic->SelectDistance[level_index]);
        }
        else
        {
            calSelectLayer(r, 3, magic->SelectDistance[level_index]);
        }
        //选择目标
        battle_operator_->setMode(BattleOperator::Action);
        battle_operator_->setRoleAndMagic(r, magic, level_index);
        calEffectLayer(r, magic, level_index);
        int select_aim = battle_operator_->run();
        //取消选择目标则重新进入选武功
        if (select_aim < 0)
        {
            continue;
        }
        else
        {
            //播放攻击画面，计算伤害
            r->Acted = 1;
            break;
        }
    }
    delete magic_menu;
}

void BattleScene::actUsePoison(Role* r)
{

}

void BattleScene::actDetoxification(Role* r)
{

}

void BattleScene::actMedcine(Role* r)
{

}

void BattleScene::actUseItem(Role* r)
{

}

void BattleScene::actWait(Role* r)
{
    battle_roles_.erase(battle_roles_.begin());
    battle_roles_.push_back(r);
}

void BattleScene::actStatus(Role* r)
{

}

void BattleScene::actAuto(Role* r)
{

}

void BattleScene::actRest(Role* r)
{
    r->PhysicalPower = GameUtil::limit(r->PhysicalPower + 5, 0, MAX_PHYSICAL_POWER);
    r->HP = GameUtil::limit(r->HP + 0.05 * r->MaxHP, 0, MAX_HP);
    r->MP = GameUtil::limit(r->MP + 0.05 * r->MaxMP, 0, MAX_MP);
    r->Acted = 1;
}

//r1使用武功magic攻击r2的伤害
int BattleScene::calHurt(Role* r1, Role* r2, Magic* magic, int magic_level)
{
    int v = r1->Attack - r2->Defence + magic->Attack[magic_level / 100] / 3;
    v += RandomClassical::rand(10) - RandomClassical::rand(10);
    return v;
}

//看不明白
void BattleScene::getMousePosition(Point* point)
{
    int x = point->x;
    int y = screen_center_y_ * 2 - point->y;
    //int yp = 0;
    //int yp = -(m_vcBattleSceneData[m_nbattleSceneNum].Data[1][x][y]);
    //mouse_x_ = (-x + screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBx;
    //mouse_y_ = (x - screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBy;
}

bool BattleScene::initBattleRoleState()
{
    //    BattleSave::getInstance()->battle_roles_.resize(BATTLE_ROLE_COUNT);
    //    for (int i = 0; i < BATTLE_ROLE_COUNT = 4096; i++)
    //    {
    //        m_vcBattleRole[i].setPosition(-1, -1);
    //        m_vcBattleRole[i].Show = 1;
    //    }
    //    m_nBStatus = 0;
    //    m_bisBattle = true;
    //    if (m_nMods == -1)
    //    {
    //        selectTeamMembers();
    //    }
    //    else
    //    {
    //    }
    //    m_nBRoleAmount = 0;
    //    int n0 = 0;
    //    int teamNum = 0;
    //    if (m_vcBattleInfo[m_nbattleNum].mate[0] == 0)
    //    {
    //        teamNum = 1;
    //    }
    //    for (int i = 0; teamNum < m_nMaxBRoleSelect && i < sizeof(m_vcBattleInfo[m_nbattleNum].mate) / sizeof(m_vcBattleInfo[m_nbattleNum].mate[0]); i++)
    //    {
    //        if (m_vcBattleInfo[m_nbattleNum].mate[m_nBRoleAmount] >= 0)
    //        {
    //            //m_vcBattleRole[m_nBRoleAmount].Y = m_vcBattleInfo[m_nbattleNum].mate_x[i];
    //            //m_vcBattleRole[m_nBRoleAmount].X = m_vcBattleInfo[m_nbattleNum].mate_y[i];
    //            m_vcBattleRole[m_nBRoleAmount].Team = 0;
    //            m_vcBattleRole[m_nBRoleAmount].Face = 2;
    //            m_vcBattleRole[m_nBRoleAmount].RoleID = m_vcBattleInfo[m_nbattleNum].mate[m_nBRoleAmount];
    //            m_vcBattleRole[m_nBRoleAmount].Auto = -1;
    //            setInitState(n0);
    //        }
    //        else if (m_nBattleList[teamNum] >= 0)
    //        {
    //            //m_vcBattleRole[m_nBRoleAmount].Y = m_vcBattleInfo[m_nbattleNum].mate_x[i];
    //            //m_vcBattleRole[m_nBRoleAmount].X = m_vcBattleInfo[m_nbattleNum].mate_y[i];
    //            m_vcBattleRole[m_nBRoleAmount].Team = 0;
    //            m_vcBattleRole[m_nBRoleAmount].Face = 2;
    //            m_vcBattleRole[m_nBRoleAmount].RoleID = m_nBattleList[teamNum];
    //            m_vcBattleRole[m_nBRoleAmount].Auto = -1;
    //            setInitState(n0);
    //            teamNum++;
    //        }
    //        m_nBRoleAmount++;
    //    }
    //    calMoveAbility();
    //    return true;
    //}
    //
    //void BattleMap::setInitState(int& n0)
    //{
    //    m_vcBattleRole[m_nBRoleAmount].Step = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Acted = 0;
    //    m_vcBattleRole[m_nBRoleAmount].ExpGot = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Show = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Progress = 0;
    //    m_vcBattleRole[m_nBRoleAmount].round = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Wait = 0;
    //    m_vcBattleRole[m_nBRoleAmount].frozen = 0;
    //    m_vcBattleRole[m_nBRoleAmount].killed = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Knowledge = 0;
    //    m_vcBattleRole[m_nBRoleAmount].Zhuanzhu = 0;
    //    m_vcBattleRole[m_nBRoleAmount].szhaoshi = 0;
    //    m_vcBattleRole[m_nBRoleAmount].pozhao = 0;
    //    m_vcBattleRole[m_nBRoleAmount].wanfang = 0;
    //    for (int j = 0; j <= 4; j++)
    //    {
    //        n0 = 0;
    //        if (m_vcBattleRole[m_nBRoleAmount].RoleID > -1)
    //        {
    //            for (int j1 = 0; j1 <= 9; j1++)
    //            {
    //                //if (m_Character[battleRole[BRoleAmount].rnum].GongTi >= 0)
    //                //{
    //                //    if ((m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j))
    //                //    {
    //                //        n0 = m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
    //                //    }
    //                //}
    //            }
    //        }
    //        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = 100;
    //        m_vcBattleRole[m_nBRoleAmount].lzhuangtai[j] = n0;
    //    }
    //    for (int j = 5; j <= 9; j++)
    //    {
    //        n0 = 0;
    //        if (m_vcBattleRole[m_nBRoleAmount].RoleID > -1)
    //        {
    //            for (int j1 = 0; j1 <= 9; j1++)
    //            {
    //                //if (m_Character[battleRole[BRoleAmount].rnum].GongTi >= 0)
    //                //{
    //                //    if ((m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j))
    //                //    {
    //                //        n0 = m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
    //                //    }
    //                //}
    //            }
    //        }
    //        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = n0;
    //        m_vcBattleRole[m_nBRoleAmount].lzhuangtai[j] = n0;
    //    }
    //    for (int j = 10; j <= 13; j++)
    //    {
    //        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = 0;
    //    }
    return true;
}



//移动动画
void BattleScene::moveAnimation(Role* r, int x, int y)
{
    //从目标往回找确定路线
    std::vector<Point> way;
    auto check_next = [&](Point p1, int step)->bool
    {
        if (canSelect(p1.x, p1.y) && select_layer_.data(p1.x, p1.y) == step)
        {
            way.push_back(p1);
            return true;
        }
        return false;
    };

    way.push_back({ x, y });
    for (int i = select_layer_.data(x, y); i < select_layer_.data(r->X(), r->Y()); i++)
    {
        int x1 = way.back().x, y1 = way.back().y;
        if (check_next({ x1 - 1, y1 }, i + 1)) { continue; }
        if (check_next({ x1 + 1, y1 }, i + 1)) { continue; }
        if (check_next({ x1, y1 - 1 }, i + 1)) { continue; }
        if (check_next({ x1, y1 + 1 }, i + 1)) { continue; }
    }

    for (int i = way.size() - 1; i >= 0; i--)
    {
        r->Face = calFace(r->X(), r->Y(), way[i].x, way[i].y);
        r->setPosition(way[i].x, way[i].y);
        drawAll();
        checkEventAndPresent(100);
    }
    r->setPosition(x, y);
    r->Moved = 1;
    select_layer_.setAll(-1);
}

//使用武学动画
void BattleScene::useMagicAnimation(Role* r, int x, int y)
{

}

