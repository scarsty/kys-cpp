#include "MainMap.h"
#include <time.h>
#include "File.h"
#include "TextureManager.h"
#include "SubMap.h"
#include "Save.h"
#include "UI.h"

MainMap::MapArray MainMap::Earth_, MainMap::Surface_, MainMap::Building_, MainMap::BuildX_, MainMap::BuildY_, MainMap::Entrance_;
bool MainMap::_readed = false;

MainMap::MainMap() :
    man_x_(Save::getInstance()->getGlobalData()->MainMapX),
    man_y_(Save::getInstance()->getGlobalData()->MainMapY)
{
    full_window_ = 1;

    srand(int(time(nullptr)));
    if (!_readed)
    {
        int length = max_coord_ * max_coord_ * sizeof(uint16_t);

        File::readFile("../game/resource/earth.002", &Earth_[0], length);
        File::readFile("../game/resource/surface.002", &Surface_[0], length);
        File::readFile("../game/resource/building.002", &Building_[0], length);
        File::readFile("../game/resource/buildx.002", &BuildX_[0], length);
        File::readFile("../game/resource/buildy.002", &BuildY_[0], length);

        divide2(Earth_);
        divide2(Surface_);
        divide2(Building_);
    }
    _readed = true;

    //100个云
    for (int i = 0; i < 100; i++)
    {
        auto c = new Cloud();
        cloud_vector_.push_back(c);
        c->initRand();
    }
    getEntrance();
}

MainMap::~MainMap()
{
    for (int i = 0; i < cloud_vector_.size(); i++)
    { delete cloud_vector_[i]; }
}

void MainMap::divide2(MapArray& m)
{
    for (int i = 0; i < max_coord_ * max_coord_; i++)
    {
        m[i] /= 2;
    }
}

void MainMap::draw()
{
    int k = 0;
    auto t0 = Engine::getInstance()->getTicks();
    struct DrawInfo { int i; Point p; };
    std::map<int, DrawInfo> map;
    //TextureManager::getInstance()->renderTexture("mmap", 0, 0, 0);
    for (int sum = -sum_region_; sum <= sum_region_ + 15; sum++)
    {
        for (int i = -width_region_; i <= width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            //auto p = getMapPoint(i1, i2, *_Mx, *_My);
            if (i1 >= 0 && i1 < max_coord_ && i2 >= 0 && i2 < max_coord_)
            {
                //共分3层，地面，表面，建筑，主角包括在建筑中
                if (Earth(i1, i2) > 0)
                { TextureManager::getInstance()->renderTexture("mmap", Earth(i1, i2), p.x, p.y); }
                if (Surface(i1, i2) > 0)
                { TextureManager::getInstance()->renderTexture("mmap", Surface(i1, i2), p.x, p.y); }
                if (Building(i1, i2) > 0)
                {
                    auto t = Building(i1, i2);
                    //根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
                    //次要排序依据是y坐标
                    //直接设置z轴
                    auto tex = TextureManager::getInstance()->loadTexture("mmap", t);
                    auto w = tex->w;
                    auto h = tex->h;
                    auto dy = tex->dy;
                    int c = ((i1 + i2) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + i1;
                    map[2 * c + 1] = { t, p };
                }
                if (i1 == man_x_ && i2 == man_y_)
                {
                    man_pic_ = man_pic0_ + Scene::towards * num_man_pic_ + step_;
                    if (rest_time_ >= begin_rest_time_)
                    { man_pic_ = rest_pic0_ + Scene::towards * num_rest_pic_ + (rest_time_ - begin_rest_time_) / rest_interval_ % num_rest_pic_; }
                    if (isWater(man_x_, man_y_))
                    {
                        man_pic_ = ship_pic0_ + Scene::towards * num_ship_pic_ + step_;
                    }
                    int c = 1024 * (i1 + i2) + i1;
                    map[2 * c] = { man_pic_, p };
                }
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    { TextureManager::getInstance()->renderTexture("mmap", i->second.i, i->second.p.x, i->second.p.y); }
    auto t1 = Engine::getInstance()->getTicks();
    //云的贴图
    for (auto& c : cloud_vector_)
    {
        c->draw();
        c->setPositionOnScreen(man_x_, man_y_, screen_center_x_, screen_center_y_);
    }
    //log("%d\n", t1 - t0);
}

//计时器，负责画图以及一些其他问题
void MainMap::dealEvent(BP_Event& e)
{
    int x = man_x_, y = man_y_;
    //drawCount++;
    if (e.type == BP_MOUSEBUTTONDOWN)
    {
        getMousePosition(e.button.x, e.button.y);
        stopFindWay();
        if (canWalk(Msx, Msy) && !isOutScreen(Msx, Msy))
        {
            FindWay(man_x_, man_y_, Msx, Msy);
        }
    }
    if (!way_que_.empty())
    {
        Point newMyPoint = way_que_.top();
        x = newMyPoint.x;
        y = newMyPoint.y;
        checkEntrance(x, y);
        Towards myTowards = (Towards)(newMyPoint.towards);
        //log("myTowards=%d", myTowards);
        walk(x, y, myTowards);
        way_que_.pop();
        //log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
    }
    if (e.type == BP_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
        case BPK_LEFT:
        {
            x--;
            checkEntrance(x, y);
            walk(x, y, LeftDown);
            stopFindWay();
            break;
        }
        case BPK_RIGHT:
        {
            x++;
            checkEntrance(x, y);
            walk(x, y, RightUp);
            stopFindWay();
            break;
        }
        case BPK_UP:
        {
            y--;
            checkEntrance(x, y);
            walk(x, y, LeftUp);
            stopFindWay();
            break;
        }
        case BPK_DOWN:
        {
            y++;
            checkEntrance(x, y);
            walk(x, y, RightDown);
            stopFindWay();
            break;
        }
        case BPK_ESCAPE:
        {
            stopFindWay();
            break;
        }
        case BPK_SPACE:
        {
            stopFindWay();
            //Save::getInstance()->m_BasicData[0].MFace = towards;
            //auto s = new BattleScene(Entrance[x][y]);
        }
        default:
        {
            rest_time_++;
        }
        }
    }
    if (e.type == BP_KEYUP)
    {
        if (e.key.keysym.sym == BPK_ESCAPE)
        {
            UI::getInstance()->run();
            e.type = BP_FIRSTEVENT;
        }
    }
}

void MainMap::entrance()
{

}

void MainMap::exit()
{
    Save::getInstance()->getGlobalData()->MainMapX = man_x_;
    Save::getInstance()->getGlobalData()->MainMapY = man_y_;
}

void MainMap::walk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
    }
    if (towards != t)
    {
        towards = t;
        //step = 0;
    }
    else
    { step_++; }
    step_ = step_ % num_man_pic_;
    if (isWater(man_x_, man_y_))
    {
        step_ = step_ % num_ship_pic_;
    }
    rest_time_ = 0;
}

bool MainMap::isBuilding(int x, int y)
{

    if (Building(BuildX(x, y), BuildY(x, y)) > 0)
    { return  true; }
    else
    { return false; }
}

bool MainMap::isWater(int x, int y)
{
    auto pic = Earth(x, y);
    if (pic == 419 || pic >= 306 && pic <= 335)
    { return true; }
    else if (pic >= 179 && pic <= 181
        || pic >= 253 && pic <= 335
        || pic >= 508 && pic <= 511)
    {
        return true;
    }
    else
    { return false; }
}

bool MainMap::isOutLine(int x, int y)
{
    if (x < 0 || x > max_coord_ || y < 0 || y > max_coord_)
    { return true; }
    else
    { return false; }
}

bool MainMap::canWalk(int x, int y)
{
    if (isBuilding(x, y) || isOutLine(x, y)/*|| checkIsWater(x, y)*/)
    { return false; }
    else
    { return true; }
}

bool MainMap::checkEntrance(int x, int y)
{
    for (int i = 0; i < Save::getInstance()->submap_records_.size(); i++)
    {
        auto s = Save::getInstance()->getSubMapRecord(i);
        if (x == s->MainEntranceX1 && y == s->MainEntranceY1 || x == s->MainEntranceX2 && y == s->MainEntranceY2)
        {
            auto sub_map = new SubMap(i);
            sub_map->run();
            return true;
        }
    }
    //if (Entrance[x][y] > 0 && Entrance[x][y] <= config::MAXScene)
    //{
    //    Save::getInstance()->m_BasicData[0].m_sMx = Mx;
    //    Save::getInstance()->m_BasicData[0].m_sMx = My;
    //    Save::getInstance()->m_BasicData[0].m_sMFace = towards;
    //    Save::getInstance()->m_BasicData[0].m_sWhere = 1;
    //    auto s = new SubScene(Entrance[x][y]);
    //    push(s);
    //    return true;
    //}
    return false;
}

void MainMap::getEntrance()
{
    //for (int x = 0; x < maxX; x++)
    //    for (int y = 0; y < maxY; y++)
    //    { Entrance[x][y] = -1; }
    //for (int i = 0; i < Save::getInstance()->m_SceneData.size(); i++)
    //{

    //    int x = Save::getInstance()->m_SceneData[i].MainEntranceX1;
    //    int y = Save::getInstance()->m_SceneData[i].MainEntranceY1;
    //    if (x > 0 && y > 0 && x < maxX && y < maxY)
    //    {
    //        Entrance[x][y] = i;
    //    }
    //    x = Save::getInstance()->m_SceneData[i].MainEntranceX2;
    //    y = Save::getInstance()->m_SceneData[i].MainEntranceY2;
    //    if (x > 0 && y > 0 && x < maxX && y < maxY)
    //    {
    //        Entrance[x][y] = i;
    //    }
    //}
}

//A*寻路
void MainMap::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } };      //四个方向
    Point* myPoint = new Point;
    myPoint->x = Mx;
    myPoint->y = My;
    myPoint->towards = (Point::Towards)CallFace(Mx, My, Fx, Fy);
    myPoint->parent = myPoint;
    myPoint->Heuristic(Fx, Fy);
    //log("Fx=%d,Fy=%d", Fx, Fy);
    //log("Mx=%d,My=%d", Mx, My);
    while (!way_que_.empty())
    {
        way_que_.pop();
    }
    std::priority_queue<Point*, std::vector<Point*>, Compare> que;            //最小优先级队列(开启列表)
    que.push(myPoint);
    int sNum = 0;
    while (!que.empty() && sNum <= 300)
    {
        Point* t = new Point();
        t = que.top();
        que.pop();
        visited[t->x][t->y] = true;
        sNum++;
        //log("t.x=%d,t.y=%d",t->x,t->y);
        if (t->x == Fx && t->y == Fy)
        {
            minStep = t->step;
            way_que_.push(*t);
            int k = 0;
            while (t != myPoint && k <= minStep)
            {
                //log("t.x=%d,t.y=%d,s.x=%d,s.y=%d,t.f=%d", t->x, t->y, t->parent->x, t->parent->y,t->f);

                t->towards = t->parent->towards;
                way_que_.push(*t);
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
                Point* s = new Point();
                s->x = t->x + dirs[i][0];
                s->y = t->y + dirs[i][1];
                if (canWalk(s->x, s->y) && !isOutScreen(s->x, s->y) && !visited[s->x][s->y])
                {
                    s->g = t->g + 10;
                    s->towards = (Point::Towards)i;
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

bool MainMap::isOutScreen(int x, int y)
{
    if (abs(man_x_ - x) >= 2 * width_region_ || abs(man_y_ - y) >= sum_region_)
    { return true; }
    else
    { return false; }
}


void MainMap::stopFindWay()
{
    while (!way_que_.empty())
    { way_que_.pop(); }
}

void MainMap::getMousePosition(int _x, int _y)
{
    int x = _x;
    int y = _y;
    int yp = 0;
    Msx = (-(x - screen_center_x_) / singleMapScene_X + (y + yp - screen_center_y_) / singleMapScene_Y) / 2 + man_x_;
    Msy = ((y + yp - screen_center_y_) / singleMapScene_Y + (x - screen_center_x_) / singleMapScene_X) / 2 + man_y_;
}


