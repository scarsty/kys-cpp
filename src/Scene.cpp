#include "Scene.h"
#include <queue>

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::calViewRegion()
{
    Engine::getInstance()->getMainTextureSize(render_center_x_, render_center_y_);
    render_center_x_ /= 2;
    render_center_y_ /= 2;
    view_width_region_ = render_center_x_ / TILE_W / 2 + 3;
    view_sum_region_ = render_center_y_ / TILE_H + 2;

    Engine::getInstance()->getPresentSize(window_center_x_, window_center_y_);
    window_center_x_ /= 2;
    window_center_y_ /= 2;
}

void Scene::checkWalk(int x, int y, BP_Event& e)
{
}

//后面两个参数是当前屏幕中心位置的游戏坐标，通常是人的坐标
Point Scene::getPositionOnRender(int x, int y, int view_x, int view_y)
{
    Point p;
    x = x - view_x;
    y = y - view_y;
    p.x = -y * TILE_W + x * TILE_W + render_center_x_;
    p.y = y * TILE_H + x * TILE_H + render_center_y_;
    return p;
}

//后面两个参数同上，一些情况下窗口尺寸和渲染尺寸不同
Point Scene::getPositionOnWindow(int x, int y, int view_x, int view_y)
{
    auto p = getPositionOnRender(x, y, view_x, view_y);
    p.x = p.x * window_center_x_ / render_center_x_;
    p.y = p.y * window_center_y_ / render_center_y_;
    return p;
}

//Point Scene::getMapPoint(int x, int y, int CenterX, int CenterY)
//{
//    Point p;
//    p.x = -(y - CenterY) * singleMapScene_X + (x - CenterX) * singleMapScene_X + screen_center_x_;
//    p.y = ((y - CenterY) * singleMapScene_Y + (x - CenterX) * singleMapScene_Y + screen_center_y_);
//    return p;
//}

//角色处于x1，y1，朝向x2，y2时，脸的方向
int Scene::calFace(int x1, int y1, int x2, int y2)
{
    int d1, d2, dm;
    d1 = y2 - y1;
    d2 = x2 - x1;
    dm = abs(d1) - abs(d2);
    if ((d1 != 0) || (d2 != 0))
    {
        if (dm >= 0)
        {
            if (d1 < 0)
            {
                return Towards_LeftUp;
            }
            else
            {
                return Towards_RightDown;
            }
        }
        else
        {
            if (d2 < 0)
            {
                return Towards_LeftDown;
            }
            else
            {
                return Towards_RightUp;
            }
        }
    }
    return Towards_None;
}

int Scene::getTowardsFromKey(BP_Keycode key)
{
    auto tw = Towards_None;
    switch (key)
    {
    case BPK_LEFT: tw = Towards_LeftDown; break;
    case BPK_RIGHT: tw = Towards_RightUp; break;
    case BPK_UP: tw = Towards_LeftUp; break;
    case BPK_DOWN: tw = Towards_RightDown; break;
    }
    return tw;
}

void Scene::getTowardsPosition(int x0, int y0, int tw, int* x1, int* y1)
{
    if (tw == Towards_None) { return; }
    *x1 = x0;
    *y1 = y0;
    switch (tw)
    {
    case Towards_LeftDown: (*x1)--; break;
    case Towards_RightUp: (*x1)++; break;
    case Towards_LeftUp: (*y1)--; break;
    case Towards_RightDown: (*y1)++; break;
    }
}

void Scene::getMousePosition(Point* point)
{
    int x = point->x;
    int y = render_center_y_ * 2 - point->y;
    //int yp = 0;
    //int yp = -(m_vcBattleSceneData[m_nbattleSceneNum].Data[1][x][y]);
    //mouse_x_ = (-x + screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBx;
    //mouse_y_ = (x - screen_center_x_ + 2 * (y + yp) - 2 * screen_center_y_ + 18) / 36 + m_nBy;
}

//A*
void Scene::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } };   //四个方向
    auto myPoint = new PointEx();
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = calFace(Mx, My, Fx, Fy);
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


