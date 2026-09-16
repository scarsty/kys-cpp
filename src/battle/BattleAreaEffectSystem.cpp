#include "BattleAreaEffectSystem.h"

#include "BattleMath.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>

namespace KysChess::Battle
{
namespace
{

bool sameSource(const EffectSourceBinding& lhs, const EffectSourceBinding& rhs)
{
    return lhs.kind == rhs.kind
        && lhs.sourceId == rhs.sourceId
        && lhs.ownerUnitId == rhs.ownerUnitId
        && lhs.sourceTeam == rhs.sourceTeam
        && lhs.runtimeInstanceId == rhs.runtimeInstanceId;
}

bool sameMergeKey(const BattleAreaMergeKey& lhs, const BattleAreaMergeKey& rhs)
{
    return sameSource(lhs.source, rhs.source)
        && lhs.ruleId == rhs.ruleId
        && lhs.targetTeamDomain == rhs.targetTeamDomain;
}

void validateGeometry(const BattleAreaGeometry& geometry)
{
    switch (geometry.shape)
    {
    case AreaShape::Circle:
        assert(geometry.radiusTiles > 0);
        assert(geometry.squareSideTiles == 0);
        break;
    case AreaShape::GridSquare:
        assert(geometry.radiusTiles == 0);
        assert(geometry.squareSideTiles > 0);
        assert(geometry.squareSideTiles % 2 == 1);
        break;
    }
}

void validateCreateRequest(const BattleAreaCreateRequest& request)
{
    assert(request.source.ownerUnitId >= 0);
    assert(request.source.sourceTeam >= 0);
    assert(request.currentFrame >= 0);
    assert(request.durationFrames > 0);
    assert(!request.modifiers.empty());
    validateGeometry(request.geometry);
    if (request.anchor.kind == BattleAreaAnchorKind::FollowSourceUnit)
    {
        assert(request.anchor.sourceUnitId == request.source.ownerUnitId);
    }
}

BattleAreaEffect makeArea(BattleAreaEffectState& state, BattleAreaCreateRequest request)
{
    BattleAreaEffect area;
    area.id = { state.nextAreaId++ };
    area.source = request.source;
    area.sourceTeam = request.source.sourceTeam;
    area.geometry = request.geometry;
    area.anchor = request.anchor;
    area.createdFrame = request.currentFrame;
    area.expiresFrameExclusive = request.currentFrame + request.durationFrames;
    area.sourceDeath = request.sourceDeath;
    area.mergeKey = { request.source, request.ruleId, request.targetTeamDomain };
    area.merge = request.merge;
    area.modifiers = std::move(request.modifiers);
    return area;
}

BattleAreaLifecycleEvent lifecycleEvent(
    const BattleAreaEffect& area,
    BattleAreaLifecycleEventType type,
    BattleAreaRemovalReason reason = BattleAreaRemovalReason::Explicit)
{
    return { type, area.id, reason, area.source, area.expiresFrameExclusive };
}

bool modifierMatchesPhase(const AreaModifier& modifier, BattleAreaQueryPhase phase)
{
    if (phase == BattleAreaQueryPhase::Any)
    {
        return true;
    }
    switch (modifier.kind)
    {
    case AreaModifierKind::Attribute:
        return phase == BattleAreaQueryPhase::UnitAttribute;
    case AreaModifierKind::OutgoingDamage:
        return phase == BattleAreaQueryPhase::OutgoingDamage;
    case AreaModifierKind::AttackSpawn:
        return phase == BattleAreaQueryPhase::AttackSpawn;
    case AreaModifierKind::PeriodicDamage:
    case AreaModifierKind::DamageRedirect:
        return false;
    case AreaModifierKind::ForcedMoveImmunity:
        return phase == BattleAreaQueryPhase::ForcedMovement;
    }
    return false;
}

bool areaMatchesPhase(const BattleAreaEffect& area, BattleAreaQueryPhase phase)
{
    return std::ranges::any_of(
        area.modifiers,
        [phase](const AreaModifier& modifier)
        {
            return modifierMatchesPhase(modifier, phase);
        });
}

bool relationMatches(EffectTeamFilter relation, int sourceTeam, int unitTeam)
{
    switch (relation)
    {
    case EffectTeamFilter::Any:
        return true;
    case EffectTeamFilter::Ally:
        return sourceTeam == unitTeam;
    case EffectTeamFilter::Enemy:
        return sourceTeam != unitTeam;
    }
    return false;
}

std::uint64_t modifierKey(const AreaModifier& modifier)
{
    const auto kind = static_cast<std::uint64_t>(modifier.kind);
    switch (modifier.kind)
    {
    case AreaModifierKind::Attribute:
        return kind << 32 | static_cast<std::uint64_t>(modifier.attribute);
    case AreaModifierKind::OutgoingDamage:
        return kind << 32 | static_cast<std::uint64_t>(modifier.damageChannel);
    case AreaModifierKind::AttackSpawn:
    case AreaModifierKind::PeriodicDamage:
    case AreaModifierKind::DamageRedirect:
    case AreaModifierKind::ForcedMoveImmunity:
        return kind << 32;
    }
    return kind << 32;
}

auto attributeStrength(const EffectNumber& number)
{
    return std::tuple{
        std::abs(number.percent),
        std::abs(number.flat),
        number.percent,
        number.flat,
    };
}

int percentStrength(int percent)
{
    return std::abs(percent - 100);
}

bool stronger(const AreaModifier& candidate, const AreaModifier& current)
{
    switch (candidate.kind)
    {
    case AreaModifierKind::Attribute:
        return attributeStrength(candidate.amount) > attributeStrength(current.amount);
    case AreaModifierKind::OutgoingDamage:
        return std::abs(candidate.percent) > std::abs(current.percent);
    case AreaModifierKind::AttackSpawn:
    {
        const int candidateStrength = std::max(
            candidate.speedPct ? percentStrength(*candidate.speedPct) : 0,
            candidate.projectilePressurePct ? percentStrength(*candidate.projectilePressurePct) : 0);
        const int currentStrength = std::max(
            current.speedPct ? percentStrength(*current.speedPct) : 0,
            current.projectilePressurePct ? percentStrength(*current.projectilePressurePct) : 0);
        return candidateStrength > currentStrength;
    }
    case AreaModifierKind::PeriodicDamage:
    case AreaModifierKind::DamageRedirect:
    case AreaModifierKind::ForcedMoveImmunity:
        return false;
    }
    return false;
}

struct PercentAccumulator
{
    int additiveDelta{};
    std::optional<int> strongest;
    bool present{};
};

void mergePercent(PercentAccumulator& accumulator, int candidate, AreaOverlapPolicy overlap)
{
    accumulator.present = true;
    if (overlap == AreaOverlapPolicy::Add)
    {
        accumulator.additiveDelta += candidate - 100;
        return;
    }
    if (!accumulator.strongest
        || percentStrength(candidate) > percentStrength(*accumulator.strongest))
    {
        accumulator.strongest = candidate;
    }
}

std::optional<int> finishPercent(const PercentAccumulator& accumulator)
{
    if (!accumulator.present)
    {
        return std::nullopt;
    }
    return 100 + accumulator.additiveDelta
        + (accumulator.strongest ? *accumulator.strongest - 100 : 0);
}

}  // namespace

std::optional<BattleAreaDamageRedirect> BattleAreaEffectSystem::damageRedirect(
    const BattleAreaEffectState& state,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int unitId,
    int frame)
{
    std::optional<BattleAreaDamageRedirect> result;
    const auto& target = units.requireCore(unitId);
    for (const auto ref : areasContainingUnit(state, gridTransform, units, unitId, frame))
    {
        const auto& area = *ref.area;
        const auto& guardian = units.requireCore(area.source.ownerUnitId);
        if (!guardian.alive || guardian.id == unitId || guardian.team != target.team)
            continue;
        for (const auto& modifier : area.modifiers)
        {
            if (modifier.kind == AreaModifierKind::DamageRedirect
                && (!result || modifier.percent > result->reductionPct))
                result = BattleAreaDamageRedirect{guardian.id, modifier.percent};
        }
    }
    return result;
}

BattleAreaCreateResult BattleAreaEffectSystem::create(
    BattleAreaEffectState& state,
    BattleAreaCreateRequest request)
{
    validateCreateRequest(request);
    const BattleAreaMergeKey mergeKey{ request.source, request.ruleId, request.targetTeamDomain };

    if (request.merge == AreaMergePolicy::RefreshSameSource)
    {
        const auto match = std::ranges::find_if(
            state.areas,
            [&](const BattleAreaEffect& area)
            {
                return area.merge == AreaMergePolicy::RefreshSameSource
                    && sameMergeKey(area.mergeKey, mergeKey);
            });
        if (match != state.areas.end())
        {
            match->source = request.source;
            match->sourceTeam = request.source.sourceTeam;
            match->geometry = request.geometry;
            match->anchor = request.anchor;
            match->expiresFrameExclusive = request.currentFrame + request.durationFrames;
            match->sourceDeath = request.sourceDeath;
            match->modifiers = std::move(request.modifiers);
            return {
                match->id,
                { lifecycleEvent(*match, BattleAreaLifecycleEventType::Refreshed) },
            };
        }
    }

    std::vector<BattleAreaLifecycleEvent> events;
    if (request.merge == AreaMergePolicy::ReplaceSameSource)
    {
        for (auto it = state.areas.begin(); it != state.areas.end();)
        {
            if (it->merge == AreaMergePolicy::ReplaceSameSource
                && sameMergeKey(it->mergeKey, mergeKey))
            {
                events.push_back(lifecycleEvent(*it,
                    BattleAreaLifecycleEventType::Removed,
                    BattleAreaRemovalReason::Replaced));
                it = state.areas.erase(it);
                continue;
            }
            ++it;
        }
    }

    auto area = makeArea(state, std::move(request));
    const auto id = area.id;
    events.push_back(lifecycleEvent(area, BattleAreaLifecycleEventType::Created));
    state.areas.push_back(std::move(area));
    return { id, std::move(events) };
}

std::optional<BattleAreaLifecycleEvent> BattleAreaEffectSystem::remove(
    BattleAreaEffectState& state,
    BattleAreaId areaId,
    BattleAreaRemovalReason reason)
{
    const auto match = std::ranges::find(state.areas, areaId, &BattleAreaEffect::id);
    if (match == state.areas.end())
    {
        return std::nullopt;
    }
    const auto event = lifecycleEvent(*match, BattleAreaLifecycleEventType::Removed, reason);
    state.areas.erase(match);
    return event;
}

std::vector<BattleAreaLifecycleEvent> BattleAreaEffectSystem::removeExpired(
    BattleAreaEffectState& state,
    int currentFrame)
{
    assert(currentFrame >= 0);
    std::vector<BattleAreaLifecycleEvent> events;
    for (auto it = state.areas.begin(); it != state.areas.end();)
    {
        if (it->expiresFrameExclusive <= currentFrame)
        {
            events.push_back(lifecycleEvent(*it,
                BattleAreaLifecycleEventType::Removed,
                BattleAreaRemovalReason::Expired));
            it = state.areas.erase(it);
            continue;
        }
        ++it;
    }
    return events;
}

std::vector<BattleAreaLifecycleEvent> BattleAreaEffectSystem::removeForSourceDeath(
    BattleAreaEffectState& state,
    int sourceUnitId)
{
    assert(sourceUnitId >= 0);
    std::vector<BattleAreaLifecycleEvent> events;
    for (auto it = state.areas.begin(); it != state.areas.end();)
    {
        if (it->source.ownerUnitId == sourceUnitId
            && it->sourceDeath == AreaSourceDeathPolicy::RemoveImmediately)
        {
            events.push_back(lifecycleEvent(*it,
                BattleAreaLifecycleEventType::Removed,
                BattleAreaRemovalReason::SourceDied));
            it = state.areas.erase(it);
            continue;
        }
        ++it;
    }
    return events;
}

bool BattleAreaEffectSystem::activeAt(const BattleAreaEffect& area, int frame)
{
    return area.createdFrame <= frame && frame < area.expiresFrameExclusive;
}

Pointf BattleAreaEffectSystem::center(
    const BattleAreaEffect& area,
    const BattleRuntimeUnits& units)
{
    if (area.anchor.kind == BattleAreaAnchorKind::FixedWorldPosition)
    {
        return area.anchor.fixedPosition;
    }
    assert(area.anchor.sourceUnitId >= 0);
    return units.requireCore(area.anchor.sourceUnitId).motion.position;
}

bool BattleAreaEffectSystem::containsUnit(
    const BattleAreaEffect& area,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int unitId,
    int frame)
{
    assert(gridTransform.tileWidth > 0.0);
    assert(gridTransform.coordCount > 0);
    if (!activeAt(area, frame))
    {
        return false;
    }

    const auto areaCenter = center(area, units);
    const auto unitPosition = units.requireCore(unitId).motion.position;
    switch (area.geometry.shape)
    {
    case AreaShape::Circle:
    {
        const double radius = area.geometry.radiusTiles * gridTransform.tileWidth;
        Pointf radiusPoint = areaCenter;
        radiusPoint.x += static_cast<float>(radius);
        return battleDistanceSquared2d(unitPosition, areaCenter)
            <= battleDistanceSquared2d(areaCenter, radiusPoint);
    }
    case AreaShape::GridSquare:
    {
        const auto centerCell = gridTransform.toGrid(areaCenter);
        const auto unitCell = gridTransform.toGrid(unitPosition);
        const int halfSide = (area.geometry.squareSideTiles - 1) / 2;
        return std::abs(unitCell.x - centerCell.x) <= halfSide
            && std::abs(unitCell.y - centerCell.y) <= halfSide;
    }
    }
    return false;
}

std::vector<BattleAreaRef> BattleAreaEffectSystem::areasContainingUnit(
    const BattleAreaEffectState& state,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int unitId,
    int frame,
    BattleAreaQueryPhase phase)
{
    std::vector<BattleAreaRef> result;
    for (const auto& area : state.areas)
    {
        if (areaMatchesPhase(area, phase)
            && containsUnit(area, gridTransform, units, unitId, frame))
        {
            result.push_back({ area.id, &area });
        }
    }
    std::ranges::sort(result, {}, [](const BattleAreaRef& ref) { return ref.id.value; });
    return result;
}

BattleAreaUnitModifiers BattleAreaEffectSystem::collectAreaUnitModifiers(
    const BattleAreaEffectState& state,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int unitId,
    int frame,
    BattleAreaQueryPhase phase)
{
    assert(phase == BattleAreaQueryPhase::UnitAttribute
        || phase == BattleAreaQueryPhase::OutgoingDamage);
    const auto& unit = units.requireCore(unitId);
    BattleAreaUnitModifiers result;
    std::map<std::uint64_t, std::size_t> selectedByKey;

    for (const auto ref : areasContainingUnit(state, gridTransform, units, unitId, frame, phase))
    {
        assert(ref.area);
        for (const auto& modifier : ref.area->modifiers)
        {
            if (!modifierMatchesPhase(modifier, phase)
                || !relationMatches(modifier.relation, ref.area->sourceTeam, unit.team))
            {
                continue;
            }

            if (modifier.overlap == AreaOverlapPolicy::Add)
            {
                result.modifiers.push_back({ ref.id, modifier });
                continue;
            }

            const auto key = modifierKey(modifier);
            const auto [selected, inserted] = selectedByKey.try_emplace(key, result.modifiers.size());
            if (inserted)
            {
                result.modifiers.push_back({ ref.id, modifier });
            }
            else if (stronger(modifier, result.modifiers[selected->second].modifier))
            {
                result.modifiers[selected->second] = { ref.id, modifier };
            }
        }
    }
    std::ranges::sort(result.modifiers, {}, [](const BattleAreaAppliedModifier& applied) {
        return applied.areaId.value;
    });
    return result;
}

BattleAreaAttackSpawnModifiers BattleAreaEffectSystem::collectAreaAttackSpawnModifiers(
    const BattleAreaEffectState& state,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int sourceUnitId,
    int frame)
{
    const auto& source = units.requireCore(sourceUnitId);
    BattleAreaAttackSpawnModifiers result;
    PercentAccumulator speed;
    PercentAccumulator pressure;

    for (const auto ref : areasContainingUnit(
             state,
             gridTransform,
             units,
             sourceUnitId,
             frame,
             BattleAreaQueryPhase::AttackSpawn))
    {
        assert(ref.area);
        for (const auto& modifier : ref.area->modifiers)
        {
            if (modifier.kind != AreaModifierKind::AttackSpawn
                || !relationMatches(modifier.relation, ref.area->sourceTeam, source.team))
            {
                continue;
            }
            if (modifier.tracking)
            {
                assert(modifier.trackingOverlap == AreaOverlapPolicy::Any);
                if (!result.tracking || !*modifier.tracking)
                {
                    result.tracking = *modifier.tracking;
                }
            }
            if (modifier.speedPct)
            {
                assert(modifier.speedOverlap);
                mergePercent(speed, *modifier.speedPct, *modifier.speedOverlap);
            }
            if (modifier.projectilePressurePct)
            {
                assert(modifier.projectilePressureOverlap);
                mergePercent(
                    pressure,
                    *modifier.projectilePressurePct,
                    *modifier.projectilePressureOverlap);
            }
        }
    }
    result.speedPct = finishPercent(speed);
    result.projectilePressurePct = finishPercent(pressure);
    return result;
}

bool BattleAreaEffectSystem::blocksForcedMovement(
    const BattleAreaEffectState& state,
    const BattleGridTransform& gridTransform,
    const BattleRuntimeUnits& units,
    int targetUnitId,
    int frame,
    ForceMoveDirection direction)
{
    const auto& target = units.requireCore(targetUnitId);
    for (const auto ref : areasContainingUnit(
             state,
             gridTransform,
             units,
             targetUnitId,
             frame,
             BattleAreaQueryPhase::ForcedMovement))
    {
        assert(ref.area);
        if (std::ranges::any_of(
                ref.area->modifiers,
                [&](const AreaModifier& modifier)
                {
                    return modifier.kind == AreaModifierKind::ForcedMoveImmunity
                        && relationMatches(modifier.relation, ref.area->sourceTeam, target.team)
                        && (!modifier.blockedDirection || *modifier.blockedDirection == direction);
                }))
        {
            return true;
        }
    }
    return false;
}

}  // namespace KysChess::Battle
