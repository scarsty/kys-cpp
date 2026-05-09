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

struct BattleUnitStore;

struct BattleDamageApplicationUnitSnapshot
{
    int id{};
    int team{};
    bool alive{};
};

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
    std::string detailText;
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
    bool aggregatePendingTransactionsByDefender = false;
    std::vector<BattleDamageApplicationUnitSnapshot> units;
    const std::vector<BattleDamageTransactionInput>* pendingTransactions = nullptr;
    const std::vector<BattleDamagePresentationInput>* pendingPresentation = nullptr;
    const std::map<int, BattleDamageApplicationUnitEffects>* unitEffects = nullptr;
    const std::map<int, int>* pendingAliveByTeam = nullptr;
    BattleDeathEffectStore* deathEffects = nullptr;
    BattleUnitStore* deathEffectUnits = nullptr;
    const BattleProjectileFollowUpContext* projectileFollowUps = nullptr;
    const BattleUnitStore* projectileFollowUpUnits = nullptr;
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
};

}  // namespace KysChess::Battle
