#pragma once
#include "ChessBattleEffects.h"
#include "Types.h"
#include <string>
#include <vector>

namespace KysChess
{

struct Chess;

struct EquipmentDef
{
    int itemId;
    int tier;
    int equipType;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;

    const Item* getItem() const;
};

struct EquipmentSynergyDef
{
    std::vector<int> roleIds;
    int equipmentId = -1;
    std::vector<ComboEffect> effects;
    std::vector<std::string> actAsComboNames;
};

class ChessEquipment
{
public:
    static void loadConfig();
    static const std::vector<EquipmentDef>& getAll();
    static const EquipmentDef* getById(int itemId);
    static std::vector<const EquipmentDef*> getByTier(int tier);
    static const std::vector<EquipmentSynergyDef>& getAllSynergies();
    static std::vector<const EquipmentSynergyDef*> getSynergiesFor(int roleId, int weaponId, int armorId);
    static void applyActiveSynergies(Chess& chess);
    static std::vector<Chess> withActiveSynergies(std::vector<Chess> chesses);
};

}    // namespace KysChess
