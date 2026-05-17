#pragma once

#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattlePresentation.h"

#include <map>
#include <string>
#include <vector>

namespace KysChess::Battle
{

class BattleRuntimeRandom;
struct BattleUnitStore;

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
};

enum class BattleDamageLifecycleEventType
{
    UnitDied,
    KillRecorded,
    BattleEnded,
};

struct BattleDamageLifecycleEvent
{
    BattleDamageLifecycleEventType type{};
    int sourceUnitId{};
    int targetUnitId{};
    int value{};
};

struct BattleDamageApplicationInput
{
    int frame = 0;
    bool sortPendingDamageByDefenderMagnitude = false;
    BattleUnitStore* units = nullptr;
    std::map<int, KysChess::RoleComboState>* comboUnits = nullptr;
    std::vector<BattleStatusRuntimeUnit>* statusUnits = nullptr;
    std::vector<BattleDamageRuntimeUnit>* damageUnitExtras = nullptr;
    const std::vector<BattlePendingDamageIntent>* pendingDamage = nullptr;
    const std::map<int, BattleDamageApplicationUnitEffects>* unitEffects = nullptr;
    BattleDeathEffectStore* deathEffects = nullptr;
    const BattleProjectileFollowUpContext* projectileFollowUps = nullptr;
    BattleRuntimeRandom* random = nullptr;
};

struct BattleDamageApplicationResult
{
    std::vector<BattleDamageTransactionResult> transactions;
    std::vector<BattleDamageLifecycleEvent> lifecycleEvents;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<BattleGameplayCommand> commands;
    bool battleEnded = false;
    int winningTeam = -1;
};

class BattleDamageApplicationSystem
{
public:
    BattleDamageApplicationResult apply(const BattleDamageApplicationInput& input) const;
    int previewDefenderPendingHpDamage(BattleDamageApplicationInput input, int defenderUnitId) const;
};

}  // namespace KysChess::Battle
