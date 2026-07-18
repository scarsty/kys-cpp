#pragma once

#include "Font.h"
#include "TextureManager.h"

#include <cassert>
#include <string>

namespace BattleSummaryEquipmentDetail
{

inline void drawEquipment(int itemId, const std::string& name, int x, int y)
{
    if (itemId < 0)
    {
        return;
    }
    assert(!name.empty());
    TextureManager::getInstance()->renderTexture(
        "item",
        itemId,
        x,
        y,
        TextureManager::RenderInfo{{255, 255, 255, 255}, 255, 0.16, 0.16});
    Font::getInstance()->draw(name, 14, x + 19, y + 1, {200, 200, 200, 255});
}

}

inline void drawBattleSummaryEquipment(
    int weaponId,
    const std::string& weaponName,
    int armorId,
    const std::string& armorName,
    int x,
    int y)
{
    BattleSummaryEquipmentDetail::drawEquipment(weaponId, weaponName, x, y);
    BattleSummaryEquipmentDetail::drawEquipment(armorId, armorName, x, y + 20);
}
