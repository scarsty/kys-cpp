#include "SubMap.h"
#include "MainMap.h"
#include "BattleMap.h"
#include "Event.h"

SubMap::SubMap()
{
    full_window_ = 1;
}


SubMap::SubMap(int num) : SubMap()
{
    setSceneNum(num);
    current_submap_ = Save::getInstance()->getSubMapRecord(num);
    current_submap_->ID = num;
    Cx = current_submap_->EntranceX;
    Cy = current_submap_->EntranceY;
}

SubMap::~SubMap()
{

}

void SubMap::draw()
{
    int k = 0;
    struct DrawInfo { int i; Point p; };
    //std::map<int, DrawInfo> map;
    for (int sum = -sumregion; sum <= sumregion + 15; sum++)
    {
        for (int i = -widthregion; i <= widthregion; i++)
        {
            int i1 = Cx + i + (sum / 2);
            int i2 = Cy - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, Cx, Cy);
            if (i1 >= 0 && i1 < max_coord_ && i2 >= 0 && i2 < max_coord_)
            {
                //Point p1 = Point(0, -m_SceneMapData[scene_id_].Data[4][i1][i2]);
                //Point p2 = Point(0, -m_SceneMapData[scene_id_].Data[5][i1][i2]);
                //地面
                int h = current_submap_->BuildingHeight(i1, i2);
                int num = current_submap_->Earth(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                //建筑和主角同一层
                num = current_submap_->Building(i1, i2) / 2;
                if (num > 0)
                {

                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h);
                }
                if (i1 == Cx && i2 == Cy)
                {
                    if (man_pic_ != -1)
                    {
                        man_pic_ = man_pic0_ + Scene::towards * num_man_pic_ + step;
                        TextureManager::getInstance()->renderTexture("smap", man_pic_, p.x, p.y - h);
                    }
                }
                //事件层
                auto event = current_submap_->Event(i1, i2);
                if (event)
                {
                    num = current_submap_->Event(i1, i2)->CurrentPic / 2;
                    //map[calBlockTurn(i1, i2, 2)] = s;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - current_submap_->EventHeight(i1, i2) - h);
                    }
                }

                //空中层
                num = current_submap_->Decoration(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
            }
            k++;
        }
    }
}

void SubMap::setPosition(int x, int y)
{
    Cx = x;
    Cy = y;
}

void SubMap::dealEvent(BP_Event& e)
{
    int x = Cx, y = Cy;
    //drawCount++;
    if (e.type == BP_MOUSEBUTTONDOWN)
    {
        getMousePosition(e.button.x, e.button.y);
        stopFindWay();
        if (canWalk(Msx, Msy) && !isOutScreen(Msx, Msy))
        {
            FindWay(Cx, Cy, Msx, Msy);
        }
    }
    if (!way_que_.empty())
    {
        Point newMyPoint = way_que_.top();
        x = newMyPoint.x;
        y = newMyPoint.y;
        isExit(x, y);
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
            if (isExit(x, y))
            {
                break;
            }
            walk(x, y, LeftDown);
            stopFindWay();
            break;
        }
        case BPK_RIGHT:
        {
            x++;
            if (isExit(x, y))
            {
                break;
            }
            walk(x, y, RightUp);
            stopFindWay();
            break;
        }
        case BPK_UP:
        {
            y--;
            if (isExit(x, y))
            {
                break;
            }
            walk(x, y, LeftUp);
            stopFindWay();
            break;
        }
        case BPK_DOWN:
        {
            y++;
            if (isExit(x, y))
            {
                break;
            }
            walk(x, y, RightDown);
            stopFindWay();
            break;
        }
        case BPK_ESCAPE:
        {
            stopFindWay();
            break;
        }
        case BPK_RETURN:
        case BPK_SPACE:
        {
            /* stopFindWay();
             ReSetEventPosition(x, y);
             if (current_submap_->Building();
             Save::getInstance()->m_SceneMapData[scene_id_].Data[3][x][y] >= 0)
                 if (Save::getInstance()->m_SceneEventData[scene_id_].Data[Save::getInstance()->m_SceneMapData[scene_id_].Data[3][x][y]].Event1 >= 0)
                     EventManager::getInstance()->callEvent(Save::getInstance()->m_SceneEventData[scene_id_].Data[Save::getInstance()->m_SceneMapData[scene_id_].Data[3][x][y]].Event1);
            */
            break;
        }
        case SDLK_b:
        {
            auto s = new BattleMap();
            push(s);
            break;
        }
        default:
        {
        }
        }
    }
}

void SubMap::backRun()
{
    for (int i = 0; i < SUBMAP_MAX_EVENT; i++)
    {
        auto e = current_submap_->Event(i);
        e->CurrentPic++;
        if (e->CurrentPic > e->EndPic)
        {
            e->CurrentPic = e->BeginPic;
        }
    }
}

void SubMap::walk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        Cx = x;
        Cy = y;
    }
    if (Scene::towards != t)
    {
        Scene::towards = t;
        step = 0;
    }
    else
    {
        step++;
    }
    step = step % num_man_pic_;
}

bool SubMap::canWalk(int x, int y)
{
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y)
        || isEvent(x, y) || isFall(x, y))
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool SubMap::isBuilding(int x, int y)
{
    if (current_submap_->Building(x, y) >= -2 && current_submap_->Building(x, y) <= 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool SubMap::isOutLine(int x, int y)
{
    if (x < 0 || x >= max_coord_ || y < 0 || y >= max_coord_)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool SubMap::isWater(int x, int y)
{
    int num = current_submap_->Earth(x, y) / 2;
    if (num >= 179 && num <= 181
        || num == 261 || num == 511
        || num >= 662 && num <= 665
        || num == 674)
    {
        return true;
    }
    return false;
}

bool SubMap::isEvent(int x, int y)
{
    auto e = current_submap_->Event(x, y);
    if (e && e->CannotWalk)
    {
        return true;
    }
    return false;
}

//what is this?
bool SubMap::isFall(int x, int y)
{
    //if (abs(Save::getInstance()->m_SceneMapData[scene_id_].Data[4][x][y] -
    //Save::getInstance()->m_SceneMapData[scene_id_].Data[4][Cx][Cy] > 10))
    //{
    //    true;
    //}
    return false;
}

bool SubMap::isExit(int x, int y)
{
    if (current_submap_->ExitX[0] == x && current_submap_->ExitY[0] == y
        || current_submap_->ExitX[1] == x && current_submap_->ExitY[1] == y
        || current_submap_->ExitX[2] == x && current_submap_->ExitY[2] == y)
    {
        loop_ = false;
        Save::getInstance()->global_data_.unused0 = 1;
        return true;
    }
    return false;
}

void SubMap::callEvent(int x, int y)
{

}

bool SubMap::isOutScreen(int x, int y)
{
    if (abs(Cx - x) >= 2 * widthregion || abs(Cy - y) >= sumregion)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void SubMap::getMousePosition(int _x, int _y)
{
    //int x = _x;
    //int y = _y;
    ////int yp = 0;
    //int yp = -(Save::getInstance()->m_SceneMapData[scene_id_].Data[4][x][y]);
    //Msx = (-(x - Center_X) / singleMapScene_X + (y + yp - Center_Y) / singleMapScene_Y) / 2 + Cx;
    //Msy = ((y + yp - Center_Y) / singleMapScene_Y + (x - Center_X) / singleMapScene_X) / 2 + Cy;
}

void SubMap::FindWay(int Mx, int My, int Fx, int Fy)
{
    bool visited[479][479] = { false };                                 //已访问标记(关闭列表)
    int dirs[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } };   //四个方向
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

void SubMap::stopFindWay()
{
    while (!way_que_.empty())
    {
        way_que_.pop();
    }
}

/*========================================
说明: 修正事件坐标
==========================================*/
void SubMap::ReSetEventPosition(int& x, int& y)
{
    switch (Scene::towards)
    {
    case Scene::LeftDown:
    {
        y--;
        break;
    }
    case Scene::RightUp:
    {
        y++;
        break;
    }
    case Scene::LeftUp:
    {
        x--;
        break;
    }
    case Scene::RightDown:
    {
        x++;
        break;
    }
    default:
        break;
    }
}