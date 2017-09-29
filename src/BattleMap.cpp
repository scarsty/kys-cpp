#include "BattleMap.h"
#include "MainMap.h"
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>
#include "Save.h"
#include "Menu.h"

BattleMap::BattleMap()
{
    full_window_ = 1;
    init();
}


BattleMap::~BattleMap()
{
}

void BattleMap::draw()
{
    int k = 0;
    struct DrawInfo { int i; Point p; };
    std::map<int, DrawInfo> map;
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = m_nBx + i + (sum / 2);
            int i2 = m_nBy - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, m_nBx, m_nBy);
            if (i1 >= 0 && i1 <= MaxSceneCoord && i2 >= 0 && i2 <= MaxSceneCoord)
            {
                //EarthS[k]->setm_bvisible(false);
                //BuildS[k]->setm_bvisible(false);
                //这里注意状况
                Point p1 = Point(0, -m_vcBattleSceneData[m_nbattleSceneNum].Data[4][i1][i2]);
                Point p2 = Point(0, -m_vcBattleSceneData[m_nbattleSceneNum].Data[5][i1][i2]);
                int num = m_vcBattleSceneData[m_nbattleSceneNum].Data[0][i1][i2] / 2;
                if (num >= 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                    /*if (p1.y < -0.1)
                    {
                    map[calBlockTurn(i1, i2, 0)] = s;
                    }
                    else
                    {
                    s->visit();
                    }*/
                }
                //建筑和主角同一层
                num = m_vcBattleSceneData[m_nbattleSceneNum].Data[1][i1][i2] / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                    map[calBlockTurn(i1, i2, 1)] = { num, p };
                }
                else if (i1 == m_nBx && i2 == m_nBy)
                {
                    m_nmanPicture = 0 + Scene::towards_ * 4;
                    map[calBlockTurn(i1, i2, 1)] = { m_nmanPicture, p };
                    //s->visit();
                }
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    {
        TextureManager::getInstance()->renderTexture("smap", i->second.i, i->second.p.x, i->second.p.y);
    }
    SDL_Color color = { 0, 0, 0, 255 };
    //string strTemp;
    //strTemp = "中文测试";
    //strTemp = Engine::getInstance()->string_To_UTF8(strTemp);
    //Engine::getInstance()->drawText("fonts/Dialogues.ttf", strTemp, 20, 5, 5, 255, BP_ALIGN_LEFT, color); //这里有问题，字符无法显示
}

void BattleMap::dealEvent(BP_Event& e)
{

}

void BattleMap::checkTimer2()
{
    //if (!isMenuOn)
    //{
    //    moveRole(m_ncurRoleNum);
    //}
    ////Draw();
}

void BattleMap::walk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        m_nBx = x;
        m_nBy = y;
    }
    if (Scene::towards_ != t)
    {
        Scene::towards_ = t;
        m_nstep = 0;
    }
    else
    {
        m_nstep++;
    }
    m_nstep = m_nstep % m_nOffset_BRolePic;
}

bool BattleMap::canWalk(int x, int y)
{
    if (checkIsOutLine(x, y) || checkIsBuilding(x, y) || checkIsHinder(x, y)
        || checkIsEvent(x, y) || checkIsFall(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleMap::checkIsBuilding(int x, int y)
{
    if (m_vcBattleSceneData[m_nbattleSceneNum].Data[1][x][y] >= -2
        && m_vcBattleSceneData[m_nbattleSceneNum].Data[1][x][y] <= 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleMap::checkIsOutLine(int x, int y)
{
    if (x < 0 || x > MaxSceneCoord || y < 0 || y > MaxSceneCoord)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool BattleMap::checkIsHinder(int x, int y)
{
    return false;
}

bool BattleMap::checkIsEvent(int x, int y)
{
    return false;
}

bool BattleMap::checkIsFall(int x, int y)
{
    return false;
}

bool BattleMap::checkIsExit(int x, int y)
{
    return false;
}

void BattleMap::callEvent(int x, int y)
{
}

bool BattleMap::checkIsOutScreen(int x, int y)
{
    if (abs(m_nBx - x) >= 16 || abs(m_nBy - y) >= 20)
    {
        return true;
    }
    else
    {
        return false;
    }
}

//看不明白
void BattleMap::getMousePosition(Point* point)
{
    int x = point->x;
    int y = screen_center_y_ * 2 - point->y;
    //int yp = 0;
    int yp = -(m_vcBattleSceneData[m_nbattleSceneNum].Data[1][x][y]);
    mouse_x_ = (-x + screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBx;
    mouse_y_ = (x - screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBy;
}

void BattleMap::FindWay(int Mx, int My, int Fx, int Fy)
{
}

int BattleMap::CallFace(int x1, int y1, int x2, int y2)
{
    int d1, d2, dm;
    d1 = x2 - x1;
    d2 = y2 - y1;
    dm = abs(d1) - abs(d2);
    if ((d1 != 0) || (d2 != 0))
    {
        if (dm >= 0)
        {
            if (d1 < 0)
            {
                return 0;
            }
            else
            {
                return 3;
            }
        }
        else
        {
            if (d2 < 0)
            {
                return 2;
            }
            else
            {
                return 1;
            }
        }
    }
}

void BattleMap::initData(int scenenum)
{
    //       for (int i = 0; i < maxBRoleSelect; i++)
    //       {
    //           int numBRole = ResultofBattle[i];
    //           char *fightPath = new char[30];
    //           sprintf(fightPath, "fight/fight%03d", numBRole);
    //           char *fightPathIn = new char[30];
    //           sprintf(fightPathIn, "fight/fight%03d/index.ka", numBRole);
    //           auto file = FileUtils::getInstance();
    //           //std::fstream file;
    //           if (file->isFileExist(fightPathIn)){
    //               loadTexture(fightPath, MyTexture2D::Battle, 250, numBRole);
    //           }
    //           //delete(fightPath);
    //           delete(fightPathIn);
    //       }
}



void BattleMap::autoSetMagic(int rnum)
{


}

bool BattleMap::autoInBattle()
{
    int x, y;
    int autoCount = 0;
    for (int i = 0; i < MaxBRoleNum; i++)
    {
        m_vcBattleRole[i].Team = 1;
        m_vcBattleRole[i].RoleID = -1;
        //我方自动参战数据
        if (m_nMods >= -1)
        {
            for (int i = 0; i < sizeof(m_vcBattleInfo[m_nbattleNum].mate) / sizeof(m_vcBattleInfo[m_nbattleNum].mate[0]); i++)
            {
                x = m_vcBattleInfo[m_nbattleNum].mate_x[i];
                y = m_vcBattleInfo[m_nbattleNum].mate_y[i];
            }
            if (m_nMods == -1)
            {
                m_vcBattleRole[m_nBRoleAmount].RoleID = m_vcBattleInfo[m_nbattleNum].autoMate[i];
            }
            else
            {
                m_vcBattleRole[m_nBRoleAmount].RoleID = -1;
            }
            m_vcBattleRole[m_nBRoleAmount].Team = 0;
            m_vcBattleRole[m_nBRoleAmount].setPosition(x, y);
            m_vcBattleRole[m_nBRoleAmount].Face = 2;
            if (m_vcBattleRole[m_nBRoleAmount].RoleID == -1)
            {
                m_vcBattleRole[m_nBRoleAmount].Dead = 1;
                m_vcBattleRole[m_nBRoleAmount].Show = 1;
            }
            else
            {
                m_vcBattleRole[m_nBRoleAmount].Dead = 0;
                m_vcBattleRole[m_nBRoleAmount].Show = 0;
                //if (!((m_Character[battleRole[BRoleAmount].rnum].TeamState == 1)
                //    || (m_Character[battleRole[BRoleAmount].rnum].TeamState == 2))
                //    && !(m_Character[battleRole[BRoleAmount].rnum].Faction == m_Character[0].Faction))
                //{
                //    autoSetMagic(battleRole[BRoleAmount].rnum);
                //    autoCount++;
                //}
            }
            m_vcBattleRole[m_nBRoleAmount].Step = 0;
            m_vcBattleRole[m_nBRoleAmount].Acted = 0;
            m_vcBattleRole[m_nBRoleAmount].ExpGot = 0;
            if (m_vcBattleRole[m_nBRoleAmount].RoleID == 0)
            {
                m_vcBattleRole[m_nBRoleAmount].Auto = -1;
            }
            else
            {
                m_vcBattleRole[m_nBRoleAmount].Auto = 3;
            }
            m_vcBattleRole[m_nBRoleAmount].Progress = 0;
            m_vcBattleRole[m_nBRoleAmount].round = 0;
            m_vcBattleRole[m_nBRoleAmount].Wait = 0;
            m_vcBattleRole[m_nBRoleAmount].frozen = 0;
            m_vcBattleRole[m_nBRoleAmount].killed = 0;
            m_vcBattleRole[m_nBRoleAmount].Knowledge = 0;
            m_vcBattleRole[m_nBRoleAmount].Zhuanzhu = 0;
            m_vcBattleRole[m_nBRoleAmount].szhaoshi = 0;
            m_vcBattleRole[m_nBRoleAmount].pozhao = 0;
            m_vcBattleRole[m_nBRoleAmount].wanfang = 0;

        }
        //自动参战结束
    }
    return true;
}

int BattleMap::selectTeamMembers()
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

void BattleMap::ShowMultiMenu(int max0, int menuNum)
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

/*
void BattleScene::menuCloseCallback(Ref* pSender)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WP8) || (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
    MessageBox("You pressed the close button. Windows Store Apps do not implement a close button.", "Alert");
    return;
#endif
    auto p = dynamic_cast<MenuItemFont*> (pSender);
    if (p == nullptr)
        return;
    switch (p->getTag())
    {
    case 1:
    {
        SlectOfresult[0] = !SlectOfresult[0];
        if (SlectOfresult[0] == true)
        {
            counter = max0 - 1;
            for (int i = 1; i < 7; i++)
            {
                if (m_BasicData[0].TeamList[i - 1] >= 0)
                {
                    item[i]->setColor(Color3B::RED);
                }
            }
            for (int i = 0; i < 6; i++)
            {
                if (m_BasicData[0].TeamList[i] >= 0)
                {
                    SlectOfresult[i + 1] = true;
                    ResultofBattle[i] = 1;
                }
            }
        }
        else if (SlectOfresult[0] == false)
        {
            counter = 0;
            for (int i = 1; i < 7; i++)
            {
                item[i]->setColor(Color3B::WHITE);
            }
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }
    case 2:
    {

        SlectOfresult[1] = !SlectOfresult[1];
        if (SlectOfresult[1] == true)
        {
            item[1]->setColor(Color3B::RED);
            ResultofBattle[0] = 1;
            counter++;
        }
        else if (SlectOfresult[1] == false)
        {
            item[1]->setColor(Color3B::WHITE);
            ResultofBattle[0] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }




        break;
    }
    case 3:
    {
        SlectOfresult[2] = !SlectOfresult[2];
        if (SlectOfresult[2] == true)
        {
            item[2]->setColor(Color3B::RED);
            ResultofBattle[1] = 1;
            counter++;
        }
        else if (SlectOfresult[2] == false)
        {
            item[2]->setColor(Color3B::WHITE);
            ResultofBattle[1] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }

    case 4:
    {
        SlectOfresult[3] = !SlectOfresult[3];
        if (SlectOfresult[3] == true)
        {

            item[3]->setColor(Color3B::RED);
            ResultofBattle[2] = 1;
            counter++;
        }
        else if (SlectOfresult[3] == false)
        {
            item[3]->setColor(Color3B::WHITE);
            ResultofBattle[2] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }
    case 5:
    {
        SlectOfresult[4] = !SlectOfresult[4];
        if (SlectOfresult[4] == true)
        {

            item[4]->setColor(Color3B::RED);
            ResultofBattle[3] = 1;
            counter++;
        }
        else if (SlectOfresult[4] == false)
        {
            item[4]->setColor(Color3B::WHITE);
            ResultofBattle[3] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }
    case 6:
    {

        SlectOfresult[5] = !SlectOfresult[5];
        if (SlectOfresult[5] == true)
        {
            item[5]->setColor(Color3B::RED);
            ResultofBattle[4] = 1;
            counter++;
        }
        else if (SlectOfresult[5] == false)
        {
            item[5]->setColor(Color3B::WHITE);
            ResultofBattle[4] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }
    case 7:
    {
        SlectOfresult[6] = !SlectOfresult[6];
        if (SlectOfresult[6] == true)
        {
            item[6]->setColor(Color3B::RED);
            ResultofBattle[5] = 1;
            counter++;
        }
        else if (SlectOfresult[1] == false)
        {
            item[6]->setColor(Color3B::WHITE);
            ResultofBattle[5] = 0;
            counter--;
        }
        if (counter > 0)
        {
            item[max0]->setEnabled(true);
        }
        else
        {
            item[max0]->setEnabled(false);
        }
        break;
    }
    case 8:
    {
        //  开始战斗入口
        for (int i = 0; i < config::MaxTeamMember; i++)
        {
            if (ResultofBattle[i])
            {
                BattleList[i] = m_BasicData[0].TeamList[i];
            }

        }
        initBattleRoleState();
        battleMainControl();
        CommonScene::cleanAllWords();
        CommonScene::cleanRoleVerticaldRawing();
        CommonScene::cleanFiveDimensional();
        SpriteLayer->removeAllChildren();
        Draw();
        menuClose();
        showBattleMenu(50, 500);
        //schedule(schedule_selector(CommonScene::checkTimer), 0.025, kRepeatForever, 0.025);

        break;
    }
    }
}

void BattleScene::menuCloseCallback1(Ref* pSender)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WP8) || (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
    MessageBox("You pressed the close button. Windows Store Apps do not implement a close button.", "Alert");
    return;
#endif
    auto p = dynamic_cast<MenuItemImage*> (pSender);
    Vec2 vec[7];
    for (int i = 0; i < 7; i++)
    {
        vec[i].x = xx;
        vec[i].y = yy - 170 - i * 20;
    }
    if (p == nullptr)
        return;
    switch (p->getTag())
    {
    case 9:
    {
        num_list--;
        if ((num_list >= 0) && (m_BasicData[0].TeamList[num_list] >= 0))
        {
            CommonScene::cleanRoleVerticaldRawing();
            CommonScene::cleanAllWords();
            showSlectMenu(s_list, num_list);
            CommonScene::drawWords(s_list, 20, BattleScene::kindOfRole, vec);

        }
        else
        {
            num_list = 0;
        }
        break;
    }
    case 10:
    {
        num_list++;
        if ((num_list <= 5) && (m_BasicData[0].TeamList[num_list] >= 0))
        {
            CommonScene::cleanRoleVerticaldRawing();
            CommonScene::cleanAllWords();
            showSlectMenu(s_list, num_list);
            CommonScene::drawWords(s_list, 20, BattleScene::kindOfRole, vec);

        }
        else
        {
            num_list = 5;
        }
        break;
    }
    case 11:
    {
        //  移动;
        if (battleRole[curRoleNum].Step > 0) {
            SpriteLayer->removeAllChildren();
            menuClose();
            Msx = Bx;
            Msy = By;
            Ax = Bx;
            Ay = By;
            moveRole(curRoleNum);

        }
        break;
    }

    case 12:
    {
        //  武学;
        break;
    }
    case 13:
    {
        //  用毒;
        break;
    }
    case 14:
    {
        //  解毒;
        break;
    }
    case 15:
    {
        //  医疗;
        break;
    }
    case 16:
    {
        //  解穴;
        break;
    }
    case 17:
    {
        //  专注;
        break;
    }
    case 18:
    {
        //  物品;
        break;
    }
    case 19:
    {
        //  等待;
        break;
    }
    case 20:
    {
        //  状态;
        break;
    }
    case 21:
    {
        //  休息;
        break;
    }
    case 22:
    {
        //  自动;
        break;
    }
    }

}
*/
void BattleMap::showBattleMenu(int x, int y)
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


void BattleMap::showSlectMenu(std::string* str, int x)
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

void BattleMap::initMultiMenu()
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


bool BattleMap::battle(int battlenum, int getexp, int mods, int id, int maternum, int enemyrnum)
{
    return true;
}

bool BattleMap::initBattleData()
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

    for (int i1 = 0; i1 <= 63; i1++)
    {
        for (int i2 = 0; i2 <= 63; i2++)
        {
            m_vcBattleSceneData[m_nbattleSceneNum].Data[2][i1][i2] = -1;
            m_vcBattleSceneData[m_nbattleSceneNum].Data[4][i1][i2] = -1;
            m_vcBattleSceneData[m_nbattleSceneNum].Data[5][i1][i2] = -1;
        }
    }
    m_nBRoleAmount = 0;
    //initBattleRoleState();
    return true;
}

bool BattleMap::initBattleRoleState()
{
    BattleData::getInstance()->m_vcBattleRole.resize(MaxBRoleNum);
    for (int i = 0; i < MaxBRoleNum; i++)
    {
        m_vcBattleRole[i].setPosition(-1, -1);
        m_vcBattleRole[i].Show = 1;
    }
    m_nBStatus = 0;
    m_bisBattle = true;
    if (m_nMods == -1)
    {
        selectTeamMembers();
    }
    else
    {
    }
    m_nBRoleAmount = 0;
    int n0 = 0;
    int teamNum = 0;
    if (m_vcBattleInfo[m_nbattleNum].mate[0] == 0)
    {
        teamNum = 1;
    }
    for (int i = 0; teamNum < m_nMaxBRoleSelect && i < sizeof(m_vcBattleInfo[m_nbattleNum].mate) / sizeof(m_vcBattleInfo[m_nbattleNum].mate[0]); i++)
    {
        if (m_vcBattleInfo[m_nbattleNum].mate[m_nBRoleAmount] >= 0)
        {
            //m_vcBattleRole[m_nBRoleAmount].Y = m_vcBattleInfo[m_nbattleNum].mate_x[i];
            //m_vcBattleRole[m_nBRoleAmount].X = m_vcBattleInfo[m_nbattleNum].mate_y[i];
            m_vcBattleRole[m_nBRoleAmount].Team = 0;
            m_vcBattleRole[m_nBRoleAmount].Face = 2;
            m_vcBattleRole[m_nBRoleAmount].RoleID = m_vcBattleInfo[m_nbattleNum].mate[m_nBRoleAmount];
            m_vcBattleRole[m_nBRoleAmount].Auto = -1;
            setInitState(n0);
        }
        else if (m_nBattleList[teamNum] >= 0)
        {
            //m_vcBattleRole[m_nBRoleAmount].Y = m_vcBattleInfo[m_nbattleNum].mate_x[i];
            //m_vcBattleRole[m_nBRoleAmount].X = m_vcBattleInfo[m_nbattleNum].mate_y[i];
            m_vcBattleRole[m_nBRoleAmount].Team = 0;
            m_vcBattleRole[m_nBRoleAmount].Face = 2;
            m_vcBattleRole[m_nBRoleAmount].RoleID = m_nBattleList[teamNum];
            m_vcBattleRole[m_nBRoleAmount].Auto = -1;
            setInitState(n0);
            teamNum++;
        }
        m_nBRoleAmount++;
    }
    calMoveAbility();
    return true;
}

void BattleMap::setInitState(int& n0)
{
    m_vcBattleRole[m_nBRoleAmount].Step = 0;
    m_vcBattleRole[m_nBRoleAmount].Acted = 0;
    m_vcBattleRole[m_nBRoleAmount].ExpGot = 0;
    m_vcBattleRole[m_nBRoleAmount].Show = 0;
    m_vcBattleRole[m_nBRoleAmount].Progress = 0;
    m_vcBattleRole[m_nBRoleAmount].round = 0;
    m_vcBattleRole[m_nBRoleAmount].Wait = 0;
    m_vcBattleRole[m_nBRoleAmount].frozen = 0;
    m_vcBattleRole[m_nBRoleAmount].killed = 0;
    m_vcBattleRole[m_nBRoleAmount].Knowledge = 0;
    m_vcBattleRole[m_nBRoleAmount].Zhuanzhu = 0;
    m_vcBattleRole[m_nBRoleAmount].szhaoshi = 0;
    m_vcBattleRole[m_nBRoleAmount].pozhao = 0;
    m_vcBattleRole[m_nBRoleAmount].wanfang = 0;
    for (int j = 0; j <= 4; j++)
    {
        n0 = 0;
        if (m_vcBattleRole[m_nBRoleAmount].RoleID > -1)
        {
            for (int j1 = 0; j1 <= 9; j1++)
            {
                //if (m_Character[battleRole[BRoleAmount].rnum].GongTi >= 0)
                //{
                //    if ((m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j))
                //    {
                //        n0 = m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
                //    }
                //}
            }
        }
        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = 100;
        m_vcBattleRole[m_nBRoleAmount].lzhuangtai[j] = n0;
    }
    for (int j = 5; j <= 9; j++)
    {
        n0 = 0;
        if (m_vcBattleRole[m_nBRoleAmount].RoleID > -1)
        {
            for (int j1 = 0; j1 <= 9; j1++)
            {
                //if (m_Character[battleRole[BRoleAmount].rnum].GongTi >= 0)
                //{
                //    if ((m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j))
                //    {
                //        n0 = m_Magic[m_Character[battleRole[BRoleAmount].rnum].LMagic[m_Character[battleRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
                //    }
                //}
            }
        }
        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = n0;
        m_vcBattleRole[m_nBRoleAmount].lzhuangtai[j] = n0;
    }
    for (int j = 10; j <= 13; j++)
    {
        m_vcBattleRole[m_nBRoleAmount].zhuangtai[j] = 0;
    }

}
//计算可移动步数(考虑装备)

void BattleMap::calMoveAbility()
{
    int i, rnum, addspeed;
    m_nMaxspeed = 0;
    for (int i = 0; i < m_vcBattleRole.size(); i++)
    {
        rnum = m_vcBattleRole[i].RoleID;
        if (rnum > -1)
        {
            addspeed = 0;
            //          if (CheckEquipSet(RRole[rnum].Equip[0], RRole[rnum].Equip[1], RRole[rnum].Equip[2], RRole[rnum].Equip[3]) == 5){
            //              addspeed += 30;
            //          }
            m_vcBattleRole[i].speed = (getRoleSpeed(m_vcBattleRole[i].RoleID, true) + addspeed);
            if (m_vcBattleRole[i].Wait == 0)
            {
                m_vcBattleRole[i].Step = round(power(m_vcBattleRole[i].speed / 15, 0.8) * (100 + m_vcBattleRole[i].zhuangtai[8]) / 100);
                if (m_nMaxspeed > m_vcBattleRole[i].speed)
                {
                    m_nMaxspeed = m_nMaxspeed;
                }
                else { m_nMaxspeed = m_vcBattleRole[i].speed; }
            }
            //        if (Rrole[rnum].Moveable > 0)
            //        {
            //m_vcBattleRole[i].Step = 0;
            //        }
        }
    }
}

//按轻功重排人物(未考虑装备)
void BattleMap::reArrangeBRole()
{
    int i, n, n1, i1, i2, x, t, s1, s2;
    BattleRole temp;
    i1 = 0;
    i2 = 1;
    for (i1 = 0; i1 < m_vcBattleRole.size() - 1; i1++)
    {
        for (i2 = i1 + 1; i2 < m_vcBattleRole.size(); i2++)
        {
            s1 = 0;
            s2 = 0;
            if ((m_vcBattleRole[i1].RoleID > -1) && (m_vcBattleRole[i1].Dead == 0))
            {
                s1 = getRoleSpeed(m_vcBattleRole[i1].RoleID, true);
                //                  if checkEquipSet(Rrole[m_vcBattleRole[i1].rnum].Equip[0], Rrole[m_vcBattleRole[i1].rnum].Equip[1],
                //                      Rrole[m_vcBattleRole[i1].rnum].Equip[2], Rrole[m_vcBattleRole[i1].rnum].Equip[3]) = 5 then
                //                      s1 = s1 + 30;
            }
            if ((m_vcBattleRole[i2].RoleID > -1) && (m_vcBattleRole[i2].Dead == 0))
            {
                s2 = getRoleSpeed(m_vcBattleRole[i2].RoleID, true);
            }
            if ((m_vcBattleRole[i1].RoleID != 0) && (m_vcBattleRole[i1].Team != 0) && (s1 < s2) && (m_vcBattleRole[i2].RoleID != 0) && (m_vcBattleRole[i2].Team != 0))
            {
                temp = m_vcBattleRole[i1];
                m_vcBattleRole[i1] = m_vcBattleRole[i2];
                m_vcBattleRole[i2] = temp;
            }
        }
    }

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

//获取人物速度
int BattleMap::getRoleSpeed(int rnum, bool Equip)
{
    int l;
    int bResult;
    //bResult = Save::getInstance()->m_Character[rnum].Speed;
    //if (Save::getInstance()->m_Character[rnum].GongTi > -1)
    //{
    //    l = getGongtiLevel(rnum);
    //    bResult = Save::getInstance()->m_Magic[Save::getInstance()->m_Character[rnum].LMagic[Save::getInstance()->m_Character[rnum].GongTi]].AddSpd[l];
    //}
    //if (Equip)
    //{
    //    for (int l = 0; l < config::MaxEquipNum; l++)
    //    {
    //        if (Save::getInstance()->m_Character[rnum].Equip[l] >= 0)
    //        {
    //            bResult += Save::getInstance()->m_Item[Save::getInstance()->m_Character[rnum].Equip[l]].AddSpeed;
    //        }
    //        bResult = bResult * 100 / (100 + Save::getInstance()->m_Character[rnum].Wounded + Save::getInstance()->m_Character[rnum].Poison);
    //    }
    //}
    return bResult;
    return 0;
}

//获取功体经验
int BattleMap::getGongtiLevel(int rnum)
{
    //int i;
    //int n = Save::getInstance()->m_Character[rnum].GongTi;
    //if ((rnum >= 0) && (n >= -1))
    //{
    //    if (Save::getInstance()->m_Magic[Save::getInstance()->m_Character[rnum].LMagic[n]].MaxLevel > Save::getInstance()->m_Character[rnum].MagLevel[n] / 100)
    //    {
    //        return Save::getInstance()->m_Character[rnum].MagLevel[n] / 100;
    //    }
    //    else { return Save::getInstance()->m_Magic[Save::getInstance()->m_Character[rnum].LMagic[n]].MaxLevel; }
    //}
    //else { return 0; }
    return 0;
}


float BattleMap::power(float base, float Exponent)
{
    if (Exponent == 0.0)
    {
        return 1.0;
    }
    else if ((base == 0.0) && (Exponent > 0.0))
    {
        return 0.0;
    }
    else if (base < 0)
    {
        /*Error(reInvalidOp);*/
        return 0.0; // avoid warning W1035 Return value of function '%s' might be undefined
    }
    else { return exp(Exponent * log(base)); }
}

//初始化范围
//mode=0移动，1攻击用毒医疗等，2查看状态
void BattleMap::calCanSelect(int bnum, int mode, int Step)
{
    //int i;
    //if (mode == 0)
    //{
    //    for (int i1 = 0; i1 <= 63; i1++)
    //    {
    //        for (int i2 = 0; i2 <= 63; i2++)
    //        {
    //            m_vcBattleSceneData[m_nbattleSceneNum].Data[3][i1][i2] = -1;
    //        }
    //    }
    //    m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_vcBattleRole[bnum].X][m_vcBattleRole[bnum].Y] = 0;
    //    seekPath2(m_vcBattleRole[bnum].X, m_vcBattleRole[bnum].Y, Step, m_vcBattleRole[bnum].Team, mode);
    //}
    //if (mode == 1)
    //{
    //    for (int i1 = 0; i1 <= 63; i1++)
    //    {
    //        for (int i2 = 0; i2 <= 63; i2++)
    //        {
    //            m_vcBattleSceneData[m_nbattleSceneNum].Data[3][i1][i2] = -1;
    //            if (abs(i1 - m_vcBattleRole[bnum].X()) + abs(i2 - m_vcBattleRole[bnum].Y()) <= m_nstep)
    //            {
    //                m_vcBattleSceneData[m_nbattleSceneNum].Data[3][i1][i2] = 0;
    //            }
    //        }
    //    }
    //}

    //if (mode == 2)
    //{
    //    for (int i1 = 0; i1 <= 63; i1++)
    //    {
    //        for (int i2 = 0; i2 <= 63; i2++)
    //        {
    //            m_vcBattleSceneData[m_nbattleSceneNum].Data[3][i1][i2] = -1;
    //            if (m_vcBattleSceneData[m_nbattleSceneNum].Data[2][i1][i2] >= 0)
    //            {
    //                m_vcBattleSceneData[m_nbattleSceneNum].Data[3][i1][i2] = 0;
    //            }
    //        }
    //    }
    //}
}

//计算可以被选中的位置
//利用队列
//移动过程中，旁边有敌人，则不能继续移动
void BattleMap::seekPath2(int x, int y, int step, int myteam, int mode)
{
    int Xlist[4096];
    int Ylist[4096];
    int steplist[4096];
    int curgrid, totalgrid;
    int Bgrid[5]; //0空位，1建筑，2友军，3敌军，4出界，5已走过 ，6水面
    int Xinc[5], Yinc[5];
    int curX, curY, curstep, nextX, nextY, i;

    Xinc[1] = 1;
    Xinc[2] = -1;
    Xinc[3] = 0;
    Xinc[4] = 0;
    Yinc[1] = 0;
    Yinc[2] = 0;
    Yinc[3] = 1;
    Yinc[4] = -1;
    curgrid = 0;
    totalgrid = 0;
    Xlist[totalgrid] = x;
    Ylist[totalgrid] = y;
    steplist[totalgrid] = 0;
    totalgrid = totalgrid + 1;
    while (curgrid < totalgrid)
    {
        curX = Xlist[curgrid];
        curY = Ylist[curgrid];
        curstep = steplist[curgrid];
        if (curstep < step)
        {
            for (int i = 1; i < 5; i++)
            {
                nextX = curX + Xinc[i];
                nextY = curY + Yinc[i];
                if ((nextX < 0) || (nextX > 63) || (nextY < 0) || (nextY > 63))
                {
                    Bgrid[i] = 4;
                }
                else if (m_vcBattleSceneData[m_nbattleSceneNum].Data[3][nextX][nextY] >= 0)
                {
                    Bgrid[i] = 5;
                }
                else if (m_vcBattleSceneData[m_nbattleSceneNum].Data[1][nextX][nextY] > 0)
                {
                    Bgrid[i] = 1;
                }
                else if ((m_vcBattleSceneData[m_nbattleSceneNum].Data[2][nextX][nextY] >= 0)
                    && (m_vcBattleRole[m_vcBattleSceneData[m_nbattleSceneNum].Data[2][nextX][nextY]].Dead == 0))
                {
                    if (m_vcBattleRole[m_vcBattleSceneData[m_nbattleSceneNum].Data[2][nextX][nextY]].Team == myteam)
                    {
                        Bgrid[i] = 2;
                    }
                    else { Bgrid[i] = 3; }
                }
                else if (((m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 >= 179)
                    && (m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 <= 190))
                    || (m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 == 261)
                    || (m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 == 511)
                    || ((m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 >= 224)
                    && (m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 <= 232))
                    || ((m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 >= 662)
                    && (m_vcBattleSceneData[m_nbattleSceneNum].Data[0][nextX][nextY] / 2 <= 674)))
                {
                    Bgrid[i] = 6;
                }
                else
                {
                    Bgrid[i] = 0;
                }
            }

            //移动的情况
            //若为初始位置，不考虑旁边是敌军的情况
            //在移动过程中，旁边没有敌军的情况下才继续移动

            if (mode == 0)
            {
                if ((curstep == 0) || ((Bgrid[1] != 3) && (Bgrid[2] != 3) && (Bgrid[3] != 3) && (Bgrid[4] != 3)))
                {
                    for (int i = 1; i < 5; i++)
                    {
                        if (Bgrid[i] == 0)
                        {
                            Xlist[totalgrid] = curX + Xinc[i];
                            Ylist[totalgrid] = curY + Yinc[i];
                            steplist[totalgrid] = curstep + 1;
                            m_vcBattleSceneData[m_nbattleSceneNum].Data[3][Xlist[totalgrid]][Ylist[totalgrid]] = steplist[totalgrid];
                            totalgrid = totalgrid + 1;
                        }
                    }
                }
            }
            else                    //非移动的情况，攻击、医疗等
            {
                for (int i = 1; i < 5; i++)
                {
                    if ((Bgrid[i] == 0) || (Bgrid[i] == 2) || ((Bgrid[i] == 3)))
                    {
                        Xlist[totalgrid] = curX + Xinc[i];
                        Ylist[totalgrid] = curY + Yinc[i];
                        steplist[totalgrid] = curstep + 1;
                        m_vcBattleSceneData[m_nbattleSceneNum].Data[3][Xlist[totalgrid]][Ylist[totalgrid]] = steplist[totalgrid];
                        totalgrid = totalgrid + 1;
                    }
                }
            }
        }
        curgrid++;
    }
}

//移动

void BattleMap::moveRole(int bnum)
{
    int s, i;
    calCanSelect(bnum, 0, m_vcBattleRole[bnum].Step);
    if (selectAim(bnum, m_vcBattleRole[bnum].Step, 0))
    {
        moveAmination(bnum);
    }
}

//选择点
bool BattleMap::selectAim(int bnum, int step, int mods)
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
void BattleMap::moveAmination(int bnum)
{
    int s, i, a, tempx, tempy;
    int Xinc[5], Yinc[5];
    //  Ax = Bx - 4;            //测试用
    //  Ay = By - 4;
    if (m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy] > 0)     //0空位，1建筑，2友军，3敌军，4出界，5已走过 ，6水面
    {
        Xinc[1] = 1;
        Xinc[2] = -1;
        Xinc[3] = 0;
        Xinc[4] = 0;
        Yinc[1] = 0;
        Yinc[2] = 0;
        Yinc[3] = 1;
        Yinc[4] = -1;
        //      MyPoint *pInt = new MyPoint();
        //      pInt->x = Bx;
        //      pInt->y = By;
        //      wayQue.push(*pInt);
        //      MyPoint *pAInt = new MyPoint();
        //      pAInt->x = Ax;
        //      pAInt->y = Ay;
        m_nlinex[0] = m_nBx;
        m_nliney[0] = m_nBy;
        m_nlinex[m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy]] = m_nAx;
        m_nliney[m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy]] = m_nAy;
        a = m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nAx][m_nAy] - 1;
        while (a >= 0)
        {
            for (int i = 1; i < 5; i++)
            {
                tempx = m_nlinex[a + 1] + Xinc[i];
                tempy = m_nliney[a + 1] + Yinc[i];
                if (m_vcBattleSceneData[m_nbattleSceneNum].Data[3][tempx][tempy] == m_vcBattleSceneData[m_nbattleSceneNum].Data[3][m_nlinex[a + 1]][m_nliney[a + 1]] - 1)
                {
                    m_nlinex[a] = tempx;
                    m_nliney[a] = tempy;
                    break;
                }
            }
            a--;
        }

        m_ncurA = 1;
        //schedule(schedule_selector(BattleScene::moveAminationStep), battleSpeed, kRepeatForever, battleSpeed)
    }
}

void BattleMap::moveAminationStep(float dt)
{

    int a = m_ncurA;
    int bnum = m_ncurRoleNum;
    if (!((m_vcBattleRole[bnum].Step == 0) || ((m_nBx == m_nAx) && (m_nBy == m_nAy))))
    {
        if ((m_nlinex[a] - m_nBx) > 0)
        {
            m_vcBattleRole[bnum].Face = 3;
        }
        else if ((m_nlinex[a] - m_nBx) < 0)
        {
            m_vcBattleRole[bnum].Face = 0;
        }
        else if ((m_nliney[a] - m_nBy) < 0)
        {
            m_vcBattleRole[bnum].Face = 2;
        }
        else { m_vcBattleRole[bnum].Face = 1; }
        if (m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] == bnum)
        {
            m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] = -1;

        }
        m_nBx = m_nlinex[a];
        m_nBy = m_nliney[a];
        //m_vcBattleRole[bnum].X = m_nBx;
        //m_vcBattleRole[bnum].Y = m_nBy;
        if (m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] == -1)
        {
            m_vcBattleSceneData[m_nbattleSceneNum].Data[2][m_nBx][m_nBy] = bnum;
        }
        a++;
        m_ncurA = a;
        m_vcBattleRole[bnum].Step--;
        draw();
    }
    else
    {
        //      battleRole[bnum].X = Bx;
        //      battleRole[bnum].Y = By;
        //unschedule(schedule_selector(BattleScene::moveAminationStep));
        showBattleMenu(50, 500);
    }
}

void BattleMap::battleMainControl()
{

    battleMainControl(-1, -1);

}

void BattleMap::battleMainControl(int mods, int id)
{


    calMoveAbility(); //计算移动能力
    reArrangeBRole();
    //m_nBx = m_vcBattleRole[m_ncurRoleNum].X;
    //m_nBy = m_vcBattleRole[m_ncurRoleNum].Y;
    draw();
}

void BattleMap::attack(int bnum)
{
    int mnum, level;
    int i = 1;
    int rnum = m_vcBattleRole[bnum].RoleID;
    //mnum = m_Character[rnum].LMagic[i];
    //level = m_Character[rnum].MagLevel[i] / 100 + 1;
    m_ncurMagic = mnum;
}

void BattleMap::init()
{

    // 因为偏移文件的问题，所以暂时读不到这个文件。
    //auto UiLayer = new UI();
    //auto MenuSprite = new Menu();
    //MenuSprite->setPosition(20, 20);
    //for (int i = 0; i < 3; i++)
    //{
    //  auto ButtonSprite = new Button();
    //  ButtonSprite->setTexture("menu", i + 1, i + 33);
    //  MenuSprite->addButton(ButtonSprite, 0, i * 33);
    //  ButtonSprite->setSize(110, 24);
    //  //ButtonSprite->setFunction(BIND_FUNC(HelloWorldScene::func));
    //}
    //UiLayer->AddSprite(MenuSprite);
    //push(UiLayer);
}