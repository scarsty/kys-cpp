#pragma once

#include "BattleCastSystem.h"
#include "BattleEffectSystem.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace KysChess::Battle
{

struct BattleEffectFreeAdditionalCast
{
    EffectCommandMetadata metadata;
    int targetUnitId = -1;
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
};

struct BattleEffectCastPreparation
{
    std::optional<int> mpCost;
    std::optional<CastRangeMode> rangeMode;
    std::optional<AttackPattern> replacementPattern;
    std::optional<CastPropagationPolicy> propagation;
    std::vector<BattleEffectFreeAdditionalCast> freeAdditionalCasts;
};

struct BattleEffectAttackApplyState
{
    int nextSharedHitGroupId = 1;
    std::optional<BattleAttackProvenance> sourceAttackProvenance;
};

enum class BattleEffectDeferredAttackReason
{
    MissingBaseAttack,
    MissingTargetPosition,
    MissingProjectileVelocity,
    SameTargetHitLimitNeedsRuntimeTracking,
};

struct BattleEffectDeferredAttack
{
    EffectCommandMetadata metadata;
    std::optional<std::size_t> requestIndex;
    BattleEffectDeferredAttackReason reason{};
    AttackPattern pattern;
    AttackTargetPolicy targetPolicy = AttackTargetPolicy::Preserve;
    int targetUnitId = -1;
    int sameTargetHitLimit{};
};

struct BattleEffectAttackTargetDirective
{
    EffectCommandMetadata metadata;
    std::size_t requestIndex{};
    AttackTargetPolicy policy = AttackTargetPolicy::Preserve;
    int targetUnitId = -1;
    bool velocityResolved = false;
};

struct BattleEffectAttackDamageDirective
{
    EffectCommandMetadata metadata;
    std::size_t requestIndex{};
    std::optional<int> damageOverride;
    std::optional<BattleDamageKind> damageKind;
    bool overrideStoredInScriptedDamage = false;
};

struct BattleEffectAttackHitLimitDirective
{
    EffectCommandMetadata metadata;
    std::size_t requestIndex{};
    int sameTargetHitLimit{};
    std::optional<int> sharedHitGroupId;
};

struct BattleEffectAttackLifecycleDirective
{
    EffectCommandMetadata metadata;
    std::size_t requestIndex{};
    CastPropagationPolicy propagation = CastPropagationPolicy::SourceRules;
    BattleAttackOriginKind origin = BattleAttackOriginKind::CastDerived;
    std::optional<BattleAttackId> parentAttackId;
    bool reservationRequired = false;
    bool rootAttack = false;
};

struct BattleEffectAttackApplyResult
{
    std::vector<BattleEffectAttackTargetDirective> targets;
    std::vector<BattleEffectAttackDamageDirective> damage;
    std::vector<BattleEffectAttackHitLimitDirective> hitLimits;
    std::vector<BattleEffectAttackLifecycleDirective> lifecycle;
    std::vector<BattleEffectDeferredAttack> deferred;
    std::optional<AttackPattern> deferredReplacementPattern;
    std::optional<AttackPattern> effectivePattern;
};

class BattleEffectAttackCastSystem
{
public:
    BattleEffectCastPreparation prepareCast(
        BattleCastInput& input,
        std::span<const EffectCommand> commands) const;
    BattleEffectCastPreparation prepareCast(
        BattleCastInput& input,
        bool ultimate,
        std::span<const EffectCommand> commands) const;

    BattleEffectAttackApplyResult applyPreparedCast(
        const BattleCastInput& input,
        BattleCastResult& result,
        const BattleEffectCastPreparation& preparation,
        BattleEffectAttackApplyState& state) const;

    BattleEffectAttackApplyResult applyAttackCommands(
        const BattleCastInput& input,
        BattleCastResult& result,
        std::span<const EffectCommand> commands,
        BattleEffectAttackApplyState& state) const;

    BattleEffectAttackApplyResult applyAttackCommands(
        const BattleCastInput& input,
        std::vector<BattleAttackSpawnRequest>& requests,
        std::span<const EffectCommand> commands,
        BattleEffectAttackApplyState& state) const;
};

}  // namespace KysChess::Battle
