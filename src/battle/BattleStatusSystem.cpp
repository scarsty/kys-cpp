#include "BattleStatusSystem.h"

#include "../ChessBattleEffectTypes.h"
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

BattleTypedStatusInstance* BattleStatusEffectState::find(BattleStatusKind kind)
{
    const auto status = std::ranges::find(statuses, kind, &BattleTypedStatusInstance::kind);
    return status != statuses.end() ? &*status : nullptr;
}

const BattleTypedStatusInstance* BattleStatusEffectState::find(BattleStatusKind kind) const
{
    const auto status = std::ranges::find(statuses, kind, &BattleTypedStatusInstance::kind);
    return status != statuses.end() ? &*status : nullptr;
}

bool BattleStatusEffectState::has(BattleStatusKind kind) const
{
    return find(kind) != nullptr;
}

int BattleStatusEffectState::remainingFrames(BattleStatusKind kind) const
{
    const auto* status = find(kind);
    return status ? status->remainingFrames : 0;
}

int BattleStatusEffectState::maximumFrames(BattleStatusKind kind) const
{
    const auto* status = find(kind);
    return status ? status->maximumFrames : 0;
}

void BattleStatusEffectState::setFrames(
    BattleStatusKind kind,
    int frames,
    int maximum,
    int sourceUnitId)
{
    assert(frames >= 0);
    assert(maximum >= 0);
    if (frames == 0)
    {
        clear(kind);
        return;
    }

    auto* status = find(kind);
    if (!status)
    {
        assert(nextStatusSequence > 0);
        assert(nextStatusSequence < std::numeric_limits<std::uint64_t>::max());
        statuses.push_back({
            .kind = kind,
            .sourceUnitId = sourceUnitId,
            .remainingFrames = frames,
            .maximumFrames = std::max(frames, maximum),
            .appliedSequence = nextStatusSequence++,
        });
        return;
    }
    status->sourceUnitId = sourceUnitId;
    status->remainingFrames = frames;
    status->maximumFrames = std::max({ status->maximumFrames, frames, maximum });
}

void BattleStatusEffectState::clear(BattleStatusKind kind)
{
    std::erase_if(statuses, [kind](const BattleTypedStatusInstance& status)
    {
        return status.kind == kind;
    });
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

BattleTypedStatusInstance& appendStatus(
    BattleStatusEffectState& effects,
    const BattleStatusApplyRequest& request,
    int remainingFrames,
    int tickFramesRemaining = 0)
{
    BattleTypedStatusInstance status;
    status.kind = request.kind;
    status.sourceUnitId = request.sourceUnitId;
    status.remainingFrames = remainingFrames;
    status.maximumFrames = remainingFrames;
    status.tickFramesRemaining = tickFramesRemaining;
    status.stacks = request.stackLimit
        ? std::clamp(request.stacks, 0, *request.stackLimit)
        : request.stacks;
    status.potency = request.potency;
    status.secondaryPotency = request.secondaryPotency;
    status.appliedSequence = allocateStatusSequence(effects);
    effects.statuses.push_back(status);
    return effects.statuses.back();
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

void tickPoison(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    auto* poison = effects.find(BattleStatusKind::Poison);
    if (!poison)
    {
        return;
    }
    assert(poison->remainingFrames > 0);
    assert(poison->stacks > 0);
    assert(poison->potency > 0);

    --poison->remainingFrames;
    if (config.poisonDamageIntervalFrames > 0
        && poisonDamageDue(config.frame, config.poisonDamageIntervalFrames))
    {
        int damage = std::max(1, target.hp() * poison->potency / 100);
        result.events.push_back({
            BattleStatusEventType::PoisonDamage,
            target.id(),
            poison->sourceUnitId,
            damage,
            "中毒",
        });
        --poison->stacks;
    }

    if (poison->remainingFrames <= 0 || poison->stacks <= 0)
    {
        effects.clear(BattleStatusKind::Poison);
    }
}

void tickBleed(
    const BattleStatusSystemConfig& config,
    RuntimeStatusTickTarget& target,
    BattleStatusTickResult& result)
{
    auto& effects = target.status.effects;
    auto* bleed = effects.find(BattleStatusKind::Bleed);
    if (!bleed)
    {
        return;
    }
    assert(bleed->stacks > 0);

    if (bleed->tickFramesRemaining > 0)
    {
        --bleed->tickFramesRemaining;
    }

    if (bleed->tickFramesRemaining <= 0)
    {
        int damage = std::max(1, target.maxHp() * bleed->stacks / 100);
        result.events.push_back({
            BattleStatusEventType::BleedDamage,
            target.id(),
            bleed->sourceUnitId,
            damage,
            "流血",
        });
        bleed->tickFramesRemaining = config.bleedDamageIntervalFrames;
    }
}

void tickSimpleTimers(RuntimeStatusTickTarget& target)
{
    if (target.invincible() > 0)
    {
        --target.invincible();
    }
}

void tickStatuses(RuntimeStatusTickTarget& target, BattleStatusTickResult& result)
{
    auto& statuses = target.status.effects.statuses;
    for (auto it = statuses.begin(); it != statuses.end();)
    {
        if (it->kind == BattleStatusKind::Poison
            || it->kind == BattleStatusKind::Bleed
            || it->kind == BattleStatusKind::Stun)
        {
            ++it;
            continue;
        }
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

        if (it->kind != BattleStatusKind::MpBlocked)
        {
            result.events.push_back({
                BattleStatusEventType::StatusExpired,
                target.id(),
                it->sourceUnitId,
                it->stacks,
                "狀態到期",
                it->kind,
            });
        }
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
    tickStatuses(target, result);
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
        auto* poison = effects.find(BattleStatusKind::Poison);
        const bool active = poison != nullptr;
        const int requestedStacks = request.stackLimit
            ? std::min(request.stacks, *request.stackLimit)
            : request.stacks;

        if (active
            && (request.stack == EffectStackPolicy::KeepStrongest
                || request.stack == EffectStackPolicy::Independent)
            && request.potency <= poison->potency)
        {
            result.outcome = BattleStatusApplyOutcome::KeptStronger;
            result.value = poison->potency;
            return result;
        }

        if (request.stack == EffectStackPolicy::AddStack)
        {
            if (!active)
            {
                poison = &appendStatus(effects, request, durationFrames);
            }
            const int before = active ? poison->stacks : 0;
            poison->stacks = std::clamp(
                before + request.stacks,
                0,
                *request.stackLimit);
            poison->remainingFrames = durationFrames;
            poison->maximumFrames = std::max(poison->maximumFrames, durationFrames);
            poison->potency = request.potency;
            poison->sourceUnitId = request.sourceUnitId;
            result.applied = poison->stacks != before;
            result.value = poison->stacks;
            result.outcome = BattleStatusApplyOutcome::StackChanged;
            return result;
        }

        if (!active)
        {
            poison = &appendStatus(effects, request, durationFrames);
        }
        else if (request.stack == EffectStackPolicy::Replace)
        {
            effects.clear(BattleStatusKind::Poison);
            poison = &appendStatus(effects, request, durationFrames);
        }
        poison->stacks = requestedStacks;
        poison->remainingFrames = durationFrames;
        poison->maximumFrames = std::max(poison->maximumFrames, durationFrames);
        poison->potency = request.potency;
        poison->sourceUnitId = request.sourceUnitId;
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
        auto* bleed = effects.find(BattleStatusKind::Bleed);
        const int before = bleed ? bleed->stacks : 0;
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

        after = std::max(0, after);
        if (after <= 0)
        {
            effects.clear(BattleStatusKind::Bleed);
        }
        else
        {
            if (before <= 0)
            {
                bleed = &appendStatus(
                    effects,
                    request,
                    0,
                    config_.bleedDamageIntervalFrames);
            }
            else if (request.stack == EffectStackPolicy::Replace)
            {
                bleed->appliedSequence = allocateStatusSequence(effects);
            }
            assert(bleed);
            bleed->stacks = after;
            if (bleed->tickFramesRemaining <= 0)
            {
                bleed->tickFramesRemaining = config_.bleedDamageIntervalFrames;
            }
            bleed->sourceUnitId = request.sourceUnitId;
        }
        result.applied = after != before;
        result.value = after;
        result.outcome = request.stack == EffectStackPolicy::AddStack
            ? BattleStatusApplyOutcome::StackChanged
            : (before > 0 ? BattleStatusApplyOutcome::Replaced : BattleStatusApplyOutcome::Applied);
        return result;
    }
    case BattleStatusKind::Stun:
    {
        auto* stun = effects.find(BattleStatusKind::Stun);
        const int before = stun ? stun->remainingFrames : 0;
        int after = before;
        switch (request.stack)
        {
        case EffectStackPolicy::Independent:
            after += durationFrames;
            break;
        case EffectStackPolicy::Refresh:
        case EffectStackPolicy::KeepStrongest:
        case EffectStackPolicy::AddStack:
            after = std::max(after, durationFrames);
            break;
        case EffectStackPolicy::Replace:
            after = durationFrames;
            break;
        }
        if (!stun || request.stack == EffectStackPolicy::Replace)
        {
            const int previousMaximum = stun ? stun->maximumFrames : 0;
            effects.clear(BattleStatusKind::Stun);
            stun = &appendStatus(effects, request, after);
            stun->maximumFrames = std::max(previousMaximum, after);
        }
        stun->remainingFrames = after;
        stun->maximumFrames = std::max(stun->maximumFrames, after);
        stun->sourceUnitId = request.sourceUnitId;

        result.applied = after > before;
        result.value = after - before;
        result.outcome = request.stack == EffectStackPolicy::AddStack
            ? BattleStatusApplyOutcome::StackChanged
            : (before > 0 ? BattleStatusApplyOutcome::Refreshed : BattleStatusApplyOutcome::Applied);
        return result;
    }
    case BattleStatusKind::MpBlocked:
    {
        assert(durationFrames > 0);
        auto* mpBlock = effects.find(BattleStatusKind::MpBlocked);
        const int before = mpBlock ? mpBlock->remainingFrames : 0;
        int after{};
        if (request.stack == EffectStackPolicy::Replace)
        {
            after = durationFrames;
        }
        else
        {
            after = std::max(before, durationFrames);
        }
        if (!mpBlock || request.stack == EffectStackPolicy::Replace)
        {
            effects.clear(BattleStatusKind::MpBlocked);
            mpBlock = &appendStatus(effects, request, after);
        }
        mpBlock->remainingFrames = after;
        mpBlock->maximumFrames = std::max(mpBlock->maximumFrames, after);
        mpBlock->sourceUnitId = request.sourceUnitId;
        result.applied = after > before;
        result.value = after - before;
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

    auto first = std::find_if(effects.statuses.begin(), effects.statuses.end(), [&](const auto& status)
    {
        return sameStatus(status, request.kind);
    });
    auto append = [&]() -> BattleTypedStatusInstance&
    {
        return appendStatus(effects, request, durationFrames);
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
        const bool replaced = first != effects.statuses.end();
        std::erase_if(effects.statuses, [&](const auto& status)
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
        if (first == effects.statuses.end())
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
        if (first == effects.statuses.end())
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
        if (first == effects.statuses.end())
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
                effects.statuses.erase(first);
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

    struct Candidate
    {
        std::size_t index{};
        BattleStatusKind kind{};
        int remainingFrames{};
        std::uint64_t sequence{};
    };

    BattleStatusRemoveResult result;
    result.target = std::move(target);
    auto& effects = result.target.effects;
    const bool hadStun = effects.has(BattleStatusKind::Stun);
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < effects.statuses.size(); ++i)
    {
        const auto& status = effects.statuses[i];
        if (matchesRemovalFilter(request, status.kind))
        {
            candidates.push_back({
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
        return std::tuple(lhs.kind, lhs.index)
            < std::tuple(rhs.kind, rhs.index);
    });

    const std::size_t removeCount = request.count > 0
        ? std::min<std::size_t>(request.count, candidates.size())
        : candidates.size();
    std::vector<bool> removeStatus(effects.statuses.size());
    bool removeStun = false;
    for (std::size_t i = 0; i < removeCount; ++i)
    {
        const auto& candidate = candidates[i];
        removeStatus[candidate.index] = true;
        removeStun = removeStun || candidate.kind == BattleStatusKind::Stun;
        result.removedStatuses.push_back(candidate.kind);
        ++result.removedCount;
    }

    for (std::size_t i = effects.statuses.size(); i > 0; --i)
    {
        const std::size_t index = i - 1;
        if (removeStatus[index])
        {
            effects.statuses.erase(effects.statuses.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }
    result.currentActionStaggerCleared = removeStun;
    if (request.clearCurrentActionStagger)
    {
        result.currentActionStaggerCleared = result.currentActionStaggerCleared || hadStun;
        effects.clear(BattleStatusKind::Stun);
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
    const auto oldestMatching = [&]
    {
        auto selected = effects.statuses.end();
        for (auto it = effects.statuses.begin(); it != effects.statuses.end(); ++it)
        {
            if (it->kind != request.kind
                || (request.sourceUnitId && it->sourceUnitId != *request.sourceUnitId))
            {
                continue;
            }
            if (selected == effects.statuses.end()
                || it->appliedSequence < selected->appliedSequence)
            {
                selected = it;
            }
        }
        return selected;
    };
    const auto before = oldestMatching();
    if (before == effects.statuses.end())
    {
        return result;
    }
    result.consumedStatus = *before;

    int remainingToConsume = request.stacks;
    int consumed = 0;
    while (remainingToConsume > 0)
    {
        const auto selected = oldestMatching();
        if (selected == effects.statuses.end())
        {
            break;
        }
        const int amount = std::min(remainingToConsume, selected->stacks);
        selected->stacks -= amount;
        remainingToConsume -= amount;
        consumed += amount;
        if (selected->stacks <= 0)
        {
            effects.statuses.erase(selected);
        }
    }
    result.consumedStatus.stacks = consumed;
    result.consumed = consumed > 0;

    for (const auto& status : effects.statuses)
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

BattleStatusQuerySnapshot BattleStatusSystem::snapshot(
    const BattleStatusEffectState& effects) const
{
    BattleStatusQuerySnapshot result;
    result.statusShield = effects.statusShield;
    result.staggerShield = effects.staggerShield;
    result.statuses = effects.statuses;
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

BattleStatusQuerySnapshot BattleStatusSystem::snapshot(const BattleStatusUnitState& target) const
{
    return snapshot(target.effects);
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

    for (auto& typedStatus : status.effects.statuses)
    {
        rewrite(typedStatus.sourceUnitId);
    }
}

}  // namespace KysChess::Battle
