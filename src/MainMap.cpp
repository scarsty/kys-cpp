#include "MainMap.h"
#include <time.h>
#include "File.h"
#include "TextureManager.h"
#include "SubMap.h"
#include "Save.h"
#include "UI.h"

MainMap::MapArray MainMap::Earth_, MainMap::Surface_, MainMap::Building_, MainMap::BuildX_, MainMap::BuildY_, MainMap::Entrance_;
bool MainMap::data_readed_ = false;

MainMap::MainMap() :
    man_x_(Save::getInstance()->MainMapX),
    man_y_(Save::getInstance()->MainMapY)
{
    full_window_ = 1;

    srand(int(time(nullptr)));
    if (!data_readed_)
    {
        int length = MAX_COORD * MAX_COORD * sizeof(uint16_t);

        File::readFile("../game/resource/earth.002", &Earth_[0], length);
        File::readFile("../game/resource/surface.002", &Surface_[0], length);
        File::readFile("../game/resource/building.002", &Building_[0], length);
        File::readFile("../game/resource/buildx.002", &BuildX_[0], length);
        File::readFile("../game/resource/buildy.002", &BuildY_[0], length);

        divide2(Earth_);
        divide2(Surface_);
        divide2(Building_);
    }
    data_readed_ = true;

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
    for (int i = 0; i < MAX_COORD * MAX_COORD; i++)
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
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            //auto p = getMapPoint(i1, i2, *_Mx, *_My);
            if (i1 >= 0 && i1 < MAX_COORD && i2 >= 0 && i2 < MAX_COORD)
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
                    man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;
                    if (rest_time_ >= BEGIN_REST_TIME)
                    { man_pic_ = REST_PIC_0 + Scene::towards_ * REST_PIC_COUNT + (rest_time_ - BEGIN_REST_TIME) / REST_INTERVAL % REST_PIC_COUNT; }
                    if (isWater(man_x_, man_y_))
                    {
                        man_pic_ = SHIP_PIC_0 + Scene::towards_ * SHIP_PIC_COUNT + step_;
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
    if (e.type == BP_MOUSEBUTTONUP)
    {
        getMousePosition(e.button.x, e.button.y);
        stopFindWay();
        if (canWalk(mouse_x_, mouse_y_) && !isOutScreen(mouse_x_, mouse_y_))
        {
            FindWay(man_x_, man_y_, mouse_x_, mouse_y_);
        }
    }
    if (!way_que_.empty())
    {
        PointEx newMyPoint = way_que_.back();
        x = newMyPoint.x;
        y = newMyPoint.y;
        checkEntrance(x, y);
        Towards myTowards = (Towards)(newMyPoint.towards);
        //log("myTowards=%d", myTowards);
        tryWalk(x, y, myTowards);
        way_que_.pop_back();
        //log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
    }
    if (e.type == BP_KEYDOWN)
    {
        //这里应该是不正确，一旦有按键，朝向应立刻改变
        Towards tw;
        switch (e.key.keysym.sym)
        {
        case BPK_LEFT:
        case BPK_RIGHT:
        case BPK_UP:
        case BPK_DOWN:
            getTowardsFromKey(e.key.keysym.sym);
            getTowardsPosition(man_x_, man_y_, &x, &y);
            //walk(x, y, towards_);
            break;
        //case BPK_ESCAPE:
        //{
        //    stopFindWay();
        //    break;
        //}
        //case BPK_SPACE:
        //{
        //    stopFindWay();
        //    //Save::getInstance()->m_BasicData[0].MFace = towards;
        //    //auto s = new BattleScene(Entrance[x][y]);
        //}
        default:
            rest_time_++;
        }
        if (checkEntrance(x, y))
        {
            clearEvent(e);
        }
        else
        {
            tryWalk(x, y, tw);
            stopFindWay();
        }
    }
    if (e.type == BP_KEYUP)
    {
        if (e.key.keysym.sym == BPK_ESCAPE)
        {
            UI::getInstance()->run();
            clearEvent(e);
        }
    }
}

void MainMap::entrance()
{

}

void MainMap::exit()
{
}

void MainMap::tryWalk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
    }
    if (towards_ != t)
    {
        towards_ = t;
        //step = 0;
    }
    else
    { step_++; }
    step_ = step_ % MAN_PIC_COUNT;
    if (isWater(man_x_, man_y_))
    {
        step_ = step_ % SHIP_PIC_COUNT;
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
    return (x < 0 || x > MAX_COORD || y < 0 || y > MAX_COORD);
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

bool MainMap::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 2 * view_width_region_ || abs(man_y_ - y) >= view_sum_region_);
}

void MainMap::getMousePosition(int _x, int _y)
{
    int x = _x;
    int y = _y;
    int yp = 0;
    mouse_x_ = (-(x - screen_center_x_) / mainmap_tile_x_ + (y + yp - screen_center_y_) / mainmap_tile_y_) / 2 + man_x_;
    mouse_y_ = ((y + yp - screen_center_y_) / mainmap_tile_y_ + (x - screen_center_x_) / mainmap_tile_x_) / 2 + man_y_;
}


