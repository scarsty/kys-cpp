#include "Scene.h"
#include <queue>
#include "GameUtil.h"

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

    //Engine::getInstance()->getPresentSize(window_center_x_, window_center_y_);
    //window_center_x_ /= 2;
    //window_center_y_ /= 2;
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
    int w, h;
    Engine::getInstance()->getPresentSize(w, h);
    p.x = p.x * w / render_center_x_ / 2;
    p.y = p.y * h / render_center_y_ / 2;
    return p;
}

//角色处于x1，y1，朝向x2，y2时，脸的方向
int Scene::calTowards(int x1, int y1, int x2, int y2)
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
                return Towards_RightUp;
            }
            else
            {
                return Towards_LeftDown;
            }
        }
        else
        {
            if (d2 < 0)
            {
                return Towards_LeftUp;
            }
            else
            {
                return Towards_RightDown;
            }
        }
    }
    return Towards_None;
}

void Scene::changeTowardsByKey(BP_Keycode key)
{
    int tw = getTowardsByKey(key);
    if (tw != Towards_None) { towards_ = tw; }
}

int Scene::getTowardsByKey(BP_Keycode key)
{
    int tw = Towards_None;
    switch (key)
    {
    case BPK_LEFT: tw = Towards_LeftUp; break;
    case BPK_RIGHT: tw = Towards_RightDown; break;
    case BPK_UP: tw = Towards_RightUp; break;
    case BPK_DOWN: tw = Towards_LeftDown; break;
    }
    return tw;
}

int Scene::getTowardsByMouse(int mouse_x, int mouse_y)
{
    int w, h;
    Engine::getInstance()->getPresentSize(w, h);
    mouse_x = mouse_x * render_center_x_ * 2 / w;
    mouse_y = mouse_y * render_center_y_ * 2 / h;
    if (mouse_x < render_center_x_ && mouse_y < render_center_y_)
    {
        return Towards_LeftUp;
    }
    if (mouse_x < render_center_x_ && mouse_y > render_center_y_)
    {
        return Towards_LeftDown;
    }
    if (mouse_x > render_center_x_ && mouse_y < render_center_y_)
    {
        return Towards_RightUp;
    }
    if (mouse_x > render_center_x_ && mouse_y > render_center_y_)
    {
        return Towards_RightDown;
    }
    return Towards_None;
}

void Scene::getTowardsPosition(int x0, int y0, int tw, int* x1, int* y1)
{
    if (tw == Towards_None) { return; }
    *x1 = x0;
    *y1 = y0;
    switch (tw)
    {
    case Towards_LeftUp: (*x1)--; break;
    case Towards_RightDown: (*x1)++; break;
    case Towards_RightUp: (*y1)--; break;
    case Towards_LeftDown: (*y1)++; break;
    }
}

//从鼠标的位置反推出在游戏地图上的坐标
Point Scene::getMousePosition(int mouse_x, int mouse_y, int view_x, int view_y)
{
    int w, h;
    Engine::getInstance()->getPresentSize(w, h);
    double mouse_x1 = mouse_x * render_center_x_ * 2.0 / w;
    double mouse_y1 = mouse_y * render_center_y_ * 2.0 / h;

    //mouse_x1 += TILE_W;
    mouse_y1 += TILE_H * 2;

    Point p;
    p.x = ((mouse_x1 - render_center_x_) / TILE_W + (mouse_y1 - render_center_y_) / TILE_H) / 2 + view_x;
    p.y = ((-mouse_x1 + render_center_x_) / TILE_W + (mouse_y1 - render_center_y_) / TILE_H) / 2 + view_y;
    return p;
}

Point Scene::getMousePosition(int view_x, int view_y)
{
    int mouse_x, mouse_y;
    Engine::getInstance()->getMouseState(mouse_x, mouse_y);
    return getMousePosition(mouse_x, mouse_y, view_x, view_y);
}

//A*
void Scene::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, 0 } };   //四个方向
    auto myPoint = new PointEx();
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = calTowards(Mx, My, Fx, Fy);
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
    while (!que.empty() && sNum <= 512)
    {
        auto t = new PointEx();
        t = que.top();
        que.pop();
        visited[t->x][t->y] = true;
        sNum++;
        //log("t.x=%d,t.y=%d",t->x,t->y);
        if (t->x == Fx && t->y == Fy)
        {
            min_step_ = t->step;
            way_que_.push_back(*t);
            int k = 0;
            while (t != myPoint && k <= min_step_)
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
                    s->towards = i;
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

void Scene::lightScene()
{
    for (int i = 10; i <= 0; i--)
    {
        auto fill = [&](void*)->void
        {
            uint8_t alpha = GameUtil::limit(i * 25, 0, 255);
            Engine::getInstance()->fillColor({ 0, 0, 0, alpha }, 0, 0, -1, -1);
        };
        drawAndPresent(1, fill);
    }
}

void Scene::darkScene()
{
    for (int i = 0; i <= 10; i++)
    {
        auto fill = [&](void*)->void
        {
            uint8_t alpha = GameUtil::limit(i * 25, 0, 255);
            Engine::getInstance()->fillColor({ 0, 0, 0, alpha }, 0, 0, -1, -1);
        };
        drawAndPresent(1, fill);
    }
}

bool Scene::isOutLine(int x, int y)
{
    return (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT);
}

