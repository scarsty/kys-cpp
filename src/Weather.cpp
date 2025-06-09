#include "Weather.h"
#include "Engine.h"

void Weather::draw()
{
    auto tex = Engine::getInstance()->getTexture("scene");
    uint8_t v = 255;
    if (slash_)
    {
        v = 64 + rand() % 192;
    }
    Engine::getInstance()->setColor(tex, { v, v, v, 255 });
    particle_.draw();
}

void Weather::backRun()
{
}

void Weather::setWeather(bool north, int rain_count)
{
    particle_.setPosition(Engine::getInstance()->getWindowWidth() / 2, 0);
    if (north)
    {
        particle_.setStyle(ParticleExample::SNOW);
        if (!particle_.isActive())
        {
            particle_.resetSystem();
        }
    }
    else
    {
        if (rain_count)
        {
            particle_.setStyle(ParticleExample::RAIN);
            particle_.setEmissionRate(50 * rain_count);
            particle_.setGravity({ 10, 20 });
            if (!particle_.isActive())
            {
                particle_.resetSystem();
            }
        }
        else
        {
            if (particle_.isActive())
            {
                particle_.stopSystem();
            }
        }
    }
}