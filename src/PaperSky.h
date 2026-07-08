#pragma once
#include "Engine.h"
#include "Point.h"
#include "Types.h"

#include <vector>

class PaperSky
{
public:
    static constexpr float CameraFov = 60.0f;

    void reset();
    void generateClouds();
    void render(Engine* engine, int viewportWidth, int viewportHeight,
        const Pointf& cameraPos, const Pointf& cameraCenter);

private:
    struct Cloud
    {
        int texture_id = 0;
        float angle = 0.0f;
        float y_ratio = 0.0f;
        float width_ratio = 0.0f;
        uint8_t alpha = 255;
        float parallax = 1.0f;
        float drift_speed = 0.0f;
        float phase = 0.0f;
    };

    static constexpr float Pi = 3.14159265358979323846f;
    static constexpr const char* TexturePath = "resource/sky/paper-sky.png";
    static constexpr float HorizonRatio = 0.74f;

    float yaw_ = 0.0f;
    bool yaw_initialized_ = false;
    Texture* texture_ = nullptr;
    bool texture_tried_ = false;
    std::vector<Cloud> clouds_;

    static float smoothStep(float edge0, float edge1, float value);
    static Color mixColor(Color from, Color to, float factor);
    float normalizeAngle(float angle) const;
    float horizontalFovRadians(int viewportWidth, int viewportHeight) const;
    int destinationHeight(int horizonY, int viewportHeight) const;
    Texture* texture();
    void fillVerticalGradient(Engine* engine, int width, int y0, int y1,
        Color topColor, Color bottomColor, int bandHeight) const;
    void renderFallbackGradient(Engine* engine, int viewportWidth, int viewportHeight, int horizonY) const;
    void renderTextureBackdrop(Engine* engine, int viewportWidth, int viewportHeight) const;
    void renderTextureQuad(Engine* engine, Texture* texture, float x, float y, float w, float h,
        float sourceX, float sourceY, float sourceW, float sourceH, Color color) const;
    void renderWrappedTexture(Engine* engine, Texture* skyTexture, int viewportWidth, int viewportHeight,
        int horizonY, float yaw, float horizontalFov) const;
    void renderCloudLayer(Engine* engine, int viewportWidth, int viewportHeight, int horizonY,
        float pitch, float yaw, float horizontalFov) const;
};
