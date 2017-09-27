#include "Scene.h"
#include <queue>

Towards Scene::towards_;

Scene::Scene()
{
    Engine::getInstance()->getPresentSize(screen_center_x_, screen_center_y_);
    screen_center_x_ /= 2;
    screen_center_y_ /= 2;
    view_width_region_ = screen_center_x_ / SUBMAP_TILE_W / 2 + 3;
    view_sum_region_ = screen_center_y_ / SUBMAP_TILE_H + 2;
}


Scene::~Scene()
{
}

Point Scene::getPositionOnScreen(int x, int y, int CenterX, int CenterY)
{
    Point p;
    x = x - CenterX;
    y = y - CenterY;
    p.x = -y * SUBMAP_TILE_W + x * SUBMAP_TILE_W + screen_center_x_;
    p.y = y * SUBMAP_TILE_H + x * SUBMAP_TILE_H + screen_center_y_;
    return p;
}

//Point Scene::getMapPoint(int x, int y, int CenterX, int CenterY)
//{
//    Point p;
//    p.x = -(y - CenterY) * singleMapScene_X + (x - CenterX) * singleMapScene_X + screen_center_x_;
//    p.y = ((y - CenterY) * singleMapScene_Y + (x - CenterX) * singleMapScene_Y + screen_center_y_);
//    return p;
//}

Towards Scene::CallFace(int x1, int y1, int x2, int y2)
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
                return LeftUp;
            }
            else
            {
                return RightDown;
            }
        }
        else
        {
            if (d2 < 0)
            {
                return LeftDown;
            }
            else
            {
                return RightUp;
            }
        }
    }
}

//A*
void Scene::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 },{ 0, 1 },{ 0, -1 },{ 1, 0 } };      //四个方向
    auto myPoint = new PointEx();
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = CallFace(Mx, My, Fx, Fy);
    myPoint->parent = myPoint;
    myPoint->Heuristic(Fx, Fy);
    //log("Fx=%d,Fy=%d", Fx, Fy);
    //log("Mx=%d,My=%d", Mx, My);
    while (!way_que_.empty())
    {
        way_que_.pop_back();
    }
    std::priority_queue<PointEx*, std::vector<PointEx*>, Compare> que;            //最小优先级队列(开启列表)
    que.push(myPoint);
    int sNum = 0;
    while (!que.empty() && sNum <= 300)
    {
        auto t = new PointEx();
        t = que.top();
        que.pop();
        visited[t->x][t->y] = true;
        sNum++;
        //log("t.x=%d,t.y=%d",t->x,t->y);
        if (t->x == Fx && t->y == Fy)
        {
            minStep = t->step;
            way_que_.push_back(*t);
            int k = 0;
            while (t != myPoint && k <= minStep)
            {
                //log("t.x=%d,t.y=%d,s.x=%d,s.y=%d,t.f=%d", t->x, t->y, t->parent->x, t->parent->y,t->f);

                t->towards = t->parent->towards;
                way_que_.push_back(*t);
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
                auto s = new PointEx();
                s->x = t->x + dirs[i][0];
                s->y = t->y + dirs[i][1];
                if (canWalk(s->x, s->y) && !isOutScreen(s->x, s->y) && !visited[s->x][s->y])
                {
                    s->g = t->g + 10;
                    s->towards = (Towards)i;
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

