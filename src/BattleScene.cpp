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
                        color = { 160, 160, 160, 255 };
                    }
                    if (battle_operator_->getMode() == BattleOperator::Action)
                    {
                        if (effect_layer_.data(i1, i2) >= 0)
                        {
                            color = { 192, 192, 192, 255 };
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
                    TextureManager::getInstance()->renderTexture(path, calRolePic(r), p.x, p.y);
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
    //r->Face = rand() % 4;
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
//mode含义：0-移动，受步数和障碍影响；1攻击用毒医疗等仅受步数影响；2查看状态，全都能选
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
                //检测4个相邻点
                check_next({ p.x - 1, p.y });
                check_next({ p.x + 1, p.y });
                check_next({ p.x, p.y - 1 });
                check_next({ p.x, p.y + 1 });
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

bool BattleScene::isNearEnemy(int x, int y)
{
    return false;
}

void BattleScene::actMove(Role* r)
{
    calSelectLayer(r, 0, 15);
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
    return 100;
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

int BattleScene::selectTeamMembers()
{
    //menuOn();
    //int i, i2;
    //max0 = 1;
    //menuString[0] = "全員出戰";
    //for (i = 0; i < config::MaxTeamMember; i++)
    //{
    //    if (m_BasicData[0].TeamList[i] >= 0)
    //    {
    //        menuString[i + 1] = m_Character[m_BasicData[0].TeamList[i]].Name;
    //        max0++;
    //    }
    //}
    //menuString[max0] = "開始戰鬥";
    //ShowMultiMenu(max0, 0);
    //Draw();
    return 1;
}

void BattleScene::ShowMultiMenu(int max0, int menuNum)
{
    ////unschedule(schedule_selector(CommonScene::checkTimer));
    //string str;
    //for (int k = 0; k < 8; k++)
    //{
    //    SlectOfresult[k] = false;
    //    item[k] = nullptr;
    //}

    //for (int i = 0; i < 6; i++)
    //{
    //    ResultofBattle[i] = -1;
    //    BattleList[i] = -1;//  参战人员列表初始化 不参战为-1
    //}
    //for (int i = 0; i <= max0; i++)
    //{
    //    MenuItemFont::setFontName("fonts/chinese.ttf");
    //    MenuItemFont::setFontSize(24);
    //    str = CommonScene::a2u(menuString[i].c_str());
    //    item[i] = MenuItemFont::create(str, CC_CALLBACK_1(BattleScene::menuCloseCallback, this));
    //    if (menuString[i] == "開始戰鬥")
    //    {
    //        item[i]->setTag(8);
    //    }
    //    else
    //    {
    //        item[i]->setTag(i + 1);
    //    }


    //}

    //item[max0]->setEnabled(false);
    //CommonScene::cleanAllWords();
    //initMultiMenu();
    //showSlectMenu(s_list, 0);
    //Vec2 vec[7];
    //for (int i = 0; i < 7; i++)
    //{
    //    vec[i].x = xx;
    //    vec[i].y = yy - 170 - i * 20;
    //}
    //CommonScene::drawWords(s_list, 20, BattleScene::kindOfRole, vec);
}


void BattleScene::showSlectMenu(std::string* str, int x)
{
    //string name = "备战人物姓名：";
    //string attack = "攻击力：";
    //string defense = "防御力：";
    //string dodge = "轻功    ：";
    //string sword = "剑法    ：";
    //string quanfa = "拳法    ：";
    //string daofa = "刀法    ：";
    //int num = m_BasicData[0].TeamList[x];
    //name += m_Character[num].Name;
    //attack += to_string(m_Character[num].Attack);
    //defense += to_string(m_Character[num].Defence);
    //dodge += to_string(m_Character[num].Speed);
    //str[0] = name;
    //str[1] = attack;
    //str[2] = defense;
    //str[3] = dodge;
    //str[4] = sword;
    //int num1 = ((m_Character[num].Fencing) / 200.0) * 10;
    //CommonScene::showRoleAbility(xx + 90, yy - 170 - 4 * 20 + 10, num1);
    //str[5] = quanfa;
    //num1 = ((m_Character[num].Boxing) / 200.0) * 10;
    //CommonScene::showRoleAbility(xx + 90, yy - 170 - 5 * 20 + 10, num1);
    //str[6] = daofa;
    //num1 = ((m_Character[num].Knife) / 200.0) * 10;
    //CommonScene::showRoleAbility(xx + 90, yy - 170 - 6 * 20 + 10, num1);
    //CommonScene::showRoleVerticaldRawing(xx + 20, 63, num, 40, 0.8, 0.9);
    //string str1 = "ui/beizhan/Node.csb";
    //CommonScene::myRunAction(str1, 0, 25);
    //CommonScene::cleanFiveDimensional();
    //CommonScene::showFiveDimensional(xx + 80, yy, num);
}

void BattleScene::initMultiMenu()
{
    //auto s = Director::getInstance()->getWinSize();
    //auto menu = Menu::create(item[0], item[1], item[2], item[3], item[4], item[5], item[6], item[7], nullptr);
    //menu->alignItemsVertically();
    //menu->setPosition(Vec2(s.width / 2.0 + 150, s.height / 2.0 + 50));

    //auto m_backgrand = Sprite::create("ui/beizhan/1.png");
    //m_backgrand->setPosition(Vec2(s.width / 2.0, s.height / 2.0));

    //auto m_backgrand1 = Sprite::create("ui/beizhan/21.png");
    //m_backgrand1->setScaleX(0.4);
    //m_backgrand1->setScaleY(1.0);
    //m_backgrand1->setPosition(Vec2(s.width / 2.0 + 150, s.height / 2.0 + 50));

    //auto m_backgrand4 = Sprite::create("ui/beizhan/21.png");
    //m_backgrand4->setScaleX(1);
    //m_backgrand4->setScaleY(1.3);
    //m_backgrand4->setOpacity(50);
    //m_backgrand4->setPosition(Vec2(s.width / 2.0, s.height / 2.0));

    //auto m_backgrand2 = Sprite::create("ui/beizhan/4.png");
    //m_backgrand2->setPosition(Vec2(s.width / 2.0 - 170, s.height / 2.0));
    //m_backgrand2->setScale(1.3);
    //m_backgrand2->setOpacity(40);

    //auto m_backgrand3 = Sprite::create("ui/beizhan/5.png");
    //m_backgrand3->setPosition(Vec2(s.width / 2.0 + 50, s.height / 2.0));
    //m_backgrand3->setScale(1.3);
    //m_backgrand3->setOpacity(40);

    //auto item1 = MenuItemImage::create("ui/beizhan/1-1.png", "ui/beizhan/1-1.png", CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
    //item1->setTag(9);
    //auto item2 = MenuItemImage::create("ui/beizhan/1-2.png", "ui/beizhan/1-2.png", CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
    //item2->setTag(10);
    //item2->setPositionY(-75);
    //auto menu1 = Menu::create(item1, item2, NULL);
    //menu1->setPosition(Vec2(297, 392 - 170));

    //SpriteLayer->addChild(m_backgrand);
    //SpriteLayer->addChild(m_backgrand4);
    ////addChild(m_backgrand3);
    //SpriteLayer->addChild(menu1);
    //SpriteLayer->addChild(menu);

}


bool BattleScene::initBattleData()
{
    //battle::FactionBackup FBackup[10];
    //battle::FactionBackup FBackup2[10];
    //battle::FactionBackup TFBackup;

    //TFBackup.len = 0;
    //TFBackup.captain = -1;
    //for (int i = 0; i < 10; i++)
    //{
    //    FBackup[i].len = 0;
    //    FBackup[i].captain = -1;
    //    FBackup2[i].len = 0;
    //    FBackup2[i].captain = -1;
    //}

    //for (int i1 = 0; i1 <= 63; i1++)
    //{
    //    for (int i2 = 0; i2 <= 63; i2++)
    //    {
    //        m_vcBattleSceneData[m_nbattleSceneNum].Data[2][i1][i2] = -1;
    //        m_vcBattleSceneData[m_nbattleSceneNum].Data[4][i1][i2] = -1;
    //        m_vcBattleSceneData[m_nbattleSceneNum].Data[5][i1][i2] = -1;
    //    }
    //}
    //m_nBRoleAmount = 0;
    ////initBattleRoleState();
    return true;
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
//计算可移动步数(考虑装备)

void BattleScene::calMoveAbility()
{
    //int i, rnum, addspeed;
    //m_nMaxspeed = 0;
    //for (int i = 0; i < m_vcBattleRole.size(); i++)
    //{
    //    rnum = m_vcBattleRole[i].RoleID;
    //    if (rnum > -1)
    //    {
    //        addspeed = 0;
    //        //          if (CheckEquipSet(RRole[rnum].Equip[0], RRole[rnum].Equip[1], RRole[rnum].Equip[2], RRole[rnum].Equip[3]) == 5){
    //        //              addspeed += 30;
    //        //          }
    //        m_vcBattleRole[i].speed = (getRoleSpeed(m_vcBattleRole[i].RoleID, true) + addspeed);
    //        if (m_vcBattleRole[i].Wait == 0)
    //        {
    //            m_vcBattleRole[i].Step = round(power(m_vcBattleRole[i].speed / 15, 0.8) * (100 + m_vcBattleRole[i].zhuangtai[8]) / 100);
    //            if (m_nMaxspeed > m_vcBattleRole[i].speed)
    //            {
    //                m_nMaxspeed = m_nMaxspeed;
    //            }
    //            else { m_nMaxspeed = m_vcBattleRole[i].speed; }
    //        }
    //        //        if (Rrole[rnum].Moveable > 0)
    //        //        {
    //        //m_vcBattleRole[i].Step = 0;
    //        //        }
    //    }
    //}
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

