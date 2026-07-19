#include "PaperPresentation.h"
#include "DayNightSystem.h"
#include "Engine.h"
#include "Font.h"
#include "GameUtil.h"
#include "TextureManager.h"
#include "filefunc.h"
#include <limits>

namespace
{
constexpr float GroundMeshCenterRatio = 0.64f;
constexpr int GroundMeshTargetDivisions = 160;
constexpr int GroundMeshExtensionDivisionsPerSide = 10;
constexpr int GroundMeshOriginalEdgeDivisionsPerSide = 18;
constexpr float GroundEdgeFadeTileCount = 16.0f;
constexpr float GroundHorizonFadeStartRatio = 0.72f;
constexpr float GroundHorizonFadeEndRatio = 1.35f;
constexpr float GroundRadialFadeStartRatio = 0.34f;
constexpr float GroundRadialFadeEndRatio = 0.48f;
}

void PaperPresentation::renderLockMarker(Engine* engine, Camera& camera, const Pointf& position,
    int current_frame, int tile_width, int texture_id) const
{
    Pointf world = position + Pointf{ 0, 0, tile_width * 1.5f };
    if (camera.getDepth(world) <= camera.getNearPlane()) { return; }
    auto projected = camera.getProj({ world });
    if (projected.empty()) { return; }
    auto texture = TextureManager::getInstance()->getTexture("title", texture_id);
    float zoom = 0.5f + 0.06f * std::sin(current_frame * 0.16f);
    int x = int(projected[0].x) - (texture ? int(texture->w * zoom * 0.5f) : 0);
    int y = int(projected[0].y) - (texture ? int(texture->h * zoom * 0.5f) : 0);
    TextureManager::getInstance()->renderTexture("title", texture_id, x, y,
        { { 255, 255, 255, 255 }, 224, zoom, zoom });
}

void PaperPresentation::renderRoleInfoAnchors(Camera& camera, const std::vector<PaperRoleInfoAnchor>& anchors,
    int tile_width, int tile_width_base, float top_gap,
    const std::function<void(Role*, int, int)>& render_info) const
{
    if (!render_info) { return; }
    int scale = (std::max)(1, tile_width / tile_width_base);
    for (const auto& anchor : anchors)
    {
        Pointf world = anchor.position + Pointf{ 0, 0, tile_width * 3.5f };
        if (camera.getDepth(world) <= camera.getNearPlane()) { continue; }
        auto projected = camera.getProj({ world });
        if (!projected.empty())
        {
            render_info(anchor.role, int(projected[0].x), int(projected[0].y) + 61 * scale - int(top_gap));
        }
    }
}

void PaperPresentation::renderTextEffects(Engine* engine, Camera& camera, const std::vector<PaperTextEffect>& effects,
    int tile_width, int scene_width, int scene_height) const
{
    int ui_width = 0;
    int ui_height = 0;
    engine->getUISize(ui_width, ui_height);
    float scene_to_ui_x = float(ui_width) / scene_width;
    float scene_to_ui_y = float(ui_height) / scene_height;
    for (const auto& effect : effects)
    {
        Pointf world = effect.position;
        world.z += tile_width * 1.5f;
        if (camera.getDepth(world) <= camera.getNearPlane()) { continue; }
        auto projected = camera.getProj({ world });
        if (!projected.empty())
        {
            float x = projected[0].x - 7.5f * Font::getTextDrawSize(effect.text);
            float y = projected[0].y - 4.0f - effect.rise;
            Font::getInstance()->draw(effect.text, effect.size, int(x * scene_to_ui_x),
                int(y * scene_to_ui_y), effect.color, 255);
        }
    }
}

void PaperPresentation::initializeScene(Engine* engine, const PaperScene& scene)
{
    scene_ = scene;
    int center = scene_.coordinate_count / 2;
    int focus_x = center;
    int focus_y = center;
    int nearest_distance = std::numeric_limits<int>::max();
    if (scene_.to_world)
    {
        for (int x = 0; x < scene_.coordinate_count; x++)
        {
            for (int y = 0; y < scene_.coordinate_count; y++)
            {
                bool has_earth = scene_.earth_layer && scene_.earth_layer->data(x, y) > 0;
                bool has_building = scene_.building_layer && scene_.building_layer->data(x, y) > 0;
                if (!has_earth && !has_building)
                {
                    continue;
                }
                int dx = x - center;
                int dy = y - center;
                int distance = dx * dx + dy * dy;
                if (distance < nearest_distance)
                {
                    nearest_distance = distance;
                    focus_x = x;
                    focus_y = y;
                }
            }
        }
        initial_focus_ = scene_.to_world(focus_x, focus_y);
    }
    else
    {
        initial_focus_ = {};
    }
    createGroundTargets(engine, scene_.coordinate_count, scene_.tile_width, scene_.tile_height, 0);
    bakeGround(engine);
}

void PaperPresentation::renderScene(Engine* engine, PaperFrame& frame)
{
    if (!frame.camera)
    {
        return;
    }
    Camera& camera = *frame.camera;
    engine->setRenderTarget("scene");
    engine->fillColor({ 0, 0, 0, 255 }, 0, 0, frame.scene_width, frame.scene_height);
    renderSky(engine, frame.scene_width, frame.scene_height, camera.pos, camera.center);
    renderGroundMesh(engine, ground_targets_, camera, scene_.coordinate_count, scene_.tile_width);
    renderGroundOverlays(engine, camera, frame.ground_overlays);
    std::vector<PaperRenderSprite> scene_sprites;
    if (scene_.building_layer && scene_.to_world)
    {
        appendMapSprites(engine, camera, *scene_.building_layer, scene_.coordinate_count,
            scene_.to_world, scene_sprites);
    }
    scene_sprites.insert(scene_sprites.end(), std::make_move_iterator(frame.sprites.begin()),
        std::make_move_iterator(frame.sprites.end()));
    for (int index = 0; index < int(scene_sprites.size()); index++)
    {
        scene_sprites[index].order = index;
    }
    renderSprites(engine, camera, scene_sprites);
    if (frame.locked_position)
    {
        renderLockMarker(engine, camera, *frame.locked_position, frame.current_frame,
            frame.tile_width, frame.lock_texture_id);
    }
    renderRoleInfoAnchors(camera, frame.role_info_anchors, frame.tile_width, frame.tile_width_base,
        frame.role_info_top_gap, frame.render_role_info);
    renderTextEffects(engine, camera, frame.text_effects, frame.tile_width, frame.scene_width, frame.scene_height);
}
void PaperPresentation::initialize()
{
    sky_.reset();
    sky_.clearLightingOverride();
    sky_.generateClouds();
}

PaperGroundTargets PaperPresentation::createGroundTargets(Engine* engine, int coordinate_count,
    int tile_width, int tile_height, int requested_extension)
{
    PaperGroundTargets targets;
    targets.base_content_width = coordinate_count * tile_width * 2;
    targets.base_content_height = coordinate_count * tile_height * 2;
    targets.texture_size = coordinate_count * tile_width * 2;

    int extension = requested_extension;
    for (;;)
    {
        targets.base_width = targets.base_content_width + extension * tile_width * 4;
        targets.base_height = targets.base_content_height + extension * tile_height * 4;
        targets.texture_width = targets.texture_size + extension * tile_width * 4;
        targets.texture_height = targets.texture_size + extension * tile_width * 4;
        engine->createRenderedTexture("earth_base", targets.base_width, targets.base_height);
        engine->createRenderedTexture("earth", targets.texture_width, targets.texture_height);
        targets.base_texture = engine->getTexture("earth_base");
        targets.texture = engine->getTexture("earth");
        if ((targets.base_texture && targets.texture) || extension == 0)
        {
            break;
        }
        extension /= 2;
    }

    if (targets.base_texture)
    {
        engine->getTextureSize(targets.base_texture, targets.base_width, targets.base_height);
    }
    if (targets.texture)
    {
        engine->getTextureSize(targets.texture, targets.texture_width, targets.texture_height);
    }

    engine->createRenderedTexture("earth2", targets.texture_size, targets.texture_size);
    ground_targets_ = targets;
    return ground_targets_;
}

void PaperPresentation::bakeGround(Engine* engine)
{
    if (!scene_.earth_layer || !scene_.to_world)
    {
        return;
    }

    constexpr int EdgeStretchSourceDivisor = 16;
    const int coordinate_count = scene_.coordinate_count;
    const int square_fill_extension = coordinate_count;
    auto earth_base_texture = ground_targets_.base_texture;
    auto earth_texture = ground_targets_.texture;
    const int base_earth_w = ground_targets_.base_content_width;
    const int base_earth_h = ground_targets_.base_content_height;
    const int paper_earth_size = ground_targets_.texture_size;
    const int actual_base_earth_w = ground_targets_.base_width;
    const int actual_base_earth_h = ground_targets_.base_height;
    const int actual_paper_earth_w = ground_targets_.texture_width;
    const int actual_paper_earth_h = ground_targets_.texture_height;
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
            int x = 0;
            int y = 0;
        };
        std::vector<GroundTile> ground_tiles;
        ground_tiles.reserve(coordinate_count * coordinate_count);
        auto tile_index = [coordinate_count](int x, int y) { return x + y * coordinate_count; };
        std::vector<int> ground_tile_indices(coordinate_count * coordinate_count, -1);
        std::vector<int> row_first_valid(coordinate_count, -1);
        std::vector<int> row_last_valid(coordinate_count, -1);
        std::vector<int> column_first_valid(coordinate_count, -1);
        std::vector<int> column_last_valid(coordinate_count, -1);
        for (int y = 0; y < coordinate_count; y++)
        {
            for (int x = 0; x < coordinate_count; x++)
            {
                int tile_number = scene_.earth_layer->data(x, y) / 2;
                auto texture = tile_number > 0 ? texture_manager->getTexture("smap", tile_number) : nullptr;
                if (!texture)
                {
                    continue;
                }
                int source_index = int(ground_tiles.size());
                ground_tiles.push_back({ texture, x, y });
                ground_tile_indices[tile_index(x, y)] = source_index;
                if (row_first_valid[y] < 0) { row_first_valid[y] = x; }
                row_last_valid[y] = x;
                if (column_first_valid[x] < 0) { column_first_valid[x] = y; }
                column_last_valid[x] = y;
            }
        }
        std::vector<int> edge_tile_indices;
        constexpr int NeighborOffsets[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
        for (int index = 0; index < int(ground_tiles.size()); index++)
        {
            const auto& tile = ground_tiles[index];
            for (const auto& offset : NeighborOffsets)
            {
                int x = tile.x + offset[0];
                int y = tile.y + offset[1];
                if (x < 0 || x >= coordinate_count || y < 0 || y >= coordinate_count
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
            if (x < 0 || x >= coordinate_count || y < 0 || y >= coordinate_count)
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
                int dx = tile.x - x;
                int dy = tile.y - y;
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
            int source_x = std::clamp(x, 0, coordinate_count - 1);
            int source_y = std::clamp(y, 0, coordinate_count - 1);
            if (y < 0 && column_first_valid[source_x] >= 0) { source_y = column_first_valid[source_x]; }
            else if (y >= coordinate_count && column_last_valid[source_x] >= 0) { source_y = column_last_valid[source_x]; }
            if (x < 0 && row_first_valid[source_y] >= 0) { source_x = row_first_valid[source_y]; }
            else if (x >= coordinate_count && row_last_valid[source_y] >= 0) { source_x = row_last_valid[source_y]; }
            source_index = get_original_tile_index(source_x, source_y);
            return source_index >= 0 ? source_index : find_nearest_edge_tile(x, y);
        };
        for (int y = -square_fill_extension; y < coordinate_count + square_fill_extension; y++)
        {
            for (int x = -square_fill_extension; x < coordinate_count + square_fill_extension; x++)
            {
                int source_index = get_extension_source_index(x, y);
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
                auto position = scene_.to_world(x, y);
                engine->renderTexture(source_texture, base_offset_x + int(position.x) - texture->dx,
                    base_offset_y + int(std::round(position.y * scene_.tile_height / float(scene_.tile_width))) - texture->dy,
                    texture->w, texture->h);
            }
        }

        auto battle_earth_group = texture_manager->getTextureGroup("battle-earth");
        std::string battle_earth_filename;
        if (battle_earth_group->getTextureCount() > 0)
        {
            battle_earth_filename = std::to_string(scene_.battle_field_id) + battle_earth_group->info_.ext_;
        }
        if (!battle_earth_filename.empty() && !battle_earth_group->getFileContent(battle_earth_filename).empty())
        {
            auto texture = texture_manager->getTexture("battle-earth", scene_.battle_field_id);
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
                    int strip_w = std::clamp(source_w / EdgeStretchSourceDivisor, 1, source_w);
                    int strip_h = std::clamp(source_h / EdgeStretchSourceDivisor, 1, source_h);
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

    if (auto earth_texture2 = engine->getTexture("earth2"))
    {
        engine->setRenderTarget(earth_texture2);
        engine->fillColor({ 0, 0, 0, 0 }, 0, 0, paper_earth_size, paper_earth_size, BLENDMODE_NONE);
    }
}

void PaperPresentation::renderGroundMesh(Engine* engine, const PaperGroundTargets& targets,
    Camera& camera, int coordinate_count, int tile_width) const
{
    if (!targets.texture)
    {
        return;
    }

    float original_size = float(coordinate_count * tile_width * 2);
    float texture_width = float(std::max(1, targets.texture_width));
    float texture_height = float(std::max(1, targets.texture_height));
    float offset_x = std::max(0.0f, (texture_width - original_size) * 0.5f);
    float offset_y = std::max(0.0f, (texture_height - original_size) * 0.5f);
    auto make_lines = [](float min_value, float max_value, float size)
    {
        std::vector<float> lines;
        auto add_segment = [&](float start, float end, int divisions)
        {
            start = std::clamp(start, min_value, max_value);
            end = std::clamp(end, min_value, max_value);
            if (end < start) { return; }
            for (int index = 0; index <= std::max(1, divisions); index++)
            {
                lines.push_back(start + (end - start) * index / std::max(1, divisions));
            }
        };
        float margin = size * (1.0f - GroundMeshCenterRatio) * 0.5f;
        int center_divisions = std::max(16, GroundMeshTargetDivisions
            - GroundMeshExtensionDivisionsPerSide * 2 - GroundMeshOriginalEdgeDivisionsPerSide * 2);
        add_segment(min_value, 0, GroundMeshExtensionDivisionsPerSide);
        add_segment(0, margin, GroundMeshOriginalEdgeDivisionsPerSide);
        add_segment(margin, size - margin, center_divisions);
        add_segment(size - margin, size, GroundMeshOriginalEdgeDivisionsPerSide);
        add_segment(size, max_value, GroundMeshExtensionDivisionsPerSide);
        std::sort(lines.begin(), lines.end());
        lines.erase(std::unique(lines.begin(), lines.end(), [](float left, float right) { return std::abs(left - right) <= 0.25f; }), lines.end());
        return lines.size() < 2 ? std::vector<float>{ min_value, max_value } : lines;
    };

    std::vector<float> x_lines = make_lines(-offset_x, texture_width - offset_x, original_size);
    std::vector<float> y_lines = make_lines(-offset_y, texture_height - offset_y, original_size);
    float edge_fade_distance = tile_width * GroundEdgeFadeTileCount;
    float horizon_start = std::max(edge_fade_distance, original_size * GroundHorizonFadeStartRatio);
    float horizon_end = std::max(horizon_start + edge_fade_distance, original_size * GroundHorizonFadeEndRatio);
    float radial_size = std::min(texture_width, texture_height);
    float radial_start = radial_size * GroundRadialFadeStartRatio;
    float radial_end = radial_size * GroundRadialFadeEndRatio;

    Color ambient = DayNightSystem::getInstance()->getLighting().ambient_color;
    std::vector<Pointf> world;
    std::vector<FPoint> source;
    std::vector<Color> colors;
    world.reserve(x_lines.size() * y_lines.size());
    source.reserve(x_lines.size() * y_lines.size());
    colors.reserve(x_lines.size() * y_lines.size());
    for (float y : y_lines)
    {
        for (float x : x_lines)
        {
            float source_x = std::clamp(x + offset_x, 0.0f, texture_width);
            float source_y = std::clamp(y + offset_y, 0.0f, texture_height);
            float edge_distance = std::min(std::min(source_x, texture_width - source_x), std::min(source_y, texture_height - source_y));
            float alpha = 255.0f * PaperSky::smoothStep(0.0f, edge_fade_distance, edge_distance);
            float center_distance = std::hypot(source_x - texture_width * 0.5f, source_y - texture_height * 0.5f);
            alpha = std::min(alpha, 255.0f * (1.0f - PaperSky::smoothStep(radial_start, radial_end, center_distance)));
            world.push_back({ x, y, 0 });
            source.push_back({ source_x, source_y });
            ambient.a = uint8_t(std::clamp(alpha, 0.0f, 255.0f));
            colors.push_back(ambient);
        }
    }

    auto projected = camera.getProj(world);
    std::vector<FPoint> destination;
    std::vector<float> depths;
    destination.reserve(projected.size());
    depths.reserve(world.size());
    for (size_t index = 0; index < world.size(); index++)
    {
        float depth = camera.getDepth(world[index]);
        float alpha = 255.0f * (1.0f - PaperSky::smoothStep(horizon_start, horizon_end, depth));
        colors[index].a = std::min(colors[index].a, uint8_t(std::clamp(alpha, 0.0f, 255.0f)));
        destination.push_back({ projected[index].x, projected[index].y });
        depths.push_back(depth);
    }

    int columns = int(x_lines.size()) - 1;
    int rows = int(y_lines.size()) - 1;
    std::vector<int> indices;
    indices.reserve(columns * rows * 6);
    for (int row = 0; row < rows; row++)
    {
        for (int column = 0; column < columns; column++)
        {
            int top_left = row * (columns + 1) + column;
            int top_right = top_left + 1;
            int bottom_left = top_left + columns + 1;
            int bottom_right = bottom_left + 1;
            if (depths[top_left] > camera.getNearPlane() && depths[top_right] > camera.getNearPlane()
                && depths[bottom_left] > camera.getNearPlane() && depths[bottom_right] > camera.getNearPlane())
            {
                indices.insert(indices.end(), { top_left, top_right, bottom_right, bottom_right, bottom_left, top_left });
            }
        }
    }
    engine->renderTextureMesh(targets.texture, destination, source, colors, indices);
}

void PaperPresentation::renderGroundOverlays(Engine* engine, Camera& camera,
    const std::vector<PaperGroundOverlay>& overlays) const
{
    for (const auto& overlay : overlays)
    {
        if (overlay.world.size() != 4)
        {
            continue;
        }
        bool in_front = true;
        for (const auto& point : overlay.world)
        {
            if (camera.getDepth(point) <= camera.getNearPlane())
            {
                in_front = false;
                break;
            }
        }
        if (!in_front)
        {
            continue;
        }
        auto projected = camera.getProj(overlay.world);
        std::vector<FPoint> destination;
        destination.reserve(projected.size());
        for (const auto& point : projected)
        {
            destination.push_back({ point.x, point.y });
        }
        engine->renderTextureMesh(nullptr, destination, std::vector<FPoint>(4),
            std::vector<Color>(4, overlay.color), { 0, 1, 2, 2, 3, 0 });
    }
}

bool PaperPresentation::isWallTile(int tile_number)
{
    return (tile_number >= 701 && tile_number <= 1139)
        || (tile_number >= 1410 && tile_number <= 1436)
        || (tile_number >= 1505 && tile_number <= 1621)
        || (tile_number >= 1816 && tile_number <= 1849)
        || (tile_number >= 2116 && tile_number <= 2144)
        || (tile_number >= 2184 && tile_number <= 2285);
}

void PaperPresentation::appendMapSprites(Engine* engine, Camera& camera, MapSquareInt& building_layer,
    int coordinate_count, const std::function<Pointf(int, int)>& to_world,
    std::vector<PaperRenderSprite>& sprites) const
{
    struct WallEdge
    {
        std::vector<Pointf> world;
        float depth = 0;
        int tile_number = 0;
    };
    Pointf view_direction = camera.center - camera.pos;
    view_direction.z = 0;
    if (view_direction.norm() == 0) { view_direction = { 0, 1, 0 }; }
    view_direction.normTo(1);
    auto depth_of = [&](const Pointf& point)
    {
        Pointf offset = point - camera.pos;
        return offset.x * view_direction.x + offset.y * view_direction.y;
    };
    auto is_wall_at = [&](int x, int y)
    {
        return x >= 0 && x < coordinate_count && y >= 0 && y < coordinate_count
            && isWallTile(building_layer.data(x, y) / 2);
    };
    std::vector<WallEdge> wall_edges;
    int sprite_order = int(sprites.size());
    auto append_sprite = [&](PaperRenderSprite sprite)
    {
        sprite.order = sprite_order++;
        sprites.push_back(std::move(sprite));
    };
    for (int x = 0; x < coordinate_count; x++)
    {
        for (int y = 0; y < coordinate_count; y++)
        {
            int tile_number = building_layer.data(x, y) / 2;
            if (isWallTile(tile_number))
            {
                Pointf p00 = to_world(x, y);
                Pointf p10 = to_world(x + 1, y);
                Pointf p01 = to_world(x, y + 1);
                Pointf p11 = to_world(x + 1, y + 1);
                auto append_edge = [&](Pointf start, Pointf end)
                {
                    constexpr float WallHeight = 80.0f;
                    WallEdge edge;
                    edge.world = { { start.x, start.y, WallHeight }, { end.x, end.y, WallHeight }, { end.x, end.y, 0 }, { start.x, start.y, 0 } };
                    edge.depth = depth_of({ (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f, WallHeight * 0.5f });
                    edge.tile_number = tile_number;
                    wall_edges.push_back(std::move(edge));
                };
                if (!is_wall_at(x, y - 1)) { append_edge(p00, p10); }
                if (!is_wall_at(x + 1, y)) { append_edge(p10, p11); }
                if (!is_wall_at(x, y + 1)) { append_edge(p11, p01); }
                if (!is_wall_at(x - 1, y)) { append_edge(p01, p00); }
                continue;
            }
            if (tile_number <= 0) { continue; }
            auto texture = TextureManager::getInstance()->getTexture("smap", tile_number);
            if (texture)
            {
                Pointf position = to_world(x, y);
                append_sprite({ .tex = texture, .anchor = { position.x, position.y, 0 }, .depth = depth_of({ position.x, position.y, 0 }) });
            }
        }
    }

    engine->createRenderedTexture("paper-wall-edge", 16, 16);
    Texture* fallback_texture = engine->getTexture("paper-wall-edge");
    auto previous_target = engine->getRenderTarget();
    engine->setRenderTarget(fallback_texture);
    engine->fillColor({ 70, 62, 50, 235 }, 0, 0, 16, 16, BLENDMODE_NONE);
    engine->fillColor({ 94, 84, 66, 235 }, 0, 1, 16, 1, BLENDMODE_NONE);
    engine->fillColor({ 47, 42, 35, 235 }, 0, 4, 16, 1, BLENDMODE_NONE);
    engine->fillColor({ 88, 78, 61, 235 }, 0, 8, 16, 1, BLENDMODE_NONE);
    engine->fillColor({ 42, 37, 31, 235 }, 0, 12, 16, 1, BLENDMODE_NONE);
    engine->fillColor({ 104, 92, 72, 210 }, 0, 15, 16, 1, BLENDMODE_NONE);
    engine->setRenderTarget(previous_target);
    for (const auto& edge : wall_edges)
    {
        PaperRenderSprite sprite;
        auto filename = GameUtil::PATH() + "resource/paper-wall-texture/" + std::to_string(edge.tile_number) + ".png";
        if (filefunc::fileExist(filename))
        {
            if (auto texture = TextureManager::getInstance()->getTexture("paper-wall-texture", edge.tile_number))
            {
                texture->load();
                if (texture->getTexture())
                {
                    sprite.texture = texture->getTexture();
                    sprite.source = { { 0, 0 }, { float(texture->w), 0 }, { float(texture->w), float(texture->h) }, { 0, float(texture->h) } };
                }
            }
        }
        if (!sprite.texture)
        {
            sprite.texture = fallback_texture;
            sprite.source = { { 0, 0 }, { 16, 0 }, { 16, 16 }, { 0, 16 } };
        }
        sprite.world = edge.world;
        sprite.anchor = { (edge.world[0].x + edge.world[2].x) * 0.5f, (edge.world[0].y + edge.world[2].y) * 0.5f, 0 };
        sprite.depth = edge.depth;
        sprite.use_perspective_mesh = true;
        append_sprite(std::move(sprite));
    }
}

void PaperPresentation::renderSprites(Engine* engine, Camera& camera, std::vector<PaperRenderSprite>& sprites) const
{
    Color ambient = DayNightSystem::getInstance()->getLighting().ambient_color;
    auto apply_ambient = [ambient](Color color)
    {
        color.r = uint8_t(int(color.r) * ambient.r / 255);
        color.g = uint8_t(int(color.g) * ambient.g / 255);
        color.b = uint8_t(int(color.b) * ambient.b / 255);
        return color;
    };
    Pointf view_direction = camera.center - camera.pos;
    view_direction.z = 0;
    if (view_direction.norm() == 0) { view_direction = { 0, 1, 0 }; }
    view_direction.normTo(1);
    Pointf paper_right = { view_direction.y, -view_direction.x, 0 };
    auto in_front = [&](const std::vector<Pointf>& points)
    {
        for (const auto& point : points)
        {
            if (camera.getDepth(point) <= camera.getNearPlane()) { return false; }
        }
        return true;
    };
    auto render_quad = [&](Texture* texture, const std::vector<Pointf>& world,
        const std::vector<FPoint>& source, Color color, int columns, int rows)
    {
        if (!texture || world.size() != 4 || source.size() != 4 || !in_front(world)) { return; }
        columns = std::clamp(columns, 1, 32);
        rows = std::clamp(rows, 1, 32);
        std::vector<FPoint> destination;
        std::vector<FPoint> mesh_source;
        std::vector<int> indices;
        destination.reserve((columns + 1) * (rows + 1));
        mesh_source.reserve((columns + 1) * (rows + 1));
        indices.reserve(columns * rows * 6);
        for (int row = 0; row <= rows; row++)
        {
            float v = float(row) / rows;
            for (int column = 0; column <= columns; column++)
            {
                float u = float(column) / columns;
                Pointf top = world[0] * (1.0f - u) + world[1] * u;
                Pointf bottom = world[3] * (1.0f - u) + world[2] * u;
                auto projected = camera.getProj({ top * (1.0f - v) + bottom * v }).front();
                destination.push_back({ projected.x, projected.y });
                FPoint source_top = { source[0].x + (source[1].x - source[0].x) * u, source[0].y + (source[1].y - source[0].y) * u };
                FPoint source_bottom = { source[3].x + (source[2].x - source[3].x) * u, source[3].y + (source[2].y - source[3].y) * u };
                mesh_source.push_back({ source_top.x + (source_bottom.x - source_top.x) * v, source_top.y + (source_bottom.y - source_top.y) * v });
            }
        }
        for (int row = 0; row < rows; row++)
        {
            for (int column = 0; column < columns; column++)
            {
                int top_left = row * (columns + 1) + column;
                int top_right = top_left + 1;
                int bottom_left = top_left + columns + 1;
                int bottom_right = bottom_left + 1;
                indices.insert(indices.end(), { top_left, top_right, bottom_right, bottom_right, bottom_left, top_left });
            }
        }
        engine->renderTextureMesh(texture, destination, mesh_source, std::vector<Color>(destination.size(), color), indices);
    };
    std::sort(sprites.begin(), sprites.end(), [](const PaperRenderSprite& left, const PaperRenderSprite& right)
        {
            if (std::abs(left.depth - right.depth) > 1.0f) { return left.depth > right.depth; }
            if (left.anchor.y != right.anchor.y) { return left.anchor.y < right.anchor.y; }
            if (left.anchor.x != right.anchor.x) { return left.anchor.x < right.anchor.x; }
            if (left.turn != right.turn) { return left.turn < right.turn; }
            return left.order < right.order;
        });
    for (const auto& sprite : sprites)
    {
        if (sprite.texture && sprite.world.size() == 4)
        {
            render_quad(sprite.texture, sprite.world, sprite.source, apply_ambient(sprite.color), sprite.use_perspective_mesh ? 4 : 1, sprite.use_perspective_mesh ? 4 : 1);
            continue;
        }
        if (!sprite.tex) { continue; }
        sprite.tex->load();
        Texture* texture = sprite.tex->getTexture();
        if (!texture) { continue; }
        float left = -float(sprite.tex->dx);
        float right = float(sprite.tex->w - sprite.tex->dx);
        float top = float(sprite.tex->dy);
        float bottom = float(sprite.tex->dy - sprite.tex->h);
        Pointf forward = camera.center - camera.pos;
        forward.normTo(1);
        Pointf camera_up = { paper_right.y * forward.z - paper_right.z * forward.y, paper_right.z * forward.x - paper_right.x * forward.z, paper_right.x * forward.y - paper_right.y * forward.x };
        camera_up.normTo(1);
        auto point = [&](float x, float z)
        {
            if (sprite.face_camera) { return sprite.anchor + paper_right * x + camera_up * z; }
            if (sprite.rotation == 90) { return sprite.anchor + paper_right * -z + Pointf{ 0, 0, x }; }
            if (sprite.rotation == 270) { return sprite.anchor + paper_right * z + Pointf{ 0, 0, -x }; }
            return sprite.anchor + paper_right * x + Pointf{ 0, 0, z };
        };
        std::vector<Pointf> world = { point(left, top), point(right, top), point(right, bottom), point(left, bottom) };
        std::vector<FPoint> source = { { 0, 0 }, { float(sprite.tex->w), 0 }, { float(sprite.tex->w), float(sprite.tex->h) }, { 0, float(sprite.tex->h) } };
        Color color = sprite.color;
        color = apply_ambient(color);
        color.a = sprite.alpha;
        int columns = sprite.face_camera ? 1 : std::clamp((sprite.tex->w + 31) / 32, 1, 32);
        int rows = sprite.face_camera ? 1 : std::clamp((sprite.tex->h + 31) / 32, 1, 32);
        render_quad(texture, world, source, color, columns, rows);
    }
}

void PaperPresentation::renderSky(Engine* engine, int viewport_width, int viewport_height,
    const Pointf& camera_pos, const Pointf& camera_center)
{
    sky_.render(engine, viewport_width, viewport_height, camera_pos, camera_center);
}