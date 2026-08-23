#pragma once

#include "BattleCastLifecycle.h"
#include "BattleDamageSystem.h"
#include "BattleEffectSystem.h"
#include "BattlePresentation.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KysChess::Battle
{

struct BattleDamagePresentationInput
{
    bool enabled = false;
    bool critical = false;
    int criticalMultiplier = 0;
    bool ultimate = false;
    bool executed = false;
    std::string skillName;
    int skillId = -1;
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
    int executeThresholdPct{};
    bool canTriggerDefenderBlock = false;
    BattleAttackProvenance provenance;
    CastWorkToken delayedCastWork;
    EffectDamageOrigin effectOrigin = EffectEnvironmentDamageOrigin{};
    // 非零時，這筆傷害完成後會釋放同一效果動作序列的後續命令。
    std::uint64_t effectCommandContinuationId{};
};

}  // namespace KysChess::Battle
