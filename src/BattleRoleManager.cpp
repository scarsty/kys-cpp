#include "BattleRoleManager.h"
#include "ChessBalance.h"
#include "ChessEquipment.h"
#include "GameState.h"

#include <cmath>

namespace KysChess
{

namespace
{

int floorGrowth(double value)
{
    return static_cast<int>(std::floor(value));
}

}  // namespace

StarBoostedStats BattleRoleManager::computeStarStats(const Role* role, int stars, int fightsWon, int extraFightWinGrowthHP, int extraFightWinGrowthAtk, int extraFightWinGrowthDef)
{
    auto& cfg = ChessBalance::config();
    int normalizedStars = (std::max)(stars, 1);
    int normalizedFightsWon = (std::max)(fightsWon, 0);
    int s = normalizedStars - 1;  // 1-based: star 1 = no bonus
    int winHP = floorGrowth(normalizedFightsWon * (cfg.fightWinGrowthHP + extraFightWinGrowthHP));
    int winATK = floorGrowth(normalizedFightsWon * (cfg.fightWinGrowthAtk + extraFightWinGrowthAtk));
    int winDEF = floorGrowth(normalizedFightsWon * (cfg.fightWinGrowthDef + extraFightWinGrowthDef));
    int winSPD = floorGrowth(normalizedFightsWon * cfg.fightWinGrowthSpeed);
    int winWeapon = floorGrowth(normalizedFightsWon * cfg.fightWinGrowthWeapon);
    double hpM = 1.0 + cfg.starHPMult * s;
    double atkM = 1.0 + cfg.starAtkMult * s;
    double defM = 1.0 + cfg.starDefMult * s;
    double spdM = 1.0 + cfg.starSpdMult * s;
    int actFlat = 15 * s;
    return {
        static_cast<int>(role->MaxHP * hpM) + cfg.starFlatHP * s + winHP,
        static_cast<int>(role->Attack * atkM) + cfg.starFlatAtk * s + winATK,
        static_cast<int>(role->Defence * defM) + cfg.starFlatDef * s + winDEF,
        static_cast<int>(role->Speed * spdM) + winSPD,
        static_cast<int>(role->Fist * defM) + actFlat + winWeapon,
        static_cast<int>(role->Sword * defM) + actFlat + winWeapon,
        static_cast<int>(role->Knife * defM) + actFlat + winWeapon,
        static_cast<int>(role->Unusual * defM) + actFlat + winWeapon,
        static_cast<int>(role->HiddenWeapon * defM) + actFlat + winWeapon,
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
