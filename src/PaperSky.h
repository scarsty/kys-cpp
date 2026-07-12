#pragma once
#include "Engine.h"
#include "Random.h"
#include "Types.h"

#include <cstdint>
#include <vector>

class PaperSky
{
public:
    static constexpr float CameraFov = 60.0f;

    static float smoothStep(float edge0, float edge1, float value);
    void reset();
    void generateClouds();
    void render(Engine* engine, int viewport_width, int viewport_height,
        const Pointf& camera_pos, const Pointf& camera_center);

private:
    struct Cloud
    {
        int texture_id = 0;
        float angle = 0;
        float y_ratio = 0;
        float width_ratio = 1;
        uint8_t alpha = 255;
        float parallax = 1;
        float drift_speed = 0;
        float phase = 0;
    };

    static constexpr float Pi = 3.14159265358979323846f;
    static constexpr const char* TexturePath = "resource/sky/paper-sky.png";
    static constexpr float HorizonRatio = 0.74f;
    static constexpr int CloudWindLayerCount = 3;
    static constexpr float CloudWindDirections[CloudWindLayerCount] = { -1.0f, 1.0f, -1.0f };
    static constexpr float CloudWindSpeeds[CloudWindLayerCount] = { 0.0012f, 0.0055f, 0.0160f };

    float yaw_ = 0;
    bool yaw_initialized_ = false;
    Texture* texture_ = nullptr;
    bool texture_tried_ = false;
    std::vector<Cloud> clouds_;

    float normalizeAngle(float angle) const;
    float getHorizontalFovRadians(int viewport_width, int viewport_height) const;
    int getDestinationHeight(int horizon_y, int viewport_height) const;
    Texture* getTexture();
    Color mixColor(const Color& from, const Color& to, float factor) const;
    void fillVerticalGradient(Engine* engine, int width, int y0, int y1,
        Color top_color, Color bottom_color, int band_height);
    void fillStretchedVerticalGradient(Engine* engine, int width, int y0, int visible_y1, int gradient_y1,
        Color top_color, Color bottom_color, int band_height);
    void renderTextureQuad(Engine* engine, Texture* texture, float x, float y, float w, float h,
        float source_x, float source_y, float source_w, float source_h, Color color);
    void renderWrappedTexture(Engine* engine, Texture* texture, int viewport_width, int viewport_height,
        int horizon_y, float yaw, float horizontal_fov);
    void renderCloudLayer(Engine* engine, int viewport_width, int viewport_height, int horizon_y,
        float pitch, float yaw, float horizontal_fov);
    void renderFallbackGradient(Engine* engine, int viewport_width, int viewport_height, int horizon_y);
    void renderTextureGradientBackdrop(Engine* engine, int viewport_width, int viewport_height);
};
