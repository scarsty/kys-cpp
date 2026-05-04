#pragma once

#include "BattleDamageSystem.h"
#include "BattleDeathEffectSystem.h"
#include "BattleHitResolver.h"
#include "BattlePresentation.h"

#include <map>
#include <vector>

namespace KysChess::Battle
{

struct BattleDamageApplicationUnitSnapshot
{
    int id = -1;
    int team = 0;
    bool alive = true;
};

struct BattleDamageApplicationUnitEffects
{
    int deathAoePct = 0;
    int deathAoeStunFrames = 0;
    int deathAoeMaxTargets = 0;
};

enum class BattleDamageLifecycleEventType
{
    UnitDied,
    KillRecorded,
    BattleEnded,
};

struct BattleDamageLifecycleEvent
{
    BattleDamageLifecycleEventType type = BattleDamageLifecycleEventType::UnitDied;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int value = 0;
};

struct BattleDamageApplicationInput
{
    int frame = 0;
    std::vector<BattleDamageApplicationUnitSnapshot> units;
    std::vector<BattleDamageTransactionInput> pendingTransactions;
    std::map<int, BattleDamageApplicationUnitEffects> unitEffects;
    std::map<int, int> pendingAliveByTeam;
    BattleDeathEffectWorld deathEffects;
};

struct BattleDamageApplicationResult
{
    std::vector<BattleDamageTransactionResult> transactions;
    std::vector<BattleDamageLifecycleEvent> lifecycleEvents;
    std::vector<BattlePresentationEvent> presentationEvents;
    std::vector<BattleGameplayCommand> commands;
    BattleDeathEffectWorld deathEffects;
    bool battleEnded = false;
    int winningTeam = -1;
};

class BattleDamageApplicationSystem
{
public:
    BattleDamageApplicationResult apply(const BattleDamageApplicationInput& input) const;
};

}  // namespace KysChess::Battle
