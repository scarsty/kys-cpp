#include "DayNightSystem.h"

#include "GameUtil.h"
#include "Random.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace
{
constexpr float Pi = 3.14159265358979323846f;

Color mixColor(Color from, Color to, float factor)
{
    factor = std::clamp(factor, 0.0f, 1.0f);
    auto mix = [factor](uint8_t left, uint8_t right)
    {
        return uint8_t(left + (right - left) * factor);
    };
    return { mix(from.r, to.r), mix(from.g, to.g), mix(from.b, to.b), mix(from.a, to.a) };
}

float smoothStep(float edge0, float edge1, float value)
{
    float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

std::shared_ptr<DayNightSystem> DayNightSystem::getInstance()
{
    static std::shared_ptr<DayNightSystem> instance = std::make_shared<DayNightSystem>();
    return instance;
}

void DayNightSystem::backRun()
{
    uint64_t now = Engine::getTicks();
    if (time_of_day_ < 0.0f)
    {
        RandomDouble random;
        time_of_day_ = float(random.rand());
        previous_ticks_ = now;
        return;
    }

    double elapsed_seconds = (now - previous_ticks_) / 1000.0;
    previous_ticks_ = now;
    if (!isDynamic())
    {
        time_of_day_ = 0.5f;
        return;
    }

    double cycle_seconds = GameUtil::getInstance()->getReal("game", "day_night_cycle_seconds", 1800.0);
    cycle_seconds = (std::max)(1.0, cycle_seconds);
    time_of_day_ = std::fmod(time_of_day_ + float(elapsed_seconds / cycle_seconds), 1.0f);
}

bool DayNightSystem::isDynamic() const
{
    return GameUtil::getInstance()->getInt("game", "dynamic_day_night", 1) != 0;
}

DayNightLighting DayNightSystem::getLighting() const
{
    float time = time_of_day_ < 0.0f ? 0.5f : time_of_day_;
    return getLighting(time);
}

DayNightLighting DayNightSystem::getLighting(float time) const
{
    float sun_height = std::sin((time - 0.25f) * 2.0f * Pi);
    float daylight = smoothStep(-0.14f, 0.22f, sun_height);
    float twilight = std::exp(-sun_height * sun_height * 18.0f) * (1.0f - daylight * 0.55f);
    float night = 1.0f - daylight;

    DayNightLighting lighting;
    Color night_zenith{ 6, 13, 34, 255 };
    Color night_mid{ 16, 26, 52, 255 };
    Color night_horizon{ 31, 41, 68, 255 };
    Color day_zenith{ 54, 107, 169, 255 };
    Color day_mid{ 104, 157, 204, 255 };
    Color day_horizon{ 176, 208, 228, 255 };
    Color twilight_zenith{ 100, 65, 116, 255 };
    Color twilight_mid{ 164, 98, 115, 255 };
    Color twilight_horizon{ 224, 151, 108, 255 };

    lighting.sky_zenith = mixColor(night_zenith, day_zenith, daylight);
    lighting.sky_mid = mixColor(night_mid, day_mid, daylight);
    lighting.sky_horizon = mixColor(night_horizon, day_horizon, daylight);
    lighting.sky_zenith = mixColor(lighting.sky_zenith, twilight_zenith, twilight * 0.65f);
    lighting.sky_mid = mixColor(lighting.sky_mid, twilight_mid, twilight * 0.75f);
    lighting.sky_horizon = mixColor(lighting.sky_horizon, twilight_horizon, twilight);

    Color night_ambient{ 100, 116, 150, 255 };
    Color day_ambient{ 255, 255, 255, 255 };
    Color twilight_ambient{ 255, 194, 156, 255 };
    lighting.ambient_color = mixColor(night_ambient, day_ambient, daylight);
    lighting.ambient_color = mixColor(lighting.ambient_color, twilight_ambient, twilight * 0.5f);
    lighting.sun_inner = mixColor(Color{ 224, 232, 255, 205 }, Color{ 255, 246, 180, 205 }, daylight);
    lighting.sun_outer = mixColor(Color{ 105, 124, 180, 24 }, Color{ 255, 255, 220, 16 }, daylight);
    lighting.sun_outer = mixColor(lighting.sun_outer, Color{ 235, 118, 86, 24 }, twilight);
    lighting.sun_azimuth = -Pi + time * 2.0f * Pi;
    lighting.sun_elevation = std::clamp(sun_height * 0.52f, -0.12f, 0.52f);
    lighting.star_alpha = smoothStep(0.10f, 0.48f, night);
    return lighting;
}

std::string DayNightSystem::getTimeLabel() const
{
    float time = time_of_day_ < 0.0f ? 0.5f : time_of_day_;
    int minutes = int(time * 24.0f * 60.0f) % (24 * 60);
    const char* period = "深夜";
    if (time >= 0.20f && time < 0.30f)
    {
        period = "黎明";
    }
    else if (time >= 0.30f && time < 0.70f)
    {
        period = "白天";
    }
    else if (time >= 0.70f && time < 0.80f)
    {
        period = "黄昏";
    }
    else if (time >= 0.80f || time < 0.20f)
    {
        period = "夜晚";
    }
    return std::format("{} {:02}:{:02}", period, minutes / 60, minutes % 60);
}