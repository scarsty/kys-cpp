#include "Scene.h"

Scene::Towards Scene::towards;

Scene::Scene()
{
}


Scene::~Scene()
{
}

Point Scene::getPositionOnScreen(int x, int y, int CenterX, int CenterY)
{
    Point p;
    p.x = -(x - CenterX) * singleScene_X + (y - CenterY) * singleScene_X + Center_X;
    p.y = ((x - CenterX) * singleScene_Y + (y - CenterY) * singleScene_Y + Center_Y);
    return p;
}

Point Scene::getMapPoint(int x, int y, int CenterX, int CenterY)
{
    Point p;
    p.x = -(x - CenterX) * singleMapScene_X + (y - CenterY) * singleMapScene_X + Center_X;
    p.y = ((x - CenterX) * singleMapScene_Y + (y - CenterY) * singleMapScene_Y + Center_Y);
    return p;
}

int Scene::CallFace(int x1, int y1, int x2, int y2)
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
		else {
			if (d2 < 0) {
				return 2;
			}
			else
			{
				return 1;
			}
		}
	}
}

