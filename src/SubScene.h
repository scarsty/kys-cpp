#pragma once

#include "Scene.h"


class SubScene : public Scene
{
public:
    SubScene();
    virtual ~SubScene();

    int Sx, Sy;
	int manPicture;
	int step = 0;
	int const offset_manPic = 5001;		//初始场景主角图偏移量
	int const num_manPic = 7;			//单向主角图张数
	int sceneNum;						//场景号
	std::vector<Sprite*> EarthS, BuildS, AirS, EventS;
	stack<MyPoint> wayQue;												//栈(路径栈)


	void Draw();

    virtual bool init(int scenenum);

	static SubScene* create(int scenenum);

	static Scene* createScene(int scenenum);

	//void keyPressed(EventKeyboard::KeyCode keyCode, Event *event);
	//void keyReleased(EventKeyboard::KeyCode keyCode, Event *event);
	void Walk(int x, int y, Towards t);

	virtual void keyPressed(EventKeyboard::KeyCode keyCode, Event *event);
	virtual void keyReleased(EventKeyboard::KeyCode keyCode, Event *event);
	void mouseDown(EventMouse eventMouse, Event *event);
	void mouseUp(EventMouse eventMouse, Event *event);
	void mouseScroll(EventMouse eventMouse, Event *event);
	void mouseMove(EventMouse eventMouse, Event *event);
	bool touchBegan(Touch *touch, Event *event);
	void touchEnded(Touch *touch, Event *event);
	void touchCancelled(Touch *touch, Event *event);
	void touchMoved(Touch *touch, Event *event);

	void checkTimer2();
	void callEvent(int x, int y);
	bool canWalk(int x, int y);
	virtual bool checkIsBuilding(int x, int y);
	virtual bool checkIsOutLine(int x, int y);
	bool checkIsHinder(int x, int y);
	bool checkIsEvent(int x, int y);
	bool checkIsFall(int x, int y);
	bool checkIsExit(int x, int y);
	bool checkIsOutScreen(int x, int y);
	void stopFindWay();

	void getMousePosition(Point *point);
	virtual void FindWay(int Mx, int My, int Fx, int Fy);

	inline int calBlockTurn(int x, int y, int layer)
	{
		return 4 * (128 * (x + y) + x) + layer;
	}

};

