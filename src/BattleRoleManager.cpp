#include "BattleRoleManager.h"
#include "ChessBalance.h"

namespace KysChess
{

StarBoostedStats BattleRoleManager::computeStarStats(const Role* role, int stars)
{
    auto& cfg = ChessBalance::config();
    int s = stars - 1;  // 1-based: star 1 = no bonus
    double hpM = 1.0 + cfg.starHPMult * s;
    double atkM = 1.0 + cfg.starAtkMult * s;
    double defM = 1.0 + cfg.starDefMult * s;
    double spdM = 1.0 + cfg.starSpdMult * s;
    int actFlat = 15 * s;
    return {
        static_cast<int>(role->MaxHP * hpM) + cfg.starFlatHP * s,
        static_cast<int>(role->Attack * atkM) + cfg.starFlatAtk * s,
        static_cast<int>(role->Defence * defM) + cfg.starFlatDef * s,
        static_cast<int>(role->Speed * spdM),
        static_cast<int>(role->Fist * defM) + actFlat,
        static_cast<int>(role->Sword * defM) + actFlat,
        static_cast<int>(role->Knife * defM) + actFlat,
        static_cast<int>(role->Unusual * defM) + actFlat,
        static_cast<int>(role->HiddenWeapon * defM) + actFlat,
    };
}

void BattleRoleManager::applyStarBonus(Role* role, int stars)
{
    if (stars <= 1) return;
    auto s = computeStarStats(role, stars);
    role->MaxHP = s.hp;
    role->HP = s.hp;
    role->Attack = s.atk;
    role->Defence = s.def;
    role->Speed = s.spd;
    role->Fist = s.fist;
    role->Sword = s.sword;
    role->Knife = s.knife;
    role->Unusual = s.unusual;
    role->HiddenWeapon = s.hidden;

    // 3-star pieces max out their ulti skill level
    if (stars >= 3 && role->MagicID[1] > 0)
        role->MagicLevel[1] = 999;
}

}  // namespace KysChess
