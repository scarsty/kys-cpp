#pragma once

#include "BattleDamageSystem.h"
#include "BattlePresentation.h"

#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleDamageApplicationUnitEffects
{
    int deathAoePct = 0;
    int deathAoeStunFrames = 0;
    int deathAoeMaxTargets = 0;
};

struct BattleDamagePresentationInput
{
    bool enabled = false;
    bool critical = false;
    bool ultimate = false;
    bool executed = false;
    std::string skillName;
    std::vector<BattleLogTextSegment> segments;
    BattlePresentationColor normalDamageColor;
    BattlePresentationColor emphasizedDamageColor;
    BattlePresentationColor executeTextColor{ 255, 136, 48, 255 };
    int normalDamageTextSize = 0;
    int emphasizedDamageTextSize = 0;
    int executeTextSize = 0;
};

struct BattleDamagePresentationStyle
{
    BattlePresentationColor normalDamageColor;
    BattlePresentationColor emphasizedDamageColor;
    BattlePresentationColor executeTextColor{ 255, 136, 48, 255 };
    int normalDamageTextSize = 0;
    int emphasizedDamageTextSize = 0;
    int executeTextSize = 0;
};

struct BattlePendingDamageIntent
{
    BattleDamageRequest request;
    BattleDamagePresentationInput presentation;
    bool canTriggerExecute = false;
    bool canTriggerDefenderBlock = false;
};

}  // namespace KysChess::Battle
