#pragma once

#include "BattlePresentation.h"

#include <vector>

namespace KysChess::Battle
{

enum class BattlePresentationCommandType
{
    SpawnFloatingText,
    SpawnRoleEffect,
    SpawnDamageNumber,
    FocusCamera,
    SpawnProjectile,
    MoveProjectile,
    ImpactProjectile,
    ExpireProjectile,
    CancelProjectile,
    BounceProjectile,
};

struct BattlePresentationCommand
{
    BattlePresentationCommandType type{};
    int frame{};
    int sourceUnitId{};
    int targetUnitId{};
    int amount{};
    int durationFrames{};
    int effectId{};
    int textSize{};
    int textMotionType{};
    std::string text;
    std::string skillName;
    std::string detailText;
    BattlePresentationColor color;
    Pointf position;
    int visualEffectId{};
    int projectileAttackId{};
    int projectileRelatedAttackId{};
    int projectileSourceUnitId{};
    int projectileTargetUnitId{};
    Pointf projectilePosition;
    Pointf projectileVelocity;
    int projectileDurationFrames{};
    int projectileOperationKind{};
    bool projectileThrough = false;
};

struct BattlePresentationPlaybackPlan
{
    std::vector<BattlePresentationCommand> commands;
};

class BattlePresentationPlaybackPlanner
{
public:
    BattlePresentationPlaybackPlan build(const BattlePresentationFrame& frame) const;

private:
    BattlePresentationCommand makeCommand(const BattleVisualEvent& event) const;
};

}  // namespace KysChess::Battle
