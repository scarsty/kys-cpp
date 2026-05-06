#pragma once

#include "../Point.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KysChess::Battle
{

inline constexpr int BattlePresentationCurrentFrame = -1;

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
    std::string text;
    std::string skillName;
    std::string detailText;
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
    int textMotionType = 0;
    std::string text;
    std::string skillName;
    std::string detailText;
    BattlePresentationColor color;
    Pointf position;
    int visualEffectId = -1;
    Pointf velocity;
    int operationKind = -1;
};

struct BattlePresentationUnitSnapshot
{
    int id = -1;
    int realRoleId = -1;
    std::string name;
    int team = 0;
    bool alive = true;
    int hp = 0;
    int maxHp = 0;
    int mp = 0;
    int maxMp = 0;
    int cooldown = 0;
    int invincible = 0;
    Pointf position;
    Pointf velocity;
};

struct BattlePresentationSnapshot
{
    int frame = 0;
    std::vector<BattlePresentationUnitSnapshot> units;
};

struct BattlePresentationFrame
{
    BattlePresentationSnapshot snapshot;
    std::vector<BattleGameplayEvent> gameplayEvents;
    std::vector<BattleLogEvent> logEvents;
    std::vector<BattleVisualEvent> visualEvents;
};

class BattlePresentationRecorder
{
public:
    void beginFrame(BattlePresentationSnapshot snapshot);
    void recordGameplay(BattleGameplayEvent event);
    void recordLog(BattleLogEvent event);
    void recordVisual(BattleVisualEvent event);

    const BattlePresentationFrame& frame() const;
    BattlePresentationFrame consumeFrame();

private:
    BattlePresentationFrame frame_;
};

}  // namespace KysChess::Battle
