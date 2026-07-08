#include "BattleScenePaper.h"
#include "Audio.h"
#include "Event.h"
#include "GameUtil.h"
#include "MainScene.h"
#include "TeamMenu.h"
#include "Weather.h"
#include "filefunc.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
constexpr float PAPER_FREE_CAMERA_ROTATE_SPEED = 0.035f;
constexpr float PAPER_FREE_CAMERA_HEIGHT_SPEED = 6.0f;
constexpr float PAPER_CAMERA_DEFAULT_DISTANCE = 400.0f;
constexpr float PAPER_CAMERA_MIN_DISTANCE = 220.0f;
constexpr float PAPER_CAMERA_MAX_DISTANCE = 900.0f;
constexpr float PAPER_CAMERA_ZOOM_STEP = 24.0f;
constexpr float PAPER_CAMERA_WHEEL_ZOOM_STEP = 80.0f;
constexpr float PAPER_CAMERA_MIN_HEIGHT = 80.0f;
constexpr float PAPER_CAMERA_MAX_HEIGHT = 500.0f;
constexpr int PAPER_LOCK_MARK_TEXTURE = 201;

BattleScenePaper::BattleScenePaper()
{
    keys_ = *UIKeyConfig::getKeyConfig();
    full_window_ = 1;
    COORD_COUNT = BATTLEMAP_COORD_COUNT;

    earth_layer_.resize(COORD_COUNT);
    building_layer_.resize(COORD_COUNT);

    heads_.resize(1);
    int i = 0;
    for (auto& h : heads_)
    {
        h = std::make_shared<Head>();
        h->setStyle(2);
        h->setAlwaysLight(1);
        addChild(h, 10, 10 + (i++) * 80);
        h->setVisible(false);
    }
    heads_[0]->setVisible(true);
    heads_[0]->setRole(Save::getInstance()->getRole(0));

    heads_[0]->setPosition(10, Engine::getInstance()->getUIHeight() - 100);

    head_boss_.resize(1);
    for (auto& h : head_boss_)
    {
        h = std::make_shared<Head>();
        h->setStyle(2);
        h->setVisible(true);
        addChild(h);
    }
    easy_block_ = GameUtil::getInstance()->getInt("game", "easy_block", 0);
}

void BattleScenePaper::draw()
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
            auto render_texture_3d = [&](Texture* texture, const std::vector<Pointf>& world, const std::vector<FPoint>& src)
            {
                auto projected = camera_.getProj(world);
                std::vector<FPoint> dst;
                dst.reserve(projected.size());
                for (auto& p : projected)
                {
                    dst.push_back({ float(p.x), float(p.y) });
                }
                Engine::getInstance()->renderTexture(texture, nullptr, dst, src);
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

            auto render_paper_texture = [&](TextureWarpper* tex, const Pointf& anchor, Color color = { 255, 255, 255, 255 }, uint8_t alpha = 255, int rot = 0)
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
                auto local_point = [&](float x, float z)
                {
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
                Engine::getInstance()->setColor(texture, c);
                render_texture_3d(texture, paper, src);
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
            };

            auto to_paper_anchor = [](Pointf p)
            {
                return Pointf{ p.x, p.y, p.z };
            };

            std::vector<PaperSprite> sprites;
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
                sprite.depth = edge.depth;
                sprites.emplace_back(std::move(sprite));
            }
            for (auto& b : buildings)
            {
                PaperSprite sprite;
                sprite.tex = b.tex;
                sprite.anchor = b.anchor;
                sprite.depth = calc_depth(sprite.anchor);
                sprites.emplace_back(sprite);
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
                sprites.emplace_back(sprite);
            }

            for (auto& ae : attack_effects_)
            {
                PaperSprite sprite;
                int frame = ae.TotalEffectFrame > 0 ? ae.Frame % ae.TotalEffectFrame : 0;
                sprite.tex = TextureManager::getInstance()->getTexture(ae.Path, frame);
                sprite.anchor = to_paper_anchor(ae.FollowRole ? ae.FollowRole->Pos : ae.Pos);
                sprite.alpha = uint8_t(GameUtil::clamp(255 * (ae.TotalFrame * 1.25 - ae.Frame) / (ae.TotalFrame * 1.25), 0.0, 255.0));
                sprite.depth = calc_depth(sprite.anchor);
                sprites.emplace_back(sprite);
            }

            std::sort(sprites.begin(), sprites.end(), [](const PaperSprite& a, const PaperSprite& b)
                {
                    if (a.depth != b.depth)
                    {
                        return a.depth > b.depth;
                    }
                    return a.turn < b.turn;
                });
            for (auto& sprite : sprites)
            {
                    if (sprite.texture && sprite.world.size() == 4)
                    {
                        if (in_camera_front(sprite.world))
                        {
                            render_texture_3d(sprite.texture, sprite.world, sprite.src);
                        }
                    }
                    else
                    {
                        render_paper_texture(sprite.tex, sprite.anchor, sprite.color, sprite.alpha, sprite.rot);
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
            Engine::getInstance()->renderTextureToMain("scene");
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

void BattleScenePaper::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    auto r = role_;
    //LOG("{},{}, {},{}\n", r->Velocity.x, r->Velocity.y, r->Acceleration.x, r->Acceleration.y);
    //show_auto_->setVisible(r->Auto);
    if (shake_ > 0)
    {
        x_ = rand_.rand_int(3) - rand_.rand_int(3);
        y_ = rand_.rand_int(3) - rand_.rand_int(3);
        shake_--;
    }
    if (frozen_ > 0)
    {
        frozen_--;
        engine->gameControllerRumble(100, 100, 50);
        return;
    }
    decreaseToZero(close_up_);
    decreaseToZero(sword_light_);
    if (close_up_ == 0)
    {
        if (r->Dead)
        {
            for (auto r1 : battle_roles_)
            {
                if (r1->Team == 0 && r1->Dead == 0)
                {
                    pos_ = r1->Pos;
                }
            }
            //engine->gameControllerRumble(65535, 65535, 1000);
        }
        else
        {
            //如果没有特写，主角位置就是当前的pos
            pos_ = r->Pos;
        }
    }

    Pointf pos = r->Pos;
    double speed = std::min(4.0, r->Speed / 30.0);
    if (e.type == EVENT_KEY_UP && e.key.key == K_TAB
        || e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_BACK)
    {
        if (r->Auto == 0) { r->Auto = 1; }
        else
        {
            r->Auto = 0;
        }
    }
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
        camera_distance_ = free_camera_distance_;
    }
    if (e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE
        || e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_START)
    {
        auto menu2 = std::make_shared<MenuText>();
        menu2->setStrings({ "確認", "取消" });
        menu2->setPosition(400, 300);
        menu2->setFontSize(24);
        menu2->setHaveBox(true);
        menu2->setText("認輸？");
        menu2->arrange(0, 50, 150, 0);
        if (menu2->run() == 0)
        {
            result_ = 1;
            for (auto r : friends_)
            {
                r->ExpGot = 0;
            }
            setExit(true);
        }
    }
    if (r->Dead == 0)
    {
        if (r->Frozen == 0 && r->CoolDown == 0 && r->Breathless == 0)
        {
            //if (current_frame_ % 3 == 0)
            {
                auto axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTX);
                auto axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFTY);
                if (abs(axis_x) < 6000) { axis_x = 0; }
                if (abs(axis_y) < 6000) { axis_y = 0; }
                if (axis_x != 0 || axis_y != 0)
                {
                    //LOG("{} {}, ", axis_x, axis_y);
                    axis_x = GameUtil::clamp(axis_x, -20000, 20000);
                    axis_y = GameUtil::clamp(axis_y, -20000, 20000);
                }
                float input_right = axis_x / 20000.0f;
                float input_forward = -axis_y / 20000.0f;
                if (engine->checkKeyPress(K_A))
                {
                    input_right -= 1;
                }
                if (engine->checkKeyPress(K_D))
                {
                    input_right += 1;
                }
                if (engine->checkKeyPress(K_W))
                {
                    input_forward += 1;
                }
                if (engine->checkKeyPress(K_S))
                {
                    input_forward -= 1;
                }
                if (!camera_locked_)
                {
                    if (engine->checkKeyPress(K_Z))
                    {
                        free_camera_distance_ += PAPER_CAMERA_ZOOM_STEP;
                    }
                    if (engine->checkKeyPress(K_X))
                    {
                        free_camera_distance_ -= PAPER_CAMERA_ZOOM_STEP;
                    }
                    auto left_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFT_TRIGGER);
                    auto right_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHT_TRIGGER);
                    if (left_trigger > 6000)
                    {
                        free_camera_distance_ += PAPER_CAMERA_ZOOM_STEP * GameUtil::clamp(left_trigger, 0, 20000) / 20000.0f;
                    }
                    if (right_trigger > 6000)
                    {
                        free_camera_distance_ -= PAPER_CAMERA_ZOOM_STEP * GameUtil::clamp(right_trigger, 0, 20000) / 20000.0f;
                    }
                    free_camera_distance_ = std::clamp(free_camera_distance_, PAPER_CAMERA_MIN_DISTANCE, PAPER_CAMERA_MAX_DISTANCE);
                    camera_distance_ = free_camera_distance_;
                }
                if (!camera_locked_)
                {
                    float camera_rotate = 0;
                    float camera_height_delta = 0;
                    if (engine->checkKeyPress(K_LEFT)) { camera_rotate -= 1; }
                    if (engine->checkKeyPress(K_RIGHT)) { camera_rotate += 1; }
                    if (engine->checkKeyPress(K_UP)) { camera_height_delta += 1; }
                    if (engine->checkKeyPress(K_DOWN)) { camera_height_delta -= 1; }
                    auto right_axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTX);
                    auto right_axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTY);
                    if (std::abs(right_axis_x) < 6000) { right_axis_x = 0; }
                    if (std::abs(right_axis_y) < 6000) { right_axis_y = 0; }
                    camera_rotate += GameUtil::clamp(right_axis_x, -20000, 20000) / 20000.0f;
                    camera_height_delta += GameUtil::clamp(-right_axis_y, -20000, 20000) / 20000.0f;
                    camera_angle_ += camera_rotate * PAPER_FREE_CAMERA_ROTATE_SPEED;
                    camera_height_ = std::clamp(camera_height_ + camera_height_delta * PAPER_FREE_CAMERA_HEIGHT_SPEED,
                        PAPER_CAMERA_MIN_HEIGHT, PAPER_CAMERA_MAX_HEIGHT);
                }
                Pointf view_dir = camera_.center - camera_.pos;
                view_dir.z = 0;
                if (view_dir.norm() == 0)
                {
                    view_dir = r->RealTowards;
                    view_dir.z = 0;
                }
                if (view_dir.norm() == 0)
                {
                    view_dir = { 0, 1, 0 };
                }
                view_dir.normTo(1);
                Pointf paper_right = { view_dir.y, -view_dir.x, 0 };
                Pointf direct = paper_right * input_right + view_dir * input_forward;
                direct.normTo(speed);
                pos += direct;
            }
        }
        //实际的朝向可以不能走到
        if (pos.x != r->Pos.x || pos.y != r->Pos.y)
        {
            r->RealTowards = pos - r->Pos;
        }

        if (canWalk90(pos, r))
        {
            r->Pos = pos;
        }

        // 初始化武功
        std::vector<Magic*> magic(4);
        for (int i = 0; i < 1; i++)
        {
            magic[i] = Save::getInstance()->getMagic(r->EquipMagic[i]);
            if (magic[i] && r->getMagicOfRoleIndex(magic[i]) < 0) { magic[i] = nullptr; }
            if (magic[i] == nullptr)
            {
                if (r->getLearnedMagicCount() > i)
                {
                    magic[i] = r->getLearnedMagics()[i];
                }
            }
            //equip_magics_[i]->setState(NodeNormal);
        }
        if (camera_locked_ && switch_magic_ == 0
            && (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_LEFT)
                || engine->checkKeyPress(K_LEFT)))
        {
            auto v = r->getLearnedMagics();
            auto& m = magic[0];
            int index = -1;
            for (int i = 0; i < v.size(); i++)
            {
                if (v[i] == m)
                {
                    index = i;
                    break;
                }
            }
            index--;
            if (index < 0)
            {
                index = v.size() - 1;
            }
            r->EquipMagic[0] = v[index]->ID;
            switch_magic_ = 20;
        }
        if (camera_locked_ && switch_magic_ == 0
            && (engine->gameControllerGetButton(GAMEPAD_BUTTON_DPAD_RIGHT)
                || engine->checkKeyPress(K_RIGHT)))
        {
            auto v = r->getLearnedMagics();
            auto& m = magic[0];
            int index = -1;
            for (int i = 0; i < v.size(); i++)
            {
                if (v[i] == m)
                {
                    index = i;
                    break;
                }
            }
            index++;
            if (index >= v.size())
            {
                index = 0;
            }
            r->EquipMagic[0] = v[index]->ID;
            switch_magic_ = 20;
        }
        decreaseToZero(switch_magic_);
        if (r->Frozen == 0 && r->CoolDown == 0)
        {
            int index = -1;
            //0攻击，5防御，3闪身，其他不处理
            if (engine->gameControllerGetButton(GAMEPAD_BUTTON_RIGHT_SHOULDER)
                || engine->gameControllerGetButton(GAMEPAD_BUTTON_NORTH)
                || engine->checkKeyPress(K_I))
            {
                index = 0;
            }
            if (engine->gameControllerGetButton(GAMEPAD_BUTTON_LEFT_SHOULDER)
                || engine->gameControllerGetButton(GAMEPAD_BUTTON_WEST)
                || engine->checkKeyPress(K_E))
            {
                index = 5;
            }
            if (engine->gameControllerGetButton(GAMEPAD_BUTTON_SOUTH)
                || engine->checkKeyPress(K_L))
            {
                index = 3;
            }
            if (index >= 0)
            {
                r->Auto = 0;
            }
            if (index == 0 && (r->OperationCount >= 3 || current_frame_ - r->PreActTimer > 60))
            {
                r->OperationCount = 0;
            }
            if (index >= 0 && index == r->OperationType)
            {
                r->OperationCount++;
            }
            if (index != r->OperationType)
            {
                r->OperationCount = 0;
            }

            if (index >= 0)
            {
                r->OperationType = index;
                //equip_magics_[index]->setState(NodePass);
                auto m = magic[0];
                r->UsingItem = nullptr;
                r->ActFrame = 0;
                r->HaveAction = 1;
                if (m)
                {
                    //攻击动作有冷却
                    if (index == 0 || index == 5)
                    {
                        r->ActType = m->MagicType;
                        r->UsingMagic = m;
                    }
                    r->CoolDown = calCoolDown(m->MagicType, index, r);
                }
                else
                {
                    //防御无冷却
                    r->CoolDown = 0;
                }
                if (r->OperationCount >= 3 && index == 0)
                {
                    r->CoolDown *= 2;
                }
            }
            if (r->OperationType == 5 && index < 0)
            {
                r->OperationType = 0;    //防御必须按住
            }
            //LOG("{},p\n", r->OperationCount);
        }
    }
    backRun1();
}

void BattleScenePaper::dealEvent2(EngineEvent& e)
{
}

void BattleScenePaper::onEntrance()
{
    paper_sky_.reset();
    paper_sky_.generateClouds();
    calViewRegion();
    Audio::getInstance()->playMusic(info_->Music);
    //注意此时才能得到窗口的大小，用来设置头像的位置
    head_self_->setPosition(10, 10);
    int count = 0;
    for (auto& h : head_boss_)
    {
        h->setPosition(30, 40);
    }
    addChild(Weather::getInstance());

    const int base_earth_w = COORD_COUNT * TILE_W * 2;
    const int base_earth_h = COORD_COUNT * TILE_H * 2;
    const int paper_earth_w = COORD_COUNT * TILE_W * 2;
    const int paper_earth_h = COORD_COUNT * TILE_W * 2;
    auto engine = Engine::getInstance();
    Texture* earth_base_texture = nullptr;
    Texture* earth_texture = nullptr;
    int selected_ground_extension_margin = 0;
    int requested_base_earth_w = base_earth_w;
    int requested_base_earth_h = base_earth_h;
    int requested_paper_earth_w = paper_earth_w;
    int requested_paper_earth_h = paper_earth_h;
    int requested_ground_extension_margin = std::max(0,
        int(std::round(COORD_COUNT * PAPER_GROUND_EXTENSION_MARGIN_RATIO)));
    for (int ground_extension_margin = requested_ground_extension_margin; ground_extension_margin >= 0;)
    {
        requested_base_earth_w = base_earth_w + ground_extension_margin * TILE_W * 4;
        requested_base_earth_h = base_earth_h + ground_extension_margin * TILE_H * 4;
        requested_paper_earth_w = paper_earth_w + ground_extension_margin * TILE_W * 4;
        requested_paper_earth_h = paper_earth_h + ground_extension_margin * TILE_W * 4;
        engine->createRenderedTexture("earth_base", requested_base_earth_w, requested_base_earth_h);
        engine->createRenderedTexture("earth", requested_paper_earth_w, requested_paper_earth_h);
        earth_base_texture = engine->getTexture("earth_base");
        earth_texture = engine->getTexture("earth");
        if (earth_base_texture && earth_texture)
        {
            selected_ground_extension_margin = ground_extension_margin;
            break;
        }
        if (ground_extension_margin == 0)
        {
            break;
        }
        ground_extension_margin /= 2;
    }

    if (earth_base_texture && earth_texture)
    {
        int base_earth_texture_w = requested_base_earth_w;
        int base_earth_texture_h = requested_base_earth_h;
        engine->getTextureSize(earth_base_texture, base_earth_texture_w, base_earth_texture_h);
        int paper_earth_texture_w = requested_paper_earth_w;
        int paper_earth_texture_h = requested_paper_earth_h;
        engine->getTextureSize(earth_texture, paper_earth_texture_w, paper_earth_texture_h);
        int base_earth_offset_x = std::max(0, (base_earth_texture_w - base_earth_w) / 2);
        int base_earth_offset_y = std::max(0, (base_earth_texture_h - base_earth_h) / 2);
        Engine::setTextureBlendMode(earth_base_texture);
        Engine::setTextureBlendMode(earth_texture);
        SDL_SetTextureScaleMode(earth_base_texture, SDL_SCALEMODE_NEAREST);
        SDL_SetTextureScaleMode(earth_texture, SDL_SCALEMODE_LINEAR);
        engine->setRenderTarget(earth_base_texture);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, base_earth_texture_w, base_earth_texture_h, BLENDMODE_NONE);

        auto texture_manager = TextureManager::getInstance();
        auto render_smap_ground = [&]()
        {
            struct GroundTile
            {
                TextureWarpper* texture = nullptr;
                int num = 0;
                int ix = 0;
                int iy = 0;
            };

            struct GroundDrawTile
            {
                int source_index = -1;
                int ix = 0;
                int iy = 0;
            };

            std::vector<GroundTile> ground_tiles;
            ground_tiles.reserve(COORD_COUNT * COORD_COUNT);
            auto tile_index = [&](int ix, int iy)
            {
                return ix + iy * COORD_COUNT;
            };
            std::vector<int> ground_tile_indices(COORD_COUNT * COORD_COUNT, -1);
            std::vector<int> row_first_valid(COORD_COUNT, -1);
            std::vector<int> row_last_valid(COORD_COUNT, -1);
            std::vector<int> column_first_valid(COORD_COUNT, -1);
            std::vector<int> column_last_valid(COORD_COUNT, -1);
            for (int ix = 0; ix < COORD_COUNT; ix++)
            {
                for (int iy = 0; iy < COORD_COUNT; iy++)
                {
                    int num = earth_layer_.data(ix, iy) / 2;
                    if (num > 0)
                    {
                        auto tex = texture_manager->getTexture("smap", num);
                        if (tex)
                        {
                            int source_index = int(ground_tiles.size());
                            ground_tiles.push_back({ tex, num, ix, iy });
                            ground_tile_indices[tile_index(ix, iy)] = source_index;
                            if (row_first_valid[iy] < 0 || ix < row_first_valid[iy])
                            {
                                row_first_valid[iy] = ix;
                            }
                            if (row_last_valid[iy] < 0 || ix > row_last_valid[iy])
                            {
                                row_last_valid[iy] = ix;
                            }
                            if (column_first_valid[ix] < 0 || iy < column_first_valid[ix])
                            {
                                column_first_valid[ix] = iy;
                            }
                            if (column_last_valid[ix] < 0 || iy > column_last_valid[ix])
                            {
                                column_last_valid[ix] = iy;
                            }
                        }
                    }
                }
            }

            std::vector<int> edge_tile_indices;
            edge_tile_indices.reserve(ground_tiles.size());
            const int neighbor_offsets[4][2] = {
                { -1, 0 },
                { 1, 0 },
                { 0, -1 },
                { 0, 1 },
            };
            for (int i = 0; i < int(ground_tiles.size()); i++)
            {
                const auto& tile = ground_tiles[i];
                bool is_edge = false;
                for (const auto& offset : neighbor_offsets)
                {
                    int nx = tile.ix + offset[0];
                    int ny = tile.iy + offset[1];
                    if (nx < 0 || nx >= COORD_COUNT || ny < 0 || ny >= COORD_COUNT
                        || ground_tile_indices[tile_index(nx, ny)] < 0)
                    {
                        is_edge = true;
                        break;
                    }
                }
                if (is_edge)
                {
                    edge_tile_indices.push_back(i);
                }
            }
            if (edge_tile_indices.empty())
            {
                for (int i = 0; i < int(ground_tiles.size()); i++)
                {
                    edge_tile_indices.push_back(i);
                }
            }

            auto get_original_tile_index = [&](int ix, int iy)
            {
                if (ix < 0 || ix >= COORD_COUNT || iy < 0 || iy >= COORD_COUNT)
                {
                    return -1;
                }
                return ground_tile_indices[tile_index(ix, iy)];
            };
            auto find_nearest_edge_tile = [&](int ix, int iy)
            {
                int best_index = -1;
                int best_distance = std::numeric_limits<int>::max();
                for (int source_index : edge_tile_indices)
                {
                    const auto& tile = ground_tiles[source_index];
                    int dx = tile.ix - ix;
                    int dy = tile.iy - iy;
                    int distance = dx * dx + dy * dy;
                    if (distance < best_distance)
                    {
                        best_distance = distance;
                        best_index = source_index;
                    }
                }
                return best_index;
            };
            auto get_extension_source_index = [&](int ix, int iy)
            {
                int original_index = get_original_tile_index(ix, iy);
                if (original_index >= 0)
                {
                    return original_index;
                }

                int sx = std::clamp(ix, 0, COORD_COUNT - 1);
                int sy = std::clamp(iy, 0, COORD_COUNT - 1);
                if (iy < 0 && column_first_valid[sx] >= 0)
                {
                    sy = column_first_valid[sx];
                }
                else if (iy >= COORD_COUNT && column_last_valid[sx] >= 0)
                {
                    sy = column_last_valid[sx];
                }
                if (ix < 0 && row_first_valid[sy] >= 0)
                {
                    sx = row_first_valid[sy];
                }
                else if (ix >= COORD_COUNT && row_last_valid[sy] >= 0)
                {
                    sx = row_last_valid[sy];
                }

                int source_index = get_original_tile_index(sx, sy);
                if (source_index >= 0)
                {
                    return source_index;
                }
                return find_nearest_edge_tile(ix, iy);
            };

            std::vector<GroundDrawTile> ground_draw_tiles;
            int ground_extension_margin = selected_ground_extension_margin;
            int ground_draw_tile_count = COORD_COUNT + ground_extension_margin * 2;
            ground_draw_tiles.reserve(ground_draw_tile_count * ground_draw_tile_count);
            for (int iy = -ground_extension_margin; iy < COORD_COUNT + ground_extension_margin; iy++)
            {
                for (int ix = -ground_extension_margin; ix < COORD_COUNT + ground_extension_margin; ix++)
                {
                    int source_index = get_extension_source_index(ix, iy);
                    if (source_index >= 0)
                    {
                        ground_draw_tiles.push_back({ source_index, ix, iy });
                    }
                }
            }

            auto render_ground_tile = [&](const GroundDrawTile& draw_tile)
            {
                const auto& tile = ground_tiles[draw_tile.source_index];
                auto tex = tile.texture;
                tex->load();
                auto texture = tex->getTexture();
                if (!texture)
                {
                    return;
                }
                Engine::setColor(texture, { 255, 255, 255, 255 });
                auto p = pos45To90(draw_tile.ix, draw_tile.iy);
                int draw_x = base_earth_offset_x + int(p.x) - tex->dx;
                int draw_y = base_earth_offset_y + int(std::round(p.y * TILE_H / float(TILE_W))) - tex->dy;
                int draw_w = tex->w;
                int draw_h = tex->h;
                engine->renderTexture(texture, draw_x, draw_y, draw_w, draw_h);
            };
            for (const auto& tile : ground_draw_tiles)
            {
                render_ground_tile(tile);
            }
        };

        render_smap_ground();

        auto battle_earth_group = texture_manager->getTextureGroup("battle-earth");
        if (battle_earth_group->getTextureCount() > 0)
        {
            std::string battle_earth_filename = std::to_string(info_->BattleFieldID) + battle_earth_group->info_.ext_;
            if (!battle_earth_group->getFileContent(battle_earth_filename).empty())
            {
                auto tex = texture_manager->getTexture("battle-earth", info_->BattleFieldID);
                if (tex)
                {
                    tex->load();
                    auto texture = tex->getTexture();
                    if (texture)
                    {
                        int texture_w = tex->w;
                        int texture_h = tex->h;
                        engine->getTextureSize(texture, texture_w, texture_h);
                        Engine::setColor(texture, { 255, 255, 255, 255 });
                        if (texture_w > 0 && texture_h > 0)
                        {
                            int center_x = base_earth_offset_x - tex->dx;
                            int center_y = base_earth_offset_y - tex->dy;
                            int right_x = center_x + base_earth_w;
                            int bottom_y = center_y + base_earth_h;
                            int left_w = std::max(0, center_x);
                            int top_h = std::max(0, center_y);
                            int right_w = std::max(0, base_earth_texture_w - right_x);
                            int bottom_h = std::max(0, base_earth_texture_h - bottom_y);
                            int strip_w = std::clamp(texture_w / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, texture_w);
                            int strip_h = std::clamp(texture_h / PAPER_GROUND_EDGE_STRETCH_SOURCE_DIVISOR, 1, texture_h);

                            auto render_texture_rect = [&](Rect source, Rect destination)
                            {
                                if (source.w <= 0 || source.h <= 0 || destination.w <= 0 || destination.h <= 0)
                                {
                                    return;
                                }
                                engine->renderTexture(texture, &source, &destination);
                            };

                            Rect full_source = { 0, 0, texture_w, texture_h };
                            Rect center_destination = { center_x, center_y, base_earth_w, base_earth_h };
                            render_texture_rect(full_source, center_destination);

                            Rect left_source = { 0, 0, strip_w, texture_h };
                            Rect right_source = { texture_w - strip_w, 0, strip_w, texture_h };
                            Rect top_source = { 0, 0, texture_w, strip_h };
                            Rect bottom_source = { 0, texture_h - strip_h, texture_w, strip_h };
                            render_texture_rect(left_source, { 0, center_y, left_w, base_earth_h });
                            render_texture_rect(right_source, { right_x, center_y, right_w, base_earth_h });
                            render_texture_rect(top_source, { center_x, 0, base_earth_w, top_h });
                            render_texture_rect(bottom_source, { center_x, bottom_y, base_earth_w, bottom_h });

                            render_texture_rect({ 0, 0, strip_w, strip_h }, { 0, 0, left_w, top_h });
                            render_texture_rect({ texture_w - strip_w, 0, strip_w, strip_h }, { right_x, 0, right_w, top_h });
                            render_texture_rect({ 0, texture_h - strip_h, strip_w, strip_h }, { 0, bottom_y, left_w, bottom_h });
                            render_texture_rect({ texture_w - strip_w, texture_h - strip_h, strip_w, strip_h },
                                { right_x, bottom_y, right_w, bottom_h });
                        }
                    }
                }
            }
        }

        engine->setRenderTarget(earth_texture);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, paper_earth_texture_w, paper_earth_texture_h, BLENDMODE_NONE);
        Engine::setColor(earth_base_texture, { 255, 255, 255, 255 });
        Rect base_source_rect = { 0, 0, base_earth_texture_w, base_earth_texture_h };
        Rect paper_destination_rect = { 0, 0, paper_earth_texture_w, paper_earth_texture_h };
        engine->renderTexture(earth_base_texture, &base_source_rect, &paper_destination_rect);
    }

    Engine::getInstance()->createRenderedTexture("earth2", paper_earth_w, paper_earth_h);

    if (auto earth_texture2 = Engine::getInstance()->getTexture("earth2"))
    {
        Engine::getInstance()->setRenderTarget(earth_texture2);
        Engine::getInstance()->fillColor(Color(0, 0, 0, 0), 0, 0, paper_earth_w, paper_earth_h);
    }

    //首先设置位置和阵营，其他的后面统一处理
    //敌方
    for (int i = 0; i < BATTLE_ENEMY_COUNT; i++)
    {
        auto r_ptr = Save::getInstance()->getRole(info_->Enemy[i]);
        if (r_ptr)
        {
            enemies_obj_.push_back(*r_ptr);
            auto r = &enemies_obj_.back();
            r->resetBattleInfo();
            enemies_.push_back(r);
            r->setPositionOnly(info_->EnemyX[i], info_->EnemyY[i]);
            r->Team = 1;
            readFightFrame(r);
            r->FaceTowards = rand_.rand_int(4);
            man_x_ = r->X();
            man_y_ = r->Y();
        }
    }

    //初始状态
    for (auto r : enemies_)
    {
        setRoleInitState(r);
    }
    pos_ = enemies_[0]->Pos;

    //敌人按能力从低到高，依次出场
    std::sort(enemies_.begin(), enemies_.end(), [](Role* l, Role* r)
        {
            return l->MaxHP + l->Attack < r->MaxHP + r->Attack;
        });

    for (int i = 0; i < head_boss_.size(); i++)
    {
        bool is_boss = false;
        if (enemies_.size() >= i + 1)
        {
            auto r = enemies_[enemies_.size() - i - 1];
            if (is_boss || r->MaxHP >= 300 || r == enemies_.back())
            {
                is_boss = true;
                head_boss_[i]->setRole(r);
            }
        }
    }
    for (int i = 0; i < 1; i++)
    {
        if (!enemies_.empty())
        {
            battle_roles_.push_back(enemies_.front());
            enemies_.pop_front();
        }
    }

    //判断是不是有自动战斗人物
    if (info_->AutoTeamMate[0] >= 0)
    {
        for (int i = 0; i < TEAMMATE_COUNT; i++)
        {
            auto r = Save::getInstance()->getRole(info_->AutoTeamMate[i]);
            if (r)
            {
                friends_.push_back(r);
                r->Auto = 2;    //由AI控制
            }
        }
    }

    if (1)    //无队友出场
    {
        auto team_menu = std::make_shared<TeamMenu>();
        team_menu->setMode(1);
        team_menu->setForceMainRole(true);
        team_menu->run();

        for (auto r : team_menu->getRoles())
        {
            if (std::find(friends_.begin(), friends_.end(), r) == friends_.end())
            {
                friends_.push_back(r);
            }
        }
    }
    //队友
    role_ = Save::getInstance()->getRole(0);
    bool have_main = false;
    for (auto r : friends_)
    {
        if (r == role_) { have_main = true; }
    }
    if (!have_main)
    {
        friends_.insert(friends_.begin(), role_);
    }
    for (int i = 0; i < friends_.size(); i++)
    {
        auto r = friends_[i];
        if (r)
        {
            r->resetBattleInfo();
            battle_roles_.push_back(r);
            r->setPositionOnly(info_->TeamMateX[i], info_->TeamMateY[i]);
            r->Team = 0;
            setRoleInitState(r);
        }
    }
    //int i = 0;
    //for (auto r : friends_)
    //{
    //    if (r && r != heads_[0]->getRole())
    //    {
    //        auto head = std::make_shared<Head>();
    //        head->setRole(r);
    //        head->setAlwaysLight(true);
    //        addChild(head, Engine::getInstance()->getWindowWidth() - 280, 10 + 80 * i++);
    //    }
    //}

    //head_self_->setRole(role_);

    //for (int i = 0; i < equip_magics_.size(); i++)
    //{
    //    auto m = Save::getInstance()->getMagic(role_->EquipMagic[i]);
    //    if (m && role_->getMagicOfRoleIndex(m) < 0) { m = nullptr; }
    //    if (m)
    //    {
    //        std::string text = m->Name;
    //        text += std::string(10 - Font::getTextDrawSize(text), ' ');
    //        equip_magics_[i]->setText(text);
    //    }
    //    else
    //    {
    //        equip_magics_[i]->setText("__________");
    //    }
    //}
    //menu_->setVisible(true);
}

void BattleScenePaper::onExit()
{
}

void BattleScenePaper::backRun1()
{
    if (slow_ > 0)
    {
        if (current_frame_ % 4) { return; }
        //Engine::delay(100);    //此处不加延迟好像会有闪烁，目前原因不清楚
        //x_ = rand_.rand_int(2) - rand_.rand_int(2);
        //y_ = rand_.rand_int(2) - rand_.rand_int(2);
        slow_--;
    }
    for (auto r : battle_roles_)
    {
        r->HurtThisFrame = 0;
        for (auto m : r->getLearnedMagics())
        {
            if (special_magic_effect_every_frame_.count(m->Name))
            {
                special_magic_effect_every_frame_[m->Name](r);
            }
        }
        decreaseToZero(r->Frozen);
        decreaseToZero(r->Shake);
        decreaseToZero(r->Posture, 0.05);
        decreaseToZero(r->Breathless);
        if (r->Posture >= MAX_POSTURE && r->Breathless == 0)
        {
            //气绝状态
            r->Breathless = 300;
            r->Shake = 300;
        }
        if (r->Frozen == 0)
        {
            //更新速度，加速度，力学位置
            {
                double rate = 1.0;
                auto p = r->Pos + r->Velocity;
                int dis = -1;
                if (r->OperationType == 3) { dis = 1; }
                if (canWalk90(p, r, dis))
                {
                    r->Pos = p;
                }
                else
                {
                    r->Velocity = { 0, 0, 0 };
                }
                //r->FaceTowards = rand_.rand() * 4;
                if (r->Pos.z < 0)
                {
                    r->Pos.z = 0;
                }
                if (r->Pos.z <= 0 && r->Velocity.norm() != 0)
                {
                    auto f = -r->Velocity;
                    f.z = 0;
                    //if (r->Velocity.z == 0)
                    {
                        f.normTo(friction_);    //摩擦力
                    }
                    r->Acceleration = { f.x, f.y, gravity_ };
                }
                else
                {
                    r->Acceleration = { 0, 0, gravity_ };
                }
                auto prev_v = r->Velocity;
                r->Velocity += r->Acceleration;
                //摩擦力不可反向的修正
                if (prev_v.x * r->Velocity.x < 0)
                {
                    r->Velocity.x = 0;
                }
                if (prev_v.y * r->Velocity.y < 0)
                {
                    r->Velocity.y = 0;
                }
                if (r->Pos.z == 0)
                {
                    r->Velocity.z = 0;
                }
                if (r->Velocity.norm() < 0.1)
                {
                    r->Velocity.x = 0;
                    r->Velocity.y = 0;
                }
            }
        }
        //else
        //{
        //    r->Velocity = { 0, 0 };
        //    if (r->HP <= 0)
        //    {
        //        r->Dead = 1;
        //        //此处只为严格化，但与击退部分可能冲突
        //    }
        //}
        decreaseToZero(r->CoolDown);
        if (r->OperationType == 5)
        {
            //防御需要持续按住
        }
        else if (r->CoolDown == 0)
        {
            if (!r->Dead)
            {
                if (current_frame_ % 3 == 0)
                {
                    r->PhysicalPower += 1;
                }
                r->MP += 1;
                r->ActFrame = 0;
            }
            //r->OperationType = -1;
            r->ActType = -1;
            r->HaveAction = 0;
        }

        decreaseToZero(r->HurtFrame);
        decreaseToZero(r->Attention);
        decreaseToZero(r->Invincible);
    }
    {
        int current_frame2 = current_frame_;
        for (auto r : battle_roles_)
        {
            //有行动
            Action(r);
        }
        for (auto r : battle_roles_)
        {
            //ai策略
            AI(r);
        }
    }

    //效果
    //if (current_frame_ % 2 == 0)
    {
        for (auto& ae : attack_effects_)
        {
            ae.Frame++;
            ae.Velocity += ae.Acceleration;
            ae.Pos += ae.Velocity;
            Role* r = nullptr;
            if (ae.Attacker)
            {
                r = findNearestEnemy(ae.Attacker->Team, ae.Pos);
            }
            if (ae.Track && r)
            {
                //追踪
                double n = ae.Velocity.norm();
                auto p = (r->Pos - ae.Pos).normTo(n / 20.0);
                ae.Velocity += p;
                ae.Velocity.normTo(n);
            }
            //是否打中了敌人
            if (r && !r->HurtFrame
                && !r->Invincible
                && r->Dead == 0
                && ae.Attacker
                && r->Team != ae.Attacker->Team
                && ae.Defender.count(r) == 0
                && EuclidDis(r->Pos, ae.Pos) < TILE_W * 2)
            {
                if (ae.UsingMagic && ae.OperationType != 5)
                {
                    Audio::getInstance()->playESound(ae.UsingMagic->EffectID);
                }
                ae.Defender[r]++;
                shake_ = 5;
                //sword_light_ = 10;
                //r->Frozen = 20;
                //r->Shake = 20;
                //slow_ = 1;
                if (ae.OperationType == 0)
                {
                    Engine::getInstance()->gameControllerRumble(100, 100, 50);
                    //if (special_magic_effect_beat_.count(ae.UsingMagic->Name) == 0)
                    //{
                    defaultMagicEffect(ae, r);
                    //}
                    //else
                    //{
                    //    special_magic_effect_beat_[ae.UsingMagic->Name](ae, r);
                    //}
                }
                //std::vector<std::string> = {};
            }
        }
        //}
        //删除播放完毕的
        for (auto it = attack_effects_.begin(); it != attack_effects_.end();)
        {
            if (it->Frame >= it->TotalFrame)
            {
                it = attack_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    //此处计算累积伤害
    for (auto r : battle_roles_)
    {
        int hurt = r->HurtThisFrame;
        if (hurt > 0)
        {
            TextEffect te;
            Color c = { 255, 255, 255, 255 };
            if (r->Team == 0)
            {
                c = { 255, 20, 20, 255 };
            }
            te.set(std::to_string(-hurt), c, r);
            text_effects_.push_back(std::move(te));
            AttackEffect ae1;
            ae1.FollowRole = r;
            //ae1.EffectNumber = eft[rand_.rand() * eft.size()];
            ae1.setPath(std::format("eft/bld{:03}", int(rand_.rand() * 5)));
            ae1.TotalFrame = ae1.TotalEffectFrame;
            ae1.Frame = 0;
            attack_effects_.push_back(std::move(ae1));
            r->HP -= hurt;
            if (r->HP <= 0)
            {
                //LOG("{} has been beat\n", r->Name);
                r->Dead = 1;
                r->HP = 0;
                //r->Velocity = r->Pos - ae1.Attacker->Pos;
                r->Velocity.normTo(15);    //因为已经有击退速度，可以直接利用
                r->Velocity.z = 12;
                r->Velocity.normTo(std::min(hurt / 2.0, 30.0));
                //r->Velocity.normTo(hurt / 2.0);
                r->Frozen = 5;
                x_ = rand_.rand_int(2) - rand_.rand_int(2);
                y_ = rand_.rand_int(2) - rand_.rand_int(2);
                //dying_ = r;
                pos_ = r->Pos;
                frozen_ = 5;
                shake_ = 10;
                slow_ = 10;
                close_up_ = 30;
            }
        }
        //限制属性
        r->HP = GameUtil::clamp(r->HP, 0, r->MaxHP);
        r->MP = GameUtil::clamp(r->MP, 0, r->MaxMP);
        r->PhysicalPower = GameUtil::clamp(r->PhysicalPower, 0, 100);
        r->Posture = GameUtil::clamp(r->Posture, 0.0, MAX_POSTURE + 1);
    }
    //处理文字
    {
        for (auto& te : text_effects_)
        {
            if (te.Type == 0) { te.Pos.y -= 2; }
            te.Frame++;
        }
        for (auto it = text_effects_.begin(); it != text_effects_.end();)
        {
            if (it->Frame >= 30)
            {
                it = text_effects_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
    {
        //人物出场
        if (getTeamMateCount(1) < getTeamMateCount(0) + 1)
        {
            if (!enemies_.empty())
            {
                battle_roles_.push_back(enemies_.front());
                head_boss_[0]->setRole(enemies_.front());
                enemies_.pop_front();
                battle_roles_.back()->Attention = 30;
                battle_roles_.back()->CoolDown = 30;
                battle_roles_.back()->Invincible = 30;
            }
        }
        ////亮血条
        //if (enemies_.size() < head_boss_.size())
        //{
        //    for (int i = 0; i < head_boss_.size(); i++)
        //    {
        //        if (i >= enemies_.size())
        //        {
        //            head_boss_[i]->setVisible(true);
        //        }
        //    }
        //}
        //检测战斗结果
        int battle_result = checkResult();

        if (battle_result >= 0)
        {
            if (result_ == -1)
            {
                //pos_ = dying_->Pos;
                close_up_ = 60;
                frozen_ = 60;
                slow_ = 30;
                shake_ = 40;
                result_ = battle_result;
            }
            if (slow_ == 0 && (result_ == 0 || result_ == 1))
            {
                //menu_->setVisible(false);
                calExpGot();
                setExit(true);
            }
        }
    }
}

void BattleScenePaper::Action(Role* r)
{
    if (r->HaveAction)
    {
        if (r && !r->Dead && r->CoolDown == 0)
        {
            auto r1 = findNearestEnemy(r->Team, r->Pos);
            if (r1)
            {
                r->RealTowards = r1->Pos - r->Pos;
            }
        }
        //音效和动画
        if (r->OperationType >= 0
            //&& r->ActFrame == r->FightFrame[r->ActType] - 3
            && r->ActFrame == calCast(r->ActType, r->OperationType, r))
        {
            //r->HaveAction = 0;
            r->PreActTimer = current_frame_;
            for (auto m : r->getLearnedMagics())
            {
                if (special_magic_effect_attack_.count(m->Name))
                {
                    special_magic_effect_attack_[m->Name](r);
                }
            }
            Magic* magic = nullptr;
            if (r->UsingMagic)
            {
                magic = r->UsingMagic;
            }
            else
            {
                std::vector<Magic*> v;
                for (int i = 0; i < ROLE_MAGIC_COUNT; i++)
                {
                    if (r->MagicID[i] > 0)
                    {
                        auto m = Save::getInstance()->getMagic(r->MagicID[i]);
                        if (m->MagicType == r->ActType)
                        {
                            v.push_back(m);
                        }
                    }
                }
                if (!v.empty())
                {
                    magic = v[rand_.rand() * v.size()];
                }
            }
            AttackEffect ae;
            if (magic)
            {
                ae.setEft(magic->EffectID);
                ae.UsingMagic = magic;
            }
            else
            {
                ae.setEft(11);
                magic = Save::getInstance()->getMagic(1);
                ae.UsingMagic = Save::getInstance()->getMagic(1);
            }
            //防御不播放音效
            if (r->OperationType == 0)
            {
                if (magic)
                {
                    Audio::getInstance()->playASound(magic->SoundID);
                }
                else
                {
                    Audio::getInstance()->playESound(r->ActType);
                }
            }
            r->PhysicalPower = GameUtil::clamp(r->PhysicalPower - 3, 0, Role::getMaxValue()->PhysicalPower);
            int level_index = r->getMagicLevelIndex(magic->ID);
            int needMP = magic->calNeedMP(level_index);
            ae.TotalFrame = 30;
            //r->CoolDown += ae.TotalFrame;
            ae.Attacker = r;
            r->RealTowards.normTo(1);
            ae.Pos = r->Pos + TILE_W * 2.0 * r->RealTowards;
            ae.Frame = 0;
            if (r->Team == 0 && r == role_)
            {
                ae.OperationType = r->OperationType;
                //if (r->OperationType == 0 && ae.UsingMagic->AttackAreaType != 0)
                //{
                //    ae.OperationType = -1;
                //}
                //if (r->OperationType == 2 && (ae.UsingMagic->AttackAreaType != 1 && ae.UsingMagic->AttackAreaType != 2))
                //{
                //    ae.OperationType = -1;
                //}
                //if (r->OperationType == 1 && ae.UsingMagic->AttackAreaType != 3)
                //{
                //    ae.OperationType = -1;
                //}
            }
            else
            {
                ae.OperationType = r->OperationType;
                if (r->OperationType == -1)
                {
                    if (ae.UsingMagic->AttackAreaType == 0)
                    {
                        ae.OperationType = 0;
                    }
                    else if (ae.UsingMagic->AttackAreaType == 1 || ae.UsingMagic->AttackAreaType == 2)
                    {
                        ae.OperationType = 2;
                    }
                    else if (ae.UsingMagic->AttackAreaType == 3)
                    {
                        ae.OperationType = 1;
                    }
                }
            }

            int index = r->getMagicOfRoleIndex(ae.UsingMagic);
            if (index >= 0)
            {
                r->MagicLevel[index] = GameUtil::clamp(r->MagicLevel[index] + rand_.rand() * 2 + 1, 0, 999);
            }
            //根据性质创造攻击效果
            if (ae.OperationType == 0)
            {
                ae.TotalFrame = 10;
                //if (r->OperationCount == 3 && magic->AttackAreaType == 0)
                {
                    ae.TotalFrame = 10;
                    shake_ = 10;
                    //ae.Strengthen = 2;
                    ae.Velocity = r->RealTowards;
                    ae.Velocity.normTo(magic->SelectDistance[level_index] / 2.0);
                    //ae.Velocity.normTo(5);
                    ae.Track = 1;
                }
                auto r1 = findNearestEnemy(r->Team, r->Pos);
                if (r1 && r1->Breathless)
                {
                    //气绝突进
                    ae.TotalFrame = 30;
                    //ae.Pos = r->Pos;
                    r->Velocity = r1->Pos - r->Pos;
                    r->RealTowards = r->Velocity;
                    ae.Pos = r->Pos;
                    ae.Velocity = r1->Pos - ae.Pos;
                    auto v = std::min(10.0, r->Speed / 15.0);
                    ae.Velocity.normTo(v);
                    r->Velocity.normTo(v);
                    //r->Velocity.z = 4;
                }
                attack_effects_.push_back(std::move(ae));
                needMP *= 0.1;
            }
            else if (ae.OperationType == 3)
            {
                if (r->HeadID == 0)
                {
                    int i = 0;
                }
                auto acc = r->RealTowards;
                acc.normTo(std::min(10.0, r->Speed / 15.0));
                r->Velocity = acc;
                r->Velocity.z = 1;
                //r->Acceleration += acc;
                //r->VelocitytFrame = 10;
                r->ActType = -1;
                auto p = ae.Pos;
                int count = std::min(3, (r->Speed + r->getActProperty(ae.UsingMagic->MagicType)) / 60);
                for (int i = 0; i < count; i++)
                {
                    ae.Pos = p + r->Velocity * (i - 1) * 2;
                    ae.Frame += 3;
                    //attack_effects_.push_back(ae);
                }
                needMP *= 0.05;
            }
            //LOG("{} use {} as {}\n", ae.Attacker->Name, ae.UsingMagic->Name, ae.OperationType);
            r->MP -= needMP;
            r->UsingMagic = nullptr;
        }

        if (r->UsingItem)
        {
            Item* item = r->UsingItem;
            if (item->ItemType == 3)
            {
                // 药品直接服用
                r->useItem(item);
                //TextEffect te;
                //BP_Color c = { 255, 255, 255, 255 };
                //if (r->Team == 0)
                //{
                //    c = { 255, 20, 220, 20 };
                //}
                //const int left = std::max(0, Save::getInstance()->getItemCountInBag(item->ID) - 1);
                //te.set(std::format("服用{}，剩余{}", item->Name, left), c, r);
                //text_effects_.push_back(std::move(te));
            }
            else if (item->ItemType == 4)
            {
                // 暗器
                AttackEffect ae1;
                auto r0 = findFarthestEnemy(r->Team, r->Pos);
                if (r0)
                {
                    ae1.Velocity = r0->Pos - r->Pos;
                }
                else
                {
                    ae1.Velocity = r->RealTowards;
                }
                ae1.Velocity.normTo(10);
                ae1.Attacker = r;
                ae1.Pos = r->Pos;
                ae1.UsingHiddenWeapon = item;
                ae1.Through = 0;
                ae1.setEft(item->HiddenWeaponEffectID);
                ae1.TotalFrame = 100;
                ae1.Frame = 0;
                ae1.OperationType = 4;
                attack_effects_.push_back(std::move(ae1));
            }
            // 减少数量
            Event::getInstance()->addItemWithoutHint(item->ID, -1);
            r->UsingItem = nullptr;
        }

        if (r->OperationType == 1)
        {
            r->ActFrame++;
            if (r->ActFrame >= 7)
            {
                shake_ = 1;
            }
        }
        else
        {
            r->ActFrame++;
        }
        if (r->OperationType == 5)
        {
            //防御动作目前使用最大帧
            r->ActFrame = 30;
        }
    }
}

void BattleScenePaper::AI(Role* r)
{
    if ((r != role_ || r->Auto)
        && r->Dead == 0)
    {
        if (r->CoolDown == 0 && r->Breathless == 0)
        {
            if (r->UsingMagic == nullptr)
            {
                //ai可以使用所有武学
                auto v = r->getLearnedMagics();
                if (v.size() == 1)
                {
                    r->UsingMagic = v[0];
                }
                else if (v.size() >= 0)
                {
                    std::vector<double> hurt;
                    double sum = 0;
                    for (auto m : v)
                    {
                        double h = m->Attack[r->getMagicLevelIndex(m)];
                        h = exp(h / 500);    //几率正比于武功威力
                        hurt.push_back(sum + h);
                        sum += h;
                    }
                    double select = rand_.rand() * sum;
                    for (int i = 0; i < hurt.size(); i++)
                    {
                        if (select < hurt[i])
                        {
                            r->UsingMagic = v[i];
                            break;
                        }
                    }
                }
            }
            if (r->UsingMagic)
            {
                r->EquipMagic[0] = r->UsingMagic->ID;
                if (r == role_)
                {
                    r->HaveAction = 0;    //因防御是持续的，所以需要重置
                    for (auto& ae : attack_effects_)
                    {
                        if (ae.Attacker && ae.Attacker->Team != r->Team && EuclidDis(r->Pos, ae.Pos) <= TILE_W * 2.1)
                        {
                            //LOG("block\n");
                            r->OperationType = 5;
                            r->HaveAction = 1;
                            r->ActFrame = 30;
                            r->ActType = r->UsingMagic->MagicType;
                            r->CoolDown = 0;
                            break;
                        }
                    }
                }
            }
            auto r0 = findNearestEnemy(r->Team, r->Pos);
            if (r0 && !r->HaveAction)
            {
                r->RealTowards = r0->Pos - r->Pos;
                //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                r->RealTowards.normTo(1);
                int dis = TILE_W * 3;
                //if (r->UsingMagic)
                //{
                //    if (r->UsingMagic->AttackAreaType == 3) { dis = 180; }
                //    if (r->UsingMagic->AttackAreaType == 1 || r->UsingMagic->AttackAreaType == 2) { dis = 300; }
                //}
                double speed = r->Speed / 30.0;
                if (EuclidDis(r->Pos, r0->Pos) > dis)
                {
                    auto p = r->Pos + speed * r->RealTowards;
                    if (canWalk90(p, r) && r->FindingWay == 0)
                    {
                        //能否闪身的条件，似乎比较复杂
                        if (rand_.rand() < 0.25 && r->Speed >= 60
                            && (r != role_ && r->UsingMagic
                                || r == role_ && r->getEquipMagicOfRoleIndex(r->UsingMagic) == 3))
                        {
                            r->OperationType = 3;
                        }
                        else
                        {
                            r->OperationType = -1;
                        }
                        if (r->OperationType == 3)
                        {
                            r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                            r->ActFrame = 0;
                            r->HaveAction = 1;
                        }
                        else
                        {
                            r->Pos = p;
                        }
                    }
                    else if (r->Velocity.norm() < 0.1)
                    {
                        //用复杂路径法查找一个目标并接近
                        MapSquareInt dis_layer;
                        dis_layer.resize(COORD_COUNT);
                        auto p_enemy45 = pos90To45(r0->Pos.x, r0->Pos.y);
                        calDistanceLayer(p_enemy45.x, p_enemy45.y, dis_layer, 64);
                        auto p_self45 = pos90To45(r->Pos.x, r->Pos.y);
                        int max_dis45 = 4096;
                        Pointf p_target = r->Pos;
                        for (int x = p_self45.x - 1; x <= p_self45.x + 1; x++)
                        {
                            for (int y = p_self45.y - 1; y <= p_self45.y + 1; y++)
                            {
                                if (calDistance(x, y, p_self45.x, p_self45.y) != 1)
                                {
                                    continue;
                                }
                                if (isOutLine(x, y))
                                {
                                    continue;
                                }
                                auto p1 = pos45To90(x, y);
                                double dis1 = dis_layer.data(x, y) + 1 * (rand_.rand() - rand_.rand());
                                if (canWalk90(p1, r) && dis1 < max_dis45)
                                {
                                    max_dis45 = dis1;
                                    p_target = p1;
                                }
                            }
                        }
                        r->FindingWay = 1;
                        r->RealTowards = p_target - r->Pos;
                        if (rand_.rand() < 0.25 && r->Speed >= 60
                            && (r != role_ && r->UsingMagic
                                || r == role_ && r->getEquipMagicOfRoleIndex(r->UsingMagic) == 3))
                        {
                            r->OperationType = 3;
                        }
                        else
                        {
                            r->OperationType = -1;
                        }
                        if (r->OperationType == 3)
                        {
                            r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                            r->ActFrame = 0;
                            r->HaveAction = 1;
                        }
                        else
                        {
                            r->RealTowards = p_target - r->Pos;
                            //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                            auto distance = r->RealTowards.norm();
                            //r->FaceTowards = readTowardsToFaceTowards(r->RealTowards);
                            r->RealTowards.normTo(1);
                            //r->Pos = p2;
                            r->Velocity = r->RealTowards * speed;
                            //todo:r->VelocitytFrame = 3;
                        }
                    }
                }
                else
                {
                    r->FindingWay = 0;
                    if (r->PhysicalPower >= 30 && r->UsingMagic)
                    {
                        //点攻击疯狗咬即可
                        if (rand_.rand() < 0.5 && EuclidDis(r->Pos, r0->Pos) < TILE_W * 2.5)
                        {
                            //attack
                            auto m = r->UsingMagic;
                            if (m)
                            {
                                r->OperationType = 0;    // 只有点攻击
                                r->CoolDown = calCoolDown(m->MagicType, r->OperationType, r);
                                r->ActFrame = 0;
                                r->ActType = m->MagicType;
                                r->HaveAction = 1;
                            }
                        }
                        else
                        {
                            if (rand_.rand() < 0.5)
                            {
                                r->OperationType = 3;
                            }
                            if (r->OperationType == 3)
                            {
                                if (EuclidDis(r->Pos, r0->Pos) < TILE_W * 3)
                                {
                                    //近身时随机移动一下，增加一些变数
                                    r->RealTowards.rotate(M_PI * 0.75 * (2 * rand_.rand() - 1));
                                }
                                else
                                {
                                    r->RealTowards = r0->Pos - r->Pos;
                                }
                                r->OperationType = 3;
                                r->CoolDown = calCoolDown(r->UsingMagic->MagicType, r->OperationType, r);
                                r->ActFrame = 0;
                                r->HaveAction = 1;
                                //r->RealTowards *= 3;
                            }
                        }
                        if (!r->HaveAction)
                        {
                            //走两步
                            r->RealTowards.rotate(M_PI * 0.5 * (2 * rand_.rand() - 1));
                            //r->FaceTowards = realTowardsToFaceTowards(r->RealTowards);
                            r->Velocity = r->RealTowards;
                            r->Velocity.normTo(speed);
                            //todo:r->VelocitytFrame = 20;
                        }
                    }
                }
            }
        }
    }
}

int BattleScenePaper::checkResult()
{
    int team0 = getTeamMateCount(0);
    int team1 = enemies_.size() + getTeamMateCount(1);
    if (team0 > 0 && team1 == 0)
    {
        return 0;
    }
    if (team0 == 0 && team1 >= 0)
    {
        return 1;
    }
    return -1;
}

void BattleScenePaper::setRoleInitState(Role* r)
{
    BattleScene::setRoleInitState(r);
    if (r->Team == 0)
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }
    else
    {
        r->HP = r->MaxHP;
        r->MP = r->MaxMP;
        r->PhysicalPower = (std::max)(r->PhysicalPower, 90);
    }

    auto p = pos45To90(r->X(), r->Y());

    r->Pos.x = p.x;
    r->Pos.y = p.y;
    if (r->FaceTowards == Towards_RightDown)
    {
        r->RealTowards = { 1, 1 };
    }
    if (r->FaceTowards == Towards_RightUp)
    {
        r->RealTowards = { 1, -1 };
    }
    if (r->FaceTowards == Towards_LeftDown)
    {
        r->RealTowards = { -1, 1 };
    }
    if (r->FaceTowards == Towards_LeftUp)
    {
        r->RealTowards = { -1, -1 };
    }
    r->Acceleration = { 0, 0, gravity_ };
    r->Posture = 25;
}

void BattleScenePaper::renderExtraRoleInfo(Role* r, double x, double y)
{
    if (r == nullptr || r->Dead)
    {
        return;
    }
    // 血条
    Color outline_color = { 0, 0, 0, 128 };
    Color background_color = { 0, 255, 0, 128 };    // 我方绿色
    if (r->Team == 1)
    {
        // 敌方红色
        background_color = { 255, 0, 0, 128 };
    }
    int hp_max_w = 24;
    int hp_x = x - hp_max_w / 2;
    int hp_y = y - 60;
    int hp_h = 3;
    double perc = ((double)r->HP / r->MaxHP);
    if (perc < 0)
    {
        perc = 0;
    }
    double alpha = 1;
    if (r->HP <= 0)
    {
        alpha = dead_alpha_ / 255.0;
    }
    Rect r0 = { hp_x - 1, hp_y - 1, hp_max_w + 2, hp_h + 2 };
    Engine::getInstance()->renderSquareTexture(&r0, outline_color, 128 * alpha);
    Rect r1 = { hp_x, hp_y, int(perc * hp_max_w), hp_h };
    Engine::getInstance()->renderSquareTexture(&r1, background_color, 128 * alpha);
    //架势条
    background_color = { 255, 165, 0, 128 };
    int posture = r->Posture * (hp_max_w + 2) / MAX_POSTURE;
    posture = posture / 2 * 2;
    Rect r2 = { hp_x + hp_max_w / 2 - posture / 2, hp_y + 5, posture, hp_h - 1 };
    Engine::getInstance()->renderSquareTexture(&r2, background_color, 192 * alpha);
}

Role* BattleScenePaper::findNearestEnemy(int team, Pointf p)
{
    double dis = 4096;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 < dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

int BattleScenePaper::realTowardsToCameraFaceTowards(const Pointf& dir, const Pointf& view_dir, const Pointf& paper_right, int current_face_towards)
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

bool BattleScenePaper::isPaperWallTile(int num)
{
    return (num >= 701 && num <= 1139)
        || (num >= 1410 && num <= 1436)
        || (num >= 1505 && num <= 1621)
        || (num >= 1816 && num <= 1849)
        || (num >= 2116 && num <= 2144)
        || (num >= 2184 && num <= 2285);
}

bool BattleScenePaper::isBuilding(int x, int y)
{
    int num = building_layer_.data(x, y) / 2;
    if (isPaperWallTile(num))
    {
        return true;
    }
    return BattleSceneAct::isBuilding(x, y);
}

Role* BattleScenePaper::findFarthestEnemy(int team, Pointf p)
{
    double dis = 0;
    Role* r0 = nullptr;
    for (auto r1 : battle_roles_)
    {
        if (r1->Dead == 0 && team != r1->Team)
        {
            auto dis1 = EuclidDis(p, r1->Pos);
            if (dis1 > dis)
            {
                dis = dis1;
                r0 = r1;
            }
        }
    }
    return r0;
}

//前摇
int BattleScenePaper::calCast(int act_type, int operation_type, Role* r)
{
    int v[4] = { 10, 20, 15, 2 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        return v[operation_type];
    }
    return 0;
}

//冷却减去前摇就是后摇
//需注意攻击判定可能仍然存在，严格来说攻击判定存在的时间加上前摇应小于冷却
int BattleScenePaper::calCoolDown(int act_type, int operation_type, Role* r)
{
    int i = r->getActProperty(act_type);
    int v[] = { 100 - i / 2, 160 - i, 70 - i / 2, 5, 0 };
    int min_v[] = { 20, 45, 30, 5, 0 };
    if (operation_type >= 0 && operation_type <= 3)
    {
        int c = std::max(min_v[operation_type], v[operation_type]);
        if (r->AttackTwice > 0)
        {
            c *= 0.666;
            c = std::max(calCast(act_type, operation_type, r) + 2, c);
        }
        return c;
    }
    else
    {
        return 0;
    }
}

void BattleScenePaper::defaultMagicEffect(AttackEffect& ae, Role* r)
{
    if (ae.NoHurt)
    {
        return;
    }
    if (r == role_)
    {
        head_boss_[0]->setRole(ae.Attacker);
    }
    if (r == ae.Attacker)
    {
        head_boss_[0]->setRole(r);
    }
    double hurt;
    //先特别处理暗器
    if (ae.UsingHiddenWeapon != nullptr)
    {
        hurt = calHiddenWeaponHurt(ae.Attacker, r, ae.UsingHiddenWeapon) / 5;
    }
    else
    {
        hurt = calMagicHurt(ae.Attacker, r, ae.UsingMagic);
    }
    hurt -= ae.Weaken;                             //弱化
    hurt *= ae.Strengthen;                         //强化
    hurt *= 1 - 0.5 * ae.Frame / ae.TotalFrame;    //距离衰减
    //角度
    auto atk_dir = ae.Pos - r->Pos;
    auto angle = acos((atk_dir.x * r->RealTowards.x + atk_dir.y * r->RealTowards.y) / atk_dir.norm() / r->RealTowards.norm());
    if (angle >= M_PI * 0.25 && angle < M_PI * 0.75)
    {
        hurt *= 1.5;
    }
    else if (angle >= M_PI * 0.75)
    {
        hurt *= 2;
    }
    //操作类型的伤害效果
    if (ae.OperationType == 0)
    {
    }
    //击退
    auto v = r->Pos - ae.Attacker->Pos;
    v.normTo(2);
    r->Velocity += v;
    if (r->Velocity.norm() > 2)
    {
        r->Velocity.normTo(2);
    }
    r->HurtFrame = 1;    //相当于无敌时间

    //用内力抵消硬直
    if (r->MP >= hurt / 10)
    {
        r->MP -= hurt / 10;
    }
    else
    {
        //r->Frozen += 10;    //硬直
        //r->ActType = -1;
        //r->ActType2 = -1;
    }

    //武功类型特殊效果
    if (ae.UsingMagic)
    {
        int act_type = ae.UsingMagic->MagicType;
        if (rand_.rand() < r->getActProperty(act_type) / 200.0)
        {
            if (act_type == 1)
            {
                //r->Frozen += 10;    //拳法打硬直
            }
            if (act_type == 2)
            {
                ae.Attacker->CoolDown *= 0.5;    //剑法冷却缩短
            }
            if (act_type == 3)
            {
                hurt *= 1.5;    //刀法暴击
            }
            if (act_type == 4)
            {
                //特殊会随机附加行动方向
                Pointf p{ float(rand_.rand()), float(rand_.rand()), 0 };
                p.normTo(1);
                r->Velocity += p;
            }
        }
    }
    //添加一点随机性
    hurt += 5 * (rand_.rand() - rand_.rand());
    //若无法破防，则随机一个小的数字
    if (hurt <= 0)
    {
        hurt = 1 + rand_.rand() * 3;
    }
    //无贯穿则后面不会再造成伤害，再播放一下
    if (ae.Through == 0)
    {
        ae.NoHurt = 1;
        ae.Frame = std::max(ae.TotalFrame - 15, ae.Frame);
    }

    double hurtPosture = hurt * 0.2;

    if (r->OperationType == 5)
    {
        //有防御
        //LOG("{}, ", r->OperationCount);
        int block_frame = 2;
        if (easy_block_)
        {
            //block_frame = 500;
        }
        auto r1 = ae.Attacker;
        //auto frame = calCast(r1->ActType, r1->OperationType, r1);
        int sound_id = -1;
        auto m = Save::getInstance()->getMagic(r1->EquipMagic[0]);
        if (m)
        {
            sound_id = m->SoundID;
        }
        Audio::getInstance()->playASound(sound_id);
        if (r->OperationCount <= block_frame)
        {
            //LOG("block1\n");
            //完美格挡
            auto posture = hurt / 2;
            if (posture < 75) { posture = 75; }
            ae.Attacker->Posture += posture;
            decreaseToZero(r->Posture, posture / 2);
            hurt = 0;
            sword_light_ = 30;
            sword_light_color_ = { 255, 255, 255, 255 };
            ae.Attacker->Velocity = r->RealTowards - ae.Attacker->RealTowards;
            ae.Attacker->Velocity.normTo(2);
            //ae.Attacker->Shake = 30;
            ae.Attacker->CoolDown = 30;
            shake_ = 30;
            r->Velocity.normTo(0);
            Audio::getInstance()->playASound(sound_id);
        }
        else
        {
            ae.Attacker->Posture += hurt * 0.01;
            r->Posture += hurt * 0.1;
            r->HurtThisFrame += hurt * 0.33;
            sword_light_ = 10;
            sword_light_color_ = { 255, 255, 255, 255 };
            r->Velocity.normTo(0);
            ae.Attacker->ExpGot += hurt / 2;
        }
    }
    else
    {
        //无防御
        ae.Attacker->ExpGot += hurt / 2;
        //扣HP或MP
        if (ae.UsingHiddenWeapon || ae.UsingMagic->HurtType == 0)
        {
            if (r->Breathless)
            {
                r->HurtThisFrame += hurt * 10;
                if (r->HurtThisFrame < r->HP + 100)
                {
                    r->HurtThisFrame = r->HP + 100;
                }
                sword_light_ = 40;
                sword_light_color_ = { 255, 0, 0, 255 };
                if (ae.UsingMagic)
                {
                    Audio::getInstance()->playESound(ae.UsingMagic->EffectID);
                }
            }
            else
            {
                r->HurtThisFrame += hurt;
                r->Posture += hurt * 0.2;
            }
        }
        if (ae.UsingMagic && ae.UsingMagic->HurtType == 1)
        {
            r->MP -= hurt;
            ae.Attacker->MP += hurt * 0.8;
            //TextEffect te;
            //te.set(std::to_string(int(-hurt)), { 160, 32, 240, 255 }, r);
            //text_effects_.push_back(std::move(te));
        }
    }
    //LOG("{} attack {} with {} as {}, hurt {}\n", ae.Attacker->Name, r->Name, ae.UsingMagic->Name, ae.OperationType, int(hurt));
}

int BattleScenePaper::calRolePic(Role* r, int style, int frame)
{
    if (style < 0 || style >= 5)
    {
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                return r->FightFrame[i] * r->FaceTowards;
            }
        }
    }
    if (r->FightFrame[style] <= 0)
    {
        //改为选一个存在的动作，否则看不出是在攻击
        for (int i = 0; i < 5; i++)
        {
            if (r->FightFrame[i] > 0)
            {
                style = i;
            }
        }
    }
    int total = 0;
    for (int i = 0; i < 5; i++)
    {
        if (i == style)
        {
            //停留在最后一帧
            if (frame < r->FightFrame[style] - 2)
            {
                return total + r->FightFrame[style] * r->FaceTowards + frame;
            }
            else
            {
                return total + r->FightFrame[style] * r->FaceTowards + r->FightFrame[style] - 2;
            }
        }
        total += r->FightFrame[i] * 4;
    }
    return r->FaceTowards;
}
