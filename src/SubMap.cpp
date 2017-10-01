#include "SubMap.h"
#include "MainMap.h"
#include "BattleMap.h"
#include "Event.h"
#include "UI.h"


SubMap::SubMap()
{
    full_window_ = 1;
}

SubMap::SubMap(int num) : SubMap()
{
    setSceneNum(num);
}

SubMap::~SubMap()
{

}

void SubMap::draw()
{
    int k = 0;
    struct DrawInfo { int i; Point p; };
    //std::map<int, DrawInfo> map;

    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, screen_center_x_*2, screen_center_y_*2);
    //以下画法存在争议
    //一整块地面
#ifndef _DEBUG
    //auto p = getPositionOnWholeEarth(view_x_, view_y_);
    //int w = screen_center_x_ * 2;
    //int h = screen_center_y_ * 2;
    //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
    //Engine::getInstance()->renderCopy(earth_texture_, { p.x - screen_center_x_, p.y - screen_center_y_, w, h }, { 0, 0, w, h }, 1);
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = view_x_ + i + (sum / 2);
            int i2 = view_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, view_x_, view_y_);
            if (i1 >= 0 && i1 < COORD_COUNT && i2 >= 0 && i2 < COORD_COUNT)
            {
                int h = record_->BuildingHeight(i1, i2);
                int num = record_->Earth(i1, i2) / 2;
                //无高度地面
                if (num > 0 && h == 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                    /*auto tex = TextureManager::getInstance()->loadTexture("smap", num);
                    //用大图画时的闪烁地面
                    if (tex->count > 1)
                    {
                        TextureManager::getInstance()->renderTexture(tex, p.x, p.y);
                    }*/
                }
            }
        }
    }
#endif
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = view_x_ + i + (sum / 2);
            int i2 = view_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, view_x_, view_y_);
            if (i1 >= 0 && i1 < COORD_COUNT && i2 >= 0 && i2 < COORD_COUNT)
            {
                //有高度地面
                int h = record_->BuildingHeight(i1, i2);
                int num = record_->Earth(i1, i2) / 2;
#ifndef _DEBUG
                if (num > 0 && h > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
#endif
                //建筑和主角
                num = record_->Building(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h);
                }
                if (i1 == man_x_ && i2 == man_y_)
                {
                    if (man_pic_ != -1)
                    {
                        man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;  //每个方向的第一张是静止图
                        TextureManager::getInstance()->renderTexture("smap", man_pic_, p.x, p.y - h);
                    }
                }
                //事件
                auto event = record_->Event(i1, i2);
                if (event)
                {
                    num = record_->Event(i1, i2)->CurrentPic / 2;
                    //map[calBlockTurn(i1, i2, 2)] = s;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h);
                    }
                }
                //装饰
                num = record_->Decoration(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - record_->DecorationHeight(i1, i2));
                }
            }
            //k++;
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();
}

void SubMap::dealEvent(BP_Event& e)
{
    //实际上很大部分与大地图类似，这里暂时不合并了，就这样
    int x = man_x_, y = man_y_;
    //功能键
    if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE
        || e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT)
    {
        UI::getInstance()->run();
        clearEvent(e);
    }

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
        getTowardsFromKey(pressed);
        getTowardsPosition(man_x_, man_y_, towards_, &x, &y);
        tryWalk(x, y, towards_);
        if (total_step_ <= 1)
        {
            Engine::getInstance()->delay(50);
        }
        total_step_++;
    }
    else
    {
        total_step_ = 0;
    }

    if (e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
    {
        if (checkEvent1(x, y, towards_))
        {
            clearEvent(e);
            step_ = 0;
        }
    }
    checkEvent3(x, y);
    if (isExit(x, y))
    {
        clearEvent(e);
        total_step_ = 0;
    }

    //鼠标寻路，未完成
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        //getMousePosition(e.button.x, e.button.y);
        //stopFindWay();
        //if (canWalk(mouse_x_, mouse_y_) && !isOutScreen(mouse_x_, mouse_y_))
        //{
        //    FindWay(view_x_, view_y_, mouse_x_, mouse_y_);
        //}
    }
    //if (!way_que_.empty())
    //{
    //    PointEx newMyPoint = way_que_.back();
    //    x = newMyPoint.x;
    //    y = newMyPoint.y;
    //    isExit(x, y);
    //    Towards myTowards = (Towards)(newMyPoint.towards);
    //    //log("myTowards=%d", myTowards);
    //    tryWalk(x, y, myTowards);
    //    way_que_.pop_back();
    //    //log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
    //}
}

void SubMap::backRun()
{
    for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
    {
        auto e = record_->Event(i);
        //if (e->PicDelay > 0)
        {
            e->CurrentPic++;
            //e->CurrentPic += e->PicDelay;
        }
        if (e->CurrentPic > e->EndPic)
        {
            e->CurrentPic = e->BeginPic;
        }
    }
}

//一大块地面的纹理，未启用
void SubMap::entrance()
{
    calViewRegion();
    record_ = Save::getInstance()->getSubMapRecord(submap_id_);
    record_->ID = submap_id_;   //这句是修正存档中可能存在的错误
    setPosition(record_->EntranceX, record_->EntranceY);
    //earth_texture_ = Engine::getInstance()->createRGBARenderedTexture(MAX_COORD * SUBMAP_TILE_W * 2, MAX_COORD * SUBMAP_TILE_H * 2);
    //Engine::getInstance()->setRenderTarget(earth_texture_);

    ////二者之差是屏幕中心与大纹理的中心的距离
    //for (int i1 = 0; i1 < MAX_COORD; i1++)
    //{
    //    for (int i2 = 0; i2 < MAX_COORD; i2++)
    //    {
    //        auto p = getPositionOnWholeEarth(i1, i2);
    //        int h = record_->BuildingHeight(i1, i2);
    //        int num = record_->Earth(i1, i2) / 2;
    //        //无高度地面
    //        if (num > 0 && h == 0)
    //        {
    //            TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
    //        }
    //    }
    //}
    //Engine::getInstance()->resetRenderTarget();
}

void SubMap::exit()
{
    //if (earth_texture_)
    //{
    //    Engine::destroyTexture(earth_texture_);
    //}
}

//冗余过多待清理
void SubMap::tryWalk(int x, int y, Towards t)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
        view_x_ = x;
        view_y_ = y;
    }
    step_++;
    if (step_ >= MAN_PIC_COUNT)
    {
        step_ = 1;
    }
}

bool SubMap::checkEvent(int x, int y, Towards t /*= None*/, int item_id /*= -1*/)
{
    getTowardsPosition(man_x_, man_y_, t, &x, &y);
    int event_index_submap = record_->EventIndex(x, y);
    if (event_index_submap >= 0)
    {
        int id;
        if (t != None)
        {
            id = record_->Event(x, y)->Event1;
            if (id > 0) { step_ = 0; }
        }
        else
        {
            id = record_->Event(x, y)->Event3;
        }
        if (id > 0)
        {
            return Event::getInstance()->callEvent(id, this, record_->ID, item_id, event_index_submap, x, y);
        }
    }
    return false;
}

bool SubMap::canWalk(int x, int y)
{
    bool ret = true;
    if (isOutLine(x, y) || isBuilding(x, y) || isWater(x, y)
        || isCannotPassEvent(x, y) || isFall(x, y))
    {
        ret = false;
    }
    if (isCanPassEvent(x, y))
    {
        ret = true;
    }
    return ret;
}

bool SubMap::isBuilding(int x, int y)
{
    return record_->Building(x, y) > 0;
    //if (current_submap_->Building(x, y) >= -2 && current_submap_->Building(x, y) <= 0)
    //{
    //    return false;
    //}
    //else
    //{
    //    return true;
    //}
}

bool SubMap::isOutLine(int x, int y)
{
    return (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT);
}

bool SubMap::isWater(int x, int y)
{
    int num = record_->Earth(x, y) / 2;
    if (num >= 179 && num <= 181
        || num == 261 || num == 511
        || num >= 662 && num <= 665
        || num == 674)
    {
        return true;
    }
    return false;
}

bool SubMap::isCanPassEvent(int x, int y)
{
    auto e = record_->Event(x, y);
    if (e && !e->CannotWalk)
    {
        return true;
    }
    return false;
}

bool SubMap::isCannotPassEvent(int x, int y)
{
    auto e = record_->Event(x, y);
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
    if (record_->ExitX[0] == x && record_->ExitY[0] == y
        || record_->ExitX[1] == x && record_->ExitY[1] == y
        || record_->ExitX[2] == x && record_->ExitY[2] == y)
    {
        loop_ = false;
        Save::getInstance()->InSubmap = 1;
        return true;
    }
    return false;
}

bool SubMap::isOutScreen(int x, int y)
{
    return (abs(view_x_ - x) >= 2 * view_width_region_ || abs(view_y_ - y) >= view_sum_region_);
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

Point SubMap::getPositionOnWholeEarth(int x, int y)
{
    auto p = getPositionOnScreen(x, y, 0, 0);
    p.x += COORD_COUNT * TILE_W - screen_center_x_;
    p.y += 2 * TILE_H - screen_center_y_;
    return p;
}

