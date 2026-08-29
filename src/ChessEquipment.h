#pragma once
#include "ChessBattleEffectTypes.h"
#include "ChessDiagnostics.h"
#include "ChessNonBattleRules.h"
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
    std::vector<EffectRule> rules;
    std::vector<ChessNonBattleRule> managementRules;
};

inline const char* chessEquipmentTypeName(int equipType)
{
    return equipType == 0 ? "武器" : "防具";
}

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
    std::vector<EffectRule> rules;
    std::vector<ChessNonBattleRule> managementRules;
};

bool loadChessEquipment(
    const std::string& path,
    const ChessTextConverter& toTraditional,
    const ChessDiagnosticSink& diagnostics,
    std::vector<EquipmentDef>& equipment,
    std::vector<EquipmentSynergyDef>& synergies);

}    // namespace KysChess
