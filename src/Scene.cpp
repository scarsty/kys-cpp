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
    x = x - CenterX;
    y = y - CenterY;
    p.x = -y * singleScene_X + x * singleScene_X + screen_center_x_;
    p.y = y * singleScene_Y + x * singleScene_Y + screen_center_y_;

    return p;
}

Point Scene::getMapPoint(int x, int y, int CenterX, int CenterY)
{
    Point p;
    p.x = -(y - CenterY) * singleMapScene_X + (x - CenterX) * singleMapScene_X + screen_center_x_;
    p.y = ((y - CenterY) * singleMapScene_Y + (x - CenterX) * singleMapScene_Y + screen_center_y_);
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

