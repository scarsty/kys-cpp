#pragma once

#include <string>

struct BattleUnitIdentity
{
    int battleId = -1;
    int realRoleId = -1;
    int team = -1;
    int headId = -1;
    std::string name;
};
