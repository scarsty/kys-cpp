#pragma once
#include "ChessBattleEffects.h"
#include "Types.h"
#include <vector>

namespace KysChess
{

struct EquipmentDef
{
    int itemId;
    int tier;
    int equipType;
    std::vector<ComboEffect> effects;

    const Item* getItem() const;
};

class ChessEquipment
{
public:
    static void loadConfig();
    static const std::vector<EquipmentDef>& getAll();
    static const EquipmentDef* getById(int itemId);
    static std::vector<const EquipmentDef*> getByTier(int tier);
};

}    // namespace KysChess
