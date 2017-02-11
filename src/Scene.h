#pragma once
#include "Base.h"
#include "Texture.h"
#include "Point.h"
#include "Save.h"

class Scene :public Base
{
public:
    Scene();
    virtual ~Scene();
    virtual void draw() override {}
    virtual void dealEvent(BP_Event& e) override {}

    typedef enum
    {
        LeftUp = 0,
        RightUp = 1,
        LeftDown = 2,
        RightDown = 3,
    } Towards;

    static Towards towards;

    const int MaxMainMapCoord = 479;
    const int MaxSceneCoord = 63;

    const int Center_X = 1200 / 2;
    const int Center_Y = 600 / 2;
    const int singleScene_X = 18;								//小图块大小X
    const int singleScene_Y = 9;							//小图块大小Y
    const int singleMapScene_X = 18;							//地面小图块大小X
    const int singleMapScene_Y = 9;							//地面小图块大小Y

    int minStep;														//起点(Mx,My),终点(Fx,Fy),最少移动次数minStep
    int Msx, Msy;

    int widthregion = Center_X / 36 + 3;
    int sumregion = Center_Y / 9 + 2;

    bool isMenuOn = 0;

    Point getPositionOnScreen(int x, int y, int CenterX, int CenterY);
    Point getMapPoint(int x, int y, int CenterX, int CenterY);
	int CallFace(int x1, int y1, int x2, int y2);

};

class Compare
{
public:
	bool operator () (Point *point1, Point *point2)
	{
		return point1->lessthan(point2);
	}
};
