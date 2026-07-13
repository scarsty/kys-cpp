#include "BattleSceneAct.h"
#include "GameUtil.h"
#include "filefunc.h"
#include <limits>

namespace
{
constexpr float PAPER_GROUND_EXTENSION_MARGIN_RATIO = 1.0f;
constexpr float PAPER_GROUND_MESH_CENTER_RATIO = 0.64f;
constexpr int PAPER_GROUND_MESH_TARGET_DIVISIONS = 160;
constexpr int PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE = 10;
constexpr int PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE = 18;
constexpr float PAPER_GROUND_EDGE_FADE_TILE_COUNT = 16.0f;
constexpr float PAPER_GROUND_HORIZON_FADE_START_RATIO = 0.72f;
constexpr float PAPER_GROUND_HORIZON_FADE_END_RATIO = 1.35f;
constexpr float PAPER_GROUND_RADIAL_FADE_START_RATIO = 0.34f;
constexpr float PAPER_GROUND_RADIAL_FADE_END_RATIO = 0.48f;
constexpr uint8_t PAPER_GROUND_EDGE_MIN_ALPHA = 0;
constexpr uint8_t PAPER_GROUND_HORIZON_MIN_ALPHA = 0;
constexpr int PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR = 16;
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

void BattleSceneAct::initializePaperPresentation()
{
    paper_sky_.reset();
    paper_sky_.generateClouds();

    const int base_earth_w = COORD_COUNT * TILE_W * 2;
    const int base_earth_h = COORD_COUNT * TILE_H * 2;
    const int paper_earth_size = COORD_COUNT * TILE_W * 2;
    const int requested_extension = 0;
    const int square_fill_extension = COORD_COUNT;
    auto engine = Engine::getInstance();
    int extension = requested_extension;
    int earth_base_w = base_earth_w;
    int earth_base_h = base_earth_h;
    int paper_earth_w = paper_earth_size;
    int paper_earth_h = paper_earth_size;
    Texture* earth_base_texture = nullptr;
    Texture* earth_texture = nullptr;
    for (;;)
    {
        earth_base_w = base_earth_w + extension * TILE_W * 4;
        earth_base_h = base_earth_h + extension * TILE_H * 4;
        paper_earth_w = paper_earth_size + extension * TILE_W * 4;
        paper_earth_h = paper_earth_size + extension * TILE_W * 4;
        engine->createRenderedTexture("earth_base", earth_base_w, earth_base_h);
        engine->createRenderedTexture("earth", paper_earth_w, paper_earth_h);
        earth_base_texture = engine->getTexture("earth_base");
        earth_texture = engine->getTexture("earth");
        if ((earth_base_texture && earth_texture) || extension == 0)
        {
            break;
        }
        extension /= 2;
    }

    int actual_base_earth_w = earth_base_w;
    int actual_base_earth_h = earth_base_h;
    int actual_paper_earth_w = paper_earth_w;
    int actual_paper_earth_h = paper_earth_h;
    if (earth_base_texture)
    {
        engine->getTextureSize(earth_base_texture, actual_base_earth_w, actual_base_earth_h);
    }
    if (earth_texture)
    {
        engine->getTextureSize(earth_texture, actual_paper_earth_w, actual_paper_earth_h);
    }
    const int base_offset_x = std::max(0, (actual_base_earth_w - base_earth_w) / 2);
    const int base_offset_y = std::max(0, (actual_base_earth_h - base_earth_h) / 2);
    if (earth_base_texture)
    {
        Engine::setTextureBlendMode(earth_base_texture);
        SDL_SetTextureScaleMode(earth_base_texture, SDL_SCALEMODE_NEAREST);
        engine->setRenderTarget(earth_base_texture);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, actual_base_earth_w, actual_base_earth_h, BLENDMODE_NONE);

        auto texture_manager = TextureManager::getInstance();
        struct GroundTile
        {
            TextureWarpper* texture = nullptr;
            int ix = 0;
            int iy = 0;
        };
        std::vector<GroundTile> ground_tiles;
        ground_tiles.reserve(COORD_COUNT * COORD_COUNT);
        auto tile_index = [&](int x, int y)
        {
            return x + y * COORD_COUNT;
        };
        std::vector<int> ground_tile_indices(COORD_COUNT * COORD_COUNT, -1);
        std::vector<int> row_first_valid(COORD_COUNT, -1);
        std::vector<int> row_last_valid(COORD_COUNT, -1);
        std::vector<int> column_first_valid(COORD_COUNT, -1);
        std::vector<int> column_last_valid(COORD_COUNT, -1);
        for (int iy = 0; iy < COORD_COUNT; iy++)
        {
            for (int ix = 0; ix < COORD_COUNT; ix++)
            {
                int num = earth_layer_.data(ix, iy) / 2;
                auto texture = num > 0 ? texture_manager->getTexture("smap", num) : nullptr;
                if (!texture)
                {
                    continue;
                }
                int source_index = int(ground_tiles.size());
                ground_tiles.push_back({ texture, ix, iy });
                ground_tile_indices[tile_index(ix, iy)] = source_index;
                if (row_first_valid[iy] < 0) { row_first_valid[iy] = ix; }
                row_last_valid[iy] = ix;
                if (column_first_valid[ix] < 0) { column_first_valid[ix] = iy; }
                column_last_valid[ix] = iy;
            }
        }
        std::vector<int> edge_tile_indices;
        constexpr int neighbor_offsets[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
        for (int index = 0; index < int(ground_tiles.size()); index++)
        {
            const auto& tile = ground_tiles[index];
            for (const auto& offset : neighbor_offsets)
            {
                int x = tile.ix + offset[0];
                int y = tile.iy + offset[1];
                if (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT
                    || ground_tile_indices[tile_index(x, y)] < 0)
                {
                    edge_tile_indices.push_back(index);
                    break;
                }
            }
        }
        if (edge_tile_indices.empty())
        {
            for (int index = 0; index < int(ground_tiles.size()); index++)
            {
                edge_tile_indices.push_back(index);
            }
        }
        auto get_original_tile_index = [&](int x, int y)
        {
            if (x < 0 || x >= COORD_COUNT || y < 0 || y >= COORD_COUNT)
            {
                return -1;
            }
            return ground_tile_indices[tile_index(x, y)];
        };
        auto find_nearest_edge_tile = [&](int x, int y)
        {
            int best_index = -1;
            int best_distance = std::numeric_limits<int>::max();
            for (int source_index : edge_tile_indices)
            {
                const auto& tile = ground_tiles[source_index];
                int dx = tile.ix - x;
                int dy = tile.iy - y;
                int distance = dx * dx + dy * dy;
                if (distance < best_distance)
                {
                    best_distance = distance;
                    best_index = source_index;
                }
            }
            return best_index;
        };
        auto get_extension_source_index = [&](int x, int y)
        {
            int source_index = get_original_tile_index(x, y);
            if (source_index >= 0)
            {
                return source_index;
            }
            int source_x = std::clamp(x, 0, COORD_COUNT - 1);
            int source_y = std::clamp(y, 0, COORD_COUNT - 1);
            if (y < 0 && column_first_valid[source_x] >= 0) { source_y = column_first_valid[source_x]; }
            else if (y >= COORD_COUNT && column_last_valid[source_x] >= 0) { source_y = column_last_valid[source_x]; }
            if (x < 0 && row_first_valid[source_y] >= 0) { source_x = row_first_valid[source_y]; }
            else if (x >= COORD_COUNT && row_last_valid[source_y] >= 0) { source_x = row_last_valid[source_y]; }
            source_index = get_original_tile_index(source_x, source_y);
            return source_index >= 0 ? source_index : find_nearest_edge_tile(x, y);
        };
        for (int iy = -square_fill_extension; iy < COORD_COUNT + square_fill_extension; iy++)
        {
            for (int ix = -square_fill_extension; ix < COORD_COUNT + square_fill_extension; ix++)
            {
                int source_index = get_extension_source_index(ix, iy);
                if (source_index < 0)
                {
                    continue;
                }
                auto texture = ground_tiles[source_index].texture;
                texture->load();
                auto source_texture = texture->getTexture();
                if (!source_texture)
                {
                    continue;
                }
                Engine::setColor(source_texture, { 255, 255, 255, 255 });
                auto position = pos45To90(ix, iy);
                engine->renderTexture(source_texture, base_offset_x + int(position.x) - texture->dx,
                    base_offset_y + int(std::round(position.y * TILE_H / float(TILE_W))) - texture->dy, texture->w, texture->h);
            }
        }

        auto battle_earth_group = texture_manager->getTextureGroup("battle-earth");
        std::string battle_earth_filename;
        if (battle_earth_group->getTextureCount() > 0)
        {
            battle_earth_filename = std::to_string(info_->BattleFieldID) + battle_earth_group->info_.ext_;
        }
        if (!battle_earth_filename.empty() && !battle_earth_group->getFileContent(battle_earth_filename).empty())
        {
            auto texture = texture_manager->getTexture("battle-earth", info_->BattleFieldID);
            if (texture)
            {
                texture->load();
                auto source_texture = texture->getTexture();
                if (source_texture)
                {
                    Engine::setColor(source_texture, { 255, 255, 255, 255 });
                    int source_w = texture->w;
                    int source_h = texture->h;
                    engine->getTextureSize(source_texture, source_w, source_h);
                    int center_x = base_offset_x - texture->dx;
                    int center_y = base_offset_y - texture->dy;
                    int strip_w = std::clamp(source_w / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, source_w);
                    int strip_h = std::clamp(source_h / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, source_h);
                    auto render_piece = [&](Rect source, Rect destination)
                    {
                        if (source.w > 0 && source.h > 0 && destination.w > 0 && destination.h > 0)
                        {
                            engine->renderTexture(source_texture, &source, &destination);
                        }
                    };
                    render_piece({ 0, 0, source_w, source_h }, { center_x, center_y, base_earth_w, base_earth_h });
                    render_piece({ 0, 0, strip_w, source_h }, { 0, center_y, center_x, base_earth_h });
                    render_piece({ source_w - strip_w, 0, strip_w, source_h },
                        { center_x + base_earth_w, center_y, actual_base_earth_w - center_x - base_earth_w, base_earth_h });
                    render_piece({ 0, 0, source_w, strip_h }, { center_x, 0, base_earth_w, center_y });
                    render_piece({ 0, source_h - strip_h, source_w, strip_h },
                        { center_x, center_y + base_earth_h, base_earth_w, actual_base_earth_h - center_y - base_earth_h });
                    render_piece({ 0, 0, strip_w, strip_h }, { 0, 0, center_x, center_y });
                    render_piece({ source_w - strip_w, 0, strip_w, strip_h },
                        { center_x + base_earth_w, 0, actual_base_earth_w - center_x - base_earth_w, center_y });
                    render_piece({ 0, source_h - strip_h, strip_w, strip_h },
                        { 0, center_y + base_earth_h, center_x, actual_base_earth_h - center_y - base_earth_h });
                    render_piece({ source_w - strip_w, source_h - strip_h, strip_w, strip_h },
                        { center_x + base_earth_w, center_y + base_earth_h,
                            actual_base_earth_w - center_x - base_earth_w, actual_base_earth_h - center_y - base_earth_h });
                }
            }
        }
    }

    if (earth_texture && earth_base_texture)
    {
        Engine::setTextureBlendMode(earth_texture);
        SDL_SetTextureScaleMode(earth_texture, SDL_SCALEMODE_LINEAR);
        engine->setRenderTarget(earth_texture);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, actual_paper_earth_w, actual_paper_earth_h, BLENDMODE_NONE);
        Engine::setColor(earth_base_texture, { 255, 255, 255, 255 });
        Rect source = { 0, 0, actual_base_earth_w, actual_base_earth_h };
        Rect destination = { 0, 0, actual_paper_earth_w, actual_paper_earth_h };
        engine->renderTexture(earth_base_texture, &source, &destination);
    }

    engine->createRenderedTexture("earth2", paper_earth_size, paper_earth_size);
    if (auto earth_texture2 = engine->getTexture("earth2"))
    {
        engine->setRenderTarget(earth_texture2);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, paper_earth_size, paper_earth_size, BLENDMODE_NONE);
    }
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
        if (engine->checkKeyPress(K_LEFT)) { rotate -= 1; }
        if (engine->checkKeyPress(K_RIGHT)) { rotate += 1; }
        if (engine->checkKeyPress(K_UP)) { height_delta += 1; }
        if (engine->checkKeyPress(K_DOWN)) { height_delta -= 1; }
        auto right_axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTX);
        auto right_axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTY);
        if (std::abs(right_axis_x) >= 6000) { rotate += GameUtil::clamp(right_axis_x, -20000, 20000) / 20000.0f; }
        if (std::abs(right_axis_y) >= 6000) { height_delta += GameUtil::clamp(-right_axis_y, -20000, 20000) / 20000.0f; }
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
        if (role_)
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
            Role* locked_role = camera_locked_ ? findNearestEnemy(role_->Team, role_->Pos) : nullptr;
            if (locked_role)
            {
                camera_focus_ = { locked_role->Pos.x, locked_role->Pos.y, 0 };
            }
            else
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
            Engine::getInstance()->setRenderTarget("scene");
            paper_sky_.render(Engine::getInstance(), render_center_x_ * 2, render_center_y_ * 2,
                camera_.pos, camera_.center);
            auto render_texture_3d = [&](Texture* texture, const std::vector<Pointf>& world, const std::vector<FPoint>& src, Color color = { 255, 255, 255, 255 })
            {
                auto projected = camera_.getProj(world);
                std::vector<FPoint> dst;
                dst.reserve(projected.size());
                for (auto& p : projected)
                {
                    dst.push_back({ float(p.x), float(p.y) });
                }
                std::vector<Color> colors(dst.size(), color);
                std::vector<int> indices = { 0, 1, 2, 2, 3, 0 };
                Engine::getInstance()->renderTextureMesh(texture, dst, src, colors, indices);
            };
            auto render_texture_3d_mesh = [&](Texture* texture, const std::vector<Pointf>& world,
                const std::vector<FPoint>& src, int columns, int rows, Color color = { 255, 255, 255, 255 })
            {
                if (!texture || world.size() != 4 || src.size() != 4)
                {
                    return;
                }
                columns = std::clamp(columns, 1, 32);
                rows = std::clamp(rows, 1, 32);
                std::vector<FPoint> dst;
                std::vector<FPoint> mesh_src;
                std::vector<int> indices;
                dst.reserve((columns + 1) * (rows + 1));
                mesh_src.reserve((columns + 1) * (rows + 1));
                indices.reserve(columns * rows * 6);

                for (int row = 0; row <= rows; ++row)
                {
                    float v = float(row) / rows;
                    for (int column = 0; column <= columns; ++column)
                    {
                        float u = float(column) / columns;
                        Pointf top = world[0] * (1.0f - u) + world[1] * u;
                        Pointf bottom = world[3] * (1.0f - u) + world[2] * u;
                        auto projected = camera_.getProj({ top * (1.0f - v) + bottom * v }).front();
                        dst.push_back({ projected.x, projected.y });

                        FPoint source_top = {
                            src[0].x + (src[1].x - src[0].x) * u,
                            src[0].y + (src[1].y - src[0].y) * u,
                        };
                        FPoint source_bottom = {
                            src[3].x + (src[2].x - src[3].x) * u,
                            src[3].y + (src[2].y - src[3].y) * u,
                        };
                        mesh_src.push_back({
                            source_top.x + (source_bottom.x - source_top.x) * v,
                            source_top.y + (source_bottom.y - source_top.y) * v,
                        });
                    }
                }

                for (int row = 0; row < rows; ++row)
                {
                    for (int column = 0; column < columns; ++column)
                    {
                        int top_left = row * (columns + 1) + column;
                        int top_right = top_left + 1;
                        int bottom_left = top_left + columns + 1;
                        int bottom_right = bottom_left + 1;
                        indices.insert(indices.end(), { top_left, top_right, bottom_right, bottom_right, bottom_left, top_left });
                    }
                }
                std::vector<Color> colors(dst.size(), color);
                Engine::getInstance()->renderTextureMesh(texture, dst, mesh_src, colors, indices);
            };
            auto in_camera_front = [&](const std::vector<Pointf>& points)
            {
                for (auto& p : points)
                {
                    if (camera_.getDepth(p) <= camera_.getNearPlane())
                    {
                        return false;
                    }
                }
                return true;
            };
            auto render_ground_mesh = [&]()
            {
                float min_x = -ground_source_offset_x;
                float min_y = -ground_source_offset_y;
                float max_x = ground_texture_wf - ground_source_offset_x;
                float max_y = ground_texture_hf - ground_source_offset_y;
                auto make_ground_mesh_lines = [&](float min_value, float max_value, float original_size)
                {
                    std::vector<float> lines;
                    lines.reserve(384);
                    auto add_line = [&](float value)
                    {
                        lines.push_back(std::clamp(value, min_value, max_value));
                    };
                    auto add_segment = [&](float start, float end, int divisions)
                    {
                        if (start > max_value || end < min_value)
                        {
                            return;
                        }
                        start = std::clamp(start, min_value, max_value);
                        end = std::clamp(end, min_value, max_value);
                        if (end < start)
                        {
                            return;
                        }
                        divisions = std::max(1, divisions);
                        for (int i = 0; i <= divisions; i++)
                        {
                            float factor = float(i) / divisions;
                            add_line(start + (end - start) * factor);
                        }
                    };

                    float center_margin = original_size * (1.0f - PAPER_GROUND_MESH_CENTER_RATIO) * 0.5f;
                    float original_min = 0.0f;
                    float original_max = original_size;
                    float center_min = original_min + center_margin;
                    float center_max = original_max - center_margin;
                    int center_divisions = std::max(16,
                        PAPER_GROUND_MESH_TARGET_DIVISIONS
                        - PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE * 2
                        - PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE * 2);

                    add_segment(min_value, original_min, PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE);
                    add_segment(original_min, center_min, PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE);
                    add_segment(center_min, center_max, center_divisions);
                    add_segment(center_max, original_max, PAPER_GROUND_MESH_ORIGINAL_EDGE_DIVISIONS_PER_SIDE);
                    add_segment(original_max, max_value, PAPER_GROUND_MESH_EXTENSION_DIVISIONS_PER_SIDE);

                    std::sort(lines.begin(), lines.end());
                    std::vector<float> unique_lines;
                    unique_lines.reserve(lines.size());
                    for (float value : lines)
                    {
                        if (unique_lines.empty() || std::abs(value - unique_lines.back()) > 0.25f)
                        {
                            unique_lines.push_back(value);
                        }
                    }
                    if (unique_lines.size() < 2)
                    {
                        unique_lines = { min_value, max_value };
                    }
                    return unique_lines;
                };
                std::vector<float> mesh_x_lines = make_ground_mesh_lines(min_x, max_x, wf);
                std::vector<float> mesh_y_lines = make_ground_mesh_lines(min_y, max_y, hf);
                int mesh_x = int(mesh_x_lines.size()) - 1;
                int mesh_y = int(mesh_y_lines.size()) - 1;
                float ground_edge_fade_distance = float(TILE_W) * PAPER_GROUND_EDGE_FADE_TILE_COUNT;
                float horizon_fade_start = std::max(ground_edge_fade_distance,
                    wf * PAPER_GROUND_HORIZON_FADE_START_RATIO);
                float horizon_fade_end = std::max(horizon_fade_start + ground_edge_fade_distance,
                    wf * PAPER_GROUND_HORIZON_FADE_END_RATIO);
                float ground_center_x = ground_texture_wf * 0.5f;
                float ground_center_y = ground_texture_hf * 0.5f;
                float ground_radial_base = std::min(ground_texture_wf, ground_texture_hf);
                float ground_radial_fade_start = ground_radial_base * PAPER_GROUND_RADIAL_FADE_START_RATIO;
                float ground_radial_fade_end = ground_radial_base * PAPER_GROUND_RADIAL_FADE_END_RATIO;

                std::vector<Pointf> world;
                std::vector<FPoint> src;
                std::vector<Color> colors;
                world.reserve(mesh_x_lines.size() * mesh_y_lines.size());
                src.reserve(mesh_x_lines.size() * mesh_y_lines.size());
                colors.reserve(mesh_x_lines.size() * mesh_y_lines.size());

                for (int iy = 0; iy < int(mesh_y_lines.size()); iy++)
                {
                    float y = mesh_y_lines[iy];
                    for (int ix = 0; ix < int(mesh_x_lines.size()); ix++)
                    {
                        float x = mesh_x_lines[ix];
                        float source_x = std::clamp(x + ground_source_offset_x, 0.0f, ground_texture_wf);
                        float source_y = std::clamp(y + ground_source_offset_y, 0.0f, ground_texture_hf);
                        float distance_to_edge = std::min(
                            std::min(source_x, ground_texture_wf - source_x),
                            std::min(source_y, ground_texture_hf - source_y));
                        float fade = PaperSky::smoothStep(0.0f, ground_edge_fade_distance, distance_to_edge);
                        float alpha = PAPER_GROUND_EDGE_MIN_ALPHA
                            + (255.0f - PAPER_GROUND_EDGE_MIN_ALPHA) * fade;
                        float center_dx = source_x - ground_center_x;
                        float center_dy = source_y - ground_center_y;
                        float center_distance = std::sqrt(center_dx * center_dx + center_dy * center_dy);
                        float radial_fade = 1.0f
                            - PaperSky::smoothStep(ground_radial_fade_start, ground_radial_fade_end, center_distance);
                        alpha = std::min(alpha, 255.0f * radial_fade);
                        world.push_back({ x, y, 0 });
                        src.push_back({ source_x, source_y });
                        colors.push_back({ 255, 255, 255, uint8_t(std::clamp(alpha, 0.0f, 255.0f)) });
                    }
                }

                auto projected = camera_.getProj(world);
                std::vector<FPoint> dst;
                std::vector<float> depths;
                dst.reserve(projected.size());
                depths.reserve(world.size());
                for (size_t i = 0; i < world.size(); i++)
                {
                    float depth = camera_.getDepth(world[i]);
                    float horizon_fade = PaperSky::smoothStep(horizon_fade_start, horizon_fade_end, depth);
                    float horizon_alpha = PAPER_GROUND_HORIZON_MIN_ALPHA
                        + (255.0f - PAPER_GROUND_HORIZON_MIN_ALPHA) * (1.0f - horizon_fade);
                    colors[i].a = std::min(colors[i].a, uint8_t(std::clamp(horizon_alpha, 0.0f, 255.0f)));

                    dst.push_back({ projected[i].x, projected[i].y });
                    depths.push_back(depth);
                }

                std::vector<int> indices;
                indices.reserve(mesh_x * mesh_y * 6);
                auto index_of = [&](int ix, int iy)
                {
                    return iy * (mesh_x + 1) + ix;
                };
                for (int iy = 0; iy < mesh_y; iy++)
                {
                    for (int ix = 0; ix < mesh_x; ix++)
                    {
                        int i0 = index_of(ix, iy);
                        int i1 = index_of(ix + 1, iy);
                        int i2 = index_of(ix + 1, iy + 1);
                        int i3 = index_of(ix, iy + 1);
                        if (depths[i0] <= camera_.getNearPlane()
                            || depths[i1] <= camera_.getNearPlane()
                            || depths[i2] <= camera_.getNearPlane()
                            || depths[i3] <= camera_.getNearPlane())
                        {
                            continue;
                        }
                        indices.insert(indices.end(), { i0, i1, i2, i2, i3, i0 });
                    }
                }

                Engine::getInstance()->renderTextureMesh(earth_texture, dst, src, colors, indices);
            };
            Engine::getInstance()->setRenderTarget("scene");
            render_ground_mesh();

            struct PaperBuilding
            {
                TextureWarpper* tex = nullptr;
                Pointf anchor;
            };

            struct PaperWallEdge
            {
                std::vector<Pointf> world;
                float depth = 0;
                int tile_num = 0;
            };

            std::vector<PaperBuilding> buildings;
            buildings.reserve(COORD_COUNT * COORD_COUNT / 4);
            std::vector<PaperWallEdge> wall_edges;
            wall_edges.reserve(COORD_COUNT * COORD_COUNT / 2);
            Pointf view_dir = camera_.center - camera_.pos;
            view_dir.z = 0;
            if (view_dir.norm() == 0)
            {
                view_dir = { 0, 1, 0 };
            }
            view_dir.normTo(1);
            auto calc_depth = [&](const Pointf& p)
            {
                auto v = p - camera_.pos;
                auto n = view_dir;
                return v.x * n.x + v.y * n.y;
            };
            auto is_paper_wall_at = [&](int x, int y)
            {
                return !isOutLine(x, y) && isPaperWallTile(building_layer_.data(x, y) / 2);
            };
            auto add_wall_edge = [&](Pointf a, Pointf b, int tile_num)
            {
                constexpr float wall_h = 80.0f;
                PaperWallEdge edge;
                edge.world = { { a.x, a.y, wall_h }, { b.x, b.y, wall_h }, { b.x, b.y, 0 }, { a.x, a.y, 0 } };
                Pointf mid = { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f, wall_h * 0.5f };
                edge.depth = calc_depth(mid);
                edge.tile_num = tile_num;
                wall_edges.emplace_back(std::move(edge));
            };
            for (int ix = 0; ix < COORD_COUNT; ix++)
            {
                for (int iy = 0; iy < COORD_COUNT; iy++)
                {
                    int num = building_layer_.data(ix, iy) / 2;
                    if (isPaperWallTile(num))
                    {
                        auto p00 = pos45To90(ix, iy);
                        auto p10 = pos45To90(ix + 1, iy);
                        auto p01 = pos45To90(ix, iy + 1);
                        auto p11 = pos45To90(ix + 1, iy + 1);
                        if (!is_paper_wall_at(ix, iy - 1)) { add_wall_edge(p00, p10, num); }
                        if (!is_paper_wall_at(ix + 1, iy)) { add_wall_edge(p10, p11, num); }
                        if (!is_paper_wall_at(ix, iy + 1)) { add_wall_edge(p11, p01, num); }
                        if (!is_paper_wall_at(ix - 1, iy)) { add_wall_edge(p01, p00, num); }
                        continue;
                    }
                    if (num <= 0 || isPaperWallTile(num))
                    {
                        continue;
                    }
                    auto tex = TextureManager::getInstance()->getTexture("smap", num);
                    if (!tex)
                    {
                        continue;
                    }
                    auto p = pos45To90(ix, iy);
                    buildings.push_back({ tex, { p.x, p.y, 0 } });
                }
            }
            Pointf paper_right = { view_dir.y, -view_dir.x, 0 };

            auto render_paper_texture = [&](TextureWarpper* tex, const Pointf& anchor, Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255, int rot = 0, bool face_camera = false)
            {
                if (!tex)
                {
                    return;
                }
                tex->load();
                auto texture = tex->getTexture();
                if (!texture)
                {
                    return;
                }
                float left = -float(tex->dx);
                float right = float(tex->w - tex->dx);
                float top = float(tex->dy);
                float bottom = float(tex->dy - tex->h);
                Pointf camera_forward = camera_.center - camera_.pos;
                camera_forward.normTo(1);
                Pointf camera_up = {
                    paper_right.y * camera_forward.z - paper_right.z * camera_forward.y,
                    paper_right.z * camera_forward.x - paper_right.x * camera_forward.z,
                    paper_right.x * camera_forward.y - paper_right.y * camera_forward.x,
                };
                camera_up.normTo(1);
                auto local_point = [&](float x, float z)
                {
                    if (face_camera)
                    {
                        return anchor + paper_right * x + camera_up * z;
                    }
                    if (rot == 90)
                    {
                        return anchor + paper_right * -z + Pointf{ 0, 0, x };
                    }
                    if (rot == 270)
                    {
                        return anchor + paper_right * z + Pointf{ 0, 0, -x };
                    }
                    return anchor + paper_right * x + Pointf{ 0, 0, z };
                };
                std::vector<Pointf> paper = {
                    local_point(left, top),
                    local_point(right, top),
                    local_point(right, bottom),
                    local_point(left, bottom),
                };
                if (!in_camera_front(paper))
                {
                    return;
                }
                std::vector<FPoint> src = { { 0, 0 }, { float(tex->w), 0 }, { float(tex->w), float(tex->h) }, { 0, float(tex->h) } };
                Color c = color;
                c.a = alpha;
                if (face_camera)
                {
                    render_texture_3d(texture, paper, src, c);
                    return;
                }
                int columns = std::clamp((tex->w + 31) / 32, 1, 32);
                int rows = std::clamp((tex->h + 31) / 32, 1, 32);
                render_texture_3d_mesh(texture, paper, src, columns, rows, c);
            };

            struct PaperSprite
            {
                Texture* texture = nullptr;
                TextureWarpper* tex = nullptr;
                Pointf anchor;
                std::vector<Pointf> world;
                std::vector<FPoint> src;
                Color color{ 255, 255, 255, 255 };
                uint8_t alpha = 255;
                float depth = 0;
                int turn = 1;
                int rot = 0;
                bool use_perspective_mesh = false;
                bool face_camera = false;
                int order = 0;
            };

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
            auto engine = Engine::getInstance();
            bool need_draw_wall_texture = true;
            if (auto old_wall_texture = engine->getTexture("paper-wall-edge"))
            {
                int old_w = 0;
                int old_h = 0;
                Engine::getTextureSize(old_wall_texture, old_w, old_h);
                need_draw_wall_texture = old_w != 16 || old_h != 16;
            }
            engine->createRenderedTexture("paper-wall-edge", 16, 16);
            auto wall_texture = engine->getTexture("paper-wall-edge");
            if (need_draw_wall_texture)
            {
                auto prev_target = engine->getRenderTarget();
                engine->setRenderTarget(wall_texture);
                engine->fillColor({ 70, 62, 50, 235 }, 0, 0, 16, 16, BLENDMODE_NONE);
                engine->fillColor({ 94, 84, 66, 235 }, 0, 1, 16, 1, BLENDMODE_NONE);
                engine->fillColor({ 47, 42, 35, 235 }, 0, 4, 16, 1, BLENDMODE_NONE);
                engine->fillColor({ 88, 78, 61, 235 }, 0, 8, 16, 1, BLENDMODE_NONE);
                engine->fillColor({ 42, 37, 31, 235 }, 0, 12, 16, 1, BLENDMODE_NONE);
                engine->fillColor({ 104, 92, 72, 210 }, 0, 15, 16, 1, BLENDMODE_NONE);
                engine->setRenderTarget(prev_target);
            }
            std::unordered_map<int, TextureWarpper*> paper_wall_texture_cache;
            auto get_paper_wall_texture = [&](int tile_num) -> TextureWarpper*
            {
                auto it = paper_wall_texture_cache.find(tile_num);
                if (it != paper_wall_texture_cache.end())
                {
                    return it->second;
                }
                TextureWarpper* tex = nullptr;
                auto filename = GameUtil::PATH() + "resource/paper-wall-texture/" + std::to_string(tile_num) + ".png";
                if (filefunc::fileExist(filename))
                {
                    tex = TextureManager::getInstance()->getTexture("paper-wall-texture", tile_num);
                    if (tex)
                    {
                        tex->load();
                        if (!tex->getTexture())
                        {
                            tex = nullptr;
                        }
                    }
                }
                paper_wall_texture_cache[tile_num] = tex;
                return tex;
            };
            for (auto& edge : wall_edges)
            {
                PaperSprite sprite;
                if (auto tex = get_paper_wall_texture(edge.tile_num))
                {
                    sprite.texture = tex->getTexture();
                    sprite.src = { { 0, 0 }, { float(tex->w), 0 }, { float(tex->w), float(tex->h) }, { 0, float(tex->h) } };
                }
                else
                {
                    sprite.texture = wall_texture;
                    sprite.src = { { 0, 0 }, { 16, 0 }, { 16, 16 }, { 0, 16 } };
                }
                sprite.world = edge.world;
                sprite.anchor = { (edge.world[0].x + edge.world[2].x) * 0.5f, (edge.world[0].y + edge.world[2].y) * 0.5f, 0 };
                sprite.depth = edge.depth;
                sprite.use_perspective_mesh = true;
                push_sprite(std::move(sprite));
            }
            for (auto& b : buildings)
            {
                PaperSprite sprite;
                sprite.tex = b.tex;
                sprite.anchor = b.anchor;
                sprite.depth = calc_depth(sprite.anchor);
                push_sprite(std::move(sprite));
            }

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
                    sprite.rot = r->FaceTowards >= 2 ? 90 : 270;
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

            std::sort(sprites.begin(), sprites.end(), [](const PaperSprite& a, const PaperSprite& b)
                {
                    constexpr float depth_epsilon = 1.0f;
                    float depth_delta = a.depth - b.depth;
                    if (std::abs(depth_delta) > depth_epsilon)
                    {
                        return depth_delta > 0;
                    }
                    if (a.anchor.y != b.anchor.y)
                    {
                        return a.anchor.y < b.anchor.y;
                    }
                    if (a.anchor.x != b.anchor.x)
                    {
                        return a.anchor.x < b.anchor.x;
                    }
                    if (a.turn != b.turn)
                    {
                        return a.turn < b.turn;
                    }
                    return a.order < b.order;
                });
            for (auto& sprite : sprites)
            {
                    if (sprite.texture && sprite.world.size() == 4)
                    {
                        if (in_camera_front(sprite.world))
                        {
                            if (sprite.use_perspective_mesh)
                            {
                                render_texture_3d_mesh(sprite.texture, sprite.world, sprite.src, 4, 4, sprite.color);
                            }
                            else
                            {
                                render_texture_3d(sprite.texture, sprite.world, sprite.src, sprite.color);
                            }
                        }
                    }
                    else
                    {
                        render_paper_texture(sprite.tex, sprite.anchor, sprite.color, sprite.alpha, sprite.rot,
                            sprite.face_camera);
                    }
            }
            if (locked_role && locked_role->Dead == 0 && locked_role->HP > 0)
            {
                Pointf marker_world = locked_role->Pos + Pointf{ 0, 0, TILE_W * 1.5f };
                if (camera_.getDepth(marker_world) > camera_.getNearPlane())
                {
                    auto projected = camera_.getProj({ marker_world });
                    if (!projected.empty())
                    {
                        auto tex = TextureManager::getInstance()->getTexture("title", PAPER_LOCK_MARK_TEXTURE);
                        float zoom = 0.5f + 0.06f * std::sin(current_frame_ * 0.16f);
                        int mark_x = int(projected[0].x) - (tex ? int(tex->w * zoom * 0.5f) : 0);
                        int mark_y = int(projected[0].y) - (tex ? int(tex->h * zoom * 0.5f) : 0);
                        TextureManager::getInstance()->renderTexture("title", PAPER_LOCK_MARK_TEXTURE, mark_x, mark_y,
                            { { 255, 255, 255, 255 }, 224, zoom, zoom });
                    }
                }
            }
            for (auto r : battle_roles_)
            {
                if (!r)
                {
                    continue;
                }
                Pointf bar_world = r->Pos + Pointf{ 0, 0, TILE_W * 3.5f };
                if (camera_.getDepth(bar_world) <= camera_.getNearPlane())
                {
                    continue;
                }
                auto projected = camera_.getProj({ bar_world });
                if (!projected.empty())
                {
                    int tile_scale = (std::max)(1, TILE_W / TILE_W_0);
                    int bar_anchor_y = int(projected[0].y) + 61 * tile_scale - int(PAPER_ROLE_INFO_TOP_GAP);
                    renderExtraRoleInfo(r, int(projected[0].x), bar_anchor_y);
                }
            }
            for (const auto& te : text_effects_)
            {
                Pointf text_world = te.PaperPos;
                text_world.z += TILE_W * 1.5f;
                if (camera_.getDepth(text_world) <= camera_.getNearPlane())
                {
                    continue;
                }
                auto projected = camera_.getProj({ text_world });
                if (!projected.empty())
                {
                    int ui_width = 0;
                    int ui_height = 0;
                    Engine::getInstance()->getUISize(ui_width, ui_height);
                    float scene_to_ui_x = float(ui_width) / (render_center_x_ * 2);
                    float scene_to_ui_y = float(ui_height) / (render_center_y_ * 2);
                    float text_x = projected[0].x - 7.5f * Font::getTextDrawSize(te.Text);
                    float text_y = projected[0].y - 4.0f - te.PaperRise;
                    Font::getInstance()->draw(te.Text, te.Size, int(text_x * scene_to_ui_x),
                        int(text_y * scene_to_ui_y), te.color, 255);
                }
            }
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

bool BattleSceneAct::isPaperWallTile(int num) const
{
    return (num >= 701 && num <= 1139)
        || (num >= 1410 && num <= 1436)
        || (num >= 1505 && num <= 1621)
        || (num >= 1816 && num <= 1849)
        || (num >= 2116 && num <= 2144)
        || (num >= 2184 && num <= 2285);
}

