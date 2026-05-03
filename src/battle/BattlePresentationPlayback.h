#pragma once

#include "BattlePresentation.h"

#include <vector>

namespace KysChess::Battle
{

enum class BattlePresentationCommandType
{
    RecordDamage,
    RecordHeal,
    RecordStatus,
    SpawnFloatingText,
    SpawnRoleEffect,
    SpawnDamageNumber,
    FocusCamera,
    MoveProjectile,
    HitProjectile,
    ExpireProjectile,
    LoseProjectileTarget,
    CancelProjectile,
    BounceProjectile,
};

struct BattlePresentationCommand
{
    BattlePresentationCommandType type = BattlePresentationCommandType::RecordStatus;
    int frame = 0;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    int durationFrames = 0;
    int effectId = -1;
    int textSize = 0;
    int textMotionType = 0;
    std::string text;
    std::string skillName;
    std::string detailText;
    BattlePresentationColor color;
    Pointf position;
};

struct BattlePresentationPlaybackPlan
{
    BattlePresentationSnapshot snapshot;
    std::vector<BattlePresentationCommand> commands;
};

class BattlePresentationPlaybackPlanner
{
public:
    BattlePresentationPlaybackPlan build(const BattlePresentationFrame& frame) const;

private:
    BattlePresentationCommand makeCommand(const BattlePresentationEvent& event) const;
};

}  // namespace KysChess::Battle
