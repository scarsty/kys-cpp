#include "BattleRoleManager.h"
#include "ChessBalance.h"
#include "ChessEquipment.h"
#include "GameState.h"

namespace KysChess
{

StarBoostedStats BattleRoleManager::computeStarStats(const Role* role, int stars, int fightsWon, int extraFightWinGrowthHP, int extraFightWinGrowthAtk, int extraFightWinGrowthDef)
{
    auto& cfg = ChessBalance::config();
    int normalizedStars = (std::max)(stars, 1);
    int normalizedFightsWon = (std::max)(fightsWon, 0);
    int s = normalizedStars - 1;  // 1-based: star 1 = no bonus
    int baseHP = role->MaxHP + normalizedFightsWon * (cfg.fightWinGrowthHP + extraFightWinGrowthHP);
    int baseATK = role->Attack + normalizedFightsWon * (cfg.fightWinGrowthAtk + extraFightWinGrowthAtk);
    int baseDEF = role->Defence + normalizedFightsWon * (cfg.fightWinGrowthDef + extraFightWinGrowthDef);
    int baseSPD = role->Speed + normalizedFightsWon * cfg.fightWinGrowthSpeed / 2;
    int baseWeapon = normalizedFightsWon * cfg.fightWinGrowthWeapon / 2;
    double hpM = 1.0 + cfg.starHPMult * s;
    double atkM = 1.0 + cfg.starAtkMult * s;
    double defM = 1.0 + cfg.starDefMult * s;
    double spdM = 1.0 + cfg.starSpdMult * s;
    int actFlat = 15 * s;
    return {
        static_cast<int>(baseHP * hpM) + cfg.starFlatHP * s,
        static_cast<int>(baseATK * atkM) + cfg.starFlatAtk * s,
        static_cast<int>(baseDEF * defM) + cfg.starFlatDef * s,
        static_cast<int>(baseSPD * spdM),
        static_cast<int>((role->Fist + baseWeapon) * defM) + actFlat,
        static_cast<int>((role->Sword + baseWeapon) * defM) + actFlat,
        static_cast<int>((role->Knife + baseWeapon) * defM) + actFlat,
        static_cast<int>((role->Unusual + baseWeapon) * defM) + actFlat,
        static_cast<int>((role->HiddenWeapon + baseWeapon) * defM) + actFlat,
    };
}

void BattleRoleManager::applyStarBonus(Role* role, int stars, int fightsWon, int extraFightWinGrowthHP, int extraFightWinGrowthAtk, int extraFightWinGrowthDef)
{
    role->Star = (std::max)(stars, 1);  // gate magic slots by star level
    auto s = computeStarStats(role, role->Star, fightsWon, extraFightWinGrowthHP, extraFightWinGrowthAtk, extraFightWinGrowthDef);
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
}

}  // namespace KysChess
