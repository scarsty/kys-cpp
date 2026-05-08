#include "BattleRoleManager.h"
#include "ChessEquipment.h"
#include "GameState.h"

namespace KysChess
{

StarBoostedStats BattleRoleManager::computeStarStats(const Role* role, int stars, int fightsWon, int extraFightWinGrowthHP, int extraFightWinGrowthAtk, int extraFightWinGrowthDef)
{
    return computeStarBoostedStats(
        {
            role->MaxHP,
            role->Attack,
            role->Defence,
            role->Speed,
            role->Fist,
            role->Sword,
            role->Knife,
            role->Unusual,
            role->HiddenWeapon,
        },
        stars,
        fightsWon,
        extraFightWinGrowthHP,
        extraFightWinGrowthAtk,
        extraFightWinGrowthDef);
}

void BattleRoleManager::applyStarBonus(Role* role, int stars, int fightsWon, int extraFightWinGrowthHP, int extraFightWinGrowthAtk, int extraFightWinGrowthDef)
{
    role->Star = normalizeBattleStar(stars);  // gate magic slots by star level
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
