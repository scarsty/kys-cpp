#include "SubMap.h"
#include "MainMap.h"
#include "BattleScene.h"
#include"Event.h"

SubMap::SubMap()
{
	full_window_ = 1;
}


SubMap::~SubMap()
{

}

void SubMap::draw()
{
    int k = 0;
    struct DrawInfo { int i; Point p; };
    std::map<int, DrawInfo> map;
    auto& m_SceneMapData = Save::getInstance()->m_SceneMapData;
	auto& m_SceneEventData = Save::getInstance()->m_SceneEventData;
    for (int sum = -sumregion; sum <= sumregion + 15; sum++)
    {
        for (int i = -widthregion; i <= widthregion; i++)
        {
            int i1 = Cx + i + (sum / 2);
            int i2 = Cy - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, Cx, Cy);
            if (i1 >= 0 && i1 <= MaxSceneCoord && i2 >= 0 && i2 <= MaxSceneCoord)
            {
                //EarthS[k]->setm_bvisible(false);
                //BuildS[k]->setm_bvisible(false);
                //这里注意状况
                Point p1 = Point(0, -m_SceneMapData[sceneNum].Data[4][i1][i2]);
                Point p2 = Point(0, -m_SceneMapData[sceneNum].Data[5][i1][i2]);
                int num = m_SceneMapData[sceneNum].Data[0][i1][i2] / 2;
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
                num = m_SceneMapData[sceneNum].Data[1][i1][i2] / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                    map[calBlockTurn(i1, i2, 1)] = { num, p };
                }
                else if (i1 == Cx && i2 == Cy)
                {
                    manPicture = offset_manPic + Scene::towards * num_manPic + step;
                    map[calBlockTurn(i1, i2, 1)] = { manPicture, p };
                    //s->visit();
                }
                //事件层
                num = m_SceneMapData[sceneNum].Data[3][i1][i2];

                int picNum = m_SceneEventData[sceneNum].Data[num].BeginPic1 / 2;
                if (num > 0 && m_SceneEventData[sceneNum].Data[num].IsActive >= 0 && picNum > 0)
                {
// 	                auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, picNum);
//                     auto s = EventS[k];
//                     t->setToSprite(s, p + p1, drawCount);
//                     map[calBlockTurn(i1, i2, 2)] = s;
					TextureManager::getInstance()->renderTexture("smap", picNum, p.x, p.y);
					map[calBlockTurn(i1, i2, 2)] = { picNum, p };
                }

				//空中层
				num = m_SceneMapData[sceneNum].Data[2][i1][i2] / 2;
				if (num > 0)
				{
					p.y = p.y + p1.y + p2.y;				
					map[calBlockTurn(i1, i2, 3)] = { num, p };
				}
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    {
        TextureManager::getInstance()->renderTexture("smap", i->second.i, i->second.p.x, i->second.p.y);
    }

}

void SubMap::init()
{
    Cx = Save::getInstance()->m_SceneData[sceneNum].EntranceX;
    Cy = Save::getInstance()->m_SceneData[sceneNum].EntranceY;
}

void SubMap::setPosition(int x, int y)
{
	Cx = x;
	Cy = y;
}

void SubMap::dealEvent(BP_Event& e)
{
    int x = Cx, y = Cy;
    //drawCount++;
    if (e.type == BP_MOUSEBUTTONDOWN)
    {
        getMousePosition(e.button.x, e.button.y);
        stopFindWay();
        if (canWalk(Msx, Msy) && !checkIsOutScreen(Msx, Msy))
        {
            FindWay(Cx, Cy, Msx, Msy);
        }
    }
    if (!wayQue.empty())
    {
        Point newMyPoint = wayQue.top();
        x = newMyPoint.x;
        y = newMyPoint.y;
        checkIsExit(x, y);
        Towards myTowards = (Towards)(newMyPoint.towards);
        //log("myTowards=%d", myTowards);
        walk(x, y, myTowards);
        wayQue.pop();
        //log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
    }
    if (e.type == BP_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
        case BPK_LEFT:
        {
            y--;
            if (checkIsExit(x, y))
            {
                break;
            }
            walk(x, y, LeftDown);
            stopFindWay();
            break;
        }
        case BPK_RIGHT:
        {
            y++;
            if (checkIsExit(x, y))
            {
                break;
            }
            walk(x, y, RightUp);
            stopFindWay();
            break;
        }
        case BPK_UP:
        {
            x--;
            if (checkIsExit(x, y))
            {
                break;
            }
            walk(x, y, LeftUp);
            stopFindWay();
            break;
        }
        case BPK_DOWN:
        {
            x++;
            if (checkIsExit(x, y))
            {
                break;
            }
            walk(x, y, RightDown);
            stopFindWay();
            break;
        }
        case BPK_ESCAPE:
        {
            stopFindWay();
            break;
        }
        case BPK_RETURN:
        case BPK_SPACE:
        {
			stopFindWay();
			ReSetEventPosition(x, y);
			if(Save::getInstance()->m_SceneMapData[sceneNum].Data[3][x][y] >= 0)
				if (Save::getInstance()->m_SceneEventData[sceneNum].Data[Save::getInstance()->m_SceneMapData[sceneNum].Data[3][x][y]].Event1 >=0)
					EventManager::getInstance()->callEvent(Save::getInstance()->m_SceneEventData[sceneNum].Data[Save::getInstance()->m_SceneMapData[sceneNum].Data[3][x][y]].Event1);
				

            break;
        }
        case SDLK_b:
        {
            auto s = new BattleScene();
            push(s);
            break;
        }
        default:
        {
        }
        }
    }
}

void SubMap::walk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        Cx = x;
        Cy = y;
    }
    if (Scene::towards != t)
    {
        Scene::towards = t;
        step = 0;
    }
    else
    {
        step++;
    }
    step = step % num_manPic;
}

bool SubMap::canWalk(int x, int y)
{

    if (checkIsOutLine(x, y) || checkIsBuilding(x, y) || checkIsHinder(x, y)
        || !checkIsEvent(x, y) || checkIsFall(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool SubMap::checkIsBuilding(int x, int y)
{
    if (Save::getInstance()->m_SceneMapData[sceneNum].Data[1][x][y] >= -2 && Save::getInstance()->m_SceneMapData[sceneNum].Data[1][x][y] <= 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool SubMap::checkIsOutLine(int x, int y)
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

bool SubMap::checkIsHinder(int x, int y)
{
    if (Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] >= 358 && Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] <= 362
        || Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] == 522 || Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] == 1022
        || Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] >= 1324 && Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] <= 1330
        || Save::getInstance()->m_SceneMapData[sceneNum].Data[0][x][y] == 1348)
    {
        return true;
    }
    return false;
}

bool SubMap::checkIsEvent(int x, int y)
{
    //if (save.SData[sceneNum].SData[4][x][y] >= 0 && (save.DData[sceneNum].DData[save.SData[sceneNum].SData[3][x][y],0] % 10)<1)
    int num = Save::getInstance()->m_SceneMapData[sceneNum].Data[3][x][y];
    int canWalk = Save::getInstance()->m_SceneEventData[sceneNum].Data[num].CanWalk;
    if (canWalk > 0)
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        
 		return false;
	}
    return true;
}

bool SubMap::checkIsFall(int x, int y)
{
    if (abs(Save::getInstance()->m_SceneMapData[sceneNum].Data[4][x][y] - Save::getInstance()->m_SceneMapData[sceneNum].Data[4][Cx][Cy] > 10))
    {
        true;
    }
    return false;
}

bool SubMap::checkIsExit(int x, int y)
{
    if ((int)Save::getInstance()->m_SceneData[sceneNum].ExitX[0] == x 
		&& (int)Save::getInstance()->m_SceneData[sceneNum].ExitY[0] == y
        || (int)Save::getInstance()->m_SceneData[sceneNum].ExitX[2] == x 
		&& (int)Save::getInstance()->m_SceneData[sceneNum].ExitY[2] == y)
    {
        pop();
		Save::getInstance()->m_BasicData[0].m_sWhere = 0;
        return true;
    }
    else if ((int)Save::getInstance()->m_SceneData[sceneNum].ExitX[1] == x 
		&& (int)Save::getInstance()->m_SceneData[sceneNum].ExitY[1] == y)
    {
		
        /*
        SaveGame::getInstance()->RBasic_Data.Mface = towards;
        SaveGame::getInstance()->RBasic_Data.Mx = save.RScene[sceneNum]->MainEntranceX2;
        SaveGame::getInstance()->RBasic_Data.My = save.RScene[sceneNum]->MainEntranceY2;
        Mx = save.RScene[sceneNum]->MainEntranceX2;
        My = save.RScene[sceneNum]->MainEntranceY2;
        MainMap* mainMap = dynamic_cast<MainMap*>(Director::getInstance()->getRunningScene()->getChildByTag(MainMap::tag_mainLayer));
        CommonScene::replaceLocation();
        */
        pop();
		Save::getInstance()->m_BasicData[0].m_sWhere = 0;
        return true;
    }
    return false;
}

//111
// void SubScene::callEvent(int x, int y)
// {
// 	int nNum = Save::getInstance()->m_SceneMapData[sceneNum].Data[3][x][y];
// 	int nEventNum = Save::getInstance()->m_SceneEventData[sceneNum].Data[nNum].Num; //触发编号为eventNum的事件
// 	if (nEventNum > 0)
// 	{
// 		string idxPath, grpPath;
// 		idxPath = "resource/kdef.idx";
// 		grpPath = "resource/kdef.grp";
// 		int nLen = 0, nOffset = 0, nTemp = 0;
// 		FILE *fpidx = fopen(idxPath.c_str(), "rb");
// 		FILE *fpgrp = fopen(grpPath.c_str(), "rb");
// 		if (!fpidx)
// 		{
// 			fprintf(stderr, "Can not open file %s\n", idxPath.c_str());
// 			return;
// 		}
// 		if (!fpgrp)
// 		{
// 			fprintf(stderr, "Can not open file %s\n", grpPath.c_str());
// 			return;
// 		}
// 		if (0 == nEventNum)
// 		{
// 			nOffset = 0;
// 			fread(&nLen, 4, 1, fpidx);
// 		}
// 		else
// 		{
// 			fseek(fpidx, (nEventNum - 1) * 4, SEEK_SET);
// 			fread(&nOffset, 4, 1, fpidx);
// 			fread(&nLen, 4, 1, fpidx);
// 		}
// 
// 		nLen = (nLen - nOffset) / 2;
// 
// 		vector<int> vc_Event;
// 		vc_Event.resize(nLen + 1);
// 
// 		fseek(fpgrp, nOffset, SEEK_CUR);
// 		fread((void*)&vc_Event[0], nLen * 2, 1, fpgrp);
// 
// 		fclose(fpidx);
// 		fclose(fpgrp);
// 		int i = 0;
// 		while (vc_Event[i] >= 0)
// 		{
// 			switch (vc_Event[i])
// 			{
// 			case 0:
// 			{
// 				break;
// 			}
// 
// 			default:
// 				break;
// 			}
// 		}
// 
// 	}
// 	
// 	
// }

bool SubMap::checkIsOutScreen(int x, int y)
{
    if (abs(Cx - x) >= 2 * widthregion || abs(Cy - y) >= sumregion)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SubMap::getMousePosition(int _x, int _y)
{
    int x = _x;
    int y = _y;
    //int yp = 0;
    int yp = -(Save::getInstance()->m_SceneMapData[sceneNum].Data[4][x][y]);
    Msx = (-(x - Center_X) / singleMapScene_X + (y + yp - Center_Y) / singleMapScene_Y) / 2 + Cx;
    Msy = ((y + yp - Center_Y) / singleMapScene_Y + (x - Center_X) / singleMapScene_X) / 2 + Cy;
}

void SubMap::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } };   //四个方向
    Point* myPoint = new Point;
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = (Point::Towards)CallFace(Mx, My, Fx, Fy);
    myPoint->parent = myPoint;
    myPoint->Heuristic(Fx, Fy);
    //log("Fx=%d,Fy=%d", Fx, Fy);
    //log("Mx=%d,My=%d", Mx, My);
    while (!wayQue.empty())
    {
        wayQue.pop();
    }
    std::priority_queue<Point*, std::vector<Point*>, Compare> que;            //最小优先级队列(开启列表)
    que.push(myPoint);
    int sNum = 0;
    while (!que.empty() && sNum <= 300)
    {
        Point* t = new Point();
        t = que.top();
        que.pop();
        visited[t->x][t->y] = true;
        sNum++;
        //log("t.x=%d,t.y=%d",t->x,t->y);
        if (t->x == Fx && t->y == Fy)
        {
            minStep = t->step;
            wayQue.push(*t);
            int k = 0;
            while (t != myPoint && k <= minStep)
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
        else
        {
            for (int i = 0; i < 4; i++)
            {
                Point* s = new Point();
                s->x = t->x + dirs[i][0];
                s->y = t->y + dirs[i][1];
                if (canWalk(s->x, s->y) && !checkIsOutScreen(s->x, s->y) && !visited[s->x][s->y])
                {
                    s->g = t->g + 10;
                    s->towards = (Point::Towards)i;
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

void SubMap::stopFindWay()
{
    while (!wayQue.empty())
    {
        wayQue.pop();
    }
}

/*========================================
说明: 修正事件坐标
==========================================*/
void SubMap::ReSetEventPosition(int &x, int &y)
{
	switch (Scene::towards)
	{
	case Scene::LeftDown:
	{
		y--;
		break;
	}
	case Scene::RightUp:
	{
		y++;
		break;
	}
	case Scene::LeftUp :
	{
		x--;
		break;
	}
	case Scene::RightDown:
	{
		x++;
		break;
	}
	default:
		break;
	}
}