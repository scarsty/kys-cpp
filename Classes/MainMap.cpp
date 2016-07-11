#include "MainMap.h"
#include "Esc.h"
#include "SubScene.h"
#include "BattleScene.h"
#include "Character.h"
#include "BasicData.h"
#include "config.h"
#include <iomanip>
#include <fstream>
#include <istream>
#include <ostream>
#include "HelloWorldScene.h"

MainMap::MapArray MainMap::Earth, MainMap::Surface, MainMap::Building, MainMap::BuildX, MainMap::BuildY, MainMap::Entrance;

MainMap::MainMap()
{

}

MainMap::~MainMap()
{
	for (auto &s : EarthS)
	{
		s->release();
	}
	for (auto &s : SurfaceS)
	{
		s->release();
	}
}

void MainMap::ReadMap()
{
	log("Reading main map.");
	int length = sizeof(MapArray);
	ReadFile("earth.002", Earth, length);
	ReadFile("surface.002", Surface, length);
	ReadFile("building.002", Building, length);
	ReadFile("buildy.002", BuildY, length);
	ReadFile("buildx.002", BuildX, length);
}

void MainMap::Draw()
{
	int k = 0;
	float sss = clock();

	//auto s = MySprite::create();
	//tex->addChild(s);
	//tex->setClearFlags(GL_COLOR_BUFFER_BIT);

	std::map<int, Sprite*> map;
	renderTex->beginWithClear(0, 0, 0, 0);
	for (int sum = -sumregion; sum <= sumregion + 15; sum++)
	{
		for (int i = -widthregion; i <= widthregion; i++)
		{
			int i1 = Mx + i + (sum / 2);
			int i2 = My - i + (sum - sum / 2);
			auto p = GetPositionOnScreen(i1, i2, Mx, My);
			auto pMap = getMapPoint(i1, i2, Mx, My);
			if (i1 >= 0 && i1 <= MaxMainMapCoord && i2 >= 0 && i2 <= MaxMainMapCoord)
			{
				//EarthS[k]->setVisible(true);
				//SurfaceS[k]->setVisible(false);
				BuildingS[k]->setVisible(false);
				//共分3层，地面，表面，建筑，主角包括在建筑中
				if (Earth[i1][i2] > 0)
				{
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::MainMap, Earth[i1][i2] / 2);
					auto s = EarthS[k];
					t->setToSprite(s, pMap, drawCount);
					s->visit();
				}
				if (Surface[i1][i2] > 0)
				{
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::MainMap, Surface[i1][i2] / 2);
					auto s = SurfaceS[k];
					t->setToSprite(s, p);
					s->visit();
				}
				if (Building[i1][i2] > 0)
				{
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::MainMap, Building[i1][i2] / 2);
					auto s = BuildingS[k];
					if (t)
					{
						t->setToSprite(s, p, drawCount);
						if (t->getTexture())
						{
							//根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
							//次要排序依据是y坐标
							//直接设置z轴
							int c = ((i1 + i2) - (t->getTexture()->getPixelsWide() + 35) / 36 - (t->getOffset().y - t->getTexture()->getPixelsHigh() + 1) / 9) * 1024 + i1;
							map[2 * c + 1] = s;
						}
					}
					//s->visit();
				}
				if (i1 == Mx && i2 == My)
				{
					manPicture = offset_manPic + CommonScene::towards * num_manPic + step;
					if (restTime >= begin_restTime)
					{
						manPicture = offset_restPic + CommonScene::towards  * num_restPic + (restTime - begin_restTime) / each_pictrueTime % num_restPic;
					}
					auto t = MyTexture2D::getSelfPointer(MyTexture2D::MainMap, manPicture);
					auto s = BuildingS[k];
					t->setToSprite(s, p, 0);
					int c = 1024 * (i1 + i2) + i1;
					map[2 * c] = s;
					//s->setLocalZOrder(1024 * (i1 + i2) + i2);
					//s->visit();
				}
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
	//log("%f, %d", (clock() - sss) / CLOCKS_PER_SEC, k);
}

bool MainMap::init()
{
	if (!Layer::init())
	{
		return false;
	}

	srand(int(time(nullptr)));

	if (!_readed)
	{
		ReadMap();
	}
	_readed = true;

	//最开始的坐标.
	Mx = m_BasicData[0].Mx;
	My = m_BasicData[0].My;
	CommonScene::towards = (Towards)m_BasicData[0].MFace;
	//log("toward=%d\n", CommonScene::towards);
	//log("Mface=%d\n", SaveGame::getInstance()->RBasic_Data.Mface);

	//地面层和云层
	auto mainLayer = Node::create();
	auto cloudLayer = Node::create();
	this->addChild(mainLayer, 1);
	this->addChild(cloudLayer, 2);
	auto scene = HelloWorld::createScene();
	Director::getInstance()->replaceScene(scene);
	//计算共需多少个
	for (int i = 0; i < calNumberOfSprites(); i++)
	{
		addNewSpriteIntoVector(EarthS);
		addNewSpriteIntoVector(SurfaceS);
		addNewSpriteIntoVector(BuildingS);
	}
	//100个云
	Cloud::initTextures();
	for (int i = 0; i < 100; i++)
	{
		auto c = Cloud::create();
		CloudS.push_back(c);
		cloudLayer->addChild(c, i);
		c->initRand();
	}

	keypressing = EventKeyboard::KeyCode::KEY_NONE;
	//Draw();
	getEntrance();

	//drawWords("萬惡淫為首", 100, 100, 20);

	auto keyboardListener = EventListenerKeyboard::create();
	keyboardListener->onKeyPressed = CC_CALLBACK_2(MainMap::keyPressed, this);
	keyboardListener->onKeyReleased = CC_CALLBACK_2(MainMap::keyReleased, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(keyboardListener, this);
	auto item1 = MenuItemImage::create("test/test1.png", "test/test1.png", CC_CALLBACK_1(MainMap::menuCloseCallback, this));
	auto item2 = MenuItemImage::create("test/test2.png", "test/test2.png", CC_CALLBACK_1(MainMap::menuCloseCallback, this));
	item1->setTag(1);
	item2->setPositionX(+60);
	item2->setPositionY(+1);
	item2->setTag(2);
	auto menu = Menu::create(item1, item2, NULL);
	menu->setPosition(Vec2(500, 40));
	this->addChild(menu, 1);

	/*
	auto mouseListener = EventListenerMouse::create();
	mouseListener->onMouseDown = CC_CALLBACK_2(MainMap::mouseDown, this);
	mouseListener->onMouseUp = CC_CALLBACK_2(MainMap::mouseUp, this);
	mouseListener->onMouseScroll = CC_CALLBACK_2(MainMap::mouseScroll, this);
	mouseListener->onMouseMove = CC_CALLBACK_2(MainMap::mouseMove, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(mouseListener, this);
	*/
	auto touchListener = EventListenerTouchOneByOne::create();
	touchListener->onTouchBegan = CC_CALLBACK_2(MainMap::touchBegan, this);
	touchListener->onTouchEnded = CC_CALLBACK_2(MainMap::touchEnded, this);
	touchListener->onTouchCancelled = CC_CALLBACK_2(MainMap::touchCancelled, this);
	touchListener->onTouchMoved = CC_CALLBACK_2(MainMap::touchMoved, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(touchListener, this);
	//schedule(schedule_selector(CommonScene::checkTimer), 0.025, kRepeatForever, 0.025);

	return true;
}

Scene* MainMap::createScene()
{
	auto scene = Scene::create();
	auto layer = MainMap::create();
	scene->addChild(layer, 0, tag_mainLayer);
	//scene->addChild(layer->drawWords(), 1, tag_wordLayer);
	return scene;
}

void MainMap::keyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	keypressing = keyCode;
}

void MainMap::keyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
	if (keypressing == EventKeyboard::KeyCode::KEY_ESCAPE)
	{
		auto esc = Esc::createScene();
		Director::getInstance()->pushScene(esc);
	}
	keypressing = EventKeyboard::KeyCode::KEY_NONE;
}

void MainMap::mouseDown(EventMouse eventMouse, Event* event)
{
	//auto target = dynamic_cast<Sprite*>(event->getCurrentTarget());
	//Point point = target->convertToNodeSpace();
}

void MainMap::mouseUp(EventMouse eventMouse, Event *event) {}
void MainMap::mouseScroll(EventMouse eventMouse, Event *event) {}
void MainMap::mouseMove(EventMouse eventMouse, Event *event) {}

bool MainMap::touchBegan(Touch *touch, Event* event)
{
	auto target = dynamic_cast<Sprite*>(event->getCurrentTarget());
	//Point locationInNode = target->convertToNodeSpace(touch->getLocation());
	//Size s = target->getContentSize();
	//Rect rect = Rect(0, 0, s.width, s.height);
	Point locationInNode = touch->getLocation();
	//log("sprite began... x = %f, y = %f", locationInNode.x, locationInNode.y);
	//target->setOpacity(180);
	getMousePosition(&locationInNode);
	if (canWalk(Msx, Msy) && !checkIsOutScreen(Msx, Msy))
	{
		FindWay(Mx, My, Msx, Msy);
		return true;
	};
	return false;
}

void MainMap::touchEnded(Touch *touch, Event *event) {}

void MainMap::touchCancelled(Touch *touch, Event *event) {}
void MainMap::touchMoved(Touch *touch, Event *event) {}


//计时器，负责画图以及一些其他问题
void MainMap::checkTimer2()
{
	if (!isMenuOn) {
		int x = Mx, y = My;
		//drawCount++;
		if (!wayQue.empty())
		{
			MyPoint newMyPoint = wayQue.top();
			x = newMyPoint.x;
			y = newMyPoint.y;
			checkIsEntrance(x, y);
			Towards myTowards = (Towards)(newMyPoint.towards);
			//log("myTowards=%d", myTowards);
			Walk(x, y, myTowards);
			wayQue.pop();
			//log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
		}
		switch (keypressing)
		{
		case EventKeyboard::KeyCode::KEY_LEFT_ARROW:
		{
			y--;
			checkIsEntrance(x, y);
			Walk(x, y, LeftDown);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_RIGHT_ARROW:
		{
			y++;
			checkIsEntrance(x, y);
			Walk(x, y, RightUp);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_UP_ARROW:
		{
			x--;
			checkIsEntrance(x, y);
			Walk(x, y, LeftUp);
			stopFindWay();
			break;
		}
		case EventKeyboard::KeyCode::KEY_DOWN_ARROW:
		{
			x++;
			checkIsEntrance(x, y);
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
		{
			stopFindWay();
			m_BasicData[0].Mx = Mx;
			m_BasicData[0].My = My;
			m_BasicData[0].MFace = towards;
			auto scene = BattleScene::createScene(73);
			auto transitionScene = TransitionPageTurn::create(0.2f, scene, true);
			this->pause();
			Director::getInstance()->replaceScene(transitionScene);
		}
		default:
		{
			restTime++;
		}
		}
		cloudMove();
	}
	Draw();
	//BackGround->setTexture(tex->getSprite()->getTexture());
}

void MainMap::Walk(int x, int y, Towards t)
{
	if (canWalk(x, y))
	{
		Mx = x;
		My = y;
	}
	if (CommonScene::towards != t)
	{
		CommonScene::towards = t;
		//step = 0;
	}
	else
	{
		step++;
	}
	step = step % num_manPic;
	restTime = 0;
}

bool MainMap::checkIsBuilding(int x, int y)
{
	if (BuildX[x][y] == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

bool MainMap::checkIsWater(int x, int y)
{
	if (Earth[x][y] == 838 || Earth[x][y] >= 612 && Earth[x][y] <= 670)
	{
		return true;
	}
	else if (Earth[x][y] >= 358 && Earth[x][y] <= 362
		|| Earth[x][y] >= 506 && Earth[x][y] <= 670
		|| Earth[x][y] >= 1016 && Earth[x][y] <= 1022)
	{
		return true;
	}
	else
	{
		return false;
	}
}


bool MainMap::checkIsOutLine(int x, int y)
{
	if (x < 0 || x > MaxMainMapCoord || y < 0 || y > MaxMainMapCoord)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool MainMap::canWalk(int x, int y)
{
	if (checkIsBuilding(x, y) || checkIsOutLine(x, y) || checkIsWater(x, y))
	{
		return false;
	}
	else
	{
		return true;
	}
}

void MainMap::cloudMove()
{
	for (auto &c0 : CloudS)
	{
		auto c = dynamic_cast<Cloud*>(c0);
		c->changePosition();
		c->setPositionOnScreen(Mx, My, Center_X, Center_Y);
	}
}

bool MainMap::checkIsEntrance(int x, int y)
{
	if (Entrance[x][y] > 0 && Entrance[x][y] <= config::MAXScene)
	{
		m_BasicData[0].Mx = Mx;
		m_BasicData[0].My = My;
		m_BasicData[0].MFace = towards;
		auto scene = SubScene::createScene(Entrance[x][y]);
		auto transitionScene = TransitionPageTurn::create(0.2f, scene, true);
		this->pause();
		Director::getInstance()->replaceScene(transitionScene);
		return true;
	}
	return false;
}

void MainMap::getEntrance()
{
	for (int x = 0; x < maxX; x++)
		for (int y = 0; y < maxY; y++)
		{
			Entrance[x][y] = -1;
		}
	for (int i = 0; i < m_SceneData.size(); i++)
	{

		int x = m_SceneData[i].MainEntranceX1;
		int y = m_SceneData[i].MainEntranceY1;
		if (x > 0 && y > 0 && x < maxX && y < maxY)
		{
			Entrance[x][y] = i;
		}
		x = m_SceneData[i].MainEntranceX2;
		y = m_SceneData[i].MainEntranceY2;
		if (x > 0 && y > 0 && x < maxX && y < maxY)
		{
			Entrance[x][y] = i;
		}
	}
}

//A*寻路
void MainMap::FindWay(int Mx, int My, int Fx, int Fy)
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


bool MainMap::checkIsOutScreen(int x, int y)
{
	if (abs(Mx - x) >= 2 * widthregion || abs(My - y) >= sumregion)
	{
		return true;
	}
	else
	{
		return false;
	}
}

void MainMap::getMousePosition(Point *point)
{
	int x = point->x;
	int y = Center_Y * 2 - point->y;
	int yp = 0;
	Msx = (-x + Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + Mx;
	Msy = (x - Center_X + 2 * (y + yp) - 2 * Center_Y + 18) / 36 + My;
}

void MainMap::menuCloseCallback(Ref* pSender)
{
#if (CC_TARGET_PLATFORM == CC_PLATFORM_WP8) || (CC_TARGET_PLATFORM == CC_PLATFORM_WINRT)
	MessageBox("You pressed the close button. Windows Store Apps do not implement a close button.", "Alert");
	return;
#endif
	auto p = dynamic_cast<MenuItemImage*> (pSender);

	if (p == nullptr)
		return;

	switch (p->getTag())
	{
	case 1:
	{

		Save::getInstance()->LoadR(0);
		break;
	}
	case 2:
	{

		Save::getInstance()->SaveR(0);
		break;
	}
	}
#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS)
	exit(0);
#endif
}

void MainMap::stopFindWay()
{
	while (!wayQue.empty())
	{
		wayQue.pop();
	}
}

