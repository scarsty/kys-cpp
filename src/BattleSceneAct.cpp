#include "BattleSceneAct.h"
#include "GameUtil.h"
#include "filefunc.h"
#include <limits>

namespace
{
constexpr int PAPER_LOCK_MARK_TEXTURE = 201;
constexpr float PAPER_FREE_CAMERA_ROTATE_SPEED = 0.035f;
constexpr float PAPER_FREE_CAMERA_HEIGHT_SPEED = 6.0f;
constexpr float PAPER_CAMERA_DEFAULT_DISTANCE = 400.0f;
constexpr float PAPER_CAMERA_MIN_DISTANCE = 220.0f;
constexpr float PAPER_CAMERA_MAX_DISTANCE = 900.0f;
constexpr float PAPER_CAMERA_ZOOM_STEP = 24.0f;
constexpr float PAPER_CAMERA_WHEEL_ZOOM_STEP = 80.0f;
constexpr float PAPER_CAMERA_MIN_HEIGHT = 80.0f;
constexpr float PAPER_CAMERA_MAX_HEIGHT = 500.0f;
constexpr float PAPER_ROLE_INFO_TOP_GAP = 4.0f;
}

bool BattleSceneAct::usePaperPresentation() const
{
    return GameUtil::getInstance()->getInt("game", "battle_presentation", 0) != 0;
}

void BattleSceneAct::drawClassicPresentation()
{
    Engine::getInstance()->setRenderTarget("scene");
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    {
        auto p = pos90To45(pos_.x, pos_.y);
        man_x_ = p.x;
        man_y_ = p.y;
    }

    auto whole_scene = Engine::getInstance()->getTexture("whole_scene");
    if (!whole_scene)
    {
        Engine::getInstance()->renderTextureToMain("scene");
        drawClassicExtraEffects();
        return;
    }
    Engine::getInstance()->setRenderTarget(whole_scene);
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
    auto earth_tex = Engine::getInstance()->getTexture("earth");
    if (earth_tex)
    {
        Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_tex, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
        Rect rect1 = rect0;
        rect0.y += TILE_H * 2;
        if (rect0.x < 0)
        {
            rect1.x -= rect0.x;
            rect0.x = 0;
        }
        if (rect0.y < 0)
        {
            rect1.y -= rect0.y;
            rect0.y = 0;
        }
        if (rect0.x + rect0.w > COORD_COUNT * TILE_W * 2)
        {
            rect1.w -= (rect0.x + rect0.w - COORD_COUNT * TILE_W * 2);
            rect0.w = COORD_COUNT * TILE_W * 2 - rect0.x;
        }
        if (rect0.y + rect0.h > COORD_COUNT * TILE_H * 2)
        {
            rect1.h -= (rect0.y + rect0.h - COORD_COUNT * TILE_H * 2);
            rect0.h = COORD_COUNT * TILE_H * 2 - rect0.y;
        }
        std::vector<Color> cv(4, { 255, 255, 255, 255 });
        Engine::getInstance()->renderTextureLight(earth_tex, &rect0, &rect1, cv, { 0.25, 0, 0, 0 });
    }
    else
    {
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
                    Color color = { 255, 255, 255, 255 };
                    bool need_draw = true;
                    if (need_draw && num > 0)
                    {
                        TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y / 2, { color });
                    }
                }
            }
        }
    }

    std::vector<ClassicDrawInfo> draw_infos;
    draw_infos.reserve(10000);

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
                    ClassicDrawInfo info;
                    info.path = "smap";
                    info.num = num;
                    info.p.x = p.x;
                    info.p.y = p.y;
                    info.shadow = 0;
                    draw_infos.emplace_back(std::move(info));
                }
            }
        }
    }

    for (auto r : battle_roles_)
    {
        ClassicDrawInfo info;
        info.path = std::format("fight/fight{:03}", r->HeadID);
        info.color = { 255, 255, 255, 255 };
        info.alpha = 255;
        info.white = 0;
        if (battle_cursor_->isRunning() && !acting_role_->isAuto())
        {
            info.color = { 128, 128, 128, 255 };
            if (inEffect(acting_role_, r))
            {
                info.color = { 255, 255, 255, 255 };
            }
        }
        info.p = r->Pos;
        if (result_ == -1 && r->Shake)
        {
            info.p.x += -2.5 + rand_.rand() * 5;
        }
        r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
        info.num = calRolePic(r, r->ActType, r->ActFrame);
        if (r->Dead)
        {
            info.rot = r->FaceTowards >= 2 ? 90 : 270;
            info.draw_turn = 0;
        }
        if (r->Attention)
        {
            info.alpha = 255 - r->Attention * 4;
        }
        info.shadow = 1;
        adjustClassicRoleDrawInfo(r, info);
        draw_infos.emplace_back(std::move(info));
    }

    for (auto& ae : attack_effects_)
    {
        ClassicDrawInfo info;
        info.path = ae.Path;
        info.num = ae.TotalEffectFrame > 0 ? ae.Frame % ae.TotalEffectFrame : 0;
        info.p = ae.FollowRole ? ae.FollowRole->Pos : ae.Pos;
        info.color = { 255, 255, 255, 255 };
        info.alpha = 192;
        info.shadow = 1;
        if (ae.Attacker && ae.Attacker->Team == 0)
        {
            info.shadow = 2;
        }
        info.alpha = 255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25);
        draw_infos.emplace_back(std::move(info));
    }

    auto sort_building = [](ClassicDrawInfo& d1, ClassicDrawInfo& d2)
    {
        if (d1.p.y != d2.p.y)
        {
            return d1.p.y < d2.p.y;
        }
        return d1.p.x < d2.p.x;
    };
    std::sort(draw_infos.begin(), draw_infos.end(), sort_building);

    for (auto& d : draw_infos)
    {
        if (d.shadow)
        {
            auto tex = TextureManager::getInstance()->getTexture(d.path, d.num);
            if (tex)
            {
                double scalex = 1;
                double scaley = 0.3;
                int yd = tex->dy * 0.7;
                if (d.rot)
                {
                    scalex = 0.3;
                    scaley = 1;
                    yd = tex->dy * 0.1;
                }
                if (d.shadow == 1)
                {
                    TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd,
                        { { 32, 32, 32, 255 }, uint8_t(d.alpha / 2), scalex, scaley, double(d.rot) });
                }
                if (d.shadow == 2)
                {
                    TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd,
                        { { 128, 128, 128, 255 }, uint8_t(d.alpha / 2), scalex, scaley, double(d.rot2), 128 });
                }
            }
        }
    }

    for (int turn = 0; turn < 2; turn++)
    {
        for (auto& d : draw_infos)
        {
            if (d.draw_turn != turn) { continue; }
            double scaley = d.rot ? 0.5 : 1;
            std::vector<Color> color_v;
            std::vector<float> brightness_v;
            if (d.alpha == 255)
            {
                brightness_v.resize(4, 0);
                brightness_v[0] = 0.5;
            }
            TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y / 2 - d.p.z,
                { d.color, d.alpha, scaley, 1, double(d.rot), d.white, color_v, brightness_v });
            if (d.breathless)
            {
                int tile_scale = (std::max)(1, TILE_W / TILE_W_0);
                TextureManager::getInstance()->renderTexture("title", 205, d.p.x - 5, d.p.y / 2 - d.p.z - 36 * tile_scale,
                    { { 255, 255, 255, 255 }, 255, 0.1, 0.1, double(d.rot2), 0 });
            }
        }
    }

    for (auto r : battle_roles_)
    {
        renderExtraRoleInfo(r, r->Pos.x, r->Pos.y / 2);
    }

    Color c = { 255, 255, 255, 255 };
    Engine::getInstance()->setColor(whole_scene, c);
    int w = render_center_x_ * 2;
    int h = render_center_y_ * 2;
    Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
    Rect rect1 = { 0, 0, w, h };
    if (rect0.x < 0)
    {
        rect1.x = -rect0.x;
        rect0.x = 0;
        rect0.w = w - rect1.x;
    }
    if (rect0.y < 0)
    {
        rect1.y = -rect0.y;
        rect0.y = 0;
        rect0.h = h - rect1.y;
    }
    rect0.w = std::min(rect0.w, COORD_COUNT * TILE_W * 2 - rect0.x);
    rect0.h = std::min(rect0.h, COORD_COUNT * TILE_H * 2 - rect0.y);
    rect1.w = rect0.w;
    rect1.h = rect0.h;
    for (auto& te : text_effects_)
    {
        int w_ui, h_ui;
        Engine::getInstance()->getUISize(w_ui, h_ui);
        int x = te.Pos.x * w_ui / render_center_x_ / 2;
        int y = te.Pos.y * h_ui / render_center_y_ / 2 / 2;
        x -= rect0.x;
        y -= rect0.y;
        Font::getInstance()->draw(te.Text, te.Size, x, y, te.color, 255);
    }

    Engine::getInstance()->setRenderTarget("scene");
    if (close_up_)
    {
        rect0.w /= 2;
        rect0.h /= 2;
        rect0.x += rect0.w / 2;
        int tile_scale = (std::max)(1, TILE_W / TILE_W_0);
        rect0.y += rect0.h / 2 - 20 * tile_scale;
    }

    Engine::getInstance()->renderTexture(whole_scene, &rect0, &rect1, 0);
    Engine::getInstance()->renderTextureToMain("scene");
    drawClassicExtraEffects();
}

void BattleSceneAct::initializePaperPresentation()
{
    paper_presentation_.initialize();
    paper_presentation_.initializeScene(Engine::getInstance(), {
        .earth_layer = &earth_layer_,
        .building_layer = &building_layer_,
        .battle_field_id = info_->BattleFieldID,
        .coordinate_count = COORD_COUNT,
        .tile_width = TILE_W,
        .tile_height = TILE_H,
        .to_world = [this](int x, int y) { return pos45To90(x, y); },
    });
    camera_focus_ = paper_presentation_.initialFocus();
    camera_pos_ = camera_focus_ + Pointf{
        std::cos(camera_angle_) * free_camera_distance_,
        std::sin(camera_angle_) * free_camera_distance_,
        camera_height_
    };
}

void BattleSceneAct::handlePaperPresentationEvent(EngineEvent& e)
{
    if (!usePaperPresentation())
    {
        return;
    }

    auto engine = Engine::getInstance();
    if ((e.type == EVENT_KEY_UP && e.key.key == K_Q)
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_MIDDLE)
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_RIGHT_STICK))
    {
        camera_locked_ = !camera_locked_;
        if (!camera_locked_)
        {
            camera_distance_ = free_camera_distance_ > 0 ? free_camera_distance_ : PAPER_CAMERA_DEFAULT_DISTANCE;
        }
    }
    if (!camera_locked_ && e.type == EVENT_MOUSE_WHEEL && e.wheel.y != 0)
    {
        free_camera_distance_ = std::clamp(free_camera_distance_ - e.wheel.y * PAPER_CAMERA_WHEEL_ZOOM_STEP,
            PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);
    }
    if (!camera_locked_)
    {
        if (engine->checkKeyPress(K_Z)) { free_camera_distance_ += PAPER_CAMERA_ZOOM_STEP; }
        if (engine->checkKeyPress(K_X)) { free_camera_distance_ -= PAPER_CAMERA_ZOOM_STEP; }
        auto left_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFT_TRIGGER);
        auto right_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHT_TRIGGER);
        if (left_trigger > 6000) { free_camera_distance_ += PAPER_CAMERA_ZOOM_STEP * GameUtil::clamp(left_trigger, 0, 20000) / 20000.0f; }
        if (right_trigger > 6000) { free_camera_distance_ -= PAPER_CAMERA_ZOOM_STEP * GameUtil::clamp(right_trigger, 0, 20000) / 20000.0f; }

        float rotate = 0;
        float height_delta = 0;
        if (engine->checkKeyPress(K_LEFT)) { rotate += 1; }
        if (engine->checkKeyPress(K_RIGHT)) { rotate -= 1; }
        if (engine->checkKeyPress(K_UP)) { height_delta -= 1; }
        if (engine->checkKeyPress(K_DOWN)) { height_delta += 1; }
        if (engine->checkKeyPress(K_J)) { rotate += 1; }
        if (engine->checkKeyPress(K_L)) { rotate -= 1; }
        if (engine->checkKeyPress(K_I)) { height_delta -= 1; }
        if (engine->checkKeyPress(K_K)) { height_delta += 1; }
        auto right_axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTX);
        auto right_axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTY);
        if (std::abs(right_axis_x) >= 6000) { rotate -= GameUtil::clamp(right_axis_x, -20000, 20000) / 20000.0f; }
        if (std::abs(right_axis_y) >= 6000)
        {
            camera_height_ += GameUtil::clamp(right_axis_y, -20000, 20000) / 20000.0f
                * free_camera_distance_ * PAPER_FREE_CAMERA_ROTATE_SPEED;
        }
        camera_angle_ += rotate * PAPER_FREE_CAMERA_ROTATE_SPEED;
        camera_height_ = std::clamp(camera_height_ + height_delta * PAPER_FREE_CAMERA_HEIGHT_SPEED,
            PAPER_CAMERA_MIN_HEIGHT, PAPER_CAMERA_MAX_HEIGHT);
        free_camera_distance_ = std::clamp(free_camera_distance_, PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);
        camera_distance_ = free_camera_distance_;
    }
}
Pointf BattleSceneAct::getPaperMoveDirection(float input_right, float input_forward) const
{
    Pointf view_direction = camera_.center - camera_.pos;
    view_direction.z = 0;
    if (view_direction.norm() == 0)
    {
        view_direction = { 0, 1, 0 };
    }
    view_direction.normTo(1);
    Pointf paper_right = { view_direction.y, -view_direction.x, 0 };
    return paper_right * input_right + view_direction * input_forward;
}

Role* BattleSceneAct::findNearestEnemy(int team, Pointf p)
{
    Role* nearest = nullptr;
    double nearest_distance = std::numeric_limits<double>::max();
    for (auto role : battle_roles_)
    {
        if (role && role->Team != team && role->Dead == 0)
        {
            double distance = EuclidDis(role->Pos, p);
            if (distance < nearest_distance)
            {
                nearest = role;
                nearest_distance = distance;
            }
        }
    }
    return nearest;
}

void BattleSceneAct::setID(int id)
{
    battle_id_ = id;
    info_ = BattleMap::getInstance()->getBattleInfo(id);

    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 0, &earth_layer_);
    BattleMap::getInstance()->copyLayerData(info_->BattleFieldID, 1, &building_layer_);
}

void BattleSceneAct::drawPaperPresentation()
{
    //在这个模式下，使用的是直角坐标
    Engine::getInstance()->setRenderTarget("scene");
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, render_center_x_ * 2, render_center_y_ * 2);

    //以下是计算出需要画的区域，先画到一个大图上，再转贴到窗口
    {
        auto p = pos90To45(pos_.x, pos_.y);
        man_x_ = p.x;
        man_y_ = p.y;
    }
    //一整块地面
    auto earth_texture = Engine::getInstance()->getTexture("earth");
    auto earth_texture2 = Engine::getInstance()->getTexture("earth2");
    if (earth_texture && earth_texture2)
    {
        std::vector<FPoint> vf;
        //Engine::getInstance()->setRenderTarget(earth_texture);
        //Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_H * 2);
        //for (int sum = -view_sum_region_ - 3; sum <= view_sum_region_ + 13; sum++)
        //{
        //    for (int i = -view_width_region_; i <= view_width_region_; i++)
        //    {
        //        int ix = man_x_ + i + (sum / 2);
        //        int iy = man_y_ - i + (sum - sum / 2);
        //        auto p = pos45To90(ix, iy);
        //        if (!isOutLine(ix, iy))
        //        {
        //            int num = earth_layer_.data(ix, iy) / 2;
        //            Color color = { 255, 255, 255, 255 };
        //            bool need_draw = true;
        //            if (need_draw && num > 0)
        //            {
        //                TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y / 2, color);
        //            }
        //        }
        //    }
        //}

        //地面的z轴为0

        Engine::getInstance()->setRenderTarget(earth_texture2);
        Engine::getInstance()->fillColor({ 0, 0, 0, 0 }, 0, 0, COORD_COUNT * TILE_W * 2, COORD_COUNT * TILE_W * 2, BLENDMODE_NONE);
        {
            float wf = float(COORD_COUNT * TILE_W * 2);
            float hf = float(COORD_COUNT * TILE_W * 2);
            float ground_texture_wf = wf;
            float ground_texture_hf = hf;
            {
                int w, h;
                Engine::getInstance()->getTextureSize(earth_texture, w, h);
                ground_texture_wf = float(std::max(1, w));
                ground_texture_hf = float(std::max(1, h));
            }
            float ground_source_offset_x = std::max(0.0f, (ground_texture_wf - wf) * 0.5f);
            float ground_source_offset_y = std::max(0.0f, (ground_texture_hf - hf) * 0.5f);
            auto x1 = wf / 2;
            auto y1 = hf / 2;
            if (camera_focus_.norm() == 0)
            {
                camera_focus_ = { x1, y1, 0 };
            }
            Role* locked_role = role_ && camera_locked_ ? findNearestEnemy(role_->Team, role_->Pos) : nullptr;
            if (locked_role)
            {
                camera_focus_ = { locked_role->Pos.x, locked_role->Pos.y, 0 };
            }
            else if (role_)
            {
                camera_focus_ = { role_->Pos.x, role_->Pos.y, 0 };
            }
            Pointf camera_dir;
            if (camera_locked_)
            {
                camera_dir = Pointf{ role_->Pos.x - camera_focus_.x, role_->Pos.y - camera_focus_.y, 0 };
                if (camera_dir.norm() == 0)
                {
                    camera_dir = role_->RealTowards;
                    camera_dir.z = 0;
                    if (camera_dir.norm() == 0)
                    {
                        camera_dir = { 0, 1, 0 };
                    }
                }
                camera_dir.normTo(PAPER_CAMERA_DEFAULT_DISTANCE);
                camera_distance_ = PAPER_CAMERA_DEFAULT_DISTANCE;
                camera_angle_ = std::atan2(camera_dir.y, camera_dir.x);
                camera_pos_ = { role_->Pos.x + camera_dir.x, role_->Pos.y + camera_dir.y, camera_height_ };
            }
            else
            {
                if (camera_distance_ <= 0)
                {
                    camera_distance_ = PAPER_CAMERA_DEFAULT_DISTANCE;
                }
                camera_height_ = std::clamp(camera_height_, PAPER_CAMERA_MIN_HEIGHT, PAPER_CAMERA_MAX_HEIGHT);
                camera_dir = { std::cos(camera_angle_) * camera_distance_, std::sin(camera_angle_) * camera_distance_, 0 };
                camera_pos_ = { camera_focus_.x + camera_dir.x, camera_focus_.y + camera_dir.y, camera_height_ };
            }
            camera_.center = camera_focus_;
            camera_.pos = camera_pos_;
            if (paper_shake_ > 0)
            {
                Pointf shake_offset = { float(rand_.rand_int(3) - rand_.rand_int(3)) * 2.0f,
                    float(rand_.rand_int(3) - rand_.rand_int(3)) * 2.0f, 0 };
                camera_.center += shake_offset;
                camera_.pos += shake_offset;
                paper_shake_--;
            }
            camera_height_ = camera_pos_.z;
            if (!camera_locked_)
            {
                camera_distance_ = Pointf{ camera_pos_.x - camera_focus_.x, camera_pos_.y - camera_focus_.y, 0 }.norm();
                free_camera_distance_ = camera_distance_;
            }
            camera_angle_ = std::atan2(camera_pos_.y - camera_focus_.y, camera_pos_.x - camera_focus_.x);
            camera_.setFov(PaperSky::CameraFov);
            camera_.setViewport(float(render_center_x_ * 2), float(render_center_y_ * 2));
            Pointf view_dir = camera_.center - camera_.pos;
            view_dir.z = 0;
            if (view_dir.norm() == 0)
            {
                view_dir = { 0, 1, 0 };
            }
            view_dir.normTo(1);
            Pointf paper_right = { view_dir.y, -view_dir.x, 0 };
            auto calc_depth = [&](const Pointf& p)
            {
                auto v = p - camera_.pos;
                auto n = view_dir;
                return v.x * n.x + v.y * n.y;
            };
            using PaperSprite = PaperRenderSprite;

            auto to_paper_anchor = [](Pointf p)
            {
                return Pointf{ p.x, p.y, p.z };
            };

            std::vector<PaperSprite> sprites;
            int sprite_order = 0;
            auto push_sprite = [&](PaperSprite sprite)
            {
                sprite.order = sprite_order++;
                sprites.emplace_back(std::move(sprite));
            };
            for (auto r : battle_roles_)
            {
                if (!r)
                {
                    continue;
                }
                PaperSprite sprite;
                if (r->RealTowards.norm() > 0.001)
                {
                    r->FaceTowards = realTowardsToCameraFaceTowards(r->RealTowards, view_dir, paper_right, r->FaceTowards);
                }
                sprite.tex = TextureManager::getInstance()->getTexture(std::format("fight/fight{:03}", r->HeadID), calRolePic(r, r->ActType, r->ActFrame));
                sprite.anchor = to_paper_anchor(r->Pos);
                sprite.alpha = r->Attention ? 255 - r->Attention * 4 : 255;
                sprite.turn = r->Dead ? 0 : 1;
                if (r->Dead || r->HP <= 0)
                {
                    sprite.rotation = r->FaceTowards >= 2 ? 90 : 270;
                    sprite.turn = 0;
                }
                if (battle_cursor_->isRunning() && !acting_role_->isAuto())
                {
                    sprite.color = inEffect(acting_role_, r) ? Color{ 255, 255, 255, 255 } : Color{ 128, 128, 128, 255 };
                }
                if (result_ == -1 && r->Shake)
                {
                    sprite.anchor.x += -2.5 + rand_.rand() * 5;
                }
                sprite.depth = calc_depth(sprite.anchor);
                push_sprite(std::move(sprite));
            }

            for (auto& ae : attack_effects_)
            {
                PaperSprite sprite;
                int frame = ae.TotalEffectFrame > 0 ? ae.Frame % ae.TotalEffectFrame : 0;
                sprite.tex = TextureManager::getInstance()->getTexture(ae.Path, frame);
                sprite.anchor = to_paper_anchor(ae.FollowRole ? ae.FollowRole->Pos : ae.Pos);
                float fade_duration = std::max(1.0f, ae.TotalFrame * 1.25f);
                float alpha = 255.0f * (fade_duration - ae.Frame) / fade_duration;
                sprite.alpha = uint8_t(std::clamp(alpha, 0.0f, 255.0f));
                sprite.depth = calc_depth(sprite.anchor);
                sprite.face_camera = true;
                push_sprite(std::move(sprite));
            }

            PaperFrame paper_frame;
            paper_frame.camera = &camera_;
            paper_frame.sprites = std::move(sprites);
            if (locked_role && locked_role->Dead == 0 && locked_role->HP > 0)
            {
                paper_frame.locked_position = &locked_role->Pos;
            }
            paper_frame.role_info_anchors.reserve(battle_roles_.size());
            for (auto r : battle_roles_)
            {
                if (!r)
                {
                    continue;
                }
                paper_frame.role_info_anchors.push_back({ r, r->Pos });
            }
            paper_frame.text_effects.reserve(text_effects_.size());
            for (const auto& te : text_effects_)
            {
                paper_frame.text_effects.push_back({ te.PaperPos, te.Text, te.Size, te.PaperRise, te.color });
            }
            paper_frame.current_frame = current_frame_;
            paper_frame.tile_width = TILE_W;
            paper_frame.tile_width_base = TILE_W_0;
            paper_frame.lock_texture_id = PAPER_LOCK_MARK_TEXTURE;
            paper_frame.role_info_top_gap = PAPER_ROLE_INFO_TOP_GAP;
            paper_frame.scene_width = render_center_x_ * 2;
            paper_frame.scene_height = render_center_y_ * 2;
            paper_frame.render_role_info = [this](Role* role, int x, int y) { renderExtraRoleInfo(role, x, y); };
            paper_presentation_.renderScene(Engine::getInstance(), paper_frame);
            Engine::getInstance()->renderTextureToMain("scene");
            if (sword_light_)
            {
                auto tex = TextureManager::getInstance()->getTexture("title", 203);
                if (tex)
                {
                    int w = tex->w;
                    int h = tex->h;
                    double zoom_max = 1.0 * Engine::getInstance()->getUIWidth() / w;
                    double zoom = zoom_max * sword_light_ / 20;
                    w = int(w * zoom);
                    h = int(h * zoom);
                    int x = Engine::getInstance()->getUIWidth() / 2 - w / 2;
                    int y = Engine::getInstance()->getUIHeight() / 2 - h / 2 + swordLightYOffset();
                    TextureManager::getInstance()->renderTexture("title", 203, x, y,
                        { sword_light_color_, 255, zoom, zoom, 0, 0 });
                }
            }
            return;
        }

        struct DrawInfo
        {
            std::string path;
            int num;
            Pointf p;
            Color color{ 255, 255, 255, 255 };
            uint8_t alpha = 255;
            int rot = 0, rot2 = 0;
            int shadow = 0;
            uint8_t white = 0;
            int breathless = 0;
            int draw_turn = 1;    //主要为了被击倒的画到后面
        };

        std::vector<DrawInfo> draw_infos;
        draw_infos.reserve(10000);

        for (int sum = -view_sum_region_ - 2; sum <= view_sum_region_ + 13; sum++)
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
                        //TextureManager::getInstance()->renderTexture("smap", num, p.x, p.y/2);
                        DrawInfo info;
                        info.path = "smap";
                        info.num = num;
                        info.p.x = p.x;
                        info.p.y = p.y;
                        info.shadow = 0;
                        //draw_infos.emplace_back(std::move(info));
                    }
                }
            }
        }

        for (auto r : battle_roles_)
        {
            //if (r->Dead) { continue; }
            DrawInfo info;
            info.path = std::format("fight/fight{:03}", r->HeadID);
            info.color = { 255, 255, 255, 255 };
            info.alpha = 255;
            info.white = 0;
            if (battle_cursor_->isRunning() && !acting_role_->isAuto())
            {
                info.color = { 128, 128, 128, 255 };
                if (inEffect(acting_role_, r))
                {
                    info.color = { 255, 255, 255, 255 };
                }
            }
            info.p = r->Pos;
            if (result_ == -1 && r->Shake)
            {
                info.p.x += -2.5 + rand_.rand() * 5;
            }
            r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
            info.num = calRolePic(r, r->ActType, r->ActFrame);
            //if (r->HurtFrame)
            //{
            //    if (r->HurtFrame % 2 == 1)
            //    {
            //        info.white = 128;
            //    }
            //}
            if (r->Dead)
            {
                //if (r->Frozen == 0)
                {
                    if (r->FaceTowards >= 2)
                    {
                        info.rot = 90;
                    }
                    else
                    {
                        info.rot = 270;
                    }
                }
                info.draw_turn = 0;
            }
            if (r->Attention)
            {
                info.alpha = 255 - r->Attention * 4;
            }
            info.shadow = 1;

            if (r->Breathless && r->HP > 0)
            {
                info.breathless = 1;
                info.rot2 = r->CoolDown;
            }
            //TextureManager::getInstance()->renderTexture(path, pic, r->X1, r->Y1, color, alpha);
            draw_infos.emplace_back(std::move(info));
        }

        //effects
        for (auto& ae : attack_effects_)
        {
            //for (auto r : ae.Defender)
            {
                DrawInfo info;
                info.path = ae.Path;
                if (ae.TotalEffectFrame > 0)
                {
                    info.num = ae.Frame % ae.TotalEffectFrame;
                }
                else
                {
                    info.num = 0;
                }
                info.p = ae.Pos;
                info.color = { 255, 255, 255, 255 };
                info.alpha = 192;
                if (ae.FollowRole)
                {
                    info.p = ae.FollowRole->Pos;
                }
                info.shadow = 1;
                if (ae.Attacker && ae.Attacker->Team == 0)
                {
                    info.shadow = 2;
                }
                info.alpha = 255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25);    //越来越透明
                draw_infos.emplace_back(std::move(info));
                //TextureManager::getInstance()->renderTexture(ae.Path, ae.Frame % ae.TotalEffectFrame, ae.X1, ae.Y1 / 2, { 255, 255, 255, 255 }, 192);
            }
        }

        auto sort_building = [](DrawInfo& d1, DrawInfo& d2)
        {
            if (d1.p.y != d2.p.y)
            {
                return d1.p.y < d2.p.y;
            }
            else
            {
                return d1.p.x < d2.p.x;
            }
        };
        std::sort(draw_infos.begin(), draw_infos.end(), sort_building);

        //影子
        for (auto& d : draw_infos)
        {
            if (d.shadow)
            {
                auto tex = TextureManager::getInstance()->getTexture(d.path, d.num);
                if (tex)
                {
                    double scalex = 1, scaley = 0.3;
                    int yd = tex->dy * 0.7;
                    if (d.rot)
                    {
                        scalex = 0.3;
                        scaley = 1;
                        yd = tex->dy * 0.1;
                    }
                    if (d.shadow == 1)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd,
                            { { 32, 32, 32, 255 }, uint8_t(d.alpha / 2), scalex, scaley, double(d.rot) });
                    }
                    if (d.shadow == 2)
                    {
                        TextureManager::getInstance()->renderTexture(tex, d.p.x, d.p.y / 2 + yd,
                            { { 128, 128, 128, 255 }, uint8_t(d.alpha / 2), scalex, scaley, double(d.rot2), 128 });
                    }
                }
            }
        }
        if (!vf.empty())
        {
            Engine::getInstance()->renderTexture(earth_texture2, nullptr, vf, { });
        }
        for (int turn = 0; turn < 2; turn++)
        {
            for (auto& d : draw_infos)
            {
                if (d.draw_turn != turn) { continue; }
                double scaley = 1;
                if (d.rot)
                {
                    scaley = 0.5;
                }
                Engine::getInstance()->setRenderTarget(earth_texture2);
                TextureManager::getInstance()->renderTexture(d.path, d.num, d.p.x, d.p.y / 2 - d.p.z,
                    { d.color, d.alpha, scaley, 1, double(d.rot), d.white });

                auto tex = TextureManager::getInstance()->getTexture(d.path, d.num);
                if (!tex)
                {
                    continue;
                }
                tex->load();
                if (!tex->getTexture())
                {
                    continue;
                }

                FRect rect = { d.p.x, d.p.y / 2 - d.p.z, float(tex->w), float(tex->h) };

                std::vector<Pointf> v;
                v.push_back({ d.p.x, d.p.y, float(tex->h) });
                v.push_back({ d.p.x + tex->w, d.p.y, float(tex->h) });
                v.push_back({ d.p.x + tex->w, d.p.y, 0 });
                v.push_back({ d.p.x, d.p.y, 0 });
                if (role_)
                {
                    //camera_.pos = { role_->Pos.x + cos(camera_angle_) * 2000, role_->Pos.y + sin(camera_angle_) * 2000, 1500.0f };
                    auto v2 = camera_.getProj(v);

                    std::vector<FPoint> vf;
                    for (auto& p : v2)
                    {
                        vf.push_back({ float(p.x), float(p.y) });
                    }
                    std::vector<FPoint> vr;
                    Engine::getInstance()->setRenderTarget("scene");
                    Engine::getInstance()->renderTexture(tex->getTexture(), nullptr, vf, vr);
                    //Engine::getInstance()->setRenderTarget(earth_texture2);
                }

                if (d.breathless)
                {
                    TextureManager::getInstance()->renderTexture("title", 205, d.p.x - 5, d.p.y / 2 - d.p.z - 36,
                        { { 255, 255, 255, 255 }, 255, 0.1, 0.1, double(d.rot2), 0 });
                }
            }
        }

        for (auto r : battle_roles_)
        {
            renderExtraRoleInfo(r, r->Pos.x, r->Pos.y / 2);
        }

        Color c = { 255, 255, 255, 255 };
        Engine::getInstance()->setColor(earth_texture, c);
        int w = render_center_x_ * 2;
        int h = render_center_y_ * 2;
        //获取的是中心位置，如贴图应减掉屏幕尺寸的一半
        Rect rect0 = { int(pos_.x - render_center_x_ - x_), int(pos_.y / 2 - render_center_y_ - y_), w, h };
        Rect rect1 = { 0, 0, w, h };
        if (rect0.x < 0)
        {
            rect1.x = -rect0.x;
            rect0.x = 0;
            rect0.w = w - rect1.x;
        }
        if (rect0.y < 0)
        {
            rect1.y = -rect0.y;
            rect0.y = 0;
            rect0.h = h - rect1.y;
        }

        rect0.w = std::min(rect0.w, COORD_COUNT * TILE_W * 2 - rect0.x);
        rect0.h = std::min(rect0.h, COORD_COUNT * TILE_H * 2 - rect0.y);
        rect1.w = rect0.w;
        rect1.h = rect0.h;
        rect0.y -= 40;    //为了刀光在正中，往下调一点
        for (auto& te : text_effects_)
        {
            Font::getInstance()->draw(te.Text, te.Size, te.Pos.x, te.Pos.y / 2, te.color, 255);
        }
        Engine::getInstance()->setRenderTarget("scene");
        if (close_up_)
        {
            rect0.w /= 2;
            rect0.h /= 2;
            rect0.x += rect0.w / 2;
            rect0.y += rect0.h / 2;
        }
    }

    Engine::getInstance()->renderTextureToMain("scene");

    if (sword_light_)
    {
        //刀光直接画在正中间
        //20最大
        int w = TextureManager::getInstance()->getTexture("title", 203)->w;
        int h = TextureManager::getInstance()->getTexture("title", 203)->h;
        double zoom_max = 1.0 * Engine::getInstance()->getUIWidth() / w;
        double zoom = zoom_max * sword_light_ / 20;
        w *= zoom;
        h *= zoom;
        int x = Engine::getInstance()->getUIWidth() / 2 - w / 2;
        int y = Engine::getInstance()->getUIHeight() / 2 - h / 2;
        TextureManager::getInstance()->renderTexture("title", 203, x, y,
            { sword_light_color_, 255, zoom, zoom, 0, 0 });
        //if (sword_light_ > 30)
        //{
        //    int w1 = TextureManager::getInstance()->getTexture("title", 204)->w;
        //    int h1 = TextureManager::getInstance()->getTexture("title", 204)->h;
        //    double zoom_max1 = 1.0 * Engine::getInstance()->getPresentWidth() / 2 / w1;
        //    double zoom1 = zoom_max1;
        //    w1 *= zoom1;
        //    h1 *= zoom1;
        //    int x1 = Engine::getInstance()->getPresentWidth() / 2 - w1 / 2;
        //    int y1 = Engine::getInstance()->getPresentHeight() / 2 - h1 / 2;
        //    TextureManager::getInstance()->renderTexture("title", 204, x1, y1, { 255, 255, 255, 255 }, 128, zoom1, zoom1, 0, 0);
        //}
    }
    //if (result_ >= 0)
    //{
    //    Engine::getInstance()->fillColor({ 0, 0, 0, 128 }, 0, 0, -1, -1);
    //}
}


int BattleSceneAct::realTowardsToCameraFaceTowards(const Pointf& dir, const Pointf& view_dir, const Pointf& paper_right, int current_face_towards)
{
    Pointf n = dir;
    n.z = 0;
    if (n.norm() == 0)
    {
        return current_face_towards;
    }
    n.normTo(1);
    float right = n.x * paper_right.x + n.y * paper_right.y;
    float forward = n.x * view_dir.x + n.y * view_dir.y;
    constexpr float right_epsilon = 0.2f;
    if (std::abs(right) < right_epsilon)
    {
        bool keep_right = current_face_towards == Towards_RightUp || current_face_towards == Towards_RightDown;
        return keep_right ? (forward > 0 ? Towards_RightUp : Towards_RightDown)
                          : (forward > 0 ? Towards_LeftUp : Towards_LeftDown);
    }
    if (right > 0 && forward > 0) { return Towards_RightUp; }
    if (right > 0 && forward <= 0) { return Towards_RightDown; }
    if (right < 0 && forward >= 0) { return Towards_LeftUp; }
    if (right < 0 && forward < 0) { return Towards_LeftDown; }
    return Towards_None;
}
