#include "BattleStatusSystem.h"

#include "../ChessBattleEffects.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <tuple>

namespace KysChess::Battle
{

bool isNegativeBattleStatus(BattleStatusKind kind)
{
    switch (kind)
    {
    case BattleStatusKind::Poison:
    case BattleStatusKind::Bleed:
    case BattleStatusKind::Stun:
    case BattleStatusKind::MpBlocked:
    case BattleStatusKind::ColdPoison:
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
    case BattleStatusKind::Blinded:
        return true;
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::NextAttackCritical:
        return false;
    }
    assert(false);
    return false;
}

bool isControlBattleStatus(BattleStatusKind kind)
{
    return kind == BattleStatusKind::Stun;
}

namespace
{

bool poisonDamageDue(int frame, int intervalFrames)
{
    assert(frame >= 0);
    assert(intervalFrames > 0);
    return frame % intervalFrames == 0;
}

}  // namespace

int projectRemainingPoisonDamage(
    const BattleRemainingPoisonDamageInput& input)
{
    assert(input.firstFutureFrame >= 0);
    assert(input.remainingFrames >= 0);
    assert(input.remainingStacks >= 0);
    assert(input.intervalFrames > 0);
    assert(input.currentHp >= 0);
    assert(input.damagePct >= 0);
    if (input.remainingFrames == 0
        || input.remainingStacks == 0
        || input.currentHp == 0
        || input.damagePct == 0)
    {
        return 0;
    }

    const int phase = input.firstFutureFrame % input.intervalFrames;
    const int firstTickOffset = phase == 0
        ? 0
        : input.intervalFrames - phase;
    if (firstTickOffset >= input.remainingFrames)
    {
        return 0;
    }

    const std::int64_t scheduledTickCount = 1
        + (static_cast<std::int64_t>(input.remainingFrames) - 1 - firstTickOffset)
            / input.intervalFrames;
    const std::int64_t tickCount = std::min<std::int64_t>(
        scheduledTickCount,
        input.remainingStacks);
    const std::int64_t maximumDamage = std::numeric_limits<int>::max();
    std::int64_t projectedHp = input.currentHp;
    std::int64_t totalDamage{};
    for (std::int64_t tickIndex = 0;
         tickIndex < tickCount && projectedHp > 0;
         ++tickIndex)
    {
        const std::int64_t tickDamage = std::max<std::int64_t>(
            1,
            projectedHp * input.damagePct / 100);
        const std::int64_t remainingCapacity = maximumDamage - totalDamage;
        if (tickDamage >= remainingCapacity)
        {
            return std::numeric_limits<int>::max();
        }

        if (tickDamage == 1)
        {
            const std::int64_t remainingTicks = tickCount - tickIndex;
            const std::int64_t finalDamage = std::min(
                remainingTicks,
                projectedHp);
            if (finalDamage >= remainingCapacity)
            {
                return std::numeric_limits<int>::max();
            }
            totalDamage += finalDamage;
            break;
        }

        totalDamage += tickDamage;
        projectedHp = std::max<std::int64_t>(0, projectedHp - tickDamage);
    }
    return static_cast<int>(totalDamage);
}

namespace
{

std::uint64_t allocateStatusSequence(BattleStatusEffectState& effects)
{
    assert(effects.nextStatusSequence > 0);
    assert(effects.nextStatusSequence < std::numeric_limits<std::uint64_t>::max());
    return effects.nextStatusSequence++;
}

bool sameStatus(const BattleTypedStatusInstance& status, BattleStatusKind kind)
{
    return status.kind == kind;
}

auto typedStatusStrength(const BattleTypedStatusInstance& status)
{
    return std::tuple(status.potency, status.secondaryPotency, status.stacks);
}

auto requestStatusStrength(const BattleStatusApplyRequest& request)
{
    return std::tuple(request.potency, request.secondaryPotency, request.stacks);
}

int remainingDurationSortValue(int remainingFrames)
{
    return remainingFrames > 0 ? remainingFrames : std::numeric_limits<int>::max();
}

bool containsStatus(const std::vector<BattleStatusKind>& statuses, BattleStatusKind kind)
{
    return std::find(statuses.begin(), statuses.end(), kind) != statuses.end();
}

bool matchesRemovalFilter(const BattleStatusRemoveRequest& request, BattleStatusKind kind)
{
    if (request.statuses.empty() && !request.negativeOnly && !request.controlOnly)
    {
        return false;
    }
    if (!request.statuses.empty() && !containsStatus(request.statuses, kind))
    {
        return false;
    }
    if (request.negativeOnly && !isNegativeBattleStatus(kind))
    {
        return false;
    }
    if (request.controlOnly && !isControlBattleStatus(kind))
    {
        return false;
    }
    return true;
}

void appendStatusView(
    std::vector<BattleTypedStatusInstance>& statuses,
    BattleStatusKind kind,
    int sourceUnitId,
    int remainingFrames,
    int stacks,
    int potency,
    int secondaryPotency,
    std::uint64_t sequence)
{
    BattleTypedStatusInstance status;
    status.kind = kind;
    status.sourceUnitId = sourceUnitId;
    status.remainingFrames = remainingFrames;
    status.stacks = stacks;
    status.potency = potency;
    status.secondaryPotency = secondaryPotency;
    status.appliedSequence = sequence;
    statuses.push_back(status);
}

bool absorbStatusShield(
    BattleStatusApplyResult& result,
    const BattleStatusApplyRequest& request,
    int& durationFrames)
{
    if (request.bypassStatusShield
        || !isNegativeBattleStatus(request.kind)
        || result.target.effects.statusShield <= 0)
    {
        return false;
    }

    const auto protection = BattleStatusSystem({}).protectNegativeEffect(
        std::move(result.target),
        durationFrames);
    result.target = protection.target;
    result.statusShieldAbsorbed = protection.statusShieldAbsorbed;
    durationFrames = protection.remainingDurationFrames;
    return protection.blocked;
}

bool absorbControlProtection(
    BattleStatusApplyResult& result,
    const BattleStatusApplyRequest& request,
    int& durationFrames)
{
    assert(durationFrames > 0);
    if (request.controlLowHpImmunityPct > 0
        && result.target.maxHp > 0
        && result.target.hp * 100 < result.target.maxHp * request.controlLowHpImmunityPct)
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByControlImmunity;
        return true;
    }

    int reductionPct = result.target.effects.freezeReductionPct;
    if (request.targetHasShield)
    {
        reductionPct += result.target.effects.shieldFreezeResPct;
    }
    reductionPct = std::clamp(reductionPct, 0, 100);
    durationFrames = durationFrames * (100 - reductionPct) / 100;

    const int immunityAbsorbed = std::min(durationFrames, result.target.effects.controlImmunityFrames);
    result.target.effects.controlImmunityFrames -= immunityAbsorbed;
    result.controlImmunityAbsorbed = immunityAbsorbed;
    durationFrames -= immunityAbsorbed;
    if (durationFrames <= 0)
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByControlImmunity;
        return true;
    }

    if (absorbStatusShield(result, request, durationFrames))
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByStatusShield;
        return true;
    }

    const int staggerAbsorbed = std::min(durationFrames, result.target.effects.staggerShield);
    result.target.effects.staggerShield -= staggerAbsorbed;
    result.staggerShieldAbsorbed = staggerAbsorbed;
    durationFrames -= staggerAbsorbed;
    if (durationFrames <= 0)
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByStaggerShield;
        return true;
    }
    return false;
}

struct RuntimeStatusTickTarget
{
    BattleRuntimeUnit& unit;
    BattleStatusRuntimeUnit& status;

    int id() const { return unit.id; }
    bool alive() const { return unit.alive; }
    int hp() const { return unit.vitals.hp; }
    int maxHp() const { return unit.vitals.maxHp; }
    int& attack() { return unit.stats.attack; }
    int& invincible() { return unit.invincible; }
};

void clearPoison(BattleStatusEffectState& effects)
{
    effects.poisonTimer = 0;
    effects.poisonStacks = 0;
    effects.poisonTickPct = 0;
    effects.poisonSourceId = -1;
    effects.poisonAppliedSequence = 0;
}

void tickPoison(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    if (effects.poisonStacks <= 0)
    {
        assert(effects.poisonTimer <= 0);
        return;
    }
    assert(effects.poisonTimer > 0);

    --effects.poisonTimer;
    if (config.poisonDamageIntervalFrames > 0
        && poisonDamageDue(config.frame, config.poisonDamageIntervalFrames))
    {
        int damage = std::max(1, target.hp() * effects.poisonTickPct / 100);
        result.events.push_back({
            BattleStatusEventType::PoisonDamage,
            target.id(),
            effects.poisonSourceId,
            damage,
            "中毒",
        });
        --effects.poisonStacks;
    }

    if (effects.poisonTimer <= 0 || effects.poisonStacks <= 0)
    {
        clearPoison(effects);
    }
}

void tickBleed(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    if (effects.bleedStacks <= 0)
    {
        effects.bleedTimer = 0;
        effects.bleedSourceId = -1;
        return;
    }

    if (effects.bleedTimer > 0)
    {
        --effects.bleedTimer;
    }

    if (effects.bleedTimer <= 0)
    {
        int damage = std::max(1, target.maxHp() * effects.bleedStacks / 100);
        result.events.push_back({
            BattleStatusEventType::BleedDamage,
            target.id(),
            effects.bleedSourceId,
            damage,
            "流血",
        });
        effects.bleedTimer = config.bleedDamageIntervalFrames;
    }
}

void tickSimpleTimers(RuntimeStatusTickTarget& target)
{
    auto& effects = target.status.effects;
    if (target.invincible() > 0)
    {
        --target.invincible();
    }
    if (effects.mpBlockTimer > 0)
    {
        --effects.mpBlockTimer;
        if (effects.mpBlockTimer <= 0)
        {
            effects.mpBlockSourceId = -1;
            effects.mpBlockAppliedSequence = 0;
        }
    }

}

void tickTypedStatuses(RuntimeStatusTickTarget& target, BattleStatusTickResult& result)
{
    auto& statuses = target.status.effects.typedStatuses;
    for (auto it = statuses.begin(); it != statuses.end();)
    {
        if (it->remainingFrames <= 0)
        {
            ++it;
            continue;
        }

        --it->remainingFrames;
        if (it->remainingFrames > 0)
        {
            ++it;
            continue;
        }

        result.events.push_back({
            BattleStatusEventType::StatusExpired,
            target.id(),
            it->sourceUnitId,
            it->stacks,
            "狀態到期",
            it->kind,
        });
        it = statuses.erase(it);
    }
}

BattleStatusTickResult tickStatusTarget(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget target)
{
    BattleStatusTickResult result;
    if (!target.alive())
    {
        return result;
    }

    tickPoison(config, target, result);
    tickBleed(config, target, result);
    tickSimpleTimers(target);
    tickTypedStatuses(target, result);
    return result;
}

}  // namespace

BattleStatusSystem::BattleStatusSystem(BattleStatusSystemConfig config)
    : config_(config)
{
}

BattleStatusTickResult BattleStatusSystem::tick(BattleRuntimeUnitRecord& record) const
{
    return tickStatusTarget(config_, RuntimeStatusTickTarget{ record.core, record.status });
}

BattleStatusTickResult BattleStatusSystem::tick(BattleRuntimeUnits& records) const
{
    BattleStatusTickResult result;
    for (auto& record : records.all())
    {
        auto unitResult = tick(record);
        result.events.insert(result.events.end(), unitResult.events.begin(), unitResult.events.end());
    }
    return result;
}

BattleStatusApplyResult BattleStatusSystem::apply(
    BattleStatusUnitState target,
    const BattleStatusApplyRequest& request) const
{
    assert(target.id >= 0);
    assert(target.hp >= 0);
    assert(target.maxHp >= 0);
    assert(target.hp <= target.maxHp);
    assert(request.durationFrames >= 0);
    assert(request.stacks > 0);
    if (request.stackLimit)
    {
        assert(*request.stackLimit > 0);
    }
    if (request.stack == EffectStackPolicy::AddStack)
    {
        assert(request.stackLimit);
    }

    BattleStatusApplyResult result;
    result.target = std::move(target);
    result.requestedDurationFrames = request.durationFrames;
    if (!result.target.alive)
    {
        result.outcome = BattleStatusApplyOutcome::TargetDead;
        return result;
    }

    int durationFrames = request.durationFrames;
    if (isControlBattleStatus(request.kind))
    {
        assert(durationFrames > 0);
        if (absorbControlProtection(result, request, durationFrames))
        {
            result.appliedDurationFrames = 0;
            return result;
        }
    }
    else if (absorbStatusShield(result, request, durationFrames))
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByStatusShield;
        result.appliedDurationFrames = 0;
        return result;
    }
    result.appliedDurationFrames = durationFrames;

    auto& effects = result.target.effects;
    switch (request.kind)
    {
    case BattleStatusKind::Poison:
    {
        assert(durationFrames > 0);
        assert(request.potency > 0);
        assert((effects.poisonTimer > 0) == (effects.poisonStacks > 0));
        const bool active = effects.poisonStacks > 0;
        const int requestedStacks = request.stackLimit
            ? std::min(request.stacks, *request.stackLimit)
            : request.stacks;

        if (active
            && (request.stack == EffectStackPolicy::KeepStrongest
                || request.stack == EffectStackPolicy::Independent)
            && request.potency <= effects.poisonTickPct)
        {
            result.outcome = BattleStatusApplyOutcome::KeptStronger;
            result.value = effects.poisonTickPct;
            return result;
        }

        if (request.stack == EffectStackPolicy::AddStack)
        {
            const int before = effects.poisonStacks;
            effects.poisonStacks = std::clamp(
                before + request.stacks,
                0,
                *request.stackLimit);
            if (!active)
            {
                effects.poisonAppliedSequence = allocateStatusSequence(effects);
            }
            effects.poisonTimer = durationFrames;
            effects.poisonTickPct = request.potency;
            effects.poisonSourceId = request.sourceUnitId;
            result.applied = effects.poisonStacks != before;
            result.value = effects.poisonStacks;
            result.outcome = BattleStatusApplyOutcome::StackChanged;
            return result;
        }

        if (!active || request.stack == EffectStackPolicy::Replace)
        {
            effects.poisonAppliedSequence = allocateStatusSequence(effects);
        }
        effects.poisonStacks = requestedStacks;
        effects.poisonTimer = durationFrames;
        effects.poisonTickPct = request.potency;
        effects.poisonSourceId = request.sourceUnitId;
        result.applied = true;
        result.value = request.potency;
        result.outcome = active
            ? (request.stack == EffectStackPolicy::Replace
                ? BattleStatusApplyOutcome::Replaced
                : BattleStatusApplyOutcome::Refreshed)
            : BattleStatusApplyOutcome::Applied;
        return result;
    }
    case BattleStatusKind::Bleed:
    {
        const int before = effects.bleedStacks;
        int after = request.stacks;
        if (request.stack == EffectStackPolicy::AddStack)
        {
            after = std::clamp(before + request.stacks, 0, *request.stackLimit);
        }
        else if (request.stack == EffectStackPolicy::KeepStrongest)
        {
            after = std::max(before, request.stacks);
        }
        else if (request.stack == EffectStackPolicy::Refresh)
        {
            after = std::max(before, request.stacks);
        }
        if (request.stackLimit)
        {
            after = std::min(after, *request.stackLimit);
        }

        effects.bleedStacks = std::max(0, after);
        if (effects.bleedStacks <= 0)
        {
            effects.bleedTimer = 0;
            effects.bleedSourceId = -1;
            effects.bleedAppliedSequence = 0;
        }
        else
        {
            if (before <= 0 || request.stack == EffectStackPolicy::Replace)
            {
                effects.bleedAppliedSequence = allocateStatusSequence(effects);
            }
            if (effects.bleedTimer <= 0)
            {
                effects.bleedTimer = config_.bleedDamageIntervalFrames;
            }
            effects.bleedSourceId = request.sourceUnitId;
        }
        result.applied = after != before;
        result.value = effects.bleedStacks;
        result.outcome = request.stack == EffectStackPolicy::AddStack
            ? BattleStatusApplyOutcome::StackChanged
            : (before > 0 ? BattleStatusApplyOutcome::Replaced : BattleStatusApplyOutcome::Applied);
        return result;
    }
    case BattleStatusKind::Stun:
    {
        const int before = effects.frozenTimer;
        switch (request.stack)
        {
        case EffectStackPolicy::Independent:
            effects.frozenTimer += durationFrames;
            break;
        case EffectStackPolicy::Refresh:
        case EffectStackPolicy::KeepStrongest:
        case EffectStackPolicy::AddStack:
            effects.frozenTimer = std::max(effects.frozenTimer, durationFrames);
            break;
        case EffectStackPolicy::Replace:
            effects.frozenTimer = durationFrames;
            break;
        }
        effects.frozenMaxTimer = std::max(effects.frozenMaxTimer, effects.frozenTimer);
        effects.frozenSourceId = request.sourceUnitId;
        if (before <= 0 || request.stack == EffectStackPolicy::Replace)
        {
            effects.frozenAppliedSequence = allocateStatusSequence(effects);
        }

        result.applied = effects.frozenTimer > before;
        result.value = effects.frozenTimer - before;
        result.outcome = request.stack == EffectStackPolicy::AddStack
            ? BattleStatusApplyOutcome::StackChanged
            : (before > 0 ? BattleStatusApplyOutcome::Refreshed : BattleStatusApplyOutcome::Applied);
        return result;
    }
    case BattleStatusKind::MpBlocked:
    {
        assert(durationFrames > 0);
        const int before = effects.mpBlockTimer;
        if (request.stack == EffectStackPolicy::Replace)
        {
            effects.mpBlockTimer = durationFrames;
        }
        else
        {
            effects.mpBlockTimer = std::max(effects.mpBlockTimer, durationFrames);
        }
        if (before <= 0 || request.stack == EffectStackPolicy::Replace)
        {
            effects.mpBlockAppliedSequence = allocateStatusSequence(effects);
        }
        effects.mpBlockSourceId = request.sourceUnitId;
        result.applied = effects.mpBlockTimer > before;
        result.value = effects.mpBlockTimer - before;
        result.outcome = before > 0
            ? BattleStatusApplyOutcome::Refreshed
            : BattleStatusApplyOutcome::Applied;
        return result;
    }
    case BattleStatusKind::ColdPoison:
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
    case BattleStatusKind::Blinded:
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::NextAttackCritical:
        break;
    }

    auto first = std::find_if(effects.typedStatuses.begin(), effects.typedStatuses.end(), [&](const auto& status)
    {
        return sameStatus(status, request.kind);
    });
    auto append = [&]() -> BattleTypedStatusInstance&
    {
        BattleTypedStatusInstance status;
        status.kind = request.kind;
        status.sourceUnitId = request.sourceUnitId;
        status.remainingFrames = durationFrames;
        status.stacks = request.stackLimit
            ? std::clamp(request.stacks, 0, *request.stackLimit)
            : request.stacks;
        status.potency = request.potency;
        status.secondaryPotency = request.secondaryPotency;
        status.appliedSequence = allocateStatusSequence(effects);
        effects.typedStatuses.push_back(status);
        return effects.typedStatuses.back();
    };

    switch (request.stack)
    {
    case EffectStackPolicy::Independent:
    {
        auto& status = append();
        result.applied = true;
        result.value = status.stacks;
        result.outcome = BattleStatusApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Replace:
    {
        const bool replaced = first != effects.typedStatuses.end();
        std::erase_if(effects.typedStatuses, [&](const auto& status)
        {
            return sameStatus(status, request.kind);
        });
        auto& status = append();
        result.applied = true;
        result.value = status.stacks;
        result.outcome = replaced
            ? BattleStatusApplyOutcome::Replaced
            : BattleStatusApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Refresh:
    {
        if (first == effects.typedStatuses.end())
        {
            auto& status = append();
            result.value = status.stacks;
            result.outcome = BattleStatusApplyOutcome::Applied;
        }
        else
        {
            first->sourceUnitId = request.sourceUnitId;
            first->remainingFrames = durationFrames;
            first->stacks = request.stacks;
            first->potency = request.potency;
            first->secondaryPotency = request.secondaryPotency;
            result.value = first->stacks;
            result.outcome = BattleStatusApplyOutcome::Refreshed;
        }
        result.applied = true;
        break;
    }
    case EffectStackPolicy::KeepStrongest:
    {
        if (first == effects.typedStatuses.end())
        {
            auto& status = append();
            result.applied = true;
            result.value = status.stacks;
            result.outcome = BattleStatusApplyOutcome::Applied;
        }
        else if (requestStatusStrength(request) > typedStatusStrength(*first))
        {
            first->sourceUnitId = request.sourceUnitId;
            first->remainingFrames = durationFrames;
            first->stacks = request.stacks;
            first->potency = request.potency;
            first->secondaryPotency = request.secondaryPotency;
            result.applied = true;
            result.value = first->stacks;
            result.outcome = BattleStatusApplyOutcome::Replaced;
        }
        else if (requestStatusStrength(request) == typedStatusStrength(*first)
                 && durationFrames > first->remainingFrames)
        {
            first->remainingFrames = durationFrames;
            result.applied = true;
            result.value = first->stacks;
            result.outcome = BattleStatusApplyOutcome::Refreshed;
        }
        else
        {
            result.value = first->stacks;
            result.outcome = BattleStatusApplyOutcome::KeptStronger;
        }
        break;
    }
    case EffectStackPolicy::AddStack:
    {
        if (first == effects.typedStatuses.end())
        {
            if (request.stacks > 0)
            {
                auto& status = append();
                result.applied = status.stacks > 0;
                result.value = status.stacks;
            }
        }
        else
        {
            const int before = first->stacks;
            first->stacks = std::clamp(first->stacks + request.stacks, 0, *request.stackLimit);
            if (request.stacks > 0)
            {
                first->sourceUnitId = request.sourceUnitId;
                first->potency = request.potency;
                first->secondaryPotency = request.secondaryPotency;
                if (durationFrames > 0)
                {
                    first->remainingFrames = durationFrames;
                }
            }
            result.applied = first->stacks != before;
            result.value = first->stacks;
            if (first->stacks <= 0)
            {
                effects.typedStatuses.erase(first);
            }
        }
        result.outcome = BattleStatusApplyOutcome::StackChanged;
        break;
    }
    }
    return result;
}

BattleStatusRemoveResult BattleStatusSystem::remove(
    BattleStatusUnitState target,
    const BattleStatusRemoveRequest& request) const
{
    assert(target.id >= 0);
    assert(request.count >= 0);

    enum class CandidateStorage
    {
        Poison,
        Bleed,
        Stun,
        MpBlocked,
        Typed,
    };
    struct Candidate
    {
        CandidateStorage storage{};
        std::size_t index{};
        BattleStatusKind kind{};
        int remainingFrames{};
        std::uint64_t sequence{};
    };

    BattleStatusRemoveResult result;
    result.target = std::move(target);
    auto& effects = result.target.effects;
    std::vector<Candidate> candidates;
    auto appendSpecialized = [&](CandidateStorage storage,
                                 BattleStatusKind kind,
                                 int remainingFrames,
                                 std::uint64_t sequence)
    {
        if (matchesRemovalFilter(request, kind))
        {
            candidates.push_back({ storage, 0, kind, remainingFrames, sequence });
        }
    };

    if (effects.poisonTimer > 0)
    {
        appendSpecialized(CandidateStorage::Poison,
                          BattleStatusKind::Poison,
                          effects.poisonTimer,
                          effects.poisonAppliedSequence);
    }
    if (effects.bleedStacks > 0)
    {
        appendSpecialized(CandidateStorage::Bleed,
                          BattleStatusKind::Bleed,
                          0,
                          effects.bleedAppliedSequence);
    }
    if (effects.frozenTimer > 0)
    {
        appendSpecialized(CandidateStorage::Stun,
                          BattleStatusKind::Stun,
                          effects.frozenTimer,
                          effects.frozenAppliedSequence);
    }
    if (effects.mpBlockTimer > 0)
    {
        appendSpecialized(CandidateStorage::MpBlocked,
                          BattleStatusKind::MpBlocked,
                          effects.mpBlockTimer,
                          effects.mpBlockAppliedSequence);
    }
    for (std::size_t i = 0; i < effects.typedStatuses.size(); ++i)
    {
        const auto& status = effects.typedStatuses[i];
        if (matchesRemovalFilter(request, status.kind))
        {
            candidates.push_back({
                CandidateStorage::Typed,
                i,
                status.kind,
                status.remainingFrames,
                status.appliedSequence,
            });
        }
    }

    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& lhs, const Candidate& rhs)
    {
        switch (request.order)
        {
        case StatusRemovalOrder::LongestRemaining:
        {
            const int lhsDuration = remainingDurationSortValue(lhs.remainingFrames);
            const int rhsDuration = remainingDurationSortValue(rhs.remainingFrames);
            if (lhsDuration != rhsDuration)
            {
                return lhsDuration > rhsDuration;
            }
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence < rhs.sequence;
            }
            break;
        }
        case StatusRemovalOrder::Oldest:
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence < rhs.sequence;
            }
            break;
        case StatusRemovalOrder::Newest:
            if (lhs.sequence != rhs.sequence)
            {
                return lhs.sequence > rhs.sequence;
            }
            break;
        }
        return std::tuple(lhs.storage, lhs.kind, lhs.index)
            < std::tuple(rhs.storage, rhs.kind, rhs.index);
    });

    const std::size_t removeCount = request.count > 0
        ? std::min<std::size_t>(request.count, candidates.size())
        : candidates.size();
    std::vector<bool> removeTyped(effects.typedStatuses.size());
    bool removePoison = false;
    bool removeBleed = false;
    bool removeStun = false;
    bool removeMpBlock = false;
    for (std::size_t i = 0; i < removeCount; ++i)
    {
        const auto& candidate = candidates[i];
        switch (candidate.storage)
        {
        case CandidateStorage::Poison:
            removePoison = true;
            break;
        case CandidateStorage::Bleed:
            removeBleed = true;
            break;
        case CandidateStorage::Stun:
            removeStun = true;
            break;
        case CandidateStorage::MpBlocked:
            removeMpBlock = true;
            break;
        case CandidateStorage::Typed:
            removeTyped[candidate.index] = true;
            break;
        }
        result.removedStatuses.push_back(candidate.kind);
        ++result.removedCount;
    }

    if (removePoison)
    {
        clearPoison(effects);
    }
    if (removeBleed)
    {
        effects.bleedStacks = 0;
        effects.bleedTimer = 0;
        effects.bleedSourceId = -1;
        effects.bleedAppliedSequence = 0;
    }
    if (removeStun || request.clearCurrentActionStagger)
    {
        result.currentActionStaggerCleared = effects.frozenTimer > 0;
        effects.clearStunAndHitstun();
    }
    if (removeMpBlock)
    {
        effects.mpBlockTimer = 0;
        effects.mpBlockSourceId = -1;
        effects.mpBlockAppliedSequence = 0;
    }

    for (std::size_t i = effects.typedStatuses.size(); i > 0; --i)
    {
        const std::size_t index = i - 1;
        if (index < removeTyped.size() && removeTyped[index])
        {
            effects.typedStatuses.erase(effects.typedStatuses.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    return result;
}

BattleStatusConsumeResult BattleStatusSystem::consume(
    BattleStatusUnitState target,
    const BattleStatusConsumeRequest& request) const
{
    assert(target.id >= 0);
    assert(request.stacks > 0);

    BattleStatusConsumeResult result;
    result.target = std::move(target);
    auto& effects = result.target.effects;
    const auto beforeSnapshot = snapshot(result.target);
    const auto before = std::find_if(beforeSnapshot.statuses.begin(), beforeSnapshot.statuses.end(), [&](const auto& status)
    {
        return status.kind == request.kind
            && (!request.sourceUnitId || status.sourceUnitId == *request.sourceUnitId);
    });
    if (before == beforeSnapshot.statuses.end())
    {
        return result;
    }
    result.consumedStatus = *before;

    switch (request.kind)
    {
    case BattleStatusKind::Poison:
        if (request.sourceUnitId && effects.poisonSourceId != *request.sourceUnitId)
        {
            return result;
        }
        result.consumedStatus.stacks = std::min(
            request.stacks,
            effects.poisonStacks);
        effects.poisonStacks -= result.consumedStatus.stacks;
        result.consumed = result.consumedStatus.stacks > 0;
        if (effects.poisonStacks <= 0)
        {
            clearPoison(effects);
        }
        break;
    case BattleStatusKind::Bleed:
    {
        if (request.sourceUnitId && effects.bleedSourceId != *request.sourceUnitId)
        {
            return result;
        }
        const int consumed = std::min(request.stacks, effects.bleedStacks);
        effects.bleedStacks -= consumed;
        result.consumedStatus.stacks = consumed;
        result.consumed = consumed > 0;
        if (effects.bleedStacks <= 0)
        {
            effects.bleedTimer = 0;
            effects.bleedSourceId = -1;
            effects.bleedAppliedSequence = 0;
        }
        break;
    }
    case BattleStatusKind::Stun:
        if (request.sourceUnitId && effects.frozenSourceId != *request.sourceUnitId)
        {
            return result;
        }
        effects.clearStunAndHitstun();
        result.consumedStatus.stacks = 1;
        result.consumed = true;
        break;
    case BattleStatusKind::MpBlocked:
        if (request.sourceUnitId && effects.mpBlockSourceId != *request.sourceUnitId)
        {
            return result;
        }
        effects.mpBlockTimer = 0;
        effects.mpBlockSourceId = -1;
        effects.mpBlockAppliedSequence = 0;
        result.consumedStatus.stacks = 1;
        result.consumed = true;
        break;
    case BattleStatusKind::ColdPoison:
    case BattleStatusKind::WitheredBone:
    case BattleStatusKind::SevenStarMark:
    case BattleStatusKind::NeutralizeForce:
    case BattleStatusKind::Blinded:
    case BattleStatusKind::NextAttackMiss:
    case BattleStatusKind::DamageBlockLayer:
    case BattleStatusKind::SingleHitCapLayer:
    case BattleStatusKind::BattleSpirit:
    case BattleStatusKind::TrueQi:
    case BattleStatusKind::PoisonExplosion:
    case BattleStatusKind::Shadowless:
    case BattleStatusKind::NextAttackCritical:
    {
        int remainingToConsume = request.stacks;
        int consumed = 0;
        while (remainingToConsume > 0)
        {
            auto selected = effects.typedStatuses.end();
            for (auto it = effects.typedStatuses.begin(); it != effects.typedStatuses.end(); ++it)
            {
                if (it->kind != request.kind
                    || (request.sourceUnitId && it->sourceUnitId != *request.sourceUnitId))
                {
                    continue;
                }
                if (selected == effects.typedStatuses.end()
                    || it->appliedSequence < selected->appliedSequence)
                {
                    selected = it;
                }
            }
            if (selected == effects.typedStatuses.end())
            {
                break;
            }
            const int amount = std::min(remainingToConsume, selected->stacks);
            selected->stacks -= amount;
            remainingToConsume -= amount;
            consumed += amount;
            if (selected->stacks <= 0)
            {
                effects.typedStatuses.erase(selected);
            }
        }
        result.consumedStatus.stacks = consumed;
        result.consumed = consumed > 0;
        break;
    }
    }

    for (const auto& status : snapshot(result.target).statuses)
    {
        if (status.kind == request.kind
            && (!request.sourceUnitId || status.sourceUnitId == *request.sourceUnitId))
        {
            result.remainingStacks += status.stacks;
        }
    }
    return result;
}

BattleStatusProtectionResult BattleStatusSystem::changeProtection(
    BattleStatusUnitState target,
    BattleResource resource,
    int delta) const
{
    assert(target.id >= 0);
    assert(resource == BattleResource::StatusShield || resource == BattleResource::StaggerShield);

    BattleStatusProtectionResult result;
    result.target = std::move(target);
    result.resource = resource;
    int& protection = resource == BattleResource::StatusShield
        ? result.target.effects.statusShield
        : result.target.effects.staggerShield;
    result.before = protection;
    const long long changed = static_cast<long long>(protection) + delta;
    assert(changed <= std::numeric_limits<int>::max());
    protection = static_cast<int>(std::max(0LL, changed));
    result.after = protection;
    return result;
}

BattleNegativeEffectProtectionResult BattleStatusSystem::protectNegativeEffect(
    BattleStatusUnitState target,
    int durationFrames) const
{
    assert(target.id >= 0);
    assert(durationFrames >= 0);

    BattleNegativeEffectProtectionResult result;
    result.target = std::move(target);
    result.requestedDurationFrames = durationFrames;
    result.remainingDurationFrames = durationFrames;

    const int cost = durationFrames > 0
        ? durationFrames
        : DurationlessNegativeStatusShieldCost;
    result.statusShieldAbsorbed = std::min(
        cost,
        result.target.effects.statusShield);
    result.target.effects.statusShield -= result.statusShieldAbsorbed;
    if (durationFrames > 0)
    {
        result.remainingDurationFrames -= result.statusShieldAbsorbed;
        result.blocked = result.remainingDurationFrames == 0;
    }
    else
    {
        result.blocked = result.statusShieldAbsorbed
            == DurationlessNegativeStatusShieldCost;
    }
    return result;
}

BattleStatusQuerySnapshot BattleStatusSystem::snapshot(const BattleStatusUnitState& target) const
{
    BattleStatusQuerySnapshot result;
    const auto& effects = target.effects;
    result.statusShield = effects.statusShield;
    result.staggerShield = effects.staggerShield;

    if (effects.poisonStacks > 0)
    {
        assert(effects.poisonTimer > 0);
        appendStatusView(result.statuses,
                         BattleStatusKind::Poison,
                         effects.poisonSourceId,
                         effects.poisonTimer,
                         effects.poisonStacks,
                         effects.poisonTickPct,
                         0,
                         effects.poisonAppliedSequence);
    }
    if (effects.bleedStacks > 0)
    {
        appendStatusView(result.statuses,
                         BattleStatusKind::Bleed,
                         effects.bleedSourceId,
                         0,
                         effects.bleedStacks,
                         0,
                         0,
                         effects.bleedAppliedSequence);
    }
    if (effects.frozenTimer > 0)
    {
        appendStatusView(result.statuses,
                         BattleStatusKind::Stun,
                         effects.frozenSourceId,
                         effects.frozenTimer,
                         1,
                         0,
                         0,
                         effects.frozenAppliedSequence);
    }
    if (effects.mpBlockTimer > 0)
    {
        appendStatusView(result.statuses,
                         BattleStatusKind::MpBlocked,
                         effects.mpBlockSourceId,
                         effects.mpBlockTimer,
                         1,
                         0,
                         0,
                         effects.mpBlockAppliedSequence);
    }
    for (const auto& status : effects.typedStatuses)
    {
        result.statuses.push_back(status);
    }
    std::sort(result.statuses.begin(), result.statuses.end(), [](const auto& lhs, const auto& rhs)
    {
        return std::tuple(lhs.appliedSequence, lhs.kind, lhs.sourceUnitId)
            < std::tuple(rhs.appliedSequence, rhs.kind, rhs.sourceUnitId);
    });

    for (const auto& status : result.statuses)
    {
        switch (status.kind)
        {
        case BattleStatusKind::ColdPoison:
            result.healingBlocked = true;
            result.speedPctDelta -= status.potency * status.stacks;
            break;
        case BattleStatusKind::WitheredBone:
            assert(status.secondaryPotency >= 0 && status.secondaryPotency <= 100);
            result.damageTakenPct += status.potency * status.stacks;
            for (int i = 0; i < status.stacks; ++i)
            {
                result.receivedHealMultipliersPct.push_back(100 - status.secondaryPotency);
            }
            break;
        case BattleStatusKind::BattleSpirit:
            result.skillDamagePct += status.potency * status.stacks;
            result.damageReductionPct += status.secondaryPotency * status.stacks;
            break;
        case BattleStatusKind::TrueQi:
            result.pureDamagePerHit += status.potency * status.stacks;
            break;
        case BattleStatusKind::Poison:
        case BattleStatusKind::Bleed:
        case BattleStatusKind::Stun:
        case BattleStatusKind::MpBlocked:
        case BattleStatusKind::SevenStarMark:
        case BattleStatusKind::NeutralizeForce:
        case BattleStatusKind::Blinded:
        case BattleStatusKind::NextAttackMiss:
        case BattleStatusKind::DamageBlockLayer:
        case BattleStatusKind::SingleHitCapLayer:
        case BattleStatusKind::PoisonExplosion:
        case BattleStatusKind::Shadowless:
        case BattleStatusKind::NextAttackCritical:
            break;
        }
    }
    return result;
}

bool BattleStatusQuerySnapshot::has(BattleStatusKind kind) const
{
    return std::any_of(statuses.begin(), statuses.end(), [&](const auto& status)
    {
        return status.kind == kind && status.stacks > 0;
    });
}

int BattleStatusQuerySnapshot::stacks(BattleStatusKind kind) const
{
    int result = 0;
    for (const auto& status : statuses)
    {
        if (status.kind == kind)
        {
            result += status.stacks;
        }
    }
    return result;
}

int BattleStatusQuerySnapshot::potency(BattleStatusKind kind) const
{
    int result = 0;
    for (const auto& status : statuses)
    {
        if (status.kind == kind)
        {
            result = std::max(result, status.potency);
        }
    }
    return result;
}

int BattleStatusQuerySnapshot::secondaryPotency(BattleStatusKind kind) const
{
    int result = 0;
    for (const auto& status : statuses)
    {
        if (status.kind == kind)
        {
            result = std::max(result, status.secondaryPotency);
        }
    }
    return result;
}

BattleStatusRuntimeUnit makeBattleStatusRuntimeUnit(const BattleStatusUnitState& unit)
{
    BattleStatusRuntimeUnit status;
    writeBattleStatusRuntimeUnit(status, unit);
    return status;
}

BattleStatusUnitState makeBattleStatusUnitState(const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState status;
    status.id = unit.id;
    status.alive = unit.alive;
    status.hp = unit.vitals.hp;
    status.maxHp = unit.vitals.maxHp;
    status.attack = unit.stats.attack;
    status.invincible = unit.invincible;
    return status;
}

BattleStatusUnitState makeBattleStatusUnitState(const BattleStatusRuntimeUnit& status, const BattleRuntimeUnit& unit)
{
    BattleStatusUnitState frame;
    frame.id = unit.id;
    frame.alive = unit.alive;
    frame.hp = unit.vitals.hp;
    frame.maxHp = unit.vitals.maxHp;
    frame.attack = unit.stats.attack;
    frame.invincible = unit.invincible;
    frame.effects = status.effects;
    return frame;
}

void writeBattleStatusRuntimeUnit(BattleStatusRuntimeUnit& status, const BattleStatusUnitState& unit)
{
    status.effects = unit.effects;
}

void rewriteBattleStatusSourceUnitId(
    BattleStatusRuntimeUnit& status,
    int sourceUnitId,
    int replacementUnitId)
{
    assert(sourceUnitId >= 0);
    assert(replacementUnitId >= 0);
    const auto rewrite = [=](int& unitId)
    {
        if (unitId == sourceUnitId)
        {
            unitId = replacementUnitId;
        }
    };

    rewrite(status.effects.poisonSourceId);
    rewrite(status.effects.bleedSourceId);
    rewrite(status.effects.frozenSourceId);
    rewrite(status.effects.mpBlockSourceId);
    for (auto& typedStatus : status.effects.typedStatuses)
    {
        rewrite(typedStatus.sourceUnitId);
    }
}

}  // namespace KysChess::Battle
