#include "SubScene.h"
#include "MainScene.h"
#include "BattleScene.h"
#include "Event.h"
#include "UI.h"
#include "Audio.h"


SubScene::SubScene()
{
    full_window_ = 1;
    COORD_COUNT = SUBMAP_COORD_COUNT;
}

SubScene::SubScene(int id) : SubScene()
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
    if (submap_info_ == nullptr) { setExit(true); }
    //submap_info_->ID = submap_id_;   //这句是修正存档中可能存在的错误
    exit_music_ = submap_info_->ExitMusic;
    Audio::getInstance()->playMusic(submap_info_->EntranceMusic);
    printf("Sub Scene %d, %s\n", submap_id_, submap_info_->Name);
}

void SubScene::draw()
{
    int k = 0;
    struct DrawInfo { int i; Point p; };
    //std::map<int, DrawInfo> map;

    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);
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
            auto p = getPositionOnRender(i1, i2, view_x_, view_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(i1, i2))
            {
                int h = submap_info_->BuildingHeight(i1, i2);
                int num = submap_info_->Earth(i1, i2) / 2;
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
            auto p = getPositionOnRender(i1, i2, view_x_, view_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(i1, i2))
            {
                //有高度地面
                int h = submap_info_->BuildingHeight(i1, i2);
                int num = submap_info_->Earth(i1, i2) / 2;
#ifndef _DEBUG
                if (num > 0 && h > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
#endif
                //建筑和主角
                num = submap_info_->Building(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h);
                }
                if (i1 == man_x_ && i2 == man_y_)
                {
                    //此处当主角的贴图为负值时，表示强制设置贴图号
                    if (force_man_pic_ < 0)
                    {
                        man_pic_ = calManPic();
                    }
                    else
                    {
                        man_pic_ = force_man_pic_;
                    }
                    TextureManager::getInstance()->renderTexture("smap", man_pic_, p.x, p.y - h);
                }
                //事件
                auto event = submap_info_->Event(i1, i2);
                if (event)
                {
                    num = submap_info_->Event(i1, i2)->CurrentPic / 2;
                    //map[calBlockTurn(i1, i2, 2)] = s;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - h);
                    }
                }
                //装饰
                num = submap_info_->Decoration(i1, i2) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y - submap_info_->DecorationHeight(i1, i2));
                }
            }
            //k++;
        }
    }

    //鼠标的位置
    auto p = getMousePosition(view_x_, view_y_);
    p = getPositionOnRender(p.x, p.y, view_x_, view_y_);
    TextureManager::getInstance()->renderTexture("mmap", 1, p.x, p.y, { 255, 255, 255, 255 }, 128);

    Engine::getInstance()->renderAssistTextureToWindow();
}

void SubScene::dealEvent(BP_Event& e)
{
    //实际上很大部分与大地图类似，这里暂时不合并了，就这样
    int x = man_x_, y = man_y_;

    checkEvent3(x, y);
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
        if (total_step_ < 1 || total_step_ >= 5)
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

    if (e.type == BP_KEYUP && (e.key.keysym.sym == BPK_RETURN || e.key.keysym.sym == BPK_SPACE))
    {
        if (checkEvent1(x, y, towards_))
        {
            clearEvent(e);
            step_ = 0;
        }
    }

    if (isExit(x, y) || isJumpSubScene(x, y))
    {
        clearEvent(e);
        total_step_ = 0;
    }

    //鼠标寻路，未完成
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        Point p = getMousePosition(e.button.x, e.button.y, x, y);
        stopFindWay();
        if (canWalk(p.x, p.y) && !isOutScreen(p.x, p.y))
        {
            FindWay(x, y, p.x, p.y);
        }
    }
    if (!way_que_.empty())
    {
        PointEx newMyPoint = way_que_.back();
        x = newMyPoint.x;
        y = newMyPoint.y;
        isExit(x, y);
        towards_ = newMyPoint.towards;
        //log("myTowards=%d", myTowards);
        tryWalk(x, y);
        way_que_.pop_back();
        //log("not empty2 %d,%d", wayQue.top()->x, wayQue.top()->y);
    }
}

void SubScene::backRun()
{
    for (int i = 0; i < SUBMAP_EVENT_COUNT; i++)
    {
        auto e = submap_info_->Event(i);
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
void SubScene::onEntrance()
{
    calViewRegion();
    towards_ = MainScene::getIntance()->towards_;
    //setManViewPosition(submap_info_->EntranceX, submap_info_->EntranceY);

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

void SubScene::onExit()
{
    Audio::getInstance()->playMusic(exit_music_);
    //if (earth_texture_)
    //{
    //    Engine::destroyTexture(earth_texture_);
    //}
}

void SubScene::onPressedCancel()
{
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

//冗余过多待清理
void SubScene::tryWalk(int x, int y)
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
            if (id > 0) { step_ = 0; }
        }
        else
        {
            id = submap_info_->Event(x, y)->Event3;
        }
        if (id > 0)
        {
            return Event::getInstance()->callEvent(id, this, submap_info_->ID, item_id, event_index_submap, x, y);
        }
    }
    return false;
}

bool SubScene::canWalk(int x, int y)
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

bool SubScene::isBuilding(int x, int y)
{
    return submap_info_->Building(x, y) > 0;
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
    if (num >= 179 && num <= 181
        || num == 261 || num == 511
        || num >= 662 && num <= 665
        || num == 674)
    {
        return true;
    }
    return false;
}

bool SubScene::isCanPassEvent(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && !e->CannotWalk)
    {
        return true;
    }
    return false;
}

bool SubScene::isCannotPassEvent(int x, int y)
{
    auto e = submap_info_->Event(x, y);
    if (e && e->CannotWalk)
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
    if (submap_info_->ExitX[0] == x && submap_info_->ExitY[0] == y
        || submap_info_->ExitX[1] == x && submap_info_->ExitY[1] == y
        || submap_info_->ExitX[2] == x && submap_info_->ExitY[2] == y)
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

Point SubScene::getPositionOnWholeEarth(int x, int y)
{
    auto p = getPositionOnRender(x, y, 0, 0);
    p.x += COORD_COUNT * TILE_W - render_center_x_;
    p.y += 2 * TILE_H - render_center_y_;
    return p;
}

void SubScene::forceExit()
{
    setVisible(false);
    setExit(true);
}

void SubScene::forceJumpSubScene(int submap_id, int x, int y)
{
    setID(submap_id);
    setManViewPosition(x, y);
}

