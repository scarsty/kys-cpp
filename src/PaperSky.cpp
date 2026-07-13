#include "PaperSky.h"
#include "GameUtil.h"
#include "RunNode.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>

Color PaperSky::mixColor(const Color& from, const Color& to, float factor) const
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    auto mix_channel = [factor](uint8_t a, uint8_t b)
    {
        return uint8_t(a + (b - a) * factor);
    };
    return {
        mix_channel(from.r, to.r),
        mix_channel(from.g, to.g),
        mix_channel(from.b, to.b),
        mix_channel(from.a, to.a)
    };
}

void PaperSky::fillVerticalGradient(Engine* engine, int width, int y0, int y1,
    Color top_color, Color bottom_color, int band_height)
{
    if (!engine || width <= 0 || y1 <= y0)
    {
        return;
    }
    band_height = std::max(1, band_height);
    int height = y1 - y0;
    for (int y = y0; y < y1; y += band_height)
    {
        int current_height = std::min(band_height, y1 - y);
        float factor = height > 1 ? float(y - y0) / float(height - 1) : 0.0f;
        engine->fillColor(mixColor(top_color, bottom_color, factor),
            0, y, width, current_height, BLENDMODE_NONE);
    }
}

void PaperSky::fillStretchedVerticalGradient(Engine* engine, int width, int y0, int visible_y1, int gradient_y1,
    Color top_color, Color bottom_color, int band_height)
{
    if (!engine || width <= 0 || visible_y1 <= y0)
    {
        return;
    }
    band_height = std::max(1, band_height);
    int gradient_height = std::max(1, gradient_y1 - y0);
    for (int y = y0; y < visible_y1; y += band_height)
    {
        int current_height = std::min(band_height, visible_y1 - y);
        float factor = gradient_height > 1 ? float(y - y0) / float(gradient_height - 1) : 0.0f;
        engine->fillColor(mixColor(top_color, bottom_color, factor),
            0, y, width, current_height, BLENDMODE_NONE);
    }
}

void PaperSky::renderTextureQuad(Engine* engine, Texture* texture, float x, float y, float w, float h,
    float source_x, float source_y, float source_w, float source_h, Color color)
{
    if (!engine || !texture || w <= 0 || h <= 0 || source_w <= 0 || source_h <= 0)
    {
        return;
    }

    std::vector<FPoint> destination = {
        { x, y },
        { x + w, y },
        { x + w, y + h },
        { x, y + h },
    };
    std::vector<FPoint> source = {
        { source_x, source_y },
        { source_x + source_w, source_y },
        { source_x + source_w, source_y + source_h },
        { source_x, source_y + source_h },
    };
    std::vector<Color> colors(4, color);
    std::vector<int> indices = { 0, 1, 2, 2, 3, 0 };
    engine->renderTextureMesh(texture, destination, source, colors, indices);
}

float PaperSky::normalizeAngle(float angle) const
{
    angle = std::fmod(angle + Pi, 2.0f * Pi);
    if (angle < 0)
    {
        angle += 2.0f * Pi;
    }
    return angle - Pi;
}

float PaperSky::smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0)
    {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float PaperSky::getHorizontalFovRadians(int viewport_width, int viewport_height) const
{
    float aspect = float(std::max(1, viewport_width)) / float(std::max(1, viewport_height));
    float vertical_fov = CameraFov * Pi / 180.0f;
    return 2.0f * std::atan(std::tan(vertical_fov * 0.5f) * aspect);
}

int PaperSky::getDestinationHeight(int horizon_y, int viewport_height) const
{
    return std::clamp(int(horizon_y / HorizonRatio),
        int(viewport_height * 0.45f), int(viewport_height * 1.18f));
}

void PaperSky::fillDisk(Engine* engine, int center_x, int center_y, int radius, Color inner_color, Color outer_color)
{
    if (!engine || radius <= 0)
    {
        return;
    }
    constexpr int Segments = 96;
    int rings = std::clamp(radius / 3, 8, 32);
    std::vector<FPoint> vertices;
    std::vector<FPoint> source;
    std::vector<Color> colors;
    std::vector<int> indices;
    vertices.reserve(1 + rings * Segments);
    source.reserve(1 + rings * Segments);
    colors.reserve(1 + rings * Segments);
    indices.reserve(Segments * 3 + (rings - 1) * Segments * 6);

    vertices.push_back({ float(center_x), float(center_y) });
    source.push_back({ 0.0f, 0.0f });
    colors.push_back(inner_color);

    for (int ring = 1; ring <= rings; ring++)
    {
        float factor = float(ring) / rings;
        float r = radius * factor;
        Color color = mixColor(inner_color, outer_color, factor);
        for (int i = 0; i < Segments; i++)
        {
            float angle = 2.0f * Pi * i / Segments;
            vertices.push_back({ center_x + std::cos(angle) * r, center_y + std::sin(angle) * r });
            source.push_back({ 0.0f, 0.0f });
            colors.push_back(color);
        }
    }

    auto ring_index = [](int ring, int segment)
    {
        return 1 + (ring - 1) * Segments + (segment % Segments);
    };
    for (int i = 0; i < Segments; i++)
    {
        indices.insert(indices.end(), { 0, ring_index(1, i), ring_index(1, i + 1) });
    }
    for (int ring = 1; ring < rings; ring++)
    {
        for (int i = 0; i < Segments; i++)
        {
            int i0 = ring_index(ring, i);
            int i1 = ring_index(ring, i + 1);
            int i2 = ring_index(ring + 1, i + 1);
            int i3 = ring_index(ring + 1, i);
            indices.insert(indices.end(), { i0, i1, i2, i2, i3, i0 });
        }
    }
    engine->renderTextureMesh(nullptr, vertices, source, colors, indices);
}

void PaperSky::renderCloudLayer(Engine* engine, int viewport_width, int viewport_height, int horizon_y,
    float pitch, float yaw, float horizontal_fov)
{
    if (!engine || viewport_width <= 0 || viewport_height <= 0 || horizontal_fov <= 0 || clouds_.empty())
    {
        return;
    }

    float focal = viewport_height * 0.5f
        / std::tan(CameraFov * Pi / 180.0f * 0.5f);
    float margin = horizontal_fov * 0.55f;
    float time = float(RunNode::getShowTimes()) / 60.0f;

    auto texture_manager = TextureManager::getInstance();
    for (const auto& cloud : clouds_)
    {
        float delta = normalizeAngle(cloud.angle + time * cloud.drift_speed - yaw * cloud.parallax);
        if (delta < -horizontal_fov * 0.5f - margin || delta > horizontal_fov * 0.5f + margin)
        {
            continue;
        }

        auto tex = texture_manager->getTexture("cloud", cloud.texture_id);
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

        float draw_w = viewport_width * cloud.width_ratio;
        float draw_h = draw_w * float(tex->h) / std::max(1.0f, float(tex->w));
        float cloud_elevation = std::clamp(31.0f - cloud.y_ratio * 38.0f, 6.0f, 30.0f)
            * Pi / 180.0f;
        float vertical_angle = std::clamp(-pitch + cloud_elevation, -1.35f, 1.35f);
        float projected_y = viewport_height * 0.5f - focal * std::tan(vertical_angle);
        float draw_x = viewport_width * 0.5f + delta / horizontal_fov * viewport_width - draw_w * 0.5f;
        float draw_y = projected_y - draw_h * 0.48f
            + std::sin(time * 0.55f + cloud.phase) * draw_h * 0.025f;
        float clipped_top = std::max(0.0f, draw_y);
        float clipped_bottom = std::min(float(viewport_height), draw_y + draw_h);
        if (clipped_bottom <= clipped_top)
        {
            continue;
        }

        int alpha = std::clamp(int(cloud.alpha + std::sin(time * 0.35f + cloud.phase) * 5.0f), 0, 255);
        Engine::setColor(texture, { 255, 255, 255, 255 });
        float source_y = (clipped_top - draw_y) / std::max(1.0f, draw_h) * tex->h;
        float source_h = (clipped_bottom - clipped_top) / std::max(1.0f, draw_h) * tex->h;
        renderTextureQuad(engine, texture, draw_x, clipped_top, draw_w, clipped_bottom - clipped_top,
            0.0f, source_y, float(tex->w), source_h, { 255, 255, 255, uint8_t(alpha) });
    }
}

void PaperSky::renderStars(Engine* engine, int viewport_width, int viewport_height, int horizon_y,
    float yaw, float horizontal_fov)
{
    if (!engine || viewport_width <= 0 || viewport_height <= 0 || horizontal_fov <= 0)
    {
        return;
    }

    constexpr int StarCount = 96;
    int star_bottom = std::clamp(horizon_y, int(viewport_height * 0.12f), int(viewport_height * 0.74f));
    for (int i = 0; i < StarCount; i++)
    {
        float angle = -Pi + (float(i) + 0.37f * std::sin(i * 12.9898f)) * (2.0f * Pi / StarCount);
        float delta = normalizeAngle(angle - yaw);
        if (delta < -horizontal_fov * 0.55f || delta > horizontal_fov * 0.55f)
        {
            continue;
        }
        float height_factor = std::fmod(std::sin(i * 78.233f) * 43758.5453f, 1.0f);
        if (height_factor < 0)
        {
            height_factor += 1.0f;
        }
        int x = int(viewport_width * 0.5f + delta / horizontal_fov * viewport_width);
        int y = int(12 + height_factor * std::max(1, star_bottom - 24));
        int size = (i % 7 == 0) ? 2 : 1;
        uint8_t alpha = uint8_t(110 + (i * 37) % 110);
        engine->fillColor({ 235, 240, 255, alpha }, x, y, size, size, BLENDMODE_BLEND);
    }
}

void PaperSky::renderProgramSky(Engine* engine, int viewport_width, int viewport_height, int horizon_y,
    float pitch, float yaw, float horizontal_fov)
{
    struct Palette
    {
        Color zenith;
        Color mid;
        Color horizon;
        Color sun_inner;
        Color sun_outer;
        float sun_angle;
        float sun_elevation;
        bool stars = false;
    };

    Palette palette;
    switch (sky_mode_)
    {
    case SkyMode::Dusk:
        palette = { { 90, 54, 92, 255 }, { 148, 79, 91, 255 }, { 214, 132, 96, 255 },
            { 255, 204, 128, 210 }, { 220, 104, 82, 20 }, 0.75f, 0.10f, false };
        break;
    case SkyMode::Night:
        palette = { { 6, 13, 34, 255 }, { 16, 26, 52, 255 }, { 31, 41, 68, 255 },
            { 224, 232, 255, 205 }, { 105, 124, 180, 24 }, -0.90f, 0.35f, true };
        break;
    case SkyMode::Dawn:
        palette = { { 82, 100, 143, 255 }, { 151, 128, 152, 255 }, { 219, 166, 139, 255 },
            { 255, 226, 168, 215 }, { 235, 148, 116, 22 }, -0.55f, 0.12f, false };
        break;
    case SkyMode::Day:
    default:
        palette = { { 54, 107, 169, 255 }, { 104, 157, 204, 255 }, { 176, 208, 228, 255 },
            { 255, 246, 180, 205 }, { 255, 255, 220, 16 }, 0.25f, 0.42f, false };
        break;
    }

    int sky_destination_height = std::max(viewport_height, getDestinationHeight(horizon_y, viewport_height));
    int mid_y = std::clamp(int(sky_destination_height * 0.58f), 1, viewport_height);
    fillStretchedVerticalGradient(engine, viewport_width, 0, mid_y, mid_y,
        palette.zenith, palette.mid, 3);
    fillStretchedVerticalGradient(engine, viewport_width, mid_y, viewport_height, sky_destination_height,
        palette.mid, palette.horizon, 3);

    if (palette.stars)
    {
        renderStars(engine, viewport_width, viewport_height, horizon_y, yaw, horizontal_fov);
    }

    float sun_delta = normalizeAngle(palette.sun_angle - yaw);
    if (sun_delta >= -horizontal_fov * 0.65f && sun_delta <= horizontal_fov * 0.65f)
    {
        int sun_x = int(viewport_width * 0.5f + sun_delta / horizontal_fov * viewport_width);
        int sun_y = int(horizon_y - std::tan(palette.sun_elevation - pitch) * viewport_height * 0.42f);
        int sun_radius = std::max(18, viewport_height / (palette.stars ? 18 : 12));
        if (palette.stars)
        {
            fillDisk(engine, sun_x, sun_y, sun_radius, palette.sun_inner, palette.sun_inner);
        }
        else
        {
            fillDisk(engine, sun_x, sun_y, sun_radius * 3, palette.sun_outer, { palette.sun_outer.r, palette.sun_outer.g, palette.sun_outer.b, 0 });
            fillDisk(engine, sun_x, sun_y, sun_radius, palette.sun_inner, { palette.sun_inner.r, palette.sun_inner.g, palette.sun_inner.b, 0 });
        }
    }
}

void PaperSky::reset()
{
    yaw_ = 0;
    yaw_initialized_ = false;
    RandomDouble random;
    sky_mode_ = SkyMode(random.rand_int(4));
}

void PaperSky::generateClouds()
{
    clouds_.clear();

    RandomDouble random;

    auto texture_manager = TextureManager::getInstance();
    int cloud_texture_count = texture_manager->getTextureGroup("cloud")->getTextureCount();
    if (cloud_texture_count <= 0)
    {
        return;
    }

    auto random_float = [&](float min_value, float max_value)
    {
        return min_value + float(random.rand()) * (max_value - min_value);
    };
    auto lerp = [](float from, float to, float factor)
    {
        return from + (to - from) * factor;
    };

    int cloud_count = std::max(34 + random.rand_int(15), cloud_texture_count);
    int texture_offset = random.rand_int(cloud_texture_count);
    clouds_.reserve(cloud_count);

    for (int i = 0; i < cloud_count; i++)
    {
        float angle_step = 2.0f * Pi / float(cloud_count);
        float angle = -Pi + (float(i) + random_float(-0.35f, 0.35f)) * angle_step;
        float depth = std::clamp(float(random.rand()), 0.0f, 1.0f);
        float depth_curve = smoothStep(0.0f, 1.0f, depth);
        int wind_layer = std::min(int(depth_curve * CloudWindLayerCount), CloudWindLayerCount - 1);
        float speed = CloudWindDirections[wind_layer] * CloudWindSpeeds[wind_layer]
            * random_float(0.82f, 1.18f);

        Cloud cloud;
        cloud.texture_id = i < cloud_texture_count
            ? (i + texture_offset) % cloud_texture_count
            : random.rand_int(cloud_texture_count);
        cloud.angle = normalizeAngle(angle);
        cloud.y_ratio = std::clamp(lerp(0.04f, 0.72f, depth_curve) + random_float(-0.055f, 0.055f),
            0.02f, 0.78f);
        float size_depth_curve = depth_curve * depth_curve;
        cloud.width_ratio = std::clamp(lerp(0.0385f, 0.434f, size_depth_curve) * random_float(0.75f, 1.16f),
            0.028f, 0.504f);
        cloud.alpha = uint8_t(std::clamp(int(lerp(30.0f, 220.0f, depth_curve)
            * random_float(0.78f, 1.15f)), 24, 232));
        cloud.parallax = std::clamp(lerp(0.46f, 1.45f, depth_curve) + random_float(-0.08f, 0.08f),
            0.40f, 1.55f);
        cloud.drift_speed = speed;
        cloud.phase = random_float(0.0f, 2.0f * Pi);
        clouds_.push_back(cloud);
    }

    std::sort(clouds_.begin(), clouds_.end(),
        [](const Cloud& a, const Cloud& b)
        {
            return a.width_ratio < b.width_ratio;
        });
}

void PaperSky::render(Engine* engine, int viewport_width, int viewport_height,
    const Pointf& camera_pos, const Pointf& camera_center)
{
    if (!engine || viewport_width <= 0 || viewport_height <= 0)
    {
        return;
    }

    Pointf forward = camera_center - camera_pos;
    float horizontal_length = std::sqrt(forward.x * forward.x + forward.y * forward.y);
    if (horizontal_length < 0.001f)
    {
        horizontal_length = 0.001f;
    }
    float pitch = std::atan2(forward.z, horizontal_length);
    float focal = viewport_height * 0.5f
        / std::tan(CameraFov * Pi / 180.0f * 0.5f);
    int horizon_y = int(viewport_height * 0.5f + focal * std::tan(pitch));
    horizon_y = std::clamp(horizon_y, int(viewport_height * 0.08f), int(viewport_height * 0.74f));

    float sky_yaw = -std::atan2(forward.y, forward.x);
    if (!yaw_initialized_)
    {
        yaw_ = sky_yaw;
        yaw_initialized_ = true;
    }
    else
    {
        yaw_ += normalizeAngle(sky_yaw - yaw_);
    }
    sky_yaw = yaw_;

    float horizontal_fov = getHorizontalFovRadians(viewport_width, viewport_height);
    renderProgramSky(engine, viewport_width, viewport_height, horizon_y, pitch, sky_yaw, horizontal_fov);
    renderCloudLayer(engine, viewport_width, viewport_height, horizon_y, pitch, sky_yaw, horizontal_fov);
}
