#include "SubScene.h"
#include "MainMap.h"
#include "Esc.h"
#include "save.h"

SubScene::SubScene()
{
}


SubScene::~SubScene()
{
	for (auto &s : EarthS)
		s->release();
	for (auto &s : BuildS)
		s->release();
	for (auto &s : EventS)
		s->release();
	for (auto &s : AirS)
		s->release();
}

SubScene* SubScene::create(int scenenum)
{
	SubScene* pRet = new SubScene();
	if (pRet && pRet->init(scenenum))
	{
		pRet->autorelease();
		return pRet;
	}
	else
	{
		delete pRet;
		pRet = NULL;
		return NULL;
	}
}

Scene* SubScene::createScene(int scenenum)
{
	auto scene = Scene::create();
	auto layer = SubScene::create(scenenum);
	scene->addChild(layer, 0);
	return scene;
}

void SubScene::Draw()
{
	int k = 0;

	std::map<int, Sprite*> map;

	renderTex->beginWithClear(0, 0, 0, 0);
	for (int sum = -sumregion; sum <= sumregion + 15; sum++)
	{
		for (int i = -widthregion; i <= widthregion; i++)
		{
			int i1 = Sx + i + (sum / 2);
			int i2 = Sy - i + (sum - sum / 2);
			auto p = GetPositionOnScreen(i1, i2, Sx, Sy);
			if (i1 >= 0 && i1 <= MaxSceneCoord && i2 >= 0 && i2 <= MaxSceneCoord)
			{
				EarthS[k]->setVisible(false);
				BuildS[k]->setVisible(false);
				//这里注意状况
				Point p1 = Point(0, -m_SceneMapData[sceneNum].Data[4][i1][i2]);
				Point p2 = Point(0, -m_SceneMapData[sceneNum].Data[5][i1][i2]);
				int num = m_SceneMapData[sceneNum].Data[0][i1][i2] / 2;
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
				num = m_SceneMapData[sceneNum].Data[1][i1][i2] / 2;
				if (num > 0)
				{
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, num);
					auto s = BuildS[k];
					t->setToSprite(s, p + p1, drawCount);
					map[calBlockTurn(i1, i2, 1)] = s;
				}
				else if (i1 == Sx && i2 == Sy)
				{
					manPicture = offset_manPic + CommonScene::towards * num_manPic + step;
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, manPicture);
					auto s = BuildS[k];
					t->setToSprite(s, p + p1, 0);
					map[calBlockTurn(i1, i2, 1)] = s;
					//s->visit();
				}
				//事件层
				num = m_SceneMapData[sceneNum].Data[3][i1][i2];
				int picNum = m_SceneEventData[sceneNum].Data[num].BeginPic1 / 2;
				if (num > 0 && m_SceneEventData[sceneNum].Data[num].IsActive >= 0 && picNum > 0)
				{					
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, picNum);
					auto s = EventS[k];
					t->setToSprite(s, p + p1, drawCount);
					map[calBlockTurn(i1, i2, 2)] = s;
				}
				num = m_SceneMapData[sceneNum].Data[2][i1][i2] / 2;
				if (num > 0)
				{
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::Scene, num);
					auto s = AirS[k];
					t->setToSprite(s, p + p2, drawCount);
					map[calBlockTurn(i1, i2, 3)] = s;
				}
			}
			k++;
		}
	}
	for (auto i = map.begin(); i != map.end(); i++)
	{
		i->second->visit();
	}

}

bool SubScene::init(int scenenum)
{
	if (!Layer::init())
	{
		return false;
	}

	//计算共需多少个
	for (int i = 0; i < calNumberOfSprites(); i++)
	{
		addNewSpriteIntoVector(EarthS, 0);
		addNewSpriteIntoVector(BuildS, 1);
		addNewSpriteIntoVector(EventS, 2);
		addNewSpriteIntoVector(AirS, 3);
	}
	sceneNum = scenenum;
	initData(scenenum);
	
	Sx = m_SceneData[sceneNum].EntranceX;
	Sy = m_SceneData[sceneNum].EntranceY;
	

	keypressing = EventKeyboard::KeyCode::KEY_NONE;

	auto keyboardListener = EventListenerKeyboard::create();
	keyboardListener->onKeyPressed = CC_CALLBACK_2(SubScene::keyPressed, this);
	keyboardListener->onKeyReleased = CC_CALLBACK_2(SubScene::keyReleased, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);

	auto touchListener = EventListenerTouchOneByOne::create();
	touchListener->onTouchBegan = CC_CALLBACK_2(SubScene::touchBegan, this);
	touchListener->onTouchEnded = CC_CALLBACK_2(SubScene::touchEnded, this);
	touchListener->onTouchCancelled = CC_CALLBACK_2(SubScene::touchCancelled, this);
	touchListener->onTouchMoved = CC_CALLBACK_2(SubScene::touchMoved, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);

	//schedule(schedule_selector(SubScene::checkTimer), 0.025, kRepeatForever, 0.025);
	//pauseSchedulerAndActions();
	//resumeSchedulerAndActions();

	return true;
}

void SubScene::keyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	keypressing = keyCode;
}

void SubScene::keyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keypressing == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		auto esc = Esc::createScene();
		Director::getInstance()->pushScene(esc);
	}
	keypressing = EventKeyboard::KeyCode::KEY_NONE;
}


void SubScene::mouseDown(EventMouse eventMouse, Event* event)
{
	//auto target = dynamic_cast<Sprite*>(event->getCurrentTarget());
	//Point point = target->convertToNodeSpace();
}

void SubScene::mouseUp(EventMouse eventMouse, Event *event){}
void SubScene::mouseScroll(EventMouse eventMouse, Event *event){}
void SubScene::mouseMove(EventMouse eventMouse, Event *event){}

bool SubScene::touchBegan(Touch *touch, Event* event)
{
	//log("go in!");
	auto target = dynamic_cast<Sprite*>(event->getCurrentTarget());
	Point locationInNode = touch->getLocation();
	getMousePosition(&locationInNode);
	if (canWalk(Msx, Msy) && !checkIsOutScreen(Msx, Msy))
	{
		log("Msx=%d", Msx);
		FindWay(Sx, Sy, Msx, Msy);
		return true;
	};
	return false;
}

void SubScene::touchEnded(Touch *touch, Event *event){}

void SubScene::touchCancelled(Touch *touch, Event *event){}
void SubScene::touchMoved(Touch *touch, Event *event){}


void SubScene::checkTimer2()
{
	if (!isMenuOn){
		int x = Sx, y = Sy;
		if (!wayQue.empty())
		{
			MyPoint newMyPoint = wayQue.top();
			x = newMyPoint.x;
			y = newMyPoint.y;
			checkIsExit(x, y);
			Towards myTowards = (Towards)(newMyPoint.towards);
			log("myTowards=%d", myTowards);
			Walk(x, y, myTowards);
			wayQue.pop();
		}
		switch (keypressing)
		{
		case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
		{
			y--;
			checkIsExit(x, y);
			Walk(x, y, LeftDown);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
		{
			y++;
			checkIsExit(x, y);
			Walk(x, y, RightUp);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_UP_ARROW:
		{
			x--;
			checkIsExit(x, y);
			Walk(x, y, LeftUp);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
		{
			x++;
			checkIsExit(x, y);
			Walk(x, y, RightDown);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_ESCAPE:
		{
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_SPACE:
		case EventKeyboard::KeyCode::KEY_KP_ENTER:
		{
			stopFindWay();
			callEvent(x, y);
			break;
		}
		default:
		{
		}
		}

	}
	Draw();
}

void SubScene::Walk(int x, int y, Towards t)
{
	if (canWalk(x, y))
	{
		Sx = x;
		Sy = y;
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

bool SubScene::canWalk(int x, int y)
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

bool SubScene::checkIsBuilding(int x, int y)
{
	if (m_SceneMapData[sceneNum].Data[1][x][y] >= -2 && m_SceneMapData[sceneNum].Data[1][x][y] <= 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool SubScene::checkIsOutLine(int x, int y)
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

bool SubScene::checkIsHinder(int x, int y)
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

bool SubScene::checkIsEvent(int x, int y)
{
	//if (save.SData[sceneNum].SData[4][x][y] >= 0 && (save.DData[sceneNum].DData[save.SData[sceneNum].SData[3][x][y],0] % 10)<1)
	int num = m_SceneMapData[sceneNum].Data[3][x][y];
	int canWalk = m_SceneEventData[sceneNum].Data[num].CanWalk;
	if (canWalk > 0)
	{
		return true;
	}
	return false;
}

bool SubScene::checkIsFall(int x, int y)
{
	if (abs(m_SceneMapData[sceneNum].Data[4][x][y] - m_SceneMapData[sceneNum].Data[4][Sx][Sy] > 10))
	{
		true;
	}
	return false;
}

bool SubScene::checkIsExit(int x, int y)
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
		SaveGame::getInstance()->RBasic_Data.Mx = save.RScene[sceneNum]->MainEntranceX2;
		SaveGame::getInstance()->RBasic_Data.My = save.RScene[sceneNum]->MainEntranceY2;
		Mx = save.RScene[sceneNum]->MainEntranceX2;
		My = save.RScene[sceneNum]->MainEntranceY2;
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

void SubScene::callEvent(int x, int y)
{
	if (checkIsEvent(x,y))
	{
		int num = m_SceneMapData[sceneNum].Data[3][x][y];
		int eventNum = m_SceneEventData[sceneNum].Data[num].Num;
		//触发编号为eventNum的事件
	}
}

bool SubScene::checkIsOutScreen(int x, int y)
{
	if (abs(Sx - x) >= 2*widthregion || abs(Sy - y) >= sumregion)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void SubScene::getMousePosition(Point *point)
{
	int x = point->x;
	int y = Center_Y * 2 - point->y;
	//int yp = 0;
	int yp = -(m_SceneMapData[sceneNum].Data[4][x][y]);
	Msx = (-x + Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + Sx;
	Msy = (x - Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + Sy;
}

void SubScene::FindWay(int Mx, int My, int Fx, int Fy)
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
	while (!que.empty() && sNum <= 300){
		MyPoint *t = new MyPoint();
		t = que.top();
		que.pop();
		visited[t->x][t->y] = true;
		sNum++;
		//log("t.x=%d,t.y=%d",t->x,t->y);
		if (t->x == Fx && t->y == Fy){
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
		else{
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

void SubScene::stopFindWay()
{
	while (!wayQue.empty())
	{
		wayQue.pop();
	}
}