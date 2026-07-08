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

void PaperSky::renderFallbackGradient(Engine* engine, int viewportWidth, int viewportHeight, int horizonY) const
{
    const int skyDestinationHeight = std::max(viewportHeight, destinationHeight(horizonY, viewportHeight));
    fillVerticalGradient(engine, viewportWidth, 0, skyDestinationHeight,
        { 38, 82, 142, 255 }, { 172, 202, 224, 255 }, 4);
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
        renderFallbackGradient(engine, viewportWidth, viewportHeight, horizonY);
    }
    renderCloudLayer(engine, viewportWidth, viewportHeight, horizonY, pitch, yaw_, horizontalFovRadians(viewportWidth, viewportHeight));
}
