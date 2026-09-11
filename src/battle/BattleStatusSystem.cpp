#include "BattleStatusSystem.h"

#include "../ChessBattleEffectSemantics.h"
#include "../ChessBattleEffectTypes.h"
#include "BattleEffectSystem.h"
#include "BattleRuntimeUnits.h"

#include <algorithm>
#include <cassert>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace KysChess::Battle
{

BattleStatusGroupStorage::BattleStatusGroupStorage(
    std::initializer_list<value_type> values)
{
    for (const auto& value : values) push_back(value);
}

BattleStatusGroupStorage::BattleStatusGroupStorage(container_type values)
{
    reserve(values.size());
    for (auto& value : values) push_back(std::move(value));
}

BattleStatusGroupStorage& BattleStatusGroupStorage::operator=(
    std::initializer_list<value_type> values)
{
    clear();
    reserve(values.size());
    for (const auto& value : values) push_back(value);
    return *this;
}

BattleStatusGroupStorage& BattleStatusGroupStorage::operator=(container_type values)
{
    clear();
    reserve(values.size());
    for (auto& value : values) push_back(std::move(value));
    return *this;
}

void BattleStatusGroupStorage::assertCanInsert(BattleStatusKind kind) const
{
    if (statusCatalogEntry(kind).storage
        == StatusStorageModel::ProducerOwnedContributions)
    {
        return;
    }
    assert(std::ranges::none_of(values_, [kind](const auto& status)
    {
        return status.kind == kind;
    }));
}

void BattleStatusGroupStorage::rebuildGroups() const
{
    groups_.clear();
    groups_.reserve(values_.size());
    taxonomyKinds_.clear();
    taxonomyKinds_.reserve(values_.size());
    for (std::size_t index = 0; index < values_.size(); ++index)
    {
        const auto kind = values_[index].kind;
        taxonomyKinds_.push_back(kind);
        auto group = std::ranges::find(groups_, kind, &BattleStatusGroupEntry::kind);
        const auto storage = statusCatalogEntry(kind).storage;
        if (group != groups_.end())
        {
            assert(storage == StatusStorageModel::ProducerOwnedContributions);
            auto* producerOwned = std::get_if<ProducerOwnedContributions>(
                &group->state);
            assert(producerOwned);
            producerOwned->contributionIndices.push_back(index);
            continue;
        }

        BattleStatusGroupState state;
        switch (storage)
        {
        case StatusStorageModel::ProducerOwnedContributions:
            state = ProducerOwnedContributions{ { index } };
            break;
        case StatusStorageModel::SharedLayerDebuff:
            state = SharedLayerDebuff{ index };
            break;
        case StatusStorageModel::SelectedDebuffInstance:
            state = SelectedDebuffInstance{ index };
            break;
        case StatusStorageModel::SharedDurationControl:
            state = SharedDurationControl{ index };
            break;
        }
        groups_.push_back({ kind, std::move(state) });
    }
}

void BattleStatusGroupStorage::ensureGroupsFresh() const
{
    const bool kindsMatch = taxonomyKinds_.size() == values_.size()
        && std::equal(
            taxonomyKinds_.begin(),
            taxonomyKinds_.end(),
            values_.begin(),
            [](BattleStatusKind cached, const value_type& value)
            {
                return cached == value.kind;
            });
    if (!kindsMatch)
        rebuildGroups();
}

void BattleStatusGroupStorage::push_back(const value_type& value)
{
    assertCanInsert(value.kind);
    values_.push_back(value);
    rebuildGroups();
}

void BattleStatusGroupStorage::push_back(value_type&& value)
{
    assertCanInsert(value.kind);
    values_.push_back(std::move(value));
    rebuildGroups();
}

BattleStatusGroupStorage::iterator BattleStatusGroupStorage::erase(
    const_iterator position)
{
    const auto offset = std::distance(values_.cbegin(), position);
    values_.erase(position);
    rebuildGroups();
    return values_.begin() + offset;
}

const BattleStatusGroupState* BattleStatusGroupStorage::groupState(
    BattleStatusKind kind) const
{
    ensureGroupsFresh();
    const auto group = std::ranges::find(groups_, kind, &BattleStatusGroupEntry::kind);
    return group != groups_.end() ? &group->state : nullptr;
}

BattleStatusProducerProvenance makeBattleStatusProducerProvenance(
    EffectSourceBinding binding,
    EffectRuleId ruleId,
    std::uint32_t ruleOrder,
    std::uint32_t actionOrder,
    std::uint32_t behaviorRuleOrder,
    std::uint32_t behaviorActionOrder)
{
    const int sourceUnitId = binding.ownerUnitId;
    return {
        .producer = {
            .binding = binding,
            .ruleId = ruleId,
            .actionOrder = actionOrder,
            .behaviorRuleOrder = behaviorRuleOrder,
            .behaviorActionOrder = behaviorActionOrder,
        },
        .producerFamily = {
            .sourceKind = binding.kind,
            .sourceId = binding.sourceId,
            .logicalOwnerUnitId = sourceUnitId,
            .ruleId = ruleId,
            .actionOrder = actionOrder,
            .behaviorRuleOrder = behaviorRuleOrder,
            .behaviorActionOrder = behaviorActionOrder,
        },
        .origin = {
            std::move(binding),
            ruleId,
            ruleOrder,
        },
        .sourceUnitId = sourceUnitId,
    };
}

namespace
{

int saturatedAdd(int left, int right)
{
    return static_cast<int>(std::clamp(
        static_cast<std::int64_t>(left) + right,
        static_cast<std::int64_t>(std::numeric_limits<int>::min()),
        static_cast<std::int64_t>(std::numeric_limits<int>::max())));
}

int saturatedNegate(int value)
{
    return value == std::numeric_limits<int>::min()
        ? std::numeric_limits<int>::max()
        : -value;
}

}

bool isNegativeBattleStatus(BattleStatusKind kind)
{
    return statusCatalogEntry(kind).negative;
}

bool isControlBattleStatus(BattleStatusKind kind)
{
    return statusCatalogEntry(kind).control;
}

namespace
{

int presentationQuantitySum(int left, int right)
{
    assert(left >= 0);
    assert(right >= 0);
    return static_cast<int>(std::min<std::int64_t>(
        static_cast<std::int64_t>(left) + right,
        std::numeric_limits<int>::max()));
}

}

BattleStatusGroupPresentation makeBattleStatusGroupPresentation(
    const BattleStatusEffectState& effects,
    BattleStatusKind kind)
{
    BattleStatusGroupPresentation result;
    result.kind = kind;

    std::vector<const BattleStatusContribution*> ordered;
    ordered.reserve(effects.statuses.size());
    for (const auto& status : effects.statuses)
    {
        if (status.kind == kind) ordered.push_back(&status);
    }
    std::ranges::sort(ordered, {}, &BattleStatusContribution::appliedSequence);

    for (const auto* status : ordered)
    {
        assert(status->stacks >= 0);
        result.quantity = presentationQuantitySum(result.quantity, status->stacks);
        if (status->targetTotalLimit)
        {
            assert(!result.targetTotalCapacity
                || result.targetTotalCapacity == status->targetTotalLimit);
            result.targetTotalCapacity = status->targetTotalLimit;
        }
        auto family = std::ranges::find_if(
            result.families,
            [&](const BattleStatusFamilyPresentation& candidate)
            {
                return candidate.producerFamily == status->producerFamily;
            });
        if (family == result.families.end())
        {
            result.families.push_back({
                .producerFamily = status->producerFamily,
                .capacity = status->familyLocalLimit,
            });
            family = std::prev(result.families.end());
        }
        else
        {
            assert(family->capacity == status->familyLocalLimit);
        }
        family->quantity = presentationQuantitySum(family->quantity, status->stacks);
        family->generations.push_back({
            .quantity = status->stacks,
            .appliedSequence = status->appliedSequence,
            .behavior = status->behavior,
        });
    }
    return result;
}

BattleStatusContribution* BattleStatusEffectState::find(BattleStatusKind kind)
{
    const auto status = std::ranges::find(statuses, kind, &BattleStatusContribution::kind);
    return status != statuses.end() ? &*status : nullptr;
}

const BattleStatusContribution* BattleStatusEffectState::find(BattleStatusKind kind) const
{
    const auto status = std::ranges::find(statuses, kind, &BattleStatusContribution::kind);
    return status != statuses.end() ? &*status : nullptr;
}

bool BattleStatusEffectState::has(BattleStatusKind kind) const
{
    return find(kind) != nullptr;
}

int BattleStatusEffectState::remainingFrames(BattleStatusKind kind) const
{
    int remaining{};
    for (const auto& status : statuses)
    {
        if (status.kind == kind)
            remaining = std::max(remaining, status.remainingFrames);
    }
    return remaining;
}

int BattleStatusEffectState::maximumFrames(BattleStatusKind kind) const
{
    int maximum{};
    for (const auto& status : statuses)
    {
        if (status.kind == kind)
            maximum = std::max(maximum, status.maximumFrames);
    }
    return maximum;
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
        BattleStatusContribution contribution{
            .kind = kind,
            .sourceUnitId = sourceUnitId,
            .remainingFrames = frames,
            .maximumFrames = std::max(frames, maximum),
            .appliedSequence = nextStatusSequence++,
        };
        if (isNegativeBattleStatus(kind))
        {
            assert(nextNegativeEffectSequence > 0);
            assert(nextNegativeEffectSequence
                < std::numeric_limits<std::uint64_t>::max());
            contribution.negativeEffectSequence = nextNegativeEffectSequence++;
        }
        statuses.push_back(std::move(contribution));
        return;
    }
    status->sourceUnitId = sourceUnitId;
    status->remainingFrames = frames;
    status->maximumFrames = std::max({ status->maximumFrames, frames, maximum });
}

void BattleStatusEffectState::clear(BattleStatusKind kind)
{
    statuses.eraseIf([kind](const BattleStatusContribution& status)
    {
        return status.kind == kind;
    });
}

int projectRemainingPoisonDamage(
    const BattleRemainingPoisonDamageInput& input)
{
    assert(input.framesUntilNextTick > 0);
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

    assert(input.framesUntilNextTick <= input.intervalFrames);
    if (input.framesUntilNextTick > input.remainingFrames)
    {
        return 0;
    }

    const std::int64_t scheduledTickCount = 1
        + (static_cast<std::int64_t>(input.remainingFrames)
            - input.framesUntilNextTick)
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

int addAndClamp(int lhs, int rhs, int minimum, int maximum)
{
    assert(minimum <= maximum);
    return static_cast<int>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(lhs) + rhs,
        minimum,
        maximum));
}

std::uint64_t allocateStatusSequence(BattleStatusEffectState& effects)
{
    assert(effects.nextStatusSequence > 0);
    assert(effects.nextStatusSequence < std::numeric_limits<std::uint64_t>::max());
    return effects.nextStatusSequence++;
}

std::uint64_t allocateNegativeEffectSequence(BattleStatusEffectState& effects)
{
    assert(effects.nextNegativeEffectSequence > 0);
    assert(effects.nextNegativeEffectSequence
        < std::numeric_limits<std::uint64_t>::max());
    return effects.nextNegativeEffectSequence++;
}

BattleStatusContribution& appendStatus(
    BattleStatusEffectState& effects,
    const BattleStatusApplyRequest& request,
    int remainingFrames)
{
    BattleStatusContribution status;
    status.kind = request.kind;
    status.producer = request.producer;
    status.producerFamily = request.producerFamily;
    status.familyLocalLimit = request.stackLimit;
    status.targetTotalLimit = request.targetTotalLimit;
    status.behavior = request.behavior;
    status.sourceUnitId = request.sourceUnitId;
    status.remainingFrames = remainingFrames;
    status.maximumFrames = remainingFrames;
    const auto quantityLimit = request.targetTotalLimit
        ? request.targetTotalLimit
        : request.stackLimit;
    status.stacks = quantityLimit
        ? std::clamp(request.stacks, 0, *quantityLimit)
        : request.stacks;
    if (status.behavior)
    {
        status.behaviorRuntime.reserve(status.behavior->rules.size());
        for (const auto& rule : status.behavior->rules)
        {
            status.behaviorRuntime.push_back({
                .intervalFramesRemaining = rule.intervalFrames,
            });
        }
    }
    status.origin = request.origin;
    status.appliedSequence = allocateStatusSequence(effects);
    if (isNegativeBattleStatus(status.kind))
        status.negativeEffectSequence = allocateNegativeEffectSequence(effects);
    effects.statuses.push_back(status);
    return effects.statuses.back();
}

bool sameStatus(const BattleStatusContribution& status, BattleStatusKind kind)
{
    return status.kind == kind;
}

bool sameProducerFamily(
    const BattleStatusContribution& status,
    const BattleStatusApplyRequest& request)
{
    if (status.producerFamily || request.producerFamily)
    {
        return status.producerFamily == request.producerFamily;
    }
    return status.kind == request.kind
        && status.familyLocalLimit == request.stackLimit;
}

bool compatibleContribution(
    const BattleStatusContribution& status,
    const BattleStatusApplyRequest& request)
{
    return status.kind == request.kind
        && status.producer == request.producer
        && sameProducerFamily(status, request)
        && status.familyLocalLimit == request.stackLimit
        && statusBehaviorsEquivalent(status.behavior, request.behavior)
        && status.origin == request.origin;
}

void validateProducerFamilyDefinition(
    const BattleStatusEffectState& effects,
    const BattleStatusApplyRequest& request)
{
    if (!request.producerFamily) return;
    for (const auto& status : effects.statuses)
    {
        if (status.producerFamily != request.producerFamily) continue;
        if (status.kind != request.kind
            || status.familyLocalLimit != request.stackLimit)
        {
            throw std::invalid_argument(
                "同一狀態生產者家族不可使用不同的狀態種類或容量上限");
        }
    }
}

int usedProducerFamilyCapacity(
    const BattleStatusEffectState& effects,
    const BattleStatusApplyRequest& request)
{
    std::int64_t used{};
    for (const auto& status : effects.statuses)
    {
        if (!sameProducerFamily(status, request)) continue;
        assert(status.kind == request.kind);
        assert(status.familyLocalLimit == request.stackLimit);
        used = std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            used + status.stacks);
    }
    return static_cast<int>(used);
}

int allocateProducerFamilyCapacity(
    const BattleStatusEffectState& effects,
    const BattleStatusApplyRequest& request)
{
    assert(request.stackLimit);
    const int used = usedProducerFamilyCapacity(effects, request);
    return std::min(request.stacks, std::max(0, *request.stackLimit - used));
}

void assertCatalogOwnedStatusRequest(const BattleStatusApplyRequest& request)
{
    const auto& catalog = statusCatalogEntry(request.kind);
    if (catalog.behaviorClassification
        != StatusBehaviorClassification::CatalogOwned)
    {
        return;
    }

    assert(request.behavior);
    ApplyStatusAction canonical;
    canonical.status = request.kind;
    if (request.kind == BattleStatusKind::NeutralizeForce)
    {
        const ChangeResourceAction* recovery = nullptr;
        for (const auto& rule : request.behavior->rules)
        {
            for (const auto& action : rule.actions)
            {
                if (const auto* candidate = std::get_if<ChangeResourceAction>(&action.value))
                {
                    assert(!recovery);
                    recovery = candidate;
                }
            }
        }
        assert(recovery);
        canonical.neutralizeMpRecovery = recovery->amount;
    }
    const auto expected = makeCatalogOwnedStatusBehavior(canonical);
    assert(statusBehaviorsEquivalent(request.behavior, expected));

    if (catalog.storage == StatusStorageModel::SharedLayerDebuff)
    {
        assert(request.stack == EffectStackPolicy::AddStack);
        assert(request.targetTotalLimit);
        assert(!request.stackLimit);
    }
    else
    {
        assert(catalog.storage == StatusStorageModel::SelectedDebuffInstance);
        assert(request.stack == EffectStackPolicy::Replace);
        assert(!request.stackLimit);
        assert(!request.targetTotalLimit);
    }
}

int remainingDurationSortValue(int remainingFrames)
{
    return remainingFrames > 0 ? remainingFrames : std::numeric_limits<int>::max();
}

bool containsStatus(const std::vector<BattleStatusKind>& statuses, BattleStatusKind kind)
{
    return std::find(statuses.begin(), statuses.end(), kind) != statuses.end();
}

bool matchesContributionFilter(
    const BattleStatusContribution& status,
    int holderUnitId,
    const StatusContributionFilter& filter)
{
    if (filter.holderUnitId && holderUnitId != *filter.holderUnitId)
        return false;
    if (filter.sourceUnitId && status.sourceUnitId != *filter.sourceUnitId)
        return false;
    if (filter.producerBinding
        && (!status.producer
            || status.producer->binding != *filter.producerBinding))
        return false;
    if (filter.appliedSequence
        && status.appliedSequence != *filter.appliedSequence)
        return false;
    return true;
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
        && static_cast<std::int64_t>(result.target.hp) * 100
            < static_cast<std::int64_t>(result.target.maxHp)
                * request.controlLowHpImmunityPct)
    {
        result.outcome = BattleStatusApplyOutcome::BlockedByControlImmunity;
        return true;
    }

    std::int64_t reductionPct = result.target.effects.freezeReductionPct;
    if (request.targetHasShield)
    {
        reductionPct += result.target.effects.shieldFreezeResPct;
    }
    reductionPct = std::clamp<std::int64_t>(reductionPct, 0, 100);
    durationFrames = static_cast<int>(
        static_cast<std::int64_t>(durationFrames) * (100 - reductionPct) / 100);

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
        if (it->kind == BattleStatusKind::Stun)
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

    static_cast<void>(config);
    tickSimpleTimers(target);
    tickStatuses(target, result);
    return result;
}

}  // namespace

int poisonDamagePercent(
    const std::shared_ptr<const StatusBehaviorDefinition>& behavior)
{
    assert(behavior);
    const auto capability = canonicalPoisonDamageCapability(*behavior);
    assert(capability && capability->damage);
    return capability->damage->amount.percent;
}

std::shared_ptr<const StatusBehaviorDefinition> makeRuntimeBleedStatusBehavior()
{
    ApplyStatusAction bleed;
    bleed.status = BattleStatusKind::Bleed;
    return makeCatalogOwnedStatusBehavior(bleed);
}

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
    if (request.targetTotalLimit)
    {
        assert(*request.targetTotalLimit > 0);
    }
    assertCatalogOwnedStatusRequest(request);
    if (request.stack == EffectStackPolicy::AddStack
        && statusCatalogEntry(request.kind).storage
            == StatusStorageModel::ProducerOwnedContributions)
    {
        assert(request.stackLimit);
    }
    if (request.behavior)
    {
        assert(request.producer);
        assert(request.producerFamily);
        assert(request.origin);
    }
    if (statusCatalogEntry(request.kind).storage
        == StatusStorageModel::ProducerOwnedContributions)
    {
        validateProducerFamilyDefinition(target.effects, request);
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
        assert(request.behavior);
        const auto active = std::ranges::find(
            effects.statuses,
            BattleStatusKind::Poison,
            &BattleStatusContribution::kind);
        if (request.stack == EffectStackPolicy::Replace)
        {
            const bool replaced = active != effects.statuses.end();
            effects.clear(BattleStatusKind::Poison);
            auto& poison = appendStatus(effects, request, durationFrames);
            result.applied = true;
            result.value = poisonDamagePercent(poison.behavior);
            result.outcome = replaced
                ? BattleStatusApplyOutcome::Replaced
                : BattleStatusApplyOutcome::Applied;
            return result;
        }
        assert(request.stack == EffectStackPolicy::KeepStrongest);
        const int incomingDamage = poisonDamagePercent(request.behavior);
        const auto strongest = std::ranges::max_element(
            effects.statuses,
            {},
            [&](const auto& status)
            {
                return status.kind == BattleStatusKind::Poison
                    ? poisonDamagePercent(status.behavior)
                    : std::numeric_limits<int>::min();
            });
        const bool hasActive = strongest != effects.statuses.end()
            && strongest->kind == BattleStatusKind::Poison;
        if (hasActive && incomingDamage <= poisonDamagePercent(strongest->behavior))
        {
            result.outcome = BattleStatusApplyOutcome::KeptStronger;
            result.value = poisonDamagePercent(strongest->behavior);
            return result;
        }
        effects.statuses.eraseIf([](const auto& status)
        {
            return status.kind == BattleStatusKind::Poison;
        });
        auto& poison = appendStatus(effects, request, durationFrames);
        result.applied = true;
        result.value = incomingDamage;
        result.outcome = hasActive
            ? BattleStatusApplyOutcome::Replaced
            : BattleStatusApplyOutcome::Applied;
        return result;
    }
    case BattleStatusKind::Bleed:
    {
        assert(request.targetTotalLimit);
        assert(request.behavior);
        auto* bleed = effects.find(BattleStatusKind::Bleed);
        if (!bleed)
        {
            auto& applied = appendStatus(effects, request, 0);
            result.applied = applied.stacks > 0;
            result.value = applied.stacks;
            result.outcome = BattleStatusApplyOutcome::Applied;
            return result;
        }

        assert(bleed->targetTotalLimit);
        const int before = bleed->stacks;
        const int ceiling = std::max(
            *bleed->targetTotalLimit,
            *request.targetTotalLimit);
        const int after = addAndClamp(before, request.stacks, 0, ceiling);
        bleed->targetTotalLimit = ceiling;
        if (after > before)
        {
            // The shared packet identity, clock, and runtime remain intact.
            // Only an application that really contributes a layer becomes the
            // effective source for later dispatches.  The negative chronology
            // still advances so cleanse ordering follows the accepted
            // application rather than the group's original creation.
            bleed->stacks = after;
            bleed->producer = request.producer;
            bleed->producerFamily = request.producerFamily;
            bleed->behavior = request.behavior;
            bleed->sourceUnitId = request.sourceUnitId;
            bleed->origin = request.origin;
            bleed->negativeEffectSequence = allocateNegativeEffectSequence(effects);
            result.applied = true;
        }
        result.value = after;
        result.outcome = BattleStatusApplyOutcome::StackChanged;
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
            after = addAndClamp(
                after,
                durationFrames,
                0,
                std::numeric_limits<int>::max());
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
        const bool incomingOwnsClock = !stun
            || request.stack == EffectStackPolicy::Replace
            || after > before;
        if (incomingOwnsClock)
        {
            const int previousMaximum = stun ? stun->maximumFrames : 0;
            effects.clear(BattleStatusKind::Stun);
            stun = &appendStatus(effects, request, after);
            stun->maximumFrames = std::max(previousMaximum, after);
        }
        stun->remainingFrames = after;
        stun->maximumFrames = std::max(stun->maximumFrames, after);

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
        const bool incomingOwnsClock = !mpBlock
            || request.stack == EffectStackPolicy::Replace
            || after > before;
        if (incomingOwnsClock)
        {
            effects.clear(BattleStatusKind::MpBlocked);
            mpBlock = &appendStatus(effects, request, after);
        }
        mpBlock->remainingFrames = after;
        mpBlock->maximumFrames = std::max(mpBlock->maximumFrames, after);
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
    {
        const bool replaced = effects.has(request.kind);
        effects.clear(request.kind);
        auto& selected = appendStatus(effects, request, durationFrames);
        result.applied = true;
        result.value = selected.stacks;
        if (!replaced)
            result.outcome = BattleStatusApplyOutcome::Applied;
        else if (request.kind == BattleStatusKind::ColdPoison
            || request.kind == BattleStatusKind::WitheredBone)
            result.outcome = BattleStatusApplyOutcome::Refreshed;
        else
            result.outcome = BattleStatusApplyOutcome::Replaced;
        return result;
    }
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

    auto compatible = std::find_if(
        effects.statuses.begin(), effects.statuses.end(), [&](const auto& status)
    {
        return compatibleContribution(status, request);
    });
    auto append = [&](const BattleStatusApplyRequest& applied) -> BattleStatusContribution&
    {
        return appendStatus(effects, applied, durationFrames);
    };

    switch (request.stack)
    {
    case EffectStackPolicy::Independent:
    {
        auto& status = append(request);
        result.applied = true;
        result.value = status.stacks;
        result.outcome = BattleStatusApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Replace:
    {
        const bool replaced = std::ranges::any_of(
            effects.statuses,
            [&](const auto& status)
            {
                return status.kind == request.kind
                    && sameProducerFamily(status, request);
            });
        effects.statuses.eraseIf([&](const auto& status)
        {
            return status.kind == request.kind
                && sameProducerFamily(status, request);
        });
        auto& status = append(request);
        result.applied = true;
        result.value = status.stacks;
        result.outcome = replaced
            ? BattleStatusApplyOutcome::Replaced
            : BattleStatusApplyOutcome::Applied;
        break;
    }
    case EffectStackPolicy::Refresh:
    {
        if (compatible == effects.statuses.end())
        {
            if (request.stackLimit
                && usedProducerFamilyCapacity(effects, request)
                    >= *request.stackLimit)
            {
                assert(request.producerFamily);
                assert(*request.stackLimit == 1);
                const auto existingFamily = std::find_if(
                    effects.statuses.begin(), effects.statuses.end(),
                    [&](const auto& status)
                {
                    return status.kind == request.kind
                        && sameProducerFamily(status, request);
                });
                assert(existingFamily != effects.statuses.end());
                existingFamily->remainingFrames = durationFrames;
                existingFamily->maximumFrames = std::max(
                    existingFamily->maximumFrames,
                    durationFrames);
                result.value = existingFamily->stacks;
                result.outcome = BattleStatusApplyOutcome::Refreshed;
            }
            else
            {
                auto& status = append(request);
                result.value = status.stacks;
                result.outcome = BattleStatusApplyOutcome::Applied;
            }
        }
        else
        {
            compatible->remainingFrames = durationFrames;
            compatible->maximumFrames = std::max(
                compatible->maximumFrames,
                durationFrames);
            result.value = compatible->stacks;
            result.outcome = BattleStatusApplyOutcome::Refreshed;
        }
        result.applied = true;
        break;
    }
    case EffectStackPolicy::KeepStrongest:
        assert(false && "保留最強必須由具名群組 reducer 處理");
        break;
    case EffectStackPolicy::AddStack:
    {
        const int allocated = allocateProducerFamilyCapacity(effects, request);
        if (compatible == effects.statuses.end())
        {
            if (allocated > 0)
            {
                auto allocatedRequest = request;
                allocatedRequest.stacks = allocated;
                auto& status = append(allocatedRequest);
                result.applied = status.stacks > 0;
            }
        }
        else
        {
            if (allocated > 0)
            {
                compatible->stacks = addAndClamp(
                    compatible->stacks,
                    allocated,
                    0,
                    *request.stackLimit);
                if (durationFrames > 0)
                {
                    compatible->remainingFrames = durationFrames;
                    compatible->maximumFrames = std::max(
                        compatible->maximumFrames,
                        durationFrames);
                }
                result.applied = true;
            }
        }
        result.value = usedProducerFamilyCapacity(effects, request);
        result.outcome = BattleStatusApplyOutcome::StackChanged;
        break;
    }
    }
    return result;
}

std::vector<BattleStatusRemovalCandidate> BattleStatusSystem::removalCandidates(
    const BattleStatusUnitState& target,
    const BattleStatusRemoveRequest& request) const
{
    assert(target.id >= 0);
    assert(request.count >= 0);

    std::vector<BattleStatusRemovalCandidate> candidates;
    const auto removalSequence = [](const BattleStatusContribution& status)
    {
        return status.negativeEffectSequence != 0
            ? status.negativeEffectSequence
            : status.appliedSequence;
    };
    for (const auto& status : target.effects.statuses)
    {
        if (!matchesRemovalFilter(request, status.kind)
            || !matchesContributionFilter(status, target.id, request.filter)) continue;
        auto candidate = std::ranges::find(
            candidates,
            status.kind,
            &BattleStatusRemovalCandidate::kind);
        if (candidate == candidates.end())
        {
            candidates.push_back({
                status.kind,
                status.remainingFrames,
                removalSequence(status),
            });
            continue;
        }
        candidate->remainingFrames = std::max(
            remainingDurationSortValue(candidate->remainingFrames),
            remainingDurationSortValue(status.remainingFrames));
        candidate->sequence = request.order == StatusRemovalOrder::Newest
            ? std::max(candidate->sequence, removalSequence(status))
            : std::min(candidate->sequence, removalSequence(status));
    }

    std::sort(candidates.begin(), candidates.end(), [&](const auto& lhs, const auto& rhs)
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
        return lhs.kind < rhs.kind;
    });
    return candidates;
}

BattleStatusRemoveResult BattleStatusSystem::remove(
    BattleStatusUnitState target,
    const BattleStatusRemoveRequest& request) const
{
    assert(target.id >= 0);
    assert(request.count >= 0);

    BattleStatusRemoveResult result;
    auto candidates = removalCandidates(target, request);
    result.target = std::move(target);
    auto& effects = result.target.effects;
    const bool hadStun = effects.has(BattleStatusKind::Stun);

    const std::size_t removeCount = request.count > 0
        ? std::min<std::size_t>(request.count, candidates.size())
        : candidates.size();
    std::vector<BattleStatusKind> selectedKinds;
    bool removeStun = false;
    for (std::size_t i = 0; i < removeCount; ++i)
    {
        const auto& candidate = candidates[i];
        selectedKinds.push_back(candidate.kind);
        removeStun = removeStun || candidate.kind == BattleStatusKind::Stun;
        result.removedStatuses.push_back(candidate.kind);
        ++result.removedCount;
    }
    effects.statuses.eraseIf([&](const auto& status)
    {
        return std::ranges::contains(selectedKinds, status.kind)
            && matchesContributionFilter(status, result.target.id, request.filter);
    });
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
                || !matchesContributionFilter(*it, result.target.id, request.filter))
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
            && matchesContributionFilter(status, result.target.id, request.filter))
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
    result.statuses.assign(effects.statuses.begin(), effects.statuses.end());
    std::sort(result.statuses.begin(), result.statuses.end(), [](const auto& lhs, const auto& rhs)
    {
        return std::tuple(lhs.appliedSequence, lhs.kind, lhs.sourceUnitId)
            < std::tuple(rhs.appliedSequence, rhs.kind, rhs.sourceUnitId);
    });

    const auto persistentNumber = [](const EffectNumber& number, int quantity)
    {
        const auto value = effectiveConstantEffectNumberValue(number, quantity);
        assert(value);
        return *value;
    };

    struct OrderedPersistentAction
    {
        const BattleStatusContribution* status{};
        const EffectAction* action{};
        EffectExecutionOrderKey order;
    };
    std::vector<OrderedPersistentAction> persistentActions;
    for (const auto& status : result.statuses)
    {
        if (!status.behavior) continue;
        assert(status.origin);
        assert(status.producer);
        for (std::uint32_t ruleOrder = 0;
             ruleOrder < status.behavior->rules.size();
             ++ruleOrder)
        {
            const auto& rule = status.behavior->rules[ruleOrder];
            if (rule.event != EffectEvent::StatusPersistent) continue;
            for (std::uint32_t actionOrder = 0;
                 actionOrder < rule.actions.size();
                 ++actionOrder)
            {
                persistentActions.push_back({
                    .status = &status,
                    .action = &rule.actions[actionOrder],
                    .order = statusBehaviorExecutionOrderKey(
                        status.origin->binding,
                        status.origin->ruleOrder,
                        status.producer->actionOrder,
                        ruleOrder,
                        -1,
                        status.appliedSequence,
                        actionOrder),
                });
            }
        }
    }
    std::ranges::stable_sort(persistentActions, [](const auto& lhs, const auto& rhs)
    {
        return lhs.order < rhs.order;
    });

    for (const auto& ordered : persistentActions)
    {
        const auto& status = *ordered.status;
        const auto& action = *ordered.action;
        std::visit([&](const auto& modifier)
        {
            using T = std::decay_t<decltype(modifier)>;
            if constexpr (std::is_same_v<T, ModifyAttributeAction>)
            {
                assert(modifier.attribute == BattleAttribute::Speed);
                assert(modifier.operation == AttributeOperation::PercentAdd);
                assert(modifier.durationFrames == 0);
                result.speedPctDelta = saturatedAdd(
                    result.speedPctDelta,
                    persistentNumber(modifier.amount, status.stacks));
            }
            else if constexpr (std::is_same_v<T, ModifyDamageAction>)
            {
                assert(modifier.durationFrames == 0);
                if (modifier.operation
                    == DamageModifierOperation::CapSingleHitAtValue)
                {
                    return;
                }
                assert(modifier.operation == DamageModifierOperation::PercentAdd);
                const int amount = persistentNumber(
                    modifier.amount,
                    status.stacks);
                if (modifier.perspective == DamageModifierPerspective::Outgoing)
                {
                    assert(modifier.stage == DamageModifierStage::BeforeDefense);
                    assert(modifier.channel == DamageChannel::Skill);
                    result.skillDamagePct = saturatedAdd(result.skillDamagePct, amount);
                }
                else if (modifier.stage == DamageModifierStage::BeforeDefense)
                {
                    assert(modifier.channel == DamageChannel::All);
                    assert(amount <= 0);
                    result.damageReductionPct = saturatedAdd(
                        result.damageReductionPct,
                        saturatedNegate(amount));
                }
                else
                {
                    assert(modifier.stage == DamageModifierStage::Final);
                    assert(modifier.channel == DamageChannel::All);
                    result.damageTakenPct = saturatedAdd(result.damageTakenPct, amount);
                }
            }
            else if constexpr (std::is_same_v<T, ModifyHealTransactionAction>)
            {
                result.healTransactionModifiers.push_back(modifier);
            }
            else if constexpr (std::is_same_v<T, BlockPositiveDamageAction>)
            {
            }
            else
            {
                assert(false && "持續狀態規則包含不可查詢的動作");
            }
        }, action.value);
    }
    return result;
}

BattleStatusQuerySnapshot BattleStatusSystem::snapshot(const BattleStatusUnitState& target) const
{
    auto result = snapshot(target.effects);
    result.holderUnitId = target.id;
    return result;
}

bool BattleStatusQuerySnapshot::has(BattleStatusKind kind) const
{
    return std::any_of(statuses.begin(), statuses.end(), [&](const auto& status)
    {
        return status.kind == kind && status.stacks > 0;
    });
}

int BattleStatusQuerySnapshot::stacks(
    BattleStatusKind kind,
    const StatusContributionFilter& filter) const
{
    std::int64_t result{};
    for (const auto& status : statuses)
    {
        if (status.kind == kind
            && matchesContributionFilter(status, holderUnitId, filter))
        {
            result = std::min<std::int64_t>(
                std::numeric_limits<int>::max(),
                result + status.stacks);
        }
    }
    return static_cast<int>(result);
}

std::optional<int> BattleStatusQuerySnapshot::familyCapacity(
    BattleStatusKind kind) const
{
    std::vector<StatusProducerFamilyKey> counted;
    std::int64_t total{};
    bool hasFiniteCapacity = false;
    for (const auto& status : statuses)
    {
        if (status.kind != kind || status.stacks <= 0) continue;
        if (!status.producerFamily || !status.familyLocalLimit)
            return std::nullopt;
        if (std::ranges::contains(counted, *status.producerFamily)) continue;
        counted.push_back(*status.producerFamily);
        hasFiniteCapacity = true;
        total = std::min<std::int64_t>(
            std::numeric_limits<int>::max(),
            total + *status.familyLocalLimit);
    }
    return hasFiniteCapacity
        ? std::optional{ static_cast<int>(total) }
        : std::nullopt;
}

std::optional<int> BattleStatusQuerySnapshot::targetTotalCapacity(
    BattleStatusKind kind) const
{
    std::optional<int> capacity;
    for (const auto& status : statuses)
    {
        if (status.kind != kind || status.stacks <= 0) continue;
        if (!status.targetTotalLimit) return std::nullopt;
        assert(!capacity || capacity == status.targetTotalLimit);
        capacity = status.targetTotalLimit;
    }
    return capacity;
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
        if (typedStatus.producer)
        {
            rewrite(typedStatus.producer->binding.ownerUnitId);
        }
        if (typedStatus.producerFamily)
        {
            rewrite(typedStatus.producerFamily->logicalOwnerUnitId);
        }
        if (typedStatus.origin)
        {
            rewrite(typedStatus.origin->binding.ownerUnitId);
        }
    }
}

}  // namespace KysChess::Battle
