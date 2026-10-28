#include "SubScene.h"
#include "Audio.h"
#include "BattleScene.h"
#include "Console.h"
#include "DayNightSystem.h"
#include "Event.h"
#include "Fade.h"
#include "Font.h"
#include "MainScene.h"
#include "ParticleExample.h"
#include "PotConv.h"
#include "Random.h"
#include "Timer.h"
#include "UI.h"
#include "Weather.h"
#include <format>
#include <cmath>
#include <algorithm>

SubScene::SubScene()
{
    full_window_ = 1;
    COORD_COUNT = SUBMAP_COORD_COUNT;
    // 内景不画云。
    //cloud_group_ = std::make_shared<CloudGroup>();
    //cloud_group_->init(2, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    //addChild(cloud_group_);
}

SubScene::SubScene(int id) :
    SubScene()
{
    setID(id);
}

SubScene::~SubScene()
{
}

void SubScene::setID(int id)
{
    submap_id_ = id;
    submap_info_ = Save::getInstance()->getSubMapInfo(submap_id_);
    if (submap_info_ == nullptr)
    {
        setExit(true);
        return;
    }
    //submap_info_->ID = submap_id_;    //这句是修正存档中可能存在的错误
    exit_music_ = submap_info_->ExitMusic;
    Audio::getInstance()->playMusic(submap_info_->EntranceMusic);
    LOG("Sub Scene {}, {}\n", submap_id_, submap_info_->Name);
}

void SubScene::draw()
{
    auto day_night = DayNightSystem::getInstance();
    Color ambient_color = day_night->getLighting().ambient_color;
    //int k = 0;
    struct DrawInfo
    {
        int i;
        Point p;
    };

    //std::map<int, DrawInfo> map;

    Engine::getInstance()->setRenderTarget("scene");
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    int cull_x = view_x_, cull_y = view_y_;
    if (walking_)
    {
        cull_x += int(std::lround((walk_to_.x - view_x_) * walk_progress_));
        cull_y += int(std::lround((walk_to_.y - view_y_) * walk_progress_));
    }
    //Timer t0;
    //一整块地面
    //如果没有创建地面，那么这个值是空，即使用之前的画法
    auto earth_texture = Engine::getInstance()->getTexture("searth");
    if (earth_texture)
    {
        auto p = getPositionOnWholeEarth(view_x_, view_y_);
        if (walking_)
        {
            auto offset = getWalkOffset();
            p.x -= offset.x;
            p.y -= offset.y;
        }
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        Rect rect0 = { p.x - render_center_x_, p.y - render_center_y_, w, h }, rect1 = { 0, 0, w, h };
        int left = std::max(0, rect0.x), top = std::max(0, rect0.y);
        int right = std::min(earth_texture->w, rect0.x + w);
        int bottom = std::min(earth_texture->h, rect0.y + h);
        rect1 = { left - rect0.x, top - rect0.y, std::max(0, right - left), std::max(0, bottom - top) };
        rect0 = { left, top, rect1.w, rect1.h };
        std::vector<Color> cv(4, ambient_color);
        if (rect0.w > 0 && rect0.h > 0) { Engine::getInstance()->renderTextureLight(earth_texture, &rect0, &rect1, cv, { 0.25f, 0, 0, 0 }); }
    }
    else
    {
        //#ifndef _DEBUG
        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = cull_x + i + (sum / 2);
                int iy = cull_y - i + (sum - sum / 2);
                auto p = getWalkingPositionOnRender(ix, iy, view_x_, view_y_);
                p.x += x_;
                p.y += y_;
                if (!isOutLine(ix, iy))
                {
                    int h = submap_info_->BuildingHeight(ix, iy);
                    int num = submap_info_->Earth(ix, iy) / 2;
                    //低高度地面
                    if (num > 0 && h <= 2)
                    {
                        if (earth_texture == nullptr)
                        {
                            TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, { ambient_color });
                        }
                        else
                        {
                            auto tex = TextureManager::getInstance()->getTexture("smap", num);
                            //用大图画时的闪烁地面
                            if (tex->count > 1)
                            {
                                TextureManager::getInstance()->renderTexture(tex, p.x, p.y, { ambient_color });
                            }
                        }
                    }
                }
            }
        }
        //#endif
    }
    std::vector<Color> color_v(4, ambient_color);
    std::vector<float> brighness_v = { 0.5f, 0, 0, 0 };
    //color_v[0] = { 128, 128, 128, 255 };
    //color_v[1] = { 192, 192, 192, 255 };
    //color_v[3] = { 192, 192, 192, 255 };
    double visual_x = man_x_ + (walking_ ? (walk_to_.x - man_x_) * walk_progress_ : 0);
    double visual_y = man_y_ + (walking_ ? (walk_to_.y - man_y_) * walk_progress_ : 0);
    double man_depth = (visual_x + visual_y) * 1024 + visual_x;
    bool man_drawn = false;
    auto draw_man = [&]()
    {
        auto p = getWalkingPositionOnRender(man_x_, man_y_, view_x_, view_y_);
        double height = submap_info_->BuildingHeight(man_x_, man_y_);
        if (walking_)
        {
            auto offset = getWalkingManOffset();
            p.x += offset.x;
            p.y += offset.y;
            height += (submap_info_->BuildingHeight(walk_to_.x, walk_to_.y) - height) * walk_progress_;
        }
        man_pic_ = force_man_pic_ == -1 ? calManPic() : force_man_pic_;
        TextureManager::getInstance()->renderTexture("smap", man_pic_, p.x + x_, p.y + y_ - int(std::lround(height * (TILE_W / 18))),
            { { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brighness_v });
    };
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 20; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = cull_x + i + (sum / 2);
            int iy = cull_y - i + (sum - sum / 2);
            auto p = getWalkingPositionOnRender(ix, iy, view_x_, view_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                //有高度地面
                //应该是先画，但是也不太对，暂时用大地面图时不画
                int rate = TILE_W / 18;                           //高清模式要乘以倍率
                int h0 = submap_info_->BuildingHeight(ix, iy);    //高清模式要乘以倍率
                int h = h0 * rate;
                int num = submap_info_->Earth(ix, iy) / 2;
                if (num > 0 && (!earth_texture || (earth_texture && h0 > 2 && rate == 1)))    //判断有点复杂，暂时先这样，高清模式下有高度的地面才画小图
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, { ambient_color });
                }
                //鼠标位置
                if (ix == cursor_x_ && iy == cursor_y_)
                {
                    TextureManager::getInstance()->renderTexture("mmap", 1, p.x, p.y - h, { { 255, 255, 255, 255 }, 128 });
                }
                double tile_depth = (ix + iy) * 1024 + ix;
                if (!man_drawn && tile_depth > man_depth)
                {
                    draw_man();
                    man_drawn = true;
                }
                //建筑和主角
                num = submap_info_->Building(ix, iy) / 2;
                if (num > 0)
                {
                    std::vector<float> building_brightness = Scene::isWallTile(num) ? std::vector<float>{} : brighness_v;
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h, { { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, building_brightness });
                }
                if (!man_drawn && tile_depth >= man_depth)
                {
                    draw_man();
                    man_drawn = true;
                }
                //事件
                auto event = submap_info_->Event(ix, iy);
                if (event)
                {
                    num = submap_info_->Event(ix, iy)->CurrentPic / 2;
                    //map[calBlockTurn(i1, i2, 2)] = s;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h, { { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brighness_v });
                    }
                }
                //装饰
                num = submap_info_->Decoration(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - submap_info_->DecorationHeight(ix, iy) * rate, { { 255, 255, 255, 255 }, 255, 1, 1, 0, 0, color_v, brighness_v });
                }
            }
            //k++;
        }
    }
    if (!man_drawn) { draw_man(); }
    Engine::getInstance()->renderTextureToMain("scene");
    //LOG("%g\n", t0.getElapsedTime());
}

void SubScene::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    updateWalking();
    if (!walking_ && (!walkingDuration() || arrival_pending_))
    {
        arrival_pending_ = false;
        if (checkArrival() && walkingDuration()) { return; }
    }
    if (!walking_ && way_que_.empty() && mouse_event_x_ >= 0)
    {
        auto target = Point{ mouse_event_x_, mouse_event_y_ };
        setMouseEventPoint(-1, -1);
        if (calDistance(man_x_, man_y_, target.x, target.y) == 1)
        {
            towards_ = calTowards(man_x_, man_y_, target.x, target.y);
            checkEvent1(man_x_, man_y_, towards_);
        }
        walk_next_started_ = -1;
    }
    int x = man_x_, y = man_y_;
    // Tab激活控制台
    if (e.type == EVENT_KEY_UP && e.key.key == K_TAB
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_BACK))
    {
        //Event::getInstance()->tryBattle(1, 0);
        stopWalking();
        Console c;
    }
    if ((e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_RIGHT)
        //|| (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_START)
        || engine->gameControllerGetButton(GAMEPAD_BUTTON_START))
    {
        stopWalking();
        UI::getInstance()->run();
        auto item = UI::getInstance()->getUsedItem();
        if (item && item->ItemType == 0)
        {
            if (checkEvent2(man_x_, man_y_, towards_, item->ID))
            {
                step_ = 0;
            }
        }
    }

    //键盘走路部分，检测4个方向键
    if (!walking_ && (walkingDuration() || engine->getTicks() - pre_pressed_ticks_ > key_walk_delay_))
    {
        int64_t pressed = 0;
        pre_pressed_ticks_ = engine->getTicks();
        auto axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTX);
        auto axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTY);
        if (abs(axis_x) < 10000) { axis_x = 0; }
        if (abs(axis_y) < 10000) { axis_y = 0; }
        if (axis_x != 0 || axis_y != 0)
        {
            Pointf axis{ float(axis_x), float(axis_y) };
            auto to = realTowardsToFaceTowards(axis);
            if (to == Towards_LeftUp) { pressed = K_LEFT; }
            if (to == Towards_LeftDown) { pressed = K_DOWN; }
            if (to == Towards_RightDown) { pressed = K_RIGHT; }
            if (to == Towards_RightUp) { pressed = K_UP; }
        }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_LEFT)) { pressed = K_LEFT; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_DOWN)) { pressed = K_DOWN; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_RIGHT)) { pressed = K_RIGHT; }
        if (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_UP)) { pressed = K_UP; }
        if (engine->checkKeyPress(K_A)) { pressed = K_LEFT; }
        if (engine->checkKeyPress(K_S)) { pressed = K_DOWN; }
        if (engine->checkKeyPress(K_D)) { pressed = K_RIGHT; }
        if (engine->checkKeyPress(K_W)) { pressed = K_UP; }

        for (auto i = int(K_RIGHT); i <= int(K_UP); i++)
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
            way_que_.clear();
            setMouseEventPoint(-1, -1);
            if (walkingDuration() || total_step_ < 1 || total_step_ >= first_step_delay_)
            {
                changeTowardsByKey(pressed);
                getTowardsPosition(man_x_, man_y_, towards_, &x, &y);
                tryWalk(x, y);
            }
            total_step_++;
        }
        else
        {
            if (rest_time_ > 2) { total_step_ = 0; }
        }
        if (!way_que_.empty())
        {
            Point p = way_que_.back();
            x = p.x;
            y = p.y;
            if (calDistance(man_x_, man_y_, x, y) <= 1)
            {
                auto tw = calTowards(man_x_, man_y_, x, y);
                if (tw != Towards_None)
                {
                    towards_ = tw;
                }
                tryWalk(x, y);
                way_que_.pop_back();
                if (!walking_ && way_que_.empty() && mouse_event_x_ >= 0 && mouse_event_y_ >= 0)
                {
                    auto tw = calTowards(man_x_, man_y_, mouse_event_x_, mouse_event_y_);
                    if (tw != Towards_None)
                    {
                        towards_ = tw;
                    }
                    checkEvent1(man_x_, man_y_, towards_);
                    setMouseEventPoint(-1, -1);
                }
            }
            else
            {
                way_que_.clear();
            }
            //if (isExit(x, y)) { way_que_.clear(); }
        }
    }
    //检查触发剧情事件
    if ((e.type == EVENT_KEY_UP && (e.key.key == K_RETURN || e.key.key == K_SPACE))
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_SOUTH))
    {
        stopWalking();
        if (checkEvent1(man_x_, man_y_, towards_))
        {
            //clearEvent(e);
            step_ = 0;
        }
    }

    int cursor_mx, cursor_my;
    engine->getMouseStateInStartWindow(cursor_mx, cursor_my);
    auto cursor = getWalkingMousePosition(cursor_mx, cursor_my, view_x_, view_y_);
    cursor_x_ = cursor.x;
    cursor_y_ = cursor.y;

    //鼠标寻路
    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
    {
        setMouseEventPoint(-1, -1);
        int mx, my;
        Engine::getInstance()->getMouseStateInStartWindow(mx, my);
        Point p = getWalkingMousePosition(mx, my, view_x_, view_y_);
        x = walking_ ? walk_to_.x : man_x_;
        y = walking_ ? walk_to_.y : man_y_;
        way_que_.clear();
        if (isCannotPassEvent(p.x, p.y))    //存在事件点则仅会走到倒数第二格
        {
            FindWay(x, y, p.x, p.y);
            if (way_que_.size() >= 1)
            {
                way_que_ = std::vector<Point>(way_que_.begin() + 1, way_que_.end());
                setMouseEventPoint(p.x, p.y);
            }
        }
        else if (isCanPassEvent1(p.x, p.y) && calDistance(p.x, p.y, x, y) == 1)    //身边的特殊事件点，仅用于硝石
        {
            FindWay(x, y, x, y);
            setMouseEventPoint(p.x, p.y);
        }
        else if (canWalk(p.x, p.y) /* && !isOutScreen(p.x, p.y)*/)
        {
            FindWay(x, y, p.x, p.y);
        }
    }
    if (!walking_) { walk_next_started_ = -1; }
}

void SubScene::backRun()
{
    rest_time_++;
    //停止走动一段时间恢复站立姿势
    if (rest_time_ > 50)
    {
        step_ = 0;
    }
    if (current_frame_ % int(200 / RunNode::getRefreshInterval()) == 0)
    {
        for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
        {
            auto e = submap_info_->Event(i);
            if (e->BeginPic < e->EndPic)
            {
                e->CurrentPic++;
                if (e->CurrentPic > e->EndPic)
                {
                    e->CurrentPic = e->BeginPic;
                }
            }
        }
    }
    //LOG("sub scene %d,", current_frame_);
    // 内景不画云。
    //if (cloud_group_)
    //{
    //    cloud_group_->setPositionOnScreen(man_x_, man_y_, render_center_x_, render_center_y_);
    //}
}

void SubScene::onEntrance()
{
    calViewRegion();
    towards_ = MainScene::getInstance()->towards_;

    for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
    {
        auto e = submap_info_->Event(i);
        if (e->BeginPic < e->EndPic)
        {
            e->CurrentPic = e->BeginPic + e->PicDelay * 2 % (e->EndPic - e->BeginPic);
        }
    }

    if (force_begin_event_ >= 0)
    {
        Event::getInstance()->callEvent(force_begin_event_, this);
    }
    //setManViewPosition(submap_info_->EntranceX, submap_info_->EntranceY);
    //RunElement::addOnRootTop(MainScene::getInstance()->getWeather());
    addChild(Weather::getInstance());
    //fillEarth();

    //一大块地面的纹理，预先拼好地面，可以减少绘制的次数
    //暂时不使用这种方法，地面的动态效果会消失
    Engine::getInstance()->createRenderedTexture("searth", COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    reDrawEarthTexture();
    //Engine::getInstance()->saveTexture(earth_texture, std::format("{}.bmp", submap_id_).c_str());
    //Fade::fadeIn(8);
    auto scene_name = std::make_shared<TextBox>();
    scene_name->setFontSize(24);
    scene_name->setText(submap_info_->Name);
    scene_name->setPosition(Engine::getInstance()->getUIWidth() / 2 - Font::getTextDrawSize(submap_info_->Name) / 2 * 12, 100);
    scene_name->setStayFrame(40);
    addChild(scene_name);
}

void SubScene::onExit()
{
    Audio::getInstance()->playMusic(exit_music_);
}

void SubScene::onPressedCancel()
{
}

//冗余过多待清理
void SubScene::tryWalk(int x, int y)
{
    if (walking_) { return; }
    if (canWalk(x, y) && !beginWalking(x, y, view_x_, view_y_))
    {
        setManViewPosition(x, y);
    }
    step_++;
    if (step_ >= MAN_PIC_COUNT)
    {
        step_ = 1;
    }
    rest_time_ = 0;
}

bool SubScene::checkEvent(int x, int y, int tw /*= None*/, int item_id /*= -1*/)
{
    getTowardsPosition(man_x_, man_y_, tw, &x, &y);
    int event_index_submap = submap_info_->EventIndex(x, y);
    if (event_index_submap >= 0)
    {
        int id = 0;
        if (tw != Towards_None)
        {
            if (item_id < 0)
            {
                id = submap_info_->Event(x, y)->Event1;
            }
            else
            {
                id = submap_info_->Event(x, y)->Event2;
            }
            if (id > 0)
            {
                step_ = 0;
            }
        }
        else
        {
            id = submap_info_->Event(x, y)->Event3;
        }
        if (id > 0)
        {
            stopWalking();
            return Event::getInstance()->callEvent(id, this, submap_info_->ID, item_id, event_index_submap, x, y);
        }
    }
    return false;
}

bool SubScene::isBuilding(int x, int y)
{
    return submap_info_->Building(x, y) > 0 && submap_info_->Building(x, y) < 9999;
    //if (current_submap_->Building(x, y) >= -2 && current_submap_->Building(x, y) <= 0)
    //{
    //    return false;
    //}
    //else
    //{
    //    return true;
    //}
}

bool SubScene::isWater(int x, int y)
{
    int num = submap_info_->Earth(x, y) / 2;
    if (num >= 179 && num <= 181 || num == 261 || num == 511 || num >= 662 && num <= 665 || num == 674)
    {
        return true;
    }
    return false;
}

//是否是可通过的事件，好像只有硝石使用了
bool SubScene::isCanPassEvent1(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && !e->CannotWalk && e->Event1 > 0)
    {
        return true;
    }
    return false;
}

bool SubScene::isCannotPassEvent(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && (e->CannotWalk || isBuilding(x, y)))    //这里加上判断建筑可能是不对
    {
        return true;
    }
    return false;
}

//what is this?
bool SubScene::isFall(int x, int y)
{
    //if (abs(Save::getInstance()->m_SceneMapData[scene_id_].Data[4][x][y] -
    //Save::getInstance()->m_SceneMapData[scene_id_].Data[4][Cx][Cy] > 10))
    //{
    //    true;
    //}
    return false;
}

bool SubScene::isExit(int x, int y)
{
    if (submap_info_->ExitX[0] == x && submap_info_->ExitY[0] == y || submap_info_->ExitX[1] == x && submap_info_->ExitY[1] == y || submap_info_->ExitX[2] == x && submap_info_->ExitY[2] == y)
    {
        setExit(true);
        //Save::getInstance()->InSubMap = 1;
        return true;
    }
    return false;
}

bool SubScene::isJumpSubScene(int x, int y)
{
    if (submap_info_->JumpSubMap >= 0 && man_x_ == submap_info_->JumpX && man_y_ == submap_info_->JumpY)
    {
        int x, y;
        auto new_submap = Save::getInstance()->getSubMapInfo(submap_info_->JumpSubMap);
        if (submap_info_->MainEntranceX1 != 0)
        {
            //若原场景在大地图上有正常入口，则设置人物位置为新场景入口位置
            x = new_submap->EntranceX;
            y = new_submap->EntranceY;
        }
        else
        {
            //若原场景无法从大地图上进入，则设置人物在跳转返回位置
            x = new_submap->JumpReturnX;
            y = new_submap->JumpReturnY;
        }
        forceJumpSubScene(submap_info_->JumpSubMap, x, y);
        return true;
    }
    return false;
}

bool SubScene::isOutScreen(int x, int y)
{
    return (abs(view_x_ - x) >= 2 * view_width_region_ || abs(view_y_ - y) >= view_sum_region_);
}

bool SubScene::canWalk(int x, int y)
{
    bool ret = true;
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y) || isCannotPassEvent(x, y) || isFall(x, y))
    {
        ret = false;
    }
    //if (isCanPassEvent(x, y))
    //{
    //    ret = true;
    //}
    return ret;
}

void SubScene::forceExit()
{
    stopWalking();
    setVisible(false);
    setExit(true);
}

void SubScene::forceJumpSubScene(int submap_id, int x, int y)
{
    stopWalking();
    setID(submap_id);
    setManViewPosition(x, y);
    reDrawEarthTexture();
}

void SubScene::fillEarth()
{
    for (int i1 = 0; i1 < COORD_COUNT; i1++)
    {
        for (int i2 = 0; i2 < COORD_COUNT; i2++)
        {
            if (submap_info_->Earth(i1, i2) <= 0)
            {
                submap_info_->Earth(i1, i2) = 70;
            }
        }
    }
}

void SubScene::reDrawEarthTexture()
{
    auto earth_texture = Engine::getInstance()->getTexture("searth");
    if (earth_texture == nullptr)
    {
        return;
    }
    Engine::getInstance()->setRenderTarget(earth_texture);
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    if (TextureManager::getInstance()->getTextureGroup("smap-earth")->getTextureCount() > 0)
    {
        TextureManager::getInstance()->renderTexture("smap-earth", submap_info_->ID, 0, 0);
    }
    else
    {
        //二者之差是屏幕中心与大纹理的中心的距离
        for (int i1 = 0; i1 < COORD_COUNT; i1++)
        {
            for (int i2 = 0; i2 < COORD_COUNT; i2++)
            {
                auto p = getPositionOnWholeEarth(i1, i2);
                int h = submap_info_->BuildingHeight(i1, i2);
                int num = submap_info_->Earth(i1, i2) / 2;
                //无高度地面
                if (num > 0 && h <= 2)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
            }
        }
    }
    Engine::getInstance()->resetRenderTarget();
}

void SubScene::stopWalking()
{
    cancelWalking();
    way_que_.clear();
    setMouseEventPoint(-1, -1);
    total_step_ = 0;
}

bool SubScene::checkArrival()
{
    bool event = checkEvent3(man_x_, man_y_);
    if (isExit(man_x_, man_y_) || isJumpSubScene(man_x_, man_y_))
    {
        stopWalking();
        return true;
    }
    return event;
}

void SubScene::walkScriptStep(int x, int y)
{
    tryWalk(x, y);
    do
    {
        drawAndPresent();
        updateWalking();
    } while (walking_);
    walk_next_started_ = -1;
}
