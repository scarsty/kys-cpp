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

Texture* PaperSky::getTexture()
{
    if (!texture_tried_)
    {
        texture_tried_ = true;
        texture_ = Engine::getInstance()->loadImage(GameUtil::PATH() + TexturePath);
    }
    return texture_;
}

void PaperSky::renderWrappedTexture(Engine* engine, Texture* texture, int viewport_width, int viewport_height,
    int horizon_y, float yaw, float horizontal_fov)
{
    if (!engine || !texture || viewport_width <= 0 || viewport_height <= 0)
    {
        return;
    }

    int texture_width = 0;
    int texture_height = 0;
    engine->getTextureSize(texture, texture_width, texture_height);
    if (texture_width <= 0 || texture_height <= 0)
    {
        return;
    }

    int destination_height = getDestinationHeight(horizon_y, viewport_height);
    int source_width = std::clamp(int(float(texture_width) * horizontal_fov / (2.0f * Pi)),
        1, texture_width);

    float normalized_yaw = yaw / (2.0f * Pi);
    float source_x_float = std::fmod(normalized_yaw * texture_width - source_width * 0.5f, float(texture_width));
    if (source_x_float < 0)
    {
        source_x_float += texture_width;
    }
    int source_x = int(source_x_float);

    auto render_slice = [&](int sx, int sw, int dx, int dw)
    {
        if (sw <= 0 || dw <= 0)
        {
            return;
        }
        Rect source{ sx, 0, sw, texture_height };
        Rect destination{ dx, 0, dw, destination_height };
        engine->renderTexture(texture, &source, &destination);
    };

    int remaining_source_width = source_width;
    int destination_x = 0;
    while (remaining_source_width > 0)
    {
        int slice_width = std::min(remaining_source_width, texture_width - source_x);
        int destination_width = remaining_source_width == slice_width
            ? viewport_width - destination_x
            : int(float(slice_width) / source_width * viewport_width);
        render_slice(source_x, slice_width, destination_x, destination_width);
        remaining_source_width -= slice_width;
        destination_x += destination_width;
        source_x = 0;
    }
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

void PaperSky::renderFallbackGradient(Engine* engine, int viewport_width, int viewport_height, int horizon_y)
{
    int sky_destination_height = std::max(viewport_height, getDestinationHeight(horizon_y, viewport_height));
    fillStretchedVerticalGradient(engine, viewport_width, 0, viewport_height, sky_destination_height,
        { 38, 82, 142, 255 }, { 172, 202, 224, 255 }, 4);
}

void PaperSky::renderTextureGradientBackdrop(Engine* engine, int viewport_width, int viewport_height)
{
    fillVerticalGradient(engine, viewport_width, 0, viewport_height,
        { 73, 133, 188, 255 }, { 91, 110, 83, 255 }, 1);
}

void PaperSky::reset()
{
    yaw_ = 0;
    yaw_initialized_ = false;
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
    Texture* sky_texture = getTexture();
    if (sky_texture)
    {
        renderTextureGradientBackdrop(engine, viewport_width, viewport_height);
        renderWrappedTexture(engine, sky_texture, viewport_width, viewport_height, horizon_y, sky_yaw, horizontal_fov);
    }
    else
    {
        renderFallbackGradient(engine, viewport_width, viewport_height, horizon_y);
    }
    renderCloudLayer(engine, viewport_width, viewport_height, horizon_y, pitch, sky_yaw, horizontal_fov);
}
