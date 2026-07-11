#pragma once

#include "../Point.h"

#include <cassert>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

namespace KysChess::Battle
{

inline constexpr int BattlePresentationCurrentFrame = -1;

inline std::string criticalMultiplierLabel(int criticalMultiplier)
{
    assert(criticalMultiplier >= 100);

    if (criticalMultiplier % 100 == 0)
    {
        return std::format("x{}", criticalMultiplier / 100);
    }
    if (criticalMultiplier % 10 == 0)
    {
        return std::format("x{:.1f}", criticalMultiplier / 100.0);
    }
    return std::format("x{:.2f}", criticalMultiplier / 100.0);
}

struct BattlePresentationColor
{
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

enum class BattleLogEventType
{
    Damage,
    Heal,
    Status,
    UnitDied,
    BattleEnded,
};

enum class BattleLogCategory
{
    Status,
    Cast,
    ProjectileCancel,
};

enum class BattleLogPerspective
{
    Targeted,
    SourceOnly,
};

enum class BattleLogTextTone
{
    Default,
    AllyName,
    EnemyName,
    SkillName,
    DamageValue,
    HealValue,
    ShieldValue,
    ResourceValue,
    DurationValue,
    FrameValue,
    FormulaValue,
    ProjectileId,
    SystemAccent,
    Positive,
    Negative,
};

struct BattleLogTextSegment
{
    std::string text;
    BattleLogTextTone tone = BattleLogTextTone::Default;
};

enum class BattleVisualEventType
{
    FloatingText,
    RoleEffect,
    DamageNumber,
    CameraFocus,
    ProjectileSpawned,
    ProjectileMoved,
    ProjectileHit,
    ProjectileExpired,
    ProjectileTargetLost,
    ProjectileCancelled,
    ProjectileBounced,
};

struct BattleLogEvent
{
    BattleLogEventType type = BattleLogEventType::Status;
    int frame = BattlePresentationCurrentFrame;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    BattleLogCategory category = BattleLogCategory::Status;
    BattleLogPerspective perspective = BattleLogPerspective::Targeted;
    std::vector<BattleLogTextSegment> segments;
    std::string skillName;
    int secondaryAmount = 0;
};

enum class BattleGameplayEventType
{
    CastStarted,
    AttackSpawned,
    ProjectileMoved,
    ProjectileHit,
    ProjectileExpired,
    ProjectileCancelled,
    DamageApplied,
    StatusApplied,
    ResourceChanged,
    UnitDied,
    BattleEnded,
};

struct BattleGameplayEvent
{
    BattleGameplayEventType type = BattleGameplayEventType::StatusApplied;
    int frame = BattlePresentationCurrentFrame;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    int effectId = -1;
    Pointf position;
    std::string text;
    int otherAttackId = -1;
};

struct BattleVisualEvent
{
    BattleVisualEventType type = BattleVisualEventType::FloatingText;
    int frame = BattlePresentationCurrentFrame;
    int sourceUnitId = -1;
    int targetUnitId = -1;
    int amount = 0;
    int durationFrames = 0;
    int effectId = -1;
    int textSize = 0;
    int criticalMultiplier = 0;
    int textMotionType = 0;
    std::string text;
    std::string skillName;
    std::vector<BattleLogTextSegment> segments;
    BattlePresentationColor color;
    Pointf position;
    int visualEffectId = -1;
    Pointf velocity;
    int operationKind = -1;
    int impactEffectSoundId = -1;
    int impactUnitShake = 0;
    int impactSceneShake = 0;
    bool impactRumble = false;
    bool through = false;
};

struct BattleFrameRumbleEvent
{
    int lowFrequency{};
    int highFrequency{};
    int durationMs{};
};

struct BattlePresentationFrame
{
    int frame = 0;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
    std::vector<int> attackSoundIds;
    std::vector<BattleFrameRumbleEvent> rumbles;
    int blinkSoundCount{};
};

}  // namespace KysChess::Battle
