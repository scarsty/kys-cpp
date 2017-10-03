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
#ifndef _DEBUG
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            if (i1 >= 0 && i1 <= COORD_COUNT && i2 >= 0 && i2 <= COORD_COUNT)
            {
                int num = earth_layer_(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
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
            if (i1 >= 0 && i1 <= COORD_COUNT && i2 >= 0 && i2 <= COORD_COUNT)
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
    auto role = battle_roles_[0];
    man_x_ = role->X();
    man_y_ = role->Y();
    head_->setRole(role);
    head_->setState(Element::Pass);

    battle_menu_->runAsRole(role);

    battle_operator_->setRoleAndMagic(role);
    battle_operator_->run();


    //依据行动选项和目标搞之

    role->Acted = 1;

    //如果此人行动过，则放到队尾
    if (role->Acted)
    {
        role->Acted = 0;
        battle_roles_.erase(battle_roles_.begin());
        battle_roles_.push_back(role);
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

    r->Face = (int)CallFace(r->X(), r->Y(), r_near->X(), r_near->Y());
    //r->Face = rand() % 4;
}

void BattleScene::sortRoles()
{
    auto sort_role = [](Role * a, Role * b)->bool { return a->Speed > b->Speed; };
    std::sort(battle_roles_.begin(), battle_roles_.end(), sort_role);
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
                    if (select_layer_.data(p1.x, p1.y) == -1 && canWalk(p1.x, p1.y))
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
    return (!isOutLine(x, y) && select_layer_.data(x, y));
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

void BattleScene::showBattleMenu(int x, int y)
{
    //menuOn();
    //MenuItemImage* item1[12];
    //for (int i = 0; i < 12; i++)
    //{
    //    string str, str1;
    //    str = "menu/" + to_string(21 + i) + ".png";
    //    str1 = "menu/" + to_string(33 + i) + ".png";
    //    item1[i] = MenuItemImage::create(str, str1, CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
    //    item1[i]->setTag(11 + i);
    //    item1[i]->setPositionY(-30 * i);
    //}
    //auto menu = Menu::create(item1[0], item1[1], item1[2], item1[3], item1[4], item1[5], item1[6], item1[7], item1[8], item1[9], item1[10], item1[11], NULL);
    //menu->setPosition(Vec2(x, y));
    //SpriteLayer->addChild(menu);
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


bool BattleScene::battle(int battlenum, int getexp, int mods, int id, int maternum, int enemyrnum)
{
    return true;
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

//按轻功重排人物(未考虑装备)
void BattleScene::reArrangeBRole()
{
    //int i, n, n1, i1, i2, x, t, s1, s2;
    //BattleRoles temp;
    //i1 = 0;
    //i2 = 1;
    //for (i1 = 0; i1 < m_vcBattleRole.size() - 1; i1++)
    //{
    //    for (i2 = i1 + 1; i2 < m_vcBattleRole.size(); i2++)
    //    {
    //        s1 = 0;
    //        s2 = 0;
    //        if ((m_vcBattleRole[i1].RoleID > -1) && (m_vcBattleRole[i1].Dead == 0))
    //        {
    //            s1 = getRoleSpeed(m_vcBattleRole[i1].RoleID, true);
    //            //                  if checkEquipSet(Rrole[m_vcBattleRole[i1].rnum].Equip[0], Rrole[m_vcBattleRole[i1].rnum].Equip[1],
    //            //                      Rrole[m_vcBattleRole[i1].rnum].Equip[2], Rrole[m_vcBattleRole[i1].rnum].Equip[3]) = 5 then
    //            //                      s1 = s1 + 30;
    //        }
    //        if ((m_vcBattleRole[i2].RoleID > -1) && (m_vcBattleRole[i2].Dead == 0))
    //        {
    //            s2 = getRoleSpeed(m_vcBattleRole[i2].RoleID, true);
    //        }
    //        if ((m_vcBattleRole[i1].RoleID != 0) && (m_vcBattleRole[i1].Team != 0) && (s1 < s2) && (m_vcBattleRole[i2].RoleID != 0) && (m_vcBattleRole[i2].Team != 0))
    //        {
    //            temp = m_vcBattleRole[i1];
    //            m_vcBattleRole[i1] = m_vcBattleRole[i2];
    //            m_vcBattleRole[i2] = temp;
    //        }
    //    }
    //}

    //for (i1 = 0; i1 < 64; i1++)
    //{
    //    for (i2 = 0; i2 < 64; i2++)
    //    {
    //        battleSceneData[battleSceneNum].Data[2][i1][i2] = -1;
    //        battleSceneData[battleSceneNum].Data[5][i1][i2] = -1;
    //    }
    //}
    //n = 0;
    //for (i = 0; i < BRole.size(); i++)
    //{
    //    if ((battleRole[i].Dead == 0) && (battleRole[i].rnum >= 0))
    //    {
    //        n++;
    //    }
    //}
    //n1 = 0;
    //for (i = 0; i < BRole.size(); i++)
    //{
    //    if (battleRole[i].rnum >= 0)
    //    {
    //        if (battleRole[i].Dead == 0)
    //        {
    //            battleSceneData[battleSceneNum].Data[2][battleRole[i].X][battleRole[i].Y] = i;
    //            battleSceneData[battleSceneNum].Data[5][battleRole[i].X][battleRole[i].Y] = -1;
    //            //              if battlemode > 0 then
    //            //                  battleRole[i].Progress : = (n - n1) * 5;
    //            n1++;
    //        }
    //        else
    //        {
    //            battleSceneData[battleSceneNum].Data[2][battleRole[i].X][battleRole[i].Y] = -1;
    //            battleSceneData[battleSceneNum].Data[5][battleRole[i].X][battleRole[i].Y] = i;
    //        }
    //    }
    //}
    //i2 = 0;
    //if (battlemode > 0) then
    //    for i1 : = 0 to length(Brole) - 1 do
    //        if ((GetPetSkill(5, 1) and (battleRole[i1].rnum = 0)) or (GetPetSkill(5, 3) and (battleRole[i1].Team = 0))) then
    //            battleRole[i1].Progress : = 299 - i2 * 5;
    //i2 = i2 + 1;
}


//移动

void BattleScene::moveRole(int bnum)
{
    //int s, i;
    //calCanSelect(bnum, 0, m_vcBattleRole[bnum].Step);
    //if (selectAim(bnum, m_vcBattleRole[bnum].Step, 0))
    //{
    //    moveAmination(bnum);
    //}
}

//选择点
bool BattleScene::selectAim(int bnum, int step, int mods)
{
    //switch (keypressing)
    //{
    //case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
    //{
    //    Ay--;
    //    if ((abs(Ax - Bx) + abs(Ay - By) > step) || (battleSceneData[battleSceneNum].Data[3][Ax][Ay] < 0))
    //    {
    //        Ay++;
    //    }
    //    return false;
    //    break;
    //}
    //case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
    //{
    //    Ay++;
    //    if ((abs(Ax - Bx) + abs(Ay - By) > step) || (battleSceneData[battleSceneNum].Data[3][Ax][Ay] < 0))
    //    {
    //        Ay--;
    //    }
    //    return false;
    //    break;
    //}
    //case EventKeyboard::KeyCode::KEY_UP_ARROW:
    //{
    //    Ax--;
    //    if ((abs(Ax - Bx) + abs(Ay - By) > step) || (battleSceneData[battleSceneNum].Data[3][Ax][Ay] < 0))
    //    {
    //        Ax++;
    //    }
    //    return false;
    //    break;
    //}
    //case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
    //{
    //    Ax++;
    //    if ((abs(Ax - Bx) + abs(Ay - By) > step) || (battleSceneData[battleSceneNum].Data[3][Ax][Ay] < 0))
    //    {
    //        Ax--;
    //    }
    //    return false;
    //    break;
    //}
    //case EventKeyboard::KeyCode::KEY_ESCAPE:
    //{
    //    Ax = Bx;
    //    Ay = By;
    //    return false;
    //    break;
    //}
    //case EventKeyboard::KeyCode::KEY_SPACE:
    //case EventKeyboard::KeyCode::KEY_KP_ENTER:
    //{
    //    return true;
    //    break;
    //}
    //default:
    //{
    //    if (Msx >= 0 && Msy >= 0 && Msx != Ax && Msy != Ay)
    //    {
    //        if ((abs(Msx - Bx) + abs(Msy - By) > step) || (battleSceneData[battleSceneNum].Data[3][Msx][Msy] < 0))
    //        {
    //            Ax = Bx;
    //            Ay = By;
    //        }
    //        else
    //        {
    //            Ax = Msx;
    //            Ay = Msy;
    //        }
    //        return true;
    //    }
    //    return false;
    //}
    //}
    return true;
}

//移动动画
void BattleScene::moveAmination(int bnum)
{
    //int s, i, a, tempx, tempy;
    //int Xinc[5], Yinc[5];
    ////  Ax = Bx - 4;            //测试用
    ////  Ay = By - 4;
    //if (m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy] > 0)     //0空位，1建筑，2友军，3敌军，4出界，5已走过 ，6水面
    //{
    //    Xinc[1] = 1;
    //    Xinc[2] = -1;
    //    Xinc[3] = 0;
    //    Xinc[4] = 0;
    //    Yinc[1] = 0;
    //    Yinc[2] = 0;
    //    Yinc[3] = 1;
    //    Yinc[4] = -1;
    //    //      MyPoint *pInt = new MyPoint();
    //    //      pInt->x = Bx;
    //    //      pInt->y = By;
    //    //      wayQue.push(*pInt);
    //    //      MyPoint *pAInt = new MyPoint();
    //    //      pAInt->x = Ax;
    //    //      pAInt->y = Ay;
    //    m_nlinex[0] = m_nBx;
    //    m_nliney[0] = m_nBy;
    //    m_nlinex[m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy]] = m_nAx;
    //    m_nliney[m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy]] = m_nAy;
    //    a = m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy] - 1;
    //    while (a >= 0)
    //    {
    //        for (int i = 1; i < 5; i++)
    //        {
    //            tempx = m_nlinex[a + 1] + Xinc[i];
    //            tempy = m_nliney[a + 1] + Yinc[i];
    //            if (m_vcBattleSceneData[m_nbattleSceneNum].Data[3][tempx][tempy] == m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nlinex[a + 1]][m_nliney[a + 1]] - 1)
    //            {
    //                m_nlinex[a] = tempx;
    //                m_nliney[a] = tempy;
    //                break;
    //            }
    //        }
    //        a--;
    //    }

    //    m_ncurA = 1;
    //    //schedule(schedule_selector(BattleScene::moveAminationStep), battleSpeed, kRepeatForever, battleSpeed)
    //}
}

void BattleScene::moveAminationStep(float dt)
{

    //    int a = m_ncurA;
    //    int bnum = m_ncurRoleNum;
    //    if (!((m_vcBattleRole[bnum].Step == 0) || ((m_nBx == m_nAx) && (m_nBy == m_nAy))))
    //    {
    //        if ((m_nlinex[a] - m_nBx) > 0)
    //        {
    //            m_vcBattleRole[bnum].Face = 3;
    //        }
    //        else if ((m_nlinex[a] - m_nBx) < 0)
    //        {
    //            m_vcBattleRole[bnum].Face = 0;
    //        }
    //        else if ((m_nliney[a] - m_nBy) < 0)
    //        {
    //            m_vcBattleRole[bnum].Face = 2;
    //        }
    //        else { m_vcBattleRole[bnum].Face = 1; }
    //        if (m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] == bnum)
    //        {
    //            m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] = -1;
    //
    //        }
    //        m_nBx = m_nlinex[a];
    //        m_nBy = m_nliney[a];
    //        //m_vcBattleRole[bnum].X = m_nBx;
    //        //m_vcBattleRole[bnum].Y = m_nBy;
    //        if (m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] == -1)
    //        {
    //            m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] = bnum;
    //        }
    //        a++;
    //        m_ncurA = a;
    //        m_vcBattleRole[bnum].Step--;
    //        draw();
    //    }
    //    else
    //    {
    //        //      battleRole[bnum].X = Bx;
    //        //      battleRole[bnum].Y = By;
    //        //unschedule(schedule_selector(BattleScene::moveAminationStep));
    //        showBattleMenu(50, 500);
    //    }
}

//void BattleMap::battleMainControl()
//{
//
//    battleMainControl(-1, -1);
//
//}

//void BattleMap::battleMainControl(int mods, int id)
//{
//
//
//    calMoveAbility(); //计算移动能力
//    reArrangeBRole();
//    //m_nBx = m_vcBattleRole[m_ncurRoleNum].X;
//    //m_nBy = m_vcBattleRole[m_ncurRoleNum].Y;
//    draw();
//}

//void BattleMap::attack(int bnum)
//{
//    //int mnum, level;
//    //int i = 1;
//    //int rnum = m_vcBattleRole[bnum].RoleID;
//    ////mnum = m_Character[rnum].LMagic[i];
//    ////level = m_Character[rnum].MagLevel[i] / 100 + 1;
//    //m_ncurMagic = mnum;
//}

