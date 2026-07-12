#include "ChessAsciiBoard.h"

#include "BattleSetupFactory.h"

#include <algorithm>
#include <format>
#include <map>

namespace KysChess
{

std::string ChessAsciiBoard::render(
    const PreparedChessBattle& prepared,
    const ChessGameContent& content)
{
    const auto input = BattleSetupFactory::build(prepared, content, {}, 36000);
    std::map<std::pair<int, int>, std::string> tokens;
    std::map<int, std::string> tokenByUnit;
    int ally = 0;
    int enemy = 0;
    int minX = BattlefieldData::CoordinateCount - 1;
    int minY = BattlefieldData::CoordinateCount - 1;
    int maxX = 0;
    int maxY = 0;
    for (const auto& unit : input.units)
    {
        const std::string token = unit.team == 0
            ? std::format("A{}", ++ally)
            : std::format("E{}", ++enemy);
        tokenByUnit.emplace(unit.unitId, token);
        tokens.emplace(std::pair{unit.gridX, unit.gridY}, token);
        minX = std::min(minX, unit.gridX);
        minY = std::min(minY, unit.gridY);
        maxX = std::max(maxX, unit.gridX);
        maxY = std::max(maxY, unit.gridY);
    }
    minX = std::max(0, minX - 2);
    minY = std::max(0, minY - 2);
    maxX = std::min(BattlefieldData::CoordinateCount - 1, maxX + 2);
    maxY = std::min(BattlefieldData::CoordinateCount - 1, maxY + 2);

    const ChessBattlefieldDefinition* definition = nullptr;
    if (const auto map = content.battleMaps().find(prepared.chosenMapId); map != content.battleMaps().end())
    {
        if (const auto field = content.battlefields().find(map->second.battlefieldId); field != content.battlefields().end())
        {
            definition = &field->second;
        }
    }
    BattlefieldData terrain(definition);
    std::string text = "    ";
    for (int x = minX; x <= maxX; ++x)
    {
        text += std::format("{:>3}", x);
    }
    text += "\n";
    for (int y = minY; y <= maxY; ++y)
    {
        text += std::format("{:>3} ", y);
        for (int x = minX; x <= maxX; ++x)
        {
            if (const auto token = tokens.find({x, y}); token != tokens.end())
            {
                text += std::format("{:>3}", token->second);
            }
            else
            {
                text += std::format("{:>3}", terrain.isBuilding(x, y) ? "#" : terrain.isWater(x, y) ? "~" : ".");
            }
        }
        text += "\n";
    }
    text += "圖例：\n";
    for (const auto& unit : input.units)
    {
        const auto* role = content.role(unit.realRoleId);
        text += std::format(
            "{} {} role={} ★{} HP={} 武器={} 防具={}\n",
            tokenByUnit.at(unit.unitId),
            role ? role->Name : "未知角色",
            unit.realRoleId,
            unit.star,
            unit.vitals.maxHp,
            unit.weaponId,
            unit.armorId);
    }
    return text;
}

}
