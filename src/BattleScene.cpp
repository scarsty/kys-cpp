#include "BattleScene.h"
#include "Esc.h"
#include "MainMap.h"
#include <iostream>
#include <string>
#include <math.h>
#include <algorithm>
#include "config.h"
#include "Save.h"

BattleScene::BattleScene()
{
    full = 1;
}


BattleScene::~BattleScene()
{
}

void BattleScene::Draw()
{
    int k = 0;

    std::map<int, Sprite*> map;

    renderTex->beginWithClear(0, 0, 0, 0);
    for (int sum = -sumregion; sum <= sumregion + 15; sum++)
    {
        for (int i = -widthregion; i <= widthregion; i++)
        {
            int i1 = Bx + i + (sum / 2);
            int i2 = By - i + (sum - sum / 2);
            auto p = GetPositionOnScreen(i1, i2, Bx, By);
            if (i1 >= 0 && i1 <= MaxSceneCoord && i2 >= 0 && i2 <= MaxSceneCoord)
            {
                EarthS[k]->setVisible(false);
                BuildS[k]->setVisible(false);
                //这里注意状况
// 				Point p1 = Point(0, -m_SceneMapData[sceneNum].Data[4][i1][i2]);
// 				Point p2 = Point(0, -m_SceneMapData[sceneNum].Data[5][i1][i2]);
                Point p1, p2 = Point(0, 0);
                int num = Battle.bData[sceneNum].Data[0][i1][i2] / 2;
                if (num >= 0)
                {
                    auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, num);
                    auto s = EarthS[k];
                    t->setToSprite(s, p, drawCount);
                    if (p1.y < -0.1)
                    {
                        map[calBlockTurn(i1, i2, 0)] = s;
                    }
                    else
                    {
                        s->visit();
                    }
                }
                //建筑和主角同一层
                num = Battle.bData[sceneNum].Data[1][i1][i2] / 2;
                if (num > 0)
                {
                    auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, num);
                    auto s = BuildS[k];
                    t->setToSprite(s, p + p1, drawCount);
                    map[calBlockTurn(i1, i2, 1)] = s;
                }
                for (int h = 0; h < BRole.size(); h++) {
                    int xx = BRole[h].X;
                    int yy = BRole[h].Y;
                    if (i1 == BRole[h].X && i2 == BRole[h].Y && BRole[h].rnum >= 0) {
                        offset_BRolePic = MyTexture2D::tex[MyTexture2D::Battle][BRole[h].rnum].size() / 4;
                        auto t = MyTexture2D::getSelfPointer(MyTexture2D::Battle, BRole[h].Face * (offset_BRolePic), BRole[h].rnum);
                        auto s = BuildS[k];
                        t->setToSprite(s, p + p1, 0);
                        map[calBlockTurn(i1, i2, 1)] = s;
                    }
                }
                // 				if (i1 == Bx && i2 == By && BRole[curRoleNum].rnum >= 0)
                // 				{
                // 					//manPicture = offset_manPic + CommonScene::towards * num_manPic + step;
                // 					offset_BRolePic = MyTexture2D::tex[MyTexture2D::Battle][BRole[curRoleNum].rnum].size() / 4;
                // 					auto t = MyTexture2D::getSelfPointer(MyTexture2D::Battle, towards * (offset_BRolePic), BRole[curRoleNum].rnum);
                // 					auto s = BuildS[k];
                // 					t->setToSprite(s, p + p1, 0);
                // 					map[calBlockTurn(i1, i2, 1)] = s;
                // 					//s->visit();
                // 				}
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    {
        i->second->visit();
    }
    renderTex->end();
    BackGround->getTexture()->setAntiAliasTexParameters();
}

bool BattleScene::init(int battlenum)
{
    if (!Layer::init())
    {
        return false;
    }
    ///ShowMultiMenu(0, 0, 0);
    //计算共需多少个
    for (int i = 0; i < calNumberOfSprites(); i++)
    {
        addNewSpriteIntoVector(EarthS, 0);
        addNewSpriteIntoVector(BuildS, 1);
        addNewSpriteIntoVector(EventS, 2);
        addNewSpriteIntoVector(AirS, 3);
    }
    battleNum = battlenum;
    sceneNum = Battle.warSta[battleNum].battleMap;
    curRoleNum = 1;
    Bx = 20;
    By = 20;

    //battle(scenenum,100,-1,1,33,55);
    //memset(ResultofBattle, -1, sizeof(int)*maxBRoleSelect);
    selectTeamMembers();
    //initData(sceneNum);

    keypressing = EventKeyboard::KeyCode::KEY_NONE;

    auto keyboardListener = EventListenerKeyboard::create();
    keyboardListener->onKeyPressed = CC_CALLBACK_2(BattleScene::keyPressed, this);
    keyboardListener->onKeyReleased = CC_CALLBACK_2(BattleScene::keyReleased, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);

    auto touchListener = EventListenerTouchOneByOne::create();
    touchListener->onTouchBegan = CC_CALLBACK_2(BattleScene::touchBegan, this);
    touchListener->onTouchEnded = CC_CALLBACK_2(BattleScene::touchEnded, this);
    touchListener->onTouchCancelled = CC_CALLBACK_2(BattleScene::touchCancelled, this);
    touchListener->onTouchMoved = CC_CALLBACK_2(BattleScene::touchMoved, this);
    _eventDispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);

    //schedule(schedule_selector(BattleScene::checkTimer), 0.025, kRepeatForever, 0.025);

    return true;
}

void BattleScene::keyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
    keypressing = keyCode;
}

void BattleScene::keyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
    if (keypressing == EventKeyboard::KeyCode::KEY_ESCAPE)
    {
        // 		auto esc = Esc::createScene();
        // 		Director::getInstance()->pushScene(esc);
    }
    keypressing = EventKeyboard::KeyCode::KEY_NONE;
}

bool BattleScene::touchBegan(Touch *touch, Event* event)
{
    //log("go in!");
    auto target = dynamic_cast<Sprite*>(event->getCurrentTarget());
    Point locationInNode = touch->getLocation();
    getMousePosition(&locationInNode);
    if (canWalk(Msx, Msy) && !checkIsOutScreen(Msx, Msy))
    {
        //log("MBx=%d", Msx);
        //FindWay(Bx, By, Msx, Msy);
        return true;
    }
    return false;
}

void BattleScene::touchEnded(Touch *touch, Event *event) {

}

void BattleScene::touchCancelled(Touch *touch, Event *event) {
    Msx = Bx;
    Msy = By;
}
void BattleScene::touchMoved(Touch *touch, Event *event) {}


void BattleScene::checkTimer2()
{
    if (!isMenuOn) {
        moveRole(curRoleNum);
    }
    //Draw();
}

void BattleScene::Walk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        Bx = x;
        By = y;
    }
    if (CommonScene::towards != t)
    {
        CommonScene::towards = t;
        step = 0;
    }
    else
    {
        step++;
    }
    step = step % offset_BRolePic;
}

bool BattleScene::canWalk(int x, int y)
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

bool BattleScene::checkIsBuilding(int x, int y)
{
    if (Battle.bData[sceneNum].Data[1][x][y] >= -2 && Battle.bData[sceneNum].Data[1][x][y] <= 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool BattleScene::checkIsOutLine(int x, int y)
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

bool BattleScene::checkIsHinder(int x, int y)
{
    if (m_SceneMapData[sceneNum].Data[0][x][y] >= 358 && m_SceneMapData[sceneNum].Data[0][x][y] <= 362
        || m_SceneMapData[sceneNum].Data[0][x][y] == 522 || m_SceneMapData[sceneNum].Data[0][x][y] == 1022
        || m_SceneMapData[sceneNum].Data[0][x][y] >= 1324 && m_SceneMapData[sceneNum].Data[0][x][y] <= 1330
        || m_SceneMapData[sceneNum].Data[0][x][y] == 1348)
    {
        return true;
    }
    return false;
}

bool BattleScene::checkIsEvent(int x, int y)
{
    //if (m_SceneMapData[sceneNum].Data[4][x][y] >= 0 && (m_SceneEventData[sceneNum].Data[m_SceneMapData[sceneNum].Data[3][x][y],0] % 10)<1)
    int num = m_SceneMapData[sceneNum].Data[3][x][y];
    int canWalk = m_SceneEventData[sceneNum].Data[num].CanWalk;
    if (canWalk > 0)
    {
        return true;
    }
    return false;
}

bool BattleScene::checkIsFall(int x, int y)
{
    if (abs(m_SceneMapData[sceneNum].Data[4][x][y] - m_SceneMapData[sceneNum].Data[4][Bx][By] > 10))
    {
        true;
    }
    return false;
}

bool BattleScene::checkIsExit(int x, int y)
{
    if ((int)m_SceneData[sceneNum].ExitX[0] == x && (int)m_SceneData[sceneNum].ExitY[0] == y
        || (int)m_SceneData[sceneNum].ExitX[2] == x && (int)m_SceneData[sceneNum].ExitY[2] == y)
    {
        auto map = MainMap::createScene();
        auto transitionMap = TransitionPageTurn::create(0.2f, map, false);
        this->pause();
        Director::getInstance()->replaceScene(transitionMap);
        return true;
    }
    else if ((int)m_SceneData[sceneNum].ExitX[1] == x && (int)m_SceneData[sceneNum].ExitY[1] == y)
    {
        /*
        SaveGame::getInstance()->RBasic_Data.Mface = towards;
        SaveGame::getInstance()->RBasic_Data.Mx = m_SceneData[sceneNum].MainEntranceX2;
        SaveGame::getInstance()->RBasic_Data.My = m_SceneData[sceneNum].MainEntranceY2;
        Mx = m_SceneData[sceneNum].MainEntranceX2;
        My = m_SceneData[sceneNum].MainEntranceY2;
        MainMap* mainMap = dynamic_cast<MainMap*>(Director::getInstance()->getRunningScene()->getChildByTag(MainMap::tag_mainLayer));
        CommonScene::replaceLocation();
        */
        auto map = MainMap::createScene();
        auto transitionMap = TransitionPageTurn::create(0.2f, map, false);
        this->pause();
        Director::getInstance()->replaceScene(transitionMap);
        return true;
    }
    return false;
}

void BattleScene::callEvent(int x, int y)
{
    if (checkIsEvent(x, y))
    {
        int num = m_SceneMapData[sceneNum].Data[3][x][y];
        int eventNum = m_SceneEventData[sceneNum].Data[num].Num;
        //触发编号为eventNum的事件
    }
}

bool BattleScene::checkIsOutScreen(int x, int y)
{
    if (abs(Bx - x) >= 16 || abs(By - y) >= 20)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void BattleScene::getMousePosition(Point *point)
{
    int x = point->x;
    int y = Center_Y * 2 - point->y;
    //int yp = 0;
    int yp = -(m_SceneMapData[sceneNum].Data[4][x][y]);
    Msx = (-x + Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + Bx;
    Msy = (x - Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + By;
}

void BattleScene::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };									//已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } };		//四个方向
    MyPoint *myPoint = new MyPoint;
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = (MyPoint::Towards)CallFace(Mx, My, Fx, Fy);
    myPoint->parent = myPoint;
    myPoint->Heuristic(Fx, Fy);
    //log("Fx=%d,Fy=%d", Fx, Fy);
    //log("Mx=%d,My=%d", Mx, My);
    while (!wayQue.empty())
    {
        wayQue.pop();
    }
    priority_queue<MyPoint*, vector<MyPoint*>, Compare> que;			//最小优先级队列(开启列表)
    que.push(myPoint);
    int sNum = 0;
    while (!que.empty() && sNum <= 300) {
        MyPoint *t = new MyPoint();
        t = que.top();
        que.pop();
        visited[t->x][t->y] = true;
        sNum++;
        //log("t.x=%d,t.y=%d",t->x,t->y);
        if (t->x == Fx && t->y == Fy) {
            minStep = t->step;
            wayQue.push(*t);
            int k = 0;
            while (t != myPoint&&k <= minStep)
            {
                //log("t.x=%d,t.y=%d,s.x=%d,s.y=%d,t.f=%d", t->x, t->y, t->parent->x, t->parent->y,t->f);

                t->towards = t->parent->towards;
                wayQue.push(*t);
                t = t->parent;
                k++;
                //log("go in!");
            }
            //log("minStep=%d", minStep);
            //log("wayQue=%d", wayQue.size());
            break;
        }
        else {
            for (int i = 0; i < 4; i++)
            {
                MyPoint *s = new MyPoint();
                s->x = t->x + dirs[i][0];
                s->y = t->y + dirs[i][1];
                if (canWalk(s->x, s->y) && !checkIsOutScreen(s->x, s->y) && !visited[s->x][s->y])
                {
                    s->g = t->g + 10;
                    s->towards = (MyPoint::Towards)i;
                    if (s->towards == t->towards)
                    {
                        s->Heuristic(Fx, Fy);
                    }
                    else
                    {
                        s->h = s->Heuristic(Fx, Fy) + 1;
                    }
                    s->step = t->step + 1;
                    s->f = s->g + s->h;
                    //t->towards = (MyPoint::Towards)i;
                    //s->Gx = dirs[i][0];
                    //s->Gy = dirs[i][1];
                    //t->child[i] = s;
                    //if (s->parent)
                    s->parent = t;
                    //log("s.x=%d,s.y=%d,t.x=%d,t.y=%d", s->x, s->y, t->x, t->y);
                    //log("s.g=%d,s.h=%d,s.f=%d", s.g, s.h, s.f);
                    que.push(s);
                }
            }
        }
    }
    myPoint->delTree(myPoint);
}


void BattleScene::loadBattleTiles()
{
    int fightNum = m_Character[0].FightNum;

}

int BattleScene::CallFace(int x1, int y1, int x2, int y2)
{
    int d1, d2, dm;
    d1 = x2 - x1;
    d2 = y2 - y1;
    dm = abs(d1) - abs(d2);
    if ((d1 != 0) || (d2 != 0)) {
        if (dm >= 0) {
            if (d1 < 0) {
                return 0;
            }
            else {
                return 3;
            }
        }
        else {
            if (d2 < 0) {
                return 2;
            }
            else {
                return 1;
            }
        }
    }
}

int BattleScene::CallFace(int num1, int num2)
{
    return CallFace(m_Character[num1].Sx, m_Character[num1].Sy, m_Character[num2].Sx, m_Character[num2].Sy);
}

void BattleScene::initData(int scenenum)
{
    // 	for (int i = 0; i < maxBRoleSelect; i++)
    // 	{
    // 		int numBRole = ResultofBattle[i];
    // 		char *fightPath = new char[30];
    // 		sprintf(fightPath, "fight/fight%03d", numBRole);
    // 		char *fightPathIn = new char[30];
    // 		sprintf(fightPathIn, "fight/fight%03d/index.ka", numBRole);
    // 		auto file = FileUtils::getInstance();
    // 		//std::fstream file;
    // 		if (file->isFileExist(fightPathIn)){
    // 			loadTexture(fightPath, MyTexture2D::Battle, 250, numBRole);
    // 		}
    // 		//delete(fightPath);
    // 		delete(fightPathIn);
    // 	}	
}



void BattleScene::autoSetMagic(int rnum)
{


}

bool BattleScene::autoInBattle()
{
    int x, y;
    int autoCount = 0;
    for (int i = 0; i < battle::MaxBRoleNum; i++)
    {
        BRole[i].Team = 1;
        BRole[i].rnum = -1;
        //我方自动参战数据
        if (mods >= -1) {
            for (int i = 0; i < sizeof(Battle.warSta[battleNum].mate) / sizeof(Battle.warSta[battleNum].mate[0]); i++) {
                x = Battle.warSta[battleNum].mate_x[i];
                y = Battle.warSta[battleNum].mate_y[i];
            }
            if (mods == -1)
            {
                BRole[BRoleAmount].rnum = Battle.warSta[battleNum].autoMate[i];
            }
            else {
                BRole[BRoleAmount].rnum = -1;
            }
            BRole[BRoleAmount].Team = 0;
            BRole[BRoleAmount].Y = y;
            BRole[BRoleAmount].X = x;
            BRole[BRoleAmount].Face = 2;
            if (BRole[BRoleAmount].rnum == -1)
            {
                BRole[BRoleAmount].Dead = 1;
                BRole[BRoleAmount].Show = 1;
            }
            else {
                BRole[BRoleAmount].Dead = 0;
                BRole[BRoleAmount].Show = 0;
                if (!((m_Character[BRole[BRoleAmount].rnum].TeamState == 1) || (m_Character[BRole[BRoleAmount].rnum].TeamState == 2)) && !(m_Character[BRole[BRoleAmount].rnum].Faction == m_Character[0].Faction))
                {
                    autoSetMagic(BRole[BRoleAmount].rnum);
                    autoCount++;
                }
            }
            BRole[BRoleAmount].Step = 0;
            BRole[BRoleAmount].Acted = 0;
            BRole[BRoleAmount].ExpGot = 0;
            if (BRole[BRoleAmount].rnum == 0) {
                BRole[BRoleAmount].Auto = -1;
            }
            else {
                BRole[BRoleAmount].Auto = 3;
            }
            BRole[BRoleAmount].Progress = 0;
            BRole[BRoleAmount].round = 0;
            BRole[BRoleAmount].Wait = 0;
            BRole[BRoleAmount].frozen = 0;
            BRole[BRoleAmount].killed = 0;
            BRole[BRoleAmount].Knowledge = 0;
            BRole[BRoleAmount].Zhuanzhu = 0;
            BRole[BRoleAmount].szhaoshi = 0;
            BRole[BRoleAmount].pozhao = 0;
            BRole[BRoleAmount].wanfang = 0;

        }
        //自动参战结束
    }
    return true;
}

int BattleScene::selectTeamMembers()
{

    menuOn();
    int i, i2;
    max0 = 1;
    menuString[0] = "全員出戰";
    for (i = 0; i < config::MaxTeamMember; i++)
    {
        if (m_BasicData[0].TeamList[i] >= 0)
        {
            menuString[i + 1] = m_Character[m_BasicData[0].TeamList[i]].Name;
            max0++;
        }
    }
    menuString[max0] = "開始戰鬥";
    ShowMultiMenu(max0, 0);
    Draw();
    return true;
}

void BattleScene::ShowMultiMenu(int max0, int menuNum)
{
    //unschedule(schedule_selector(CommonScene::checkTimer));
    string str;
    for (int k = 0; k < 8; k++)
    {
        SlectOfresult[k] = false;
        item[k] = nullptr;
    }

    for (int i = 0; i < 6; i++)
    {
        ResultofBattle[i] = -1;
        BattleList[i] = -1;//  参战人员列表初始化 不参战为-1
    }
    for (int i = 0; i <= max0; i++)
    {
        MenuItemFont::setFontName("fonts/chinese.ttf");
        MenuItemFont::setFontSize(24);
        str = CommonScene::a2u(menuString[i].c_str());
        item[i] = MenuItemFont::create(str, CC_CALLBACK_1(BattleScene::menuCloseCallback, this));
        if (menuString[i] == "開始戰鬥")
        {
            item[i]->setTag(8);
        }
        else
        {
            item[i]->setTag(i + 1);
        }


    }

    item[max0]->setEnabled(false);
    CommonScene::cleanAllWords();
    initMultiMenu();
    showSlectMenu(s_list, 0);
    Vec2 vec[7];
    for (int i = 0; i < 7; i++)
    {
        vec[i].x = xx;
        vec[i].y = yy - 170 - i * 20;
    }
    CommonScene::drawWords(s_list, 20, BattleScene::kindOfRole, vec);

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
        if (BRole[curRoleNum].Step > 0) {
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
void BattleScene::showBattleMenu(int x, int y)
{
    menuOn();
    MenuItemImage *item1[12];
    for (int i = 0; i < 12; i++)
    {
        string str, str1;
        str = "menu/" + to_string(21 + i) + ".png";
        str1 = "menu/" + to_string(33 + i) + ".png";
        item1[i] = MenuItemImage::create(str, str1, CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
        item1[i]->setTag(11 + i);
        item1[i]->setPositionY(-30 * i);
    }
    auto menu = Menu::create(item1[0], item1[1], item1[2], item1[3], item1[4], item1[5], item1[6], item1[7], item1[8], item1[9], item1[10], item1[11], NULL);
    menu->setPosition(Vec2(x, y));
    SpriteLayer->addChild(menu);

}


void BattleScene::showSlectMenu(string *str, int x)
{
    string name = "备战人物姓名：";
    string attack = "攻击力：";
    string defense = "防御力：";
    string dodge = "轻功    ：";
    string sword = "剑法    ：";
    string quanfa = "拳法    ：";
    string daofa = "刀法    ：";
    int num = m_BasicData[0].TeamList[x];
    name += m_Character[num].Name;
    attack += to_string(m_Character[num].Attack);
    defense += to_string(m_Character[num].Defence);
    dodge += to_string(m_Character[num].Speed);
    str[0] = name;
    str[1] = attack;
    str[2] = defense;
    str[3] = dodge;
    str[4] = sword;
    int num1 = ((m_Character[num].Fencing) / 200.0) * 10;
    CommonScene::showRoleAbility(xx + 90, yy - 170 - 4 * 20 + 10, num1);
    str[5] = quanfa;
    num1 = ((m_Character[num].Boxing) / 200.0) * 10;
    CommonScene::showRoleAbility(xx + 90, yy - 170 - 5 * 20 + 10, num1);
    str[6] = daofa;
    num1 = ((m_Character[num].Knife) / 200.0) * 10;
    CommonScene::showRoleAbility(xx + 90, yy - 170 - 6 * 20 + 10, num1);
    CommonScene::showRoleVerticaldRawing(xx + 20, 63, num, 40, 0.8, 0.9);
    string str1 = "ui/beizhan/Node.csb";
    CommonScene::myRunAction(str1, 0, 25);
    CommonScene::cleanFiveDimensional();
    CommonScene::showFiveDimensional(xx + 80, yy, num);
}

void BattleScene::initMultiMenu()
{
    auto s = Director::getInstance()->getWinSize();
    auto menu = Menu::create(item[0], item[1], item[2], item[3], item[4], item[5], item[6], item[7], nullptr);
    menu->alignItemsVertically();
    menu->setPosition(Vec2(s.width / 2.0 + 150, s.height / 2.0 + 50));

    auto m_backgrand = Sprite::create("ui/beizhan/1.png");
    m_backgrand->setPosition(Vec2(s.width / 2.0, s.height / 2.0));

    auto m_backgrand1 = Sprite::create("ui/beizhan/21.png");
    m_backgrand1->setScaleX(0.4);
    m_backgrand1->setScaleY(1.0);
    m_backgrand1->setPosition(Vec2(s.width / 2.0 + 150, s.height / 2.0 + 50));

    auto m_backgrand4 = Sprite::create("ui/beizhan/21.png");
    m_backgrand4->setScaleX(1);
    m_backgrand4->setScaleY(1.3);
    m_backgrand4->setOpacity(50);
    m_backgrand4->setPosition(Vec2(s.width / 2.0, s.height / 2.0));

    auto m_backgrand2 = Sprite::create("ui/beizhan/4.png");
    m_backgrand2->setPosition(Vec2(s.width / 2.0 - 170, s.height / 2.0));
    m_backgrand2->setScale(1.3);
    m_backgrand2->setOpacity(40);

    auto m_backgrand3 = Sprite::create("ui/beizhan/5.png");
    m_backgrand3->setPosition(Vec2(s.width / 2.0 + 50, s.height / 2.0));
    m_backgrand3->setScale(1.3);
    m_backgrand3->setOpacity(40);

    auto item1 = MenuItemImage::create("ui/beizhan/1-1.png", "ui/beizhan/1-1.png", CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
    item1->setTag(9);
    auto item2 = MenuItemImage::create("ui/beizhan/1-2.png", "ui/beizhan/1-2.png", CC_CALLBACK_1(BattleScene::menuCloseCallback1, this));
    item2->setTag(10);
    item2->setPositionY(-75);
    auto menu1 = Menu::create(item1, item2, NULL);
    menu1->setPosition(Vec2(297, 392 - 170));

    SpriteLayer->addChild(m_backgrand);
    SpriteLayer->addChild(m_backgrand4);
    //addChild(m_backgrand3);
    SpriteLayer->addChild(menu1);
    SpriteLayer->addChild(menu);

}


bool BattleScene::battle(int battlenum, int getexp, int mods, int id, int maternum, int enemyrnum)
{
    return true;
}

bool BattleScene::initBattleData()
{

    battle::FactionBackup FBackup[10];
    battle::FactionBackup FBackup2[10];
    battle::FactionBackup TFBackup;

    TFBackup.len = 0;
    TFBackup.captain = -1;
    for (int i = 0; i < 10; i++) {
        FBackup[i].len = 0;
        FBackup[i].captain = -1;
        FBackup2[i].len = 0;
        FBackup2[i].captain = -1;
    }

    for (int i1 = 0; i1 <= 63; i1++) {
        for (int i2 = 0; i2 <= 63; i2++) {
            Battle.bData[sceneNum].Data[2][i1][i2] = -1;
            Battle.bData[sceneNum].Data[4][i1][i2] = -1;
            Battle.bData[sceneNum].Data[5][i1][i2] = -1;
        }
    }
    BRoleAmount = 0;
    //initBattleRoleState();

    return true;
}

bool BattleScene::initBattleRoleState()
{
    BRole.resize(MaxBRoleNum);
    for (int i = 0; i < MaxBRoleNum; i++)
    {
        BRole[i].X = -1;
        BRole[i].Y = -1;
        BRole[i].Show = 1;
    }
    BStatus = 0;
    isBattle = true;
    if (mods == -1) {
        selectTeamMembers();
    }
    else {
    }
    BRoleAmount = 0;
    int n0 = 0;
    int teamNum = 0;
    if (Battle.warSta[battleNum].mate[0] == 0) {
        teamNum = 1;
    }
    for (int i = 0; teamNum < maxBRoleSelect && i < sizeof(Battle.warSta[battleNum].mate) / sizeof(Battle.warSta[battleNum].mate[0]); i++) {
        if (Battle.warSta[battleNum].mate[BRoleAmount] >= 0) {
            BRole[BRoleAmount].Y = Battle.warSta[battleNum].mate_x[i];
            BRole[BRoleAmount].X = Battle.warSta[battleNum].mate_y[i];
            BRole[BRoleAmount].Team = 0;
            BRole[BRoleAmount].Face = 2;
            BRole[BRoleAmount].rnum = Battle.warSta[battleNum].mate[BRoleAmount];
            BRole[BRoleAmount].Auto = -1;
            setInitState(n0);
        }
        else if (BattleList[teamNum] >= 0) {
            BRole[BRoleAmount].Y = Battle.warSta[battleNum].mate_x[i];
            BRole[BRoleAmount].X = Battle.warSta[battleNum].mate_y[i];
            BRole[BRoleAmount].Team = 0;
            BRole[BRoleAmount].Face = 2;
            BRole[BRoleAmount].rnum = BattleList[teamNum];
            BRole[BRoleAmount].Auto = -1;
            setInitState(n0);
            teamNum++;
        }
        BRoleAmount++;
    }
    calMoveAbility();
    return true;
}

void BattleScene::setInitState(int &n0) {

    BRole[BRoleAmount].Step = 0;
    BRole[BRoleAmount].Acted = 0;
    BRole[BRoleAmount].ExpGot = 0;
    BRole[BRoleAmount].Show = 0;
    BRole[BRoleAmount].Progress = 0;
    BRole[BRoleAmount].round = 0;
    BRole[BRoleAmount].Wait = 0;
    BRole[BRoleAmount].frozen = 0;
    BRole[BRoleAmount].killed = 0;
    BRole[BRoleAmount].Knowledge = 0;
    BRole[BRoleAmount].Zhuanzhu = 0;
    BRole[BRoleAmount].szhaoshi = 0;
    BRole[BRoleAmount].pozhao = 0;
    BRole[BRoleAmount].wanfang = 0;
    for (int j = 0; j <= 4; j++) {
        n0 = 0;
        if (BRole[BRoleAmount].rnum > -1) {
            for (int j1 = 0; j1 <= 9; j1++) {
                if (m_Character[BRole[BRoleAmount].rnum].GongTi >= 0) {
                    if ((m_Magic[m_Character[BRole[BRoleAmount].rnum].LMagic[m_Character[BRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j)) {
                        n0 = m_Magic[m_Character[BRole[BRoleAmount].rnum].LMagic[m_Character[BRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
                    }
                }
            }
        }
        BRole[BRoleAmount].zhuangtai[j] = 100;
        BRole[BRoleAmount].lzhuangtai[j] = n0;
    }
    for (int j = 5; j <= 9; j++) {
        n0 = 0;
        if (BRole[BRoleAmount].rnum > -1) {
            for (int j1 = 0; j1 <= 9; j1++) {
                if (m_Character[BRole[BRoleAmount].rnum].GongTi >= 0) {
                    if ((m_Magic[m_Character[BRole[BRoleAmount].rnum].LMagic[m_Character[BRole[BRoleAmount].rnum].GongTi]].MoveDistance[j1] == 60 + j)) {
                        n0 = m_Magic[m_Character[BRole[BRoleAmount].rnum].LMagic[m_Character[BRole[BRoleAmount].rnum].GongTi]].AttDistance[j1];
                    }
                }
            }
        }
        BRole[BRoleAmount].zhuangtai[j] = n0;
        BRole[BRoleAmount].lzhuangtai[j] = n0;
    }
    for (int j = 10; j <= 13; j++) {
        BRole[BRoleAmount].zhuangtai[j] = 0;
    }

}
//计算可移动步数(考虑装备)

void BattleScene::calMoveAbility() {
    int i, rnum, addspeed;
    maxspeed = 0;
    for (int i = 0; i < BRole.size(); i++) {
        rnum = BRole[i].rnum;
        if (rnum > -1) {
            addspeed = 0;
            // 			if (CheckEquipSet(RRole[rnum].Equip[0], RRole[rnum].Equip[1], RRole[rnum].Equip[2], RRole[rnum].Equip[3]) == 5){
            // 				addspeed += 30;
            // 			}
            BRole[i].speed = (getRoleSpeed(BRole[i].rnum, true) + addspeed);
            if (BRole[i].Wait == 0) {
                BRole[i].Step = round(power(BRole[i].speed / 15, 0.8) * (100 + BRole[i].zhuangtai[8]) / 100);
                if (maxspeed > BRole[i].speed) {
                    maxspeed = maxspeed;
                }
                else maxspeed = BRole[i].speed;
            }
            if (m_Character[rnum].Moveable > 0) {
                BRole[i].Step = 0;
            }
        }
    }

}

//按轻功重排人物(未考虑装备)
void BattleScene::reArrangeBRole() {

    int i, n, n1, i1, i2, x, t, s1, s2;
    battle::TBattleRole temp;
    i1 = 0;
    i2 = 1;
    for (i1 = 0; i1 < BRole.size() - 1; i1++) {
        for (i2 = i1 + 1; i2 < BRole.size(); i2++) {
            s1 = 0;
            s2 = 0;
            if ((BRole[i1].rnum > -1) && (BRole[i1].Dead == 0)) {
                s1 = getRoleSpeed(BRole[i1].rnum, true);
                // 			if checkEquipSet(Rrole[Brole[i1].rnum].Equip[0], Rrole[Brole[i1].rnum].Equip[1],
                // 				Rrole[Brole[i1].rnum].Equip[2], Rrole[Brole[i1].rnum].Equip[3]) = 5 then
                // 			s1  = s1 + 30;
            }
            if ((BRole[i2].rnum > -1) && (BRole[i2].Dead == 0)) {
                s2 = getRoleSpeed(BRole[i2].rnum, true);
            }
            if ((BRole[i1].rnum != 0) && (BRole[i1].Team != 0) && (s1 < s2) && (BRole[i2].rnum != 0) && (BRole[i2].Team != 0)) {
                temp = BRole[i1];
                BRole[i1] = BRole[i2];
                BRole[i2] = temp;
            }
        }
    }

    for (i1 = 0; i1 < 64; i1++) {
        for (i2 = 0; i2 < 64; i2++) {
            Battle.bData[sceneNum].Data[2][i1][i2] = -1;
            Battle.bData[sceneNum].Data[5][i1][i2] = -1;
        }
    }
    n = 0;
    for (i = 0; i < BRole.size(); i++) {
        if ((BRole[i].Dead == 0) && (BRole[i].rnum >= 0)) {
            n++;
        }
    }
    n1 = 0;
    for (i = 0; i < BRole.size(); i++) {
        if (BRole[i].rnum >= 0) {
            if (BRole[i].Dead == 0) {
                Battle.bData[sceneNum].Data[2][BRole[i].X][BRole[i].Y] = i;
                Battle.bData[sceneNum].Data[5][BRole[i].X][BRole[i].Y] = -1;
                // 				if battlemode > 0 then
                // 					Brole[i].Progress : = (n - n1) * 5;
                n1++;
            }
            else {
                Battle.bData[sceneNum].Data[2][BRole[i].X][BRole[i].Y] = -1;
                Battle.bData[sceneNum].Data[5][BRole[i].X][BRole[i].Y] = i;
            }
        }
    }
    i2 = 0;
    // 	if (battlemode > 0) then
    // 		for i1 : = 0 to length(Brole) - 1 do
    // 			if ((GetPetSkill(5, 1) and(Brole[i1].rnum = 0)) or(GetPetSkill(5, 3) and(Brole[i1].Team = 0))) then
    // 				Brole[i1].Progress : = 299 - i2 * 5;
    // 	i2 = i2 + 1;
}

//获取人物速度
int BattleScene::getRoleSpeed(int rnum, bool Equip) {
    int l;
    int bResult;
    bResult = m_Character[rnum].Speed;
    if (m_Character[rnum].GongTi > -1) {
        l = getGongtiLevel(rnum);
        bResult = m_Magic[m_Character[rnum].LMagic[m_Character[rnum].GongTi]].AddSpd[l];
    }
    if (Equip) {
        for (int l = 0; l < config::MaxEquipNum; l++) {
            if (m_Character[rnum].Equip[l] >= 0) {
                bResult += m_Item[m_Character[rnum].Equip[l]].AddSpeed;
            }
            bResult = bResult * 100 / (100 + m_Character[rnum].Wounded + m_Character[rnum].Poison);
        }
    }
    return bResult;
}

//获取功体经验
int BattleScene::getGongtiLevel(int rnum) {
    int i;
    int n = m_Character[rnum].GongTi;
    if ((rnum >= 0) && (n >= -1)) {
        if (m_Magic[m_Character[rnum].LMagic[n]].MaxLevel > m_Character[rnum].MagLevel[n] / 100) {
            return m_Character[rnum].MagLevel[n] / 100;
        }
        else return m_Magic[m_Character[rnum].LMagic[n]].MaxLevel;
    }
    else return 0;
}


float BattleScene::power(float Base, float Exponent) {
    if (Exponent == 0.0) {
        return 1.0;
    }
    else if ((Base == 0.0) && (Exponent > 0.0)) {
        return 0.0;
    }
    else if (Base < 0) {
        /*Error(reInvalidOp);*/
        return 0.0; // avoid warning W1035 Return value of function '%s' might be undefined
    }
    else return exp(Exponent * log(Base));
}

//初始化范围
//mode=0移动，1攻击用毒医疗等，2查看状态
void BattleScene::calCanSelect(int bnum, int mode, int Step) {
    int i;
    if (mode == 0) {
        for (int i1 = 0; i1 <= 63; i1++) {
            for (int i2 = 0; i2 <= 63; i2++) {
                Battle.bData[sceneNum].Data[3][i1][i2] = -1;
            }
        }
        Battle.bData[sceneNum].Data[3][BRole[bnum].X][BRole[bnum].Y] = 0;
        seekPath2(BRole[bnum].X, BRole[bnum].Y, Step, BRole[bnum].Team, mode);
    }
    if (mode == 1) {
        for (int i1 = 0; i1 <= 63; i1++) {
            for (int i2 = 0; i2 <= 63; i2++) {
                Battle.bData[sceneNum].Data[3][i1][i2] = -1;
                if (abs(i1 - BRole[bnum].X) + abs(i2 - BRole[bnum].Y) <= step) {
                    Battle.bData[sceneNum].Data[3][i1][i2] = 0;
                }
            }
        }
    }

    if (mode == 2) {
        for (int i1 = 0; i1 <= 63; i1++) {
            for (int i2 = 0; i2 <= 63; i2++) {
                Battle.bData[sceneNum].Data[3][i1][i2] = -1;
                if (Battle.bData[sceneNum].Data[2][i1][i2] >= 0) {
                    Battle.bData[sceneNum].Data[3][i1][i2] = 0;
                }
            }
        }
    }
}

//计算可以被选中的位置
//利用队列
//移动过程中，旁边有敌人，则不能继续移动
void BattleScene::seekPath2(int x, int y, int step, int myteam, int mode) {
    int Xlist[4096];
    int Ylist[4096];
    int steplist[4096];
    int curgrid, totalgrid;
    int Bgrid[5]; //0空位，1建筑，2友军，3敌军，4出界，5已走过 ，6水面
    int	Xinc[5], Yinc[5];
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
        if (curstep < step) {
            for (int i = 1; i < 5; i++) {
                nextX = curX + Xinc[i];
                nextY = curY + Yinc[i];
                if ((nextX < 0) || (nextX > 63) || (nextY < 0) || (nextY > 63)) {
                    Bgrid[i] = 4;
                }
                else if (Battle.bData[sceneNum].Data[3][nextX][nextY] >= 0) {
                    Bgrid[i] = 5;
                }
                else if (Battle.bData[sceneNum].Data[1][nextX][nextY] > 0) {
                    Bgrid[i] = 1;
                }
                else if ((Battle.bData[sceneNum].Data[2][nextX][nextY] >= 0) && (BRole[Battle.bData[sceneNum].Data[2][nextX][nextY]].Dead == 0)) {
                    if (BRole[Battle.bData[sceneNum].Data[2][nextX][nextY]].Team == myteam) {
                        Bgrid[i] = 2;
                    }
                    else Bgrid[i] = 3;
                }
                else if (((Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 >= 179) && (Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 <= 190)) ||
                    (Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 == 261) || (Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 == 511) ||
                    ((Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 >= 224) && (Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 <= 232)) ||
                    ((Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 >= 662) && (Battle.bData[sceneNum].Data[0][nextX][nextY] / 2 <= 674))) {
                    Bgrid[i] = 6;
                }
                else
                    Bgrid[i] = 0;
            }

            //移动的情况
            //若为初始位置，不考虑旁边是敌军的情况
            //在移动过程中，旁边没有敌军的情况下才继续移动

            if (mode == 0) {
                if ((curstep == 0) || ((Bgrid[1] != 3) && (Bgrid[2] != 3) && (Bgrid[3] != 3) && (Bgrid[4] != 3))) {
                    for (int i = 1; i < 5; i++) {
                        if (Bgrid[i] == 0) {
                            Xlist[totalgrid] = curX + Xinc[i];
                            Ylist[totalgrid] = curY + Yinc[i];
                            steplist[totalgrid] = curstep + 1;
                            Battle.bData[sceneNum].Data[3][Xlist[totalgrid]][Ylist[totalgrid]] = steplist[totalgrid];
                            totalgrid = totalgrid + 1;
                        }
                    }
                }
            }
            else {					//非移动的情况，攻击、医疗等
                for (int i = 1; i < 5; i++) {
                    if ((Bgrid[i] == 0) || (Bgrid[i] == 2) || ((Bgrid[i] == 3))) {
                        Xlist[totalgrid] = curX + Xinc[i];
                        Ylist[totalgrid] = curY + Yinc[i];
                        steplist[totalgrid] = curstep + 1;
                        Battle.bData[sceneNum].Data[3][Xlist[totalgrid]][Ylist[totalgrid]] = steplist[totalgrid];
                        totalgrid = totalgrid + 1;
                    }
                }
            }
        }
        curgrid++;
    }
}

//移动

void BattleScene::moveRole(int bnum) {
    int s, i;
    calCanSelect(bnum, 0, BRole[bnum].Step);
    if (selectAim(bnum, BRole[bnum].Step, 0)) {
        moveAmination(bnum);
    }
}

//选择点
bool BattleScene::selectAim(int bnum, int step, int mods) {

    switch (keypressing)
    {
    case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
    {
        Ay--;
        if ((abs(Ax - Bx) + abs(Ay - By) > step) || (Battle.bData[sceneNum].Data[3][Ax][Ay] < 0)) {
            Ay++;
        }
        return false;
        break;
    }
    case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
    {
        Ay++;
        if ((abs(Ax - Bx) + abs(Ay - By) > step) || (Battle.bData[sceneNum].Data[3][Ax][Ay] < 0)) {
            Ay--;
        }
        return false;
        break;
    }
    case EventKeyboard::KeyCode::KEY_UP_ARROW:
    {
        Ax--;
        if ((abs(Ax - Bx) + abs(Ay - By) > step) || (Battle.bData[sceneNum].Data[3][Ax][Ay] < 0)) {
            Ax++;
        }
        return false;
        break;
    }
    case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
    {
        Ax++;
        if ((abs(Ax - Bx) + abs(Ay - By) > step) || (Battle.bData[sceneNum].Data[3][Ax][Ay] < 0)) {
            Ax--;
        }
        return false;
        break;
    }
    case EventKeyboard::KeyCode::KEY_ESCAPE:
    {
        Ax = Bx;
        Ay = By;
        return false;
        break;
    }
    case EventKeyboard::KeyCode::KEY_SPACE:
    case EventKeyboard::KeyCode::KEY_KP_ENTER:
    {
        return true;
        break;
    }
    default:
    {
        if (Msx >= 0 && Msy >= 0 && Msx != Ax && Msy != Ay) {
            if ((abs(Msx - Bx) + abs(Msy - By) > step) || (Battle.bData[sceneNum].Data[3][Msx][Msy] < 0)) {
                Ax = Bx;
                Ay = By;
            }
            else
            {
                Ax = Msx;
                Ay = Msy;
            }
            return true;
        }
        return false;
    }
    }
}

//移动动画
void BattleScene::moveAmination(int bnum) {
    int s, i, a, tempx, tempy;
    int Xinc[5], Yinc[5];
    // 	Ax = Bx - 4;			//测试用
    // 	Ay = By - 4;
    if (Battle.bData[sceneNum].Data[3][Ax][Ay] > 0) {   //0空位，1建筑，2友军，3敌军，4出界，5已走过 ，6水面
        Xinc[1] = 1;
        Xinc[2] = -1;
        Xinc[3] = 0;
        Xinc[4] = 0;
        Yinc[1] = 0;
        Yinc[2] = 0;
        Yinc[3] = 1;
        Yinc[4] = -1;
        // 		MyPoint *pInt = new MyPoint();
        // 		pInt->x = Bx;
        // 		pInt->y = By;
        // 		wayQue.push(*pInt);
        // 		MyPoint *pAInt = new MyPoint();
        // 		pAInt->x = Ax;
        // 		pAInt->y = Ay;
        linex[0] = Bx;
        liney[0] = By;
        linex[Battle.bData[sceneNum].Data[3][Ax][Ay]] = Ax;
        liney[Battle.bData[sceneNum].Data[3][Ax][Ay]] = Ay;
        a = Battle.bData[sceneNum].Data[3][Ax][Ay] - 1;
        while (a >= 0) {
            for (int i = 1; i < 5; i++) {
                tempx = linex[a + 1] + Xinc[i];
                tempy = liney[a + 1] + Yinc[i];
                if (Battle.bData[sceneNum].Data[3][tempx][tempy] == Battle.bData[sceneNum].Data[3][linex[a + 1]][liney[a + 1]] - 1) {
                    linex[a] = tempx;
                    liney[a] = tempy;
                    break;
                }
            }
            a--;
        }

        curA = 1;
        schedule(schedule_selector(BattleScene::moveAminationStep), battleSpeed, kRepeatForever, battleSpeed);



    }
}

void BattleScene::moveAminationStep(float dt) {

    int a = curA;
    int bnum = curRoleNum;
    if (!((BRole[bnum].Step == 0) || ((Bx == Ax) && (By == Ay)))) {
        if ((linex[a] - Bx) > 0) {
            BRole[bnum].Face = 3;
        }
        else if ((linex[a] - Bx) < 0) {
            BRole[bnum].Face = 0;
        }
        else if ((liney[a] - By) < 0) {
            BRole[bnum].Face = 2;
        }
        else BRole[bnum].Face = 1;
        if (Battle.bData[sceneNum].Data[2][Bx][By] == bnum) {
            Battle.bData[sceneNum].Data[2][Bx][By] = -1;

        }
        Bx = linex[a];
        By = liney[a];
        BRole[bnum].X = Bx;
        BRole[bnum].Y = By;
        if (Battle.bData[sceneNum].Data[2][Bx][By] == -1) {
            Battle.bData[sceneNum].Data[2][Bx][By] = bnum;
        }
        a++;
        curA = a;
        BRole[bnum].Step--;
        Draw();
    }
    else {
        // 		BRole[bnum].X = Bx;
        // 		BRole[bnum].Y = By;
        unschedule(schedule_selector(BattleScene::moveAminationStep));
        showBattleMenu(50, 500);
    }
}

void BattleScene::battleMainControl() {

    battleMainControl(-1, -1);

}

void BattleScene::battleMainControl(int mods, int id) {


    calMoveAbility(); //计算移动能力
    reArrangeBRole();
    Bx = BRole[curRoleNum].X;
    By = BRole[curRoleNum].Y;
    Draw();
}

void BattleScene::attack(int bnum)
{
    int mnum, level;
    int i = 1;
    int rnum = BRole[bnum].rnum;
    mnum = m_Character[rnum].LMagic[i];
    level = m_Character[rnum].MagLevel[i] / 100 + 1;
    curMagic = mnum;

}