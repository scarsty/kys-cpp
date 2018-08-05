#include "Console.h"
#include "File.h"
#include "MainScene.h"
#include "Random.h"
#include "Save.h"
#include "SubScene.h"
#include "TextureManager.h"
#include "UI.h"
#include "UISave.h"
#include "Util.h"
#include <time.h>
//#include "Timer.h"

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

    //100����
    for (int i = 0; i < 100; i++)
    {
        auto c = new Cloud();
        cloud_vector_.push_back(c);
        c->initRand();
    }
    //getEntrance();
    weather_ = new ParticleWeather();
    weather_->setRenderer(Engine::getInstance()->getRenderer());
    weather_->setTexture(TextureManager::getInstance()->loadTexture("title", 201)->getTexture());
    weather_->stopSystem();
    addChild(weather_);
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

    struct DrawInfo
    {
        int index;
        int i;
        Point p;
    };

    //Timer t1;
    //std::map<int, DrawInfo> map;
    static std::vector<DrawInfo> building_vec(10000);
    int building_count = 0;
    //TextureManager::getInstance()->renderTexture("mmap", 0, 0, 0);
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, -1, -1);
    //�����15���·��ϸ���ͼ�����������ೡ��ͬ
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
                //����3�㣬���棬���棬���������ǰ����ڽ�����
#ifndef _DEBUG
                //����ģʽ�²��������棬ͼ������̫��ռ��CPU�ܴ�
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
                    //����ͼƬ�Ŀ�ȼ���ͼ���е�, Ϊ�������С��, ʵ�����е������2��
                    //��Ҫ����������y����
                    //ֱ������z��
                    auto tex = TextureManager::getInstance()->loadTexture("mmap", t);
                    auto w = tex->w;
                    auto h = tex->h;
                    auto dy = tex->dy;
                    int c = ((ix + iy) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + ix;
                    //map[2 * c + 1] = { 2*c+1, t, p };
                    building_vec[building_count++] = { 2 * c + 1, t, p };
                }
                if (ix == man_x_ && iy == man_y_)
                {
                    if (isWater(man_x_, man_y_))
                    {
                        man_pic_ = SHIP_PIC_0 + Scene::towards_ * SHIP_PIC_COUNT + step_;
                    }
                    else
                    {
                        man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;    //ÿ������ĵ�һ���Ǿ�ֹͼ
                        if (rest_time_ >= BEGIN_REST_TIME)
                        {
                            man_pic_ = REST_PIC_0 + Scene::towards_ * REST_PIC_COUNT + (rest_time_ - BEGIN_REST_TIME) / REST_INTERVAL % REST_PIC_COUNT;
                        }
                    }
                    int c = 1024 * (ix + iy) + ix;
                    //map[2 * c] = {2*c, man_pic_, p };
                    building_vec[building_count++] = { 2 * c, man_pic_, p };
                }
            }
        }
    }
    //for (auto i = map.begin(); i != map.end(); i++)
    //{
    //    TextureManager::getInstance()->renderTexture("mmap", i->second.i, i->second.p.x, i->second.p.y);
    //}

    auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
    {
        return d1.index < d2.index;
    };
    std::sort(building_vec.begin(), building_vec.begin() + building_count, sort_building);
    for (int i = 0; i < building_count; i++)
    {
        auto& d = building_vec[i];
        TextureManager::getInstance()->renderTexture("mmap", d.i, d.p.x, d.p.y);
    }

    auto p = getPositionOnRender(cursor_x_, cursor_y_, man_x_, man_y_);
    TextureManager::getInstance()->renderTexture("mmap", 1, p.x, p.y, { 255, 255, 255, 255 }, 128);

    for (auto& c : cloud_vector_)
    {
        c->draw();
    }
    //printf("%d buildings in %g s.\n", building_count, t1.getElapsedTime());
    Engine::getInstance()->renderAssistTextureToWindow();
}

void MainScene::backRun()
{
    rest_time_++;    //ֻҪ�����߶���rest_time�ͻ�����
    //�Ƶ���ͼ
    view_cloud_ = 0;
    for (auto& c : cloud_vector_)
    {
        c->flow();
        c->setPositionOnScreen(man_x_, man_y_, render_center_x_, render_center_y_);
        int x, y;
        c->getPosition(x, y);
        if (x > -render_center_x_ * 1 && x < render_center_x_ * 3 && y > -0 && y < render_center_y_ * 2)
        {
            view_cloud_++;
        }
    }
    setWeather();
}

void MainScene::dealEvent(BP_Event& e)
{
    //ǿ�ƽ��룬ͨ�����ڿ�ʼ
    if (force_submap_ >= 0)
    {
        setVisible(true);
        auto sub_map = new SubScene(force_submap_);
        sub_map->setManViewPosition(force_submap_x_, force_submap_y_);
        sub_map->setTowards(towards_);
        sub_map->setForceBeginEvent(force_event_);
        sub_map->run();
        towards_ = sub_map->towards_;
        delete sub_map;
        force_submap_ = -1;
        force_event_ = -1;
    }

    int x = man_x_, y = man_y_;

    //������·���֣����4�������
    int pressed = 0;

    // Tab�������̨
    if (Engine::getInstance()->checkKeyPress(BPK_TAB)) {
        Console c;
    }

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
        //ע�⣬�м�ճ�����������Ϊ�˿��Ե����ж����ӳ���ͬ
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
        if (tw != Towards_None)
        {
            towards_ = tw;
        }
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

    calCursorPosition(man_x_, man_y_);

    //���Ѱ·
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        setMouseEventPoint(-1, -1);
        Point p = getMousePosition(e.button.x, e.button.y, x, y);
        way_que_.clear();
        if (canWalk(p.x, p.y) /* && !isOutScreen(p.x, p.y)*/)
        {
            FindWay(x, y, p.x, p.y);
        }
        //����ǽ������ڴ˽����ĸ�����ͼ�������
        if (isBuilding(p.x, p.y))
        {
            int buiding_x = build_x_layer_->data(p.x, p.y);
            int buiding_y = build_y_layer_->data(p.x, p.y);
            bool found_entrance = false;
            for (int ix = buiding_x + 1; ix > buiding_x - 9; ix--)
            {
                for (int iy = buiding_y + 1; iy > buiding_y - 9; iy--)
                {
                    if (build_x_layer_->data(ix, iy) == buiding_x && build_y_layer_->data(ix, iy) == buiding_y && checkEntrance(ix, iy, true))
                    {
                        p.x = ix;
                        p.y = iy;    //p��ֵ�仯��
                        found_entrance = true;
                        break;
                    }
                }
                if (found_entrance)
                {
                    break;
                }
            }
            if (found_entrance)
            {
                //��������ܲ���һ�������ߵ��ĵط�
                std::vector<Point> ps;
                if (canWalk(p.x - 1, p.y))
                {
                    ps.push_back({ p.x - 1, p.y });
                }
                if (canWalk(p.x + 1, p.y))
                {
                    ps.push_back({ p.x + 1, p.y });
                }
                if (canWalk(p.x, p.y - 1))
                {
                    ps.push_back({ p.x, p.y - 1 });
                }
                if (canWalk(p.x, p.y + 1))
                {
                    ps.push_back({ p.x, p.y + 1 });
                }
                if (!ps.empty())
                {
                    RandomDouble r;
                    int i = r.rand_int(ps.size());
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

//1 - can walk
//2 - cannot walk
int MainScene::isWater(int x, int y)
{
    auto pic = earth_layer_->data(x, y);
    if (pic == 419 || pic >= 306 && pic <= 335)
    {
        return 2;
    }
    else if (pic >= 179 && pic <= 181 || pic >= 253 && pic <= 335 || pic >= 508 && pic <= 511)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

bool MainScene::canWalk(int x, int y)
{
    //���ﲻ��Ҫ�ӣ�ʵ������ڶ����޷��ߵ���
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) == 2)
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
                //ע���������2���趨
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
                //���￴����Ҫ�����໭һ֡������
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

void MainScene::forceEnterSubScene(int submap_id, int x, int y, int event)
{
    force_submap_ = submap_id;
    if (x >= 0)
    {
        force_submap_x_ = x;
    }
    if (y >= 0)
    {
        force_submap_y_ = y;
    }
    force_event_ = event;
    setVisible(false);
}

void MainScene::setWeather()
{
    weather_->setPosition(Engine::getInstance()->getWindowWidth() / 2, 0);
    if (inNorth())
    {
        weather_->setStyle(ParticleExample::SNOW);
        if (!weather_->isActive())
        {
            weather_->resetSystem();
        }
    }
    else
    {
        if (view_cloud_)
        {
            weather_->setStyle(ParticleExample::RAIN);
            weather_->setEmissionRate(50 * view_cloud_);
            weather_->setGravity({ 10, 20 });
            if (!weather_->isActive())
            {
                weather_->resetSystem();
            }
        }
        else
        {
            if (weather_->isActive())
            {
                weather_->stopSystem();
            }
        }
    }
}

void MainScene::setEntrance()
{
}

bool MainScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 2 * view_width_region_ || abs(man_y_ - y) >= view_sum_region_);
}
