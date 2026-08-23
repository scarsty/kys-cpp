#pragma once

#include "../ChessBattleEffects.h"
#include "../Point.h"

#include <compare>
#include <optional>
#include <vector>

namespace KysChess::Battle
{

struct BattleGridTransform;
class BattleRuntimeUnits;

struct BattleAreaId
{
    int value = -1;

    bool isValid() const { return value >= 0; }
    auto operator<=>(const BattleAreaId&) const = default;
};

struct BattleAreaGeometry
{
    AreaShape shape{};
    int radiusTiles = 0;
    int squareSideTiles = 0;
};

enum class BattleAreaAnchorKind
{
    FixedWorldPosition,
    FollowSourceUnit,
};

struct BattleAreaAnchor
{
    BattleAreaAnchorKind kind{};
    Pointf fixedPosition;
    int sourceUnitId = -1;
};

struct BattleAreaMergeKey
{
    EffectSourceBinding source;
    EffectRuleId ruleId;
    int targetTeamDomain = -1;
};

struct BattleAreaEffect
{
    BattleAreaId id;
    EffectSourceBinding source;
    int sourceTeam{};
    BattleAreaGeometry geometry;
    BattleAreaAnchor anchor;
    int createdFrame{};
    int expiresFrameExclusive{};
    AreaSourceDeathPolicy sourceDeath{};
    BattleAreaMergeKey mergeKey;
    AreaMergePolicy merge{};
    std::vector<AreaModifier> modifiers;
};

struct BattleAreaEffectState
{
    int nextAreaId{};
    std::vector<BattleAreaEffect> areas;
};

struct BattleAreaCreateRequest
{
    EffectSourceBinding source;
    EffectRuleId ruleId;
    int targetTeamDomain = -1;
    BattleAreaGeometry geometry;
    BattleAreaAnchor anchor;
    int currentFrame{};
    int durationFrames{};
    AreaSourceDeathPolicy sourceDeath{};
    AreaMergePolicy merge{};
    std::vector<AreaModifier> modifiers;
};

enum class BattleAreaLifecycleEventType
{
    Created,
    Refreshed,
    Removed,
};

enum class BattleAreaRemovalReason
{
    Explicit,
    Expired,
    SourceDied,
    Replaced,
};

struct BattleAreaLifecycleEvent
{
    BattleAreaLifecycleEventType type{};
    BattleAreaId areaId;
    BattleAreaRemovalReason removalReason = BattleAreaRemovalReason::Explicit;
};

struct BattleAreaCreateResult
{
    BattleAreaId areaId;
    std::vector<BattleAreaLifecycleEvent> events;
};

enum class BattleAreaQueryPhase
{
    Any,
    UnitAttribute,
    OutgoingDamage,
    AttackSpawn,
    ForcedMovement,
};

struct BattleAreaRef
{
    BattleAreaId id;
    const BattleAreaEffect* area = nullptr;
};

struct BattleAreaAppliedModifier
{
    BattleAreaId areaId;
    AreaModifier modifier;
};

struct BattleAreaUnitModifiers
{
    std::vector<BattleAreaAppliedModifier> modifiers;
};

struct BattleAreaAttackSpawnModifiers
{
    std::optional<bool> tracking;
    std::optional<int> speedPct;
    std::optional<int> projectilePressurePct;
};

class BattleAreaEffectSystem
{
public:
    static BattleAreaCreateResult create(
        BattleAreaEffectState& state,
        BattleAreaCreateRequest request);

    static std::optional<BattleAreaLifecycleEvent> remove(
        BattleAreaEffectState& state,
        BattleAreaId areaId,
        BattleAreaRemovalReason reason = BattleAreaRemovalReason::Explicit);

    static std::vector<BattleAreaLifecycleEvent> removeExpired(
        BattleAreaEffectState& state,
        int currentFrame);

    static std::vector<BattleAreaLifecycleEvent> removeForSourceDeath(
        BattleAreaEffectState& state,
        int sourceUnitId);

    static bool activeAt(const BattleAreaEffect& area, int frame);

    static Pointf center(
        const BattleAreaEffect& area,
        const BattleRuntimeUnits& units);

    static bool containsUnit(
        const BattleAreaEffect& area,
        const BattleGridTransform& gridTransform,
        const BattleRuntimeUnits& units,
        int unitId,
        int frame);

    static std::vector<BattleAreaRef> areasContainingUnit(
        const BattleAreaEffectState& state,
        const BattleGridTransform& gridTransform,
        const BattleRuntimeUnits& units,
        int unitId,
        int frame,
        BattleAreaQueryPhase phase = BattleAreaQueryPhase::Any);

    static BattleAreaUnitModifiers collectAreaUnitModifiers(
        const BattleAreaEffectState& state,
        const BattleGridTransform& gridTransform,
        const BattleRuntimeUnits& units,
        int unitId,
        int frame,
        BattleAreaQueryPhase phase);

    static BattleAreaAttackSpawnModifiers collectAreaAttackSpawnModifiers(
        const BattleAreaEffectState& state,
        const BattleGridTransform& gridTransform,
        const BattleRuntimeUnits& units,
        int sourceUnitId,
        int frame);

    static bool blocksForcedMovement(
        const BattleAreaEffectState& state,
        const BattleGridTransform& gridTransform,
        const BattleRuntimeUnits& units,
        int targetUnitId,
        int frame,
        ForceMoveDirection direction);
};

}  // namespace KysChess::Battle
