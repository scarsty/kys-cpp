#include "BattleSceneHades.h"
#include "Audio.h"
#include "GameUtil.h"
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
    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderAssistTexture();
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(man_x1_, man_y1_);
        man_x_ = p.x;
        man_y_ = p.y;
    }
    //一整块地面
    if (earth_texture_)
    {
        Engine::getInstance()->setRenderTarget(earth_texture_);
        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {
                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
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

        struct DrawInfo
        {
            std::string path;
            int num;
            Point p;
        };
        std::vector<DrawInfo> building_vec;
        building_vec.reserve(10000);

        for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
        {
            for (int i = -view_width_region_; i <= view_width_region_; i++)
            {

                int ix = man_x_ + i + (sum / 2);
                int iy = man_y_ - i + (sum - sum / 2);
                auto p = pos45To90(ix, iy);
                if (!isOutLine(ix, iy))
                {
                    int num = building_layer_.data(ix, iy) / 2;
                    if (num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y);
                        building_vec.emplace_back("smap", num, p);
                    }
                }
            }
        }

        for (auto r : battle_roles_)
        {
            std::string path = fmt1::format("fight/fight{:03}", r->HeadID);
            BP_Color color = { 255, 255, 255, 255 };
            uint8_t alpha = 255;
            if (battle_cursor_->isRunning() && !acting_role_->isAuto())
            {
                color = { 128, 128, 128, 255 };
                if (inEffect(acting_role_, r))
                {
                    color = { 255, 255, 255, 255 };
                }
            }
            int pic;
            if (r == acting_role_)
            {
                pic = calRolePic(r, action_type_, action_frame_);
            }
            else
            {
                pic = calRolePic(r);
            }
            if (r->HP <= 0)
            {
                alpha = dead_alpha_;
            }
            pic = calRolePic(r, r->ActType, r->ActFrame);
            //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
            building_vec.emplace_back(path, pic, Point{ r->X1, r->Y1 });
            //renderExtraRoleInfo(r, r->X1, r->Y1);
        }

        auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
        {
            if (d1.p.y != d2.p.y)
                return d1.p.y < d2.p.y;
            else
                return d1.p.x < d2.p.x;
        };
        std::sort(building_vec.begin(), building_vec.end(), sort_building);
        for (auto& d : building_vec)
        {
            TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y);
        }

        BP_Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture_, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        BP_Rect rect0 = { man_x1_ - render_center_x_ - x_, man_y1_ - render_center_y_ - y_, w, h }, rect1 = { 0, 0, w, h };
        Engine::getInstance()->setRenderAssistTexture();
        Engine::getInstance()->renderCopy(earth_texture_, &rect0, &rect1, 1);
    }

    Engine::getInstance()->renderAssistTextureToWindow();

    if (result_ >= 0)
    {
        Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    }
}

void BattleSceneHades::dealEvent(BP_Event& e)
{
    auto engine = Engine::getInstance();

    //方向
    man_x1_ = role_->X1;
    man_y1_ = role_->Y1;
    if (cool_down_ == 0)
    {
        if (engine->checkKeyPress(BPK_a))
        {
            man_x1_ -= 2;
            role_->FaceTowards = Towards_LeftDown;
        }
        if (engine->checkKeyPress(BPK_d))
        {
            man_x1_ += 2;
            role_->FaceTowards = Towards_RightUp;
        }
        if (engine->checkKeyPress(BPK_w))
        {
            man_y1_ -= 1;
            role_->FaceTowards = Towards_LeftUp;
        }
        if (engine->checkKeyPress(BPK_s))
        {
            man_y1_ += 1;
            role_->FaceTowards = Towards_RightDown;
        }

        if (engine->checkKeyPress(BPK_1))
        {
            weapon_ = 1;
        }
        if (engine->checkKeyPress(BPK_2))
        {
            weapon_ = 2;
        }
        if (engine->checkKeyPress(BPK_3))
        {
            weapon_ = 3;
        }
        if (engine->checkKeyPress(BPK_4))
        {
            weapon_ = 4;
        }
    }
    if (engine->checkKeyPress(BPK_w) && engine->checkKeyPress(BPK_d))
    {
        role_->FaceTowards = Towards_RightUp;
    }
    if (engine->checkKeyPress(BPK_s) && engine->checkKeyPress(BPK_a))
    {
        role_->FaceTowards = Towards_LeftDown;
    }

    if (canWalk90(man_x1_, man_y1_))
    {
        role_->X1 = man_x1_;
        role_->Y1 = man_y1_;
    }
    if (cool_down_ == 0)
    {
        if (engine->checkKeyPress(BPK_j))    //轻攻击
        {
            cool_down_ = 30;
            role_->ActType = weapon_;
            role_->ActFrame = 0;
            role_->PhysicalPower -= 5;
            heavy_ = 0;
        }
        if (engine->checkKeyPress(BPK_k))    //重攻击
        {
            cool_down_ = 60;
            role_->ActType = weapon_;
            role_->ActFrame = 0;
            role_->PhysicalPower -= 10;
            heavy_ = 1;
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
    head_self_->setPosition(80, Engine::getInstance()->getWindowHeight() - 200);

    addChild(MainScene::getInstance()->getWeather());

    earth_texture_ = Engine::getInstance()->createARGBRenderedTexture(COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);

    readBattleInfo();
    //初始状态
    for (auto r : battle_roles_)
    {
        setRoleInitState(r);

        auto p = pos45To90(r->X(), r->Y());

        r->X1 = p.x;
        r->Y1 = p.y;
        r->Progress = 0;
        if (r->HeadID == 0)
        {
            role_ = r;
            head_self_->setRole(r);
            man_x1_ = r->X1;
            man_y1_ = r->Y1;
        }
    }
}

void BattleSceneHades::onExit()
{
    Engine::getInstance()->destroyTexture(earth_texture_);
}

void BattleSceneHades::backRun()
{
    if (cool_down_ > 0) { cool_down_--; }
    if (current_frame_ % 2 == 0)
    {
        for (auto r : battle_roles_)
        {
            if (r->ActType >= 0)
            {
                if ((heavy_ == 0 && r->ActFrame >= r->FightFrame[r->ActType] / 2)
                    || (heavy_ == 1 && r->ActFrame >= r->FightFrame[r->ActType]))
                {
                    r->ActType = -1;
                    r->ActFrame = 0;
                }
                else
                {
                    if (heavy_ == 0)
                    {
                        r->ActFrame++;
                    }
                    else
                    {
                        if (current_frame_ % 8 == 0)
                        {
                            r->ActFrame++;
                            //x_ = rng_.rand_int(2) - rng_.rand_int(2);
                            //y_ = rng_.rand_int(2) - rng_.rand_int(2);
                        }
                    }
                }
            }
            else
            {
                r->ActFrame = 0;
            }
        }
    }
    if (cool_down_ == 0)
    {
        for (auto r : battle_roles_)
        {
            if (current_frame_ % 5 == 0) { r->PhysicalPower += 1; }
            r->PhysicalPower = GameUtil::limit(r->PhysicalPower, 0, 100);
        }
    }
}
