#pragma once
#include "ChessBattleEffects.h"
#include "ChessDiagnostics.h"
#include "Types.h"
#include <string>
#include <vector>

namespace KysChess
{

struct EquipmentDef
{
    int itemId;
    int tier;
    int equipType;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;

};

inline std::vector<const EquipmentDef*> filterEquipmentByMaxTier(const std::vector<EquipmentDef>& equipments, int maxTier)
{
    std::vector<const EquipmentDef*> result;
    for (const auto& equipment : equipments)
    {
        if (equipment.tier <= maxTier)
        {
            result.push_back(&equipment);
        }
    }
    return result;
}

struct EquipmentSynergyDef
{
    std::vector<int> roleIds;
    int equipmentId = -1;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;
};

bool loadChessEquipment(
    const std::string& path,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    std::vector<EquipmentDef>& equipment,
    std::vector<EquipmentSynergyDef>& synergies);

}    // namespace KysChess
