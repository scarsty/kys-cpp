#include "DayNightDisplay.h"

#include "DayNightSystem.h"
#include "Font.h"

void DayNightDisplay::draw()
{
    auto day_night = DayNightSystem::getInstance();
    if (!day_night->isDynamic())
    {
        return;
    }
    int ui_width, ui_height;
    Engine::getInstance()->getUISize(ui_width, ui_height);
    Font::getInstance()->draw(day_night->getTimeLabel(), 20, 16, ui_height - 32,
        { 255, 255, 255, 255 }, 255);
}