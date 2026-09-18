#include "BattleEffectAttackCastSystem.h"

#include "BattleMath.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <ranges>
#include <tuple>

namespace KysChess::Battle
{

namespace
{

using OrderedEffectCommands = std::vector<const EffectCommand*>;

auto commandOrderKey(const EffectCommand& command)
{
    const auto& metadata = command.metadata;
    return std::tuple{
        metadata.ruleOrder,
        metadata.actionOrder,
        metadata.targetOrder,
        metadata.commandOrdinal,
        metadata.binding.ownerUnitId,
        static_cast<int>(metadata.binding.kind),
        metadata.binding.sourceId,
        metadata.binding.sourceTeam,
        metadata.binding.runtimeInstanceId,
        metadata.ruleId.value,
        static_cast<int>(metadata.event),
        metadata.targetUnitId,
    };
}

template <typename Command>
OrderedEffectCommands orderedCommands(std::span<const EffectCommand> commands)
{
    OrderedEffectCommands ordered;
    for (const auto& command : commands)
    {
        if (std::holds_alternative<Command>(command.value))
        {
            ordered.push_back(&command);
        }
    }
    std::ranges::sort(ordered, {}, [](const EffectCommand* command)
    {
        return commandOrderKey(*command);
    });
    return ordered;
}

auto actionGroupKey(const EffectCommand& command)
{
    const auto& metadata = command.metadata;
    return std::tuple{
        metadata.ruleOrder,
        metadata.actionOrder,
        metadata.binding.ownerUnitId,
        static_cast<int>(metadata.binding.kind),
        metadata.binding.sourceId,
        metadata.binding.sourceTeam,
        metadata.binding.runtimeInstanceId,
        metadata.ruleId.value,
        static_cast<int>(metadata.event),
    };
}

bool sameActionGroup(const EffectCommand& lhs, const EffectCommand& rhs)
{
    return actionGroupKey(lhs) == actionGroupKey(rhs);
}

BattleCastSkillState& selectedSkill(BattleCastInput& input, bool ultimate)
{
    return ultimate ? input.ultimateSkill : input.normalSkill;
}

std::optional<Pointf> targetPosition(const BattleCastInput& input, int unitId)
{
    if (unitId < 0)
    {
        return std::nullopt;
    }
    if (unitId == input.targetUnitId)
    {
        return input.targetPosition;
    }
    const auto target = std::ranges::find(
        input.projectileSpreadTargets,
        unitId,
        &BattleCastProjectileTarget::unitId);
    if (target == input.projectileSpreadTargets.end())
    {
        return std::nullopt;
    }
    return target->position;
}

int scaledStrength(int strengthPct, int modifierPct)
{
    assert(strengthPct >= 0);
    assert(modifierPct >= 0);
    const auto scaled = static_cast<std::int64_t>(strengthPct) * modifierPct / 100;
    assert(scaled <= std::numeric_limits<int>::max());
    return static_cast<int>(scaled);
}

double spreadAngleRadians(const AttackPattern& pattern, int index, int count)
{
    assert(index >= 0 && index < count);
    if (pattern.kind == AttackPatternKind::Flanks)
    {
        // 側翼不占用零度的既有主彈方向；由近至遠左右交錯，奇數時末枚落在負角側。
        const int pairCount = (count + 1) / 2;
        const int pairIndex = index / 2 + 1;
        const double magnitudeDegrees = pattern.spreadDegrees
            * static_cast<double>(pairIndex) / (pairCount * 2);
        const double offsetDegrees = index % 2 == 0
            ? -magnitudeDegrees
            : magnitudeDegrees;
        return offsetDegrees * BattlePi / 180.0;
    }
    if (pattern.kind != AttackPatternKind::Fan || count == 1)
    {
        return 0.0;
    }
    const double offsetDegrees = -pattern.spreadDegrees / 2.0
        + static_cast<double>(index) * pattern.spreadDegrees / (count - 1);
    return offsetDegrees * BattlePi / 180.0;
}

struct WorkingAttack
{
    BattleAttackSpawnRequest request;
    std::optional<BattleEffectAttackTargetDirective> target;
    std::optional<BattleEffectAttackDamageDirective> damage;
    std::optional<BattleEffectAttackHitLimitDirective> hitLimit;
    std::optional<BattleEffectAttackLifecycleDirective> lifecycle;
    std::vector<BattleEffectDeferredAttack> deferred;
};

void detachDerivedAttack(WorkingAttack& attack)
{
    const auto plan = attack.request.provenance;
    attack.request.provenance = {};
    attack.request.provenance.propagation = plan.propagation;
    attack.request.provenance.origin = plan.origin;
    attack.request.provenance.parentAttackId = plan.parentAttackId;
    attack.request.provenance.mainProjectile = plan.mainProjectile;
    attack.request.provenance.sharedHitGroupId = plan.sharedHitGroupId;
    attack.request.castWork = {};
}

void setDirectiveIndexes(WorkingAttack& attack, std::size_t requestIndex)
{
    if (attack.target)
    {
        attack.target->requestIndex = requestIndex;
    }
    if (attack.damage)
    {
        attack.damage->requestIndex = requestIndex;
    }
    if (attack.hitLimit)
    {
        attack.hitLimit->requestIndex = requestIndex;
    }
    if (attack.lifecycle)
    {
        attack.lifecycle->requestIndex = requestIndex;
    }
    for (auto& deferred : attack.deferred)
    {
        deferred.requestIndex = requestIndex;
    }
}

std::size_t prototypeIndex(const std::vector<WorkingAttack>& attacks)
{
    const auto main = std::ranges::find_if(attacks, [](const WorkingAttack& attack)
    {
        return attack.request.provenance.mainProjectile;
    });
    return main == attacks.end()
        ? 0
        : static_cast<std::size_t>(std::distance(attacks.begin(), main));
}

std::vector<int> selectedTargetIds(
    std::span<const EffectCommand* const> group,
    int maximumCount,
    int excludedUnitId)
{
    assert(maximumCount > 0);
    std::vector<int> result;
    result.reserve(std::min<std::size_t>(group.size(), maximumCount));
    for (const auto* command : group)
    {
        const int unitId = command->metadata.targetUnitId;
        if (unitId < 0
            || unitId == excludedUnitId
            || std::ranges::find(result, unitId) != result.end())
        {
            continue;
        }
        result.push_back(unitId);
        if (result.size() == static_cast<std::size_t>(maximumCount))
        {
            break;
        }
    }
    return result;
}

const EffectCommand& commandForProjectile(
    std::span<const EffectCommand* const> group,
    AttackTargetPolicy targetPolicy,
    std::span<const int> selectedTargets,
    int projectileIndex)
{
    assert(!group.empty());
    if (targetPolicy == AttackTargetPolicy::SelectedTargets)
    {
        assert(projectileIndex >= 0
            && projectileIndex < static_cast<int>(selectedTargets.size()));
        const auto command = std::ranges::find(
            group,
            selectedTargets[projectileIndex],
            [](const EffectCommand* candidate)
            {
                return candidate->metadata.targetUnitId;
            });
        assert(command != group.end());
        return **command;
    }
    return *group.front();
}

int targetIdForProjectile(
    const BattleCastInput& input,
    const BattleAttackSpawnRequest& prototype,
    std::span<const int> selectedTargets,
    AttackTargetPolicy targetPolicy,
    int projectileIndex,
    const EffectCommand& command)
{
    switch (targetPolicy)
    {
    case AttackTargetPolicy::Preserve:
    case AttackTargetPolicy::SamePoint:
        return -1;
    case AttackTargetPolicy::SameTarget:
        if (command.metadata.targetUnitId >= 0)
        {
            return command.metadata.targetUnitId;
        }
        if (prototype.initial.preferredTargetUnitId >= 0)
        {
            return prototype.initial.preferredTargetUnitId;
        }
        return input.targetUnitId;
    case AttackTargetPolicy::SelectedTargets:
        assert(projectileIndex >= 0
            && projectileIndex < static_cast<int>(selectedTargets.size()));
        return selectedTargets[projectileIndex];
    }
    assert(false);
    return -1;
}

void applyTargetPolicy(
    WorkingAttack& attack,
    const BattleCastInput& input,
    const EffectCommand& command,
    const ModifyAttackEffectCommand& action,
    int targetUnitId)
{
    if (action.targets == AttackTargetPolicy::Preserve)
    {
        return;
    }
    if (action.targets == AttackTargetPolicy::SamePoint)
    {
        attack.request.initial.preferredTargetUnitId = -1;
        attack.request.initial.requirePreferredTarget = false;
        attack.target = BattleEffectAttackTargetDirective{
            command.metadata,
            0,
            action.targets,
            -1,
            true,
        };
        return;
    }

    assert(targetUnitId >= 0);
    attack.request.initial.preferredTargetUnitId = targetUnitId;
    attack.request.initial.requirePreferredTarget = true;
    bool velocityResolved = false;
    const auto position = targetPosition(input, targetUnitId);
    const double speed = attack.request.initial.velocity.norm();
    if (position && speed > input.config.minimumFacingNorm)
    {
        const auto direction = *position - attack.request.initial.position;
        if (direction.norm() > input.config.minimumFacingNorm)
        {
            attack.request.initial.velocity = normalizedTo(
                direction,
                speed,
                input.config.minimumFacingNorm);
            velocityResolved = true;
        }
    }
    attack.target = BattleEffectAttackTargetDirective{
        command.metadata,
        0,
        action.targets,
        targetUnitId,
        velocityResolved,
    };
    if (!position)
    {
        attack.deferred.push_back({
            command.metadata,
            std::nullopt,
            BattleEffectDeferredAttackReason::MissingTargetPosition,
            action.pattern,
            action.targets,
            targetUnitId,
        });
    }
    else if (!velocityResolved)
    {
        attack.deferred.push_back({
            command.metadata,
            std::nullopt,
            BattleEffectDeferredAttackReason::MissingProjectileVelocity,
            action.pattern,
            action.targets,
            targetUnitId,
        });
    }
}

void applyPatternGeometry(
    WorkingAttack& attack,
    const EffectCommand& command,
    const ModifyAttackEffectCommand& action,
    int projectileIndex,
    int projectileCount,
    const BattleCastInput& input)
{
    if (action.pattern.kind == AttackPatternKind::Fan
        || action.pattern.kind == AttackPatternKind::Flanks)
    {
        const double speed = attack.request.initial.velocity.norm();
        if (speed <= input.config.minimumFacingNorm)
        {
            attack.deferred.push_back({
                command.metadata,
                std::nullopt,
                BattleEffectDeferredAttackReason::MissingProjectileVelocity,
                action.pattern,
                action.targets,
                attack.request.initial.preferredTargetUnitId,
            });
        }
        else
        {
            attack.request.initial.velocity = rotateBattlePoint(
                attack.request.initial.velocity,
                spreadAngleRadians(action.pattern, projectileIndex, projectileCount));
        }
    }

    const int delayOrdinal = action.pattern.kind == AttackPatternKind::SamePointSequence
        ? projectileIndex + 1
        : projectileIndex;
    attack.request.spawnDelayFrames += delayOrdinal * action.pattern.intervalFrames;
}

void applyAttackSource(WorkingAttack& attack, const ModifyAttackEffectCommand& command)
{
    if (!command.source)
    {
        return;
    }
    assert(command.source->unitId >= 0);
    attack.request.initial.attackSourceUnitId = command.source->unitId;
    attack.request.initial.position = command.source->position;
}

void applyAttackPayload(
    WorkingAttack& attack,
    const EffectCommand& command,
    const ModifyAttackEffectCommand& typed,
    int sharedHitGroupId,
    bool rootAttack,
    BattleAttackOriginKind origin,
    std::optional<BattleAttackId> parentAttackId)
{
    const auto& action = typed;
    attack.request.initial.strengthPct = scaledStrength(
        attack.request.initial.strengthPct,
        action.strengthPct);
    if (action.through)
    {
        attack.request.initial.through = *action.through;
    }
    if (action.tracking)
    {
        attack.request.initial.track = *action.tracking;
    }
    attack.request.initial.projectileClearRadiusPct = std::max(
        attack.request.initial.projectileClearRadiusPct, action.projectileClearRadiusPct);
    attack.request.provenance.propagation = action.propagation;
    attack.request.provenance.origin = origin;
    attack.request.provenance.parentAttackId = parentAttackId;
    attack.request.provenance.rootAttack = rootAttack;
    attack.request.provenance.mainProjectile = action.mainProjectile;
    attack.request.provenance.sharedHitGroupId = sharedHitGroupId;

    if (typed.damageOverride)
    {
        assert(*typed.damageOverride >= 0);
        if (*typed.damageOverride > 0)
        {
            attack.request.initial.scriptedDamage = *typed.damageOverride;
            attack.request.initial.payloadClass =
                BattleProjectilePayloadClass::scriptedDamage();
        }
    }
    if (typed.damageOverride || action.damageKind)
    {
        attack.damage = BattleEffectAttackDamageDirective{
            command.metadata,
            0,
            typed.damageOverride,
            action.damageKind,
            typed.damageOverride && *typed.damageOverride > 0,
        };
    }
    if (action.sameTargetHitLimit > 0)
    {
        attack.hitLimit = BattleEffectAttackHitLimitDirective{
            command.metadata,
            0,
            action.sameTargetHitLimit,
            sharedHitGroupId > 0
                ? std::optional<int>{ sharedHitGroupId }
                : std::nullopt,
        };
        if (action.sameTargetHitLimit > 1)
        {
            attack.deferred.push_back({
                command.metadata,
                std::nullopt,
                BattleEffectDeferredAttackReason::SameTargetHitLimitNeedsRuntimeTracking,
                action.pattern,
                action.targets,
                attack.request.initial.preferredTargetUnitId,
                action.sameTargetHitLimit,
            });
        }
    }
    attack.lifecycle = BattleEffectAttackLifecycleDirective{
        command.metadata,
        0,
        action.propagation,
        origin,
        parentAttackId,
        !attack.request.provenance.valid(),
        rootAttack,
    };
}

void applyCommandGroup(
    const BattleCastInput& input,
    std::vector<WorkingAttack>& attacks,
    std::span<const EffectCommand* const> group,
    BattleEffectAttackApplyState& state,
    std::vector<BattleEffectDeferredAttack>& deferredWithoutRequest)
{
    assert(!group.empty());
    const auto& firstCommand = *group.front();
    const auto& typed = std::get<ModifyAttackEffectCommand>(firstCommand.value);
    const auto& action = typed;
    assert(action.pattern.projectileCount > 0);
    assert(action.pattern.intervalFrames >= 0);
    assert(action.strengthPct >= 0);
    assert(action.sameTargetHitLimit >= 0);
    assert(state.nextSharedHitGroupId > 0);

    if (attacks.empty() && !action.independentProjectile)
    {
        deferredWithoutRequest.push_back({
            firstCommand.metadata,
            std::nullopt,
            BattleEffectDeferredAttackReason::MissingBaseAttack,
            action.pattern,
            action.targets,
            firstCommand.metadata.targetUnitId,
            action.sameTargetHitLimit,
        });
        return;
    }

    if (action.pattern.kind == AttackPatternKind::Preserve
        && !action.addToBaseAttack)
    {
        const int sharedHitGroupId = action.sameTargetHitLimit == 1
            ? state.nextSharedHitGroupId++
            : 0;
        for (std::size_t index = 0; index < attacks.size(); ++index)
        {
            auto& attack = attacks[index];
            applyAttackSource(attack, typed);
            const int targetUnitId = targetIdForProjectile(
                input,
                attack.request,
                {},
                action.targets,
                0,
                firstCommand);
            applyTargetPolicy(attack, input, firstCommand, action, targetUnitId);
            applyAttackPayload(
                attack,
                firstCommand,
                typed,
                sharedHitGroupId,
                attack.request.provenance.rootAttack,
                BattleAttackOriginKind::CastDerived,
                std::nullopt);
        }
        return;
    }

    const auto prototype = action.independentProjectile
        ? WorkingAttack{BattleAttackSpawnRequest{BattleAttackPayload(
            BattleAttackDelivery::projectile(),
            BattleProjectilePayloadClass::combat(),
            BattleAttackReflectionLineageKind::Ordinary)}}
        : attacks[prototypeIndex(attacks)];
    const int excludedTargetUnitId = action.pattern.kind
            == AttackPatternKind::EchoNearestOthers
        ? (prototype.request.initial.preferredTargetUnitId >= 0
            ? prototype.request.initial.preferredTargetUnitId
            : input.targetUnitId)
        : -1;
    const auto selectedTargets = selectedTargetIds(
        group,
        action.pattern.projectileCount,
        excludedTargetUnitId);
    int projectileCount = action.pattern.projectileCount;
    if (action.targets == AttackTargetPolicy::SelectedTargets)
    {
        projectileCount = std::min(
            projectileCount,
            static_cast<int>(selectedTargets.size()));
    }
    if (projectileCount == 0)
    {
        deferredWithoutRequest.push_back({
            firstCommand.metadata,
            std::nullopt,
            BattleEffectDeferredAttackReason::MissingTargetPosition,
            action.pattern,
            action.targets,
            -1,
            action.sameTargetHitLimit,
        });
        return;
    }

    const int sharedHitGroupId = action.sameTargetHitLimit == 1
        ? state.nextSharedHitGroupId++
        : 0;
    const auto parentAttackId = state.sourceAttackProvenance
        ? std::optional<BattleAttackId>{ state.sourceAttackProvenance->attackId }
        : prototype.request.provenance.parentAttackId;
    const auto origin = action.pattern.kind == AttackPatternKind::EchoNearestOthers
        ? BattleAttackOriginKind::Echo
        : BattleAttackOriginKind::CastDerived;

    std::vector<WorkingAttack> generated;
    generated.reserve(projectileCount);
    for (int projectileIndex = 0; projectileIndex < projectileCount; ++projectileIndex)
    {
        auto attack = prototype;
        const bool preservesPrototypeReservation = !action.addToBaseAttack
            && projectileIndex == 0;
        if (!preservesPrototypeReservation)
        {
            detachDerivedAttack(attack);
        }

        const auto& command = commandForProjectile(
            group,
            action.targets,
            selectedTargets,
            projectileIndex);
        const auto& projectileCommand = std::get<ModifyAttackEffectCommand>(command.value);
        if (projectileCommand.independentProjectile)
        {
            const auto& projectile = *projectileCommand.independentProjectile;
            // 獨立 prototype 不繼承觸發招式的近戰、彈射、流血或延遲。
            assert(projectileCommand.source);
            attack.request.initial.potencySnapshot = BattleDamageSystem().snapshotAttackPotency(
                projectileCommand.source->attack, projectile.magicPower);
            attack.request.initial.operationType = BattleOperationType::RangedProjectile;
            attack.request.initial.visualEffectId = projectile.visualEffectId;
            attack.request.initial.totalFrame = projectile.lifetimeFrames;
            attack.request.initial.velocity = {static_cast<float>(projectile.speed), 0.0f, 0.0f};
            attack.request.initial.suppressNearbyTrackingProjectileProc = true;
        }
        applyAttackSource(attack, projectileCommand);
        const int targetUnitId = targetIdForProjectile(
            input,
            prototype.request,
            selectedTargets,
            action.targets,
            projectileIndex,
            command);
        applyTargetPolicy(
            attack,
            input,
            command,
            projectileCommand,
            targetUnitId);
        applyPatternGeometry(
            attack,
            command,
            projectileCommand,
            projectileIndex,
            projectileCount,
            input);
        applyAttackPayload(
            attack,
            command,
            projectileCommand,
            sharedHitGroupId,
            preservesPrototypeReservation
                && prototype.request.provenance.rootAttack,
            origin,
            parentAttackId);
        generated.push_back(std::move(attack));
    }

    if (action.addToBaseAttack)
    {
        attacks.insert(
            attacks.end(),
            std::make_move_iterator(generated.begin()),
            std::make_move_iterator(generated.end()));
    }
    else
    {
        attacks = std::move(generated);
    }
}

BattleEffectAttackApplyResult applyAttackCommandsToRequests(
    const BattleCastInput& input,
    std::vector<BattleAttackSpawnRequest>& requests,
    std::span<const EffectCommand> commands,
    BattleEffectAttackApplyState& state)
{
    assert(input.config.minimumFacingNorm > 0.0);
    assert(state.nextSharedHitGroupId > 0);
    assert(!state.sourceAttackProvenance
        || state.sourceAttackProvenance->valid());

    std::vector<WorkingAttack> attacks;
    attacks.reserve(requests.size());
    for (auto& request : requests)
    {
        attacks.push_back({ std::move(request) });
    }
    requests.clear();

    BattleEffectAttackApplyResult result;
    const auto ordered = orderedCommands<ModifyAttackEffectCommand>(commands);
    for (std::size_t begin = 0; begin < ordered.size();)
    {
        const auto& action = std::get<ModifyAttackEffectCommand>(
            ordered[begin]->value);
        if (!std::holds_alternative<std::monostate>(action.runtimeBehavior))
        {
            ++begin;
            continue;
        }
        std::size_t end = begin + 1;
        while (end < ordered.size()
            && sameActionGroup(*ordered[begin], *ordered[end]))
        {
            ++end;
        }
        applyCommandGroup(
            input,
            attacks,
            std::span(ordered).subspan(begin, end - begin),
            state,
            result.deferred);
        const auto& pattern = action.pattern;
        if (pattern.kind != AttackPatternKind::Preserve)
        {
            result.effectivePattern = pattern;
        }
        begin = end;
    }

    requests.reserve(attacks.size());
    for (std::size_t index = 0; index < attacks.size(); ++index)
    {
        auto& attack = attacks[index];
        setDirectiveIndexes(attack, index);
        requests.push_back(std::move(attack.request));
        if (attack.target)
        {
            result.targets.push_back(std::move(*attack.target));
        }
        if (attack.damage)
        {
            result.damage.push_back(std::move(*attack.damage));
        }
        if (attack.hitLimit)
        {
            result.hitLimits.push_back(std::move(*attack.hitLimit));
        }
        if (attack.lifecycle)
        {
            result.lifecycle.push_back(std::move(*attack.lifecycle));
        }
        result.deferred.insert(
            result.deferred.end(),
            std::make_move_iterator(attack.deferred.begin()),
            std::make_move_iterator(attack.deferred.end()));
    }
    return result;
}

BattleEffectAttackApplyResult applyReplacementPatternToRequests(
    const BattleCastInput& input,
    std::vector<BattleAttackSpawnRequest>& requests,
    const AttackPattern& pattern)
{
    assert(pattern.projectileCount > 0);
    assert(pattern.intervalFrames >= 0);
    assert(!requests.empty());

    std::vector<WorkingAttack> attacks;
    attacks.reserve(requests.size());
    for (auto& request : requests)
    {
        attacks.push_back({ std::move(request) });
    }
    requests.clear();

    const auto prototype = attacks[prototypeIndex(attacks)];
    attacks.clear();
    attacks.reserve(pattern.projectileCount);

    ModifyAttackEffectCommand geometryOnly;
    geometryOnly.pattern = pattern;
    geometryOnly.targets = AttackTargetPolicy::Preserve;
    EffectCommand sourceCommand;
    sourceCommand.value = geometryOnly;
    for (int projectileIndex = 0;
         projectileIndex < pattern.projectileCount;
         ++projectileIndex)
    {
        auto attack = prototype;
        if (projectileIndex > 0)
        {
            detachDerivedAttack(attack);
        }
        applyPatternGeometry(
            attack,
            sourceCommand,
            geometryOnly,
            projectileIndex,
            pattern.projectileCount,
            input);
        attacks.push_back(std::move(attack));
    }

    BattleEffectAttackApplyResult result;
    requests.reserve(attacks.size());
    for (std::size_t index = 0; index < attacks.size(); ++index)
    {
        auto& attack = attacks[index];
        setDirectiveIndexes(attack, index);
        requests.push_back(std::move(attack.request));
        result.deferred.insert(
            result.deferred.end(),
            std::make_move_iterator(attack.deferred.begin()),
            std::make_move_iterator(attack.deferred.end()));
    }
    return result;
}

void appendCastPropagationDirectives(
    BattleEffectAttackApplyResult& result,
    std::span<BattleAttackSpawnRequest> requests,
    CastPropagationPolicy propagation)
{
    for (std::size_t index = 0; index < requests.size(); ++index)
    {
        requests[index].provenance.propagation = propagation;
        const auto existing = std::ranges::find(
            result.lifecycle,
            index,
            &BattleEffectAttackLifecycleDirective::requestIndex);
        if (existing != result.lifecycle.end())
        {
            existing->propagation = propagation;
            continue;
        }
        result.lifecycle.push_back({
            {},
            index,
            propagation,
            BattleAttackOriginKind::CastDerived,
            std::nullopt,
            !requests[index].provenance.valid(),
            requests[index].provenance.rootAttack,
        });
    }
}

}  // namespace

BattleEffectCastPreparation BattleEffectAttackCastSystem::prepareCast(
    BattleCastInput& input,
    std::span<const EffectCommand> commands) const
{
    const bool ultimate = input.ultimateSkill.id >= 0
        && input.unit.mp == input.unit.maxMp;
    return prepareCast(input, ultimate, commands);
}

BattleEffectCastPreparation BattleEffectAttackCastSystem::prepareCast(
    BattleCastInput& input,
    bool ultimate,
    std::span<const EffectCommand> commands) const
{
    assert(!ultimate || input.ultimateSkill.id >= 0);
    BattleEffectCastPreparation result;
    const auto ordered = orderedCommands<ModifyCastEffectCommand>(commands);
    for (const auto* command : ordered)
    {
        const auto& typed = std::get<ModifyCastEffectCommand>(command->value);
        const auto& action = typed;
        if (typed.mpCost)
        {
            assert(*typed.mpCost >= 0);
            result.mpCost = *typed.mpCost;
        }
        if (action.rangeMode)
        {
            result.rangeMode = action.rangeMode;
        }
        if (action.replacementPattern)
        {
            result.replacementPattern = action.replacementPattern;
        }
        result.propagation = action.propagation;
        if (action.freeAdditionalCast)
        {
            result.freeAdditionalCasts.push_back({
                command->metadata,
                command->metadata.targetUnitId,
                action.propagation,
            });
        }
    }

    if (result.rangeMode == CastRangeMode::Ranged)
    {
        auto& skill = selectedSkill(input, ultimate);
        skill.forceRanged = true;
        skill.rangedStyle = true;
    }
    return result;
}

BattleEffectAttackApplyResult BattleEffectAttackCastSystem::applyPreparedCast(
    const BattleCastInput& input,
    BattleCastResult& result,
    const BattleEffectCastPreparation& preparation,
    BattleEffectAttackApplyState& state) const
{
    (void)state;
    if (preparation.mpCost)
    {
        result.mpDelta = -*preparation.mpCost;
    }

    BattleEffectAttackApplyResult applied;
    if (preparation.replacementPattern)
    {
        if (result.attackSpawnRequests.empty())
        {
            applied.deferredReplacementPattern = preparation.replacementPattern;
        }
        else
        {
            applied = applyReplacementPatternToRequests(
                input,
                result.attackSpawnRequests,
                *preparation.replacementPattern);
        }
        applied.effectivePattern = preparation.replacementPattern;
        result.attackPattern = *preparation.replacementPattern;
    }
    if (preparation.propagation)
    {
        appendCastPropagationDirectives(
            applied,
            result.attackSpawnRequests,
            *preparation.propagation);
    }
    return applied;
}

BattleEffectAttackApplyResult BattleEffectAttackCastSystem::applyAttackCommands(
    const BattleCastInput& input,
    BattleCastResult& result,
    std::span<const EffectCommand> commands,
    BattleEffectAttackApplyState& state) const
{
    auto applied = applyAttackCommands(input, result.attackSpawnRequests, commands, state);
    if (applied.effectivePattern)
    {
        result.attackPattern = *applied.effectivePattern;
    }
    return applied;
}

BattleEffectAttackApplyResult BattleEffectAttackCastSystem::applyAttackCommands(
    const BattleCastInput& input,
    std::vector<BattleAttackSpawnRequest>& requests,
    std::span<const EffectCommand> commands,
    BattleEffectAttackApplyState& state) const
{
    return applyAttackCommandsToRequests(input, requests, commands, state);
}

}  // namespace KysChess::Battle
