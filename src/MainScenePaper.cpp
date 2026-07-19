#include "MainScenePaper.h"
#include "Console.h"
#include "DayNightSystem.h"
#include "Engine.h"
#include "GameUtil.h"
#include "Random.h"
#include "SubScene.h"
#include "TextureManager.h"
#include "UI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace
{
constexpr float CameraRotateStep = 0.06f;
constexpr float CameraHeightStep = 8.0f;
constexpr float ControllerCameraRotateSpeed = 0.035f;
constexpr float CameraZoomStep = 24.0f;
constexpr float CameraMinDistance = 220.0f;
constexpr float CameraMaxDistance = 900.0f;
constexpr float Pi = 3.14159265358979323846f;
}

MainScenePaper::MainScenePaper()
{
}

Pointf MainScenePaper::pos45To90(int x, int y) const
{
    Pointf p;
    p.x = -y * TILE_W + x * TILE_W + COORD_COUNT * TILE_W;
    p.y = y * TILE_W + x * TILE_W;
    p.z = 0;
    return p;
}

bool MainScenePaper::inCameraFront(const std::vector<Pointf>& points)
{
    for (auto& p : points)
    {
        if (camera_.getDepth(p) <= camera_.getNearPlane())
        {
            return false;
        }
    }
    return true;
}

bool MainScenePaper::isMountainTexture(int pic) const
{
    static constexpr std::array<int, 53> mountain_pics = {
        1194, 1195, 1196, 1197, 1198, 1199, 1200, 1201,
        1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209,
        1348, 1349, 1362, 1363, 1367, 1374, 1375, 1376,
        1377, 1381, 1383, 1386, 1388, 1390, 1395, 1399,
        1404, 1405, 1406, 1407, 1408, 1410, 1412, 1413,
        1417, 1419, 1420, 1422, 1423, 1443, 1449, 1450,
        1451, 1452, 1453, 1455, 1457,
    };
    return std::ranges::find(mountain_pics, pic) != mountain_pics.end();
}

void MainScenePaper::renderTexture3D(Texture* texture, const std::vector<Pointf>& world, const std::vector<FPoint>& src, Color color)
{
    auto projected = camera_.getProj(world);
    std::vector<FPoint> dst;
    dst.reserve(projected.size());
    for (auto& p : projected)
    {
        dst.push_back({ float(p.x), float(p.y) });
    }
    Engine::setColor(texture, color);
    Engine::getInstance()->renderTexture(texture, nullptr, dst, src);
}

void MainScenePaper::renderGroundPatch3D(Texture* texture, float world_x0, float world_y0, float world_x1, float world_y1, float src_w, float src_h, int mesh_x, int mesh_y)
{
    for (int iy = 0; iy < mesh_y; iy++)
    {
        for (int ix = 0; ix < mesh_x; ix++)
        {
            float x0 = world_x0 + (world_x1 - world_x0) * ix / mesh_x;
            float x1 = world_x0 + (world_x1 - world_x0) * (ix + 1) / mesh_x;
            float y0 = world_y0 + (world_y1 - world_y0) * iy / mesh_y;
            float y1 = world_y0 + (world_y1 - world_y0) * (iy + 1) / mesh_y;
            std::vector<Pointf> world = { { x0, y0, 0 }, { x1, y0, 0 }, { x1, y1, 0 }, { x0, y1, 0 } };
            if (!inCameraFront(world))
            {
                continue;
            }
            float u0 = src_w * ix / mesh_x;
            float u1 = src_w * (ix + 1) / mesh_x;
            float v0 = src_h * iy / mesh_y;
            float v1 = src_h * (iy + 1) / mesh_y;
            std::vector<FPoint> src = { { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 } };
            renderTexture3D(texture, world, src, ambient_color_);
        }
    }
}

void MainScenePaper::onEntrance()
{
    MainScene::onEntrance();
    camera_.setViewport(float(render_center_x_ * 2), float(render_center_y_ * 2));
    paper_sky_.reset();
    paper_sky_.generateClouds();
}

void MainScenePaper::draw()
{
    auto engine = Engine::getInstance();
    ambient_color_ = DayNightSystem::getInstance()->getLighting().ambient_color;
    auto right_axis_x = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTX);
    auto right_axis_y = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHTY);
    if (std::abs(right_axis_x) >= 6000)
    {
        camera_yaw_ -= GameUtil::clamp(right_axis_x, -20000, 20000) / 20000.0f * ControllerCameraRotateSpeed;
    }
    if (std::abs(right_axis_y) >= 6000)
    {
        camera_height_ = std::clamp(camera_height_ + GameUtil::clamp(right_axis_y, -20000, 20000) / 20000.0f * camera_distance_ * ControllerCameraRotateSpeed,
            80.0f, 300.0f);
    }
    engine->setRenderTarget("scene");
    engine->fillColor({ 0, 0, 0, 255 }, 0, 0, -1, -1);

    auto man_pos = pos45To90(man_x_, man_y_);
    camera_focus_ = man_pos;
    Pointf camera_back = { std::sin(camera_yaw_), -std::cos(camera_yaw_), 0 };
    camera_pos_ = man_pos + camera_back * camera_distance_ + Pointf{ 0, 0, camera_height_ };
    camera_.center = camera_focus_;
    camera_.pos = camera_pos_;
    camera_.setFov(PaperSky::CameraFov);
    camera_.setViewport(float(render_center_x_ * 2), float(render_center_y_ * 2));

    paper_sky_.render(engine, render_center_x_ * 2, render_center_y_ * 2, camera_.pos, camera_.center);

    const int earth_size = TILE_W * 480 * 2;
    if (TextureManager::getInstance()->getTextureGroup("mmap-earth")->getTextureCount() > 0)
    {
        for (int j = 0; j < 8; j++)
        {
            for (int i = 0; i < 8; i++)
            {
                auto tex = TextureManager::getInstance()->getTexture("mmap-earth", i + j * 8);
                if (!tex)
                {
                    continue;
                }
                tex->load();
                auto texture = tex->getTexture();
                if (!texture)
                {
                    continue;
                }
                float x0 = float(i * earth_size / 8);
                float x1 = float((i + 1) * earth_size / 8);
                float y0 = float(j * earth_size / 8);
                float y1 = float((j + 1) * earth_size / 8);
                constexpr int mesh_per_chunk = 16;
                renderGroundPatch3D(texture, x0, y0, x1, y1, float(tex->w), float(tex->h), mesh_per_chunk, mesh_per_chunk);
            }
        }
    }
    else
    {
        for (int ix = 0; ix < COORD_COUNT; ix++)
        {
            for (int iy = 0; iy < COORD_COUNT; iy++)
            {
                auto tex = earth_layer_.data(ix, iy).getTexture();
                if (!tex)
                {
                    continue;
                }
                tex->load();
                auto texture = tex->getTexture();
                if (!texture)
                {
                    continue;
                }
                auto p00 = pos45To90(ix, iy);
                auto p10 = pos45To90(ix + 1, iy);
                auto p11 = pos45To90(ix + 1, iy + 1);
                auto p01 = pos45To90(ix, iy + 1);
                std::vector<Pointf> world = { p00, p10, p11, p01 };
                if (!inCameraFront(world))
                {
                    continue;
                }
                std::vector<FPoint> src = { { 0, 0 }, { float(tex->w), 0 }, { float(tex->w), float(tex->h) }, { 0, float(tex->h) } };
                renderTexture3D(texture, world, src, ambient_color_);
            }
        }
    }

    Pointf view_dir = camera_.center - camera_.pos;
    view_dir.z = 0;
    if (view_dir.norm() == 0)
    {
        view_dir = { 0, 1, 0 };
    }
    view_dir.normTo(1);
    Pointf paper_right = { view_dir.y, -view_dir.x, 0 };

    struct PaperSprite
    {
        TextureWarpper* tex = nullptr;
        Texture* texture = nullptr;
        Pointf anchor;
        std::vector<Pointf> world;
        std::vector<FPoint> src;
        float depth = 0;
        int turn = 1;
    };
    std::vector<PaperSprite> sprites;
    sprites.reserve(10000);

    auto calc_depth = [&](const Pointf& p)
    {
        auto v = p - camera_.pos;
        return v.x * view_dir.x + v.y * view_dir.y;
    };

    struct Footprint
    {
        int min_x = std::numeric_limits<int>::max();
        int min_y = std::numeric_limits<int>::max();
        int max_x = std::numeric_limits<int>::min();
        int max_y = std::numeric_limits<int>::min();
    };

    auto is_valid_footprint = [](const Footprint& fp)
    {
        return fp.min_x <= fp.max_x && fp.min_y <= fp.max_y;
    };
    std::unordered_map<int, Footprint> mountain_footprints;
    auto footprint_key = [&](int x, int y)
    {
        return x * COORD_COUNT + y;
    };
    for (int x = 0; x < COORD_COUNT; x++)
    {
        for (int y = 0; y < COORD_COUNT; y++)
        {
            int bx = build_x_layer_.data(x, y);
            int by = build_y_layer_.data(x, y);
            if (bx < 0 || bx >= COORD_COUNT || by < 0 || by >= COORD_COUNT)
            {
                continue;
            }
            auto anchor_tex = building_layer_.data(bx, by).getTexture();
            if (!anchor_tex || !isMountainTexture(anchor_tex->num_))
            {
                continue;
            }
            auto& fp = mountain_footprints[footprint_key(bx, by)];
            fp.min_x = std::min(fp.min_x, x);
            fp.min_y = std::min(fp.min_y, y);
            fp.max_x = std::max(fp.max_x, x);
            fp.max_y = std::max(fp.max_y, y);
        }
    }

    for (int ix = 0; ix < COORD_COUNT; ix++)
    {
        for (int iy = 0; iy < COORD_COUNT; iy++)
        {
            if (auto tex = building_layer_.data(ix, iy).getTexture())
            {
                auto p = pos45To90(ix, iy);
                if (isMountainTexture(tex->num_))
                {
                    tex->load();
                    Texture* pyramid_texture = tex->getTexture();
                    if (!pyramid_texture)
                    {
                        continue;
                    }
                    float ground_v = float(std::clamp(tex->dy, 1, tex->h));
                    float height = ground_v;
                    auto footprint_it = mountain_footprints.find(footprint_key(ix, iy));
                    std::vector<Pointf> base;
                    if (footprint_it != mountain_footprints.end() && is_valid_footprint(footprint_it->second))
                    {
                        auto& fp = footprint_it->second;
                        base = {
                            pos45To90(fp.min_x, fp.min_y),
                            pos45To90(fp.max_x + 1, fp.min_y),
                            pos45To90(fp.max_x + 1, fp.max_y + 1),
                            pos45To90(fp.min_x, fp.max_y + 1),
                        };
                    }
                    else
                    {
                        float base_radius = float(std::max(tex->dx, tex->w - tex->dx));
                        float base_depth = float(std::max(tex->dy, tex->h - tex->dy)) * 0.5f;
                        base = {
                            p + Pointf{ -base_radius, -base_depth, 0 },
                            p + Pointf{ base_radius, -base_depth, 0 },
                            p + Pointf{ base_radius, base_depth, 0 },
                            p + Pointf{ -base_radius, base_depth, 0 },
                        };
                    }
                    Pointf base_center = (base[0] + base[1] + base[2] + base[3]) * 0.25;
                    Pointf top = base_center + Pointf{ 0, 0, height };
                    std::vector<std::vector<Pointf>> faces = {
                        { base[0], base[1], top, top },
                        { base[1], base[2], top, top },
                        { base[2], base[3], top, top },
                        { base[3], base[0], top, top },
                    };
                    for (int k = 0; k < int(faces.size()); k++)
                    {
                        auto& face = faces[k];
                        Pointf center = (face[0] + face[1] + face[2]) * (1.0 / 3.0);
                        float mid_x = float(tex->w) * 0.5f;
                        float top_x = mid_x;
                        float y0 = 0;
                        float y1 = ground_v;
                        std::vector<FPoint> src;
                        if (k == 1 || k == 2)
                        {
                            src = { { mid_x, y1 }, { float(tex->w), y1 }, { top_x, y0 }, { top_x, y0 } };
                        }
                        else
                        {
                            src = { { 0, y1 }, { mid_x, y1 }, { top_x, y0 }, { top_x, y0 } };
                        }
                        sprites.push_back({ nullptr, pyramid_texture, center, face, src, calc_depth(center), 1 });
                    }
                }
                else
                {
                    sprites.push_back({ tex, nullptr, p, {}, {}, calc_depth(p), 1 });
                }
            }
        }
    }

    if (isWater(man_x_, man_y_))
    {
        man_pic_ = SHIP_PIC_0 + Scene::towards_ * SHIP_PIC_COUNT + step_;
    }
    else
    {
        man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;
        if (rest_time_ >= BEGIN_REST_TIME)
        {
            man_pic_ = REST_PIC_0 + Scene::towards_ * REST_PIC_COUNT + (rest_time_ - BEGIN_REST_TIME) / REST_INTERVAL % REST_PIC_COUNT;
        }
    }
    sprites.push_back({ TextureManager::getInstance()->getTexture("mmap", man_pic_), nullptr, man_pos, {}, {}, calc_depth(man_pos), 0 });

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
        if (sprite.texture && !sprite.world.empty())
        {
            if (inCameraFront(sprite.world))
            {
                renderTexture3D(sprite.texture, sprite.world, sprite.src, ambient_color_);
            }
            continue;
        }
        auto tex = sprite.tex;
        if (!tex)
        {
            continue;
        }
        tex->load();
        auto texture = tex->getTexture();
        if (!texture)
        {
            continue;
        }
        float left = -float(tex->dx);
        float right = float(tex->w - tex->dx);
        float top = float(tex->dy);
        float bottom = float(tex->dy - tex->h);
        std::vector<Pointf> paper = {
            sprite.anchor + paper_right * left + Pointf{ 0, 0, top },
            sprite.anchor + paper_right * right + Pointf{ 0, 0, top },
            sprite.anchor + paper_right * right + Pointf{ 0, 0, bottom },
            sprite.anchor + paper_right * left + Pointf{ 0, 0, bottom },
        };
        if (!inCameraFront(paper))
        {
            continue;
        }
        std::vector<FPoint> src = { { 0, 0 }, { float(tex->w), 0 }, { float(tex->w), float(tex->h) }, { 0, float(tex->h) } };
        renderTexture3D(texture, paper, src, ambient_color_);
    }

    auto cursor_pos = pos45To90(cursor_x_, cursor_y_);
    if (auto cursor = TextureManager::getInstance()->getTexture("mmap", 1))
    {
        cursor->load();
        if (auto texture = cursor->getTexture())
        {
            float size = float(TILE_W * 2);
            std::vector<Pointf> cursor_quad = {
                cursor_pos + Pointf{ -size * 0.5f, -size * 0.5f, 1 },
                cursor_pos + Pointf{ size * 0.5f, -size * 0.5f, 1 },
                cursor_pos + Pointf{ size * 0.5f, size * 0.5f, 1 },
                cursor_pos + Pointf{ -size * 0.5f, size * 0.5f, 1 },
            };
            if (inCameraFront(cursor_quad))
            {
                std::vector<FPoint> src = { { 0, 0 }, { float(cursor->w), 0 }, { float(cursor->w), float(cursor->h) }, { 0, float(cursor->h) } };
                renderTexture3D(texture, cursor_quad, src, { 255, 255, 255, 128 });
                Engine::setColor(texture, { 255, 255, 255, 255 });
            }
        }
    }

    engine->renderTextureToMain("scene");
}

void MainScenePaper::dealEvent(EngineEvent& e)
{
    auto engine = Engine::getInstance();
    if (force_submap_ >= 0)
    {
        setVisible(true);
        auto sub_map = std::make_shared<SubScene>(force_submap_);
        sub_map->setManViewPosition(force_submap_x_, force_submap_y_);
        sub_map->setTowards(towards_);
        sub_map->setForceBeginEvent(force_event_);
        sub_map->run();
        towards_ = sub_map->towards_;
        force_submap_ = -1;
        force_event_ = -1;
    }

    if (e.type == EVENT_KEY_UP && e.key.key == K_TAB
        || (e.type == EVENT_GAMEPAD_BUTTON_UP && e.gbutton.button == GAMEPAD_BUTTON_BACK))
    {
        Console c;
    }
    if ((e.type == EVENT_KEY_UP && e.key.key == K_ESCAPE)
        || (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_RIGHT)
        || engine->gameControllerGetButton(GAMEPAD_BUTTON_START))
    {
        UI::getInstance()->run();
    }

    if (engine->checkKeyPress(K_LEFT))
    {
        camera_yaw_ += CameraRotateStep;
    }
    if (engine->checkKeyPress(K_RIGHT))
    {
        camera_yaw_ -= CameraRotateStep;
    }
    if (engine->checkKeyPress(K_UP))
    {
        camera_height_ = std::max(80.0f, camera_height_ - CameraHeightStep);
    }
    if (engine->checkKeyPress(K_DOWN))
    {
        camera_height_ = std::min(300.0f, camera_height_ + CameraHeightStep);
    }
    if (engine->checkKeyPress(K_Z))
    {
        camera_distance_ += CameraZoomStep;
    }
    if (engine->checkKeyPress(K_X))
    {
        camera_distance_ -= CameraZoomStep;
    }
    camera_distance_ /= engine->consumeTouchPinchScale();
    auto touch_pan = engine->consumeTouchCameraPan();
    camera_yaw_ += touch_pan.x / engine->getUIWidth() * Pi;
    camera_height_ = std::clamp(camera_height_ - touch_pan.y / engine->getUIHeight() * 220.0f, 80.0f, 300.0f);
    auto left_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_LEFT_TRIGGER);
    auto right_trigger = engine->gameControllerGetAxis(GAMEPAD_AXIS_RIGHT_TRIGGER);
    if (left_trigger > 6000)
    {
        camera_distance_ += CameraZoomStep * GameUtil::clamp(left_trigger, 0, 20000) / 20000.0f;
    }
    if (right_trigger > 6000)
    {
        camera_distance_ -= CameraZoomStep * GameUtil::clamp(right_trigger, 0, 20000) / 20000.0f;
    }
    camera_distance_ = std::clamp(camera_distance_, CameraMinDistance, CameraMaxDistance);
    if (camera_yaw_ > Pi)
    {
        camera_yaw_ -= Pi * 2.0f;
    }
    else if (camera_yaw_ < -Pi)
    {
        camera_yaw_ += Pi * 2.0f;
    }

    int x = man_x_, y = man_y_;
    if (engine->getTicks() - pre_pressed_ticks_ > key_walk_delay_)
    {
        int pressed = 0;
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

        if (pressed == 0 && (pre_pressed_ == K_A || pre_pressed_ == K_S || pre_pressed_ == K_D || pre_pressed_ == K_W) && engine->checkKeyPress(pre_pressed_))
        {
            pressed = pre_pressed_;
        }
        pre_pressed_ = pressed;

        if (pressed)
        {
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
            if (rest_time_ > 2) { total_step_ = 0; }
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
    }
    calCursorPosition(man_x_, man_y_);

    if (e.type == EVENT_MOUSE_BUTTON_UP && e.button.button == BUTTON_LEFT)
    {
        setMouseEventPoint(-1, -1);
        int mx, my;
        Engine::getInstance()->getMouseStateInStartWindow(mx, my);
        Point p = getMousePosition(mx, my, x, y);
        way_que_.clear();
        if (canWalk(p.x, p.y))
        {
            FindWay(x, y, p.x, p.y);
        }
        if (isBuilding(p.x, p.y))
        {
            int buiding_x = build_x_layer_.data(p.x, p.y);
            int buiding_y = build_y_layer_.data(p.x, p.y);
            bool found_entrance = false;
            for (int ix = buiding_x + 1; ix > buiding_x - 9; ix--)
            {
                for (int iy = buiding_y + 1; iy > buiding_y - 9; iy--)
                {
                    if (build_x_layer_.data(ix, iy) == buiding_x && build_y_layer_.data(ix, iy) == buiding_y && checkEntrance(ix, iy, true))
                    {
                        p.x = ix;
                        p.y = iy;
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
