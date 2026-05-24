#pragma once

#include "BattleUnitIdentity.h"

#include <string>
#include <vector>

struct BattlePostBattleUnitSummary
{
    BattleUnitIdentity identity;
    int star = 1;
    int chessInstanceId = -1;
    int hp = 0;
    int maxHp = 0;
    int attack = 0;
    int defence = 0;
    int speed = 0;
    int weaponId = -1;
    int armorId = -1;
    std::string skillNames;
    int hpRemaining = 0;
    int maxHpRemaining = 0;
    bool dead = false;
    int cancelDmg = 0;
};

struct BattlePostBattleSummary
{
    std::vector<BattlePostBattleUnitSummary> allies;
    std::vector<BattlePostBattleUnitSummary> enemies;
    int battleResult = -1;
};
