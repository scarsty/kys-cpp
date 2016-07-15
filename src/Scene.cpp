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

