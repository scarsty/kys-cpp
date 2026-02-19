#include "BattleRoleManager.h"
#include "ChessBalance.h"

namespace KysChess
{

void BattleRoleManager::applyStarBonus(Role* role, int stars)
{
    if (stars <= 0) return;
    auto& cfg = ChessBalance::config();
    double hpAtkMult = 1.0 + cfg.starHPAtkMult * stars;
    double defMult = 1.0 + cfg.starDefMult * stars;
    double spdMult = 1.0 + cfg.starSpdMult * stars;
    role->MaxHP = static_cast<int>(role->MaxHP * hpAtkMult);
    role->HP = role->MaxHP;
    role->Attack = static_cast<int>(role->Attack * hpAtkMult);
    role->Defence = static_cast<int>(role->Defence * defMult);
    role->Speed = static_cast<int>(role->Speed * spdMult);
    role->Fist = static_cast<int>(role->Fist * defMult);
    role->Sword = static_cast<int>(role->Sword * defMult);
    role->Knife = static_cast<int>(role->Knife * defMult);
    role->Unusual = static_cast<int>(role->Unusual * defMult);
    role->HiddenWeapon = static_cast<int>(role->HiddenWeapon * defMult);

    // 3-star pieces max out their ulti skill level
    if (stars >= 2 && role->MagicID[1] > 0)
        role->MagicLevel[1] = 999;
}

}  // namespace KysChess
