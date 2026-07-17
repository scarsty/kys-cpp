#include "PaperSky.h"

#include "GameUtil.h"
#include "Random.h"
#include "RunNode.h"
#include "TextureManager.h"

#include <algorithm>
#include <cmath>

Color PaperSky::mixColor(Color from, Color to, float factor)
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    auto mixChannel = [factor](uint8_t a, uint8_t b)
    {
        return static_cast<uint8_t>(a + (b - a) * factor);
    };
    return {
        mixChannel(from.r, to.r),
        mixChannel(from.g, to.g),
        mixChannel(from.b, to.b),
        mixChannel(from.a, to.a)
    };
}

float PaperSky::smoothStep(float edge0, float edge1, float value)
{
    if (edge1 <= edge0)
    {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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

float PaperSky::horizontalFovRadians(int viewportWidth, int viewportHeight) const
{
    const float aspect = static_cast<float>(std::max(1, viewportWidth)) / std::max(1, viewportHeight);
    const float verticalFov = CameraFov * Pi / 180.0f;
    return 2.0f * std::atan(std::tan(verticalFov * 0.5f) * aspect);
}

int PaperSky::destinationHeight(int horizonY, int viewportHeight) const
{
    return std::clamp(static_cast<int>(horizonY / HorizonRatio),
        static_cast<int>(viewportHeight * 0.45f),
        static_cast<int>(viewportHeight * 1.18f));
}

void PaperSky::fillDisk(Engine* engine, int centerX, int centerY, int radius,
    Color innerColor, Color outerColor) const
{
    if (!engine || radius <= 0)
    {
        return;
    }

    constexpr int Segments = 64;
    const int rings = std::clamp(radius / 3, 8, 24);
    std::vector<FPoint> vertices;
    std::vector<FPoint> source;
    std::vector<Color> colors;
    std::vector<int> indices;
    vertices.reserve(1 + rings * Segments);
    source.reserve(1 + rings * Segments);
    colors.reserve(1 + rings * Segments);

    vertices.push_back({ static_cast<float>(centerX), static_cast<float>(centerY) });
    source.push_back({ 0.0f, 0.0f });
    colors.push_back(innerColor);
    for (int ring = 1; ring <= rings; ++ring)
    {
        const float factor = static_cast<float>(ring) / rings;
        const float ringRadius = radius * factor;
        const Color color = mixColor(innerColor, outerColor, factor);
        for (int segment = 0; segment < Segments; ++segment)
        {
            const float angle = 2.0f * Pi * segment / Segments;
            vertices.push_back({ centerX + std::cos(angle) * ringRadius, centerY + std::sin(angle) * ringRadius });
            source.push_back({ 0.0f, 0.0f });
            colors.push_back(color);
        }
    }

    const auto ringIndex = [](int ring, int segment)
    {
        return 1 + (ring - 1) * Segments + (segment % Segments);
    };
    for (int segment = 0; segment < Segments; ++segment)
    {
        indices.insert(indices.end(), { 0, ringIndex(1, segment), ringIndex(1, segment + 1) });
    }
    for (int ring = 1; ring < rings; ++ring)
    {
        for (int segment = 0; segment < Segments; ++segment)
        {
            indices.insert(indices.end(), {
                ringIndex(ring, segment), ringIndex(ring, segment + 1), ringIndex(ring + 1, segment + 1),
                ringIndex(ring + 1, segment + 1), ringIndex(ring + 1, segment), ringIndex(ring, segment)
            });
        }
    }
    engine->renderTextureMesh(nullptr, vertices, source, colors, indices);
}

Texture* PaperSky::texture()
{
    if (!texture_tried_)
    {
        texture_tried_ = true;
        texture_ = Engine::getInstance()->loadImage(GameUtil::PATH() + TexturePath);
    }
    return texture_;
}

void PaperSky::fillVerticalGradient(Engine* engine, int width, int y0, int y1,
    Color topColor, Color bottomColor, int bandHeight) const
{
    if (!engine || width <= 0 || y1 <= y0)
    {
        return;
    }
    bandHeight = std::max(1, bandHeight);
    const int height = y1 - y0;
    for (int y = y0; y < y1; y += bandHeight)
    {
        const int currentHeight = std::min(bandHeight, y1 - y);
        const float factor = height > 1 ? static_cast<float>(y - y0) / (height - 1) : 0.0f;
        engine->fillColor(mixColor(topColor, bottomColor, factor),
            0, y, width, currentHeight, BLENDMODE_NONE);
    }
}

void PaperSky::fillStretchedVerticalGradient(Engine* engine, int width, int y0, int visibleY1, int gradientY1,
    Color topColor, Color bottomColor, int bandHeight) const
{
    if (!engine || width <= 0 || visibleY1 <= y0)
    {
        return;
    }
    bandHeight = std::max(1, bandHeight);
    const int gradientHeight = std::max(1, gradientY1 - y0);
    for (int y = y0; y < visibleY1; y += bandHeight)
    {
        const int currentHeight = std::min(bandHeight, visibleY1 - y);
        const float factor = gradientHeight > 1 ? static_cast<float>(y - y0) / (gradientHeight - 1) : 0.0f;
        engine->fillColor(mixColor(topColor, bottomColor, factor),
            0, y, width, currentHeight, BLENDMODE_NONE);
    }
}

void PaperSky::renderFallbackGradient(Engine* engine, int viewportWidth, int viewportHeight, int horizonY) const
{
    const float horizontalFov = horizontalFovRadians(viewportWidth, viewportHeight);
    renderProgramSky(engine, viewportWidth, viewportHeight, horizonY, 0.0f, yaw_, horizontalFov);
}

void PaperSky::renderTextureBackdrop(Engine* engine, int viewportWidth, int viewportHeight) const
{
    fillVerticalGradient(engine, viewportWidth, 0, viewportHeight,
        { 73, 133, 188, 255 }, { 91, 110, 83, 255 }, 2);
}

void PaperSky::renderTextureQuad(Engine* engine, Texture* texture, float x, float y, float w, float h,
    float sourceX, float sourceY, float sourceW, float sourceH, Color color) const
{
    if (!engine || !texture || w <= 0 || h <= 0 || sourceW <= 0 || sourceH <= 0)
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
        { sourceX, sourceY },
        { sourceX + sourceW, sourceY },
        { sourceX + sourceW, sourceY + sourceH },
        { sourceX, sourceY + sourceH },
    };
    std::vector<Color> colors(4, color);
    std::vector<int> indices = { 0, 1, 2, 2, 3, 0 };
    engine->renderTextureMesh(texture, destination, source, colors, indices);
}

void PaperSky::renderWrappedTexture(Engine* engine, Texture* skyTexture, int viewportWidth, int viewportHeight,
    int horizonY, float yaw, float horizontalFov) const
{
    if (!engine || !skyTexture || viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }

    int textureWidth = 0;
    int textureHeight = 0;
    Engine::getTextureSize(skyTexture, textureWidth, textureHeight);
    if (textureWidth <= 0 || textureHeight <= 0)
    {
        return;
    }

    const int destHeight = destinationHeight(horizonY, viewportHeight);
    const int sourceWidth = std::clamp(static_cast<int>(textureWidth * horizontalFov / (2.0f * Pi)), 1, textureWidth);
    float sourceXFloat = std::fmod(yaw / (2.0f * Pi) * textureWidth - sourceWidth * 0.5f, static_cast<float>(textureWidth));
    if (sourceXFloat < 0)
    {
        sourceXFloat += textureWidth;
    }

    int sourceX = static_cast<int>(sourceXFloat);
    int remainingSourceWidth = sourceWidth;
    int destinationX = 0;
    while (remainingSourceWidth > 0)
    {
        const int sliceWidth = std::min(remainingSourceWidth, textureWidth - sourceX);
        const int destinationWidth = remainingSourceWidth == sliceWidth
            ? viewportWidth - destinationX
            : static_cast<int>(static_cast<float>(sliceWidth) / sourceWidth * viewportWidth);
        if (sliceWidth > 0 && destinationWidth > 0)
        {
            Rect source{ sourceX, 0, sliceWidth, textureHeight };
            Rect destination{ destinationX, 0, destinationWidth, destHeight };
            engine->renderTexture(skyTexture, &source, &destination);
        }
        remainingSourceWidth -= sliceWidth;
        destinationX += destinationWidth;
        sourceX = 0;
    }
}

void PaperSky::reset()
{
    yaw_ = 0.0f;
    yaw_initialized_ = false;
    RandomDouble random;
    sky_mode_ = static_cast<SkyMode>(random.rand_int(4));
}

void PaperSky::generateClouds()
{
    clouds_.clear();

    auto textureManager = TextureManager::getInstance();
    auto cloudGroup = textureManager->getTextureGroup("cloud");
    const int cloudTextureCount = cloudGroup ? cloudGroup->getTextureCount() : 0;
    if (cloudTextureCount <= 0)
    {
        return;
    }

    RandomDouble random;
    auto randomFloat = [&](float minValue, float maxValue)
    {
        return minValue + static_cast<float>(random.rand()) * (maxValue - minValue);
    };
    auto lerp = [](float from, float to, float factor)
    {
        return from + (to - from) * factor;
    };

    const int cloudCount = std::max(34 + random.rand_int(15), cloudTextureCount);
    const int textureOffset = random.rand_int(cloudTextureCount);
    clouds_.reserve(cloudCount);

    for (int i = 0; i < cloudCount; ++i)
    {
        const float angleStep = 2.0f * Pi / cloudCount;
        const float angle = -Pi + (i + randomFloat(-0.35f, 0.35f)) * angleStep;
        const float depth = std::clamp(static_cast<float>(random.rand()), 0.0f, 1.0f);
        const float depthCurve = smoothStep(0.0f, 1.0f, depth);
        float speed = lerp(0.00015f, 0.018f, depthCurve * depthCurve) * randomFloat(0.45f, 1.85f);
        if (random.rand() < 0.5)
        {
            speed = -speed;
        }

        Cloud cloud;
        cloud.texture_id = i < cloudTextureCount
            ? (i + textureOffset) % cloudTextureCount
            : random.rand_int(cloudTextureCount);
        cloud.angle = normalizeAngle(angle);
        cloud.y_ratio = std::clamp(lerp(0.04f, 0.72f, depthCurve) + randomFloat(-0.055f, 0.055f), 0.02f, 0.78f);
        cloud.width_ratio = std::clamp(lerp(0.0385f, 0.434f, depthCurve * depthCurve) * randomFloat(0.75f, 1.16f), 0.028f, 0.504f);
        cloud.alpha = static_cast<uint8_t>(std::clamp(static_cast<int>(lerp(30.0f, 220.0f, depthCurve) * randomFloat(0.78f, 1.15f)), 24, 232));
        cloud.parallax = std::clamp(lerp(0.46f, 1.45f, depthCurve) + randomFloat(-0.08f, 0.08f), 0.40f, 1.55f);
        cloud.drift_speed = speed;
        cloud.phase = randomFloat(0.0f, 2.0f * Pi);
        clouds_.push_back(cloud);
    }

    std::sort(clouds_.begin(), clouds_.end(),
        [](const Cloud& a, const Cloud& b)
        {
            return a.width_ratio < b.width_ratio;
        });
}

void PaperSky::renderCloudLayer(Engine* engine, int viewportWidth, int viewportHeight, int horizonY,
    float pitch, float yaw, float horizontalFov) const
{
    if (!engine || viewportWidth <= 0 || viewportHeight <= 0 || horizontalFov <= 0 || clouds_.empty())
    {
        return;
    }

    const float focal = viewportHeight * 0.5f / std::tan(CameraFov * Pi / 180.0f * 0.5f);
    const float margin = horizontalFov * 0.55f;
    const float time = static_cast<float>(RunNode::getShowTimes()) / 60.0f;
    auto textureManager = TextureManager::getInstance();

    for (const auto& cloud : clouds_)
    {
        const float delta = normalizeAngle(cloud.angle + time * cloud.drift_speed - yaw * cloud.parallax);
        if (delta < -horizontalFov * 0.5f - margin || delta > horizontalFov * 0.5f + margin)
        {
            continue;
        }

        auto tex = textureManager->getTexture("cloud", cloud.texture_id);
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

        const float drawW = viewportWidth * cloud.width_ratio;
        const float drawH = drawW * static_cast<float>(tex->h) / std::max(1.0f, static_cast<float>(tex->w));
        const float cloudElevation = std::clamp(31.0f - cloud.y_ratio * 38.0f, 6.0f, 30.0f) * Pi / 180.0f;
        const float verticalAngle = std::clamp(-pitch + cloudElevation, -1.35f, 1.35f);
        const float projectedY = viewportHeight * 0.5f - focal * std::tan(verticalAngle);
        const float drawX = viewportWidth * 0.5f + delta / horizontalFov * viewportWidth - drawW * 0.5f;
        const float drawY = projectedY - drawH * 0.48f + std::sin(time * 0.55f + cloud.phase) * drawH * 0.025f;
        const float clippedTop = std::max(0.0f, drawY);
        const float clippedBottom = std::min(static_cast<float>(viewportHeight), drawY + drawH);
        if (clippedBottom <= clippedTop)
        {
            continue;
        }

        const int alpha = std::clamp(static_cast<int>(cloud.alpha + std::sin(time * 0.35f + cloud.phase) * 5.0f), 0, 255);
        const float sourceY = (clippedTop - drawY) / std::max(1.0f, drawH) * tex->h;
        const float sourceH = (clippedBottom - clippedTop) / std::max(1.0f, drawH) * tex->h;
        renderTextureQuad(engine, texture, drawX, clippedTop, drawW, clippedBottom - clippedTop,
            0.0f, sourceY, static_cast<float>(tex->w), sourceH, { 255, 255, 255, static_cast<uint8_t>(alpha) });
    }
}

void PaperSky::renderStars(Engine* engine, int viewportWidth, int viewportHeight, int horizonY,
    float yaw, float horizontalFov) const
{
    if (!engine || viewportWidth <= 0 || viewportHeight <= 0 || horizontalFov <= 0.0f)
    {
        return;
    }

    constexpr int StarCount = 96;
    const int starBottom = std::clamp(horizonY, static_cast<int>(viewportHeight * 0.12f), static_cast<int>(viewportHeight * 0.74f));
    for (int index = 0; index < StarCount; ++index)
    {
        const float angle = -Pi + (index + 0.37f * std::sin(index * 12.9898f)) * (2.0f * Pi / StarCount);
        const float delta = normalizeAngle(angle - yaw);
        if (delta < -horizontalFov * 0.55f || delta > horizontalFov * 0.55f)
        {
            continue;
        }
        float heightFactor = std::fmod(std::sin(index * 78.233f) * 43758.5453f, 1.0f);
        if (heightFactor < 0.0f)
        {
            heightFactor += 1.0f;
        }
        const int x = static_cast<int>(viewportWidth * 0.5f + delta / horizontalFov * viewportWidth);
        const int y = static_cast<int>(12 + heightFactor * std::max(1, starBottom - 24));
        const int size = index % 7 == 0 ? 2 : 1;
        const uint8_t alpha = static_cast<uint8_t>(110 + (index * 37) % 110);
        engine->fillColor({ 235, 240, 255, alpha }, x, y, size, size, BLENDMODE_BLEND);
    }
}

void PaperSky::renderProgramSky(Engine* engine, int viewportWidth, int viewportHeight, int horizonY,
    float pitch, float yaw, float horizontalFov) const
{
    struct Palette
    {
        Color zenith;
        Color mid;
        Color horizon;
        Color sunInner;
        Color sunOuter;
        float sunAngle;
        float sunElevation;
        bool stars;
    };

    Palette palette{};
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

    const int skyDestinationHeight = std::max(viewportHeight, destinationHeight(horizonY, viewportHeight));
    const int midY = std::clamp(static_cast<int>(skyDestinationHeight * 0.58f), 1, viewportHeight);
    fillStretchedVerticalGradient(engine, viewportWidth, 0, midY, midY, palette.zenith, palette.mid, 3);
    fillStretchedVerticalGradient(engine, viewportWidth, midY, viewportHeight, skyDestinationHeight,
        palette.mid, palette.horizon, 3);

    if (palette.stars)
    {
        renderStars(engine, viewportWidth, viewportHeight, horizonY, yaw, horizontalFov);
    }

    const float sunDelta = normalizeAngle(palette.sunAngle - yaw);
    if (sunDelta < -horizontalFov * 0.65f || sunDelta > horizontalFov * 0.65f)
    {
        return;
    }
    const int sunX = static_cast<int>(viewportWidth * 0.5f + sunDelta / horizontalFov * viewportWidth);
    const int sunY = static_cast<int>(horizonY - std::tan(palette.sunElevation - pitch) * viewportHeight * 0.42f);
    const int sunRadius = std::max(18, viewportHeight / (palette.stars ? 18 : 12));
    if (palette.stars)
    {
        fillDisk(engine, sunX, sunY, sunRadius, palette.sunInner, palette.sunInner);
        return;
    }
    fillDisk(engine, sunX, sunY, sunRadius * 3, palette.sunOuter,
        { palette.sunOuter.r, palette.sunOuter.g, palette.sunOuter.b, 0 });
    fillDisk(engine, sunX, sunY, sunRadius, palette.sunInner,
        { palette.sunInner.r, palette.sunInner.g, palette.sunInner.b, 0 });
}

void PaperSky::render(Engine* engine, int viewportWidth, int viewportHeight,
    const Pointf& cameraPos, const Pointf& cameraCenter)
{
    if (!engine || viewportWidth <= 0 || viewportHeight <= 0)
    {
        return;
    }

    Pointf forward = cameraCenter - cameraPos;
    float horizontalLength = std::sqrt(forward.x * forward.x + forward.y * forward.y);
    if (horizontalLength < 0.001f)
    {
        horizontalLength = 0.001f;
    }
    const float pitch = std::atan2(forward.z, horizontalLength);
    const float focal = viewportHeight * 0.5f / std::tan(CameraFov * Pi / 180.0f * 0.5f);
    int horizonY = static_cast<int>(viewportHeight * 0.5f + focal * std::tan(pitch));
    horizonY = std::clamp(horizonY, static_cast<int>(viewportHeight * 0.08f), static_cast<int>(viewportHeight * 0.74f));

    const float skyYaw = -std::atan2(forward.y, forward.x);
    if (!yaw_initialized_)
    {
        yaw_ = skyYaw;
        yaw_initialized_ = true;
    }
    else
    {
        yaw_ += normalizeAngle(skyYaw - yaw_);
    }

    if (auto* skyTexture = texture())
    {
        renderTextureBackdrop(engine, viewportWidth, viewportHeight);
        renderWrappedTexture(engine, skyTexture, viewportWidth, viewportHeight, horizonY, yaw_, horizontalFovRadians(viewportWidth, viewportHeight));
    }
    else
    {
        renderProgramSky(engine, viewportWidth, viewportHeight, horizonY, pitch, yaw_, horizontalFovRadians(viewportWidth, viewportHeight));
    }
    renderCloudLayer(engine, viewportWidth, viewportHeight, horizonY, pitch, yaw_, horizontalFovRadians(viewportWidth, viewportHeight));
}
