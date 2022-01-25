#include "BattleSceneHades.h"
#include "Audio.h"
#include "Head.h"
#include "MainScene.h"

BattleSceneHades::BattleSceneHades()
{
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    head_self_ = std::make_shared<Head>();
    addChild(head_self_);
    //head_self_->setRole();
}

BattleSceneHades::BattleSceneHades(int id) : BattleSceneHades()
{
    setID(id);
}

BattleSceneHades::~BattleSceneHades()
{
}

void BattleSceneHades::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_);
}

void BattleSceneHades::draw()
{
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //地面是否需要亮度的变化，自动人物或者选择位置部分没有运行

    //一整块地面
    if (earth_texture_)
    {
        BP_Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture_, c);
        auto p = getPositionOnWholeEarth(man_x_, man_y_);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { p.x - render_center_x_ - x_, p.y - render_center_y_ - y_, w, h }, rect1 = { 0, 0, w, h };
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 1);
    }

#ifndef _DEBUG
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = earth_layer_.data(ix, iy) / 2;
                BP_Color color = { 255, 255, 255, 255 };
                bool need_draw = true;
                if (need_draw && num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y, color);
                }
            }
        }
    }
#endif
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int ix = man_x_ + i + (sum / 2);
            int iy = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnRender(ix, iy, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            if (!isOutLine(ix, iy))
            {
                int num = building_layer_.data(ix, iy) / 2;
                if (num > 0)
                {
                    TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                }
                //if (r)
                //{
                //    std::string path = fmt1::format("fight/fight{:03}", r->HeadID);
                //    BP_Color color = { 255, 255, 255, 255 };
                //    uint8_t alpha = 255;
                //    if (battle_cursor_->isRunning() && !acting_role_->isAuto())
                //    {
                //        color = { 128, 128, 128, 255 };
                //        if (inEffect(acting_role_, r))
                //        {
                //            color = { 255, 255, 255, 255 };
                //        }
                //    }
                //    int pic;
                //    if (r == acting_role_)
                //    {
                //        pic = calRolePic(r, action_type_, action_frame_);
                //    }
                //    else
                //    {
                //        pic = calRolePic(r);
                //    }
                //    if (r->HP <= 0)
                //    {
                //        alpha = dead_alpha_;
                //    }
                //    TextureManager::getInstance()->renderTexture(path, pic, p.x, p.y, color, alpha);
                //    renderExtraRoleInfo(r, p.x, p.y);
                //}
            }
        }
    }
    Engine::getInstance()->renderAssistTextureToWindow();


    if (result_ >= 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    }
}

void BattleSceneHades::dealEvent(BP_Event& e)
{
    if (e.type == BP_KEYDOWN)
    {
        switch (e.key.keysym.sym)
        {
        case BPK_a:
            man_x_-=.3;
            break;
        case BPK_d:
            man_x_ += .3;
            break;
        case BPK_w:
            man_y_ -= .3;
            break;
        case BPK_s:
            man_y_ += .3;
            break;
        }
    }
}

void BattleSceneHades::dealEvent2(BP_Event& e)
{

}

void BattleSceneHades::onEntrance()
{
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(80, 100);

    //RunElement::addOnRootTop(MainScene::getInstance()->getWeather());
    addChild(MainScene::getInstance()->getWeather());

    //earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    //Engine::getInstance()->setRenderTarget(earth_texture_);
    ////二者之差是屏幕中心与大纹理的中心的距离
    //for (int i1 = 0; i1 < COORD_COUNT; i1++)
    //{
    //    for (int i2 = 0; i2 < COORD_COUNT; i2++)
    //    {
    //        auto p = getPositionOnWholeEarth(i1, i2);
    //        int num = earth_layer_.data(i1, i2) / 2;
    //        //无高度地面
    //        if (num > 0)
    //        {
    //            TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
    //        }
    //    }
    //}
    //Engine::getInstance()->resetRenderTarget();

    readBattleInfo();
    //初始状态
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);
        r->Progress = 0;
    }
}

void BattleSceneHades::onExit()
{

}

void BattleSceneHades::backRun()
{

}
