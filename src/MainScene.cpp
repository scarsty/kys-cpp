#include "MainScene.h"
#include <time.h>
#include "File.h"
#include "TextureManager.h"
#include "SubScene.h"
#include "Save.h"
#include "UI.h"
#include "Util.h"
#include "UISave.h"
#include "Random.h"

MainScene MainScene::main_scene_;

MainScene::MainScene()
{
    full_window_ = 1;
    COORD_COUNT = MAINMAP_COORD_COUNT;

    if (!data_readed_)
    {
        earth_layer_ = new MapSquareInt(COORD_COUNT);
        surface_layer_ = new MapSquareInt(COORD_COUNT);
        building_layer_ = new MapSquareInt(COORD_COUNT);
        build_x_layer_ = new MapSquareInt(COORD_COUNT);
        build_y_layer_ = new MapSquareInt(COORD_COUNT);

        int length = COORD_COUNT * COORD_COUNT * sizeof(MAP_INT);

        File::readFile("../game/resource/earth.002", &earth_layer_->data(0), length);
        File::readFile("../game/resource/surface.002", &surface_layer_->data(0), length);
        File::readFile("../game/resource/building.002", &building_layer_->data(0), length);
        File::readFile("../game/resource/buildx.002", &build_x_layer_->data(0), length);
        File::readFile("../game/resource/buildy.002", &build_y_layer_->data(0), length);

        divide2(earth_layer_);
        divide2(surface_layer_);
        divide2(building_layer_);
    }
    data_readed_ = true;

    //100个云
    for (int i = 0; i < 100; i++)
    {
        auto c = new Cloud();
        cloud_vector_.push_back(c);
        c->initRand();
    }
    //getEntrance();
}

MainScene::~MainScene()
{
    for (int i = 0; i < cloud_vector_.size(); i++)
    {
        delete cloud_vector_[i];
    }
    Util::safe_delete({ &earth_layer_, &surface_layer_, &building_layer_, &build_x_layer_, &build_y_layer_, &entrance_layer_ });
}

void MainScene::divide2(MapSquareInt* m)
{
    for (int i = 0; i < m->squareSize(); i++)
    {
        m->data(i) /= 2;
    }
}

void MainScene::draw()
{
    Engine::getInstance()->setRenderAssistTexture();
    //LOG("main\n");
    int k = 0;
    //auto t0 = Engine::getInstance()->getTicks();
    struct DrawInfo { int i; Point p; };
    std::map<int, DrawInfo> map;
    //TextureManager::getInstance()->renderTexture("mmap", 0, 0, 0);
#ifdef _DEBUG
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, -1, -1);
#endif
    //下面的15是下方较高贴图的余量，其余场景同
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            //auto p = getMapPoint(ix, iy, *_Mx, *_My);
            if (!isOutLine(ix, iy))
            {
                //共分3层，地面，表面，建筑，主角包括在建筑中
#ifndef _DEBUG
                //调试模式下不画出地面，图的数量太多占用CPU很大
                if (earth_layer_->data(ix, iy) > 0)
                {
                    TextureManager::getInstance()->renderTexture("mmap", earth_layer_->data(ix, iy), p.x, p.y);
                }
#endif
                if (surface_layer_->data(ix, iy) > 0)
                {
                    TextureManager::getInstance()->renderTexture("mmap", surface_layer_->data(ix, iy), p.x, p.y);
                }
                if (building_layer_->data(ix, iy) > 0)
                {
                    auto t = building_layer_->data(ix, iy);
                    //根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
                    //次要排序依据是y坐标
                    //直接设置z轴
                    auto tex = TextureManager::getInstance()->loadTexture("mmap", t);
                    auto w = tex->w;
                    auto h = tex->h;
                    auto dy = tex->dy;
                    int c = ((ix + iy) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + ix;
                    map[2 * c + 1] = { t, p };
                }
                if (ix == man_x_ && iy == man_y_)
                {
                    if (isWater(man_x_, man_y_))
                    {
                        man_pic_ = SHIP_PIC_0 + Scene::towards_ * SHIP_PIC_COUNT + step_;
                    }
                    else
                    {
                        man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;  //每个方向的第一张是静止图
                        if (rest_time_ >= BEGIN_REST_TIME)
                        {
                            man_pic_ = REST_PIC_0 + Scene::towards_ * REST_PIC_COUNT + (rest_time_ - BEGIN_REST_TIME) / REST_INTERVAL % REST_PIC_COUNT;
                        }
                    }
                    int c = 1024 * (ix + iy) + ix;
                    map[2 * c] = { man_pic_, p };
                }
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    {
        TextureManager::getInstance()->renderTexture("mmap", i->second.i, i->second.p.x, i->second.p.y);
    }

    //鼠标的位置，此处直接画到最上面了
    auto p = getMousePosition(man_x_, man_y_);
    p = getPositionOnRender(p.x, p.y, man_x_, man_y_);
    TextureManager::getInstance()->renderTexture("mmap", 1, p.x, p.y, { 255, 255, 255, 255 }, 128);

    for (auto& c : cloud_vector_)
    {
        c->draw();
    }

    //auto t1 = Engine::getInstance()->getTicks();

    //log("%d\n", t1 - t0);
    Engine::getInstance()->renderAssistTextureToWindow();
}

void MainScene::backRun()
{
    rest_time_++;    //只要出现走动，rest_time就会清零
    //云的贴图
    for (auto& c : cloud_vector_)
    {
        c->flow();
        c->setPositionOnScreen(man_x_, man_y_, render_center_x_, render_center_y_);
    }
}

void MainScene::dealEvent(BP_Event& e)
{
    //强制进入，通常用于开始
    if (force_submap_ >= 0)
    {
        auto sub_map = new SubScene(force_submap_);
        sub_map->setManViewPosition(force_submap_x_, force_submap_y_);
        sub_map->setTowards(towards_);
        sub_map->run();
        towards_ = sub_map->towards_;
        delete sub_map;
        force_submap_ = -1;
        setVisible(true);
    }

    int x = man_x_, y = man_y_;

    //键盘走路部分，检测4个方向键
    int pressed = 0;
    for (auto i = int(BPK_RIGHT); i <= int(BPK_UP); i++)
    {
        if (i != pre_pressed_ && Engine::getInstance()->checkKeyPress(i))
        {
            pressed = i;
        }
    }
    if (pressed == 0 && Engine::getInstance()->checkKeyPress(pre_pressed_))
    {
        pressed = pre_pressed_;
    }
    pre_pressed_ = pressed;

    if (pressed)
    {
        //注意，中间空出几个步数是为了可以单步行动，子场景同
        if (total_step_ < 1 || total_step_ >= first_step_delay_)
        {
            changeTowardsByKey(pressed);
            getTowardsPosition(man_x_, man_y_, towards_, &x, &y);
            tryWalk(x, y);
        }
        total_step_++;
    }
    else
    {
        total_step_ = 0;
    }

    if (pressed && checkEntrance(x, y))
    {
        way_que_.clear();
        total_step_ = 0;
    }

    if (!way_que_.empty())
    {
        Point p = way_que_.back();
        x = p.x;
        y = p.y;
        auto tw = calTowards(man_x_, man_y_, x, y);
        if (tw != Towards_None) { towards_ = tw; }
        tryWalk(x, y);
        way_que_.pop_back();
        if (way_que_.empty() && mouse_event_x_ >= 0 && mouse_event_y_ >= 0)
        {
            towards_ = calTowards(man_x_, man_y_, mouse_event_x_, mouse_event_y_);
            if (checkEntrance(mouse_event_x_, mouse_event_y_))
            {
                way_que_.clear();
                setMouseEventPoint(-1, -1);
            }
        }
    }

    //鼠标寻路
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        setMouseEventPoint(-1, -1);
        Point p = getMousePosition(e.button.x, e.button.y, x, y);
        stopFindWay();
        if (canWalk(p.x, p.y)/* && !isOutScreen(p.x, p.y)*/)
        {
            FindWay(x, y, p.x, p.y);
        }
        //如果是建筑，在此建筑的附近试图查找入口
        if (isBuilding(p.x, p.y))
        {
            int buiding_x = build_x_layer_->data(p.x, p.y);
            int buiding_y = build_y_layer_->data(p.x, p.y);
            bool found_entrance = false;
            for (int ix = buiding_x + 1; ix > buiding_x - 9; ix--)
            {
                for (int iy = buiding_y + 1; iy > buiding_y - 9; iy--)
                {
                    if (build_x_layer_->data(ix, iy) == buiding_x
                        && build_y_layer_->data(ix, iy) == buiding_y
                        && checkEntrance(ix, iy, true))
                    {
                        p.x = ix;
                        p.y = iy;    //p的值变化了
                        found_entrance = true;
                        break;
                    }
                }
                if (found_entrance) { break; }
            }
            if (found_entrance)
            {
                //在入口四周查找一个可以走到的地方
                std::vector<Point> ps;
                if (canWalk(p.x - 1, p.y)) { ps.push_back({ p.x - 1, p.y }); }
                if (canWalk(p.x + 1, p.y)) { ps.push_back({ p.x + 1, p.y }); }
                if (canWalk(p.x, p.y - 1)) { ps.push_back({ p.x, p.y - 1 }); }
                if (canWalk(p.x, p.y + 1)) { ps.push_back({ p.x, p.y + 1 }); }
                if (!ps.empty())
                {
                    int i = RandomClassical::rand(ps.size());
                    FindWay(x, y, ps[i].x, ps[i].y);
                    setMouseEventPoint(p.x, p.y);
                }
            }
        }
    }
}

void MainScene::onEntrance()
{
    calViewRegion();
    //if (force_submap_ >= 0)
    //{
    //    forceEnterSubScene(force_submap_, force_submap_x_, force_submap_y_);
    //}
}

void MainScene::onExit()
{
}

void MainScene::onPressedCancel()
{
    UI::getInstance()->run();
}

void MainScene::tryWalk(int x, int y)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
    }
    step_++;
    if (isWater(man_x_, man_y_))
    {
        step_ = step_ % SHIP_PIC_COUNT;
    }
    else
    {
        if (step_ >= MAN_PIC_COUNT)
        {
            step_ = 1;
        }
    }
    rest_time_ = 0;
}

bool MainScene::isBuilding(int x, int y)
{
    return (building_layer_->data(build_x_layer_->data(x, y), build_y_layer_->data(x, y)) > 0);
}

bool MainScene::isWater(int x, int y)
{
    auto pic = earth_layer_->data(x, y);
    if (pic == 419 || pic >= 306 && pic <= 335)
    {
        return true;
    }
    else if (pic >= 179 && pic <= 181
        || pic >= 253 && pic <= 335
        || pic >= 508 && pic <= 511)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool MainScene::canWalk(int x, int y)
{
    //if (checkEntrance(x, y, true))
    //{
    //    return true;
    //}  这里不需要加，实际上入口都是无法走到的
    if (isOutLine(x, y) || isBuilding(x, y)/*|| checkIsWater(x, y)*/)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool MainScene::checkEntrance(int x, int y, bool only_check /*= false*/)
{
    for (int i = 0; i < Save::getInstance()->getSubMapInfos().size(); i++)
    {
        auto s = Save::getInstance()->getSubMapInfo(i);
        if (x == s->MainEntranceX1 && y == s->MainEntranceY1 || x == s->MainEntranceX2 && y == s->MainEntranceY2)
        {
            bool can_enter = false;
            if (s->EntranceCondition == 0)
            {
                can_enter = true;
            }
            else if (s->EntranceCondition == 2)
            {
                //注意进入条件2的设定
                for (auto r : Save::getInstance()->Team)
                {
                    if (Save::getInstance()->getRole(r)->Speed >= 70)
                    {
                        can_enter = true;
                        break;
                    }
                }
            }
            if (only_check)
            {
                return true;
            }
            if (can_enter)
            {
                UISave::autoSave();
                //这里看起来要主动多画一帧，待修
                drawAndPresent();
                auto sub_map = new SubScene(i);
                sub_map->setManViewPosition(s->EntranceX, s->EntranceY);
                sub_map->run();
                towards_ = sub_map->towards_;
                delete sub_map;
                return true;
            }
        }
    }
    return false;
}

void MainScene::forceEnterSubScene(int submap_id, int x, int y)
{
    force_submap_ = submap_id;
    if (x >= 0) { force_submap_x_ = x; }
    if (y >= 0) { force_submap_y_ = y; }
    setVisible(false);
}

void MainScene::setEntrance()
{
}

bool MainScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 2 * view_width_region_ || abs(man_y_ - y) >= view_sum_region_);
}

